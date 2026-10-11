#include "street_2_1.h"
#include "enemy.h"    // sprVramFits
#include "audio.h"    // scream_april

// ===========================================================================
// UTILERIA DE LA CALLE del 2-1 -- ver street_2_1.h
// ===========================================================================
// COMO SE MIDIO (11/10). gameplayARCADE/tmnt.avi es 320x224 sin escalar, asi
// que cada frame del video calza 1:1 dentro del PNG del nivel (el rip
// "511372.png", mismas coordenadas que "Stage 2-_16_colors_v2.png"). Con un
// template matching del frame entero (sin el HUD) sale la camara del arcade
// en cada frame, y con la camara, la posicion de mundo de cualquier cosa que
// se vea en pantalla. Los numeros de abajo son eso:
//
//   ventana del frente   se rompe en el frame 11272 (el morado que cae
//                        pateando al empezar la scene)
//   Robodyne 1/2/3       17476 / 17488 / 17800 (bajando por la esquina)
//   edificio K 1/2       18207 / 18231
//   hidrante 1           x 427..455, pies 182 (frente a "Electronics")
//   hidrante 2           x 1275..1303, pies 234
//   cartel               colgado en (1964,416); se cae en el 19045 y queda
//                        tirado en (1970,552)
//   chica en skate       entra por la derecha con la camara clavada en 1179,
//                        pies en y=195, 2 px por frame hacia la izquierda
//
// OJO con la Robodyne 3 (1688,176): el fondo de 16 colores YA la tiene rota
// (el rip se saco despues de que el soldier pasara). El sprite se pone igual
// cuando entra el soldier; no cambia nada a la vista.
// ===========================================================================

#define SCREEN_W  320
#define SCREEN_H  224
#define HUD_H      40    // la franja del HUD (los marcos llegan a ~32)

// ---------------------------------------------------------------------------
// Ventanas
// ---------------------------------------------------------------------------
typedef struct { s16 x, y; u8 side; u8 anim; } WinDef;

static const WinDef winDef[L21W_COUNT] = {
    {  238,  33, 0, 0 },   // frente (edificio del principio)
    { 1624, 112, 1, 0 },   // Robodyne
    { 1656, 144, 1, 0 },
    { 1688, 176, 1, 0 },   // (ya rota en el fondo, ver arriba)
    { 1752, 240, 1, 1 },   // edificio K
    { 1784, 272, 1, 1 },
};
#define WIN_FRONT_W  38
#define WIN_FRONT_H  74
#define WIN_SIDE_W   20
#define WIN_SIDE_H   75
#define WIN_MARGIN   48    // px fuera de pantalla con el sprite todavia creado

static bool    winBroken[L21W_COUNT];
static Sprite* winSpr[L21W_COUNT];

void streetWindowRect(u8 w, s16* x, s16* y, s16* wd, s16* ht) {
    *x  = winDef[w].x;
    *y  = winDef[w].y;
    *wd = winDef[w].side ? WIN_SIDE_W : WIN_FRONT_W;
    *ht = winDef[w].side ? WIN_SIDE_H : WIN_FRONT_H;
}

bool streetWindowVisible(u8 w, s16 camX, s16 camY) {
    s16 x, y, wd, ht;
    streetWindowRect(w, &x, &y, &wd, &ht);
    s16 sx = (s16)(x - camX), sy = (s16)(y - camY);
    // Entera en X y con el alfeizar por encima del borde de abajo. Si la
    // camara ya bajo tanto que la ventana quedo detras del HUD, igual vale:
    // el soldier arranca justo debajo del HUD (ver L21K_WINDOW en level2_1.c).
    return sx >= 0 && sx + wd <= SCREEN_W && sy + ht <= SCREEN_H - 8;
}

void streetBreakWindow(u8 w) {
    if (w < L21W_COUNT) winBroken[w] = TRUE;
}

static void winRelease(u8 w) {
    if (winSpr[w]) SPR_releaseSprite(winSpr[w]);
    winSpr[w] = NULL;
}

static void winUpdate(s16 camX, s16 camY) {
    for (u8 w = 0; w < L21W_COUNT; w++) {
        if (!winBroken[w]) continue;
        s16 sx = (s16)(winDef[w].x - camX), sy = (s16)(winDef[w].y - camY);
        bool near = sx > -WIN_MARGIN && sx < SCREEN_W + WIN_MARGIN &&
                    sy > -(WIN_FRONT_H + WIN_MARGIN) && sy < SCREEN_H + WIN_MARGIN;
        if (!near) { winRelease(w); continue; }
        if (!winSpr[w]) {
            const SpriteDefinition* def = winDef[w].side ? &l21_win_side : &l21_win_front;
            if (!sprVramFits(def->maxNumTile)) continue;    // se reintenta
            winSpr[w] = SPR_addSprite(def, 0, 0, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
            if (!winSpr[w]) continue;
            SPR_setAutoAnimation(winSpr[w], FALSE);
            SPR_setAnimAndFrame(winSpr[w], winDef[w].anim, 0);
            SPR_setDepth(winSpr[w], SPR_MAX_DEPTH);       // pegada a la pared
        }
        SPR_setPosition(winSpr[w], sx, sy);
    }
}

// ---------------------------------------------------------------------------
// Hidrantes
// ---------------------------------------------------------------------------
// L = borde izquierdo del cuerpo (lo naranja), top = su borde de arriba.
// En las celdas el cuerpo esta en x=5; en el quieto arriba en y=2, en las del
// rocio en y=20. Espejado, el cuerpo (26 px) queda al otro lado de la celda.
#define HYD_COUNT        2
#define HYD_BODY_W      26
#define HYD_BODY_H      45     // de top a los pies
#define HYD_SPRAY_W     80
#define HYD_JET_W      112
#define HYD_JET_DY      12     // top de la celda del chorro, respecto de top
#define HYD_SPRAY_TICKS  6     // frames por cuadro del rocio
#define HYD_JET_SPEED    6
#define HYD_HIT_HALF_W  10     // hurtbox del hidrante para el golpe
#define HYD_BODY_HIT_H  44
#define HYD_BLOCK_HALF_W 10    // base solida
#define HYD_BLOCK_HALF_D  5
#define HYD_JET_LANE_TOL 20
#define HYD_ARM_MARGIN  64
// (11/10) Hacen falta DOS golpes para abrirlo. El primero solo lo sacude.
// Un golpe cuenta en el flanco: mientras el mismo ataque siga tocandolo no
// vuelve a contar (si no, un solo swing de varios frames lo abria).
#define HYD_HITS_TO_OPEN  2
#define HYD_SHAKE_TICKS   8

static const s16 hydL[HYD_COUNT]   = {  430, 1278 };
static const s16 hydTop[HYD_COUNT] = {  137,  189 };

typedef enum { HYD_IDLE = 0, HYD_SPRAY } HydState;

static struct {
    u8      state;
    u8      hits;         // golpes recibidos estando cerrado
    u8      latch;        // bit por tortuga: su ataque ya lo estaba tocando
    u8      shake;        // frames de sacudon que quedan (primer golpe)
    s8      dir;          // +1 tira el agua a la derecha
    s8      owner;        // tortuga que lo abrio (puntaje)
    Sprite* spr;          // quieto o rocio segun el estado
    bool    sprIsSpray;
    u16     tick;
    // chorro con la tapa
    bool    jetOn;
    s16     jetX;         // borde izquierdo de la celda (mundo)
    Sprite* jetSpr;
    u8      jetMask;      // soldiers ya alcanzados (bit por indice)
} hyd[HYD_COUNT];

static inline s16 hydFeet(u8 i) { return (s16)(hydTop[i] + HYD_BODY_H); }
static inline s16 hydCX(u8 i)   { return (s16)(hydL[i] + HYD_BODY_W / 2); }

static void hydRelease(u8 i) {
    if (hyd[i].spr) SPR_releaseSprite(hyd[i].spr);
    hyd[i].spr = NULL;
}
static void hydJetRelease(u8 i) {
    if (hyd[i].jetSpr) SPR_releaseSprite(hyd[i].jetSpr);
    hyd[i].jetSpr = NULL;
}

bool streetBlock(s16 fx, s16 fy) {
    for (u8 i = 0; i < HYD_COUNT; i++) {
        if (abs(fx - hydCX(i)) > HYD_BLOCK_HALF_W) continue;
        if (abs(fy - hydFeet(i)) > HYD_BLOCK_HALF_D) continue;
        return TRUE;
    }
    return FALSE;
}

static void hydUpdate(s16 camX, s16 camY) {
    for (u8 i = 0; i < HYD_COUNT; i++) {
        s16 sx = (s16)(hydL[i] - camX);
        bool near = sx > -(HYD_SPRAY_W + HYD_ARM_MARGIN) &&
                    sx < SCREEN_W + HYD_ARM_MARGIN;
        bool spray = (hyd[i].state == HYD_SPRAY);

        if (!near) {
            hydRelease(i);
        } else {
            // El rocio cuesta ~67 tiles: si no entra, se sigue viendo el
            // quieto y se reintenta (el agua "sale" en cuanto haya lugar).
            if (spray && hyd[i].spr && !hyd[i].sprIsSpray &&
                sprVramFits(l21_hydrant_spray.maxNumTile)) {
                hydRelease(i);
            }
            if (!hyd[i].spr) {
                bool wantSpray = spray && sprVramFits(l21_hydrant_spray.maxNumTile);
                const SpriteDefinition* def = wantSpray ? &l21_hydrant_spray : &l21_hydrant;
                if (wantSpray || sprVramFits(def->maxNumTile))
                    hyd[i].spr = SPR_addSprite(def, 0, 0, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
                hyd[i].sprIsSpray = wantSpray && (hyd[i].spr != NULL);
                if (hyd[i].spr) {
                    SPR_setAutoAnimation(hyd[i].spr, FALSE);
                    SPR_setAnimAndFrame(hyd[i].spr, 0, 0);
                    SPR_setHFlip(hyd[i].spr, hyd[i].sprIsSpray && hyd[i].dir < 0);
                }
            }
            if (hyd[i].spr) {
                s16 x, y;
                if (hyd[i].sprIsSpray) {
                    SPR_setFrame(hyd[i].spr, (s16)((hyd[i].tick / HYD_SPRAY_TICKS) & 3));
                    x = (hyd[i].dir < 0) ? (s16)(hydL[i] + HYD_BODY_W - (HYD_SPRAY_W - 5))
                                         : (s16)(hydL[i] - 5);
                    y = (s16)(hydTop[i] - 20);
                } else {
                    x = (s16)(hydL[i] - 5);
                    y = (s16)(hydTop[i] - 2);
                    if (hyd[i].shake) {                 // sacudon del 1er golpe
                        x = (s16)(x + ((hyd[i].shake & 2) ? 1 : -1));
                        hyd[i].shake--;
                    }
                }
                SPR_setPosition(hyd[i].spr, (s16)(x - camX), (s16)(y - camY));
                SPR_setDepth(hyd[i].spr, (s16)(-hydFeet(i)));
            }
        }

        if (spray) hyd[i].tick++;

        // Chorro con la tapa: vuela en linea recta hasta salir de camara.
        if (hyd[i].jetOn) {
            hyd[i].jetX = (s16)(hyd[i].jetX + hyd[i].dir * HYD_JET_SPEED);
            s16 jx = (s16)(hyd[i].jetX - camX);
            if (jx < -(HYD_JET_W + 16) || jx > SCREEN_W + 16) {
                hyd[i].jetOn = FALSE;
                hydJetRelease(i);
            } else if (hyd[i].jetSpr) {
                SPR_setVisibility(hyd[i].jetSpr, VISIBLE);
                SPR_setPosition(hyd[i].jetSpr, jx,
                                (s16)(hydTop[i] + HYD_JET_DY - camY));
                SPR_setDepth(hyd[i].jetSpr, (s16)(-hydFeet(i) - 1));
            }
        }
    }
}

static void hydOpen(u8 i, s8 dir, s8 owner) {
    hyd[i].state = HYD_SPRAY;
    hyd[i].dir   = dir;
    hyd[i].owner = owner;
    hyd[i].tick  = 0;
    hydRelease(i);                      // el proximo update pone el rocio
    // La tapa sale desde el costado del cuerpo hacia donde va el agua.
    hyd[i].jetOn   = TRUE;
    hyd[i].jetMask = 0;
    hyd[i].jetX    = (dir > 0) ? (s16)(hydL[i] + 20)
                               : (s16)(hydL[i] + HYD_BODY_W - 20 - HYD_JET_W);
    hydJetRelease(i);
    if (sprVramFits(l21_hydrant_jet.maxNumTile)) {
        hyd[i].jetSpr = SPR_addSprite(&l21_hydrant_jet, 0, 0,
                                      TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
        if (hyd[i].jetSpr) {
            SPR_setAutoAnimation(hyd[i].jetSpr, FALSE);
            // El arte va hacia la DERECHA (tapa adelante): se espeja a la izq.
            SPR_setHFlip(hyd[i].jetSpr, dir < 0);
            SPR_setVisibility(hyd[i].jetSpr, HIDDEN);   // se ubica en el update
        }
    }
}

// ---------------------------------------------------------------------------
// Cartel
// ---------------------------------------------------------------------------
#define BILL_X        1964
#define BILL_Y         416
#define BILL_FALL_X   1972
#define BILL_FALL_Y0   472
#define BILL_DOWN_X   1970
#define BILL_DOWN_Y    552
#define BILL_FALL_SPEED  5
#define BILL_ARM_MARGIN 64
#define BILL_W        128
// Al caer golpea a la tortuga que esta debajo (una barra).
#define BILL_HIT_X0   1976
#define BILL_HIT_X1   2090
#define BILL_HIT_Y0    552
#define BILL_HIT_Y1    590

typedef enum { BILL_HANG = 0, BILL_FALL, BILL_DOWN } BillState;
static u8      billState;
static s16     billY;
static Sprite* billSpr;
static bool    billSprFlat;

static void billRelease(void) {
    if (billSpr) SPR_releaseSprite(billSpr);
    billSpr = NULL;
}

void streetBillboardFall(void) {
    if (billState != BILL_HANG) return;
    billState = BILL_FALL;
    billY     = BILL_FALL_Y0;
    billRelease();                       // el colgado se suelta en el acto
}

static void billUpdate(Player** pls, u8 nPl, s16 camX, s16 camY) {
    s16 x = (billState == BILL_HANG) ? BILL_X
          : (billState == BILL_FALL) ? BILL_FALL_X : BILL_DOWN_X;
    s16 y = (billState == BILL_HANG) ? BILL_Y
          : (billState == BILL_FALL) ? billY : BILL_DOWN_Y;

    if (billState == BILL_FALL) {
        billY = (s16)(billY + BILL_FALL_SPEED);
        if (billY >= BILL_DOWN_Y) {
            billState = BILL_DOWN;
            y = BILL_DOWN_Y; x = BILL_DOWN_X;
            for (u8 k = 0; k < nPl; k++) {
                s16 fx = (s16)(pls[k]->x + PLAYER_SPRITE_W / 2);
                if (fx < BILL_HIT_X0 || fx > BILL_HIT_X1) continue;
                if (pls[k]->y < BILL_HIT_Y0 || pls[k]->y > BILL_HIT_Y1) continue;
                if (!playerCanBeHit(pls[k])) continue;
                damagePlayer(pls[k], (s16)((BILL_HIT_X0 + BILL_HIT_X1) / 2));
            }
        }
    }

    s16 sx = (s16)(x - camX), sy = (s16)(y - camY);
    bool near = sx > -(BILL_W + BILL_ARM_MARGIN) && sx < SCREEN_W + BILL_ARM_MARGIN &&
                sy > -(64 + BILL_ARM_MARGIN) && sy < SCREEN_H + BILL_ARM_MARGIN;
    bool flat = (billState != BILL_HANG);
    if (!near || (billSpr && billSprFlat != flat)) billRelease();
    if (!near) return;
    if (!billSpr) {
        const SpriteDefinition* def = flat ? &l21_bill_flat : &l21_bill_front;
        if (!sprVramFits(def->maxNumTile)) return;
        billSpr = SPR_addSprite(def, 0, 0, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
        if (!billSpr) return;
        billSprFlat = flat;
        SPR_setAutoAnimation(billSpr, FALSE);
        SPR_setDepth(billSpr, SPR_MAX_DEPTH);   // pared / piso: todos por encima
    }
    if (flat) SPR_setAnimAndFrame(billSpr, 0, (billState == BILL_DOWN) ? 1 : 0);
    SPR_setPosition(billSpr, sx, sy);
}

// ---------------------------------------------------------------------------
// Chica en skate
// ---------------------------------------------------------------------------
// Celda de 56x64: la tabla arranca en x=2, los pies (la tabla) en la ultima
// fila. Paleta propia en PAL3: la usaba la TV de la vidriera, que esta 700 px
// antes y para cuando la chica entra ya termino (y Bebop, que tambien la usa,
// aparece mucho despues).
#define GIRL_W          56
#define GIRL_FEET      195
#define GIRL_SPEED       2
#define GIRL_HIT_HALF_W 12
#define GIRL_BODY_H     56

typedef enum { GIRL_OFF = 0, GIRL_RIDE, GIRL_DONE } GirlState;
static u8      girlState;
static bool    girlDown;      // agachada (le pegaron)
static s16     girlX;         // borde izquierdo de la celda (mundo)
static Sprite* girlSpr;

static void girlRelease(void) {
    if (girlSpr) SPR_releaseSprite(girlSpr);
    girlSpr = NULL;
}

void streetGirlStart(s16 camX) {
    if (girlState != GIRL_OFF) return;
    if (!sprVramFits(l21_skategirl.maxNumTile)) return;   // se reintenta
    girlSpr = SPR_addSprite(&l21_skategirl, 0, 0, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    if (!girlSpr) return;
    PAL_setPalette(PAL3, l21_skategirl.palette->data, DMA);
    SPR_setAutoAnimation(girlSpr, FALSE);
    SPR_setAnimAndFrame(girlSpr, 0, 0);
    girlX     = (s16)(camX + SCREEN_W);
    girlDown  = FALSE;
    girlState = GIRL_RIDE;
}

static void girlUpdate(s16 camX, s16 camY) {
    if (girlState != GIRL_RIDE) return;
    girlX = (s16)(girlX - GIRL_SPEED);
    s16 sx = (s16)(girlX - camX);
    if (sx < -(GIRL_W + 8)) {             // salio por la izquierda: no vuelve
        girlRelease();
        girlState = GIRL_DONE;
        return;
    }
    if (girlSpr) {
        SPR_setPosition(girlSpr, sx, (s16)(GIRL_FEET - 63 - camY));
        SPR_setDepth(girlSpr, (s16)(-GIRL_FEET));
    }
}

// ---------------------------------------------------------------------------
// Pizza (11/10)
// ---------------------------------------------------------------------------
// Medida en el video: la caja esta en la CORNISA (sobre los portones) con la
// celda en mundo (1238,46); la caja ocupa x 1242..1273 y apoya en y=92. La
// tortuga que sube y la toca (frame 16456) recupera toda la vida; la caja se
// abre (~8 frames por cuadro), queda abierta con "ENERGY" ~68 frames y
// desaparece (16544). En el video sigue ahi hasta que alguien sube: no tiene
// tiempo limite.
#define PIZZA_X          1238    // celda (mundo)
#define PIZZA_Y            46
#define PIZZA_BOX_X0     1242    // la caja
#define PIZZA_BOX_X1     1273
#define PIZZA_FEET         92
#define PIZZA_PICK_DX      14    // tolerancia en X de los pies
#define PIZZA_PICK_DY      14    // y en profundidad (la cornisa es y 96..103)
#define PIZZA_OPEN_TICKS    8    // frames 1 y 2
#define PIZZA_SHOW_TICKS   68    // frame 3 hasta desaparecer
#define PIZZA_MARGIN       48

typedef enum { PIZZA_WAIT = 0, PIZZA_OPEN, PIZZA_DONE } PizzaState;
static u8      pizzaState;
static u16     pizzaTick;
static Sprite* pizzaSpr;

// (11/10) "PIZZA TIME!": en el video el globo sale en el frame 16173 con la
// camara en 990 (la caja ya entera en pantalla) y la cola apuntando a la caja
// (globo en mundo x 1199..1262, y 65..96), 64 frames. Con el voice over.
#define PIZZA_TIME_CAM    990
#define PIZZA_BUB_X      1199
#define PIZZA_BUB_Y        65
#define PIZZA_BUB_T        64
static u8      pizzaBubSt;     // 0 = todavia no, 1 = en pantalla, 2 = ya fue
static u16     pizzaBubT;
static Sprite* pizzaBub;

static void pizzaBubOff(void) {
    if (pizzaBub) SPR_releaseSprite(pizzaBub);
    pizzaBub = NULL;
}

static void pizzaBubUpdate(s16 camX, s16 camY) {
    if (pizzaBubSt == 0) {
        if (pizzaState != PIZZA_WAIT || camX < PIZZA_TIME_CAM) return;
        if (!sprVramFits(pizza_time_bubble.maxNumTile)) return;   // se reintenta
        pizzaBub = SPR_addSprite(&pizza_time_bubble, 0, 0, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
        if (!pizzaBub) return;
        SPR_setDepth(pizzaBub, SPR_MIN_DEPTH);
        XGM2_playPCMEx(pizza_time_vo, sizeof(pizza_time_vo), SOUND_PCM_CH3, 15, FALSE, FALSE);
        pizzaBubSt = 1;
        pizzaBubT  = 0;
    }
    if (pizzaBubSt != 1) return;
    s16 sx = (s16)(PIZZA_BUB_X - camX), sy = (s16)(PIZZA_BUB_Y - camY);
    if (++pizzaBubT >= PIZZA_BUB_T || sx < -64 || sx > SCREEN_W) {
        pizzaBubOff();
        pizzaBubSt = 2;
        return;
    }
    if (sy < HUD_H - 6) sy = HUD_H - 6;
    SPR_setPosition(pizzaBub, sx, sy);
}

static void pizzaRelease(void) {
    if (pizzaSpr) SPR_releaseSprite(pizzaSpr);
    pizzaSpr = NULL;
}

static void pizzaUpdate(Player** pls, u8 nPl, s16 camX, s16 camY) {
    pizzaBubUpdate(camX, camY);
    if (pizzaState == PIZZA_DONE) return;

    if (pizzaState == PIZZA_WAIT) {
        for (u8 k = 0; k < nPl; k++) {
            Player* p = pls[k];
            if (isPlayerGameOver(p) || p->state == STATE_KO) continue;
            s16 fx = (s16)(p->x + PLAYER_SPRITE_W / 2);
            if (fx < PIZZA_BOX_X0 - PIZZA_PICK_DX || fx > PIZZA_BOX_X1 + PIZZA_PICK_DX) continue;
            if (abs(p->y - (PIZZA_FEET + 6)) > PIZZA_PICK_DY) continue;
            p->health  = PLAYER_MAX_HEALTH;          // toda la vida
            pizzaState = PIZZA_OPEN;
            pizzaTick  = 0;
            break;
        }
    } else if (++pizzaTick >= 2 * PIZZA_OPEN_TICKS + PIZZA_SHOW_TICKS) {
        pizzaRelease();
        pizzaState = PIZZA_DONE;
        return;
    }

    s16 sx = (s16)(PIZZA_X - camX), sy = (s16)(PIZZA_Y - camY);
    bool near = sx > -(40 + PIZZA_MARGIN) && sx < SCREEN_W + PIZZA_MARGIN &&
                sy > -(48 + PIZZA_MARGIN) && sy < SCREEN_H + PIZZA_MARGIN;
    if (!near) {
        pizzaRelease();
        if (pizzaState == PIZZA_OPEN) { pizzaState = PIZZA_DONE; }  // ya se fue de cuadro
        return;
    }
    if (!pizzaSpr) {
        if (!sprVramFits(l21_pizza.maxNumTile)) return;
        pizzaSpr = SPR_addSprite(&l21_pizza, 0, 0, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
        if (!pizzaSpr) return;
        SPR_setAutoAnimation(pizzaSpr, FALSE);
    }
    s16 f = 0;
    if (pizzaState == PIZZA_OPEN)
        f = (pizzaTick < PIZZA_OPEN_TICKS) ? 1 : (pizzaTick < 2 * PIZZA_OPEN_TICKS) ? 2 : 3;
    SPR_setAnimAndFrame(pizzaSpr, 0, f);
    SPR_setPosition(pizzaSpr, sx, sy);
    SPR_setDepth(pizzaSpr, (s16)(-PIZZA_FEET));
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
void streetInit(void) {
    for (u8 w = 0; w < L21W_COUNT; w++) { winBroken[w] = FALSE; winSpr[w] = NULL; }
    for (u8 i = 0; i < HYD_COUNT; i++) {
        hyd[i].state = HYD_IDLE;
        hyd[i].hits = 0;
        hyd[i].latch = 0;
        hyd[i].shake = 0;
        hyd[i].dir = 1;
        hyd[i].owner = -1;
        hyd[i].spr = NULL;
        hyd[i].sprIsSpray = FALSE;
        hyd[i].tick = 0;
        hyd[i].jetOn = FALSE;
        hyd[i].jetSpr = NULL;
        hyd[i].jetMask = 0;
    }
    billState = BILL_HANG;
    billY = BILL_Y;
    billSpr = NULL;
    billSprFlat = FALSE;
    girlState = GIRL_OFF;
    girlDown = FALSE;
    girlSpr = NULL;
    pizzaState = PIZZA_WAIT;
    pizzaTick = 0;
    pizzaSpr = NULL;
    pizzaBubSt = 0;
    pizzaBubT = 0;
    pizzaBub = NULL;
}

void streetReleaseAll(void) {
    for (u8 w = 0; w < L21W_COUNT; w++) winRelease(w);
    for (u8 i = 0; i < HYD_COUNT; i++) { hydRelease(i); hydJetRelease(i); hyd[i].jetOn = FALSE; }
    billRelease();
    girlRelease();
    pizzaRelease();
    pizzaBubOff();
}

void streetUpdate(Player** pls, u8 nPl, s16 camX, s16 camY) {
    winUpdate(camX, camY);
    hydUpdate(camX, camY);
    billUpdate(pls, nPl, camX, camY);
    girlUpdate(camX, camY);
    pizzaUpdate(pls, nPl, camX, camY);
}

bool streetPlayerHits(Player** pls, u8 nPl) {
    bool opened = FALSE;
    for (u8 k = 0; k < nPl; k++) {
        // Hidrantes: HYD_HITS_TO_OPEN golpes lo abren. El agua sale
        // ALEJANDOSE de la tortuga del ultimo golpe (mismo criterio que los
        // parquimetros: el lado en que esta, no hacia donde mira).
        u8 bit = (u8)(1 << k);
        for (u8 i = 0; i < HYD_COUNT; i++) {
            if (hyd[i].state != HYD_IDLE) continue;
            if (!playerAttackHitsBox(pls[k], hydCX(i), hydFeet(i),
                                     HYD_HIT_HALF_W, HYD_BODY_HIT_H)) {
                hyd[i].latch &= (u8)~bit;
                continue;
            }
            if (hyd[i].latch & bit) continue;      // el mismo golpe sigue tocando
            hyd[i].latch |= bit;
            opened = TRUE;                         // suena el golpe
            if (++hyd[i].hits < HYD_HITS_TO_OPEN) {
                hyd[i].shake = HYD_SHAKE_TICKS;
                continue;
            }
            s16 pcx = (s16)(pls[k]->x + PLAYER_SPRITE_W / 2);
            s8 dir = (hydCX(i) > pcx) ? 1 : (hydCX(i) < pcx) ? -1
                   : ((getPlayerDir(pls[k]) >= 0) ? 1 : -1);
            hydOpen(i, dir, (s8)k);
        }
        // La chica: un golpe y se agacha gritando. Una sola vez.
        if (girlState == GIRL_RIDE && !girlDown &&
            playerAttackHitsBox(pls[k], (s16)(girlX + GIRL_W / 2), GIRL_FEET,
                                GIRL_HIT_HALF_W, GIRL_BODY_H)) {
            girlDown = TRUE;
            if (girlSpr) SPR_setFrame(girlSpr, 1);
            // CH3: en CH2 van los golpes con prioridad 15 y lo cortarian.
            XGM2_playPCMEx(scream_april, sizeof(scream_april),
                           SOUND_PCM_CH3, 15, FALSE, FALSE);
        }
    }
    return opened;
}

bool streetHitEnemy(u16 enemyIdx, s16 ex, s16 ey, s16 halfW, s8* owner, bool* kill) {
    u8 ix  = (u8)(enemyIdx & 7);
    u8 bit = (u8)(1 << ix);
    for (u8 i = 0; i < HYD_COUNT; i++) {
        if (hyd[i].state != HYD_SPRAY) continue;
        // La tapa con el chorro: la punta (los 52 px de adelante) mata.
        if (hyd[i].jetOn && !(hyd[i].jetMask & bit) &&
            abs(ey - hydFeet(i)) <= HYD_JET_LANE_TOL) {
            s16 x0 = (hyd[i].dir > 0) ? (s16)(hyd[i].jetX + 60) : hyd[i].jetX;
            s16 x1 = (s16)(x0 + 52);
            if (ex + halfW >= x0 && ex - halfW <= x1) {
                hyd[i].jetMask |= bit;
                if (owner) *owner = hyd[i].owner;
                if (kill) *kill = TRUE;
                return TRUE;
            }
        }
        // (11/10) El rocio que queda despues es SOLO ESTETICO: no pega.
    }
    return FALSE;
}
