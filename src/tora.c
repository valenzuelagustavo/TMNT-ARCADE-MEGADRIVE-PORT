#include "tora.h"
#include "difficulty.h"    // diffBossHp / diffBossTrack
#include "audio.h"         // mouser_hit_sfx, boss_hit, hit_turtles, window_broken_sfx
#include "boss_flash.h"
#include "stage_level.h"
#include "enemy.h"         // sprVramFits
#include "boss_vo.h"       // (24/10) bossVoActive: se queda quieto mientras habla

// ===========================================================================
// TORA + ESFERAS DE HIELO — ver tora.h (23/10)
// ===========================================================================

static s16 tabs(s16 v)                 { return (v < 0) ? -v : v; }
static s16 tclamp(s16 v, s16 a, s16 b) { return (v < a) ? a : ((v > b) ? b : v); }

static s16 tStep(u8* acc, u16 q) {
    u16 s = (u16)(*acc + q);
    *acc = (u8)(s & 0xFF);
    return (s16)(s >> 8);
}

static s16 tToward(s16 v, s16 to, s16 step) {
    if (v < to) return (to - v < step) ? to : (s16)(v + step);
    if (v > to) return (v - to < step) ? to : (s16)(v - step);
    return v;
}

static Player* toraNearest(Player** pls, u8 nPl, s16 x) {
    Player* best = pls[0];
    s16 bestD = 0x7FFF;
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k])) continue;
        s16 d = tabs((s16)(getPlayerHurtCX(pls[k]) - x));
        if (d < bestD) { bestD = d; best = pls[k]; }
    }
    return best;
}

static u16 toraPal[16], toraPalBurn[16];

void toraLoadPal(void) {
    for (u16 i = 0; i < 16; i++) toraPal[i] = snow_tora.palette->data[i];
    bossFlashBurn(toraPal, toraPalBurn);
    PAL_setPalette(PAL3, toraPal, DMA);
}

// ---------------------------------------------------------------------------
// ESFERAS DE HIELO
// Celda 80x72 con la esfera centrada en (40,36); sombra 56x24 centrada en
// (28,12). z = altura del CENTRO de la esfera sobre el piso (lane).
// ---------------------------------------------------------------------------
#define ICE_CELL_CX    40
#define ICE_CELL_CY    36
#define ICE_SH_CX      28
#define ICE_SH_CY      12
#define ICE_FALL_V0Q   32      // Q4: 2 px/f al empezar a caer
#define ICE_FALL_GQ     4      // Q4: +0,25 px/f por frame
#define ICE_HELD_VQ    96      // Q4: 6 px/f cayendo a las manos
#define ICE_ARC_H      10      // panza del arco (23/10: 40 -> 10, no se mete en el HUD)
#define ICE_BRK1        6
#define ICE_BRK2       14

enum { ICE_FREE, ICE_WARN, ICE_FALL, ICE_HELD, ICE_THROWN, ICE_BREAK };

static struct {
    u8      mode;
    Sprite* s;
    Sprite* sh;
    s16     x, lane;
    s16     zq, vzq;           // Q4
    u16     t;
    s16     x0, l0, x1, l1, z0; // arco
    u8      dmg;
    u8      hitMask;
} ice[TORA_ICE_MAX];

static s16 iceCamX;

static void iceFree(u16 i) {
    if (ice[i].s)  { SPR_releaseSprite(ice[i].s);  ice[i].s  = NULL; }
    if (ice[i].sh) { SPR_releaseSprite(ice[i].sh); ice[i].sh = NULL; }
    ice[i].mode = ICE_FREE;
}

void toraIceInit(void) {
    for (u16 i = 0; i < TORA_ICE_MAX; i++) {
        ice[i].mode = ICE_FREE;
        ice[i].s = ice[i].sh = NULL;
    }
}

void toraIceReleaseAll(void) {
    for (u16 i = 0; i < TORA_ICE_MAX; i++) iceFree(i);
}

u16 toraIceAlive(void) {
    u16 n = 0;
    for (u16 i = 0; i < TORA_ICE_MAX; i++) if (ice[i].mode != ICE_FREE) n++;
    return n;
}

// Un lugar libre con VRAM para la esfera (y la sombra si withShadow).
static s16 iceAlloc(bool withShadow) {
    for (u16 i = 0; i < TORA_ICE_MAX; i++) {
        if (ice[i].mode != ICE_FREE) continue;
        u16 need = (u16)(snow_iceball.maxNumTile + (withShadow ? snow_iceshadow.maxNumTile : 0));
        if (!sprVramFits(need)) return -1;
        if (withShadow) {
            ice[i].sh = SPR_addSprite(&snow_iceshadow, -128, -128, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
            if (!ice[i].sh) return -1;
            SPR_setVisibility(ice[i].sh, HIDDEN);
        }
        ice[i].s = SPR_addSprite(&snow_iceball, -128, -128, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        if (!ice[i].s) { iceFree(i); return -1; }
        SPR_setAutoAnimation(ice[i].s, FALSE);
        SPR_setAnimAndFrame(ice[i].s, 0, 0);
        SPR_setVisibility(ice[i].s, HIDDEN);
        ice[i].t = 0;
        ice[i].hitMask = 0;
        ice[i].vzq = 0;
        return (s16)i;
    }
    return -1;
}

bool toraIceRain(s16 x, s16 lane) {
    s16 i = iceAlloc(TRUE);
    if (i < 0) return FALSE;
    ice[i].mode = ICE_WARN;
    ice[i].x = x;
    ice[i].lane = lane;
    ice[i].zq = TORA_ICE_SKY_Z << 4;
    ice[i].dmg = (dificultad == DIFICULTAD_ARCADE) ? TORA_RAIN_DMG_ARCADE : TORA_RAIN_DMG;
    return TRUE;
}

// La del jefe: cae del cielo hasta sus manos.
static s16 iceHeldStart(s16 x, s16 lane) {
    s16 i = iceAlloc(TRUE);
    if (i < 0) return -1;
    ice[i].mode = ICE_HELD;
    ice[i].x = x;
    ice[i].lane = lane;
    ice[i].zq = TORA_ICE_SKY_Z << 4;
    ice[i].dmg = TORA_BALL_DMG;
    return i;
}

static bool iceHeldLanded(s16 i) {
    return i >= 0 && ice[i].mode == ICE_HELD && ice[i].zq <= (TORA_HOLD_Z << 4);
}

static void iceBreak(u16 i) {
    if (ice[i].mode == ICE_FREE || ice[i].mode == ICE_BREAK) return;
    ice[i].mode = ICE_BREAK;
    ice[i].t = 0;
    if (ice[i].sh) SPR_setVisibility(ice[i].sh, HIDDEN);
    if (ice[i].s) SPR_setFrame(ice[i].s, 1);
    XGM2_playPCMEx(window_broken_sfx, sizeof(window_broken_sfx), SOUND_PCM_CH3, 10, FALSE, FALSE);
}

static void iceThrow(s16 i, s16 x1, s16 l1) {
    if (i < 0 || ice[i].mode != ICE_HELD) return;
    ice[i].mode = ICE_THROWN;
    ice[i].t = 0;
    ice[i].x0 = ice[i].x;  ice[i].l0 = ice[i].lane;
    ice[i].x1 = x1;        ice[i].l1 = l1;
    ice[i].z0 = (s16)(ice[i].zq >> 4);
    ice[i].hitMask = 0;
}

// Aplasta / golpea a las tortugas en su camino. TRUE si pego.
static bool iceHits(u16 i, Player** pls, u8 nPl, s16 fromX) {
    s16 bottom = (s16)((ice[i].zq >> 4) - TORA_ICE_R);
    if (bottom >= TORA_ICE_HIT_Z) return FALSE;
    bool hit = FALSE;
    for (u8 k = 0; k < nPl; k++) {
        u8 bit = (u8)(1 << k);
        if ((ice[i].hitMask & bit) || !playerCanBeHit(pls[k])) continue;
        if (tabs((s16)(getPlayerHurtCX(pls[k]) - ice[i].x)) >= TORA_ICE_HIT_DX) continue;
        if (tabs((s16)(getPlayerY(pls[k]) - ice[i].lane)) > TORA_ICE_HIT_DY) continue;
        ice[i].hitMask |= bit;
        XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
        playerHitBarsKnockdown(pls[k], fromX, ice[i].dmg);
        hit = TRUE;
    }
    return hit;
}

void toraIceUpdate(Player** pls, u8 nPl, s16 camX) {
    iceCamX = camX;
    for (u16 i = 0; i < TORA_ICE_MAX; i++) {
        if (ice[i].mode == ICE_FREE) continue;
        ice[i].t++;
        bool shadow = FALSE, ball = TRUE;
        switch (ice[i].mode) {
        case ICE_WARN:
            // La sombra sola: parpadea la primera mitad, despues fija.
            ball = FALSE;
            shadow = (ice[i].t > TORA_ICE_WARN / 2) || ((ice[i].t >> 2) & 1);
            if (ice[i].t >= TORA_ICE_WARN) {
                ice[i].mode = ICE_FALL;
                ice[i].t = 0;
                ice[i].vzq = ICE_FALL_V0Q;
            }
            break;
        case ICE_FALL:
            shadow = TRUE;
            ice[i].zq -= ice[i].vzq;
            ice[i].vzq += ICE_FALL_GQ;
            if (ice[i].zq <= (TORA_ICE_R << 4)) ice[i].zq = TORA_ICE_R << 4;
            iceHits(i, pls, nPl, (s16)(ice[i].x - 1));
            if (ice[i].zq <= (TORA_ICE_R << 4)) { iceBreak(i); stageShake(6); }
            break;
        case ICE_HELD:
            // Cae a las manos del jefe (el jefe le pone x / lane).
            if (ice[i].zq > (TORA_HOLD_Z << 4)) {
                ice[i].zq -= ICE_HELD_VQ;
                if (ice[i].zq < (TORA_HOLD_Z << 4)) ice[i].zq = TORA_HOLD_Z << 4;
            }
            break;
        case ICE_THROWN: {
            shadow = TRUE;
            s32 t = ice[i].t, T = TORA_THROW_AIR;
            ice[i].x    = (s16)(ice[i].x0 + (s32)(ice[i].x1 - ice[i].x0) * t / T);
            ice[i].lane = (s16)(ice[i].l0 + (s32)(ice[i].l1 - ice[i].l0) * t / T);
            s32 z = ice[i].z0 + (s32)(TORA_ICE_R - ice[i].z0) * t / T
                  + (s32)4 * ICE_ARC_H * t * (T - t) / (T * T);
            ice[i].zq = (s16)(z << 4);
            s16 from = (ice[i].x1 >= ice[i].x0) ? (s16)(ice[i].x - 1) : (s16)(ice[i].x + 1);
            if (iceHits(i, pls, nPl, from) || t >= T) { iceBreak(i); if (t >= T) stageShake(6); }
            break;
        }
        case ICE_BREAK:
            if (ice[i].t == ICE_BRK1 && ice[i].s) SPR_setFrame(ice[i].s, 2);
            if (ice[i].t >= ICE_BRK2) { iceFree(i); continue; }
            break;
        default: break;
        }

        s16 sx = (s16)(ice[i].x - camX);
        if (ice[i].sh) {
            SPR_setVisibility(ice[i].sh, shadow ? VISIBLE : HIDDEN);
            if (shadow) {
                SPR_setPosition(ice[i].sh, (s16)(sx - ICE_SH_CX),
                                (s16)(ice[i].lane - ICE_SH_CY - stageCamY));
                SPR_setDepth(ice[i].sh, (s16)(-ice[i].lane + 1));
            }
        }
        if (ice[i].s) {
            // (23/10) Nunca en las filas del HUD: el HUD tiene sprites (los
            // recuadros) y con los 6 de la esfera en las mismas lineas se pasa
            // el limite de sprites por linea y el HUD se rompe. La que cae del
            // cielo aparece recien debajo del HUD.
            s16 z = (s16)(ice[i].zq >> 4);
            s16 top = (s16)(ice[i].lane - z - ICE_CELL_CY - stageCamY);
            if (top < TORA_HUD_BOTTOM) ball = FALSE;
            SPR_setVisibility(ice[i].s, ball ? VISIBLE : HIDDEN);
            if (ball) {
                SPR_setPosition(ice[i].s, (s16)(sx - ICE_CELL_CX), top);
                SPR_setDepth(ice[i].s, (s16)(-ice[i].lane - 2));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// TORA
// ---------------------------------------------------------------------------
void toraInit(Tora* t) {
    memset(t, 0, sizeof(Tora));
    t->sprite = NULL;
    t->state = TORA_INACTIVE;
    t->dir = -1;
    t->ball = -1;
    t->anim = 0xFF;
    t->hp = diffBossHp(TORA_HP); diffBossTrack(&t->hp);
    toraIceInit();
}

static void toraEnter(Tora* t, ToraState s) { t->state = s; t->timer = 0; }

// (23/10) La hoja sale de rescomp con tiempo 0 (sin animacion automatica):
// la caminata se avanza a mano, un cuadro cada 'ticks' frames.
static void toraWalk(Tora* t, u8 ticks) {
    if (t->anim != TORA_ANIM_WALK) {
        t->anim = TORA_ANIM_WALK;
        t->walkT = 0;
        SPR_setAutoAnimation(t->sprite, FALSE);
        SPR_setAnimAndFrame(t->sprite, TORA_ANIM_WALK, 0);
    }
    t->walkT++;
    SPR_setFrame(t->sprite, (s16)((t->walkT / ticks) & 3));
}

static void toraManual(Tora* t, u8 a, u8 f) {
    t->anim = a;
    SPR_setAutoAnimation(t->sprite, FALSE);
    SPR_setAnimAndFrame(t->sprite, a, f);
}

static void toraNextDash(Tora* t) {
    t->dashT = (u16)(TORA_DASH_EVERY + (random() % TORA_DASH_RND));
}

void toraSpawn(Tora* t, s16 arenaLeft, s16 arenaRight, s16 laneTop, s16 laneBot,
               ToraTopAtFn topAt) {
    toraLoadPal();
    bossFlashReset(&t->flashLow);
    if (!t->sprite)
        t->sprite = SPR_addSpriteSafe(&snow_tora, -160, -160, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    t->hp = diffBossHp(TORA_HP); diffBossTrack(&t->hp);
    t->arenaLeft = arenaLeft;   t->arenaRight = arenaRight;
    t->laneTop = laneTop;       t->laneBot = laneBot;
    t->topAt = topAt;
    // Entra caminando desde afuera, por la derecha, en el medio de la franja.
    t->xq = (s16)((arenaRight + 72) * 4);
    t->yq = (s16)(((laneTop + laneBot) / 2) * 4);
    t->dir = -1;
    t->ball = -1;
    t->flash = t->invuln = 0;
    t->streak = t->streakT = 0;
    t->punchCd = 0;
    t->anim = 0xFF;
    toraNextDash(t);
    toraEnter(t, TORA_ENTER);
    if (t->sprite) {
        SPR_setVisibility(t->sprite, VISIBLE);
        toraWalk(t, TORA_WALK_TICKS);
    }
}

bool toraIsGone(const Tora* t)  { return t->state == TORA_GONE; }
bool toraJustArrived(const Tora* t) { return t->arrived; }

static void toraSayRelease(Tora* t) {
    if (t->say) { SPR_releaseSprite(t->say); t->say = NULL; }
}
bool toraIsDying(const Tora* t) { return t->state == TORA_DEATH || t->state == TORA_GONE; }

static bool toraCanBeHit(const Tora* t) {
    return t->sprite && t->state != TORA_INACTIVE && t->state != TORA_ENTER &&
           t->state != TORA_TAUNT &&
           t->state != TORA_DEATH && t->state != TORA_GONE;
}

static s16 toraLaneTop(const Tora* t, s16 x) {
    s16 top = t->laneTop;
    if (t->topAt) { s16 w = t->topAt(x); if (w > top) top = w; }
    return (top > t->laneBot) ? t->laneBot : top;
}

static void toraDropBall(Tora* t) {
    if (t->ball >= 0) { iceBreak((u16)t->ball); t->ball = -1; }
}

static void toraStartDash(Tora* t, s16 px) {
    s16 x = (s16)(t->xq >> 2);
    s16 mid = (s16)((t->arenaLeft + t->arenaRight) / 2);
    t->dashTo = (px >= mid) ? (s16)(t->arenaLeft + TORA_EDGE_IN) : (s16)(t->arenaRight - TORA_EDGE_IN);
    t->dir = (t->dashTo >= x) ? 1 : -1;
    toraEnter(t, TORA_DASH);
    t->anim = 0xFF;
    toraWalk(t, TORA_DASH_TICKS);
}

static void toraKill(Tora* t) {
    t->hp = 0;
    toraSayRelease(t);
    toraDropBall(t);
    toraEnter(t, TORA_DEATH);
    t->blinks = 0;
    PAL_setPalette(PAL3, toraPal, DMA);
    toraManual(t, TORA_ANIM_DEATH, 0);
    XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 15, FALSE, FALSE);
}

bool toraPlayerHits(Tora* t, Player** pls, u8 nPl, s8* killer) {
    if (!toraCanBeHit(t) || t->invuln) return FALSE;
    s16 x = (s16)(t->xq >> 2), y = (s16)(t->yq >> 2);
    for (u8 k = 0; k < nPl; k++) {
        s16 top = isPlayerJumping(pls[k]) ? TORA_BODY_TOP_AIR : TORA_BODY_TOP;
        if (!playerAttackHitsBossBand(pls[k], x, y, TORA_BODY_HALF_W,
                                      (s16)(-top), (s16)(-TORA_BODY_BOT),
                                      TORA_HIT_DY_UP, TORA_HIT_DY_DOWN)) continue;
        s16 dmg = isPlayerSpecialAttack(pls[k]) ? TORA_SPECIAL_DMG
                : (isPlayerJumpKicking(pls[k]) ? TORA_JUMPKICK_DMG : 1);
        // Mismo sonido que los robots del latigo y los otros jefes.
        XGM2_playPCMEx(mouser_hit_sfx, sizeof(mouser_hit_sfx), SOUND_PCM_CH3, 15, FALSE, FALSE);
        t->hp -= dmg;
        t->flash = 8;
        t->invuln = TORA_HURT_TICKS;
        if (t->hp <= 0) {
            toraKill(t);
            if (killer) *killer = (s8)k;
            return TRUE;
        }
        bool stagger = (t->state == TORA_CHASE || t->state == TORA_PUNCH || t->state == TORA_HURT);
        if (!stagger) return FALSE;                  // corriendo / con la esfera: super armor
        t->streakT = TORA_STREAK_RESET;
        if (++t->streak > TORA_STREAK_MAX) {         // anti-trabado: se escapa a lanzar
            t->streak = 0;
            t->flash = 0;
            toraStartDash(t, getPlayerHurtCX(pls[k]));
            return FALSE;
        }
        toraEnter(t, TORA_HURT);
        toraManual(t, TORA_ANIM_HURT, 0);
        return FALSE;
    }
    return FALSE;
}

static void toraRender(Tora* t) {
    s16 x = (s16)(t->xq >> 2), y = (s16)(t->yq >> 2);
    SPR_setHFlip(t->sprite, t->dir < 0);
    SPR_setPosition(t->sprite, (s16)(x - t->camX - TORA_CX), (s16)(y - TORA_FRAME_H - stageCamY));
    SPR_setDepth(t->sprite, (s16)(-y));
}

void toraUpdate(Tora* t, Player** pls, u8 nPl, s16 camX) {
    if (t->state == TORA_INACTIVE || t->state == TORA_GONE || !t->sprite) return;
    t->camX = camX;
    s16 x = (s16)(t->xq >> 2), y = (s16)(t->yq >> 2);
    Player* tgt = toraNearest(pls, nPl, x);
    s16 px = getPlayerHurtCX(tgt);
    s16 py = getPlayerY(tgt);
    s16 dX = (s16)(px - x), dY = (s16)(py - y);
    bool gameOn = !isPlayerGameOver(tgt);

    t->timer++;
    t->arrived = FALSE;
    if (t->flash)   t->flash--;
    if (t->invuln)  t->invuln--;
    if (t->punchCd) t->punchCd--;
    if (t->streakT && --t->streakT == 0) t->streak = 0;
    if (t->dashT && t->state == TORA_CHASE) t->dashT--;

    switch (t->state) {

    case TORA_ENTER:
        toraWalk(t, TORA_WALK_TICKS);
        t->dir = -1;
        t->xq -= 6;                                   // 1,5 px/f
        if ((t->xq >> 2) <= t->arenaRight - TORA_ENTER_DX) {
            // (24/10) Llego: se para y habla, con su globo.
            toraEnter(t, TORA_TAUNT);
            t->arrived = TRUE;
            toraManual(t, TORA_ANIM_WALK, 0);
            if (!t->say)
                t->say = SPR_addSprite(&tora_ice_bubble, -128, -128, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
            if (t->say) SPR_setDepth(t->say, SPR_MIN_DEPTH);
        }
        break;

    case TORA_TAUNT:
        t->dir = (dX >= 0) ? 1 : -1;
        if (t->say) {
            s16 bx = tclamp((s16)(x - camX - TORA_SAY_TAIL_X), 4, (s16)(320 - TORA_SAY_W - 4));
            s16 by = (s16)(y - TORA_SAY_DY - TORA_SAY_H - stageCamY);
            if (by < TORA_SAY_TOP_MIN) by = TORA_SAY_TOP_MIN;
            SPR_setPosition(t->say, bx, by);
        }
        // timer > 1: la voz arranca en este mismo frame (el nivel la ve en
        // toraJustArrived despues de toraUpdate).
        if ((t->timer > 1 && t->timer >= TORA_TAUNT_MIN && !bossVoActive()) ||
            t->timer >= TORA_TAUNT_MAX) {
            toraSayRelease(t);
            t->anim = 0xFF;
            toraEnter(t, TORA_CHASE);
        }
        break;

    case TORA_CHASE: {
        toraWalk(t, TORA_WALK_TICKS);
        t->dir = (dX >= 0) ? 1 : -1;
        if (!gameOn) break;
        if (t->dashT == 0) { toraStartDash(t, px); break; }
        if (t->punchCd == 0 && tabs(dX) <= TORA_NEAR_DX && tabs(dY) <= TORA_ALIGN_DY) {
            toraEnter(t, TORA_PUNCH);
            toraManual(t, TORA_ANIM_PUNCH, 0);
            break;
        }
        s16 sx = tStep(&t->accX, TORA_WALK_Q);
        s16 sy = tStep(&t->accY, TORA_LANE_Q);
        if (tabs(dX) > TORA_MIN_DIST) t->xq += (dX >= 0) ? sx * 4 : -sx * 4;
        t->yq = (s16)(tToward(y, py, sy) * 4);
        break;
    }

    case TORA_PUNCH:
        if (t->timer == TORA_PUNCH_WIND) SPR_setFrame(t->sprite, 1);
        if (t->timer >= TORA_PUNCH_WIND && t->timer < TORA_PUNCH_WIND + TORA_PUNCH_HIT_T) {
            s16 hx = (s16)(x + t->dir * TORA_PUNCH_HIT_DX);
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                if (tabs((s16)(getPlayerHurtCX(pls[k]) - hx)) >= TORA_PUNCH_HIT_W) continue;
                s16 ky = (s16)(getPlayerY(pls[k]) - y);
                if (ky < -TORA_HIT_DY_UP || ky > TORA_HIT_DY_DOWN) continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
                playerHitBarsKnockdown(pls[k], x, TORA_PUNCH_DMG);
            }
        }
        if (t->timer >= TORA_PUNCH_T) {
            t->punchCd = TORA_PUNCH_CD;
            t->anim = 0xFF;
            toraEnter(t, TORA_CHASE);
        }
        break;

    case TORA_DASH: {
        s16 d = (s16)(t->dashTo - x);
        // (23/10) Tambien baja hacia adelante: con la esfera en las manos
        // tiene que quedar debajo del HUD (TORA_CATCH_LANE).
        if (y < TORA_CATCH_LANE) t->yq += 2 * 4;
        if (tabs(d) <= TORA_DASH_PX && y >= TORA_CATCH_LANE - 2) {
            t->xq = (s16)(t->dashTo * 4);
            t->dir = (dX >= 0) ? 1 : -1;
            toraEnter(t, TORA_CATCH);
            toraManual(t, TORA_ANIM_THROW, 0);
            t->ball = iceHeldStart((s16)(t->dashTo + t->dir * TORA_HOLD_DX), y);
            if (t->ball < 0) { toraNextDash(t); t->anim = 0xFF; toraEnter(t, TORA_CHASE); }
        } else {
            toraWalk(t, TORA_DASH_TICKS);
            if (tabs(d) > TORA_DASH_PX) t->xq += (d > 0) ? TORA_DASH_PX * 4 : -TORA_DASH_PX * 4;
            else t->xq = (s16)(t->dashTo * 4);
        }
        break;
    }

    case TORA_CATCH:
        t->dir = (dX >= 0) ? 1 : -1;
        if (t->ball >= 0) {
            ice[(u8)t->ball].x = (s16)(x + t->dir * TORA_HOLD_DX);
            ice[(u8)t->ball].lane = y;
        }
        if (!iceHeldLanded(t->ball)) { t->timer = 0; break; }
        if (t->timer >= TORA_HOLD_T) {
            s16 dist = tclamp(tabs(dX), TORA_THROW_MIN, TORA_THROW_MAX);
            s16 x1 = tclamp((s16)(x + t->dir * dist), (s16)(t->arenaLeft - 16), (s16)(t->arenaRight + 16));
            s16 l1 = tclamp(py, t->laneTop, t->laneBot);
            iceThrow(t->ball, x1, l1);
            t->ball = -1;
            toraEnter(t, TORA_THROW);
            toraManual(t, TORA_ANIM_THROW, 1);
        }
        break;

    case TORA_THROW:
        if (t->timer >= TORA_THROW_T) {
            toraNextDash(t);
            t->anim = 0xFF;
            toraEnter(t, TORA_CHASE);
        }
        break;

    case TORA_HURT:
        if (t->timer >= TORA_HURT_TICKS) { t->anim = 0xFF; toraEnter(t, TORA_CHASE); }
        break;

    case TORA_DEATH:
        t->flash = 0;
        if (t->timer < 3 * TORA_DEATH_STEP) {
            SPR_setFrame(t->sprite, (s16)(t->timer / TORA_DEATH_STEP));
        } else if (((t->timer - 3 * TORA_DEATH_STEP) & 7) == 0) {
            if (++t->blinks > TORA_DEATH_BLINK * 2) {
                SPR_releaseSprite(t->sprite);
                t->sprite = NULL;
                t->state = TORA_GONE;
                return;
            }
            SPR_setVisibility(t->sprite, (t->blinks & 1) ? HIDDEN : VISIBLE);
        }
        break;

    default:
        break;
    }

    if (t->state != TORA_DEATH && bossFlashStep(&t->flashLow, t->hp, diffBossHp(TORA_HP)))
        PAL_setPalette(PAL3, t->flashLow.on ? toraPalBurn : toraPal, DMA);

    if (t->state != TORA_ENTER) {
        x = tclamp((s16)(t->xq >> 2), t->arenaLeft, t->arenaRight);
        t->xq = (s16)(x * 4);
    }
    x = (s16)(t->xq >> 2);
    y = tclamp((s16)(t->yq >> 2), toraLaneTop(t, x), t->laneBot);
    t->yq = (s16)(y * 4);

    if (t->state != TORA_DEATH)
        SPR_setVisibility(t->sprite, (t->flash & 1) ? HIDDEN : VISIBLE);
    toraRender(t);
}

void toraRelease(Tora* t) {
    if (t->sprite) { SPR_releaseSprite(t->sprite); t->sprite = NULL; }
    toraSayRelease(t);
    t->state = TORA_INACTIVE;
    t->ball = -1;
    toraIceReleaseAll();
}
