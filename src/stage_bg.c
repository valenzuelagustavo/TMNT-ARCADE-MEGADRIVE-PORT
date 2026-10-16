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
static bool         sbgWrapOn;        // (12/10) la imagen se repite en X (patinetas)
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

static void slotsReset(void) {
    for (u16 i = 0; i < SBG_MAX_TILES; i++) tileSlot[i] = SBG_NONE;
    freeTop = 0;
    for (u16 s = sbgSlots; s > 0; s--) {        // slot 0 queda arriba del stack
        slotRef[s - 1] = 0;
        freeStack[freeTop++] = (u16)(s - 1);
    }
    used = 0;
}

static s16 colLeft, colRight;         // ventana de columnas de MUNDO dibujadas
static TransferMethod upTm;           // DMA en el init (pantalla en negro), CPU despues

static u16 colBuf[32];
#define SBG_QUEUE_SPARE  24     // entradas de la cola que se dejan para el resto (sprites, scroll...)
#define SBG_BYTES_SPARE 2048    // bytes por cuadro que se dejan para el resto

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

// (14/10) Subida de un tile por CPU, directa al puerto del VDP. Por la cola
// de DMA cada tile cuesta una entrada y ~3,5 lineas de CPU solo en armarla
// (DMA_transfer + DMA_queueDmaFast); asi son ~2 lineas, aun con el VDP
// frenando las escrituras en pleno cuadro, y la cola queda para los sprites.
// El slot nuevo todavia no esta en ninguna celda: escribirlo ahora no se ve.
static void tileCpu(u16 tile, u16 slot) {
    const u32* src = sbgTiles + ((u32)tile << 3);
    vu32* const dp = (vu32*)VDP_DATA_PORT;
    VDP_setAutoInc(2);                  // la cola puede haberlo dejado en 128
    *(vu32*)VDP_CTRL_PORT = VDP_WRITE_VRAM_ADDR((u32)(sbgVram + slot) << 5);
    *dp = src[0]; *dp = src[1]; *dp = src[2]; *dp = src[3];
    *dp = src[4]; *dp = src[5]; *dp = src[6]; *dp = src[7];
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
    // (10/10) En el 2D (Technodrome) va por la COLA de DMA: escribir VRAM por
    // CPU en pleno cuadro es legal pero el VDP solo acepta unas pocas
    // escrituras por linea, y bajando en el ascensor entran ~70 tiles cada
    // dos cuadros -- el juego andaba a media velocidad. Si la cola esta por
    // llenarse (o el cuadro ya no tiene banda), se sigue por CPU.
    TransferMethod tm = upTm;
    if (tm == DMA_QUEUE &&
        (DMA_getQueueSize() + SBG_QUEUE_SPARE >= DMA_getMaxQueueSize() ||
         DMA_getQueueTransferSize() + SBG_BYTES_SPARE >= DMA_getMaxTransferSize()))
        tm = CPU;
    if (tm == CPU) tileCpu(tile, s);
    else           VDP_loadTileData(sbgTiles + (u32)tile * 8, sbgVram + s, 1, tm);
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

// (12/10) Columna de la IMAGEN para la columna de mundo col (con la imagen
// repetida en X, sbgSetWrap). -1 = fuera.
static inline s16 imgCol(s16 col) {
    if (col < 0) return -1;
    if (sbgWrapOn) return (s16)((u16)col % sbgMapW);
    return (col < (s16)sbgMapW) ? col : -1;
}

void sbgSetWrap(bool on) { sbgWrapOn = on; }

// (14/10) Columnas mas baratas: sin regiones variables (casi todos los
// niveles) la entrada del mapa sale de un puntero que baja de a una fila, en
// vez de cellEntry() por celda (una multiplicacion de 32 bits = llamada a
// __mulsi3 en el 68000, mas el recorrido de las regiones). Si el tile ya esta
// en VRAM (lo comun) se resuelve ahi mismo, sin llamar a slotAcquire, y los
// datos del formato (mascara, corrimiento del flip, atributo base) se sacan
// una sola vez por columna. La columna del plano va por la cola de DMA desde
// un anillo de buffers (sin copiarla); antes iba por CPU en pleno cuadro.
// Con la ruta de la patineta a 7 px/f entra y sale casi una columna por
// cuadro y esto se comia ~30 % del cuadro: el nivel andaba a ~42 fps.
#define SBG_RING 4
static u16 colRing[SBG_RING][32];
static u16 ringUsed;                  // buffers del anillo usados este cuadro

static void colLoad(s16 col) {
    u16 rows = (sbgMapH < 32) ? sbgMapH : 32;
    s16 ic = imgCol(col);
    TransferMethod tm;
    u16* buf;
    if (upTm == DMA)               { buf = colBuf; tm = CPU; }
    else if (ringUsed < SBG_RING)  { buf = colRing[ringUsed++]; tm = DMA_QUEUE; }
    else                           { buf = colBuf; tm = DMA_QUEUE_COPY; }

    if (ic < 0) {
        for (u16 r = 0; r < rows; r++) buf[r] = 0;
    } else if (sbgNRegs == 0) {
        // Bucle caliente: indices como desplazamientos s16 en bytes para que
        // el 68000 use el modo (An,Dn.w) sin extender a 32 bits en cada celda.
        const u16* m     = sbgMap + (u16)ic;
        const s16  stepB = (s16)(sbgMapW << 1);
        const u16  imask = sbgWide ? SBG_RAW_INDEX_MASK : TILE_INDEX_MASK;
        const u16  fsh   = sbgWide ? SBG_RAW_FLIP_SHIFT : 0;
        const u16  base  = (u16)(TILE_ATTR(sbgPal, FALSE, FALSE, FALSE) + sbgVram);
        u8* const  tsB   = (u8*)tileSlot;
        u8* const  refB  = (u8*)slotRef;
        u16* d = buf;
        for (u16 r = rows; r; r--) {
            u16 e = *m;
            m = (const u16*)((const u8*)m + stepB);
            u16 tile = e & imask;
            u16 v    = 0;
            if (tile < SBG_MAX_TILES) {
                u16 s = *(u16*)(tsB + (s16)(tile << 1));
                if (s != SBG_NONE) (*(u16*)(refB + (s16)(s << 1)))++;
                else               s = slotAcquire(tile);
                if (s != SBG_NONE)
                    v = (u16)(((e >> fsh) & (TILE_ATTR_VFLIP_MASK | TILE_ATTR_HFLIP_MASK))
                              | (base + s));
            }
            *d++ = v;
        }
    } else {
        for (u16 r = 0; r < rows; r++) {
            u16 e    = cellEntry((u16)ic, r);
            u16 tile = cellTile(e);
            u16 s    = (tile < SBG_MAX_TILES) ? slotAcquire(tile) : SBG_NONE;
            buf[r] = (s != SBG_NONE)
                   ? (u16)(cellFlip(e) | TILE_ATTR(sbgPal, FALSE, FALSE, FALSE) | (sbgVram + s))
                   : 0;
        }
    }
    VDP_setTileMapDataRect(sbgPlane, buf, (u16)(col & 63), 0, 1, rows, 1, tm);
}

static void colUnload(s16 col) {
    s16 ic = imgCol(col);
    if (ic < 0) return;
    u16 rows = (sbgMapH < 32) ? sbgMapH : 32;
    if (sbgNRegs) {
        for (u16 r = 0; r < rows; r++) {
            u16 tile = cellTile(cellEntry((u16)ic, r));
            if (tile < SBG_MAX_TILES) slotRelease(tile);
        }
        return;
    }
    // (14/10) slotRelease en linea, con desplazamientos s16 (ver colLoad).
    const u16* m     = sbgMap + (u16)ic;
    const s16  stepB = (s16)(sbgMapW << 1);
    const u16  imask = sbgWide ? SBG_RAW_INDEX_MASK : TILE_INDEX_MASK;
    u8* const  tsB   = (u8*)tileSlot;
    u8* const  refB  = (u8*)slotRef;
    for (u16 r = rows; r; r--) {
        u16 tile = *m & imask;
        m = (const u16*)((const u8*)m + stepB);
        if (tile >= SBG_MAX_TILES) continue;
        u16* ts = (u16*)(tsB + (s16)(tile << 1));
        u16  s  = *ts;
        if (s == SBG_NONE) continue;
        u16* rp = (u16*)(refB + (s16)(s << 1));
        if (*rp > 1) { (*rp)--; continue; }
        *rp = 0;
        *ts = SBG_NONE;
        freeStack[freeTop++] = s;
        used--;
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

    slotsReset();

    upTm     = DMA;
    colLeft  = (s16)(camX >> 3);
    colRight = (s16)(colLeft + SBG_WIN_COLS - 1);
    for (s16 c = colLeft; c <= colRight; c++) colLoad(c);
    // (14/10) En el juego los tiles nuevos van directo por CPU (tileCpu) y
    // la columna del plano por la cola de DMA (colLoad).
    upTm     = CPU;
}

void sbgUpdate(s16 camX) {
    ringUsed = 0;                     // la cola se vacio en el vblank
    // (12/10) Con la imagen repetida el scroll es u16 (da la vuelta a los
    // 65536 px = 8192 columnas, multiplo de las 64 del plano).
    s16 wantL = sbgWrapOn ? (s16)((u16)camX >> 3) : (s16)(camX >> 3);
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

// (10/10) Reescrito para el viaje en ascensor, que trae una columna Y una
// fila cada dos cuadros (~140 celdas entre las que entran y las que salen).
// Antes cada celda calculaba su direccion en el mapa con una multiplicacion
// de 32 bits (en el 68000 es una llamada a __mulsi3) y escribia el plano con
// su propio VDP_setTileMapXY: la actualizacion se comia mas de un cuadro
// entero y el juego bajaba a la mitad de velocidad. Ahora se calcula UNA
// direccion por tira y se avanza con el puntero; el plano va en bloque por
// la cola de DMA.
static inline u16 attrOf(u16 e) {
    u16 tile = (u16)(e & SBG_RAW_INDEX_MASK);
    if (tile == 0 || tile >= SBG_MAX_TILES) return 0;
    u16 s = slotAcquire(tile);
    if (s == SBG_NONE) return 0;
    return (u16)(((e >> SBG_RAW_FLIP_SHIFT) & (TILE_ATTR_VFLIP_MASK | TILE_ATTR_HFLIP_MASK))
                 | TILE_ATTR(sbgPal, FALSE, FALSE, FALSE) | (sbgVram + s));
}

static inline void dropOf(u16 e) {
    u16 tile = (u16)(e & SBG_RAW_INDEX_MASK);
    if (tile == 0 || tile >= SBG_MAX_TILES) return;
    slotRelease(tile);
}

// Rango de filas de la ventana que caen dentro del mapa.
static inline bool rowsIn(s16* r0, s16* r1) {
    *r0 = (rowTop < 0) ? 0 : rowTop;
    *r1 = (rowBot >= (s16)sbgMapH) ? (s16)(sbgMapH - 1) : rowBot;
    return *r0 <= *r1;
}
static inline bool colsIn(s16* c0, s16* c1) {
    *c0 = (colLeft < 0) ? 0 : colLeft;
    *c1 = (colRight >= (s16)sbgMapW) ? (s16)(sbgMapW - 1) : colRight;
    return *c0 <= *c1;
}

static u16 lineBuf[64];

static void colLoad2(s16 col) {
    u16 n = 0;
    s16 r0 = 0, r1 = -1;
    bool inX = (col >= 0 && col < (s16)sbgMapW);
    const u16* m = NULL;
    if (inX && rowsIn(&r0, &r1)) m = sbgMap + (u32)(u16)r0 * sbgMapW + (u16)col;
    for (s16 r = rowTop; r <= rowBot; r++) {
        u16 v = 0;
        if (m && r >= r0 && r <= r1) { v = attrOf(*m); m += sbgMapW; }
        lineBuf[n++] = v;
    }
    u16 y0 = (u16)(rowTop & 31);
    u16 h1 = (u16)((n < 32 - y0) ? n : 32 - y0);
    VDP_setTileMapDataRect(sbgPlane, lineBuf, (u16)(col & 63), y0, 1, h1, 1, DMA_QUEUE_COPY);
    if (h1 < n)
        VDP_setTileMapDataRect(sbgPlane, lineBuf + h1, (u16)(col & 63), 0, 1,
                               (u16)(n - h1), 1, DMA_QUEUE_COPY);
}

static void colUnload2(s16 col) {
    s16 r0, r1;
    if (col < 0 || col >= (s16)sbgMapW || !rowsIn(&r0, &r1)) return;
    const u16* m = sbgMap + (u32)(u16)r0 * sbgMapW + (u16)col;
    for (s16 r = r0; r <= r1; r++) { dropOf(*m); m += sbgMapW; }
}

static void rowLoad2(s16 row) {
    u16 n = 0;
    s16 c0 = 0, c1 = -1;
    bool inY = (row >= 0 && row < (s16)sbgMapH);
    const u16* m = NULL;
    if (inY && colsIn(&c0, &c1)) m = sbgMap + (u32)(u16)row * sbgMapW + (u16)c0;
    for (s16 c = colLeft; c <= colRight; c++) {
        u16 v = 0;
        if (m && c >= c0 && c <= c1) v = attrOf(*m++);
        lineBuf[n++] = v;
    }
    u16 x0 = (u16)(colLeft & 63);
    u16 w1 = (u16)((n < 64 - x0) ? n : 64 - x0);
    VDP_setTileMapDataRow(sbgPlane, lineBuf, (u16)(row & 31), x0, w1, DMA_QUEUE_COPY);
    if (w1 < n)
        VDP_setTileMapDataRow(sbgPlane, lineBuf + w1, (u16)(row & 31), 0,
                              (u16)(n - w1), DMA_QUEUE_COPY);
}

static void rowUnload2(s16 row) {
    s16 c0, c1;
    if (row < 0 || row >= (s16)sbgMapH || !colsIn(&c0, &c1)) return;
    const u16* m = sbgMap + (u32)(u16)row * sbgMapW + (u16)c0;
    for (s16 c = c0; c <= c1; c++) dropOf(*m++);
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

    slotsReset();

    upTm     = DMA;
    colLeft  = (s16)(camX >> 3);
    colRight = (s16)(colLeft + SBG_WIN_COLS - 1);
    rowTop   = (s16)(camY >> 3);
    rowBot   = (s16)(rowTop + SBG_WIN_ROWS - 1);
    for (s16 c = colLeft; c <= colRight; c++) colLoad2(c);
    DMA_flushQueue();         // (10/10) colLoad2 encola: en el init se manda ya
    upTm     = DMA_QUEUE;     // (10/10) ver slotAcquire
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
