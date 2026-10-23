#include "granitor.h"
#include "difficulty.h"  // (20/10) vida segun jugadores
#include "audio.h"    // boss_hit, hit_turtles, foot_soldier_explode
#include "boss_flash.h"
#include "stage_level.h"   // (18/10) stageShake
#include "enemy.h"         // sprDefragLock (las explosiones comparten tiles)

// ===========================================================================
// GRANITOR + LLAMAS — ver granitor.h
// ===========================================================================

static s16 gabs(s16 v)                 { return (v < 0) ? -v : v; }
static s16 gclamp(s16 v, s16 a, s16 b) { return (v < a) ? a : ((v > b) ? b : v); }

static s16 gStep(u8* acc, u16 q) {
    u16 t = (u16)(*acc + q);
    *acc = (u8)(t & 0xFF);
    return (s16)(t >> 8);
}

static s16 gToward(s16 v, s16 to, s16 step) {
    if (v < to) return (to - v < step) ? to : (s16)(v + step);
    if (v > to) return (v - to < step) ? to : (s16)(v - step);
    return v;
}

// El jugador EN JUEGO mas cercano; si no queda ninguno, el P1.
static Player* nearestPlayer(Player** pls, u8 nPl, s16 x) {
    Player* best = pls[0];
    s16 bestD = 0x7FFF;
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k])) continue;
        s16 d = gabs((s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2 - x));
        if (d < bestD) { bestD = d; best = pls[k]; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// LLAMAS: el chorro del lanzallamas. Una cada GRAN_FLAME_EVERY frames; cada
// una avanza recta, CRECE (anim 0, a mano) y se apaga a los GRAN_FLAME_LIFE.
// ---------------------------------------------------------------------------
#define FLAME_Z   55    // (18/10) altura de la punta del arma sobre los pies (fila 85 de la celda, pies en la 140)

static struct {
    bool    active;
    Sprite* sprite;
    s16     x;             // centro (mundo)
    s16     lane;          // lane de Granitor al disparar
    s8      dir;
    u8      acc;
    u8      t;
    u8      hitMask;       // jugadores ya quemados
} flames[MAX_GRANITOR_FLAMES];

static void flameInitAll(void) {
    for (u16 i = 0; i < MAX_GRANITOR_FLAMES; i++) {
        flames[i].active = FALSE;
        flames[i].sprite = NULL;
    }
}

static void flameFire(s16 x, s16 lane, s8 dir) {
    for (u16 i = 0; i < MAX_GRANITOR_FLAMES; i++) {
        if (flames[i].active) continue;
        // Sin sprite (VRAM llena) la llama igual quema.
        if (!flames[i].sprite)
            flames[i].sprite = SPR_addSprite(&granitor_flame, -64, -64,
                                             TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        flames[i].active  = TRUE;
        flames[i].x       = x;
        flames[i].lane    = lane;
        flames[i].dir     = dir;
        flames[i].acc     = 0;
        flames[i].t       = 0;
        flames[i].hitMask = 0;
        if (flames[i].sprite) {
            SPR_setAutoAnimation(flames[i].sprite, FALSE);
            SPR_setAnimAndFrame(flames[i].sprite, 0, 0);
            SPR_setHFlip(flames[i].sprite, dir < 0);
            SPR_setVisibility(flames[i].sprite, VISIBLE);
        }
        return;
    }
}

void granitorFlameUpdate(Player** pls, u8 nPl, s16 camX) {
    for (u16 i = 0; i < MAX_GRANITOR_FLAMES; i++) {
        if (!flames[i].active) continue;
        if (++flames[i].t > GRAN_FLAME_LIFE) {
            flames[i].active = FALSE;
            if (flames[i].sprite) SPR_setVisibility(flames[i].sprite, HIDDEN);
            continue;
        }
        flames[i].x += flames[i].dir * gStep(&flames[i].acc, GRAN_FLAME_Q);

        // Quema a cada tortuga una vez por llama.
        for (u8 k = 0; k < nPl; k++) {
            u8 bit = (u8)(1 << k);
            if ((flames[i].hitMask & bit) || !playerCanBeHit(pls[k])) continue;
            s16 px = getPlayerHurtCX(pls[k]);
            if (gabs((s16)(px - flames[i].x)) >= GRAN_FLAME_HALF_W) continue;
            s16 dy = (s16)(getPlayerY(pls[k]) - flames[i].lane);
            if (dy < -GRAN_FLAME_DY_UP || dy > GRAN_FLAME_DY_DOWN) continue;
            flames[i].hitMask |= bit;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
            playerBurnHit(pls[k], flames[i].x, GRAN_FLAME_DMG);   // (18/10) queda quemada (ANIM_BURN)
        }

        if (flames[i].sprite) {
            u8 f = (u8)((flames[i].t * 7) / (GRAN_FLAME_LIFE + 1));
            SPR_setFrame(flames[i].sprite, f);
            SPR_setPosition(flames[i].sprite, flames[i].x - camX - 16,
                            flames[i].lane - FLAME_Z - 16);
            SPR_setDepth(flames[i].sprite, (s16)(-(flames[i].lane) - 1));
        }
    }
}

void granitorFlameReleaseAll(void) {
    for (u16 i = 0; i < MAX_GRANITOR_FLAMES; i++) {
        if (flames[i].sprite) { SPR_releaseSprite(flames[i].sprite); flames[i].sprite = NULL; }
        flames[i].active = FALSE;
    }
}

// ---------------------------------------------------------------------------
// (18/10) EXPLOSIONES de la muerte (como las de Krang, krang.c): cada una son
// 4 cuartos de 64x64; los de la izquierda con su arte (anim 0 arriba, 1
// abajo) y los de la derecha espejados apuntando a sus tiles. Si no hay VRAM
// para dos cuartos, uno y 3 espejos; si ni eso, esa no se ve (suena igual).
// state: 0 libre, 1 esperando su turno, 2 explotando.
// ---------------------------------------------------------------------------
static struct {
    u8      state;
    u16     wait;
    Sprite* s[4];
    u8      owner[4];
    s16     cx, cy;       // centro (mundo)
    u8      frame, tick;
} gBooms[GRAN_BOOMS];

static const s8 gBoomOff[GRAN_BOOMS][2] = {
    { 0, 0 }, { -28, -26 }, { 26, 18 }, { 8, -46 }, { -18, 22 }
};

static void gBoomRelease(u16 i) {
    for (u16 q = 0; q < 4; q++)
        if (gBooms[i].s[q]) { SPR_releaseSprite(gBooms[i].s[q]); gBooms[i].s[q] = NULL; }
}

static void gBoomsInit(void) {
    for (u16 i = 0; i < GRAN_BOOMS; i++) {
        gBooms[i].state = 0;
        for (u16 q = 0; q < 4; q++) gBooms[i].s[q] = NULL;
    }
}

static Sprite* gBoomMaster(u16 anim) {
    Sprite* s = SPR_addSprite(&granitor_boom, -128, -128, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    if (s) SPR_setAnimAndFrame(s, (s16)anim, 0);
    return s;
}

static Sprite* gBoomMirror(const Sprite* m, u16 anim, bool hf, bool vf) {
    if (!m) return NULL;
    Sprite* s = SPR_addSpriteEx(&granitor_boom, -128, -128,
                                TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE,
                                               m->attribut & TILE_INDEX_MASK), 0);
    if (!s) return NULL;
    SPR_setAnimAndFrame(s, (s16)anim, 0);
    SPR_setHFlip(s, hf);
    SPR_setVFlip(s, vf);
    return s;
}

static void gBoomStart(u16 i) {
    const u16 T = granitor_boom.maxNumTile;
    Sprite** sp = gBooms[i].s;
    gBooms[i].state = 2;
    gBooms[i].frame = 0;
    gBooms[i].tick = 0;
    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                   SOUND_PCM_CH3, 15, FALSE, FALSE);
    stageShake(10);
    if (SPR_getLargestFreeVRAMBlock() < T) return;
    sp[0] = gBoomMaster(0);
    if (!sp[0]) return;
    if (SPR_getLargestFreeVRAMBlock() >= T) sp[2] = gBoomMaster(1);
    if (sp[2]) {
        sp[1] = gBoomMirror(sp[0], 0, TRUE, FALSE);
        sp[3] = gBoomMirror(sp[2], 1, TRUE, FALSE);
        gBooms[i].owner[1] = 0; gBooms[i].owner[2] = 2; gBooms[i].owner[3] = 2;
    } else {
        sp[1] = gBoomMirror(sp[0], 0, TRUE,  FALSE);
        sp[2] = gBoomMirror(sp[0], 0, FALSE, TRUE);
        sp[3] = gBoomMirror(sp[0], 0, TRUE,  TRUE);
        gBooms[i].owner[1] = gBooms[i].owner[2] = gBooms[i].owner[3] = 0;
    }
    gBooms[i].owner[0] = 0;
    for (u16 q = 0; q < 4; q++) if (sp[q]) {
        SPR_setAutoAnimation(sp[q], FALSE);
        SPR_setDepth(sp[q], (s16)(SPR_MIN_DEPTH + 1 + (GRAN_BOOMS - i)));
    }
}

static void gBoomsChain(s16 x, s16 y) {
    sprDefragLock = 1;            // los espejos apuntan a la VRAM de su dueno
    for (u16 i = 0; i < GRAN_BOOMS; i++) {
        gBoomRelease(i);
        gBooms[i].state = 1;
        gBooms[i].wait = (u16)(i * GRAN_BOOM_GAP);
        gBooms[i].cx = (s16)(x + gBoomOff[i][0]);
        gBooms[i].cy = (s16)(y - GRAN_BOOM_DZ + gBoomOff[i][1]);
    }
}

static void gBoomsUpdate(s16 camX) {
    bool live = FALSE, any = FALSE;
    for (u16 i = 0; i < GRAN_BOOMS; i++) {
        if (!gBooms[i].state) continue;
        any = TRUE;
        if (gBooms[i].state == 1) {
            if (gBooms[i].wait) { gBooms[i].wait--; live = TRUE; continue; }
            gBoomStart(i);
        } else if (++gBooms[i].tick >= GRAN_BOOM_TICKS) {
            gBooms[i].tick = 0;
            if (++gBooms[i].frame >= GRAN_BOOM_FRAMES) {
                gBoomRelease(i);
                gBooms[i].state = 0;
                continue;
            }
            for (u16 q = 0; q < 4; q++)
                if (gBooms[i].s[q]) SPR_setFrame(gBooms[i].s[q], gBooms[i].frame);
        }
        live = TRUE;
        Sprite** sp = gBooms[i].s;
        for (u16 q = 1; q < 4; q++) {
            Sprite* m = sp[gBooms[i].owner[q]];
            if (!sp[q] || !m || gBooms[i].owner[q] == q) continue;
            u16 idx = m->attribut & TILE_INDEX_MASK;
            if ((sp[q]->attribut & TILE_INDEX_MASK) != idx) SPR_setVRAMTileIndex(sp[q], idx);
        }
        s16 x0 = (s16)(gBooms[i].cx - GRAN_BOOM_HALF - camX);
        s16 y0 = (s16)(gBooms[i].cy - GRAN_BOOM_HALF);
        for (u16 q = 0; q < 4; q++)
            if (sp[q])
                SPR_setPosition(sp[q], (s16)(x0 + (q & 1) * GRAN_BOOM_HALF),
                                       (s16)(y0 + (q >> 1) * GRAN_BOOM_HALF));
    }
    if (any && !live) sprDefragLock = 0;
}

static void gBoomsReleaseAll(void) {
    bool had = FALSE;
    for (u16 i = 0; i < GRAN_BOOMS; i++) {
        if (gBooms[i].state) had = TRUE;
        gBoomRelease(i);
        gBooms[i].state = 0;
    }
    if (had) sprDefragLock = 0;
}

// ---------------------------------------------------------------------------
// GRANITOR
// ---------------------------------------------------------------------------
static u16 granPal[16], granPalBurn[16];

void granitorInit(Granitor* g) {
    memset(g, 0, sizeof(Granitor));
    g->sprite = NULL;
    g->state = GRAN_INACTIVE;
    g->dir = -1;
    g->hp = diffBossHp(GRAN_HP); diffBossTrack(&g->hp);
    g->anim = 0xFF;
    g->laneTop = 142; g->laneBot = 196;
    g->topAt = NULL;
    flameInitAll();
    gBoomsInit();
}

void granitorSpawn(Granitor* g, s16 arenaLeft, s16 arenaRight, s16 laneTop, s16 laneBot,
                   GranitorTopAtFn topAt) {
    for (u16 i = 0; i < 16; i++) granPal[i] = granitor_boss.palette->data[i];
    bossFlashBurn(granPal, granPalBurn);
    PAL_setPalette(PAL3, granPal, DMA);
    bossFlashReset(&g->flashLow);
    if (!g->sprite)
        g->sprite = SPR_addSpriteSafe(&granitor_boss, -160, -160,
                                      TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    g->state = GRAN_ENTER;
    g->hp = diffBossHp(GRAN_HP); diffBossTrack(&g->hp);
    // (18/10) Cae desde arriba frente a la puerta del ascensor, en la mitad
    // de atras de la franja.
    g->xq = (s16)((arenaLeft + GRAN_DROP_DX) * 4);
    g->yq = (s16)((laneTop + (laneBot - laneTop) / 3) * 4);
    g->zq = GRAN_DROP_Z << 8;
    g->vz = 0;
    g->z = GRAN_DROP_Z;
    g->landed = 0;
    g->streak = 0;
    g->streakT = 0;
    g->closeCd = 0;
    g->dir = -1;
    g->anim = 0xFF;
    g->flash = 0;
    g->invuln = 0;
    g->timer = 0;
    g->arenaLeft = arenaLeft;
    g->arenaRight = arenaRight;
    g->laneTop = laneTop;
    g->laneBot = laneBot;
    g->topAt = topAt;
    if (g->sprite) {
        SPR_setVisibility(g->sprite, VISIBLE);
        SPR_setAutoAnimation(g->sprite, FALSE);   // pose del salto mientras cae
        SPR_setAnimAndFrame(g->sprite, GRAN_ANIM_IDLE2, 0);
        g->anim = GRAN_ANIM_IDLE2;
    }
}

bool granitorIsGone(const Granitor* g)  { return g->state == GRAN_GONE; }
bool granitorIsDying(const Granitor* g) { return g->state == GRAN_DEATH || g->state == GRAN_GONE; }

bool granitorCanBeHit(const Granitor* g) {
    // Solo quieto o caminando.
    return (g->state == GRAN_IDLE || g->state == GRAN_WALK) && g->sprite;
}

static void granSetAnim(Granitor* g, u8 a, bool loop) {
    if (g->anim == a) return;
    g->anim = a;
    SPR_setAutoAnimation(g->sprite, TRUE);
    SPR_setAnimAndFrame(g->sprite, a, 0);
    SPR_setAnimationLoop(g->sprite, loop);
}

static void granManual(Granitor* g, u8 a, u8 f) {
    g->anim = a;
    SPR_setAutoAnimation(g->sprite, FALSE);
    SPR_setAnimAndFrame(g->sprite, a, f);
}

// Tope de la lane en esta X: el de la arena y el de la pared del nivel.
static s16 granLaneTop(const Granitor* g, s16 x) {
    s16 t = g->laneTop;
    if (g->topAt) {
        s16 w = g->topAt(x);
        if (w > t) t = w;
    }
    return (t > g->laneBot) ? g->laneBot : t;
}

static void granEnter(Granitor* g, GranitorState s) {
    g->state = s;
    g->timer = 0;
}

static void granToIdle(Granitor* g)  { granEnter(g, GRAN_IDLE); }
static void granToWalk(Granitor* g)  { granEnter(g, GRAN_WALK); g->accX = g->accY = 0; }

static void granKill(Granitor* g) {
    g->hp = 0;
    granEnter(g, GRAN_DEATH);
    g->dPhase = 0;
    g->blinks = 0;
    g->z = 0;
    PAL_setPalette(PAL3, granPal, DMA);
    granManual(g, GRAN_ANIM_DAMAGE, GRAN_DMG_FLASH);
    XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 15, FALSE, FALSE);
    gBoomsChain((s16)(g->xq >> 2), (s16)(g->yq >> 2));   // (18/10)
}

// (18/10) Culatazo hacia 'dir' (contraataque o tortuga pegada).
static void granStartPunch(Granitor* g, s8 dir) {
    g->dir = dir;
    granEnter(g, GRAN_PUNCH);
    granManual(g, GRAN_ANIM_PUNCH, 0);
}

// (18/10) La tortuga en juego pegada a el (a culatazo, lane +-GRAN_CLOSE_DY);
// devuelve hacia donde esta (0 = ninguna). Con espera entre uno y otro: si no,
// pegado a una tortuga daba culatazos sin parar y no se lo podia golpear.
static s8 granCloseDir(Granitor* g, Player** pls, u8 nPl) {
    if (g->closeCd) return 0;
    s16 x = (s16)(g->xq >> 2), y = (s16)(g->yq >> 2);
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k]) || !playerCanBeHit(pls[k])) continue;
        s16 dx = (s16)(getPlayerHurtCX(pls[k]) - x);
        s16 dy = (s16)(getPlayerY(pls[k]) - y);
        if (gabs(dx) > GRAN_NEAR_DX || gabs(dy) > GRAN_CLOSE_DY) continue;
        g->closeCd = GRAN_CLOSE_CD;
        return (dx >= 0) ? 1 : -1;
    }
    return 0;
}

bool granitorPlayerHits(Granitor* g, Player** pls, u8 nPl, s8* killer) {
    if (!granitorCanBeHit(g) || g->invuln) return FALSE;
    s16 gx = (s16)(g->xq >> 2);
    s16 gy = (s16)(g->yq >> 2);
    for (u8 k = 0; k < nPl; k++) {
        s16 top = isPlayerJumping(pls[k]) ? GRAN_BODY_TOP_AIR : GRAN_BODY_TOP;
        if (!playerAttackHitsBossBand(pls[k], (s16)(gx + g->dir * GRAN_BODY_DX), gy, GRAN_BODY_HALF_W,
                                      (s16)(-top), (s16)(-GRAN_BODY_BOT),
                                      GRAN_HIT_DY_UP, GRAN_HIT_DY_DOWN)) continue;
        s16 dmg = isPlayerSpecialAttack(pls[k]) ? GRAN_SPECIAL_DMG
                : (isPlayerJumpKicking(pls[k]) ? GRAN_JUMPKICK_DMG : 1);
        // (16/10) Rock soldier: el "hit foot (hit rock soldier)" del arcade.
        XGM2_playPCMEx(hit_foot_sfx, sizeof(hit_foot_sfx), SOUND_PCM_CH3, 15, FALSE, FALSE);
        g->hp -= dmg;
        g->flash = 8;
        g->invuln = GRAN_HURT_TICKS;
        if (g->hp <= 0) {
            granKill(g);
            if (killer) *killer = (s8)k;
            return TRUE;
        }
        // (18/10) Anti-trabado: pasada la racha, este golpe lo absorbe y
        // contraataca con el culatazo hacia el que le pego.
        g->streakT = GRAN_STREAK_RESET;
        if (++g->streak > GRAN_STREAK_MAX) {
            g->streak = 0;
            g->flash = 0;
            granStartPunch(g, (getPlayerHurtCX(pls[k]) >= gx) ? 1 : -1);
            XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 12, FALSE, FALSE);
            return FALSE;
        }
        granEnter(g, GRAN_HURT);
        granManual(g, GRAN_ANIM_DAMAGE, GRAN_DMG_FLASH);
        XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 12, FALSE, FALSE);
        return FALSE;
    }
    return FALSE;
}

static void granitorRender(Granitor* g) {
    s16 x = (s16)(g->xq >> 2);
    s16 y = (s16)(g->yq >> 2);
    SPR_setHFlip(g->sprite, (g->dir < 0));
    SPR_setPosition(g->sprite, x - g->camX - GRAN_FRAME_W / 2, y - g->z - GRAN_FOOT_OFFSET);
    SPR_setDepth(g->sprite, (s16)(-y));
    SPR_setVisibility(g->sprite, (g->flash & 1) ? HIDDEN : VISIBLE);
}

// Al terminar un golpe, la recarga o el golpeado.
static void granAfterAction(Granitor* g, bool aligned) {
    if (aligned) granToWalk(g);
    else if (random() & 1) granToWalk(g);
    else granToIdle(g);
}

void granitorUpdate(Granitor* g, Player** pls, u8 nPl, s16 camX) {
    if (g->state == GRAN_INACTIVE || g->state == GRAN_GONE || !g->sprite) return;

    g->camX = camX;
    s16 x  = (s16)(g->xq >> 2);
    s16 y  = (s16)(g->yq >> 2);
    Player* tgt = nearestPlayer(pls, nPl, x);
    s16 px = (s16)(getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2);
    s16 py = getPlayerY(tgt);
    s16 dX = (s16)(px - x);
    s16 dY = (s16)(py - y);
    bool alignY = (gabs(dY) <= GRAN_ALIGN_DY);
    bool alNear = alignY && gabs(dX) <= GRAN_NEAR_DX;
    bool alFar  = alignY && gabs(dX) >  GRAN_FAR_DX;
    bool gameOn = !isPlayerGameOver(tgt);

    g->timer++;
    if (g->flash)    g->flash--;
    if (g->invuln)   g->invuln--;
    if (g->streakT && --g->streakT == 0) g->streak = 0;
    if (g->closeCd && g->state != GRAN_PUNCH) g->closeCd--;
    gBoomsUpdate(camX);

    switch (g->state) {

    case GRAN_ENTER:
        // (18/10) Cae desde arriba frente a la puerta; al tocar el piso
        // tiembla la pantalla y se queda un momento agachado.
        g->dir = (dX >= 0) ? 1 : -1;
        if (!g->landed) {
            g->vz -= GRAN_GRAV_Q * 2;
            g->zq += g->vz;
            if (g->zq <= 0) {
                g->zq = 0;
                g->landed = 1;
                g->timer = 0;
                stageShake(GRAN_SHAKE_LAND);
                XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                               SOUND_PCM_CH3, 15, FALSE, FALSE);
                granManual(g, GRAN_ANIM_IDLE1, 0);
            }
            g->z = (s16)(g->zq >> 8);
        } else if (g->timer >= GRAN_LAND_T) {
            granToIdle(g);
        }
        break;

    case GRAN_IDLE:
        granSetAnim(g, GRAN_ANIM_IDLE1, TRUE);
        g->dir = (dX >= 0) ? 1 : -1;
        if (!gameOn) break;
        {
            // (18/10) Una tortuga pegada (aunque no este en su lane exacta)
            // se come el culatazo apenas pasa la mitad de la espera.
            s8 cd = granCloseDir(g, pls, nPl);
            if (cd && g->timer >= GRAN_IDLE_T / 2) { granStartPunch(g, cd); break; }
        }
        if (g->timer < GRAN_IDLE_T) break;
        if (alNear) {
            granEnter(g, GRAN_PUNCH);
            granManual(g, GRAN_ANIM_PUNCH, 0);
        } else if (random() & 1) {
            granToWalk(g);
        } else {
            granEnter(g, GRAN_JUMP);       // salta y hace temblar el piso
            g->vz = GRAN_JUMP_VQ;
            g->zq = 0;
            g->landed = 0;
            granManual(g, GRAN_ANIM_IDLE2, 0);
        }
        break;

    case GRAN_WALK: {
        s8 cdw;
        granSetAnim(g, GRAN_ANIM_WALK, TRUE);
        g->dir = (dX >= 0) ? 1 : -1;
        s16 sx = gStep(&g->accX, GRAN_WALK_Q);
        s16 sy = gStep(&g->accY, GRAN_WALK_Q);
        if (gabs(dX) > GRAN_MIN_DIST) g->xq += (dX >= 0) ? sx * 4 : -sx * 4;
        g->yq = (s16)(gToward(y, py, sy) * 4);
        if (!gameOn) break;
        if (alNear) {
            granEnter(g, GRAN_PUNCH);
            granManual(g, GRAN_ANIM_PUNCH, 0);
        } else if ((cdw = granCloseDir(g, pls, nPl)) != 0) {  // (18/10)
            granStartPunch(g, cdw);
        } else if (alFar && (random() % 3) != 0) {
            granEnter(g, GRAN_FIRE);
            granManual(g, GRAN_ANIM_FIRE, 0);
        } else if (g->timer > GRAN_WALK_MAX) {
            granToIdle(g);
        }
        break;
    }

    case GRAN_FIRE:
        // Chorro de llamas: una cada GRAN_FLAME_EVERY frames desde el arma.
        if ((g->timer % GRAN_FLAME_EVERY) == 1)
            flameFire((s16)(x + g->dir * GRAN_FLAME_DX), y, g->dir);
        if (g->timer >= GRAN_FIRE_T) granAfterAction(g, alignY);
        break;

    case GRAN_PUNCH: {
        // 21 frames: 0-1-2 de preparacion y el 3 (impacto) hasta el final;
        // pega solo en el primer tramo del impacto.
        u8 f = (g->timer < 5) ? 0 : (g->timer < 10) ? 1 : (g->timer < 14) ? 2 : 3;
        SPR_setFrame(g->sprite, f);
        if (g->timer >= 14 && g->timer < 14 + GRAN_PUNCH_HIT_T) {
            s16 hx = (s16)(x + g->dir * GRAN_PUNCH_HIT_DX);
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                s16 kx = getPlayerHurtCX(pls[k]);
                if (gabs((s16)(kx - hx)) >= GRAN_PUNCH_HIT_W) continue;
                s16 ky = (s16)(getPlayerY(pls[k]) - y);
                if (ky < -GRAN_HIT_DY_UP || ky > GRAN_HIT_DY_DOWN) continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
                playerHitBarsKnockdown(pls[k], x, GRAN_PUNCH_DMG);
            }
        }
        if (g->timer >= GRAN_PUNCH_T) granAfterAction(g, alignY);
        break;
    }

    case GRAN_JUMP:
        // Salto en el lugar; al caer tiembla el piso: le saca una barra a
        // toda tortuga que este parada. Medio segundo despues, quieto.
        if (!g->landed) {
            g->zq += g->vz;
            g->vz -= GRAN_GRAV_Q;
            if (g->zq <= 0) {
                g->zq = 0;
                g->landed = 1;
                g->timer = 0;
                stageShake(GRAN_SHAKE_LAND);       // (18/10)
                XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                               SOUND_PCM_CH3, 15, FALSE, FALSE);
                for (u8 k = 0; k < nPl; k++) {
                    if (!playerCanBeHit(pls[k])) continue;      // saltando no le llega
                    playerHitBars(pls[k], x, GRAN_QUAKE_DMG);
                }
                granManual(g, GRAN_ANIM_IDLE1, 0);
            }
            g->z = (s16)(g->zq >> 8);
        } else if (g->timer >= GRAN_LAND_T) {
            granToIdle(g);
        }
        break;

    case GRAN_HURT:
        if (g->timer >= GRAN_HURT_TICKS) {
            s8 cd = granCloseDir(g, pls, nPl);       // (18/10) pegado: culatazo
            if (cd) granStartPunch(g, cd);
            else    granAfterAction(g, alignY);
        }
        break;

    case GRAN_DEATH:
        g->flash = 0;
        if (g->dPhase == 0) {
            if (g->timer > 8) {
                g->timer = 0;
                g->blinks++;
                SPR_setFrame(g->sprite, (g->blinks & 1) ? GRAN_DMG_FLASH : GRAN_DMG_BREAK);
                if (g->blinks >= GRAN_DEATH_BLINK * 2) {
                    g->dPhase = 1;
                    SPR_setFrame(g->sprite, GRAN_DMG_BREAK);
                }
            }
        } else if (g->dPhase == 1) {
            if (g->timer > GRAN_DEATH_BREAK_T) {
                g->dPhase = 2;
                g->timer = 0;
                SPR_setFrame(g->sprite, GRAN_DMG_PILE);
                XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                               SOUND_PCM_CH3, 15, FALSE, FALSE);
            }
        } else if (g->timer > GRAN_DEATH_PILE_T) {
            gBoomsReleaseAll();                  // (18/10)
            g->state = GRAN_GONE;
            SPR_releaseSprite(g->sprite);
            g->sprite = NULL;
            granitorFlameReleaseAll();
            return;
        }
        break;

    default:
        break;
    }

    // Parpadeo de vida baja (PAL3 es solo suya y de sus llamas).
    if (g->state != GRAN_DEATH && bossFlashStep(&g->flashLow, g->hp, diffBossHp(GRAN_HP)))
        PAL_setPalette(PAL3, g->flashLow.on ? granPalBurn : granPal, DMA);

    // Arena y lane (con la pared del nivel).
    x = gclamp((s16)(g->xq >> 2), g->arenaLeft, g->arenaRight);
    g->xq = (s16)(x * 4);
    y = gclamp((s16)(g->yq >> 2), granLaneTop(g, x), g->laneBot);
    g->yq = (s16)(y * 4);

    granitorRender(g);
}

void granitorRelease(Granitor* g) {
    if (g->sprite) { SPR_releaseSprite(g->sprite); g->sprite = NULL; }
    g->state = GRAN_INACTIVE;
    granitorFlameReleaseAll();
    gBoomsReleaseAll();
}
