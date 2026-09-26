#include "pause_menu.h"
#include "hud.h"       // playerJoy
#include "level1.h"    // hud_font: la fuente que cargan los tres niveles

// ===========================================================================
// PAUSA + SELECTOR DE NIVELES — ver pause_menu.h
// ===========================================================================
//
// COMO SE DIBUJA SIN GASTAR VRAM
// ------------------------------
// El recuadro va en BG_A con prioridad alta (tapa a los sprites, que son de
// prioridad baja) y en PAL1, la paleta del HUD. Hay dos problemas:
//
//  1. Un plano no puede tener fondo Y texto en la misma celda, y los glifos
//     de hud_font tienen el fondo transparente: el texto quedaria "flotando"
//     sobre el nivel. Mientras dura la pausa se REESCRIBEN los glifos de la
//     fuente en VRAM con el fondo pintado del color mas oscuro de PAL1, y al
//     salir se vuelve a subir hud_font tal cual. El glifo del espacio NO se
//     toca: SGDK borra texto rellenando con ese tile (sin paleta ni
//     prioridad), asi que pintarlo mancharia todo el HUD.
//  2. Para las celdas vacias del recuadro hace falta un tile liso. Se usa el
//     tile de sistema 1 (TILE_SYSTEM_INDEX + 1): los niveles arrancan sus
//     tiles en TILE_USER_INDEX (16), asi que los 16 de sistema estan libres.
//     Igual se guarda su contenido y se repone.
//
// El tilemap de BG_A debajo del recuadro tambien se lee de VRAM y se repone
// al cerrar, asi que no importa que haya ahi.
//
// (26/09) BG_A puede estar scrolleado (los caños de la cloaca, la ruta de la
// freeway): al abrir se lee el H-scroll de la fila del recuadro y se busca la
// columna del PLANO que cae en la columna 8 de la pantalla. Todas las
// escrituras envuelven en las 64 columnas del plano.
// ===========================================================================

#define SOLID_TILE      (TILE_SYSTEM_INDEX + 1)
#define BLINK_FRAMES    12      // medio ciclo del parpadeo de lo seleccionado

// --- Tabla de niveles ------------------------------------------------------
// Una fila por "SCENE" del arcade; en cada una, sus subniveles. Para sumar un
// nivel: agregarlo aca (hasta 3 subniveles por fila).
#if DEV_LEVEL_SELECT
#define MAX_SUB 3
typedef struct {
    const char* name;
    u8          n;
    const char* sub[MAX_SUB];
    SceneId     scene[MAX_SUB];
} PauseRow;

static const PauseRow levelRows[] = {
    { "SCENE 1", 2, { "1-1", "1-2" }, { SCENE_1_1, SCENE_1_2 } },
    { "SCENE 2", 1, { "2-1" },        { SCENE_2_1 } },
    { "SCENE 3", 1, { "3-1" },        { SCENE_3_1 } },
    { "SCENE 4", 1, { "4-1" },        { SCENE_4_1 } },
    { "SCENE 5", 1, { "5-1" },        { SCENE_5_1 } },
};
#define LEVEL_ROWS  ((u16)(sizeof(levelRows) / sizeof(levelRows[0])))
#define ITEMS       ((u16)(LEVEL_ROWS + 1))   // item 0 = SEGUIR

// Filas del recuadro (relativas a PAUSE_BOX_ROW)
#define ROW_TITLE   0
#define ROW_ITEMS   2
#define VIS_ITEMS   4           // filas de items visibles (2..5); si hay mas, scrollea
#define ROW_HINT    7
// Columnas (relativas a PAUSE_BOX_COL)
#define COL_NAME    1
#define COL_SUB0    11
#define SUB_STEP    4
#endif

static u16  prevJoy;
static u16  solidAttr;
static u16  savedMap[PAUSE_BOX_W * PAUSE_BOX_H];
static u32  savedSolid[8];
static u32  fontBuf[8 * 8];     // 8 glifos por tanda

// ---------------------------------------------------------------------------
// VRAM: lectura directa por el puerto de datos del VDP
// ---------------------------------------------------------------------------
static void vramRead(u16 addr, u16* dst, u16 nWords) {
    vu32* ctrl = (vu32*) VDP_CTRL_PORT;
    vu16* data = (vu16*) VDP_DATA_PORT;
    *ctrl = VDP_READ_VRAM_ADDR((u32) addr);
    while (nWords--) *dst++ = *data;
}

static u16 boxCol;      // columna del PLANO donde arranca el recuadro

// Columna de BG_A que se ve en la columna PAUSE_BOX_COL de la pantalla.
// (Llamar con las interrupciones cortadas: usa el puerto de datos.)
static u16 findBoxCol(void) {
    u16 addr = VDP_getHScrollTableAddress();
    if (VDP_getHorizontalScrollingMode() != HSCROLL_PLANE)
        addr = (u16)(addr + PAUSE_BOX_ROW * 8 * 4);   // 8 lineas x (A,B) por fila
    u16 w;
    vramRead(addr, &w, 1);
    s16 sc = (s16)((s16)(w << 6) >> 6);               // 10 bits con signo
    s16 px = (s16)(PAUSE_BOX_COL * 8 - sc);
    return (u16)(((px + 4) >> 3) & 63);
}

static inline u16 planeCol(u16 relX) { return (u16)((boxCol + relX) & 63); }

static void boxFill(u16 relX, u16 relY, u16 w, u16 attr) {
    for (u16 i = 0; i < w; i++)
        VDP_setTileMapXY(BG_A, attr, planeCol((u16)(relX + i)),
                         (u16)(PAUSE_BOX_ROW + relY));
}

static void saveUnderBox(void) {
    VDP_waitDMACompletion();
    SYS_disableInts();
    VDP_setAutoInc(2);
    boxCol = findBoxCol();
    for (u16 r = 0; r < PAUSE_BOX_H; r++)
        for (u16 c = 0; c < PAUSE_BOX_W; c++)
            vramRead(VDP_getPlaneAddress(BG_A, planeCol(c), (u16)(PAUSE_BOX_ROW + r)),
                     &savedMap[r * PAUSE_BOX_W + c], 1);
    vramRead((u16)(SOLID_TILE * 32), (u16*) savedSolid, 16);
    SYS_enableInts();
}

static void restoreUnderBox(void) {
    for (u16 r = 0; r < PAUSE_BOX_H; r++)
        for (u16 c = 0; c < PAUSE_BOX_W; c++)
            VDP_setTileMapXY(BG_A, savedMap[r * PAUSE_BOX_W + c], planeCol(c),
                             (u16)(PAUSE_BOX_ROW + r));
    VDP_loadTileData(savedSolid, SOLID_TILE, 1, CPU);
    VDP_loadTileData(hud_font.tiles, TILE_FONT_INDEX, hud_font.numTile, CPU);
}

// Indice del color mas oscuro de PAL1 (sin contar el 0, que es transparente).
static u8 darkestPal1(void) {
    u8  best = 1;
    u16 bestLum = 0xFFFF;
    for (u8 i = 1; i < 16; i++) {
        u16 c = PAL_getColor((u16)(16 + i));
        u16 lum = (u16)((c & 0xE) + ((c >> 4) & 0xE) + ((c >> 8) & 0xE));
        if (lum < bestLum) { bestLum = lum; best = i; }
    }
    return best;
}

// Glifos con el fondo pintado (menos el espacio, ver arriba).
static void opaqueFont(u8 bg) {
    const u32* src = hud_font.tiles;
    u16 total = hud_font.numTile;
    for (u16 t0 = 1; t0 < total; t0 += 8) {
        u16 n = (u16)((total - t0 < 8) ? (total - t0) : 8);
        for (u16 i = 0; i < n * 8; i++) {
            u32 v = src[t0 * 8 + i];
            u32 out = 0;
            for (u16 s = 0; s < 32; s += 4) {
                u32 px = (v >> s) & 0xF;
                out |= (px ? px : bg) << s;
            }
            fontBuf[i] = out;
        }
        VDP_loadTileData(fontBuf, (u16)(TILE_FONT_INDEX + t0), n, CPU);
    }
}

// ---------------------------------------------------------------------------
// Dibujo
// ---------------------------------------------------------------------------
#if DEV_LEVEL_SELECT
static void boxClear(u16 relX, u16 relY, u16 w) {
    boxFill(relX, relY, w, solidAttr);
}
#endif

static void boxText(const char* s, u16 relX, u16 relY) {
    u16 x = relX;
    u16 y = (u16)(PAUSE_BOX_ROW + relY);
    while (*s) {
        char ch = *s++;
        u16 t = (ch == ' ')
              ? solidAttr
              : TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE,
                               TILE_FONT_INDEX + (u16)((u8) ch - 32));
        VDP_setTileMapXY(BG_A, t, planeCol(x++), y);
    }
}

static u16 textLen(const char* s) { u16 n = 0; while (s[n]) n++; return n; }

static void boxTextCentered(const char* s, u16 relY) {
    boxText(s, (u16)((PAUSE_BOX_W - textLen(s)) / 2), relY);
}

#if DEV_LEVEL_SELECT
static u16 selItem;     // 0 = SEGUIR, 1.. = fila de levelRows
static u8  selSub;
static u16 firstItem;   // primer item visible (scroll)

// Dibuja un item; 'on' = FALSE lo borra (para el parpadeo del seleccionado).
static void drawItem(u16 item, bool blinkOff) {
    if (item < firstItem || item >= firstItem + VIS_ITEMS) return;
    u16 y = (u16)(ROW_ITEMS + item - firstItem);
    boxClear(0, y, PAUSE_BOX_W);
    if (item == 0) {
        if (!(blinkOff && selItem == 0)) boxText("SEGUIR", COL_NAME, y);
        return;
    }
    const PauseRow* r = &levelRows[item - 1];
    boxText(r->name, COL_NAME, y);
    for (u8 s = 0; s < r->n; s++) {
        if (blinkOff && selItem == item && selSub == s) continue;
        boxText(r->sub[s], (u16)(COL_SUB0 + s * SUB_STEP), y);
    }
}

static void drawItems(bool blinkOff) {
    for (u16 i = 0; i < VIS_ITEMS; i++) {
        u16 item = (u16)(firstItem + i);
        if (item < ITEMS) drawItem(item, blinkOff);
        else boxClear(0, (u16)(ROW_ITEMS + i), PAUSE_BOX_W);
    }
}

static void clampSub(void) {
    if (selItem == 0) { selSub = 0; return; }
    u8 n = levelRows[selItem - 1].n;
    if (selSub >= n) selSub = (u8)(n - 1);
}
#endif

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
void pauseReset(void) {
    prevJoy = JOY_readJoypad(playerJoy(0));
#if DEV_LEVEL_SELECT
    selItem = 0;
    selSub = 0;
    firstItem = 0;
#endif
}

SceneId pausePoll(Player** pls, u8 nPl) {
    u16 joy = JOY_readJoypad(playerJoy(0));
    bool pressed = (joy & BUTTON_START) && !(prevJoy & BUTTON_START);
    prevJoy = joy;
    if (!pressed || nPl == 0) return PAUSE_NO_JUMP;
    // Jugador 1 en game over: ese START es del CONTINUE?, no de la pausa.
    if (isPlayerGameOver(pls[0])) return PAUSE_NO_JUMP;

    // --- Abrir ---------------------------------------------------------------
    XGM2_pause();
    saveUnderBox();
    u8 bg = darkestPal1();
    VDP_fillTileData((u8)(bg | (bg << 4)), SOLID_TILE, 1, TRUE);
    opaqueFont(bg);
    solidAttr = TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, SOLID_TILE);

    for (u16 r = 0; r < PAUSE_BOX_H; r++) boxFill(0, r, PAUSE_BOX_W, solidAttr);
    SceneId result = PAUSE_NO_JUMP;

#if DEV_LEVEL_SELECT
    boxTextCentered("PAUSA", ROW_TITLE);
    boxTextCentered("START: ELEGIR", ROW_HINT);
    // Arranca siempre en SEGUIR: START, START sale de la pausa.
    selItem = 0;
    selSub = 0;
    firstItem = 0;
    drawItems(FALSE);
#else
    boxTextCentered("PAUSA", 2);
    boxTextCentered("START: SEGUIR", 5);
#endif

    u16 blink = 0;
    bool blinkOff = FALSE;
    while (1) {
        SYS_doVBlankProcess();
        joy = JOY_readJoypad(playerJoy(0));
        u16 hit = (u16)(joy & ~prevJoy);
        prevJoy = joy;

#if DEV_LEVEL_SELECT
        bool moved = FALSE;
        if ((hit & BUTTON_UP) && selItem > 0)               { selItem--; moved = TRUE; }
        if ((hit & BUTTON_DOWN) && selItem + 1 < ITEMS)     { selItem++; moved = TRUE; }
        if ((hit & BUTTON_LEFT) && selSub > 0)              { selSub--;  moved = TRUE; }
        if ((hit & BUTTON_RIGHT) && selItem > 0 &&
            selSub + 1 < levelRows[selItem - 1].n)          { selSub++;  moved = TRUE; }
        if (moved) {
            clampSub();
            if (selItem < firstItem) firstItem = selItem;
            if (selItem >= firstItem + VIS_ITEMS) firstItem = (u16)(selItem - VIS_ITEMS + 1);
            blink = 0;
            blinkOff = FALSE;
            drawItems(FALSE);
        }
        if (++blink >= BLINK_FRAMES) {
            blink = 0;
            blinkOff = !blinkOff;
            drawItem(selItem, blinkOff);
        }
        if (hit & BUTTON_START) {
            if (selItem > 0) result = levelRows[selItem - 1].scene[selSub];
            break;
        }
#else
        (void) blink; (void) blinkOff;
        if (hit & BUTTON_START) break;
#endif
    }

    // --- Cerrar --------------------------------------------------------------
    restoreUnderBox();
    if (result == PAUSE_NO_JUMP) {
        XGM2_resume();
        return PAUSE_NO_JUMP;
    }

    // Salto de nivel: el nivel que sigue arranca de cero con vidas y barra
    // llenas (para probar tranquilo), pero se conserva el puntaje. Tambien
    // vuelve el companero que estuviera fuera de juego. La musica queda en
    // pausa: la corta el clearScene del nivel y la siguiente arranca con
    // XGM2_play, que limpia la pausa.
    for (u8 k = 0; k < nPl; k++) {
        pls[k]->lives  = vidasIniciales;
        pls[k]->health = PLAYER_MAX_HEALTH;
        playerPersistSave(pls[k]);
    }
    return result;
}
