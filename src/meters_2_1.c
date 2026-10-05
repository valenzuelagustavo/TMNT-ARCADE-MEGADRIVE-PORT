#include "meters_2_1.h"

// ===========================================================================
// PARQUIMETROS del 2-1 — ver meters_2_1.h
// ===========================================================================

#define METER_SCREEN_W  320

// Posiciones MEDIDAS sobre el mapa marcado: la captura coincide
// 1:1 con "Stage 2-_16_colors_v2.png" (offset 0,0), asi que las coordenadas
// son las de mundo. cx = centro del palo, y = fila de los pies (la ultima con
// pixeles del parquimetro pegado en la captura). La vereda ahi va de y=160 a
// 250, asi que 223 queda adentro y se lo puede rodear por los dos lados.
static const s16 meterX[METERS_COUNT] = {  145,  396,  654,  910, 1166 };
static const s16 meterY[METERS_COUNT] = {  223,  223,  223,  223,  223 };

typedef enum {
    METER_STANDING = 0,
    METER_HIT,          // mostrando "recibe el golpe", todavia quieto
    METER_FLYING,
    METER_GONE
} MeterState;

static struct {
    u8      state;
    Sprite* sprite;     // stand o fly segun el estado
    s16     x;          // centro del palo (mundo)
    s8      dir;        // direccion del vuelo
    u8      timer;
    s8      owner;      // jugador que lo arranco (puntaje)
    u8      hitMask;    // enemigos ya golpeados (bit por indice)
} meters[METERS_COUNT];

static void meterRelease(u16 i) {
    if (meters[i].sprite) SPR_releaseSprite(meters[i].sprite);
    meters[i].sprite = NULL;
}

void metersInit(void) {
    for (u16 i = 0; i < METERS_COUNT; i++) {
        meters[i].state   = METER_STANDING;
        meters[i].sprite  = NULL;
        meters[i].x       = meterX[i];
        meters[i].dir     = 1;
        meters[i].timer   = 0;
        meters[i].owner   = -1;
        meters[i].hitMask = 0;
    }
}

void metersReleaseAll(void) {
    for (u16 i = 0; i < METERS_COUNT; i++) meterRelease(i);
}

bool metersBlock(s16 fx, s16 fy) {
    for (u16 i = 0; i < METERS_COUNT; i++) {
        if (meters[i].state != METER_STANDING) continue;
        if (abs(fx - meters[i].x) > METER_BLOCK_HALF_W) continue;
        if (abs(fy - meterY[i]) > METER_BLOCK_HALF_D) continue;
        return TRUE;
    }
    return FALSE;
}

// Celda grande espejada? El arte mira como un golpe hacia la izquierda: se
// espeja cuando sale volando a la derecha (dir > 0).
#define METER_FLIPPED(dir)  ((bool)((dir) > 0))

bool metersPlayerHits(Player** pls, u8 nPl) {
    bool any = FALSE;
    for (u16 i = 0; i < METERS_COUNT; i++) {
        if (meters[i].state != METER_STANDING) continue;
        for (u8 k = 0; k < nPl; k++) {
            if (!playerAttackHitsBox(pls[k], meters[i].x, meterY[i],
                                     METER_HIT_HALF_W, METER_BODY_H))
                continue;
            // (25/09) Sale volando ALEJANDOSE de la tortuga que lo golpeo:
            // se decide por el LADO en que esta la tortuga, no por hacia donde
            // mira. Con el facing, un golpe que conectaba de espaldas (el giro
            // del especial, o los 12 px de tolerancia hacia atras) lo mandaba
            // hacia la tortuga y con el flip al reves. (27/09) El flip
            // estaba invertido; el arte del sheet es el de un golpe HACIA LA
            // IZQUIERDA, asi que se espeja cuando sale volando a la DERECHA
            // (ver METER_FLIPPED).
            {
                s16 pcx = (s16)(pls[k]->x + PLAYER_SPRITE_W / 2);
                if (meters[i].x > pcx)      meters[i].dir = 1;
                else if (meters[i].x < pcx) meters[i].dir = -1;
                else                        meters[i].dir = (pls[k]->dir >= 0) ? 1 : -1;
            }
            meters[i].owner   = (s8)k;
            meters[i].state   = METER_HIT;
            meters[i].timer   = METER_HIT_TICKS;
            meters[i].hitMask = 0;
            // Cambia al sprite grande (el parado se suelta en el acto).
            meterRelease(i);
            meters[i].sprite = SPR_addSprite(&parking_meter_fly, 0, 0,
                                             TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
            if (meters[i].sprite) {
                SPR_setAutoAnimation(meters[i].sprite, FALSE);
                SPR_setAnimAndFrame(meters[i].sprite, 0, 0);
                SPR_setHFlip(meters[i].sprite, METER_FLIPPED(meters[i].dir));
            }
            any = TRUE;
            break;
        }
    }
    return any;
}

bool metersHitEnemy(u16 enemyIdx, s16 ex, s16 ey, s16 halfW, s8* owner) {
    u8 bit = (u8)(1 << (enemyIdx & 7));
    for (u16 i = 0; i < METERS_COUNT; i++) {
        if (meters[i].state != METER_FLYING) continue;
        if (meters[i].hitMask & bit) continue;
        if (abs(ex - meters[i].x) > (s16)(METER_FLY_HALF_W + halfW)) continue;
        if (abs(ey - meterY[i]) > METER_FLY_TOL_Y) continue;
        meters[i].hitMask |= bit;
        if (owner) *owner = meters[i].owner;
        return TRUE;
    }
    return FALSE;
}

void metersUpdate(s16 camX, s16 camY) {
    for (u16 i = 0; i < METERS_COUNT; i++) {
        s16 sx = (s16)(meters[i].x - camX);

        switch (meters[i].state) {
        case METER_STANDING: {
            // Sprite solo cerca de camara, igual que las tapas cerradas.
            bool near = (sx > -METER_ARM_MARGIN) &&
                        (sx < (s16)(METER_SCREEN_W + METER_ARM_MARGIN));
            if (near && !meters[i].sprite) {
                meters[i].sprite = SPR_addSprite(&parking_meter_stand, 0, 0,
                                                 TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
            } else if (!near && meters[i].sprite) {
                meterRelease(i);
            }
            if (meters[i].sprite) {
                SPR_setPosition(meters[i].sprite, (s16)(sx - METER_STAND_PX),
                                (s16)(meterY[i] - METER_FOOT_OFFSET - camY));
                SPR_setDepth(meters[i].sprite, (s16)(-meterY[i]));
            }
            continue;
        }

        case METER_HIT:
            if (meters[i].timer > 0) meters[i].timer--;
            if (meters[i].timer == 0) {
                meters[i].state = METER_FLYING;
                if (meters[i].sprite) SPR_setAnimAndFrame(meters[i].sprite, 0, 1);
            }
            break;

        case METER_FLYING:
            meters[i].x = (s16)(meters[i].x + meters[i].dir * METER_SPEED);
            sx = (s16)(meters[i].x - camX);
            if (sx < -(METER_FLY_W + METER_MARGIN) ||
                sx > (s16)(METER_SCREEN_W + METER_FLY_W + METER_MARGIN)) {
                meterRelease(i);
                meters[i].state = METER_GONE;
                continue;
            }
            break;

        default:
            continue;
        }

        // Golpeado o volando: celda grande. Con HFlip el palo queda en
        // METER_FLY_W - METER_FLY_PX desde el borde izquierdo.
        if (meters[i].sprite) {
            s16 px = METER_FLIPPED(meters[i].dir) ? (s16)(METER_FLY_W - METER_FLY_PX)
                                                  : METER_FLY_PX;
            SPR_setPosition(meters[i].sprite, (s16)(sx - px),
                            (s16)(meterY[i] - METER_FOOT_OFFSET - camY));
            SPR_setDepth(meters[i].sprite, (s16)(-meterY[i]));
        }
    }
}
