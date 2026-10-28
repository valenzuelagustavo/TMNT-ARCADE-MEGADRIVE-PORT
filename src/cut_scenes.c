// ===========================================================================
// cut_scenes.c - Cinematicas de UNA imagen entre niveles (23/10)
// ===========================================================================
// SCENE_CUT_VAN (despues del nivel de las patinetas): la imagen del choque de
// la camioneta (cut_accident, cutscenes.res). Fundido desde negro, se queda
// CUT_HOLD_SECS segundos (START de cualquier joystick la adelanta), fundido a
// negro y sigue el titulo de la SCENE 5.
//
// SCENE_CUT_SPLINTER (27/10, despues de la fabrica): Splinter agradece a las
// tortugas ("Thank you, my turtles!"). El fondo (cut_splinter_bg) lleva el
// globo y el cuerpo de Splinter; solo la parte que cambia entre los 13 cuadros
// (cabeza y boca, 64x40) es un sprite (cut_splinter_mouth), que recorre sus
// cuadros al ritmo de la voz. START la saltea.
// ===========================================================================
#include <genesis.h>
#include "scenes.h"
#include "cutscenes.h"   // cut_accident, cut_splinter_* (rescomp)
#include "audio.h"       // thank_you_vo

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

// ---------------------------------------------------------------------------
// SCENE_CUT_SPLINTER
// ---------------------------------------------------------------------------
#define SPL_MOUTH_X      168
#define SPL_MOUTH_Y      104
#define SPL_FRAMES        13
#define SPL_PRE_TICKS     24     // quieto un momento antes de hablar
#define SPL_TALK_NTSC     74     // la voz dura 1,24 s: 74 cuadros a 60 Hz ...
#define SPL_TALK_PAL      62     // ... y 62 a 50 Hz
#define SPL_POST_SECS      2     // se queda con el ultimo cuadro
#define SPL_VO_PRIO       15

static bool splStart(u16* prev) {
    u16 v = JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2);
    bool go = (v & ~(*prev) & BUTTON_START) != 0;
    *prev = v;
    return go;
}

SceneId showCutSplinter(void) {
    clearScene();
    while ((JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2)) & BUTTON_START)
        SYS_doVBlankProcess();

    VDP_setScreenWidth320();
    VDP_setBackgroundColor(0);
    SPR_end();
    SPR_initEx(64);                   // un solo sprite de 40 tiles

    VDP_drawImageEx(BG_B, &cut_splinter_bg,
                    TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX),
                    0, 0, FALSE, TRUE);
    Sprite* mouth = SPR_addSprite(&cut_splinter_mouth, SPL_MOUTH_X, SPL_MOUTH_Y,
                                  TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
    if (mouth) {
        SPR_setAutoAnimation(mouth, FALSE);
        SPR_setFrame(mouth, 0);
    }
    SPR_update();
    SYS_doVBlankProcess();
    PAL_fadeIn(0, 15, cut_splinter_bg.palette->data, CUT_FADE_TICKS, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    const u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    const u16 talk = IS_PAL_SYSTEM ? SPL_TALK_PAL : SPL_TALK_NTSC;
    u16 prev = 0xFFFF;
    bool skip = FALSE;

    for (u16 t = 0; t < SPL_PRE_TICKS && !skip; t++) {
        skip = splStart(&prev) && prev != 0xFFFF;
        SPR_update();
        SYS_doVBlankProcess();
    }
    if (!skip)
        XGM2_playPCMEx(thank_you_vo, sizeof(thank_you_vo), SOUND_PCM_CH2, SPL_VO_PRIO, FALSE, FALSE);
    for (u16 t = 0; t < talk && !skip; t++) {
        if (mouth) SPR_setFrame(mouth, (s16)(((u32)t * SPL_FRAMES) / talk));
        skip = splStart(&prev);
        SPR_update();
        SYS_doVBlankProcess();
    }
    if (mouth) SPR_setFrame(mouth, 0);       // boca cerrada
    for (u16 t = 0; t < fps * SPL_POST_SECS && !skip; t++) {
        skip = splStart(&prev);
        SPR_update();
        SYS_doVBlankProcess();
    }
    if (skip) XGM2_stopPCM(SOUND_PCM_CH2);

    clearScene();                     // fundido a negro
    SPR_end();
    SPR_initEx(752);                  // lo que esperan las demas escenas
    return SCENE_DOJO;
}
