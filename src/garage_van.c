// ===========================================================================
// garage_van.c - la camioneta de las tortugas, fin del nivel 4 (30/10)
// Ver garage_van.h.
// ===========================================================================
#include <genesis.h>
#include "garage_van.h"
#include "level4_1.h"          // van_tiles, van_map, pal_van, garage_cone (rescomp, NEAR)
#include "level4_1_van.h"      // VAN_TILES, VAN_COLS... (generado)
#include "stage_level.h"       // stageAOffX/Y, stageScriptMask
#include "enemy.h"             // sprVramFits, sprDefragLock
#include "audio.h"             // car_sfx, capsule_door_sfx

// --- Pantalla (320x224, camara clavada en el maximo) -----------------------
#define VAN_PAL           PAL2     // los jefes y la capsula ya no estan
#define VAN_PARK_X        20       // x en pantalla del borde izquierdo, parada (captura 22 - 2)
#define VAN_DOOR_CX      100       // centro del hueco de la puerta desde ese borde
#define VAN_DOOR_FEET_Y  138       // pies de la tortuga ya adentro del hueco (mundo)
#define VAN_WAIT_Y       156       // lane donde esperan frente a la puerta (mundo)
#define VAN_WAIT_DY        8       // la de indice impar, un poco mas abajo
#define VAN_WAIT_DX       18       // separacion entre tortugas frente a la puerta
#define VAN_APRIL_VX     (-75)     // desde este x la camioneta ya tapa a April

// --- Tiempos (frames de juego; ESTIMADOS: las capturas no tienen reloj) -----
#define VAN_VRAM_T       120       // espera maxima por VRAM libre
#define VAN_LOAD_CHUNK    48       // tiles por frame (1,5 KB de DMA)
#define VAN_SETTLE_MAX   120
#define VAN_KNOT_F         6       // frames entre dos puntos de la tabla de entrada
#define VAN_ARRIVE_T      10       // parada, antes de abrir la puerta
#define VAN_DOOR_STEP_F    5       // frames por paso de la puerta (3 pasos)
#define VAN_BOARD_MAX    420       // tope para que suban todas
#define VAN_JUMP_F        18       // salto al hueco
#define VAN_JUMP_PEAK     18
#define VAN_JUMP_GAP      14       // entre una tortuga y la siguiente
#define VAN_INSIDE_T      12       // todas adentro, antes de cerrar
#define VAN_CLOSED_T      12       // puerta cerrada, antes de vibrar
#define VAN_BUMP_CYCLE    12       // una vibracion
#define VAN_BUMP_ON        4       //   (frames desplazada)
#define VAN_BUMPS          2
#define VAN_GO_T          10       // despues de vibrar, antes de arrancar
#define VAN_HOLD_T        40       // se fue: pantalla libre antes de cerrar el nivel
#define VAN_BUMP_DX        2
#define VAN_BUMP_DY       (-2)

// x de la camioneta en cada punto de la entrada (pantalla): medidos de las
// capturas 1679..1687 (-2 de la captura a pantalla), el primero es de antes de
// que se vea (fuera de pantalla).
static const s16 enterX[] = { -268, -208, -148, -106, -68, -38, -14, 4, 14, 20 };
#define VAN_KNOTS ((u16)(sizeof(enterX) / sizeof(enterX[0])))

enum { VS_IDLE = 0, VS_VRAM, VS_LOAD, VS_SETTLE, VS_ENTER, VS_OPEN, VS_BOARD,
       VS_CLOSE, VS_SHAKE, VS_LEAVE, VS_HOLD, VS_DONE };
enum { PS_WALK = 0, PS_READY, PS_JUMP, PS_GONE };

static u8             st;
static u16            t;
static SpriteDefinition holdDef;       // copia de garage_cone con maxNumTile = VAN_TILES
static Sprite*        hold;            // sprite oculto que reserva los tiles
static u16            base;            // primer tile de VRAM
static bool           locked;
static u16            loaded;
static u8             doorSt;          // 0 cerrada .. 3 abierta
static bool           colOn[VAN_COLS]; // columnas dibujadas en el plano
static s16            vx;              // x en pantalla del borde izquierdo
static s16            shx, shy;        // vibracion
static bool           aprCov;
static u8             pst[MAX_PLAYERS];
static u8             nextJump;
static u16            jumpT;
static u16            insideT;
static bool           reloadPending;   // un defrag de afuera mudo el bloque

// ---------------------------------------------------------------------------
static u16 baseAttr(void) {
    return (u16)TILE_ATTR_FULL(VAN_PAL, FALSE, FALSE, FALSE, base);
}

static void drawRect(u8 state, u16 c0, u16 r0, u16 w, u16 h) {
    const u16* m = (const u16*) van_map + (u16)(state * VAN_ROWS * VAN_COLS)
                   + (u16)(r0 * VAN_COLS) + c0;
    VDP_setTileMapDataRectEx(BG_A, m, baseAttr(), c0, r0, w, h, VAN_COLS, CPU);
}

// El plano mide 512 px: la camioneta (240) parcialmente fuera de pantalla
// reaparece del otro lado. Con la camioneta en la columna 0 del plano, el
// pixel u de la camioneta se ve en x = (u + vx) mod 512. Sobre el borde
// izquierdo (vx < -192) las primeras columnas aparecerian a la derecha; sobre
// el derecho (vx > 272) las ultimas, a la izquierda: se blanquean (estan fuera
// de pantalla donde corresponde, no se pierde nada).
static void clipUpdate(void) {
    if (reloadPending) return;            // los tiles todavia no estan en su lugar
    s16 cutL = (vx < -192) ? (s16)((-vx - 192 + 7) >> 3) : 0;
    s16 cutR = (vx > 272) ? (s16)((512 - vx) >> 3) : (s16)VAN_COLS;
    if (cutL > VAN_COLS) cutL = VAN_COLS;
    if (cutR < 0) cutR = 0;
    for (u16 c = 0; c < VAN_COLS; c++) {
        bool want = (bool)((s16)c >= cutL && (s16)c < cutR);
        if (want == colOn[c]) continue;
        colOn[c] = want;
        if (want) drawRect(doorSt, c, 0, 1, VAN_ROWS);
        else      VDP_clearTileMapRect(BG_A, c, 0, 1, VAN_ROWS);
    }
}

static void setDoor(u8 s) {
    if (s == doorSt) return;
    doorSt = s;
    // la puerta solo se mueve con la camioneta parada: toda en pantalla
    drawRect(doorSt, VAN_DOOR_C0, VAN_DOOR_R0, VAN_DOOR_W, VAN_DOOR_H);
}

// Con BG_A = camioneta en la columna 0 y scroll de plano -camX + stageAOffX:
// stageAOffX = vx + camX deja el borde izquierdo en vx (camX se cancela).
static void scrollSet(s16 camX) {
    stageAOffX = (s16)(vx + shx + camX);
    stageAOffY = (s16)(VAN_WORLD_Y0 + shy);
}

// ---------------------------------------------------------------------------
void vanInit(void) {
    st = VS_IDLE; t = 0;
    hold = NULL; base = 0; locked = FALSE; loaded = 0;
    doorSt = 0;
    for (u16 c = 0; c < VAN_COLS; c++) colOn[c] = FALSE;
    vx = enterX[0]; shx = shy = 0;
    aprCov = FALSE;
    for (u8 k = 0; k < MAX_PLAYERS; k++) pst[k] = PS_WALK;
    nextJump = 0; jumpT = 0; insideT = 0; reloadPending = FALSE;
}

void vanRelease(void) {
    if (hold) { SPR_releaseSprite(hold); hold = NULL; }
    if (locked) { if (sprDefragLock) sprDefragLock--; locked = FALSE; }
    st = VS_IDLE;
    stageAOffX = 0; stageAOffY = 0;
}

bool vanAprilCovered(void) { return aprCov; }

// Reserva VRAM de sprites para los tiles con un sprite oculto (su definicion
// es la del cono con maxNumTile = VAN_TILES; sin carga automatica de tiles).
static bool vanReserve(void) {
    if (!sprVramFits(VAN_TILES) || SPR_getFreeVRAM() < VAN_TILES) return FALSE;
    holdDef = garage_cone;
    holdDef.maxNumTile = VAN_TILES;
    hold = SPR_addSpriteExSafe(&holdDef, 0, -128, TILE_ATTR(VAN_PAL, FALSE, FALSE, FALSE),
                               SPR_FLAG_AUTO_VRAM_ALLOC);
    if (!hold) return FALSE;
    SPR_setVisibility(hold, HIDDEN);
    base = (u16)(hold->attribut & TILE_INDEX_MASK);
    sprDefragLock++;                   // que sprVramFits no la mude
    locked = TRUE;
    return TRUE;
}

// Sube los tiles por la cola de DMA, VAN_LOAD_CHUNK por frame. TRUE al terminar.
static bool loadStep(void) {
    u16 n = (u16)(VAN_TILES - loaded);
    if (n > VAN_LOAD_CHUNK) n = VAN_LOAD_CHUNK;
    if (n) {
        if (!DMA_canQueue(DMA_VRAM, (u16)(n * 16))) return FALSE;
        const u32* src = (const u32*) van_tiles + (u16)(loaded * 8);
        if (!DMA_queueDma(DMA_VRAM, (void*) src, (u16)((base + loaded) * 32), (u16)(n * 16), 2))
            return FALSE;
        loaded = (u16)(loaded + n);
    }
    return (bool)(loaded >= VAN_TILES);
}

// Si algun SPR_defragVRAM de afuera mudo el bloque, los tiles se cargan de nuevo
// (y las columnas se vuelven a dibujar) en el lugar nuevo.
static void vanCheckMoved(void) {
    if (!hold) return;
    u16 now = (u16)(hold->attribut & TILE_INDEX_MASK);
    if (now == base) return;
    base = now;
    loaded = 0;
    for (u16 c = 0; c < VAN_COLS; c++) colOn[c] = FALSE;
    VDP_clearTileMapRect(BG_A, 0, 0, VAN_COLS, VAN_ROWS);
    if (st > VS_LOAD) reloadPending = TRUE;
}

static bool settled(Player* p) {
    if (isPlayerGameOver(p) || !p->sprite) return TRUE;
    return (bool)((p->state == STATE_IDLE || p->state == STATE_WALKING) && !isPlayerJumping(p));
}

static void playersStand(Player** pls, u8 nPl) {
    for (u8 k = 0; k < nPl; k++) {
        if (pst[k] == PS_GONE) continue;
        playerCutsceneStand(pls[k]);
    }
}

// Las tortugas caminan frente a la puerta y saltan adentro de a una.
// TRUE cuando ya no queda ninguna.
static bool boardStep(Player** pls, u8 nPl, s16 camX) {
    s16 doorWX = (s16)(camX + VAN_PARK_X + VAN_DOOR_CX);
    bool allGone = TRUE;
    jumpT++;
    for (u8 k = 0; k < nPl; k++) {
        Player* p = pls[k];
        switch (pst[k]) {
        case PS_WALK: {
            s16 sx = (s16)(doorWX + (k * 2 - (nPl - 1)) * (VAN_WAIT_DX / 2));
            s16 sy = (s16)(VAN_WAIT_Y + (k & 1) * VAN_WAIT_DY);
            if (playerCutsceneWalkTo(p, (s16)(sx - PLAYER_SPRITE_W / 2), sy)) pst[k] = PS_READY;
            break;
        }
        case PS_READY:
            playerCutsceneStand(p);
            break;
        case PS_JUMP:
            if (playerExitJumpStep(p)) {
                if (p->sprite) SPR_setVisibility(p->sprite, HIDDEN);
                pst[k] = PS_GONE;
            }
            break;
        default:
            break;
        }
        if (pst[k] != PS_GONE) allGone = FALSE;
    }
    // el turno: la siguiente que ya espera frente a la puerta, pasado el hueco
    if (nextJump < nPl) {
        u8 k = nextJump;
        bool busy = FALSE;
        for (u8 j = 0; j < nPl; j++) if (pst[j] == PS_JUMP) busy = TRUE;
        if (pst[k] == PS_GONE) {
            nextJump++;
        } else if (pst[k] == PS_READY && !busy && jumpT >= VAN_JUMP_GAP) {
            Player* p = pls[k];
            s16 tx = (s16)(doorWX + (k * 2 - (nPl - 1)) * 6 - PLAYER_SPRITE_W / 2);
            playerExitJumpStart(p, tx, VAN_DOOR_FEET_Y, VAN_JUMP_F, VAN_JUMP_PEAK);
            pst[k] = PS_JUMP;
            nextJump++;
            jumpT = 0;
        }
    }
    return allGone;
}

// ---------------------------------------------------------------------------
bool vanUpdate(Player** pls, u8 nPl, s16 camX) {
    if (nPl > MAX_PLAYERS) nPl = MAX_PLAYERS;
    switch (st) {
    case VS_IDLE:
        st = VS_VRAM; t = 0;
        /* fallthrough */
    case VS_VRAM:
        if (!hold && !vanReserve()) {
            if (++t >= VAN_VRAM_T) { st = VS_DONE; return TRUE; }   // sin lugar: sin camioneta
            return FALSE;
        }
        PAL_setPalette(VAN_PAL, pal_van.data, DMA);
        loaded = 0;
        st = VS_LOAD;
        scrollSet(camX);
        return FALSE;

    case VS_LOAD:
        vanCheckMoved();
        scrollSet(camX);
        if (!loadStep()) return FALSE;
        for (u8 k = 0; k < nPl; k++) {
            pst[k] = (isPlayerGameOver(pls[k]) || !pls[k]->sprite) ? PS_GONE : PS_WALK;
        }
        clipUpdate();
        st = VS_SETTLE; t = 0;
        return FALSE;

    default:
        break;
    }

    vanCheckMoved();
    if (reloadPending && loadStep()) {
        reloadPending = FALSE;
        clipUpdate();                       // vuelve a dibujar las columnas
    }

    switch (st) {
    case VS_SETTLE: {
        bool all = TRUE;
        for (u8 k = 0; k < nPl; k++) if (!settled(pls[k])) all = FALSE;
        if (!all && ++t < VAN_SETTLE_MAX) break;
        for (u8 k = 0; k < nPl; k++) {
            pls[k]->state = STATE_IDLE;
            pls[k]->jumpZ = 0; pls[k]->jumpZq = 0;
        }
        stageScriptMask = 0x0F;
        XGM2_playPCMEx(car_sfx, sizeof(car_sfx), SOUND_PCM_CH3, 14, TRUE, FALSE);   // (placeholder)
        st = VS_ENTER; t = 0;
        break;
    }
    case VS_ENTER: {
        playersStand(pls, nPl);
        u16 seg = (u16)(t / VAN_KNOT_F);
        u16 f   = (u16)(t % VAN_KNOT_F);
        if (seg >= VAN_KNOTS - 1) {
            vx = VAN_PARK_X;
            st = VS_OPEN; t = 0;
        } else {
            s16 a = enterX[seg], b = enterX[seg + 1];
            vx = (s16)(a + (s16)(((s16)(b - a) * (s16)f) / VAN_KNOT_F));
            t++;
        }
        if (vx >= VAN_APRIL_VX) aprCov = TRUE;
        clipUpdate();
        break;
    }
    case VS_OPEN: {
        playersStand(pls, nPl);
        t++;
        if (t == VAN_ARRIVE_T) {
            setDoor(1);
            XGM2_playPCMEx(capsule_door_sfx, sizeof(capsule_door_sfx),
                           SOUND_PCM_CH3, 12, TRUE, FALSE);                          // (placeholder)
        }
        else if (t == VAN_ARRIVE_T + VAN_DOOR_STEP_F)     setDoor(2);
        else if (t == VAN_ARRIVE_T + 2 * VAN_DOOR_STEP_F) setDoor(3);
        else if (t >= VAN_ARRIVE_T + 3 * VAN_DOOR_STEP_F) {
            st = VS_BOARD; t = 0; jumpT = VAN_JUMP_GAP; nextJump = 0;
            while (nextJump < nPl && pst[nextJump] == PS_GONE) nextJump++;
        }
        break;
    }
    case VS_BOARD: {
        t++;
        bool all = boardStep(pls, nPl, camX);
        if (!all && t >= VAN_BOARD_MAX) {          // alguna no llego: se la lleva igual
            for (u8 k = 0; k < nPl; k++) {
                if (pst[k] != PS_GONE && pls[k]->sprite) SPR_setVisibility(pls[k]->sprite, HIDDEN);
                pst[k] = PS_GONE;
            }
            all = TRUE;
        }
        if (all) { st = VS_CLOSE; t = 0; }
        break;
    }
    case VS_CLOSE: {
        t++;
        if (t == VAN_INSIDE_T) {
            setDoor(2);
            XGM2_playPCMEx(capsule_door_sfx, sizeof(capsule_door_sfx),
                           SOUND_PCM_CH3, 12, TRUE, FALSE);                          // (placeholder)
        }
        else if (t == VAN_INSIDE_T + VAN_DOOR_STEP_F)     setDoor(1);
        else if (t == VAN_INSIDE_T + 2 * VAN_DOOR_STEP_F) setDoor(0);
        else if (t >= VAN_INSIDE_T + 2 * VAN_DOOR_STEP_F + VAN_CLOSED_T) { st = VS_SHAKE; t = 0; }
        break;
    }
    case VS_SHAKE: {
        u16 ph = (u16)(t % VAN_BUMP_CYCLE);
        bool on = (bool)(ph < VAN_BUMP_ON && t < VAN_BUMPS * VAN_BUMP_CYCLE);
        shx = on ? VAN_BUMP_DX : 0;
        shy = on ? VAN_BUMP_DY : 0;
        t++;
        if (t >= VAN_BUMPS * VAN_BUMP_CYCLE + VAN_GO_T) {
            shx = shy = 0;
            st = VS_LEAVE; t = 0;
            XGM2_playPCMEx(car_sfx, sizeof(car_sfx), SOUND_PCM_CH3, 14, TRUE, FALSE);   // (placeholder)
        }
        break;
    }
    case VS_LEAVE: {
        t++;
        vx = (s16)(VAN_PARK_X + (s16)(((u32) t * t) >> 1));    // arranca y acelera
        clipUpdate();
        if (vx >= 330) { st = VS_HOLD; t = 0; }
        break;
    }
    case VS_HOLD:
        if (++t >= VAN_HOLD_T) { st = VS_DONE; }
        break;
    default:
        break;
    }
    scrollSet(camX);
    return (bool)(st == VS_DONE);
}
