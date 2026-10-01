#include "stage_bg.h"

// ===========================================================================
// STAGE_BG — ver stage_bg.h
// ===========================================================================

#define SBG_MAX_TILES   3072          // tope del tileset de ROM (formato ancho: 12 bits)
// (28/09) 768 -> 832: la cloaca nueva pide 800 (peor caso 796). Con 768 el
// cache se llenaba y las celdas sin lugar salian NEGRAS (slotAcquire), que
// eran los bloques negros del sewer. OJO: bgSlots de un nivel por encima de
// este tope se recorta en silencio.
// (29/09) 832 -> 896: con la camara vertical la cloaca dibuja 32 filas y su
// peor caso sube a 861 (bgSlots 864).
#define SBG_MAX_SLOTS    896
#define SBG_NONE      0xFFFF

static const u32*   sbgTiles;
static bool         sbgWide;          // mapa en formato ancho (ver SbgRaw)
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

// Regiones variables (ver SbgRegion en stage_bg.h).
static const SbgRegion* sbgRegs;
static u16 sbgNRegs;
static u16 sbgRegVar[SBG_MAX_REGIONS];

void sbgSetRegions(const SbgRegion* regs, u16 n) {
    sbgRegs  = regs;
    sbgNRegs = (n > SBG_MAX_REGIONS) ? SBG_MAX_REGIONS : n;
    for (u16 i = 0; i < SBG_MAX_REGIONS; i++) sbgRegVar[i] = 0;
}

// Entrada del mapa de la celda (col, row) con las variantes puestas.
static u16 cellEntry(u16 col, u16 row) {
    for (u16 i = 0; i < sbgNRegs; i++) {
        const SbgRegion* g = &sbgRegs[i];
        u16 v = sbgRegVar[i];
        if (!v) continue;
        if (col < g->c0 || col >= g->c0 + g->w || row < g->r0 || row >= g->r0 + g->h)
            continue;
        return g->var[(u32)(v - 1) * g->w * g->h + (u32)(row - g->r0) * g->w + (col - g->c0)];
    }
    return sbgMap[(u32)row * sbgMapW + col];
}

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
    VDP_loadTileData(sbgTiles + (u32)tile * 8, sbgVram + s, 1, upTm);
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

// Indice y flips de una celda del mapa, en los dos formatos.
static inline u16 cellTile(u16 e) { return sbgWide ? (u16)(e & SBG_RAW_INDEX_MASK)
                                                   : (u16)(e & TILE_INDEX_MASK); }
static inline u16 cellFlip(u16 e) {
    return sbgWide ? (u16)((e >> SBG_RAW_FLIP_SHIFT) & (TILE_ATTR_VFLIP_MASK | TILE_ATTR_HFLIP_MASK))
                   : (u16)(e & (TILE_ATTR_VFLIP_MASK | TILE_ATTR_HFLIP_MASK));
}

static void colLoad(s16 col) {
    u16 rows = (sbgMapH < 32) ? sbgMapH : 32;
    for (u16 r = 0; r < rows; r++) {
        u16 v = 0;
        if (col >= 0 && col < (s16)sbgMapW) {
            u16 e    = cellEntry((u16)col, r);
            u16 tile = cellTile(e);
            u16 s    = (tile < SBG_MAX_TILES) ? slotAcquire(tile) : SBG_NONE;
            if (s != SBG_NONE)
                v = (u16)(cellFlip(e)
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
        u16 tile = cellTile(cellEntry((u16)col, r));
        if (tile < SBG_MAX_TILES) slotRelease(tile);
    }
}

void sbgSetVariant(u16 reg, u16 v) {
    if (reg >= sbgNRegs || sbgRegVar[reg] == v || v > sbgRegs[reg].nVar) return;
    const SbgRegion* g = &sbgRegs[reg];
    s16 cA = (s16)g->c0, cB = (s16)(g->c0 + g->w - 1);
    if (cA < colLeft)  cA = colLeft;
    if (cB > colRight) cB = colRight;
    // Primero se suelta la vieja (asi el pico del cache no suma las dos)...
    for (s16 c = cA; c <= cB; c++)
        for (u16 r = g->r0; r < g->r0 + g->h; r++) {
            u16 tile = cellTile(cellEntry((u16)c, r));
            if (tile < SBG_MAX_TILES) slotRelease(tile);
        }
    sbgRegVar[reg] = v;
    // ...y despues se pide y se dibuja la nueva.
    for (s16 c = cA; c <= cB; c++)
        for (u16 r = g->r0; r < g->r0 + g->h; r++) {
            u16 e    = cellEntry((u16)c, r);
            u16 tile = cellTile(e);
            u16 s    = (tile < SBG_MAX_TILES) ? slotAcquire(tile) : SBG_NONE;
            u16 val  = 0;
            if (s != SBG_NONE)
                val = (u16)(cellFlip(e) | TILE_ATTR(sbgPal, FALSE, FALSE, FALSE) | (sbgVram + s));
            VDP_setTileMapXY(sbgPlane, val, (u16)(c & 63), r);
        }
}

static void sbgStart(VDPPlane plane, u16 pal, u16 vramBase, u16 slots, s16 camX);

void sbgInit(const Image* img, VDPPlane plane, u16 pal, u16 vramBase,
             u16 slots, s16 camX) {
    sbgTiles = img->tileset->tiles;
    sbgWide  = FALSE;
    sbgMap   = img->tilemap->tilemap;
    sbgMapW  = img->tilemap->w;
    sbgMapH  = img->tilemap->h;
    sbgStart(plane, pal, vramBase, slots, camX);
}

void sbgInitRaw(const SbgRaw* raw, VDPPlane plane, u16 pal, u16 vramBase,
                u16 slots, s16 camX) {
    sbgTiles = raw->tiles;
    sbgWide  = TRUE;
    sbgMap   = raw->map;
    sbgMapW  = raw->w;
    sbgMapH  = raw->h;
    sbgStart(plane, pal, vramBase, slots, camX);
}

static void sbgStart(VDPPlane plane, u16 pal, u16 vramBase, u16 slots, s16 camX) {
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

// ---------------------------------------------------------------------------
// 2D (27/09, Technodrome): ventana de SBG_WIN_COLS x SBG_WIN_ROWS celdas que
// sigue a la camara en X e Y. El plano es el circular de 64x32: la celda de
// mundo (c, r) va en (c & 63, r & 31). Una columna nueva trae las filas de la
// ventana y una fila nueva las columnas; se sueltan igual. El indice 0 del
// mapa es el VACIO: no ocupa slot y se dibuja con el tile 0 (transparente).
// ---------------------------------------------------------------------------
static s16 rowTop, rowBot;            // ventana de filas de MUNDO dibujadas

static u16 cellAttr2(s16 col, s16 row) {
    if (col < 0 || row < 0 || col >= (s16)sbgMapW || row >= (s16)sbgMapH) return 0;
    u16 e    = sbgMap[(u32)row * sbgMapW + (u16)col];
    u16 tile = cellTile(e);
    if (tile == 0 || tile >= SBG_MAX_TILES) return 0;
    u16 s = slotAcquire(tile);
    if (s == SBG_NONE) return 0;
    return (u16)(cellFlip(e) | TILE_ATTR(sbgPal, FALSE, FALSE, FALSE) | (sbgVram + s));
}

static void cellDrop2(s16 col, s16 row) {
    if (col < 0 || row < 0 || col >= (s16)sbgMapW || row >= (s16)sbgMapH) return;
    u16 tile = cellTile(sbgMap[(u32)row * sbgMapW + (u16)col]);
    if (tile == 0 || tile >= SBG_MAX_TILES) return;
    slotRelease(tile);
}

static void colLoad2(s16 col) {
    for (s16 r = rowTop; r <= rowBot; r++)
        VDP_setTileMapXY(sbgPlane, cellAttr2(col, r), (u16)(col & 63), (u16)(r & 31));
}
static void colUnload2(s16 col) {
    for (s16 r = rowTop; r <= rowBot; r++) cellDrop2(col, r);
}
static void rowLoad2(s16 row) {
    for (s16 c = colLeft; c <= colRight; c++)
        VDP_setTileMapXY(sbgPlane, cellAttr2(c, row), (u16)(c & 63), (u16)(row & 31));
}
static void rowUnload2(s16 row) {
    for (s16 c = colLeft; c <= colRight; c++) cellDrop2(c, row);
}

void sbgInitRaw2D(const SbgRaw* raw, VDPPlane plane, u16 pal, u16 vramBase,
                  u16 slots, s16 camX, s16 camY) {
    sbgTiles = raw->tiles;
    sbgWide  = TRUE;
    sbgMap   = raw->map;
    sbgMapW  = raw->w;
    sbgMapH  = raw->h;
    sbgPlane = plane;
    sbgPal   = pal;
    sbgVram  = vramBase;
    sbgSlots = (slots > SBG_MAX_SLOTS) ? SBG_MAX_SLOTS : slots;

    for (u16 i = 0; i < SBG_MAX_TILES; i++) tileSlot[i] = SBG_NONE;
    freeTop = 0;
    for (u16 s = sbgSlots; s > 0; s--) {
        slotRef[s - 1] = 0;
        freeStack[freeTop++] = (u16)(s - 1);
    }
    used = 0;

    upTm     = DMA;
    colLeft  = (s16)(camX >> 3);
    colRight = (s16)(colLeft + SBG_WIN_COLS - 1);
    rowTop   = (s16)(camY >> 3);
    rowBot   = (s16)(rowTop + SBG_WIN_ROWS - 1);
    for (s16 c = colLeft; c <= colRight; c++) colLoad2(c);
    upTm     = CPU;
}

void sbgUpdate2D(s16 camX, s16 camY) {
    s16 wantL = (s16)(camX >> 3);
    s16 wantR = (s16)(wantL + SBG_WIN_COLS - 1);
    s16 wantT = (s16)(camY >> 3);
    s16 wantB = (s16)(wantT + SBG_WIN_ROWS - 1);
    // Primero soltar lo que salio, despues traer lo que entro (el pico nunca
    // pasa de la ventana). Las columnas usan las filas actuales y viceversa.
    while (colLeft < wantL)  colUnload2(colLeft++);
    while (colRight > wantR) colUnload2(colRight--);
    while (rowTop < wantT)   rowUnload2(rowTop++);
    while (rowBot > wantB)   rowUnload2(rowBot--);
    while (colRight < wantR) colLoad2(++colRight);
    while (colLeft > wantL)  colLoad2(--colLeft);
    while (rowBot < wantB)   rowLoad2(++rowBot);
    while (rowTop > wantT)   rowLoad2(--rowTop);
}
