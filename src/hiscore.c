// ============================================================================
// hiscore.c -- carga de iniciales y "BEST 10 PLAYERS" (20/10)
// ============================================================================
// Como en el arcade: al terminar la partida (game over, o despues de los
// creditos del final) los jugadores que entran en el top 10 ponen 3 letras,
// (23/10) todos a la vez en la misma pantalla: cada tortuga tiene su lugar,
// con su retrato del selector a color dentro de un marco (con "1UP", su
// puntaje y el puesto); las tortugas que no cargan quedan en negro, sin el
// fondo plano de color. Despues se ve la tabla: titulo, puesto, iniciales y puntos,
// con los retratos del selector pasando por detras de derecha a izquierda,
// cada uno en un solo tono.
//
// La tabla queda en la SRAM del cartucho (ver hsSramLoad/hsSramSave): el
// header ya declara SRAM de 8 bits en las direcciones impares (rom_head.c).
// Con una ROM de mas de 2 MB la SRAM se ve EN LUGAR de la ROM de 0x200000 en
// adelante mientras esta habilitada (registro 0xA130F1, SRAM_enable de SGDK):
// por eso se lee y escribe todo de una, con las interrupciones apagadas y el
// Z80 frenado (el driver de sonido lee de la ROM), a un buffer en RAM, sin
// tocar datos constantes en el medio.
//
// Graficos: tools/gen_highscore.py (hiscore_gfx.c y hs_portraits.png).
// ============================================================================
#include <genesis.h>
#include "scenes.h"
#include "hud.h"            // playerJoy
#include "player.h"         // playerFinalPeek, MAX_PLAYERS
#include "menus.h"          // hs_portraits
#include "audio.h"          // music_profiles (iniciales), music_charselect (tabla)
#include "hiscore.h"
#include "hiscore_gfx.h"

#define HS_N          10
#define HS_NAME        3
#define HS_REC         6          // 3 letras + puntaje (2) + tortuga
#define HS_SRAM_SIZE  (5 + HS_N * HS_REC + 1)   // "TMNT" + version + tabla + suma
// (23/10) 2: tabla de fabrica nueva (290 arriba). Una SRAM con la version 1
// se vuelve a formatear con la tabla nueva.
#define HS_VERSION     2

typedef struct { char name[HS_NAME]; u16 score; u8 ch; } HsEntry;

static HsEntry hsTable[HS_N];
static bool    hsLoaded = FALSE;
static s8      hsNew[MAX_PLAYERS] = { -1, -1, -1, -1 };   // puesto recien cargado por cada jugador

// Tabla de fabrica: los personajes del juego. (23/10) Mas alta: el 1ro con
// 290 y de a 30 hacia abajo (antes 100..10).
static const HsEntry hsDefault[HS_N] = {
    { {'L','E','O'}, 290, 0 }, { {'M','I','K'}, 260, 1 }, { {'D','O','N'}, 230, 2 },
    { {'R','A','P'}, 200, 3 }, { {'A','P','R'}, 170, 0 }, { {'S','P','L'}, 140, 1 },
    { {'C','A','S'}, 110, 2 }, { {'B','E','B'},  80, 3 }, { {'R','O','C'},  50, 0 },
    { {'S','H','R'},  20, 1 },
};

// ---------------------------------------------------------------------------
// SRAM
// ---------------------------------------------------------------------------
static u8 hsBuf[HS_SRAM_SIZE];

// Copia SRAM <-> hsBuf. Sin datos constantes adentro (la ROM alta no se ve).
// (23/10) Al escribir, se relee lo escrito y se reintenta (hasta 3 veces) si
// algun byte no quedo: en el log de BlastEm/Gens KMod queda si la SRAM no
// responde (cabecera mal leida por el emulador, SRAM no mapeada).
static bool hsSramIO(bool write) {
    bool ok = TRUE;
    SYS_disableInts();
    Z80_requestBus(TRUE);
    SRAM_enable();
    if (write) {
        for (u16 tries = 0; tries < 3; tries++) {
            for (u16 i = 0; i < HS_SRAM_SIZE; i++) SRAM_writeByte(i, hsBuf[i]);
            ok = TRUE;
            for (u16 i = 0; i < HS_SRAM_SIZE; i++)
                if (SRAM_readByte(i) != hsBuf[i]) { ok = FALSE; break; }
            if (ok) break;
        }
    } else {
        for (u16 i = 0; i < HS_SRAM_SIZE; i++) hsBuf[i] = SRAM_readByte(i);
    }
    SRAM_disable();
    Z80_releaseBus();
    SYS_enableInts();
    if (!ok) KLog("hiscore: la SRAM no guardo los datos (no esta mapeada?)");
    return ok;
}

static u8 hsSum(void) {
    u8 s = 0x5A;
    for (u16 i = 0; i < HS_SRAM_SIZE - 1; i++) s = (u8)((s << 1 | s >> 7) ^ hsBuf[i]);
    return s;
}

static void hsSramSave(void) {
    hsBuf[0] = 'T'; hsBuf[1] = 'M'; hsBuf[2] = 'N'; hsBuf[3] = 'T';
    hsBuf[4] = HS_VERSION;
    for (u16 k = 0; k < HS_N; k++) {
        u8* r = &hsBuf[5 + k * HS_REC];
        r[0] = (u8)hsTable[k].name[0];
        r[1] = (u8)hsTable[k].name[1];
        r[2] = (u8)hsTable[k].name[2];
        r[3] = (u8)(hsTable[k].score >> 8);
        r[4] = (u8)hsTable[k].score;
        r[5] = hsTable[k].ch;
    }
    hsBuf[HS_SRAM_SIZE - 1] = hsSum();
    hsSramIO(TRUE);
}

void hiscoreLoad(void) {
    if (hsLoaded) return;
    hsLoaded = TRUE;
    hsSramIO(FALSE);
    bool ok = hsBuf[0] == 'T' && hsBuf[1] == 'M' && hsBuf[2] == 'N' && hsBuf[3] == 'T' &&
              hsBuf[4] == HS_VERSION && hsBuf[HS_SRAM_SIZE - 1] == hsSum();
    if (ok) {
        for (u16 k = 0; k < HS_N; k++) {
            const u8* r = &hsBuf[5 + k * HS_REC];
            hsTable[k].name[0] = (char)r[0];
            hsTable[k].name[1] = (char)r[1];
            hsTable[k].name[2] = (char)r[2];
            hsTable[k].score = (u16)((r[3] << 8) | r[4]);
            hsTable[k].ch = r[5] & 3;
        }
    } else {
        for (u16 k = 0; k < HS_N; k++) hsTable[k] = hsDefault[k];
        hsSramSave();                 // deja la SRAM formateada
    }
}

// Puesto que le tocaria a 'score' (HS_N = no entra). Empate: queda abajo.
static u8 hsRankOf(u16 score) {
    for (u8 k = 0; k < HS_N; k++)
        if (score > hsTable[k].score) return k;
    return HS_N;
}

static void hsInsert(u8 rank, const char* name, u16 score, u8 ch) {
    for (s16 k = HS_N - 1; k > rank; k--) hsTable[k] = hsTable[k - 1];
    hsTable[rank].name[0] = name[0];
    hsTable[rank].name[1] = name[1];
    hsTable[rank].name[2] = name[2];
    hsTable[rank].score = score;
    hsTable[rank].ch = ch;
}

// ---------------------------------------------------------------------------
// Dibujo: fuente alta (8x16) en BG_A, degrade en BG_B, marco
// ---------------------------------------------------------------------------
static u16 hsFontBase, hsFrameBase, hsGradBase;

static void hsLoadGfx(const u32* gradTiles) {
    hsFontBase  = TILE_USER_INDEX;
    hsFrameBase = hsFontBase + HS_FONT_TILES;
    hsGradBase  = hsFrameBase + HS_FRAME_TILES;
    VDP_loadTileData(hsFontTiles, hsFontBase, HS_FONT_TILES, DMA);
    VDP_loadTileData(hsFrameTiles, hsFrameBase, HS_FRAME_TILES, DMA);
    VDP_loadTileData(gradTiles, hsGradBase, 28, DMA);
    for (u16 r = 0; r < 28; r++)
        VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, hsGradBase + r),
                            0, r, 64, 1);
}

static s16 hsGlyph(char c) {
    const char* g = HS_GLYPH_STR;
    for (s16 i = 0; g[i]; i++) if (g[i] == c) return i;
    return 0;                         // espacio
}

static void hsPutChar(u16 x, u16 y, char c, u8 color) {
    u16 t = (u16)(hsFontBase + ((u16)color * HS_GLYPHS + hsGlyph(c)) * 2);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, t), x, y);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, t + 1), x, y + 1);
}

static void hsText(u16 x, u16 y, const char* s, u8 color) {
    for (; *s; s++, x++) hsPutChar(x, y, *s, color);
}

static const char* const hsRankStr[HS_N] = {
    "1ST", "2ND", "3RD", "4TH", "5TH", "6TH", "7TH", "8TH", "9TH", "10TH"
};

static void hsFrame(u16 x0, u16 y0, u16 x1, u16 y1) {
    u16 b = hsFrameBase;
#define FT(i) TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, b + (i))
    VDP_setTileMapXY(BG_A, FT(0), x0, y0);
    VDP_setTileMapXY(BG_A, FT(2), x1, y0);
    VDP_setTileMapXY(BG_A, FT(5), x0, y1);
    VDP_setTileMapXY(BG_A, FT(7), x1, y1);
    VDP_fillTileMapRect(BG_A, FT(1), x0 + 1, y0, x1 - x0 - 1, 1);
    VDP_fillTileMapRect(BG_A, FT(6), x0 + 1, y1, x1 - x0 - 1, 1);
    VDP_fillTileMapRect(BG_A, FT(3), x0, y0 + 1, 1, y1 - y0 - 1);
    VDP_fillTileMapRect(BG_A, FT(4), x1, y0 + 1, 1, y1 - y0 - 1);
#undef FT
}

// Numero sin ceros adelante, alineado a la derecha terminando en 'xEnd'.
static void hsNumber(u16 xEnd, u16 y, u16 v, u8 color, u16 step) {
    u16 x = xEnd;
    do {
        hsPutChar(x, y, (char)('0' + v % 10), color);
        v /= 10;
        x -= step;
    } while (v);
}

// Retrato del selector (SPRITE 8x16 tiles, frame = tortuga).
static Sprite* hsPortrait(u8 ch, s16 x, s16 y, u16 pal) {
    Sprite* s = SPR_addSprite(&hs_portraits, x, y, TILE_ATTR(pal, FALSE, FALSE, FALSE));
    if (s) {
        SPR_setAutoAnimation(s, FALSE);
        SPR_setFrame(s, ch & 3);
    }
    return s;
}

static void hsSceneBegin(void) {
    clearSceneEx(TRUE);               // la musica sigue de una pantalla a la otra
    SPR_initEx(4 * 128 + 16);         // 4 retratos de 128 tiles
    VDP_setScreenWidth320();
    VDP_setBackgroundColor(0);
}

static void hsSceneEnd(void) {
    PAL_fadeOutAll(20, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();
    SPR_reset();
    SPR_update();
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    SYS_doVBlankProcess();
    SPR_initEx(752);                  // lo que esperan las demas escenas
}

static void hsFadeIn(const u16* p0, const u16* p1, const u16* p2, const u16* p3) {
    u16 pal[64];
    memcpy(&pal[0],  p0, 32);
    memcpy(&pal[16], p1, 32);
    memcpy(&pal[32], p2, 32);
    memcpy(&pal[48], p3, 32);
    PAL_fadeIn(0, 63, pal, 20, TRUE);
}

// ---------------------------------------------------------------------------
// CARGA DE INICIALES
// ---------------------------------------------------------------------------
#define HS_ENTRY_SECS  30
static const char hsAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.!?-*& ";
#define HS_ALPHA_N ((s16)(sizeof(hsAlphabet) - 1))

// (23/10) TODOS LOS JUGADORES A LA VEZ (antes: una pantalla por jugador). La
// pantalla tiene un lugar fijo por TORTUGA (Leo, Mike, Don, Raph, de
// izquierda a derecha, HS_SLOT_W columnas cada uno): como cada jugador tiene
// una tortuga distinta, nunca se pisan. El que entra en la tabla tiene su
// retrato a color, su marco con "nUP", su puntaje, el puesto y sus 3 letras;
// las tortugas que no cargan iniciales quedan en negro. Cada uno carga con su
// joystick; termina cuando terminaron todos o se acaba el tiempo.
#define HS_SLOT_W     10          // columnas por tortuga (40 / 4)
#define HS_LETTER_Y   24

typedef struct {
    u8   k, ch, rank;
    u16  score;
    char name[HS_NAME];
    s16  cur, sel;
    u16  joy, prev, rep, blink;
} HsInput;

// Columna 0 del lugar de la tortuga 'ch' (el marco va de x0 + 1 a x0 + 9).
static u16 hsSlotX(u8 ch) { return (u16)(ch * HS_SLOT_W); }

static void hsLetter(const HsInput* in, s16 i, char c, u8 color) {
    hsPutChar((u16)(hsSlotX(in->ch) + 3 + i * 2), HS_LETTER_Y, c, color);
}

static s16 hsAlphaIndex(char c) {
    for (s16 i = 0; i < HS_ALPHA_N; i++) if (hsAlphabet[i] == c) return i;
    return 0;
}

// Un cuadro de entrada de un jugador. TRUE = ya tiene las 3 letras.
static bool hsInputStep(HsInput* in) {
    if (in->cur >= HS_NAME) return TRUE;
    u16 v = JOY_readJoypad(in->joy);
    u16 down = v & ~in->prev;
    if (in->prev == 0xFFFF) down = 0;     // lo que venia apretado no cuenta
    in->prev = v;
    in->blink++;

    s16 d = 0;
    if (down & BUTTON_UP)   { d = 1;  in->rep = 0; }
    if (down & BUTTON_DOWN) { d = -1; in->rep = 0; }
    if (v & (BUTTON_UP | BUTTON_DOWN)) {
        if (++in->rep > 18 && (in->rep & 3) == 0) d = (v & BUTTON_UP) ? 1 : -1;
    }
    if (d) {
        in->sel = (s16)((in->sel + d + HS_ALPHA_N) % HS_ALPHA_N);
        in->name[in->cur] = hsAlphabet[in->sel];
        in->blink = 0;
    }
    if ((down & BUTTON_LEFT) && in->cur > 0) {
        hsLetter(in, in->cur, in->name[in->cur], HS_WHITE);
        in->cur--;
        in->sel = hsAlphaIndex(in->name[in->cur]);
    }
    if (down & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START | BUTTON_RIGHT)) {
        hsLetter(in, in->cur, in->name[in->cur], HS_WHITE);
        in->cur++;
        if (in->cur < HS_NAME) {
            in->name[in->cur] = in->name[in->cur - 1];      // arranca en la misma letra
            in->sel = hsAlphaIndex(in->name[in->cur]);
        }
        in->blink = 0;
    }
    for (s16 i = 0; i < HS_NAME; i++) {
        if (i == in->cur) hsLetter(in, i, (in->blink & 16) ? ' ' : in->name[i], HS_LAV);
        else if (i > in->cur) hsLetter(in, i, in->name[i], HS_WHITE);
    }
    return in->cur >= HS_NAME;
}

static void hsEntryAll(HsInput* in, u8 n) {
    hsSceneBegin();
    hsLoadGfx(hsGradEntryTiles);

    bool entering[4] = { FALSE, FALSE, FALSE, FALSE };
    for (u8 i = 0; i < n; i++) {
        HsInput* p = &in[i];
        u16 x0 = hsSlotX(p->ch);
        entering[p->ch & 3] = TRUE;
        hsFrame((u16)(x0 + 1), 1, (u16)(x0 + 9), 27);
        char up[4] = { (char)('1' + p->k), 'U', 'P', 0 };
        hsText((u16)(x0 + 2), 2, up, HS_LAV);
        hsNumber((u16)(x0 + 8), 2, p->score, HS_WHITE, 1);
        hsText((u16)(x0 + 3), 5, hsRankStr[p->rank], HS_WHITE);
        p->name[0] = p->name[1] = p->name[2] = 'A';
        p->cur = 0; p->sel = 0; p->prev = 0xFFFF; p->rep = 0; p->blink = 0;
        p->joy = playerJoy(p->k);
    }
    // Los cuatro retratos, cada uno en su lugar: a color (PAL1, los colores
    // del selector) los que cargan, en negro (PAL2) los demas.
    for (u8 t = 0; t < 4; t++)
        hsPortrait(t, (s16)(12 + 80 * t), 64, entering[t] ? PAL1 : PAL2);
    SPR_update();
    hsFadeIn(hsPalEntry, hs_portraits.palette->data, hsPalBlack, hsPalBlack);

    u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    u16 timer = (u16)(HS_ENTRY_SECS * fps);
    while (timer > 0) {
        timer--;
        bool all = TRUE;
        for (u8 i = 0; i < n; i++) if (!hsInputStep(&in[i])) all = FALSE;
        SPR_update();
        SYS_doVBlankProcess();
        if (all) break;
    }
    // Sin tiempo: a cada uno lo que le haya quedado.
    for (u8 i = 0; i < n; i++)
        for (s16 c = 0; c < HS_NAME; c++) hsLetter(&in[i], c, in[i].name[c], HS_WHITE);
    for (u16 t = 0; t < fps; t++) SYS_doVBlankProcess();
    hsSceneEnd();
}

SceneId showHiscoreEntry(void) {
    for (u8 k = 0; k < MAX_PLAYERS; k++) hsNew[k] = -1;
    // La SRAM se lee con el sonido callado (ver hsSramIO).
    XGM2_stop();
    hiscoreLoad();

    // Jugadores de esta partida, de mayor a menor puntaje.
    u8 order[MAX_PLAYERS], n = 0;
    u16 sc[MAX_PLAYERS];
    u8 chs[MAX_PLAYERS];
    for (u8 k = 0; k < MAX_PLAYERS; k++) {
        if (!playerFinalPeek(k, &sc[k], &chs[k])) continue;
        u8 i = n++;
        while (i > 0 && sc[order[i - 1]] < sc[k]) { order[i] = order[i - 1]; i--; }
        order[i] = k;
    }
    playerFinalClear();

    // (23/10) Los puestos se reservan ANTES de cargar, del mejor al peor: el
    // siguiente (con igual o menos puntaje) siempre queda abajo, asi que los
    // puestos ya reservados no se corren. Despues se cargan todos juntos.
    HsInput in[MAX_PLAYERS];
    u8 m = 0;
    static const char blank[HS_NAME] = { ' ', ' ', ' ' };
    for (u8 i = 0; i < n; i++) {
        u8 k = order[i];
        u8 rank = hsRankOf(sc[k]);
        if (rank >= HS_N || sc[k] == 0) continue;
        hsInsert(rank, blank, sc[k], chs[k]);
        in[m].k = k; in[m].ch = chs[k] & 3; in[m].rank = rank; in[m].score = sc[k];
        m++;
    }
    if (m) {
        // (20/10) Por ahora, "02 - Character Profiles" mientras se cargan
        // las iniciales (en loop: setLoopNumber(-1) antes del play).
        XGM2_setLoopNumber(-1);
        playMusicVol(music_profiles, 80);
        hsEntryAll(in, m);
        for (u8 i = 0; i < m; i++) {
            HsEntry* e = &hsTable[in[i].rank];
            e->name[0] = in[i].name[0];
            e->name[1] = in[i].name[1];
            e->name[2] = in[i].name[2];
            hsNew[in[i].k] = (s8)in[i].rank;
        }
        XGM2_stop();
        hsSramSave();
    }
    return SCENE_HISCORE_BOARD;
}

// ---------------------------------------------------------------------------
// BEST 10 PLAYERS
// ---------------------------------------------------------------------------
#define HS_BOARD_SECS   12
#define HS_PORT_GAP    130        // de un retrato al siguiente (px)
#define HS_PORT_Y       48

static void hsBoardRow(u8 i, bool show) {
    static const u8 rowColor[HS_N] = { HS_CYAN, HS_YELLOW, HS_MAGENTA, HS_WHITE, HS_WHITE,
                                       HS_WHITE, HS_WHITE, HS_WHITE, HS_WHITE, HS_WHITE };
    u16 y = (u16)(4 + i * 2);
    u8 c = rowColor[i];
    if (!show) {
        VDP_clearTileMapRect(BG_A, 3, y, 33, 2);
        return;
    }
    const char* r = hsRankStr[i];
    u16 len = (r[3]) ? 4 : 3;
    hsText((u16)(8 - len), y, r, c);
    hsPutChar(12, y, hsTable[i].name[0], c);
    hsPutChar(14, y, hsTable[i].name[1], c);
    hsPutChar(16, y, hsTable[i].name[2], c);
    hsNumber(28, y, hsTable[i].score, c, 2);
    hsText(30, y, "PTS", c);
}

bool hiscoreBoardAttract = FALSE;

// (22/10) Botones que cortan la tabla en modo atracto (como los perfiles).
#define HS_ATTRACT_BUTTONS (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C | \
                            BUTTON_X | BUTTON_Y | BUTTON_Z | BUTTON_MODE)

SceneId showHiscoreBoard(void) {
    bool attract = hiscoreBoardAttract;   // (22/10) viene de los perfiles
    hiscoreBoardAttract = FALSE;
    hiscoreLoad();
    hsSceneBegin();
    hsLoadGfx(hsGradBoardTiles);
    hsText(12, 1, "BEST 10 PLAYERS", HS_LAV);
    for (u8 i = 0; i < HS_N; i++) hsBoardRow(i, TRUE);

    // Tres retratos que se reciclan (cada uno con su linea de paleta, que se
    // recarga con el tono de la tortuga que le toca al volver a entrar).
    Sprite* port[3];
    s16 px[3];
    u8  pch[3];
    u16 pal[3][16];
    for (u8 j = 0; j < 3; j++) {
        px[j] = (s16)(40 + j * HS_PORT_GAP);
        pch[j] = j;
        memcpy(pal[j], hsPalTint[j], 32);
        port[j] = hsPortrait(j, px[j], HS_PORT_Y, (u16)(PAL1 + j));
    }
    u8 nextCh = 3;
    SPR_update();
    hsFadeIn(hsPalBoard, pal[0], pal[1], pal[2]);
    if (!XGM2_isPlaying()) playMusicVol(music_charselect, 80);

    u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    u16 timer = (u16)(HS_BOARD_SECS * fps);
    u16 prev = 0xFFFF, t = 0;
    while (timer > 0) {
        timer--;
        t++;
        u16 v = JOY_readJoypad(JOY_1);
        u16 cut = BUTTON_START;
        if (attract) {                    // (22/10) cualquier boton, los dos joysticks
            v |= JOY_readJoypad(JOY_2);
            cut = HS_ATTRACT_BUTTONS;
        }
        if ((v & ~prev & cut) && prev != 0xFFFF) break;
        prev = v;

        for (u8 j = 0; j < 3; j++) {
            px[j]--;
            if (px[j] < -64) {
                px[j] = (s16)(px[(j + 2) % 3] + HS_PORT_GAP);
                pch[j] = nextCh;
                nextCh = (u8)((nextCh + 1) & 3);
                SPR_setFrame(port[j], pch[j]);
                PAL_setPalette((u16)(PAL1 + j), hsPalTint[pch[j]], DMA);
            }
            SPR_setPosition(port[j], px[j], HS_PORT_Y);
        }
        // Los que acaban de entrar titilan.
        if ((t & 15) == 0) {
            for (u8 k = 0; k < MAX_PLAYERS; k++)
                if (hsNew[k] >= 0) hsBoardRow((u8)hsNew[k], (t & 16) != 0);
        }
        SPR_update();
        SYS_doVBlankProcess();
    }
    for (u8 k = 0; k < MAX_PLAYERS; k++) hsNew[k] = -1;
    XGM2_stop();
    hsSceneEnd();
    if (attract) {
        // Que el mismo pulso no confirme una opcion del menu al que volvemos.
        while ((JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2)) & HS_ATTRACT_BUTTONS)
            SYS_doVBlankProcess();
        return SCENE_PLAYER_SELECT;
    }
    return SCENE_SEGA;
}
