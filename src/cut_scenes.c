// ===========================================================================
// cut_scenes.c - Cinematicas de UNA imagen entre niveles (23/10)
// ===========================================================================
// SCENE_CUT_VAN (despues del nivel de las patinetas): la imagen del choque de
// la camioneta (cut_accident, cutscenes.res). Fundido desde negro, se queda
// CUT_HOLD_SECS segundos (START de cualquier joystick la adelanta), fundido a
// negro y sigue el titulo de la SCENE 5.
// ===========================================================================
#include <genesis.h>
#include "scenes.h"
#include "cutscenes.h"   // cut_accident (rescomp)

#define CUT_HOLD_SECS   4
#define CUT_FADE_TICKS 20

static SceneId cutImage(const Image* img, SceneId next) {
    clearScene();                     // paletas en negro, planos y sprites limpios
    while ((JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2)) & BUTTON_START)
        SYS_doVBlankProcess();

    VDP_setScreenWidth320();
    VDP_setBackgroundColor(0);
    VDP_drawImageEx(BG_B, img,
                    TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX),
                    0, 0, FALSE, TRUE);
    SYS_doVBlankProcess();
    PAL_fadeIn(0, 15, img->palette->data, CUT_FADE_TICKS, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    u16 timer = (u16)((IS_PAL_SYSTEM ? 50 : 60) * CUT_HOLD_SECS);
    u16 prev = 0xFFFF;
    while (timer > 0) {
        timer--;
        u16 v = JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2);
        if ((v & ~prev & BUTTON_START) && prev != 0xFFFF) break;
        prev = v;
        SYS_doVBlankProcess();
    }

    clearScene();                     // fundido a negro
    return next;
}

SceneId showCutVan(void) {
    return cutImage(&cut_accident, SCENE_S5_TITLE);
}
