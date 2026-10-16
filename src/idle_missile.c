#include "idle_missile.h"
#include "idle_missile_res.h"   // idle_missile (rescomp)
#include "level5_1.h"           // fw_bomb_boom: la explosion de Baxter en PAL2
#include "enemy.h"              // sprVramFits
#include "audio.h"              // foot_soldier_explode

// ===========================================================================
// Ver idle_missile.h
// ===========================================================================
#define IM_FALL_V          6      // px/f que baja
#define IM_START_ABOVE    24      // arranca esto por encima del borde de la pantalla
#define IM_W              32
#define IM_H              64
#define IM_HIT_DX         28      // radio de la explosion en X ...
#define IM_HIT_DY         14      // ... y en profundidad
#define IM_HIT_Z          48      // saltando mas alto que esto, se salva
#define IM_BOOM_FRAMES     8
#define IM_BOOM_TICKS      4
#define IM_BOOM_HALF      64

static struct { s16 x, y, cam; u16 t; } imIdle[MAX_PLAYERS];
static struct { u8 on; s16 x, y, z; u8 tick, fr; Sprite* s; } imMis;
static struct { Sprite* s[4]; u8 owner[4]; s16 cx, cy; u8 frame, tick; } imBoom;

void idleMissileReset(void) {
    for (u16 k = 0; k < MAX_PLAYERS; k++) { imIdle[k].t = 0; imIdle[k].x = imIdle[k].y = imIdle[k].cam = -32000; }
    imMis.on = 0; imMis.s = NULL;
    for (u16 q = 0; q < 4; q++) imBoom.s[q] = NULL;
}

// --- La explosion (como la de la bomba del 5: dos cuartos de la izquierda y
// sus espejos, o uno solo y 3 espejos si no hay VRAM) ------------------------
static void imBoomRelease(void) {
    for (u16 q = 0; q < 4; q++)
        if (imBoom.s[q]) { SPR_releaseSprite(imBoom.s[q]); imBoom.s[q] = NULL; }
}

static Sprite* imMirror(const Sprite* m, u16 anim, bool hf, bool vf) {
    if (!m) return NULL;
    Sprite* s = SPR_addSpriteEx(&fw_bomb_boom, -128, -128,
                                TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE,
                                               m->attribut & TILE_INDEX_MASK), 0);
    if (!s) return NULL;
    SPR_setAnimAndFrame(s, (s16)anim, 0);
    SPR_setHFlip(s, hf);
    SPR_setVFlip(s, vf);
    return s;
}

static void imBoomAt(s16 cx, s16 feet) {
    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                   SOUND_PCM_CH3, 15, FALSE, FALSE);
    imBoomRelease();
    const u16 T = fw_bomb_boom.maxNumTile;
    if (!sprVramFits(T)) return;                  // sin lugar: solo el sonido
    Sprite** sp = imBoom.s;
    sp[0] = SPR_addSprite(&fw_bomb_boom, -128, -128, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
    if (!sp[0]) return;
    SPR_setAnimAndFrame(sp[0], 0, 0);
    if (SPR_getLargestFreeVRAMBlock() >= T) {
        sp[2] = SPR_addSprite(&fw_bomb_boom, -128, -128, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
        if (sp[2]) SPR_setAnimAndFrame(sp[2], 1, 0);
    }
    if (sp[2]) {
        sp[1] = imMirror(sp[0], 0, TRUE, FALSE);
        sp[3] = imMirror(sp[2], 1, TRUE, FALSE);
        imBoom.owner[1] = 0; imBoom.owner[2] = 2; imBoom.owner[3] = 2;
    } else {
        sp[1] = imMirror(sp[0], 0, TRUE,  FALSE);
        sp[2] = imMirror(sp[0], 0, FALSE, TRUE);
        sp[3] = imMirror(sp[0], 0, TRUE,  TRUE);
        imBoom.owner[1] = imBoom.owner[2] = imBoom.owner[3] = 0;
    }
    imBoom.owner[0] = 0;
    for (u16 q = 0; q < 4; q++) if (sp[q]) {
        SPR_setAutoAnimation(sp[q], FALSE);
        SPR_setDepth(sp[q], SPR_MIN_DEPTH);
    }
    imBoom.cx = cx; imBoom.cy = (s16)(feet - 40);
    imBoom.frame = 0; imBoom.tick = 0;
}

static void imBoomUpdate(s16 camX, s16 camY) {
    if (!imBoom.s[0]) return;
    if (++imBoom.tick >= IM_BOOM_TICKS) {
        imBoom.tick = 0;
        if (++imBoom.frame >= IM_BOOM_FRAMES) { imBoomRelease(); return; }
        for (u16 q = 0; q < 4; q++) if (imBoom.s[q]) SPR_setFrame(imBoom.s[q], imBoom.frame);
    }
    // Si SPR_defragVRAM mudo a un dueno, sus espejos lo siguen.
    for (u16 q = 1; q < 4; q++) {
        Sprite* m = imBoom.s[imBoom.owner[q]];
        if (!imBoom.s[q] || !m || imBoom.owner[q] == q) continue;
        u16 idx = m->attribut & TILE_INDEX_MASK;
        if ((imBoom.s[q]->attribut & TILE_INDEX_MASK) != idx) SPR_setVRAMTileIndex(imBoom.s[q], idx);
    }
    s16 x0 = (s16)(imBoom.cx - IM_BOOM_HALF - camX);
    s16 y0 = (s16)(imBoom.cy - IM_BOOM_HALF - camY);
    for (u16 q = 0; q < 4; q++)
        if (imBoom.s[q])
            SPR_setPosition(imBoom.s[q], (s16)(x0 + (q & 1) * IM_BOOM_HALF),
                                         (s16)(y0 + (q >> 1) * IM_BOOM_HALF));
}

// --- El misil ---------------------------------------------------------------
static void imFire(const Player* p, s16 camY) {
    if (!sprVramFits(idle_missile.maxNumTile)) return;
    imMis.s = SPR_addSprite(&idle_missile, -64, -64, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
    if (!imMis.s) return;
    SPR_setAutoAnimation(imMis.s, FALSE);
    SPR_setDepth(imMis.s, SPR_MIN_DEPTH);
    imMis.on = 1;
    imMis.x  = getPlayerHurtCX(p);
    imMis.y  = getPlayerY(p);
    // La punta arranca arriba del borde de la pantalla.
    imMis.z  = (s16)(imMis.y - camY + IM_START_ABOVE);
    imMis.tick = 0; imMis.fr = 0;
}

static void imMisUpdate(Player** pls, u8 nPl, s16 camX, s16 camY) {
    if (!imMis.on) return;
    imMis.z = (s16)(imMis.z - IM_FALL_V);
    if (++imMis.tick >= 4) { imMis.tick = 0; imMis.fr ^= 1; SPR_setFrame(imMis.s, imMis.fr); }
    if (imMis.z <= 0) {
        // Toco el piso: explota. Al que este ahi le saca la vida entera.
        SPR_releaseSprite(imMis.s); imMis.s = NULL; imMis.on = 0;
        imBoomAt(imMis.x, imMis.y);
        for (u8 k = 0; k < nPl; k++) {
            Player* p = pls[k];
            if (!p->sprite || isPlayerGameOver(p)) continue;
            s16 dx = (s16)(getPlayerHurtCX(p) - imMis.x), dy = (s16)(getPlayerY(p) - imMis.y);
            if (dx < 0) dx = (s16)-dx;
            if (dy < 0) dy = (s16)-dy;
            if (dx > IM_HIT_DX || dy > IM_HIT_DY || getPlayerJumpZ(p) > IM_HIT_Z) continue;
            playerHitBarsKnockdown(p, imMis.x, PLAYER_MAX_HEALTH);
        }
        return;
    }
    // La punta (fila de abajo del sprite) en los pies + z.
    SPR_setPosition(imMis.s, (s16)(imMis.x - IM_W / 2 - camX),
                    (s16)(imMis.y - imMis.z - IM_H - camY));
}

void idleMissileUpdate(Player** pls, u8 nPl, s16 camX, s16 camY) {
    if (dificultad != DIFICULTAD_ARCADE) return;
    const u16 limit = (u16)((IS_PAL_SYSTEM ? 50 : 60) * IDLE_MISSILE_SECS);
    for (u8 k = 0; k < nPl && k < MAX_PLAYERS; k++) {
        Player* p = pls[k];
        bool still = (bool)(p->sprite && !isPlayerGameOver(p) && p->state == STATE_IDLE &&
                            p->x == imIdle[k].x && p->y == imIdle[k].y && camX == imIdle[k].cam);
        imIdle[k].x = p->x; imIdle[k].y = p->y; imIdle[k].cam = camX;
        if (!still) { imIdle[k].t = 0; continue; }
        if (++imIdle[k].t >= limit && !imMis.on && !imBoom.s[0]) {
            imIdle[k].t = 0;
            imFire(p, camY);
        }
    }
    imMisUpdate(pls, nPl, camX, camY);
    imBoomUpdate(camX, camY);
}
