// =============================================================================
// konami_logo.c -- el logo animado de KONAMI (14/10)
// =============================================================================
// Rehecho cuadro a cuadro de la hoja de capturas de Gustavo
// (res/images/konami_logo/konami_logo_frames.png, ver tools/gen_konami_logo.py):
//
//   1. Una estrella cruza la pantalla de izquierda a derecha (8 px/f) dejando
//      una linea fina rosa.
//   2. La linea se hace gruesa y BAJA (2 px/f). Lo que queda por encima de
//      ella muestra el logo dibujado en azul; lo de abajo todavia es negro.
//   3. Con la linea fuera de pantalla, el logo se PINTA: es la misma imagen con
//      otra paleta. El degrade de cada pieza gira (ciclo de paleta) hasta
//      quedar quieto y despues KONAMI pasa a plateado con su propio giro.
//   4. El fondo pasa de negro a gris claro, queda un rato y todo se funde a
//      negro.
// Con su jingle ("17 - Konami Logo.vgm", ver KL_VOL_MUSIC).
//
// Pantalla H32 (256 px, el ancho de las capturas). Planos:
//   BG_A  la pieza de arriba (PAL1)
//   BG_B  la pieza de abajo (PAL2) y KONAMI (PAL0)
//   sprites: la linea y la estrella (PAL3); el indice 15 de PAL3 es el fondo
//   (color de backdrop del VDP).
//
// EL CORTE DE LA LINEA (paso 2) va con una interrupcion horizontal: en la
// linea de pantalla donde esta el borde de arriba del laser, el scroll
// vertical de los dos planos salta a 256 (la mitad de abajo de un plano de
// 32x64 tiles, vacia), asi que de ahi para abajo no se ve nada. En el
// vblank se vuelve a 0. Es al pixel, como en el original. Mientras corre la
// interrupcion el bucle principal NO toca el VDP directamente: todo lo suyo
// (sprites, paletas) va por la cola de DMA, que se vacia en el vblank.
// =============================================================================
#include <genesis.h>
#include "scenes.h"
#include "intro.h"
#include "konami_logo_pal.h"
#include "audio.h"   // music_konami

#define KL_HIDE_V          256   // scroll vertical que muestra la parte vacia
#define KL_SCREEN_H        224

// Tiempos en frames (60 Hz). Las capturas van de a 5 frames: la estrella
// avanza 40 px por captura y la linea 10.
#define KL_T_BLACK          20   // negro antes de la estrella
#define KL_STAR_VX           8
#define KL_STAR_Y           23   // centro de la linea fina
#define KL_BAND_Y0          18   // primera fila de la linea gruesa al aparecer
#define KL_T_BAND_HOLD       4   // la linea gruesa quieta antes de bajar
#define KL_BAND_VY           2
#define KL_T_OUTLINE_HOLD   12   // logo azul entero, sin linea
#define KL_TOP_SHIFT0       19   // el degrade gira dos vueltas antes de quedar
#define KL_BOT_SHIFT0       20   //   (la de abajo va un paso atrasada)
#define KL_TXT_SHIFT0        5   // KONAMI plateado arranca corrido 5 ...
#define KL_TXT_STEPS         9   //   ... y gira 9 pasos (queda en 7, como en
                                 //   las capturas 32..38)
#define KL_T_COLOR_HOLD     20
#define KL_T_BG_STEP         5   // el fondo sube un nivel de gris cada 5 frames
#define KL_BG_MAX            6   // hasta 0xCCC: el gris de las capturas (224)
#define KL_T_HOLD          120   // logo terminado
#define KL_T_FADE           24

// Musica (14/10): "17 - Konami Logo.vgm", 2,5 s sin loop. Arranca cuando
// aparece la linea gruesa: el arpegio acompana la bajada y el acorde final
// (a los ~1,6 s) cae cuando la linea descubre KONAMI; la cola suena mientras
// el logo se pinta.
#define KL_VOL_MUSIC        90

#define KL_BAND_SPRITES      8   // 8 x 32 px = toda la pantalla
#define KL_TRAIL_SPRITES     8

static volatile u16 klHideLine;  // de esta linea para abajo no se ven los planos

static void klWriteVScroll(u16 v) {
    vu32* ctrl = (vu32*) VDP_CTRL_PORT;
    vu32* data = (vu32*) VDP_DATA_PORT;
    *ctrl = VDP_WRITE_VSRAM_ADDR(0);
    *data = ((u32)v << 16) | v;          // BG_A y BG_B
}

HINTERRUPT_CALLBACK klHInt(void) {
    klWriteVScroll(KL_HIDE_V);
}

// Arranque de cada frame (interrupcion vertical, ya en el vblank): planos
// visibles otra vez y la interrupcion horizontal en la linea del laser.
static void klVInt(void) {
    u16 line = klHideLine;
    klWriteVScroll((line == 0) ? KL_HIDE_V : 0);
    if (line > 0 && line < KL_SCREEN_H) {
        VDP_setHIntCounter((u8)(line - 1));
        VDP_setHInterrupt(TRUE);
    } else {
        VDP_setHInterrupt(FALSE);
    }
}

// Paleta de una pieza con el degrade corrido 'shift' pasos.
static void klCyclePal(u16* out, const u16* still, const u16* cyc, const u8* base,
                       u16 nMove, u16 nCyc, u16 shift) {
    memcpy(out, still, 16 * sizeof(u16));
    for (u16 i = 0; i < nMove; i++)
        out[1 + i] = cyc[(base[i] + shift) % nCyc];
}

static bool klSkip(void) {
    return (JOY_readJoypad(JOY_1) & BUTTON_START) != 0;
}

SceneId showKonamiIntro(void) {
    // --- Preparacion (todo en negro) ---
    PAL_setColors(0, palette_black, 64, CPU);
    VDP_setScreenWidth256();
    VDP_setPlaneSize(32, 64, TRUE);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_A, KL_HIDE_V);
    VDP_setVerticalScroll(BG_B, KL_HIDE_V);
    VDP_setBackgroundColor(63);          // PAL3, indice 15

    u16 vram = TILE_USER_INDEX;
    VDP_drawImageEx(BG_A, &konami_logo_a, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, vram),
                    0, 0, FALSE, CPU);
    vram += konami_logo_a.tileset->numTile;
    VDP_drawImageEx(BG_B, &konami_logo_b, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, vram),
                    0, 0, FALSE, CPU);

    Sprite* star = SPR_addSprite(&konami_star, -32, KL_STAR_Y - 8,
                                 TILE_ATTR(PAL3, TRUE, FALSE, FALSE));
    Sprite* trail[KL_TRAIL_SPRITES];
    Sprite* band[KL_BAND_SPRITES];
    for (u16 i = 0; i < KL_TRAIL_SPRITES; i++) {
        trail[i] = SPR_addSprite(&konami_trail, -32, KL_STAR_Y - 4,
                                 TILE_ATTR(PAL3, TRUE, FALSE, FALSE));
        if (trail[i]) SPR_setVisibility(trail[i], HIDDEN);
    }
    for (u16 i = 0; i < KL_BAND_SPRITES; i++) {
        band[i] = SPR_addSprite(&konami_band, (s16)(i * 32), KL_BAND_Y0,
                                TILE_ATTR(PAL3, TRUE, FALSE, FALSE));
        if (band[i]) SPR_setVisibility(band[i], HIDDEN);
    }
    if (star) {
        SPR_setVisibility(star, HIDDEN);
        SPR_setDepth(star, SPR_MIN_DEPTH);   // primera en la linea: nunca se cae
    }
    SPR_update();

    // Paletas: el logo en azul, la linea y el fondo negro.
    u16 pal[16];
    PAL_setColors(0,  klPalTxtBlue, 16, CPU);
    PAL_setColors(16, klPalTopBlue, 16, CPU);
    PAL_setColors(32, klPalBotBlue, 16, CPU);
    memcpy(pal, klPalFx, sizeof(pal));
    pal[15] = 0x000;
    PAL_setColors(48, pal, 16, CPU);

    klHideLine = 0;                       // todo tapado
    SYS_setHIntCallback(klHInt);
    SYS_setVIntCallback(klVInt);
    SYS_doVBlankProcess();

    bool skip = FALSE;

    // --- 0. Negro ---
    for (u16 t = 0; t < KL_T_BLACK && !skip; t++) {
        SYS_doVBlankProcess();
        skip = klSkip();
    }

    // --- 1. La estrella cruza con la linea fina ---
    if (star) SPR_setVisibility(star, VISIBLE);
    for (u16 t = 0; !skip; t++) {
        s16 x = (s16)(t * KL_STAR_VX);
        if (x >= 256) break;
        if (star) {
            SPR_setFrame(star, (t >> 1) & 3);
            SPR_setPosition(star, x - 8, KL_STAR_Y - 8);
        }
        // la linea termina debajo de la estrella
        for (u16 i = 0; i < KL_TRAIL_SPRITES; i++) {
            if (!trail[i]) continue;
            s16 tx = (s16)(x - 32 * (i + 1));
            if (tx + 32 <= 0) { SPR_setVisibility(trail[i], HIDDEN); continue; }
            SPR_setVisibility(trail[i], VISIBLE);
            SPR_setPosition(trail[i], tx, KL_STAR_Y - 4);
        }
        SPR_update();
        SYS_doVBlankProcess();
        skip = klSkip();
    }
    if (star) SPR_setVisibility(star, HIDDEN);
    for (u16 i = 0; i < KL_TRAIL_SPRITES; i++)
        if (trail[i]) SPR_setVisibility(trail[i], HIDDEN);

    // --- 2. La linea gruesa baja y descubre el logo azul ---
    if (!skip) {
        XGM2_setLoopNumber(0);            // SIEMPRE antes del play: suena una vez
        playMusicVol(music_konami, KL_VOL_MUSIC);
    }
    s16 by = KL_BAND_Y0;
    for (u16 i = 0; i < KL_BAND_SPRITES; i++)
        if (band[i]) SPR_setVisibility(band[i], VISIBLE);
    for (u16 t = 0; !skip; t++) {
        if (t >= KL_T_BAND_HOLD) by += KL_BAND_VY;
        if (by >= KL_SCREEN_H) break;
        for (u16 i = 0; i < KL_BAND_SPRITES; i++)
            if (band[i]) SPR_setPosition(band[i], (s16)(i * 32), by);
        klHideLine = (u16)by;
        SPR_update();
        SYS_doVBlankProcess();
        skip = klSkip();
    }
    for (u16 i = 0; i < KL_BAND_SPRITES; i++)
        if (band[i]) SPR_setVisibility(band[i], HIDDEN);
    klHideLine = KL_SCREEN_H;             // logo entero
    SPR_update();
    for (u16 t = 0; t < KL_T_OUTLINE_HOLD && !skip; t++) {
        SYS_doVBlankProcess();
        skip = klSkip();
    }

    // --- 3. Se pinta: los degrades giran hasta quedar quietos ---
    s16 topShift = KL_TOP_SHIFT0, botShift = KL_BOT_SHIFT0;
    s16 txtLeft = -1;                     // -1 = KONAMI todavia azul
    u16 txtShift = KL_TXT_SHIFT0;
    while (!skip) {
        klCyclePal(pal, klPalTopColor, klCycTop, klBaseTop, KL_TOP_MOVE, KL_TOP_CYCLE, (u16)topShift);
        PAL_setColors(16, pal, 16, DMA);
        klCyclePal(pal, klPalBotColor, klCycBot, klBaseBot, KL_BOT_MOVE, KL_BOT_CYCLE, (u16)botShift);
        PAL_setColors(32, pal, 16, DMA);
        if (txtLeft >= 0) {
            klCyclePal(pal, klPalTxtColor, klCycTxt, klBaseTxt, KL_TXT_MOVE, KL_TXT_CYCLE, txtShift);
            PAL_setColors(0, pal, 16, DMA);
        }
        SYS_doVBlankProcess();
        skip = klSkip();

        if (topShift > 0) topShift--;
        if (botShift > 0) botShift--;
        if (txtLeft < 0) {
            if (topShift == 0) txtLeft = KL_TXT_STEPS;     // plateado
        } else if (txtLeft > 0) {
            txtLeft--;
            txtShift = (u16)((txtShift + KL_TXT_CYCLE - 1) % KL_TXT_CYCLE);
        } else {
            break;
        }
    }
    for (u16 t = 0; t < KL_T_COLOR_HOLD && !skip; t++) {
        SYS_doVBlankProcess();
        skip = klSkip();
    }

    // --- 4. El fondo de negro a gris claro ---
    for (u16 lv = 1; lv <= KL_BG_MAX && !skip; lv++) {
        PAL_setColor(63, (u16)((lv << 9) | (lv << 5) | (lv << 1)));
        for (u16 t = 0; t < KL_T_BG_STEP && !skip; t++) {
            SYS_doVBlankProcess();
            skip = klSkip();
        }
    }
    for (u16 t = 0; t < KL_T_HOLD && !skip; t++) {
        SYS_doVBlankProcess();
        skip = klSkip();
    }

    // --- Fundido a negro y limpieza ---
    klHideLine = KL_SCREEN_H;
    PAL_fadeOutAll(skip ? 10 : KL_T_FADE, FALSE);
    while (JOY_readJoypad(JOY_1) & BUTTON_START) SYS_doVBlankProcess();

    XGM2_setLoopNumber(-1);               // las demas musicas repiten
    SYS_setVIntCallback(NULL);
    VDP_setHInterrupt(FALSE);
    SYS_setHIntCallback(NULL);

    if (star) SPR_releaseSprite(star);
    for (u16 i = 0; i < KL_TRAIL_SPRITES; i++) if (trail[i]) SPR_releaseSprite(trail[i]);
    for (u16 i = 0; i < KL_BAND_SPRITES; i++)  if (band[i])  SPR_releaseSprite(band[i]);
    SPR_update();
    SYS_doVBlankProcess();

    // Volver a lo que esperan las escenas siguientes: plano 64x32, 320 px.
    VDP_setPlaneSize(64, 32, TRUE);
    VDP_setScreenWidth320();
    clearScene();
    return SCENE_SGDK;
}
