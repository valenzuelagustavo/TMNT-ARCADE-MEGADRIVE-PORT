// ===========================================================================
// techno_tubes.c - tubos de spray congelante del TECHNODROME (25/10)
// ===========================================================================
// Ver techno_tubes.h.
#include <genesis.h>
#include "techno_tubes.h"
#include "level8_1_far.h"   // techno_tube, techno_spray
#include "audio.h"
#include "enemy.h"          // sprVramFits

#define SCREEN_W        320

#define TT_MAX            3
#define TT_HITS           1        // golpes para destruirlo (26/10: de uno)
#define TT_HIT_CD        14        // un golpe por swing
#define TT_BODY_H        50
#define TT_HALF_W         9

// Tiempos (cuadros a 60 Hz)
#define TT_RISE_N         6        // cuadros de la salida (0..5 del sprite)
#define TT_AIM_T         16        // entero, apuntando, antes del chorro
#define TT_SPRAY_T       80        // largo de cada chorro
#define TT_FADE_T         6        // cuadros ralos al empezar y al terminar
#define TT_PAUSE_T       24        // entre un chorro y el otro
#define TT_BURSTS         2
#define TT_SINK_STEP      5        // cuadros por cuadro al hundirse
#define TT_DEAD_T        50        // destruido: queda y parpadea
#define TT_BOOM_BLINK    18

// Chorro: hasta donde llega (px desde la boquilla) y tolerancia de lane
#define TT_REACH         58
#define TT_DY            10
#define TT_DMG            2        // barras

// Aparicion
#define TT_SPAWN_MIN      70       // cuadros entre un tubo y el siguiente
#define TT_SPAWN_RND      60
#define TT_AHEAD_MIN      70       // a que distancia al frente de la tortuga
#define TT_AHEAD_RND      70
#define TT_NEAR_PLAYER    44       // nunca mas cerca que esto (X) de una tortuga en su lane

// Cuadros del sprite
#define TF_RISE_LAST       5
#define TF_FIRE            6
#define TF_DEAD            7
#define TS_SPRAY_A         0
#define TS_SPRAY_B         1
#define TS_FADE_A          2
#define TS_FADE_B          3

#define TT_SPR_W          16
#define TT_SPR_H          56
#define TT_SPRAY_W        64
#define TT_NOZZLE_DY      23       // el sprite del spray empieza tanto ARRIBA del del tubo

typedef enum { TS_OFF, TS_RISE, TS_AIM, TS_SPRAY, TS_PAUSE, TS_SINK, TS_DEAD } TubeState;

typedef struct {
    u8      st;
    s8      dir;           // hacia donde tira: -1 izquierda, +1 derecha
    s16     x, lane;       // mundo (centro, pies)
    u16     t;
    u8      burst;
    u8      hits;
    u8      hitCd;
    u8      hitMask;
    Sprite* s;
    Sprite* sp;            // el spray (solo mientras tira)
} Tube;

static Tube   tubes[TT_MAX];
static u16    spawnWait;

static s16 iabs(s16 v) { return (s16)(v < 0 ? -v : v); }

static void sprayFree(Tube* b) {
    if (b->sp) { SPR_releaseSprite(b->sp); b->sp = NULL; }
}

static void tubeFree(Tube* b) {
    sprayFree(b);
    if (b->s) { SPR_releaseSprite(b->s); b->s = NULL; }
    b->st = TS_OFF;
}

void ttInit(void) {
    for (u8 i = 0; i < TT_MAX; i++) { tubes[i].s = NULL; tubes[i].sp = NULL; tubes[i].st = TS_OFF; }
    spawnWait = 90;     // el primero, un rato despues de entrar a la zona
}

bool ttBusy(void) {
    for (u8 i = 0; i < TT_MAX; i++) if (tubes[i].st != TS_OFF) return TRUE;
    return FALSE;
}

void ttRelease(void) {
    for (u8 i = 0; i < TT_MAX; i++) tubeFree(&tubes[i]);
}

// Cuadro del sprite del tubo segun el estado
static s16 riseFrame(u16 t) {
    // 0 y 1 (el disco) un poco mas lentos; despues 5 cuadros por cuadro
    if (t < 5)  return 0;
    if (t < 10) return 1;
    u16 f = (u16)(2 + (t - 10) / 4);
    return (s16)(f > TF_RISE_LAST ? TF_RISE_LAST : f);
}
#define TT_RISE_T   (10 + 4 * 4 + 2)   // 28: el cuadro 5 ya entero (26/10: antes 40)

static void trySpawn(Player** pls, u8 nPl, s16 camX, s16 laneT, s16 laneB) {
    Tube* b = NULL;
    u8 alive = 0;
    for (u8 i = 0; i < TT_MAX; i++) { if (tubes[i].st == TS_OFF) { if (!b) b = &tubes[i]; } else alive++; }
    if (!b || alive >= TT_MAX) return;
    if (!sprVramFits(techno_tube.maxNumTile)) return;
    // (26/10) En cualquier lugar de la parte VISIBLE de la zona (adelante o
    // atras de las tortugas: tapan el paso) y mirando para cualquier lado.
    s16 x0 = (s16)(camX + 28), x1 = (s16)(camX + SCREEN_W - 28);
    if (x0 < TT_X_MIN) x0 = TT_X_MIN;
    if (x1 > TT_X_MAX) x1 = TT_X_MAX;
    if (x1 <= x0) return;
    s16 x = (s16)(x0 + (random() % (x1 - x0 + 1)));
    s16 lane = (s16)(laneT + 14 + (random() % (laneB - laneT - 19)));
    // ni encima de una tortuga ni pegado a otro tubo
    for (u8 k = 0; k < nPl; k++) {
        s16 cx = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
        if (iabs((s16)(cx - x)) < TT_NEAR_PLAYER && iabs((s16)(getPlayerY(pls[k]) - lane)) < 24) return;
    }
    for (u8 i = 0; i < TT_MAX; i++) {
        if (tubes[i].st == TS_OFF) continue;
        if (iabs((s16)(tubes[i].x - x)) < 56 && iabs((s16)(tubes[i].lane - lane)) < 26) return;
    }
    b->s = SPR_addSprite(&techno_tube, -64, -64, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    if (!b->s) return;
    b->dir = (random() & 1) ? 1 : -1;  // 26/10: tambien tiran a la derecha
    SPR_setHFlip(b->s, b->dir > 0);
    SPR_setAutoAnimation(b->s, FALSE);
    SPR_setAnimAndFrame(b->s, 0, 0);
    b->st = TS_RISE; b->t = 0; b->x = x; b->lane = lane;
    b->burst = 0; b->hits = 0; b->hitCd = 0; b->hitMask = 0;
    spawnWait = (u16)(TT_SPAWN_MIN + (random() % TT_SPAWN_RND));
}

static void startSpray(Tube* b) {
    if (!sprVramFits(techno_spray.maxNumTile)) { b->st = TS_PAUSE; b->t = 0; return; }   // sin VRAM: se saltea
    b->sp = SPR_addSprite(&techno_spray, -80, -80, TILE_ATTR(PAL3, FALSE, FALSE, b->dir > 0));
    if (!b->sp) { b->st = TS_PAUSE; b->t = 0; return; }
    SPR_setAutoAnimation(b->sp, FALSE);
    SPR_setHFlip(b->sp, b->dir > 0);
    b->st = TS_SPRAY; b->t = 0; b->hitMask = 0;
    XGM2_playPCMEx(hydrant_sfx, sizeof(hydrant_sfx), SOUND_PCM_CH3, 9, TRUE, FALSE);
}

void ttUpdate(Player** pls, u8 nPl, s16 camX, s16 camY, s16 laneT, s16 laneB,
              s16 leadFeetX, bool spawn) {
    if (spawn && leadFeetX >= TT_ZONE_X0 && leadFeetX <= TT_ZONE_X1) {
        if (spawnWait) spawnWait--;
        else trySpawn(pls, nPl, camX, laneT, laneB);
    }

    for (u8 i = 0; i < TT_MAX; i++) {
        Tube* b = &tubes[i];
        if (b->st == TS_OFF) continue;
        b->t++;
        if (b->hitCd) b->hitCd--;
        s16 frame = TF_RISE_LAST;
        bool showSpray = FALSE;
        s16 sprayFrame = TS_SPRAY_A;

        switch (b->st) {
        case TS_RISE:
            frame = riseFrame(b->t);
            if (b->t >= TT_RISE_T) { b->st = TS_AIM; b->t = 0; }
            break;
        case TS_AIM:
            if (b->t >= TT_AIM_T) startSpray(b);
            break;
        case TS_SPRAY: {
            frame = TF_FIRE;
            showSpray = TRUE;
            if (b->t < TT_FADE_T)                   sprayFrame = (b->t & 2) ? TS_FADE_B : TS_FADE_A;
            else if (b->t >= TT_SPRAY_T - TT_FADE_T) sprayFrame = (b->t & 2) ? TS_FADE_B : TS_FADE_A;
            else                                    sprayFrame = (b->t & 4) ? TS_SPRAY_B : TS_SPRAY_A;
            // Congela a la tortuga que el chorro alcance (una vez por chorro)
            if (b->t >= TT_FADE_T && b->t < TT_SPRAY_T - TT_FADE_T) {
                for (u8 k = 0; k < nPl; k++) {
                    u8 bit = (u8)(1 << k);
                    if (b->hitMask & bit) continue;
                    if (!playerCanBeHit(pls[k])) continue;
                    s16 fwd = (s16)((getPlayerHurtCX(pls[k]) - b->x) * b->dir);
                    if (fwd < -4 || fwd > TT_REACH) continue;
                    if (iabs((s16)(getPlayerY(pls[k]) - b->lane)) > TT_DY) continue;
                    b->hitMask |= bit;
                    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, TRUE, FALSE);
                    playerFreezeHit(pls[k], b->x, TT_DMG);
                }
            }
            if (b->t >= TT_SPRAY_T) {
                sprayFree(b);
                b->burst++;
                b->st = (b->burst >= TT_BURSTS) ? TS_SINK : TS_PAUSE;
                b->t = 0;
            }
            break;
        }
        case TS_PAUSE:
            if (b->t >= TT_PAUSE_T) { b->st = TS_AIM; b->t = (u16)(TT_AIM_T - 10); }
            break;
        case TS_SINK: {
            u16 step = (u16)(b->t / TT_SINK_STEP);
            if (step > TF_RISE_LAST) { tubeFree(b); continue; }
            frame = (s16)(TF_RISE_LAST - step);
            break;
        }
        case TS_DEAD:
            frame = TF_DEAD;
            if (b->t >= TT_DEAD_T) { tubeFree(b); continue; }
            break;
        }

        // Golpes de las tortugas (entero y de pie)
        if ((b->st == TS_AIM || b->st == TS_SPRAY || b->st == TS_PAUSE) && !b->hitCd) {
            for (u8 k = 0; k < nPl; k++) {
                if (!playerAttackHitsBand(pls[k], b->x, b->lane, TT_HALF_W,
                                          (s16)-TT_BODY_H, 0)) continue;
                b->hitCd = TT_HIT_CD;
                XGM2_playPCMEx(mouser_hit_sfx, sizeof(mouser_hit_sfx), SOUND_PCM_CH3, 15, TRUE, FALSE);
                if (++b->hits >= TT_HITS) {
                    sprayFree(b);
                    b->st = TS_DEAD; b->t = 0;
                    frame = TF_DEAD;
                    showSpray = FALSE;
                    addPlayerScore(pls[k], 1);
                }
                break;
            }
        }

        // --- Dibujo -----------------------------------------------------------
        s16 sx = (s16)(b->x - TT_SPR_W / 2 - camX);
        s16 sy = (s16)(b->lane - (TT_SPR_H - 1) - camY);
        bool vis = !(b->st == TS_DEAD && b->t >= (TT_DEAD_T - TT_BOOM_BLINK) && (b->t & 2));
        SPR_setVisibility(b->s, vis ? VISIBLE : HIDDEN);
        SPR_setFrame(b->s, frame);
        SPR_setPosition(b->s, sx, sy);
        SPR_setDepth(b->s, (s16)-b->lane);
        if (b->sp) {
            SPR_setVisibility(b->sp, showSpray ? VISIBLE : HIDDEN);
            SPR_setFrame(b->sp, sprayFrame);
            s16 px = (b->dir < 0) ? (s16)(sx - TT_SPRAY_W + 1) : (s16)(sx + TT_SPR_W - 1);
            SPR_setPosition(b->sp, px, (s16)(sy - TT_NOZZLE_DY));
            SPR_setDepth(b->sp, (s16)(-b->lane + 1));
        }
    }
}
