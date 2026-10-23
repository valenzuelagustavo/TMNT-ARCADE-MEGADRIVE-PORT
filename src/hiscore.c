// ============================================================================
// hiscore.c -- carga de iniciales y "BEST 10 PLAYERS" (20/10)
// ============================================================================
// Como en el arcade: al terminar la partida (game over, o despues de los
// creditos del final) cada jugador que entra en el top 10 pone 3 letras en una
// pantalla con SU retrato del selector a color dentro de un marco (con "1UP",
// su puntaje y el puesto) y las otras tres tortugas en negro, sin el fondo
// plano de color. Despues se ve la tabla: titulo, puesto, iniciales y puntos,
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
#define HS_VERSION     1

typedef struct { char name[HS_NAME]; u16 score; u8 ch; } HsEntry;

static HsEntry hsTable[HS_N];
static bool    hsLoaded = FALSE;
static s8      hsNew[MAX_PLAYERS] = { -1, -1, -1, -1 };   // puesto recien cargado por cada jugador

// Tabla de fabrica: los personajes del juego.
static const HsEntry hsDefault[HS_N] = {
    { {'L','E','O'}, 100, 0 }, { {'M','I','K'},  90, 1 }, { {'D','O','N'},  80, 2 },
    { {'R','A','P'},  70, 3 }, { {'A','P','R'},  60, 0 }, { {'S','P','L'},  50, 1 },
    { {'C','A','S'},  40, 2 }, { {'B','E','B'},  30, 3 }, { {'R','O','C'},  20, 0 },
    { {'S','H','R'},  10, 1 },
};

// ---------------------------------------------------------------------------
// SRAM
// ---------------------------------------------------------------------------
static u8 hsBuf[HS_SRAM_SIZE];

// Copia SRAM <-> hsBuf. Sin datos constantes adentro (la ROM alta no se ve).
static void hsSramIO(bool write) {
    SYS_disableInts();
    Z80_requestBus(TRUE);
    SRAM_enable();
    for (u16 i = 0; i < HS_SRAM_SIZE; i++) {
        if (write) SRAM_writeByte(i, hsBuf[i]);
        else       hsBuf[i] = SRAM_readByte(i);
    }
    SRAM_disable();
    Z80_releaseBus();
    SYS_enableInts();
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

static void hsEntryOne(u8 k, u16 score, u8 ch, u8 rank) {
    hsSceneBegin();
    hsLoadGfx(hsGradEntryTiles);

    // Marco y textos (cols 2..10). "nUP" y el puntaje arriba, el puesto, el
    // retrato y las 3 letras abajo.
    hsFrame(2, 1, 10, 27);
    char up[4] = { (char)('1' + k), 'U', 'P', 0 };
    hsText(3, 2, up, HS_LAV);
    hsNumber(9, 2, score, HS_WHITE, 1);
    hsText(4, 5, hsRankStr[rank], HS_WHITE);

    Sprite* me = hsPortrait(ch, 20, 64, PAL1);
    Sprite* sil[3];
    u8 n = 0;
    for (u8 t = 0; t < 4; t++) {
        if (t == ch) continue;
        sil[n] = hsPortrait(t, (s16)(96 + 72 * n), 64, PAL2);
        n++;
    }
    SPR_update();
    // PAL1: los colores del selector (los de la hoja del sprite).
    hsFadeIn(hsPalEntry, hs_portraits.palette->data, hsPalBlack, hsPalBlack);

    char name[HS_NAME] = { 'A', 'A', 'A' };
    s16 cur = 0, sel = 0;
    u16 joy = playerJoy(k), prev = 0xFFFF;
    u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    u16 timer = (u16)(HS_ENTRY_SECS * fps);
    u16 rep = 0, blink = 0;
    while (cur < HS_NAME && timer > 0) {
        u16 v = JOY_readJoypad(joy);
        u16 down = v & ~prev;
        if (prev == 0xFFFF) down = 0;     // lo que venia apretado no cuenta
        prev = v;
        timer--;
        blink++;

        s16 d = 0;
        if (down & BUTTON_UP)   { d = 1;  rep = 0; }
        if (down & BUTTON_DOWN) { d = -1; rep = 0; }
        if (v & (BUTTON_UP | BUTTON_DOWN)) {
            if (++rep > 18 && (rep & 3) == 0) d = (v & BUTTON_UP) ? 1 : -1;
        }
        if (d) {
            sel = (s16)((sel + d + HS_ALPHA_N) % HS_ALPHA_N);
            name[cur] = hsAlphabet[sel];
            blink = 0;
        }
        if ((down & BUTTON_LEFT) && cur > 0) {
            hsPutChar((u16)(4 + cur * 2), 24, name[cur], HS_WHITE);
            cur--;
            for (s16 i = 0; i < HS_ALPHA_N; i++) if (hsAlphabet[i] == name[cur]) sel = i;
        }
        if (down & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START | BUTTON_RIGHT)) {
            hsPutChar((u16)(4 + cur * 2), 24, name[cur], HS_WHITE);
            cur++;
            if (cur < HS_NAME) {
                name[cur] = name[cur - 1];      // arranca en la misma letra
                for (s16 i = 0; i < HS_ALPHA_N; i++) if (hsAlphabet[i] == name[cur]) sel = i;
            }
            blink = 0;
        }
        for (s16 i = 0; i < HS_NAME; i++) {
            if (i == cur) hsPutChar((u16)(4 + i * 2), 24, (blink & 16) ? ' ' : name[i], HS_LAV);
            else if (i > cur) hsPutChar((u16)(4 + i * 2), 24, name[i], HS_WHITE);
        }
        SPR_update();
        SYS_doVBlankProcess();
    }
    // Sin tiempo: lo que haya quedado.
    for (s16 i = 0; i < HS_NAME; i++) hsPutChar((u16)(4 + i * 2), 24, name[i], HS_WHITE);
    for (u16 t = 0; t < fps; t++) SYS_doVBlankProcess();

    hsInsert(rank, name, score, ch);
    hsNew[k] = (s8)rank;
    for (u8 j = 0; j < MAX_PLAYERS; j++)            // los de otros jugadores bajan
        if (j != k && hsNew[j] >= (s8)rank && hsNew[j] >= 0) hsNew[j]++;
    (void)me; (void)sil;
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

    bool any = FALSE;
    for (u8 i = 0; i < n; i++) {
        u8 k = order[i];
        u8 rank = hsRankOf(sc[k]);
        if (rank >= HS_N || sc[k] == 0) continue;
        // (20/10) Por ahora, "02 - Character Profiles" mientras se cargan
        // las iniciales (en loop: setLoopNumber(-1) antes del play).
        if (!any) { XGM2_setLoopNumber(-1); playMusicVol(music_profiles, 80); }
        any = TRUE;
        hsEntryOne(k, sc[k], chs[k], rank);
    }
    if (any) {
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

SceneId showHiscoreBoard(void) {
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
        if ((v & ~prev & BUTTON_START) && prev != 0xFFFF) break;
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
    return SCENE_SEGA;
}
