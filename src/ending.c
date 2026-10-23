// ===========================================================================
// ending.c - EPILOGO y CREDITOS del final (18/10)
// ===========================================================================
// Despues de vencer a Shredder (6-2, level9_1.c: explota el Technodrome):
//   1. showEpilogue      texto del epilogo del arcade, letra a letra, en negro.
//   2. showStaffCredits  la ciudad de la intro (roof_bg_b, la vista hacia
//                        arriba entre los edificios) con los cuadros de las
//                        tortugas pasando de derecha a izquierda y, arriba, el
//                        staff del arcade y despues el del port.
//   3. SCENE_GAME_OVER   (scenes.c) y de vuelta al logo de SEGA.
//
// (18/10) Por ahora SIN musica, ni en el epilogo ni en los creditos.
//
// CREDITOS, como estan armados:
//   * Modo H32 (256 px): el fondo de la ciudad es de 256, como en la intro.
//   * BG_B = la ciudad (PAL0), quieta.
//   * BG_A = filas 0..11 el texto (PAL2 amarillo = rol, PAL3 verde = nombre)
//            filas 13..26 el cuadro de UNA tortuga (160x112, PAL1), que
//            entra por la derecha y sale por la izquierda; despues la que
//            sigue (Leo, Mike, Don, Raph, y de nuevo).
//     El scroll horizontal va POR TILE: las filas del texto quedan en 0 y
//     las del cuadro corren. El cuadro se dibuja en x = 256 del plano (fuera
//     de pantalla con el scroll en 0); el scroll avanza hasta que sale por
//     la izquierda (416 px) y ahi, con la franja vacia, se dibuja el
//     siguiente en el mismo lugar y el scroll vuelve a 0. Sin sprites.
//   * El logo de KONAMI ("PRODUCED BY") se dibuja en las filas del texto.
//   * START corta (igual que el resto de las pantallas de texto).
// ===========================================================================

#include "scenes.h"
#include "level1.h"       // title_font, title_font_pal
#include "roof_april.h"   // roof_bg_b (la ciudad de la intro, 256x224)
#include "credits.h"      // credits_turtle_0..3, credits_konami

// Espera 'frames' cuadros; TRUE si apretaron START (flanco).
static u16 endPrevJoy;
static bool endWait(u16 frames) {
    while (frames--) {
        u16 joy = JOY_readJoypad(JOY_1);
        bool hit = (joy & BUTTON_START) && !(endPrevJoy & BUTTON_START);
        endPrevJoy = joy;
        if (hit) return TRUE;
        SYS_doVBlankProcess();
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// 1. EPILOGO
// ---------------------------------------------------------------------------
// Texto del arcade tal cual (incluido el "DIMENTION"). Fuente arcade
// (title_font, 40 columnas), una linea cada dos filas y un blanco entre
// parrafos. Cada linea se centra sola.
#define EPI_CHAR_DELAY   3
#define EPI_HOLD_SECS    8

static const struct { const char* text; u16 row; } epiLines[] = {
    { "EPILOGUE",                                2 },
    { "FREAKED THE FOOTS, MANGLED THE MOUSERS",  5 },
    { "AND TOTALLED THE TECHNODROME.",           7 },
    { "THAT'S .. TURTLE POWER!!",                9 },
    { "BUT, WHAT ABOUT THE SHREDDER AND",       12 },
    { "KRANG ...!?",                            14 },
    { "BURNED TO TOAST ?",                      16 },
    { "VAPORIZED TO MILK SHAKE ??",             18 },
    { "OR.. ESCAPED TO DIMENTION X ???",        20 },
    { "UNTIL WE KNOW, NONE OF US CAN SLEEP",    23 },
    { "SAFELY IN OUR BEDS.. ER, SHELLS!",       25 },
};
#define EPI_LINES  ((u16)(sizeof(epiLines) / sizeof(epiLines[0])))

SceneId showEpilogue() {
    clearScene();

    VDP_setScreenWidth320();
    VDP_loadFont(&title_font, DMA);
    VDP_setTextPlane(BG_A);
    VDP_setTextPriority(0);
    VDP_setTextPalette(PAL0);
    VDP_setBackgroundColor(0);
    PAL_setColors(0, title_font_pal.data, title_font_pal.length, DMA);

    endPrevJoy = JOY_readJoypad(JOY_1);

    bool skipped = FALSE;
    char buf[2] = { 0, 0 };
    for (u16 i = 0; i < EPI_LINES && !skipped; i++) {
        const char* t = epiLines[i].text;
        u16 x = (u16)((40 - strlen(t)) / 2);
        for (u16 c = 0; t[c] && !skipped; c++) {
            buf[0] = t[c];
            if (buf[0] != ' ') VDP_drawText(buf, (u16)(x + c), epiLines[i].row);
            skipped = endWait(EPI_CHAR_DELAY);
        }
        if (!skipped) skipped = endWait(EPI_CHAR_DELAY * 4);   // respiro entre lineas
    }
    if (skipped)
        for (u16 i = 0; i < EPI_LINES; i++)
            VDP_drawText(epiLines[i].text,
                         (u16)((40 - strlen(epiLines[i].text)) / 2), epiLines[i].row);

    endWait((IS_PAL_SYSTEM ? 50 : 60) * EPI_HOLD_SECS);

    VDP_loadFont(&font_default, DMA);
    clearScene();
    return SCENE_STAFF_CREDITS;
}

// ---------------------------------------------------------------------------
// 2. CREDITOS
// ---------------------------------------------------------------------------
#define CR_COLS          32          // H32
#define CR_TEXT_ROWS     12          // filas 0..11: texto (scroll 0)
#define CR_PANEL_ROW     13          // filas 13..26: el cuadro (scrollea)
#define CR_PANEL_ROWS    14          // 112 px
#define CR_PANEL_COL     32          // x = 256 del plano: fuera de pantalla
#define CR_PANEL_TILES  280          // 20x14 tiles, el peor caso de un cuadro
#define CR_PANEL_END    416          // 256 + 160: ya salio por la izquierda
#define CR_ROW_ROLE       2
#define CR_ROW_ROLE2      4
#define CR_ROW_NAME1      7
#define CR_ROW_NAME2      9
#define CR_LOGO_COL      11          // (32 - 9) / 2
#define CR_LOGO_ROW       7
#define CR_CHAR_DELAY     2
#define CR_ENTRY_SECS     4          // cada pantalla de credito
#define CR_LAST_SECS      6          // la ultima se queda un poco mas
#define CR_SCROLL_SPEED   1          // px por cuadro de los cuadros de las tortugas

// Rol (amarillo, hasta dos lineas) y hasta dos nombres (verde). 'logo' = en
// vez de nombres, el logo de KONAMI. Textos en MAYUSCULAS y solo con los
// glifos que tiene la fuente arcade (letras, digitos y ! " ' ( ) , - . : ; ?).
// Maximo 32 caracteres por linea (H32).
typedef struct {
    const char* role;
    const char* role2;
    const char* name1;
    const char* name2;
    bool        logo;
} CreditEntry;

static const CreditEntry crEntries[] = {
    // --- Staff del arcade (1989) ---
    { "DIRECTOR",         NULL, "H, OHYAMA",          NULL,         FALSE },
    { "SUB DIRECTOR",     NULL, "K, TAKABAYASHI",     NULL,         FALSE },
    { "GAME PROGRAMMER",  NULL, "G, SUZUKI",          NULL,         FALSE },
    { "CHARACTER DESIGN", NULL, "MORIYAMA 25",        NULL,         FALSE },
    { "GRAPHIC DESIGN",   NULL, "K, HATTORI",         NULL,         FALSE },
    { "VISUAL DESIGN",    NULL, "Y, ASANO",           NULL,         FALSE },
    { "ANIMATION DESIGN", NULL, "K, YAMASHITA",       NULL,         FALSE },
    { "SOUND DIRECTOR",   NULL, "S, TASAKA",          NULL,         FALSE },
    { "SOUND DESIGNER",   NULL, "IMO, HIDETO",        NULL,         FALSE },
    { "MUSIC COMPOSER",   NULL, "M, IZUMI",           "MIKI CHANG", FALSE },
    { "HARDWARE DESIGN",  NULL, "S, MATSUMOTO",       NULL,         FALSE },
    { "SPECIAL THANKS",   NULL, "M, INOUE",           NULL,         FALSE },
    { "PRODUCED BY",      NULL, NULL,                 NULL,         TRUE  },
    // --- El port ---
    { "MEGA DRIVE PORT",  NULL, NULL,                 NULL,         FALSE },
    { "DEVELOPER",        NULL, "GUSTAVO VALENZUELA", NULL,         FALSE },
    { "MUSIC",            NULL, "SANSENPAI35",        NULL,         FALSE },
    { "2D ASSETS AND",    "GRAPHIC ADAPTATION", "ROBSON RICARDO", NULL, FALSE },
    { "2D ASSETS AND",    "DEVELOPMENT",        "RAY CASTELLO",   NULL, FALSE },
    { "THANK YOU",        NULL, "TO THE COMMUNITY!",  NULL,         FALSE },
};
#define CR_ENTRIES  ((u16)(sizeof(crEntries) / sizeof(crEntries[0])))

static u16 crScroll;
static s16 crScrollTbl[CR_PANEL_ROWS];
static u16 crKonamiTile;
static u16 crPanelTile;
static u16 crTurtle;              // cuadro en pantalla (0..3)

static const Image* const crTurtles[4] = {
    &credits_turtle_0, &credits_turtle_1, &credits_turtle_2, &credits_turtle_3,
};

static void crDrawTurtle(u16 k) {
    VDP_drawImageEx(BG_A, crTurtles[k & 3],
                    TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, crPanelTile),
                    CR_PANEL_COL, CR_PANEL_ROW, FALSE, DMA);
}

static void crStep(void) {
    crScroll += CR_SCROLL_SPEED;
    if (crScroll >= CR_PANEL_END) {
        // Salio por la izquierda: la franja esta vacia, se dibuja la que
        // sigue fuera de pantalla y vuelve a entrar por la derecha.
        crScroll = 0;
        crTurtle = (u16)((crTurtle + 1) & 3);
        crDrawTurtle(crTurtle);
    }
    s16 v = (s16)-(s16)crScroll;
    for (u16 i = 0; i < CR_PANEL_ROWS; i++) crScrollTbl[i] = v;
    VDP_setHorizontalScrollTile(BG_A, CR_PANEL_ROW, crScrollTbl, CR_PANEL_ROWS, DMA_QUEUE);
}

// Espera con el tren de cuadros andando. TRUE si apretaron START.
static bool crWait(u16 frames) {
    while (frames--) {
        u16 joy = JOY_readJoypad(JOY_1);
        bool hit = (joy & BUTTON_START) && !(endPrevJoy & BUTTON_START);
        endPrevJoy = joy;
        if (hit) return TRUE;
        crStep();
        SYS_doVBlankProcess();
    }
    return FALSE;
}

// Letra a letra; TRUE si apretaron START.
static bool crTypewriter(const char* t, u16 row, u16 pal) {
    if (!t) return FALSE;
    u16 x = (u16)((CR_COLS - strlen(t)) / 2);
    char buf[2] = { 0, 0 };
    for (u16 c = 0; t[c]; c++) {
        buf[0] = t[c];
        if (buf[0] != ' ')
            VDP_drawTextEx(BG_A, buf, TILE_ATTR(pal, TRUE, FALSE, FALSE),
                           (u16)(x + c), row, DMA);
        if (crWait(CR_CHAR_DELAY)) return TRUE;
    }
    return FALSE;
}

SceneId showStaffCredits() {
    clearScene();

    // Sin motor de sprites: deja todo el heap para descomprimir las IMAGE.
    SPR_end();
    VDP_setScreenWidth256();
    VDP_setBackgroundColor(0);
    VDP_loadFont(&title_font, DMA);

    // Fondo y cuadros. VRAM: la ciudad, despues los cuadros y el logo.
    u16 idx = TILE_USER_INDEX;
    VDP_drawImageEx(BG_B, &roof_bg_b, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, idx),
                    0, 0, FALSE, DMA);
    idx += roof_bg_b.tileset->numTile;
    crPanelTile = idx;
    idx += CR_PANEL_TILES;
    crKonamiTile = idx;
    crTurtle = 0;
    crDrawTurtle(0);

    VDP_setScrollingMode(HSCROLL_TILE, VSCROLL_PLANE);
    {
        static s16 zero[28] = { 0 };
        VDP_setHorizontalScrollTile(BG_A, 0, zero, 28, DMA);
        VDP_setHorizontalScrollTile(BG_B, 0, zero, 28, DMA);
    }
    crScroll = 0;

    // Paletas: PAL0 ciudad, PAL1 cuadros/logo, PAL2 rol (amarillo, con
    // sombra naranja/roja como en el arcade), PAL3 nombre (verde). La fuente
    // usa los indices 1..3 (cara, medio, sombra).
    u16 target[64];
    for (u16 i = 0; i < 64; i++) target[i] = 0;
    for (u16 i = 0; i < 16; i++) {
        target[i]      = roof_bg_b.palette->data[i];
        target[16 + i] = credits_turtle_0.palette->data[i];
    }
    target[32 + 1] = RGB24_TO_VDPCOLOR(0xFFEE22);
    target[32 + 2] = RGB24_TO_VDPCOLOR(0xFF8800);
    target[32 + 3] = RGB24_TO_VDPCOLOR(0xCC2200);
    target[48 + 1] = RGB24_TO_VDPCOLOR(0x66EE66);
    target[48 + 2] = RGB24_TO_VDPCOLOR(0x22AA44);
    target[48 + 3] = RGB24_TO_VDPCOLOR(0x116622);
    PAL_fadeInAll(target, 20, TRUE);
    endPrevJoy = JOY_readJoypad(JOY_1);
    while (PAL_isDoingFade()) { crStep(); SYS_doVBlankProcess(); }

    const u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    bool skipped = FALSE;
    for (u16 e = 0; e < CR_ENTRIES && !skipped; e++) {
        const CreditEntry* ce = &crEntries[e];
        VDP_clearTileMapRect(BG_A, 0, 0, 64, CR_TEXT_ROWS);
        skipped = crTypewriter(ce->role, CR_ROW_ROLE, PAL2);
        if (!skipped) skipped = crTypewriter(ce->role2, CR_ROW_ROLE2, PAL2);
        if (ce->logo)
            VDP_drawImageEx(BG_A, &credits_konami,
                            TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, crKonamiTile),
                            CR_LOGO_COL, CR_LOGO_ROW, FALSE, DMA);
        if (!skipped) skipped = crTypewriter(ce->name1, CR_ROW_NAME1, PAL3);
        if (!skipped) skipped = crTypewriter(ce->name2, CR_ROW_NAME2, PAL3);
        if (skipped) break;
        u16 hold = (u16)(fps * ((e + 1 == CR_ENTRIES) ? CR_LAST_SECS : CR_ENTRY_SECS));
        skipped = crWait(hold);
    }

    // Salida: fundido y todo como lo esperan las demas escenas.
    PAL_fadeOutAll(30, TRUE);
    while (PAL_isDoingFade()) { crStep(); SYS_doVBlankProcess(); }
    VDP_setScreenWidth320();
    VDP_loadFont(&font_default, DMA);
    SPR_init();
    clearScene();
    return SCENE_GAME_OVER;
}
