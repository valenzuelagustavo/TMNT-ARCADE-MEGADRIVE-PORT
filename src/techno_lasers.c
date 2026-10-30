// ===========================================================================
// techno_lasers.c - las tres barras LASER de la puerta del Technodrome (30/10)
// Ver techno_lasers.h.
// ===========================================================================
#include <genesis.h>
#include "techno_lasers.h"
#include "level8_1.h"      // techno_laser_bar
#include "audio.h"         // electric_shock_sfx, shock_pain_vo

#define TL_CELL_DX   2     // la barra va en las columnas 2..5 de la celda de 8 px

static Sprite* bar[TL_N];
static bool    active = FALSE;
static u16     tick;

bool tlActive(void) { return active; }

void tlStart(void) {
    if (active) return;
    tick = 0;
    for (u8 i = 0; i < TL_N; i++) {
        bar[i] = SPR_addSpriteSafe(&techno_laser_bar, 0, -256,
                                   TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
        if (!bar[i]) continue;
        SPR_setAutoAnimation(bar[i], FALSE);
        SPR_setVisibility(bar[i], HIDDEN);
        // Detras de la tortuga que esta en la misma lane (menor valor = delante).
        SPR_setDepth(bar[i], (s16)(-(TL_TOP0 + TL_STEP * i + TL_BODY) + 1));
    }
    active = TRUE;
}

void tlEnd(void) {
    for (u8 i = 0; i < TL_N; i++) {
        if (bar[i]) { SPR_releaseSprite(bar[i]); bar[i] = NULL; }
    }
    active = FALSE;
}

void tlUpdate(Player** pls, u8 nPl, s16 camX, s16 camY) {
    if (!active) return;
    tick++;
    bool on = (tick % (TL_ON_FRAMES + TL_OFF_FRAMES)) < TL_ON_FRAMES;

    for (u8 i = 0; i < TL_N; i++) {
        if (!bar[i]) continue;
        s16 left = (s16)(TL_X0 + TL_STEP * i);
        s16 top  = (s16)(TL_TOP0 + TL_STEP * i);
        SPR_setVisibility(bar[i], on ? VISIBLE : HIDDEN);
        SPR_setFrame(bar[i], (s16)((tick >> 1) & 1));       // el destello
        SPR_setPosition(bar[i], (s16)(left - TL_CELL_DX - camX), (s16)(top - camY));
    }
    if (!on) return;

    // Descarga: la tortuga en la lane de la barra y tocandola.
    for (u8 k = 0; k < nPl; k++) {
        Player* p = pls[k];
        if (!playerCanBeHitAir(p)) continue;
        s16 cx = getPlayerHurtCX(p);
        s16 y  = getPlayerY(p);
        for (u8 i = 0; i < TL_N; i++) {
            if (!bar[i]) continue;
            s16 barCX = (s16)(TL_X0 + TL_STEP * i + 2);
            s16 lane  = (s16)(TL_TOP0 + TL_STEP * i + TL_BODY);
            s16 dx = (s16)(cx - barCX);
            s16 dy = (s16)(y - lane);
            if (dx < 0) dx = (s16)-dx;
            if (dy < 0) dy = (s16)-dy;
            if (dx > TL_HIT_DX || dy > TL_HIT_DY) continue;
            playerElectroHit(p, barCX, TL_DMG);
            XGM2_playPCMEx(electric_shock_sfx, sizeof(electric_shock_sfx),
                           SOUND_PCM_CH3, 15, TRUE, FALSE);
            XGM2_playPCMEx(shock_pain_vo, sizeof(shock_pain_vo),
                           SOUND_PCM_CH2, 12, TRUE, FALSE);
            break;
        }
    }
}
