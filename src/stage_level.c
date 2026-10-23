// ===========================================================================
// stage_level.c — nivel generico de scroll horizontal. Ver stage_level.h.
// ===========================================================================
// Nacio como src/level3_1.c (la cloaca) y se partio en dos el 26/09 al llegar
// la Scene 4: aca queda el motor; en cada levelN_M.c, los datos y el jefe.
// ===========================================================================

#include <genesis.h>
#include "stage_level.h"
#include "idle_missile.h"   // (14/10) modo ARCADE: el misil al que se queda quieto
#include "level1.h"            // hud_font
#include "enemies.h"           // foot_soldier (paleta)
#include "hud.h"
#include "enemy.h"
#include "difficulty.h"      // (20/10) foot soldiers extra por oleada
#include "stage_bg.h"
#include "pause_menu.h"
#include "audio.h"
#include "boss_vo.h"           // (01/10) voz de entrada de los jefes

#ifndef IS_PAL_SYSTEM
#define IS_PAL_SYSTEM IS_PALSYSTEM
#endif

extern u8 cantidadJugadores;

#define SCREEN_W            320
#define STAGE_HUD_ROWS        4     // filas de BG_A que no scrollean (HUD)
#define STAGE_VRAM_WAIT      30     // (06/10) sin VRAM y sin nadie vivo: se cambia por un morado
#define STAGE_VRAM_SKIP     150     // ... y si ni el morado entra, se saltea
#define STAGE_ROWS           28
#define CAM_DEAD_ZONE_RIGHT 120
#define CAM_MAX_SPEED_X       4
#define FOOT_DX  (PLAYER_SPRITE_W / 2)
#define STAGE_MAX_ALIVE_1P    3     // presupuesto de VRAM de sprites (como el 2-1)
#define STAGE_MAX_ALIVE_2P    2
#define STAGE_START_X        40

// (20/10) Oleadas con foot soldiers EXTRA (difficulty.h): despues de las
// entradas de la tabla se repiten sus SOLDIERS (los actores especiales del
// nivel no) desde el primero.
__attribute__((noinline)) static u8 waveSoldiers(const StageWave* w) {
    u8 c = 0;
    for (u8 k = 0; k < w->n; k++) if (w->type[k] < STAGE_SPECIAL) c++;
    return c;
}
__attribute__((noinline)) static u8 waveEntry(const StageWave* w, u8 idx) {
    if (idx < w->n) return idx;
    u8 ns = waveSoldiers(w);
    if (!ns) return 0;
    u8 want = (u8)((idx - w->n) % ns);
    for (u8 k = 0; k < w->n; k++) {
        if (w->type[k] >= STAGE_SPECIAL) continue;
        if (want-- == 0) return k;
    }
    return 0;
}
// (18/10) Entrada cayendo despues de la intro (ver introStep): desde esta
// altura, escalonados, y derivando 1 px por frame a la derecha estos frames.
#define STAGE_DROP_Z        190
#define STAGE_DROP_STAGGER   36
#define STAGE_DROP_BACK      40     // arrancan esto mas a la izquierda...
#define STAGE_DROP_TICKS     40     // ...y derivan 1 px/f a la derecha
#define STAGE_CLEAR_SECS      5
#define VOL_MUSIC_CLEAR      80

static const StageLevel* cur;       // el nivel en curso

// (29/09) Camara vertical: ver camYMin en stage_level.h. stageCamY (player.h)
// es la que restan al dibujar todos los sprites del mundo.
#define STAGE_CAMY_FEET_TOP  128    // la tortuga mas alta, a esta Y de pantalla
#define STAGE_CAMY_FEET_BOT  216    // la mas baja, nunca por debajo de esta
#define STAGE_CAMY_SPEED       1    // px por frame
// (01/10) Tambien hacia ABAJO: camYMax > 0 (el garage tiene 16 px de piso
// debajo de la pantalla; la fila 0 de la imagen sigue siendo y = 0).
static inline bool camYOn(void) { return cur && (cur->camYMin < 0 || cur->camYMax > 0); }

// A donde quiere ir la camara: la tortuga mas ALTA (menor Y de pies) cerca de
// STAGE_CAMY_FEET_TOP en pantalla -- arriba de la vereda se ve el techo --,
// sin dejar a la mas BAJA por debajo de STAGE_CAMY_FEET_BOT.
static s16 camYTarget(Player** pls, u8 nPl) {
    s16 top = 0x7FFF, bot = -0x7FFF;
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k]) || !pls[k]->sprite) continue;
        s16 y = pls[k]->y;
        if (y < top) top = y;
        if (y > bot) bot = y;
    }
    if (top == 0x7FFF) return stageCamY;
    s16 t = (s16)(top - STAGE_CAMY_FEET_TOP);
    s16 need = (s16)(bot - STAGE_CAMY_FEET_BOT);
    if (t < need) t = need;
    if (t < cur->camYMin) t = cur->camYMin;
    if (t > cur->camYMax) t = cur->camYMax;
    return t;
}

// Scroll vertical de los dos planos: la fila de la imagen que queda arriba.
// (21/10) Corrimiento extra de BG_A (ver stage_level.h).
s16 stageAOffX, stageAOffY;

static void camYApply(bool now) {
    s16 vs = (s16)(stageCamY - cur->camYMin);
    s16 vsA = cur->far ? vs : (s16)((vs - stageAOffY) & 0xFF);   // (21/10) plano de 256 px
    if (now) {
        VDP_setVerticalScroll(BG_A, vsA);
        VDP_setVerticalScroll(BG_B, vs);
    } else {
        VDP_setVerticalScrollVSync(BG_A, vsA);
        VDP_setVerticalScrollVSync(BG_B, vs);
    }
    pauseSetVScroll(vsA);
}

// ---------------------------------------------------------------------------
// Caminable
// ---------------------------------------------------------------------------
// (01/10) Los foot soldiers del nivel en curso (MAX_ENEMIES), para los props
// del nivel que les pegan (conos y barriles del garage).
static Enemy* stageEnemyList;
Enemy* stageEnemies(void) { return stageEnemyList; }
static u16 stageMaxAliveV = 3;      // (12/10) el tope de vivos en curso

// (21/10) Tope de vivos del nivel en curso (ver maxAlive1P/2P en stage_level.h).
static u16 aliveCap(bool dos) {
    u16 v = dos ? (cur->maxAlive2P ? cur->maxAlive2P : STAGE_MAX_ALIVE_2P)
                : (cur->maxAlive1P ? cur->maxAlive1P : STAGE_MAX_ALIVE_1P);
    return (v > MAX_ENEMIES) ? MAX_ENEMIES : v;
}
static s16 stageCamXv;              // (12/10) la camara en curso (para los actores)
static s16 stageSpawnY;             // (12/10) y de STAGE_JUMP_IN / STAGE_IN_PLACE
static s16 stageProg;               // (12/10) avance del fondo (autoScrollQ), tope 32000
static u16 stageScroll;             // (12/10) el scroll que se ve (u16: da la vuelta)
static u8  stageProgAcc;
static u8  stageVisAcc;             // (14/10) resto del scroll que se ve (autoScrollVisQ)
s16 stageProgress(void) { return stageProg; }

s16 stageWalkTopAt(s16 fx) {
    if (!cur || !cur->walkTop) return cur ? cur->walkYMin : 0;
    s16 c = (s16)(fx >> 3);
    if (c < 0) c = 0;
    if (c >= (s16)cur->walkCols) c = (s16)(cur->walkCols - 1);
    return (s16)cur->walkTop[c];
}

// Revierte lo justo (primero la Y, despues la X) si el paso dejo los pies por
// encima del tope de su columna. Nunca teletransporta.
static void clampWalk(s16* x, s16* y, s16 px, s16 py, s16 footDx) {
    if (*y >= stageWalkTopAt(*x + footDx)) return;
    if (py >= stageWalkTopAt(*x + footDx)) { *y = py; return; }
    if (*y >= stageWalkTopAt(px + footDx)) { *x = px; return; }
    *x = px;
    *y = py;
    s16 t = stageWalkTopAt(*x + footDx);   // red de seguridad
    if (*y < t) *y = t;
}

// ---------------------------------------------------------------------------
// ESCALON (28/09, sewer). Tiene la misma idea que la cornisa del 2-1: la 'y'
// es PROFUNDIDAD, y la cara del escalon es una banda de profundidad donde no
// se puede estar parado.
//   - Parado arriba y caminando hacia abajo: al pasarse del borde se deja
//     caer al piso de abajo (playerFallTo: el sprite no salta, baja con la
//     gravedad del salto).
//   - Parado abajo y caminando hacia arriba: el escalon frena. Hay que
//     saltar y, en el aire, apretar arriba hasta pasar el borde.
//   - Al aterrizar dentro de la banda: si quedo a LEDGE_SNAP px del borde de
//     arriba se lo sube (no perder el salto por nada); si no, cae abajo.
// Los soldiers no saltan: cuando su 'y' cruza la banda se la pasa de una al
// otro piso y se compensa con un desplazamiento VISUAL (eStepZ) que vuelve a
// 0 de a pocos px por frame: se los ve bajar o trepar el escalon.
// ---------------------------------------------------------------------------
#define LEDGE_SNAP        6
#define LEDGE_Z_DOWN      3     // px por frame que baja un soldier
#define LEDGE_Z_UP        2     // px por frame que trepa

// (29/09) Durante la pelea con el JEFE el escalon no existe: la franja vuelve
// a ser una sola, como antes del 28/09 (el escalon no
// tiene que afectar la pelea del jefe). Se apaga al arrancar el jefe y vuelve
// a prenderse en cada stageLevelRun.
static bool ledgeOff;
static inline bool ledgeOn(void) {
    return cur && !ledgeOff && cur->ledgeBot > cur->ledgeTop;
}
static inline bool inLedge(s16 y) {
    return (y > cur->ledgeTop && y < cur->ledgeBot);
}
// (18/10) Pies dentro del tramo del escalon (ver ledgeX0/X1).
static inline bool ledgeAtX(s16 fx) {
    if (cur->ledgeX1 <= cur->ledgeX0) return TRUE;
    return (fx >= cur->ledgeX0 && fx < cur->ledgeX1);
}
// (18/10) Arriba de una plataforma de tramo: lo que se pasa de los costados en
// diagonal (a esta profundidad). 0 = adentro; <0 se paso a la izquierda.
static s16 ledgeTopOut(s16 fx, s16 y) {
    if (cur->ledgeX1 <= cur->ledgeX0 || y > cur->ledgeTop || !ledgeAtX(fx)) return 0;
    s16 d  = (s16)(cur->ledgeTop - y);
    s16 xl = (s16)(cur->ledgeX0 + cur->ledgeInset - d);
    s16 xr = (s16)(cur->ledgeX1 - cur->ledgeInset + d);
    if (fx < xl) return (s16)(fx - xl);
    if (fx >= xr) return (s16)(fx - xr + 1);
    return 0;
}

// Una Y de pies valida en el suelo: la de la banda va al piso mas cercano.
static s16 ledgeSnap(s16 y) {
    if (!ledgeOn() || !inLedge(y)) return y;
    return (y - cur->ledgeTop <= cur->ledgeBot - y) ? cur->ledgeTop : cur->ledgeBot;
}

static void ledgePlayer(Player* p, s16 prevX, s16 prevY, bool* wasAir) {
    if (!ledgeOn()) return;
    const s16 lt = cur->ledgeTop, lb = cur->ledgeBot;
    // (18/10) Fuera del tramo del escalon (plataforma de la fabrica) la banda
    // es piso comun: solo cuenta con los pies dentro de [ledgeX0, ledgeX1).
    bool at = ledgeAtX((s16)(p->x + FOOT_DX));
    if (!isPlayerJumping(p)) {
        if (*wasAir) {                              // aterrizo este frame
            if (at && inLedge(p->y)) {
                if (p->y <= lt + LEDGE_SNAP) p->y = lt;
                else playerFallTo(p, lb);
            }
        } else if (at && prevY <= lt && p->y > lt) {    // se paso del borde
            playerFallTo(p, lb);
        } else if (at && prevY >= lb && p->y < lb) {    // contra el escalon
            p->y = lb;
        } else if (at && inLedge(p->y)) {
            if (!ledgeAtX((s16)(prevX + FOOT_DX))) p->x = prevX;   // (18/10) contra el costado
            else p->y = ledgeSnap(p->y);                        // red de seguridad
        }
        // (18/10) Arriba, los costados en diagonal son pared.
        s16 out = ledgeTopOut((s16)(p->x + FOOT_DX), p->y);
        if (out) p->x = (s16)(p->x - out);
    }
    *wasAir = isPlayerJumping(p);
}

static s16 eStepZ[MAX_ENEMIES];

static void ledgeEnemy(Enemy* e, u16 i, s16 prevY) {
    if (!ledgeOn()) return;
    s16 fx = getEnemyCenterX(e);
    if (ledgeAtX(fx) && inLedge(e->y)) {
        s16 to;
        if (prevY <= cur->ledgeTop)      to = cur->ledgeBot;   // baja
        else if (prevY >= cur->ledgeBot) to = cur->ledgeTop;   // sube
        else                             to = ledgeSnap(e->y);
        // Mismo lugar en pantalla: y - z se conserva.
        eStepZ[i] = (s16)(eStepZ[i] + (to - e->y));
        e->y = to;
    }
    s16 out = ledgeTopOut(fx, e->y);                // (18/10) costados de arriba
    if (out) e->x = (s16)(e->x - out);
    if (eStepZ[i] > 0) eStepZ[i] = (eStepZ[i] > LEDGE_Z_DOWN) ? eStepZ[i] - LEDGE_Z_DOWN : 0;
    else if (eStepZ[i] < 0) eStepZ[i] = (eStepZ[i] < -LEDGE_Z_UP) ? eStepZ[i] + LEDGE_Z_UP : 0;
}

// ---------------------------------------------------------------------------
// Primer plano (BG_A, prioridad alta, sin las filas del HUD)
// ---------------------------------------------------------------------------
static u16 fgVram;
// (18/10) MODO LIVIANO con 3-4 jugadores: sin primer plano (los caños de la
// cloaca, las columnas de la fabrica), sin sus tramos en la franja del HUD
// (fgTop) y sin capa lejana (el skyline de la autopista y del skate). Toda
// esa VRAM pasa a los sprites: con cuatro tortugas en pantalla no alcanzaba
// para los enemigos y actores (el auto de la autopista no entraba nunca).
static bool stageLite;
static s16 fgColRight, fgColLeft;
static u16 fgBuf[32];

// (18/10) Overlay (ver ovW en stage_level.h): tiles propios en ovVram.
static u16 ovVram;

static u16 extraVram;
u16 stageExtraVram(void) { return extraVram; }

// (18/10) Temblor: +-STAGE_SHAKE_AMP px alternando cada 2 cuadros.
#define STAGE_SHAKE_AMP 3
static u8 shakeT;
void stageShake(u8 frames) { if (frames > shakeT) shakeT = frames; }
static s16 shakeX(void) {
    if (!shakeT) return 0;
    return ((shakeT >> 1) & 1) ? STAGE_SHAKE_AMP : -STAGE_SHAKE_AMP;
}

void stageOverlayFrame(const u32* tiles) {
    if (cur && cur->ovW) VDP_loadTileData(tiles, ovVram, (u16)(cur->ovW * cur->ovH), DMA_QUEUE);
}

static void fgDrawCol(s16 col) {
    // (18/10) Sin primer plano (o en modo liviano) solo se dibuja el overlay.
    const TileMap* tm = (cur->fg && !stageLite) ? cur->fg->tilemap : NULL;
    // Con camara vertical el HUD va en WINDOW: se dibujan TODAS las filas.
    const u16 r0 = camYOn() ? 0 : STAGE_HUD_ROWS;
    const u16 r1 = camYOn() ? 32 : STAGE_ROWS;
    const bool ov = cur->ovW && col >= cur->ovCol && col < (s16)(cur->ovCol + cur->ovW);
    for (u16 r = r0; r < r1; r++) {
        u16 v = 0;
        if (ov && r >= cur->ovRow && r < (u16)(cur->ovRow + cur->ovH)) {
            v = (u16)(TILE_ATTR(PAL1, FALSE, FALSE, FALSE)
                      | (ovVram + (r - cur->ovRow) * cur->ovW + (u16)(col - cur->ovCol)));
        } else if (tm && col >= 0 && col < (s16)tm->w && r < tm->h) {
            u16 e   = tm->tilemap[(u32)r * tm->w + (u16)col];
            u16 idx = e & TILE_INDEX_MASK;
            if (idx)   // tile 0 = vacio (rescomp lo deja primero)
                v = (u16)((e & (TILE_ATTR_VFLIP_MASK | TILE_ATTR_HFLIP_MASK))
                          | TILE_ATTR(PAL0, TRUE, FALSE, FALSE) | (fgVram + idx));
        }
        fgBuf[r - r0] = v;
    }
    VDP_setTileMapDataRect(BG_A, fgBuf, (u16)(col & 63), r0, 1, (u16)(r1 - r0), 1, CPU);
}

static void fgUpdate(s16 camX) {
    if ((!cur->fg || stageLite) && !cur->ovW) return;
    s16 wantL = (s16)(camX >> 3);
    s16 wantR = (s16)(wantL + SBG_WIN_COLS - 1);
    while (fgColRight < wantR) fgDrawCol(++fgColRight);
    while (fgColLeft > wantL)  fgDrawCol(--fgColLeft);
    if (fgColLeft < wantL) fgColLeft = wantL;
    if (fgColRight > wantR) fgColRight = wantR;
}

// (21/10) Relleno de BG_A (aPat): el plano entero, una sola vez.
static void aPatFill(void) {
    const u16 attr = TILE_ATTR(PAL0, cur->aPatHigh, FALSE, FALSE);
    for (u16 r = 0; r < 32; r++) {
        u16 row[64];
        const u8* pr = cur->aPat + (u32)(r % cur->aPatH) * cur->aPatW;
        for (u16 c = 0; c < 64; c++) {
            u8 k = pr[c % cur->aPatW];
            row[c] = k ? (u16)(attr | (extraVram + k - 1)) : 0;
        }
        VDP_setTileMapDataRect(BG_A, row, 0, r, 64, 1, 64, CPU);
    }
}

// Tramos del primer plano en la franja del HUD (filas 0-3), como sprites de
// 8x32 que scrollean con la camara. Fuera de pantalla se ocultan (una X muy
// negativa da la vuelta en la VDP).
#define STAGE_FGTOP_MAX 16
static Sprite* fgTopSpr[STAGE_FGTOP_MAX];

// Tiles por frame de fgTop (todos los frames miden lo mismo).
static u16 fgTopTilesPerFrame(void) {
    return (u16)((cur->fgTop->w >> 3) * (cur->fgTop->h >> 3));
}
// VRAM de planos que piden los frames compartidos de fgTop (0 si no hay).
static u16 fgTopSharedTiles(const StageLevel* L) {
    if (!L->fgTop || !L->fgTopF) return 0;
    return (u16)(L->fgTopFrames * ((L->fgTop->w >> 3) * (L->fgTop->h >> 3)));
}

static void fgTopInit(u16 sharedVram) {
    for (u16 i = 0; i < STAGE_FGTOP_MAX; i++) fgTopSpr[i] = NULL;
    if (!cur->fgTop || stageLite) return;
    const u16 tpf = fgTopTilesPerFrame();
    if (cur->fgTopF) {
        // Cada frame DISTINTO se carga una sola vez; los sprites apuntan ahi.
        const Animation* an = cur->fgTop->animations[0];
        for (u16 f = 0; f < cur->fgTopFrames && f < an->numFrame; f++)
            VDP_loadTileSet(an->frames[f]->tileset, (u16)(sharedVram + f * tpf), DMA);
    }
    for (u16 i = 0; i < cur->fgTopN && i < STAGE_FGTOP_MAX; i++) {
        // Prioridad BAJA: el texto del HUD (prioridad alta) queda encima.
        Sprite* s;
        if (cur->fgTopF) {
            u16 f = cur->fgTopF[i];
            s = SPR_addSpriteEx(cur->fgTop, 0, 0,
                                TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE,
                                               (u16)(sharedVram + f * tpf)),
                                SPR_FLAG_AUTO_VISIBILITY);
            if (!s) continue;
            SPR_setAutoAnimation(s, FALSE);
            SPR_setAnimAndFrame(s, 0, (s16)f);
        } else {
            s = SPR_addSprite(cur->fgTop, 0, 0, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
            if (!s) continue;
            SPR_setAutoAnimation(s, FALSE);
            SPR_setAnimAndFrame(s, 0, (s16)i);
        }
        SPR_setDepth(s, SPR_MIN_DEPTH + 1);
        SPR_setVisibility(s, HIDDEN);
        fgTopSpr[i] = s;
    }
    hudFramesToFront();
}

static void fgTopUpdate(s16 camX) {
    for (u16 i = 0; i < STAGE_FGTOP_MAX; i++) {
        Sprite* s = fgTopSpr[i];
        if (!s) continue;
        s16 sx = (s16)(cur->fgTopX[i] - camX);
        bool vis = (sx > -8) && (sx < SCREEN_W);
        SPR_setVisibility(s, vis ? VISIBLE : HIDDEN);
        // Con camara vertical el frame arranca en la fila 0 de la imagen.
        if (vis) SPR_setPosition(s, sx, camYOn() ? (s16)(cur->camYMin - stageCamY) : 0);
    }
}

// Capa lejana (BG_B, PAL3, baja prioridad): entera en VRAM, filas 4-27 del
// plano (0-3 son del HUD), envuelve cada farW columnas.
static void farDraw(u16 vram) {
    const TileMap* tm = cur->far->tilemap;
    for (u16 r = STAGE_HUD_ROWS; r < STAGE_ROWS; r++) {
        u16 sr = (u16)(r + cur->farRowShift);
        for (u16 c = 0; c < 64; c++) {
            u16 v = 0;
            if (sr < tm->h) {
                u16 e = tm->tilemap[(u32)sr * tm->w + (c % tm->w)];
                v = (u16)((e & (TILE_ATTR_VFLIP_MASK | TILE_ATTR_HFLIP_MASK))
                          | TILE_ATTR(cur->farPal0 ? PAL0 : PAL3, FALSE, FALSE, FALSE)
                          | (vram + (e & TILE_INDEX_MASK)));
            }
            fgBuf[c & 31] = v;
            if ((c & 31) == 31)
                VDP_setTileMapDataRect(BG_B, fgBuf, (u16)(c - 31), r, 32, 1, 32, CPU);
        }
    }
}

// ---------------------------------------------------------------------------
// (22/10) Bandas de parallax en BG_B (ver StageBands en stage_level.h)
// ---------------------------------------------------------------------------
static u16 bandVram;
static s16 bandL[STAGE_MAX_BANDS], bandR[STAGE_MAX_BANDS];
static s16 bandRowScr[STAGE_ROWS];      // scroll de BG_B por fila (sin el temblor)
static u16 bandBuf[32];

// (22/10) Cache de tiles de las bandas (StageBands.slots > 0).
#define BAND_NONE          0xFFFF
#define BAND_MAX_TILES     512
#define BAND_MAX_SLOTS     320
#define BAND_QUEUE_NEED     24          // lugar en la cola de DMA para una columna
static u16  bandTileSlot[BAND_MAX_TILES];
static u16  bandSlotRef[BAND_MAX_SLOTS];
static u16  bandFree[BAND_MAX_SLOTS];
static u16  bandFreeTop;
static bool bandCache;
static bool bandNow;                    // arranque: todo inmediato (pantalla en negro)

static u16 bandAcquire(u16 idx) {
    if (idx >= BAND_MAX_TILES) return BAND_NONE;
    u16 s = bandTileSlot[idx];
    if (s != BAND_NONE) { bandSlotRef[s]++; return s; }
    if (!bandFreeTop) return BAND_NONE;             // sin lugar: celda vacia
    s = bandFree[--bandFreeTop];
    bandTileSlot[idx] = s;
    bandSlotRef[s] = 1;
    VDP_loadTileData(cur->bands->tiles + (u32)idx * 8, (u16)(bandVram + s), 1,
                     bandNow ? DMA : DMA_QUEUE);
    return s;
}

static void bandRelease(u16 idx) {
    if (idx >= BAND_MAX_TILES) return;
    u16 s = bandTileSlot[idx];
    if (s == BAND_NONE) return;
    if (--bandSlotRef[s] == 0) {
        bandTileSlot[idx] = BAND_NONE;
        bandFree[bandFreeTop++] = s;
    }
}

static s16 bandX(const StageBand* b, s16 camX) {
    return (s16)(((s32)camX * b->num) / b->den);
}

static void bandDrawCol(const StageBand* b, s16 col) {
    u16 c = (u16)((col < 0) ? 0 : col) % b->w;
    const u16* m = b->map + c;
    for (u16 r = 0; r < b->rows; r++) {
        u16 e   = *m;
        u16 idx = (u16)(e & SBG_RAW_INDEX_MASK);
        u16 v   = 0;
        if (idx) {
            u16 t = bandCache ? bandAcquire(idx) : idx;
            if (t != BAND_NONE)
                v = (u16)(((e >> SBG_RAW_FLIP_SHIFT) & (TILE_ATTR_VFLIP_MASK | TILE_ATTR_HFLIP_MASK))
                          | TILE_ATTR(PAL0, FALSE, FALSE, FALSE) | (bandVram + t));
        }
        bandBuf[r] = v;
        m += b->w;
    }
    VDP_setTileMapDataRect(BG_B, bandBuf, (u16)(col & 63), b->row0, 1, b->rows, 1,
                           bandNow ? CPU : DMA_QUEUE_COPY);
}

// Suelta los tiles de una columna que salio (solo con cache).
static void bandDropCol(const StageBand* b, s16 col) {
    if (!bandCache) return;
    u16 c = (u16)((col < 0) ? 0 : col) % b->w;
    const u16* m = b->map + c;
    for (u16 r = 0; r < b->rows; r++) {
        u16 idx = (u16)(*m & SBG_RAW_INDEX_MASK);
        if (idx) bandRelease(idx);
        m += b->w;
    }
}

static bool bandQueueOk(void) {
    return bandNow || (DMA_getQueueSize() + BAND_QUEUE_NEED < DMA_getMaxQueueSize());
}

// Trae las columnas que entraron en cada banda (y suelta las que salieron) y
// arma el scroll por fila. Si la cola de DMA esta llena, la columna nueva
// espera al cuadro siguiente (entra por el borde, fuera de pantalla).
static void bandsUpdate(s16 camX) {
    const StageBands* bs = cur->bands;
    for (u16 r = 0; r < STAGE_ROWS; r++) bandRowScr[r] = 0;
    for (u16 k = 0; k < bs->n && k < STAGE_MAX_BANDS; k++) {
        const StageBand* b = &bs->band[k];
        s16 x = bandX(b, camX);
        s16 wantL = (s16)(x >> 3);
        s16 wantR = (s16)(wantL + SBG_WIN_COLS - 1);
        // primero se sueltan las que salieron (liberan lugar en el cache)
        while (bandL[k] < wantL && bandL[k] <= bandR[k]) bandDropCol(b, bandL[k]++);
        while (bandR[k] > wantR && bandR[k] >= bandL[k]) bandDropCol(b, bandR[k]--);
        if (bandL[k] > bandR[k]) { bandL[k] = wantL; bandR[k] = (s16)(wantL - 1); }
        while (bandR[k] < wantR && bandQueueOk()) bandDrawCol(b, ++bandR[k]);
        while (bandL[k] > wantL && bandQueueOk()) bandDrawCol(b, --bandL[k]);
        for (u16 r = b->row0; r < b->row0 + b->rows && r < STAGE_ROWS; r++)
            bandRowScr[r] = (s16)-x;
    }
}

static void bandsInit(s16 camX) {
    const StageBands* bs = cur->bands;
    bandCache = (bs->slots != 0);
    bandNow   = TRUE;
    if (bandCache) {
        u16 n = (bs->slots > BAND_MAX_SLOTS) ? BAND_MAX_SLOTS : bs->slots;
        for (u16 i = 0; i < BAND_MAX_TILES; i++) bandTileSlot[i] = BAND_NONE;
        for (u16 i = 0; i < n; i++) { bandFree[i] = (u16)(n - 1 - i); bandSlotRef[i] = 0; }
        bandFreeTop = n;
    } else {
        VDP_loadTileData(bs->tiles, bandVram, bs->nTiles, DMA);
    }
    for (u16 k = 0; k < STAGE_MAX_BANDS; k++) {
        s16 l = (k < bs->n) ? (s16)(bandX(&bs->band[k], camX) >> 3) : 0;
        bandL[k] = l;
        bandR[k] = (s16)(l - 1);
    }
    bandsUpdate(camX);
    bandNow = FALSE;
}

// Scroll por fila de tile. static: van por DMA_QUEUE.
static s16 scrA[STAGE_ROWS], scrB[STAGE_ROWS];

static void applyScroll(s16 camX, TransferMethod tm) {
    s16 farX = 0;
    if (cur->far) {
        u16 dv = cur->farDiv ? cur->farDiv : 1;
        farX = cur->autoScrollQ ? (s16)-(s16)((u16)camX / dv)     // (12/10) scroll u16
                                : (s16)-(camX / (s16)dv);
    }
    s16 sh = shakeX();
    if (shakeT) shakeT--;
    for (u16 r = 0; r < STAGE_ROWS; r++) {
        s16 rs = (r < STAGE_HUD_ROWS) ? 0 : sh;     // el HUD no tiembla
        if (cur->bands) {
            // (22/10) Parallax: el frente con la camara, BG_B por banda. El HUD
            // va en WINDOW (tapa las filas 0-3 de BG_A).
            scrA[r] = (s16)(-camX + rs);
            scrB[r] = (s16)(bandRowScr[r] + rs);
        } else if (cur->far) {
            scrA[r] = (s16)(-camX + rs);
            scrB[r] = (r < STAGE_HUD_ROWS) ? 0 : (s16)(farX + rs);
        } else {
            scrB[r] = (s16)(-camX + rs);
            scrA[r] = (r < STAGE_HUD_ROWS && !camYOn()) ? 0 : (s16)(-camX + rs + stageAOffX);
        }
    }
    VDP_setHorizontalScrollTile(BG_A, 0, scrA, STAGE_ROWS, tm);
    VDP_setHorizontalScrollTile(BG_B, 0, scrB, STAGE_ROWS, tm);
}

// ---------------------------------------------------------------------------
// Spawn de un soldier de oleada: entra por el borde de la camara, por la
// izquierda con la voltereta y por la derecha con la patada (spawns del 1-1).
// ---------------------------------------------------------------------------
// (12/10) SALPICON del agua (sewer): efecto suelto, 4 frames.
#define STAGE_SPLASH_MAX     2
#define STAGE_SPLASH_TICKS   5
#define STAGE_SPLASH_FRAMES  4
#define STAGE_SPLASH_W      64
#define STAGE_SPLASH_H      48
// El que sale del agua: patada de entrada saltando para arriba desde la
// lane del canal (el arco lo hace el SPAWNING de la patada, ver enemy.c).
#define STAGE_WATER_V0_Q   (-1024)  // 4 px/f hacia arriba (gravedad 0,25: apice ~32 px)
static struct { Sprite* spr; s16 x, y; u8 fr, tick; } splash[STAGE_SPLASH_MAX];

static void splashInit(void) {
    for (u16 i = 0; i < STAGE_SPLASH_MAX; i++) splash[i].spr = NULL;
}
static void splashRelease(void) {
    for (u16 i = 0; i < STAGE_SPLASH_MAX; i++)
        if (splash[i].spr) { SPR_releaseSprite(splash[i].spr); splash[i].spr = NULL; }
}
static void splashAt(s16 x, s16 y) {
    for (u16 i = 0; i < STAGE_SPLASH_MAX; i++) {
        if (splash[i].spr) continue;
        Sprite* s = SPR_addSprite(&water_splash, -80, -80, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        if (!s) return;                        // sin VRAM: sale igual, sin salpicon
        // Los colores del agua estan en la paleta del blanco (PAL3, que hasta
        // el jefe es de los blancos; con capa lejana no hay agua).
        if (!cur->far) PAL_setPalette(PAL3, foot_soldier_white.palette->data, DMA_QUEUE);
        SPR_setAutoAnimation(s, FALSE);
        SPR_setFrame(s, 0);
        splash[i].spr = s; splash[i].x = x; splash[i].y = y;
        splash[i].fr = 0; splash[i].tick = 0;
        return;
    }
}
static void splashUpdate(s16 camX) {
    for (u16 i = 0; i < STAGE_SPLASH_MAX; i++) {
        if (!splash[i].spr) continue;
        if (++splash[i].tick >= STAGE_SPLASH_TICKS) {
            splash[i].tick = 0;
            if (++splash[i].fr >= STAGE_SPLASH_FRAMES) {
                SPR_releaseSprite(splash[i].spr); splash[i].spr = NULL; continue;
            }
            SPR_setFrame(splash[i].spr, splash[i].fr);
        }
        SPR_setPosition(splash[i].spr, (s16)(splash[i].x - camX - STAGE_SPLASH_W / 2),
                        (s16)(splash[i].y - STAGE_SPLASH_H + 6 - stageCamY));
        SPR_setDepth(splash[i].spr, (s16)(-(splash[i].y) - 2));
    }
}

static void spawnWaveEnemy(Enemy* e, u8 type, s8 side, s16 camX, s16 atX) {
    s16 w  = enemySpriteW(type);
    if (side == STAGE_IN_WATER) {
        // (12/10) Sale del AGUA como en el arcade: salpicon y un salto para
        // arriba (la patada de entrada) hasta caer en el canal.
        s16 y  = ledgeSnap((s16)(cur->walkYMax - 6));
        s8  d  = (atX < camX + SCREEN_W / 2) ? 1 : -1;
        u8  pl = (type == ENEMY_TYPE_FOOT_SOLDIER_WHITE || type == ENEMY_TYPE_FOOT_SOLDIER_KNIFE) ? PAL3 : PAL2;
        if (type == ENEMY_TYPE_FOOT_SOLDIER) {
            initEnemyKickSpawn(e, (s16)(atX - w / 2), y, d, pl, type);
            e->jumpZq  = 1 << 8;
            e->jumpZ   = 1;
            e->jumpVel = STAGE_WATER_V0_Q;
        } else {
            initEnemyWalkInSpawn(e, (s16)(atX - w / 2), y, d, pl, type);
        }
        setEnemyBounds(e, cur->walkYMin, cur->walkYMax, 0, 0, cur->levelW);
        // (13/10) El salpicon DESPUES del soldier: antes se pedia primero y
        // podia quedarse con la VRAM que sprVramFits habia visto libre para
        // el soldier -> el soldier quedaba sin sprite y la oleada trabada.
        if (e->sprite) splashAt(atX, y);
        return;
    }
    if (side == STAGE_JUMP_IN || side == STAGE_IN_PLACE) {
        // (12/10) Desde un actor del nivel (ver stage_level.h).
        s16 y  = ledgeSnap(stageSpawnY);
        s8  d  = (atX < camX + SCREEN_W / 2) ? 1 : -1;
        u8  pl = (type == ENEMY_TYPE_FOOT_SOLDIER_WHITE || type == ENEMY_TYPE_FOOT_SOLDIER_KNIFE) ? PAL3 : PAL2;
        if (side == STAGE_JUMP_IN && type == ENEMY_TYPE_FOOT_SOLDIER) {
            initEnemyKickSpawn(e, (s16)(atX - w / 2), y, d, pl, type);
            e->jumpZq  = 1 << 8;
            e->jumpZ   = 1;
            e->jumpVel = STAGE_WATER_V0_Q;
        } else {
            initEnemyWalkInSpawn(e, (s16)(atX - w / 2), y, d, pl, type);
        }
        setEnemyBounds(e, cur->walkYMin, cur->walkYMax, 0, 0, cur->levelW);
        return;
    }
    if (side == STAGE_FROM_BACK) {
        // (12/10) Desde el FONDO del piso (garage): el morado baja de un salto
        // (como del agua, sin salpicon); los demas salen caminando hacia el
        // centro de la pantalla.
        s16 y  = ledgeSnap((s16)(stageWalkTopAt(atX) + 4));
        s8  d  = (atX < camX + SCREEN_W / 2) ? 1 : -1;
        u8  pl = (type == ENEMY_TYPE_FOOT_SOLDIER_WHITE || type == ENEMY_TYPE_FOOT_SOLDIER_KNIFE) ? PAL3 : PAL2;
        if (type == ENEMY_TYPE_FOOT_SOLDIER) {
            initEnemyKickSpawn(e, (s16)(atX - w / 2), y, d, pl, type);
            e->jumpZq  = 1 << 8;
            e->jumpZ   = 1;
            e->jumpVel = STAGE_WATER_V0_Q;
        } else {
            initEnemyWalkInSpawn(e, (s16)(atX - w / 2), y, d, pl, type);
        }
        setEnemyBounds(e, cur->walkYMin, cur->walkYMax, 0, 0, cur->levelW);
        return;
    }
    s16 x  = (side < 0) ? (s16)(camX - w) : (s16)(camX + SCREEN_W);
    s16 fx = (s16)((side < 0) ? camX + 24 : camX + SCREEN_W - 24);
    s16 top = stageWalkTopAt(fx);
    s16 span = (s16)(cur->walkYMax - 8 - (top + 8));
    s16 y = (s16)(top + 8 + ((span > 0) ? (s16)(random() % (u16)span) : 0));
    y = ledgeSnap(y);
    // (29/09) Solo el MORADO tiene voltereta y patada de entrada. El naranja
    // no tiene la anim 15: con la voltereta aparecia de golpe o con pixeles
    // basura. El naranja, el blanco y los de arma (06/10) entran CAMINANDO.
    if (type != ENEMY_TYPE_FOOT_SOLDIER) {
        initEnemyWalkInSpawn(e, x, y, (s8)((side < 0) ? 1 : -1),
                             (type == ENEMY_TYPE_FOOT_SOLDIER_WHITE || type == ENEMY_TYPE_FOOT_SOLDIER_KNIFE) ? PAL3 : PAL2, type);
    }
    else if (side < 0) initEnemySomersaultSpawn(e, x, y, 1, PAL2, type);
    else               initEnemyKickSpawn(e, x, y, -1, PAL2, type);
    setEnemyBounds(e, cur->walkYMin, cur->walkYMax, 0, 0, cur->levelW);
}

u16 stageAliveEnemies(void) {
    u16 n = 0;
    if (!stageEnemyList) return 0;
    for (u16 i = 0; i < MAX_ENEMIES; i++)
        if (stageEnemyList[i].state != ENEMY_STATE_INACTIVE) n++;
    return n;
}

bool stageSpawnEnemyAt(u8 type, s8 side, s16 x, s16 y) {
    if (!cur || !stageEnemyList) return FALSE;
    // STAGE_IN_PLACE reemplaza a un actor que ya estaba peleando (el del
    // misil): no espera lugar en el tope de vivos.
    if (side != STAGE_IN_PLACE && stageAliveEnemies() >= stageMaxAliveV) return FALSE;
    u16 i;
    for (i = 0; i < MAX_ENEMIES; i++)
        if (stageEnemyList[i].state == ENEMY_STATE_INACTIVE) break;
    if (i >= MAX_ENEMIES) return FALSE;
    if (!sprVramFits(enemySheetDef(type)->maxNumTile)) return FALSE;
    eStepZ[i] = 0;
    stageSpawnY = y;
    spawnWaveEnemy(&stageEnemyList[i], type, side, stageCamXv, x);
    if (!stageEnemyList[i].sprite) {            // (13/10) sin sprite: no salio
        stageEnemyList[i].state = ENEMY_STATE_INACTIVE;
        return FALSE;
    }
    return TRUE;
}

// ===========================================================================
// El nivel
// ===========================================================================
SceneId stageLevelRun(const StageLevel* L) {
    cur = L;
    clearScene();

    const u8  nPlSetup  = numJugadores();
    const u16 barBlocks = (u16)((nPlSetup > 2) ? MAX_PLAYERS : 2) * HUD_VRAM_PER_PLAYER;
    stageLite = (nPlSetup > 2);         // (18/10) ver stageLite
    // En la variante con capa lejana, su tileset ocupa el lugar del primer plano.
    // (18/10) En modo liviano no se carga ninguno de los dos.
    const Image* extra  = stageLite ? NULL : (L->far ? L->far : L->fg);
    const u16 fgTiles   = extra ? extra->tileset->numTile : 0;
    const u16 topTiles  = stageLite ? 0 : fgTopSharedTiles(L);
    const bool camV = (L->camYMin < 0 || L->camYMax > 0);
    const VDPPlane hudPl = L->far ? BG_B : ((camV || L->bands) ? WINDOW : BG_A);
    // (22/10) parallax: VRAM de las bandas (enteras o el cache)
    const u16 bandTiles = !L->bands ? 0
                        : (L->bands->slots ? L->bands->slots : L->bands->nTiles);
    const VDPPlane bgPl  = (L->far || L->bands) ? BG_A : BG_B;
    const s16 camMaxX   = (s16)(L->levelW - SCREEN_W);

    // VRAM: HUD | primer plano | bandas | cache del fondo | overlay | sprites.
    const u16 ovTiles   = (u16)(L->ovW * L->ovH);   // (18/10) overlay
    SPR_initEx((u16)(TILE_FONT_INDEX - (TILE_USER_INDEX + barBlocks + fgTiles
                                        + bandTiles + topTiles + L->bgSlots
                                        + ovTiles + L->extraTiles)));
    shakeT = 0;
    stageCamY = 0;
    stageAOffX = 0; stageAOffY = 0;    // (21/10)

    VDP_setScreenWidth320();
    VDP_setPlaneSize(64, 32, TRUE);
    VDP_setScrollingMode(HSCROLL_TILE, VSCROLL_PLANE);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    if (camV || L->bands) {
        // HUD en WINDOW: las 4 filas de arriba, fijas, tapando a BG_A.
        VDP_clearPlane(WINDOW, TRUE);
        VDP_setWindowHPos(FALSE, 0);
        VDP_setWindowVPos(FALSE, STAGE_HUD_ROWS);
    }
    VDP_setBackgroundColor((u8)L->backdrop);   // 0 = PAL0[0]: los rips lo usan como color

    u8   nPl = nPlSetup;
    bool dosJugadores = (nPl >= 2);
    s16  cameraX = 0;

    u16 hudVram = TILE_USER_INDEX;
    fgVram = (u16)(TILE_USER_INDEX + barBlocks);
    bandVram = (u16)(fgVram + fgTiles);        // (22/10)
    ovVram = (u16)(fgVram + fgTiles + bandTiles + topTiles + L->bgSlots);
    extraVram = (u16)(ovVram + ovTiles);       // (18/10)
    if (extra) VDP_loadTileSet(extra->tileset, fgVram, DMA);
    if (L->far && !stageLite) farDraw(fgVram);
    // (12/10) Fondo que corre solo: la imagen se repite en X.
    stageProg = 0; stageProgAcc = 0; stageScroll = 0; stageVisAcc = 0;
    sbgSetWrap(L->autoScrollQ != 0);
    playerSkateLift = L->skateLift;
    enemySkateMode  = (bool)(L->skateLift != 0);
    sbgSetRegions(L->bgRegions, L->nBgRegions);   // (01/10) NULL/0 casi siempre
    if (L->bgRaw)
        sbgInitRaw(L->bgRaw, bgPl, PAL0,
                   (u16)(fgVram + fgTiles + bandTiles + topTiles), L->bgSlots, cameraX);
    else
        sbgInit(L->bg, bgPl, PAL0,
                (u16)(fgVram + fgTiles + bandTiles + topTiles), L->bgSlots, cameraX);
    fgColLeft  = 0;
    fgColRight = -1;
    fgUpdate(cameraX);
    if (L->bands) bandsInit(cameraX);          // (22/10) parallax
    if (L->aPat && !L->fg && !L->ovW && !L->far && !L->bands) aPatFill();   // (21/10) la nieve
    applyScroll(cameraX, DMA);

    hudSetPlane(hudPl);
    hudInit();
    fgTopInit((u16)(fgVram + fgTiles + bandTiles));

    Player p1, p2, p3, p4;
    Player* pls[MAX_PLAYERS] = { &p1, &p2, &p3, &p4 };
    for (u8 k = 0; k < nPl; k++) {
        // Lanes repartidas en la franja (arriba de todo nadie queda comodo).
        s16 top = stageWalkTopAt(STAGE_START_X + FOOT_DX);
        s16 span = (s16)(L->walkYMax - top);
        static const u8 frac[MAX_PLAYERS] = { 40, 65, 20, 85 };   // % de la franja
        s16 y = ledgeSnap((s16)(top + (span * frac[k]) / 100));
        initPlayer(pls[k], playerChar(k), playerJoy(k), PAL1, STAGE_START_X, y);
        setPlayerLane(pls[k], L->walkYMin, L->walkYMax);
        setPlayerEndWall(pls[k], 0, 0);
        setPlayerLeftBound(pls[k], cameraX);
        setPlayerRightBound(pls[k], cameraX + SCREEN_W - PLAYER_SPRITE_W);
        setPlayerCamera(pls[k], cameraX);
    }

    VDP_loadFont(&hud_font, DMA);
    VDP_setTextPlane(hudPl);
    VDP_setTextPriority(1);
    VDP_setTextPalette(PAL1);

    p2JoinReset();
    static HudPlayer huds[MAX_PLAYERS];
    static ContPlayer conts[MAX_PLAYERS];
    contResetAll(conts);
    const u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    for (u8 k = 0; k < nPl; k++)
        hudPlayerInit(&huds[k], pls[k], hudPlayerCol(k),
                      (u16)(hudVram + k * HUD_VRAM_PER_PLAYER));

    resetEnemyAI(cantidadJugadores);
    shurikenInit();
    boomerangInit();
    splashInit();
    static Enemy enemies[MAX_ENEMIES];
    stageEnemyList = enemies;
    for (u16 i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].state  = ENEMY_STATE_INACTIVE;
        enemies[i].sprite = NULL;
        eStepZ[i] = 0;
    }
    bool plWasAir[MAX_PLAYERS] = { FALSE, FALSE, FALSE, FALSE };
    u16 maxAlive = aliveCap(dosJugadores);
    stageMaxAliveV = maxAlive;

    if (L->bossInit) L->bossInit();
    if (L->levelInit) L->levelInit();
    bool bossStarted = FALSE;
    ledgeOff = FALSE;

    s16 waveIdx     = -1;       // ultima oleada disparada
    u8  waveSpawned = 0;
    u8  waveTot     = 0;        // (20/10) entradas + extra de la dificultad
    u16 vramWait    = 0;        // frames sin VRAM para el siguiente de la oleada
    s16 camLockX    = -1;       // -1 = camara libre

    // Revelado: PAL0 el fondo, PAL1 las tortugas, PAL2 los foot soldiers,
    // PAL3 en negro (la carga el jefe al entrar).
    u16 target[64];
    for (u16 i = 0; i < 16; i++) {
        target[i]      = L->bgRaw ? L->bgRaw->pal->data[i] : L->bg->palette->data[i];
        target[16 + i] = leo_player.palette->data[i];
        target[32 + i] = foot_soldier.palette->data[i];
        target[48 + i] = L->pal3 ? L->pal3[i]
                       : ((L->far && !L->farPal0) ? L->far->palette->data[i] : 0);
    }
    for (u8 k = 0; k < nPl; k++) updatePlayer(pls[k]);   // coloca los sprites
    if (camYOn()) {
        stageCamY = camYTarget(pls, nPl);
        camYApply(TRUE);
        for (u8 k = 0; k < nPl; k++)
            if (pls[k]->sprite)
                playerSetSpritePos(pls[k], pls[k]->x - cameraX,
                                pls[k]->y - PLAYER_FOOT_OFFSET - playerDrawZ(pls[k]) - stageCamY);
        fgTopUpdate(cameraX);
    }
    // (18/10) Intro: las tortugas no estan hasta que termina.
    if (L->introStep) {
        for (u8 k = 0; k < nPl; k++)
            if (pls[k]->sprite) SPR_setVisibility(pls[k]->sprite, HIDDEN);
        if (L->introBegin) L->introBegin(target);
    }
    SPR_update();

    XGM2_setLoopNumber(-1);
    playMusicVol(L->music, L->musicVol);

    static const u16 black[64] = { 0 };
    PAL_setColors(0, black, 64, DMA);
    PAL_fadeInAll(target, 20, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    // (18/10) Intro y entrada cayendo de un salto desde la izquierda.
    u16 dropTicks = 0;
    if (L->introStep) {
        while (L->introStep(cameraX)) {
            SPR_update();
            SYS_doVBlankProcess();
        }
        for (u8 k = 0; k < nPl; k++) {
            pls[k]->x = (s16)(pls[k]->x - STAGE_DROP_BACK);
            if (pls[k]->sprite) SPR_setVisibility(pls[k]->sprite, VISIBLE);
            playerDropIn(pls[k], (s16)(STAGE_DROP_Z + k * STAGE_DROP_STAGGER));
            updatePlayer(pls[k]);
        }
        dropTicks = STAGE_DROP_TICKS;
    }

    bool running = TRUE;
    bool allOut  = FALSE;
    bool win     = FALSE;
    SceneId jump = PAUSE_NO_JUMP;
    pauseReset();
    idleMissileReset();
    bossVoReset();
    while (running) {
        jump = pausePoll(pls, nPl);
        if (jump != PAUSE_NO_JUMP) break;
        bossVoUpdate();   // (01/10) el tema del jefe entra cuando termina su voz

        // --- Jugadores -----------------------------------------------------
        for (u8 k = 0; k < nPl; k++) {
            s16 prevX = pls[k]->x, prevY = pls[k]->y;
            updatePlayer(pls[k]);
            clampWalk(&pls[k]->x, &pls[k]->y, prevX, prevY, FOOT_DX);
            ledgePlayer(pls[k], prevX, prevY, &plWasAir[k]);
        }
        // (18/10) Deriva de la entrada cayendo (despues de la intro).
        if (dropTicks > 0) {
            dropTicks--;
            for (u8 k = 0; k < nPl; k++)
                if (isPlayerJumping(pls[k])) pls[k]->x = (s16)(pls[k]->x + 1);
        }
        // (14/10) Modo ARCADE: quieto 7 s con la camara parada -> misil. En la
        // patineta no (la camara no se mueve nunca: corre el fondo).
        if (!L->autoScrollQ) idleMissileUpdate(pls, nPl, cameraX, stageCamY);

        // --- Camara: dead-zone a la derecha, nunca vuelve -------------------
        s16 leadX = p1.x;
        {
            bool any = FALSE;
            for (u8 k = 0; k < nPl; k++) {
                if (isPlayerGameOver(pls[k])) continue;
                if (!any || pls[k]->x > leadX) leadX = pls[k]->x;
                any = TRUE;
            }
        }
        s16 leadScreenX = leadX - cameraX;
        if (leadScreenX > CAM_DEAD_ZONE_RIGHT) {
            s16 nc = cameraX + (leadScreenX - CAM_DEAD_ZONE_RIGHT);
            if (nc - cameraX > CAM_MAX_SPEED_X) nc = cameraX + CAM_MAX_SPEED_X;
            if (camLockX >= 0 && nc > camLockX) nc = camLockX;
            if (nc > camMaxX) nc = camMaxX;
            if (nc > cameraX) cameraX = nc;
        }
        // (29/09) Camara vertical: se acerca de a STAGE_CAMY_SPEED px.
        if (camYOn()) {
            s16 t = camYTarget(pls, nPl);
            if (stageCamY < t) stageCamY = (s16)((t - stageCamY > STAGE_CAMY_SPEED) ? stageCamY + STAGE_CAMY_SPEED : t);
            else if (stageCamY > t) stageCamY = (s16)((stageCamY - t > STAGE_CAMY_SPEED) ? stageCamY - STAGE_CAMY_SPEED : t);
            camYApply(FALSE);
        }
        for (u8 k = 0; k < nPl; k++) {
            setPlayerCamera(pls[k], cameraX);
            setPlayerLeftBound(pls[k], cameraX);
            setPlayerRightBound(pls[k], cameraX + SCREEN_W - PLAYER_SPRITE_W);
            // updatePlayer ya lo dibujo, pero antes del recorte de la pared y
            // del movimiento de camara de este frame: se lo reubica.
            if (pls[k]->sprite)
                playerSetSpritePos(pls[k], pls[k]->x - cameraX,
                                pls[k]->y - PLAYER_FOOT_OFFSET - playerDrawZ(pls[k]) - stageCamY);
        }

        // --- P2 que se suma en plena partida --------------------------------
        if (nPl == 1) {
            u8 ch2 = p2JoinPoll(hudPlayerCol(1));
            if (ch2 != 0xFF) {
                cantidadJugadores = 2;
                initPlayer(&p2, ch2, playerJoy(1), PAL1,
                           (s16)(getPlayerWorldX(&p1) - 48), getPlayerY(&p1));
                setPlayerLane(&p2, L->walkYMin, L->walkYMax);
                setPlayerEndWall(&p2, 0, 0);
                setPlayerCamera(&p2, cameraX);
                setPlayerLeftBound(&p2, cameraX);
                setPlayerRightBound(&p2, cameraX + SCREEN_W - PLAYER_SPRITE_W);
                hudPlayerInit(&huds[1], &p2, hudPlayerCol(1),
                              (u16)(hudVram + HUD_VRAM_PER_PLAYER));
                nPl          = 2;
                dosJugadores = TRUE;
                maxAlive     = aliveCap(TRUE);
                stageMaxAliveV = maxAlive;
                resetEnemyAI(2);
            }
        }
        for (u8 k = 0; k < nPl; k++) hudPlayerUpdate(&huds[k]);

        // Continues (con el jefe muriendo no se evaluan).
        bool dying = (bossStarted && L->bossDying && L->bossDying());
        if (!dying && continueStepAll(conts, pls, huds, nPl, fps)) { allOut = TRUE; break; }

        // --- Oleadas --------------------------------------------------------
        u16 alive = 0;
        for (u16 i = 0; i < MAX_ENEMIES; i++)
            if (enemies[i].state != ENEMY_STATE_INACTIVE) alive++;
        if (L->specialAlive) alive += L->specialAlive();   // (12/10) actores del nivel
        stageCamXv = cameraX;

        s16 leadFeetX = leadX + FOOT_DX;
        // (12/10) Con el fondo corriendo solo, el avance manda (no los pies).
        if (L->autoScrollQ) {
            u16 t = (u16)(stageProgAcc + L->autoScrollQ);
            stageProgAcc = (u8)(t & 0xFF);
            if (stageProg < 32000) stageProg = (s16)(stageProg + (s16)(t >> 8));
            // (14/10) Lo que se ve puede correr mas rapido que el avance.
            u16 v = (u16)(stageVisAcc + (L->autoScrollVisQ ? L->autoScrollVisQ : L->autoScrollQ));
            stageVisAcc = (u8)(v & 0xFF);
            stageScroll = (u16)(stageScroll + (v >> 8));
            leadFeetX = stageProg;
        }
        if ((waveIdx < 0 || waveSpawned >= waveTot) &&
            waveIdx + 1 < (s16)L->nWaves &&
            leadFeetX >= L->waves[waveIdx + 1].trigX) {
            waveIdx++;
            waveSpawned = 0;
            waveTot = (u8)(L->waves[waveIdx].n + diffWaveExtra(waveSoldiers(&L->waves[waveIdx])));
            camLockX = L->waves[waveIdx].lockX;
            if (camLockX < cameraX) camLockX = cameraX;
        }
        if (waveIdx >= 0) {
            const StageWave* w = &L->waves[waveIdx];
            while (waveSpawned < waveTot && alive < maxAlive) {
                u8 we = waveEntry(w, waveSpawned);
                u16 i;
                for (i = 0; i < MAX_ENEMIES; i++)
                    if (enemies[i].state == ENEMY_STATE_INACTIVE) break;
                if (i >= MAX_ENEMIES) break;
                // (29/09) Sin VRAM de sprites para su sheet, espera (con la
                // camara vertical de la cloaca el presupuesto quedo justo): un
                // soldier sin sprite no se puede animar.
                // (06/10) Con 4 jugadores la VRAM puede no alcanzar NUNCA (ni con
                // la pantalla vacia): la camara quedaba clavada para siempre.
                // Si no hay nadie vivo y el que toca no entra en medio segundo
                // (lo que tardan en irse misiles y explosiones), sale un morado
                // (la hoja mas chica); si tampoco entra, a los 2,5 s se lo
                // saltea. La oleada nunca se traba.
                u8 ty = w->type[we];
                if (ty >= STAGE_SPECIAL) {
                    // (12/10) Actor del nivel: no ocupa lugar de soldier.
                    if (L->specialSpawn &&
                        !L->specialSpawn(ty, w->side[we], w->x[we], cameraX)) {
                        // Todavia no: se reintenta. (18/10) Pero si no hay
                        // nadie vivo y no arranca nunca (sin VRAM con 4
                        // jugadores), la camara quedaba clavada para siempre
                        // sin enemigos: a los STAGE_VRAM_SKIP frames se lo
                        // saltea (con su reemplazo, si el nivel tiene uno).
                        if (alive > 0) { vramWait = 0; break; }
                        if (++vramWait < STAGE_VRAM_SKIP) break;
                        vramWait = 0;
                        if (L->specialFallback)
                            L->specialFallback(ty, w->side[we], w->x[we], cameraX);
                    }
                    vramWait = 0;
                    waveSpawned++;
                    continue;
                }
                if (!sprVramFits(enemySheetDef(ty)->maxNumTile)) {
                    if (alive > 0) { vramWait = 0; break; }
                    vramWait++;
                    if (vramWait < STAGE_VRAM_WAIT) break;
                    ty = ENEMY_TYPE_FOOT_SOLDIER;
                    if (!sprVramFits(enemySheetDef(ty)->maxNumTile)) {
                        if (vramWait < STAGE_VRAM_SKIP) break;
                        vramWait = 0;
                        waveSpawned++;
                        continue;
                    }
                }
                vramWait = 0;
                eStepZ[i] = 0;
                spawnWaveEnemy(&enemies[i], ty, w->side[we], cameraX, w->x[we]);
                // (13/10) Si SGDK no le dio el sprite (VRAM fragmentada, o la
                // tomo otra cosa en el medio), no cuenta: se reintenta. Un
                // soldier sin sprite no se actualiza nunca (updateEnemyN sale
                // de una) y trababa la oleada para siempre.
                if (!enemies[i].sprite) {
                    enemies[i].state = ENEMY_STATE_INACTIVE;
                    break;
                }
                waveSpawned++;
                alive++;
            }
            // (22/10) 'alive' se conto al principio del cuadro: un actor que
            // arranco recien (el tractor del 3-1) todavia no estaba y la camara
            // se soltaba en el acto. Se vuelve a preguntar por los actores.
            if (waveSpawned >= waveTot && alive == 0 &&
                !(L->specialAlive && L->specialAlive())) camLockX = -1;   // limpia
        }

        splashUpdate(cameraX);

        // --- Foot soldiers ---------------------------------------------------
        separateEnemies(enemies, MAX_ENEMIES);
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            Enemy* e = &enemies[i];
            if (e->state == ENEMY_STATE_INACTIVE) continue;
            // (13/10) Red de seguridad: activo pero sin sprite = colgado.
            if (!e->sprite) { e->state = ENEMY_STATE_INACTIVE; continue; }
            s16 prevEX = e->x, prevEY = e->y;
            setEnemyCamera(e, cameraX);
            updateEnemyN(e, pls, nPl);
            if (e->state == ENEMY_STATE_INACTIVE) continue;
            clampWalk(&e->x, &e->y, prevEX, prevEY, (s16)(getEnemyCenterX(e) - e->x));
            ledgeEnemy(e, i, prevEY);
            if (e->sprite)
                SPR_setPosition(e->sprite, e->x - cameraX + enemyShakeX(e),
                                e->y - e->footOffset - e->jumpZ - eStepZ[i] - L->skateLift - stageCamY);
        }

        // Golpe del jugador al soldier.
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (!enemyCanBeHit(&enemies[i])) continue;
            s16 ex = getEnemyCenterX(&enemies[i]);
            s16 ey = getEnemyCenterY(&enemies[i]);
            Player* att = NULL;
            s16 dmg = 0;
            for (u8 k = 0; k < nPl; k++) {
                if (!playerAttackHitsEnemy(pls[k], &enemies[i])) continue;   // (02/10) hurtbox por tipo
                dmg = isPlayerSpecialAttack(pls[k]) ? ENEMY_HP : 1; att = pls[k];
                break;
            }
            if (dmg > 0) {
                damageEnemy(&enemies[i], dmg);          // (16/10) suena hit_foot adentro
                if (att && enemies[i].state == ENEMY_STATE_DEAD) {
                    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                                   SOUND_PCM_CH3, 15, FALSE, FALSE);
                    addPlayerScore(att, 1);
                    // (27/09) El golpe que lo mata lo lanza (ver ENEMY_DEATH_PUSH).
                    enemyDeathPush(&enemies[i], (s16)(att->x + PLAYER_SPRITE_W / 2),
                                   getPlayerDir(att), isPlayerSpecialAttack(att));
                }
            }
        }

        // Golpe del soldier al jugador.
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            Enemy* e = &enemies[i];
            if (e->state != ENEMY_STATE_ATTACK) continue;
            for (u8 k = 0; k < nPl; k++) {
                if (!enemyCanHitPlayer(e, pls[k])) continue;   // (15/10)
                if (!enemyTryHitPlayerBox(e, getPlayerHurtX(pls[k]), getPlayerY(pls[k]),
                                          PLAYER_BODY_HALF_W)) continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                               SOUND_PCM_CH3, 15, FALSE, FALSE);
                enemyDamagePlayer(e, pls[k]);
                break;
            }
        }

        // Shurikens del naranja.
        shurikenUpdate(cameraX);
        for (u8 k = 0; k < nPl; k++)
            if (shurikenBreakByPlayerAttack(pls[k]))
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                               SOUND_PCM_CH3, 15, FALSE, FALSE);
        {
            s16 hitX = 0;
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                if (!shurikenCheckHitPlayer(getPlayerWorldX(pls[k]), getPlayerY(pls[k]), &hitX))
                    continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                               SOUND_PCM_CH3, 15, FALSE, FALSE);
                damagePlayer(pls[k], hitX);
            }
        }

        // Boomerangs del amarillo (29/09).
        if (boomerangStep(pls, nPl, cameraX, stageCamY))
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                           SOUND_PCM_CH3, 15, FALSE, FALSE);

        // --- Lo propio del nivel (29/09: los misiles del agua del sewer) -----
        if (L->levelUpdate) L->levelUpdate(pls, nPl, cameraX);

        // --- Jefe ------------------------------------------------------------
        bool wavesDone = (L->nWaves == 0) ||
                         (waveIdx == (s16)L->nWaves - 1 &&
                          waveSpawned >= waveTot && alive == 0);
        if (!bossStarted && wavesDone && cameraX >= camMaxX &&
            leadFeetX >= L->bossFeetX) {
            bossStarted = TRUE;
            ledgeOff    = TRUE;    // (29/09) con el jefe, sin escalon
            if (L->bossStart) {
                // (01/10) Con bossMusicByVo el nivel arranca el tema el mismo,
                // despues de la voz del jefe (bossVoStart).
                if (L->bossMusic && !L->bossMusicByVo) {
                    XGM2_setLoopNumber(-1);
                    playMusicVol(L->bossMusic, L->bossMusicVol);
                }
                L->bossStart(cameraX, L->levelW);
            }
        }
        if (bossStarted) {
            if (!L->bossUpdate || L->bossUpdate(pls, nPl, cameraX)) {
                win = TRUE;
                running = FALSE;
            }
        }

        // --- Fondo -----------------------------------------------------------
        {
            // (12/10) Con el fondo corriendo solo se pasa el scroll u16 tal
            // cual (stage_bg lo trata como u16 con la imagen repetida).
            s16 bgX = L->autoScrollQ ? (s16)stageScroll : cameraX;
            sbgUpdate(bgX);
            fgUpdate(cameraX);
            fgTopUpdate(cameraX);
            if (L->bands) bandsUpdate(bgX);    // (22/10) parallax
            applyScroll(bgX, DMA_QUEUE);
        }

        SPR_update();
        SYS_doVBlankProcess();
    }

    if (win && !allOut) {
        for (u8 k = 0; k < nPl; k++) playerPersistSave(pls[k]);
        XGM2_stop();
        XGM2_setLoopNumber(0);
        playMusicVol(music_scene_clear, VOL_MUSIC_CLEAR);
        u16 hold = (IS_PAL_SYSTEM ? 50 : 60) * STAGE_CLEAR_SECS;
        while (hold > 0) { hold--; SYS_doVBlankProcess(); }
        XGM2_stop();
        XGM2_setLoopNumber(-1);
    }

    shurikenReleaseAll();
    boomerangReleaseAll();
    splashRelease();
    sbgSetWrap(FALSE);                 // (12/10)
    playerSkateLift = 0;
    enemySkateMode  = FALSE;
    if (L->levelRelease) L->levelRelease();
    if (L->bossRelease) L->bossRelease();
    VDP_setTextPriority(0);
    VDP_setTextPalette(PAL0);
    VDP_setTextPlane(BG_A);
    clearScene();          // tambien vuelve el scroll a modo plano y el HUD a BG_A
    stageCamY = 0;
    stageAOffX = 0; stageAOffY = 0;    // (21/10)
    pauseSetVScroll(0);
    cur = NULL;
    if (jump != PAUSE_NO_JUMP) return jump;
    if (win && !allOut) return L->nextScene;
    return SCENE_GAME_OVER;
}
