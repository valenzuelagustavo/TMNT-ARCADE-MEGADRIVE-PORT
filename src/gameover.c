// gameover.c (02/11) -- CONTINUE grande y cartel de GAME OVER.
// Arte: tools/gen_gameover.py. Recursos: res/gameover_res.res.
#include <genesis.h>
#include "gameover.h"
#include "gameover_res.h"
#include "gameover_pal.h"      // goPalFrames: las 5 paletas del cartel
#include "audio.h"             // music_gameover

// ===========================================================================
// CONTINUE grande
// ===========================================================================
// Rotulo y digitos van en PAL1 (paleta de las tortugas: rojo = indice 14).
// Bloque centrado: rotulo (8 px) + 12 de aire + digitos (56 px) = 76 px.
#define CB_LABEL_W    64
#define CB_DIGIT_W    40
#define CB_BLOCK_H    76
#define CB_DIGIT_DY   20             // del rotulo al digito

static Sprite* cbLabel;
static Sprite* cbDig[2];             // [0] decenas (solo si >= 10), [1] unidades

static Sprite* cbAdd(const SpriteDefinition* def, s16 x, s16 y) {
    Sprite* s = SPR_addSprite(def, x, y, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    return s;
}

bool contBigShow(s8 seconds) {
    if (seconds < 0) { contBigHide(); return FALSE; }
    const bool two = (seconds >= 10);
    const s16 top  = (s16)((VDP_getScreenHeight() - CB_BLOCK_H) / 2);
    const s16 xOne = (s16)((320 - CB_DIGIT_W) / 2);
    const s16 x1   = two ? (s16)(xOne + (CB_DIGIT_W + 8) / 2) : xOne;   // unidades
    const s16 x0   = (s16)(xOne - (CB_DIGIT_W + 8) / 2);                // decenas

    if (!cbLabel) cbLabel = cbAdd(&continue_label, (s16)((320 - CB_LABEL_W) / 2), top);
    if (!cbDig[1]) cbDig[1] = cbAdd(&continue_digits, x1, (s16)(top + CB_DIGIT_DY));
    if (!cbLabel || !cbDig[1]) { contBigHide(); return FALSE; }

    SPR_setPosition(cbDig[1], x1, (s16)(top + CB_DIGIT_DY));
    SPR_setFrame(cbDig[1], (s16)(seconds % 10));
    if (two) {
        if (!cbDig[0]) cbDig[0] = cbAdd(&continue_digits, x0, (s16)(top + CB_DIGIT_DY));
        if (!cbDig[0]) { contBigHide(); return FALSE; }
        SPR_setFrame(cbDig[0], (s16)(seconds / 10));
    } else if (cbDig[0]) {
        SPR_releaseSprite(cbDig[0]);
        cbDig[0] = NULL;
    }
    return TRUE;
}

void contBigHide(void) {
    if (cbLabel)  { SPR_releaseSprite(cbLabel);  cbLabel  = NULL; }
    if (cbDig[0]) { SPR_releaseSprite(cbDig[0]); cbDig[0] = NULL; }
    if (cbDig[1]) { SPR_releaseSprite(cbDig[1]); cbDig[1] = NULL; }
}

void contBigForget(void) {
    cbLabel = NULL;
    cbDig[0] = cbDig[1] = NULL;
}

// ===========================================================================
// Cartel GAME OVER
// ===========================================================================
#define GO_PAL_LINE      PAL3        // linea del cartel (colores 1..15)
#define GO_PAL_PERIOD    5           // cuadros de video entre una paleta y la siguiente
#define GO_MUSIC_VOL     90
#define GO_SIGN_W        112
#define GO_SIGN_H        32
// Fundido del nivel a negro (en decimas de segundo) y salida del cartel.
#define GO_FADE_IN_T10   8
#define GO_FADE_OUT_T10  4
// El cartel se queda hasta que termina el jingle (3,5 s) y como minimo esto;
// tope por si el driver nunca avisa que termino.
#define GO_HOLD_MIN_SECS 4
#define GO_HOLD_MAX_SECS 6
// START no lo saltea antes de esto: el jugador venia apretandolo en el CONTINUE.
#define GO_SKIP_AFTER_T10 12

static bool goShown;

bool gameOverTakeShown(void) {
    bool s = goShown;
    goShown = FALSE;
    return s;
}

// Color Mega Drive (0x0BGR, 3 bits) escalado k/64.
static u16 goScale(u16 c, u16 k) {
    u16 r = (u16)((((c >> 1) & 7) * k) >> 6);
    u16 g = (u16)((((c >> 5) & 7) * k) >> 6);
    u16 b = (u16)((((c >> 9) & 7) * k) >> 6);
    return (u16)((b << 9) | (g << 5) | (r << 1));
}

static Sprite* goSignCreate(void) {
    Sprite* s = SPR_addSprite(&gameover_sign, -GO_SIGN_W, -GO_SIGN_H,
                              TILE_ATTR(GO_PAL_LINE, TRUE, FALSE, FALSE));
    if (s) SPR_setVisibility(s, HIDDEN);
    return s;
}

// Si no hubo VRAM de sprites: el "GAME OVER" en letras de siempre.
static void goFallbackText(void) {
    VDP_loadFont(&font_default, DMA);
    VDP_setTextPlane(BG_A);
    VDP_setTextPriority(1);
    VDP_setTextPalette(PAL0);
    VDP_setBackgroundColor(0);
    PAL_setColor(15, 0x0EEE);
    VDP_drawText("GAME OVER", 15, 13);
}

// Con la pantalla en negro: muestra el cartel, rota la paleta mientras suena el
// jingle y lo apaga. 'sign' puede ser NULL (texto).
static void goRunSign(Sprite* sign, u16 fps) {
    u16 pf = 0;
    if (sign) {
        SPR_setPosition(sign, (s16)((320 - GO_SIGN_W) / 2),
                        (s16)((VDP_getScreenHeight() - GO_SIGN_H) / 2));
        PAL_setColors(GO_PAL_LINE * 16 + 1, goPalFrames[0], 15, CPU);
        SPR_setVisibility(sign, VISIBLE);
    } else {
        goFallbackText();
    }

    const u16 skipAt = (u16)(fps * GO_SKIP_AFTER_T10 / 10);
    const u16 minHold = (u16)(fps * GO_HOLD_MIN_SECS);
    const u16 maxHold = (u16)(fps * GO_HOLD_MAX_SECS);
    u16 prev = JOY_readJoypad(JOY_1);
    u16 f = 0;
    while (f < maxHold) {
        if (sign && f && (f % GO_PAL_PERIOD) == 0) {
            pf = (u16)((pf + 1) % GO_PAL_FRAMES);
            PAL_setColors(GO_PAL_LINE * 16 + 1, goPalFrames[pf], 15, CPU);
        }
        SPR_update();
        SYS_doVBlankProcess();
        f++;
        u16 joy = JOY_readJoypad(JOY_1);
        bool start = (joy & BUTTON_START) && !(prev & BUTTON_START);
        prev = joy;
        if (f >= skipAt && start) break;
        if (f >= minHold && !XGM2_isPlaying()) break;
    }

    // Sale: el cartel se apaga en ~0,4 s.
    if (sign) {
        const u16 n = (u16)(fps * GO_FADE_OUT_T10 / 10);
        for (u16 i = 1; i <= n; i++) {
            u16 buf[15];
            for (u16 c = 0; c < 15; c++)
                buf[c] = goScale(goPalFrames[pf][c], (u16)(64 - 64 * i / n));
            PAL_setColors(GO_PAL_LINE * 16 + 1, buf, 15, CPU);
            SPR_update();
            SYS_doVBlankProcess();
        }
        SPR_releaseSprite(sign);
        SPR_update();
    }
    XGM2_stop();
    XGM2_setLoopNumber(-1);          // los demas temas repiten solos
    // Que se suelte START para no saltear la pantalla que sigue.
    while (JOY_readJoypad(JOY_1) & BUTTON_START)
        SYS_doVBlankProcess();
}

static void goPlayJingle(void) {
    XGM2_stop();
    XGM2_setLoopNumber(0);           // una sola vez
    XGM2_setFMVolume(GO_MUSIC_VOL);
    XGM2_setPSGVolume(GO_MUSIC_VOL);
    XGM2_play(music_gameover);
}

// Dentro del nivel: todo se funde a negro y recien ahi entra el cartel. Los
// cuatro bancos de paleta del nivel estan en uso (PAL3 la usan los soldiers
// blancos, los jefes y capas de fondo), asi que el cartel carga su paleta en
// PAL3 cuando la pantalla ya esta negra y se sacaron los sprites y planos.
void gameOverPlayInLevel(void) {
    const u16 fps = (u16)(IS_PAL_SYSTEM ? 50 : 60);

    PAL_interruptFade();             // por si quedo un fade asincrono
    u16 base[64];
    PAL_getColors(0, base, 64);

    Sprite* sign = goSignCreate();   // oculto; NULL si no hay VRAM de sprites
    goPlayJingle();

    const u16 n = (u16)(fps * GO_FADE_IN_T10 / 10);
    for (u16 i = 1; i <= n; i++) {
        u16 buf[64];
        const u16 k = (u16)(64 - 64 * i / n);
        for (u16 c = 0; c < 64; c++) buf[c] = goScale(base[c], k);
        PAL_setColors(0, buf, 64, CPU);
        SPR_update();
        SYS_doVBlankProcess();
    }
    // Todo en negro: se saca lo que quedaba para que, al cargar la paleta del
    // cartel en PAL3, nada del nivel se recoloree. Los sprites del nivel solo
    // se ocultan (el nivel los libera despues); los planos se vacian.
    for (Sprite* s = firstSprite; s; s = s->next)
        if (s != sign) SPR_setVisibility(s, HIDDEN);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    goRunSign(sign, fps);
    goShown = TRUE;
}

// Despues de los creditos: la escena ya esta limpia (todo en negro, sin sprites).
void gameOverScreen(void) {
    const u16 fps = (u16)(IS_PAL_SYSTEM ? 50 : 60);
    VDP_setBackgroundColor(0);
    Sprite* sign = goSignCreate();
    goPlayJingle();
    goRunSign(sign, fps);
}
