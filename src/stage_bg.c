#include "stage_bg.h"

// ===========================================================================
// STAGE_BG — ver stage_bg.h
// ===========================================================================

#define SBG_MAX_TILES   2048          // tope del tileset de ROM (indice de 11 bits)
#define SBG_MAX_SLOTS    768
#define SBG_NONE      0xFFFF

static const Image* sbgImg;
static const u16*   sbgMap;
static u16          sbgMapW, sbgMapH;
static VDPPlane     sbgPlane;
static u16          sbgPal;
static u16          sbgVram;
static u16          sbgSlots;

static u16 tileSlot[SBG_MAX_TILES];   // tile del tileset -> slot (o NONE)
static u16 slotTile[SBG_MAX_SLOTS];   // slot -> tile del tileset
static u16 slotRef[SBG_MAX_SLOTS];    // celdas dibujadas que usan el slot (u16: un
                                      // tile liso puede estar en cientos de celdas)
static u16 freeStack[SBG_MAX_SLOTS];
static u16 freeTop;
static u16 used;

static s16 colLeft, colRight;         // ventana de columnas de MUNDO dibujadas
static TransferMethod upTm;           // DMA en el init (pantalla en negro), CPU despues

static u16 colBuf[32];

static u16 slotAcquire(u16 tile) {
    u16 s = tileSlot[tile];
    if (s != SBG_NONE) {
        slotRef[s]++;
        return s;
    }
    if (freeTop == 0) return SBG_NONE;          // sin lugar: se dibuja negro
    s = freeStack[--freeTop];
    tileSlot[tile] = s;
    slotTile[s]    = tile;
    slotRef[s]     = 1;
    used++;
    // Subida del tile. En el juego va por CPU: una columna nueva trae a lo
    // sumo 28 tiles y meterlos en la cola de DMA la desbordaria (el motor de
    // sprites tambien la usa). Escribir VRAM en pleno cuadro es legal en el
    // Mega Drive, solo mas lento.
    VDP_loadTileData(sbgImg->tileset->tiles + (u32)tile * 8, sbgVram + s, 1, upTm);
    return s;
}

static void slotRelease(u16 tile) {
    u16 s = tileSlot[tile];
    if (s == SBG_NONE) return;
    if (slotRef[s] > 1) { slotRef[s]--; return; }
    slotRef[s]     = 0;
    tileSlot[tile] = SBG_NONE;
    freeStack[freeTop++] = s;
    used--;
}

static void colLoad(s16 col) {
    u16 rows = (sbgMapH < 32) ? sbgMapH : 32;
    for (u16 r = 0; r < rows; r++) {
        u16 v = 0;
        if (col >= 0 && col < (s16)sbgMapW) {
            u16 e    = sbgMap[(u32)r * sbgMapW + (u16)col];
            u16 tile = e & TILE_INDEX_MASK;
            u16 s    = (tile < SBG_MAX_TILES) ? slotAcquire(tile) : SBG_NONE;
            if (s != SBG_NONE)
                v = (u16)((e & (TILE_ATTR_VFLIP_MASK | TILE_ATTR_HFLIP_MASK))
                          | TILE_ATTR(sbgPal, FALSE, FALSE, FALSE) | (sbgVram + s));
        }
        colBuf[r] = v;
    }
    VDP_setTileMapDataRect(sbgPlane, colBuf, (u16)(col & 63), 0, 1, rows, 1, CPU);
}

static void colUnload(s16 col) {
    if (col < 0 || col >= (s16)sbgMapW) return;
    u16 rows = (sbgMapH < 32) ? sbgMapH : 32;
    for (u16 r = 0; r < rows; r++) {
        u16 tile = sbgMap[(u32)r * sbgMapW + (u16)col] & TILE_INDEX_MASK;
        if (tile < SBG_MAX_TILES) slotRelease(tile);
    }
}

void sbgInit(const Image* img, VDPPlane plane, u16 pal, u16 vramBase,
             u16 slots, s16 camX) {
    sbgImg   = img;
    sbgMap   = img->tilemap->tilemap;
    sbgMapW  = img->tilemap->w;
    sbgMapH  = img->tilemap->h;
    sbgPlane = plane;
    sbgPal   = pal;
    sbgVram  = vramBase;
    sbgSlots = (slots > SBG_MAX_SLOTS) ? SBG_MAX_SLOTS : slots;

    for (u16 i = 0; i < SBG_MAX_TILES; i++) tileSlot[i] = SBG_NONE;
    freeTop = 0;
    for (u16 s = sbgSlots; s > 0; s--) {        // slot 0 queda arriba del stack
        slotRef[s - 1] = 0;
        freeStack[freeTop++] = (u16)(s - 1);
    }
    used = 0;

    upTm     = DMA;
    colLeft  = (s16)(camX >> 3);
    colRight = (s16)(colLeft + SBG_WIN_COLS - 1);
    for (s16 c = colLeft; c <= colRight; c++) colLoad(c);
    upTm     = CPU;
}

void sbgUpdate(s16 camX) {
    s16 wantL = (s16)(camX >> 3);
    s16 wantR = (s16)(wantL + SBG_WIN_COLS - 1);
    // Primero soltar lo que salio (asi el pico nunca pasa de SBG_WIN_COLS
    // columnas), despues traer lo que entro.
    while (colLeft < wantL)  colUnload(colLeft++);
    while (colRight > wantR) colUnload(colRight--);
    while (colRight < wantR) colLoad(++colRight);
    while (colLeft > wantL)  colLoad(--colLeft);
}

u16 sbgUsed(void) { return used; }
