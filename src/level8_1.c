// ===========================================================================
// level8_1.c - Scene 8: el TECHNODROME (27/09)
// ===========================================================================
// Sexto nivel con arte del proyecto del companero (Ray Project): una L enorme
// de 4672x2816 px. Arriba un pasillo, despues un POZO en diagonal a 45 grados
// y abajo la sala donde espera el General Traag. El pozo se baja en ASCENSOR:
//
//   ARRIBA   camara solo en X (camY fija). Oleadas de foot soldiers con la
//            camara trabada, como siempre. Al final del pasillo esta el
//            ascensor: cuando TODAS las tortugas en juego se paran encima un
//            momento, arranca.
//   VIAJE    el ascensor baja en diagonal y la camara con el (4 px por frame
//            en X y en Y): en pantalla el ascensor queda quieto y el pozo
//            pasa. Las tortugas viajan encima, sin poder bajarse.
//   ABAJO    camY fija otra vez; una oleada al llegar y, al fondo de la sala,
//            Traag (traag.c).
//
// PLANOS
//   BG_B  el mapa (4672x2816, 2316 tiles unicos) streameado en 2D por
//         stage_bg.c (sbgInitRaw2D): ventana de 42x29 celdas que sigue a la
//         camara en los dos ejes sobre el plano circular de 64x32. El indice
//         0 del mapa es el vacio: se ve el color de fondo (negro).
//   BG_A  filas 0-3: el HUD. Filas 12-27: el ASCENSOR (techno_elevator.png,
//         320x128) dibujado en columnas de PANTALLA, con el scroll fino de
//         su X (se redibuja cuando cruza un tile). Su Y en pantalla nunca
//         cambia: 96 arriba, 96 durante el viaje (baja con la camara) y 96
//         abajo. El mapa original lo traia dibujado en su lugar de arranque;
//         tools/gen_level8_1.py lo borra (ver ahi).
//
// Musica: music_technodrome, "16 - The Technodrome (Scene 5)"; con Traag, music_boss.
//
// (18/10) Como en el arcade (video, seg_06):
//   - ANIMACION DE PALETA del mapa (techPal*): los rojos y el rosa/violeta
//     "respiran" (oscuro <-> encendido), el amarillo titila en ambar, las
//     lucecitas cian de los paneles cambian de color y el azul late. El gris
//     del piso y de las paredes no se toca.
//   - OLEADAS nuevas: blancos de espada al entrar, blancos + lanzas entre las
//     columnas, blancos del cuchillo (en el arcade son los del monociclo, que
//     no tienen arte), morados y lanzas al final del pasillo; abajo los del
//     fusil antes de Traag. Los proyectiles ya saben de camara vertical
//     (stageCamY), asi que las lanzas se tiran y los fusiles disparan.
//   - COLUMNAS con laser arriba y BOLAS en el viaje: techno_hazards.c.
//   PAL3 es la paleta del foot soldier BLANCO (blancos, cuchillos, columnas,
//   laser y bolas) hasta que entra Traag, que carga la suya.
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "difficulty.h"  // (20/10) foot soldiers extra por oleada
#include "idle_missile.h"   // (14/10) modo ARCADE: el misil al que se queda quieto
#include "level8_1.h"          // pal_techno, techno_tiles/map, techno_elevator
#include "level1.h"            // hud_font
#include "enemies.h"           // foot_soldier (paleta)
#include "player.h"
#include "hud.h"
#include "enemy.h"
#include "stage_bg.h"
#include "traag.h"
#include "techno_hazards.h"   // (18/10) columnas con laser y bolas del ascensor
#include "pause_menu.h"
#include "audio.h"

#ifndef IS_PAL_SYSTEM
#define IS_PAL_SYSTEM IS_PALSYSTEM
#endif

extern u8 cantidadJugadores;

#define SCREEN_W            320
#define HUD_ROWS              4
#define ROWS                 28

// --- Mapa ------------------------------------------------------------------
#define L8_MAP_W            584          // tiles
#define L8_MAP_H            352
#define L8_W               (L8_MAP_W * 8)    // 4672
#define L8_BG_SLOTS         512          // >= 487, el peor caso medido en el recorrido
#define L8_BACKDROP          11          // PAL0[11]: negro

// --- Arriba ------------------------------------------------------------------
#define L8_TOP_CAM_Y         32
#define L8_TOP_CAM_MAX_X   1348          // la camara no pasa de aca arriba
#define L8_TOP_LANE_T       128          // pies (mundo)
#define L8_TOP_LANE_B       248

// --- Ascensor ------------------------------------------------------------------
#define L8_ELEV_X0         1408          // esquina (mundo), donde lo dibujaba el mapa
#define L8_ELEV_Y0          128
#define L8_ELEV_W           320
#define L8_ELEV_H           128
#define L8_ELEV_ROW          12          // fila de pantalla (siempre la misma)
// (10/10) El piso del ascensor es un PARALELOGRAMO a 45 grados (medido en
// techno_elevator.png): en la fila local y (pies - elevY) va de
// x = L8_FLOOR_L0 - y a x = L8_FLOOR_R0 - y. Antes la zona de "parado
// encima" era un rectangulo chico (x 40..264, y 48..108): parado en la
// mitad de arriba del ascensor (pies a menos de 48 px de su borde, lo mas
// natural al entrar por el pasillo) no contaba, el ascensor no arrancaba
// nunca y la pantalla quedaba trabada sin ninguna pista de por que.
#define L8_FLOOR_L0         126
#define L8_FLOOR_R0         317
#define L8_FLOOR_T            4          // filas de pies validas (local)
#define L8_FLOOR_B          120
#define L8_FLOOR_MARGIN      12          // pies a esto del borde, como minimo
#define L8_ARM_FRAMES        36          // todos encima este tiempo -> arranca
#define L8_RIDE_SPEED         4
#define L8_RIDE_SLOW        160          // (18/10) px finales en los que frena
// (18/10) Mientras baja el ascensor suena elevator_tecnodrome_sfx en LOOP por
// CH2 (en el viaje no hay soldiers ni golpes que lo necesiten). Si algo le
// pisa el canal (un golpe al aire, el jingle de pausa va por CH3), se vuelve
// a disparar en el cuadro siguiente. Se corta al llegar abajo o al salir.
// (18/10) En el canal de las VOCES (CH2) con prioridad baja: una voz lo corta
// y el viaje lo relanza al terminar; los golpes (CH3) no lo tocan.
#define L8_ELEV_SFX_CH     SOUND_PCM_CH2
#define L8_ELEV_SFX_PRIO    6          // < 8: hasta los gritos de las tortugas lo cortan
static void elevSfxPlay(void) {
    XGM2_playPCMEx(elevator_tecnodrome_sfx, sizeof(elevator_tecnodrome_sfx),
                   L8_ELEV_SFX_CH, L8_ELEV_SFX_PRIO, TRUE, TRUE);   // (18/10) halfRate: va a 6650 Hz (audio.res)
}

// --- Abajo -------------------------------------------------------------------
#define L8_BOT_CAM_Y       2592          // 2816 - 224
#define L8_BOT_CAM_MAX_X   (L8_W - SCREEN_W)   // 4352
#define L8_BOT_LANE_T      2696
#define L8_BOT_LANE_B      2808
#define L8_BOSS_FEET_X     4420
// (18/10) Bolas en el viaje: dejan de salir cuando faltan ~360 px de bajada
// (las que ya salieron terminan de cruzar el ascensor).
#define L8_BALLS_STOP_Y   (L8_BOT_CAM_Y - 360)

// --- Juego -------------------------------------------------------------------
#define CAM_DEAD_ZONE_RIGHT 120
#define CAM_MAX_SPEED_X       4
#define FOOT_DX  (PLAYER_SPRITE_W / 2)
#define L8_MAX_ALIVE_1P       3
#define L8_MAX_ALIVE_2P       2
#define L8_START_X           40
#define L8_CLEAR_SECS         5
#define VOL_MUSIC            80
#define L8_DMA_QUEUE        200          // (10/10) entradas de la cola de DMA en este nivel
#define L8_SPAWN_GIVEUP     180          // (10/10) 3 s sin VRAM para el siguiente -> se saltea
#define L8_OFF_MARGIN        96          // (10/10) fuera de pantalla por mas de esto...
#define L8_OFF_GIVEUP       360          // ...durante 6 s -> se saca

typedef enum { PH_TOP, PH_RIDE, PH_BOTTOM } TechnoPhase;

// ---------------------------------------------------------------------------
// OLEADAS (morados y, desde el 06/10, los de la LANZA, como en el remaster:
// tres en cada una de las dos ultimas de arriba). Nada que tire proyectiles:
// los shurikens y la lanza tirada no saben de camara vertical, asi que los de
// la lanza de aca no la tiran (pelean con la estocada). Arriba se disparan
// por los pies del lider; la de abajo, al llegar el ascensor.
// ---------------------------------------------------------------------------
// (18/10) Rehechas siguiendo el arcade (ver el encabezado). Las columnas
// estan entre x 520 y 1240 (techno_hazards.c): las oleadas 2 a 5 se pelean
// entre los lasers.
#define P ENEMY_TYPE_FOOT_SOLDIER
#define S ENEMY_TYPE_FOOT_SOLDIER_SPEAR
#define W ENEMY_TYPE_FOOT_SOLDIER_WHITE
#define K ENEMY_TYPE_FOOT_SOLDIER_KNIFE
#define G ENEMY_TYPE_FOOT_SOLDIER_GUN
#define TW_MAX 6
typedef struct { s16 trigX, lockX; u8 n; s8 side[TW_MAX]; u8 type[TW_MAX]; } TechnoWave;
static const TechnoWave topWaves[] = {
    {  200,    0, 3, { +1, -1, +1 },             { W, W, W } },
    {  540,  360, 4, { +1, +1, -1, +1 },         { W, W, W, W } },
    {  820,  640, 4, { -1, +1, +1, -1 },         { W, S, W, S } },
    { 1080,  900, 4, { +1, -1, +1, +1 },         { K, P, K, P } },
    { 1300, 1120, 5, { +1, -1, +1, -1, +1 },     { P, S, P, S, P } },
    { 1460, 1348, 4, { +1, +1, -1, +1 },         { S, P, P, S } },
};
#define TOP_WAVES ((s16)(sizeof(topWaves) / sizeof(topWaves[0])))
// Abajo: al llegar el ascensor, los del fusil (y dos del cuchillo, que en el
// arcade son los del monociclo); con la camara al fondo, lanzas y morados.
static const TechnoWave botWaves[] = {
    {    0,    0, 6, { +1, +1, -1, +1, -1, +1 }, { G, G, K, G, K, G } },
    { 4200, 4352, 4, { +1, -1, +1, +1 },         { S, P, S, P } },
};
#define BOT_WAVES ((s16)(sizeof(botWaves) / sizeof(botWaves[0])))
#undef P
#undef S
#undef W
#undef K
#undef G

static const SbgBlk2D technoMap = {
    (const u32*) techno_tiles,
    (const u16*) techno_blocks,         // (18/10) por bloques: tools/pack_techno_map.py
    (const u8*)  techno_bidx,
    L8_MAP_W, L8_MAP_H,
    &pal_techno,
};

// ---------------------------------------------------------------------------
// Ascensor en BG_A (columnas de pantalla + scroll fino)
// ---------------------------------------------------------------------------
static u16 elevVram;
static s16 elevOc;                 // columna de pantalla de su borde (tiles), 0x7FFF = sin dibujar
static bool elevDrawn;
// (10/10) Filas del ascensor YA ARMADAS (atributos resueltos) con 41 celdas
// vacias a cada lado: la ventana de 41 columnas que va a pantalla es un
// puntero dentro de la fila, sin calcular nada. Antes se rearmaban las 16
// filas celda por celda y se escribian por CPU en pleno cuadro cada vez que
// el ascensor cruzaba un tile (cada 2 cuadros caminando abajo): lentitud y
// cortes en pantalla.
#define ELEV_PAD   41
#define ELEV_BUF_W (ELEV_PAD + 40 + ELEV_PAD + 1)
static u16 elevBuf[L8_ELEV_H / 8][ELEV_BUF_W];

static void elevBuild(void) {
    const TileMap* tm = techno_elevator.tilemap;
    for (u16 r = 0; r < L8_ELEV_H / 8; r++) {
        for (u16 c = 0; c < ELEV_BUF_W; c++) elevBuf[r][c] = 0;
        if (r >= tm->h) continue;
        for (u16 lx = 0; lx < tm->w && lx < 40; lx++) {
            u16 e = tm->tilemap[(u32)r * tm->w + lx];
            u16 idx = e & TILE_INDEX_MASK;
            if (idx)
                elevBuf[r][ELEV_PAD + lx] = (u16)((e & (TILE_ATTR_VFLIP_MASK | TILE_ATTR_HFLIP_MASK))
                                     | TILE_ATTR(PAL0, FALSE, FALSE, FALSE) | (elevVram + idx));
        }
    }
}

static void elevClear(void) {
    if (!elevDrawn) return;
    VDP_clearTileMapRect(BG_A, 0, L8_ELEV_ROW, 41, L8_ELEV_H / 8);
    elevDrawn = FALSE;
}

// oc = columna de pantalla del borde izquierdo (-40..39). Pantalla col c =
// ascensor col c - oc = elevBuf[r][ELEV_PAD + c - oc].
static void elevBlit(s16 oc, TransferMethod tm) {
    for (u16 r = 0; r < L8_ELEV_H / 8; r++)
        VDP_setTileMapDataRow(BG_A, &elevBuf[r][ELEV_PAD - oc], (u16)(L8_ELEV_ROW + r),
                              0, 41, tm);
    elevDrawn = TRUE;
}

// sx = X del ascensor en pantalla. Devuelve el scroll fino para BG_A.
static s16 elevUpdate(s16 sx, TransferMethod tm) {
    if (sx <= -L8_ELEV_W || sx >= SCREEN_W) {
        elevClear();
        elevOc = 0x7FFF;
        return 0;
    }
    s16 oc = (s16)(sx >> 3);            // piso (sx puede ser negativo)
    if (oc != elevOc || !elevDrawn) {
        elevBlit(oc, tm);
        elevOc = oc;
    }
    return (s16)(sx & 7);
}

// Scroll por fila de tile. static: van por DMA_QUEUE.
static s16 scrA[ROWS], scrB[ROWS];

static void applyScroll(s16 camX, s16 camY, s16 elevFine, TransferMethod tm) {
    for (u16 r = 0; r < ROWS; r++) {
        scrB[r] = (s16)-camX;
        scrA[r] = (r < HUD_ROWS) ? 0 : elevFine;
    }
    VDP_setHorizontalScrollTile(BG_A, 0, scrA, ROWS, tm);
    VDP_setHorizontalScrollTile(BG_B, 0, scrB, ROWS, tm);
    if (tm == DMA_QUEUE) VDP_setVerticalScrollVSync(BG_B, (s16)(camY & 255));
    else                 VDP_setVerticalScroll(BG_B, (s16)(camY & 255));
}

// ---------------------------------------------------------------------------
// Helpers de jugadores
// ---------------------------------------------------------------------------
// Bordes del piso del ascensor (X de MUNDO del sprite, no de los pies) a la
// altura de los pies de 'p'.
static s16 padLeftX(const Player* p, s16 ex, s16 ey) {
    return (s16)(ex + L8_FLOOR_L0 - (p->y - ey) + L8_FLOOR_MARGIN - FOOT_DX);
}
static s16 padRightX(const Player* p, s16 ex, s16 ey) {
    return (s16)(ex + L8_FLOOR_R0 - (p->y - ey) - L8_FLOOR_MARGIN - FOOT_DX);
}

static bool onPad(const Player* p, s16 ex, s16 ey) {
    s16 ly = (s16)(p->y - ey);
    if (ly < L8_FLOOR_T || ly > L8_FLOOR_B) return FALSE;
    return p->x >= padLeftX(p, ex, ey) && p->x <= padRightX(p, ex, ey);
}

// Durante el viaje: encima del ascensor, sin poder bajarse. Los bordes
// dependen de la Y, asi que se recalculan cada cuadro y se corrige la X
// (moverse en Y contra un borde diagonal te dejaria afuera).
static void clampToPad(Player* p, s16 ex, s16 ey) {
    setPlayerLane(p, (s16)(ey + L8_FLOOR_T), (s16)(ey + L8_FLOOR_B));
    s16 l = padLeftX(p, ex, ey), r = padRightX(p, ex, ey);
    setPlayerLeftBound(p, l);
    setPlayerRightBound(p, r);
    if (p->x < l) p->x = l;
    if (p->x > r) p->x = r;
}

// Cuenta para arrancar el ascensor: en juego y con cuerpo (los que estan
// fuera de juego esperando continue, o sin sprite, no frenan a nadie).
static bool countsForElevator(const Player* p) {
    return !isPlayerGameOver(p) && p->sprite != NULL;
}

static void drawPlayers(Player** pls, u8 nPl, s16 camX, s16 camY) {
    for (u8 k = 0; k < nPl; k++)
        if (pls[k]->sprite)
            playerSetSpritePos(pls[k], pls[k]->x - camX,
                            pls[k]->y - PLAYER_FOOT_OFFSET - playerDrawZ(pls[k]) - camY);
}

static void spawnWaveEnemy(Enemy* e, s8 side, u8 type, s16 camX, s16 laneT, s16 laneB) {
    s16 x = (side < 0) ? (s16)(camX - enemySpriteW(type)) : (s16)(camX + SCREEN_W);
    s16 span = (s16)(laneB - laneT - 16);
    s16 y = (s16)(laneT + 8 + ((span > 0) ? (s16)(random() % (u16)span) : 0));
    if (type != ENEMY_TYPE_FOOT_SOLDIER) {
        // (18/10) Los blancos (espada y cuchillo) van en PAL3. Con stageCamY
        // los proyectiles ya se dibujan bien: la lanza se tira otra vez.
        u8 pl = (type == ENEMY_TYPE_FOOT_SOLDIER_WHITE ||
                 type == ENEMY_TYPE_FOOT_SOLDIER_KNIFE) ? PAL3 : PAL2;
        initEnemyWalkInSpawn(e, x, y, (s8)((side < 0) ? 1 : -1), pl, type);
    }
    else if (side < 0) initEnemySomersaultSpawn(e, x, y, 1, PAL2, type);
    else               initEnemyKickSpawn(e, x, y, -1, PAL2, type);
    setEnemyBounds(e, laneT, laneB, 0, 0, L8_W);
}

// ---------------------------------------------------------------------------
// (18/10) ANIMACION DE PALETA del mapa (PAL0), medida en el video del arcade
// ---------------------------------------------------------------------------
// En el arcade, con la camara quieta, cambian solo algunos colores:
//   - los bloques rojos del pozo y las luces rojas: rampa de brillo de 8
//     niveles ida y vuelta, 6 frames por paso (84 frames la vuelta);
//   - el panel azul del piso: la misma rampa en azules;
//   - las lucecitas de los paneles de la pared: 6 colores, 5 frames cada uno.
// En el mapa de Ray esos detalles quedaron en pocos indices: 3 y 10 (rojos),
// 6 (rosa/violeta: triangulos del piso y bloques del pozo), 4 (amarillo: las
// rayas de los paneles y de la rampa), 9 (cian: los displays) y 12 (azul).
// Los grises del piso y las paredes (5, 7, 13, 14, 15) y el resto no cambian.
#define TP_PULSE_T     6               // frames por paso de la rampa
#define TP_LAMP_T      5               // frames por color de las lucecitas
static u16 tpBase[16];
static u16 tpCur[16];
static u16 tpT;

// Escala un color de la VDP (0BBB0GGG0RRR0) por num/8.
static u16 tpScale(u16 c, u16 num) {
    u16 r = (u16)(((c >> 1) & 7) * num / 8);
    u16 g = (u16)(((c >> 5) & 7) * num / 8);
    u16 b = (u16)(((c >> 9) & 7) * num / 8);
    return (u16)((b << 9) | (g << 5) | (r << 1));
}

static const u16 tpYellow[4] = {        // ambar que titila (ida y vuelta)
    RGB24_TO_VDPCOLOR(0xFFFF9C), RGB24_TO_VDPCOLOR(0xFFD600),
    RGB24_TO_VDPCOLOR(0xFFAD00), RGB24_TO_VDPCOLOR(0xFFD600),
};
static const u16 tpLamp[6] = {          // las lucecitas de los paneles
    RGB24_TO_VDPCOLOR(0x00FFFF), RGB24_TO_VDPCOLOR(0x00FF8C),
    RGB24_TO_VDPCOLOR(0xFFEF94), RGB24_TO_VDPCOLOR(0xFF00A5),
    RGB24_TO_VDPCOLOR(0xFFD600), RGB24_TO_VDPCOLOR(0x0000CE),
};

static void techPalInit(void) {
    for (u16 i = 0; i < 16; i++) tpBase[i] = tpCur[i] = pal_techno.data[i];
    tpT = 0;
}

static void techPalStep(void) {
    tpT++;
    u16 st = (u16)((tpT / TP_PULSE_T) % 14);
    u16 lv = (st < 8) ? st : (u16)(14 - st);            // 0..7..1
    u16 num = (u16)(3 + (5 * lv + 3) / 7);              // 3/8 .. 8/8
    u16 nc[16];
    for (u16 i = 0; i < 16; i++) nc[i] = tpBase[i];
    nc[3]  = tpScale(tpBase[3], num);
    nc[10] = tpScale(tpBase[10], num);
    nc[6]  = tpScale(tpBase[6], num);
    nc[12] = tpScale(tpBase[12], (u16)(11 - num));       // el azul late a contramano
    nc[4]  = tpYellow[(tpT / TP_LAMP_T) & 3];
    nc[9]  = tpLamp[(tpT / TP_LAMP_T) % 6];
    bool ch = FALSE;
    for (u16 i = 0; i < 16; i++)
        if (nc[i] != tpCur[i]) { tpCur[i] = nc[i]; ch = TRUE; }
    if (ch) PAL_setColors(0, tpCur, 16, DMA_QUEUE);
}

// ===========================================================================
// La escena
// ===========================================================================
SceneId showScene81() {
    clearScene();

    const u8  nPlSetup  = numJugadores();
    const u16 barBlocks = (u16)((nPlSetup > 2) ? MAX_PLAYERS : 2) * HUD_VRAM_PER_PLAYER;
    const u16 elevTiles = techno_elevator.tileset->numTile;

    // VRAM: HUD | ascensor | cache del mapa | columnas/laser (18/10) | sprites.
    SPR_initEx((u16)(TILE_FONT_INDEX - (TILE_USER_INDEX + barBlocks + elevTiles
                                        + L8_BG_SLOTS + TH_VRAM_TILES)));

    // (10/10) Cola de DMA mas grande mientras dura el nivel: los tiles del
    // mapa que entran (sobre todo bajando en el ascensor) van por ahi, uno por
    // entrada. Se vuelve al default al salir.
    DMA_setMaxQueueSize(L8_DMA_QUEUE);

    VDP_setScreenWidth320();
    VDP_setPlaneSize(64, 32, TRUE);
    VDP_setScrollingMode(HSCROLL_TILE, VSCROLL_PLANE);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setBackgroundColor(L8_BACKDROP);

    u8   nPl = nPlSetup;
    bool dosJugadores = (nPl >= 2);
    s16  cameraX = 0;
    s16  cameraY = L8_TOP_CAM_Y;
    s16  elevX = L8_ELEV_X0, elevY = L8_ELEV_Y0;
    TechnoPhase phase = PH_TOP;

    u16 hudVram = TILE_USER_INDEX;
    elevVram = (u16)(TILE_USER_INDEX + barBlocks);
    VDP_loadTileSet(techno_elevator.tileset, elevVram, DMA);
    elevBuild();
    elevDrawn = FALSE;
    elevOc = 0x7FFF;
    sbgInitRaw2D(&technoMap, BG_B, PAL0, (u16)(elevVram + elevTiles), L8_BG_SLOTS,
                 cameraX, cameraY);
    thInit((u16)(elevVram + elevTiles + L8_BG_SLOTS), thTechnoPillars, TH_TECHNO_PILLARS, 110);
    techPalInit();
    stageCamY = cameraY;
    s16 elevFine = elevUpdate((s16)(elevX - cameraX), DMA);
    applyScroll(cameraX, cameraY, elevFine, DMA);

    hudSetPlane(BG_A);
    hudInit();

    Player p1, p2, p3, p4;
    Player* pls[MAX_PLAYERS] = { &p1, &p2, &p3, &p4 };
    static const u8 frac[MAX_PLAYERS] = { 40, 65, 20, 85 };
    for (u8 k = 0; k < nPl; k++) {
        s16 y = (s16)(L8_TOP_LANE_T + ((L8_TOP_LANE_B - L8_TOP_LANE_T) * frac[k]) / 100);
        initPlayer(pls[k], playerChar(k), playerJoy(k), PAL1, L8_START_X, y);
        setPlayerLane(pls[k], L8_TOP_LANE_T, L8_TOP_LANE_B);
        setPlayerEndWall(pls[k], 0, 0);
        setPlayerLeftBound(pls[k], cameraX);
        setPlayerRightBound(pls[k], cameraX + SCREEN_W - PLAYER_SPRITE_W);
        setPlayerCamera(pls[k], cameraX);
    }

    VDP_loadFont(&hud_font, DMA);
    VDP_setTextPlane(BG_A);
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
    shurikenInit();                    // (18/10) lanzas, balas y cuchillos
    static Enemy enemies[MAX_ENEMIES];
    for (u16 i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].state  = ENEMY_STATE_INACTIVE;
        enemies[i].sprite = NULL;
    }
    u16 maxAlive = dosJugadores ? L8_MAX_ALIVE_2P : L8_MAX_ALIVE_1P;
    static u16 offTime[MAX_ENEMIES];
    for (u16 i = 0; i < MAX_ENEMIES; i++) offTime[i] = 0;

    static Traag traag;
    traagInit(&traag);
    bool bossStarted = FALSE;

    const TechnoWave* wave = NULL;     // oleada en curso
    s16 topIdx      = -1;              // ultima oleada de arriba disparada
    s16 botIdx      = -1;              // (18/10) idem abajo
    bool botFired   = FALSE;
    u8  waveSpawned = 0;
    u8  waveTot     = 0;               // (20/10) n + extra de la dificultad
    s16 camLockX    = -1;
    u8  arm         = 0;               // frames con todos encima del ascensor
    u16 spawnWait   = 0;               // (10/10) frames sin poder hacer entrar al siguiente
    u16 rideAcc     = 0;               // (18/10) fraccion de px del viaje (Q8)

    // Revelado: PAL0 el mapa, PAL1 las tortugas, PAL2 los soldiers, PAL3 la
    // del blanco (18/10: blancos, columnas, laser y bolas; Traag la pisa al
    // entrar).
    u16 target[64];
    for (u16 i = 0; i < 16; i++) {
        target[i]      = pal_techno.data[i];
        target[16 + i] = leo_player.palette->data[i];
        target[32 + i] = foot_soldier.palette->data[i];
        target[48 + i] = foot_soldier_white.palette->data[i];
    }
    for (u8 k = 0; k < nPl; k++) updatePlayer(pls[k]);
    drawPlayers(pls, nPl, cameraX, cameraY);
    SPR_update();

    XGM2_setLoopNumber(-1);
    playMusicVol(music_technodrome, VOL_MUSIC);

    static const u16 black[64] = { 0 };
    PAL_setColors(0, black, 64, DMA);
    PAL_fadeInAll(target, 20, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    bool running = TRUE;
    bool allOut  = FALSE;
    bool win     = FALSE;
    SceneId jump = PAUSE_NO_JUMP;
    pauseReset();
    idleMissileReset();
    while (running) {
        jump = pausePoll(pls, nPl);
        if (jump != PAUSE_NO_JUMP) break;

        // --- Jugadores -----------------------------------------------------
        for (u8 k = 0; k < nPl; k++) updatePlayer(pls[k]);
        idleMissileUpdate(pls, nPl, cameraX, cameraY);   // (14/10) modo ARCADE

        s16 leadX = p1.x;
        {
            bool any = FALSE;
            for (u8 k = 0; k < nPl; k++) {
                if (isPlayerGameOver(pls[k])) continue;
                if (!any || pls[k]->x > leadX) leadX = pls[k]->x;
                any = TRUE;
            }
        }
        s16 leadFeetX = (s16)(leadX + FOOT_DX);

        u16 alive = 0;
        for (u16 i = 0; i < MAX_ENEMIES; i++)
            if (enemies[i].state != ENEMY_STATE_INACTIVE) alive++;

        // --- Fases ------------------------------------------------------------
        if (phase == PH_TOP) {
            s16 leadScreenX = (s16)(leadX - cameraX);
            if (leadScreenX > CAM_DEAD_ZONE_RIGHT) {
                s16 nc = (s16)(cameraX + (leadScreenX - CAM_DEAD_ZONE_RIGHT));
                if (nc - cameraX > CAM_MAX_SPEED_X) nc = (s16)(cameraX + CAM_MAX_SPEED_X);
                if (camLockX >= 0 && nc > camLockX) nc = camLockX;
                if (nc > L8_TOP_CAM_MAX_X) nc = L8_TOP_CAM_MAX_X;
                if (nc > cameraX) cameraX = nc;
            }
            // Oleadas de arriba.
            if ((wave == NULL || waveSpawned >= waveTot) && topIdx + 1 < TOP_WAVES &&
                leadFeetX >= topWaves[topIdx + 1].trigX) {
                topIdx++;
                wave = &topWaves[topIdx];
                waveSpawned = 0;
                waveTot = (u8)(wave->n + diffWaveExtra(wave->n));
                camLockX = wave->lockX;
                if (camLockX < cameraX) camLockX = cameraX;
            }
            bool wavesDone = (topIdx == TOP_WAVES - 1 && wave && waveSpawned >= waveTot &&
                              alive == 0);
            // El ascensor arranca con todos los que siguen en juego encima.
            bool all = wavesDone;
            u8 inGame = 0;
            for (u8 k = 0; k < nPl && all; k++) {
                if (!countsForElevator(pls[k])) continue;
                inGame++;
                if (!onPad(pls[k], elevX, elevY)) all = FALSE;
            }
            if (all && inGame) {
                if (++arm >= L8_ARM_FRAMES) {
                    phase = PH_RIDE;
                    for (u8 k = 0; k < nPl; k++) clampToPad(pls[k], elevX, elevY);
                    elevSfxPlay();
                }
            } else {
                arm = 0;
            }
            if (phase == PH_TOP) {
                for (u8 k = 0; k < nPl; k++) {
                    setPlayerLane(pls[k], L8_TOP_LANE_T, L8_TOP_LANE_B);
                    setPlayerLeftBound(pls[k], cameraX);
                    // A la derecha del pasillo solo esta el ascensor (su
                    // borde derecho es diagonal: depende de la Y).
                    s16 rb = (s16)(cameraX + SCREEN_W - PLAYER_SPRITE_W);
                    s16 padR = padRightX(pls[k], elevX, elevY);
                    setPlayerRightBound(pls[k], (rb < padR) ? rb : padR);
                }
            }
        } else if (phase == PH_RIDE) {
            if (!XGM2_isPlayingPCM(1 << L8_ELEV_SFX_CH)) elevSfxPlay();
            // (18/10) Frena al llegar: en los ultimos L8_RIDE_SLOW px la
            // velocidad baja en proporcion a lo que falta (Q8, minimo 0,5 px/f).
            s16 left = (s16)(L8_BOT_CAM_Y - cameraY);
            u16 vq = (u16)L8_RIDE_SPEED << 8;
            if (left < L8_RIDE_SLOW) {
                vq = (u16)(((u32)left << 8) * L8_RIDE_SPEED / L8_RIDE_SLOW);
                if (vq < 128) vq = 128;
            }
            rideAcc = (u16)(rideAcc + vq);
            s16 s = (s16)(rideAcc >> 8);
            rideAcc &= 0xFF;
            if (s > left) s = left;
            elevX += s; elevY += s;
            cameraX += s; cameraY += s;
            for (u8 k = 0; k < nPl; k++) {
                pls[k]->x += s;
                pls[k]->y += s;
                clampToPad(pls[k], elevX, elevY);
            }
            if (cameraY >= L8_BOT_CAM_Y) {
                phase = PH_BOTTOM;
                XGM2_stopPCM(L8_ELEV_SFX_CH);
                botIdx = 0;
                wave = &botWaves[0];
                botFired = TRUE;
                waveSpawned = 0;
                waveTot = (u8)(wave->n + diffWaveExtra(wave->n));
                camLockX = cameraX;
            }
        } else {
            s16 leadScreenX = (s16)(leadX - cameraX);
            if (leadScreenX > CAM_DEAD_ZONE_RIGHT) {
                s16 nc = (s16)(cameraX + (leadScreenX - CAM_DEAD_ZONE_RIGHT));
                if (nc - cameraX > CAM_MAX_SPEED_X) nc = (s16)(cameraX + CAM_MAX_SPEED_X);
                if (camLockX >= 0 && nc > camLockX) nc = camLockX;
                if (nc > L8_BOT_CAM_MAX_X) nc = L8_BOT_CAM_MAX_X;
                if (nc > cameraX) cameraX = nc;
            }
            // (18/10) Oleadas de abajo despues de la de llegada.
            if (botIdx >= 0 && (wave == NULL || waveSpawned >= waveTot) && alive == 0 &&
                botIdx + 1 < BOT_WAVES && leadFeetX >= botWaves[botIdx + 1].trigX) {
                botIdx++;
                wave = &botWaves[botIdx];
                waveSpawned = 0;
                waveTot = (u8)(wave->n + diffWaveExtra(wave->n));
                camLockX = wave->lockX;
                if (camLockX < cameraX) camLockX = cameraX;
            }
            for (u8 k = 0; k < nPl; k++) {
                setPlayerLane(pls[k], L8_BOT_LANE_T, L8_BOT_LANE_B);
                // A la izquierda del ascensor esta el pozo (borde diagonal).
                s16 lb = padLeftX(pls[k], elevX, elevY);
                setPlayerLeftBound(pls[k], (cameraX > lb) ? cameraX : lb);
                setPlayerRightBound(pls[k], cameraX + SCREEN_W - PLAYER_SPRITE_W);
            }
        }
        for (u8 k = 0; k < nPl; k++) setPlayerCamera(pls[k], cameraX);

        // --- P2 que se suma en plena partida (no durante el viaje) ------------
        if (nPl == 1 && phase != PH_RIDE) {
            u8 ch2 = p2JoinPoll(hudPlayerCol(1));
            if (ch2 != 0xFF) {
                cantidadJugadores = 2;
                initPlayer(&p2, ch2, playerJoy(1), PAL1,
                           (s16)(getPlayerWorldX(&p1) - 48), getPlayerY(&p1));
                setPlayerLane(&p2, phase == PH_TOP ? L8_TOP_LANE_T : L8_BOT_LANE_T,
                              phase == PH_TOP ? L8_TOP_LANE_B : L8_BOT_LANE_B);
                setPlayerEndWall(&p2, 0, 0);
                setPlayerCamera(&p2, cameraX);
                setPlayerLeftBound(&p2, cameraX);
                setPlayerRightBound(&p2, cameraX + SCREEN_W - PLAYER_SPRITE_W);
                hudPlayerInit(&huds[1], &p2, hudPlayerCol(1),
                              (u16)(hudVram + HUD_VRAM_PER_PLAYER));
                nPl          = 2;
                dosJugadores = TRUE;
                maxAlive     = L8_MAX_ALIVE_2P;
                resetEnemyAI(2);
            }
        }
        for (u8 k = 0; k < nPl; k++) hudPlayerUpdate(&huds[k]);

        bool dying = bossStarted && traagIsDying(&traag);
        if (!dying && continueStepAll(conts, pls, huds, nPl, fps)) { allOut = TRUE; break; }

        // --- Hacer entrar a los de la oleada en curso ------------------------
        if (wave && phase != PH_RIDE) {
            s16 lt = (phase == PH_TOP) ? L8_TOP_LANE_T : L8_BOT_LANE_T;
            s16 lb = (phase == PH_TOP) ? L8_TOP_LANE_B : L8_BOT_LANE_B;
            while (waveSpawned < waveTot && alive < maxAlive) {
                u8 we = (u8)(waveSpawned % wave->n);   // los extra repiten la tabla
                u16 i;
                for (i = 0; i < MAX_ENEMIES; i++)
                    if (enemies[i].state == ENEMY_STATE_INACTIVE) break;
                if (i >= MAX_ENEMIES) break;
                if (!sprVramFits(enemySheetDef(wave->type[we])->maxNumTile)) {
                    // (10/10) Red de seguridad: si el que sigue no entra en
                    // VRAM por L8_SPAWN_GIVEUP cuadros, se saltea. Sin esto la
                    // oleada nunca "terminaba", la camara quedaba trabada y el
                    // ascensor no arrancaba.
                    if (++spawnWait >= L8_SPAWN_GIVEUP) { spawnWait = 0; waveSpawned++; }
                    break;
                }
                spawnWait = 0;
                // (18/10) Abajo todos entran por la DERECHA: el jugador tiene
                // que avanzar.
                spawnWaveEnemy(&enemies[i], (phase == PH_BOTTOM) ? 1 : wave->side[we],
                               wave->type[we], cameraX, lt, lb);
                waveSpawned++;
                alive++;
            }
            if (waveSpawned >= waveTot && alive == 0) camLockX = -1;
        }

        // (18/10) La Y de camara para lo que dibuja enemy.c (soldiers y sus
        // proyectiles) y player.c.
        stageCamY = cameraY;

        // --- Foot soldiers -----------------------------------------------------
        separateEnemies(enemies, MAX_ENEMIES);
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            Enemy* e = &enemies[i];
            if (e->state == ENEMY_STATE_INACTIVE) continue;
            setEnemyCamera(e, cameraX);
            updateEnemyN(e, pls, nPl);
            if (e->state == ENEMY_STATE_INACTIVE) continue;
            // (10/10) Red de seguridad: uno que quedo lejos de la pantalla
            // mucho tiempo (trabado contra algo) se saca, para que la oleada
            // pueda terminar y la camara se destrabe.
            {
                s16 sx = (s16)(e->x - cameraX);
                if (sx < -L8_OFF_MARGIN || sx > SCREEN_W + L8_OFF_MARGIN) {
                    if (++offTime[i] >= L8_OFF_GIVEUP) {
                        if (e->sprite) SPR_releaseSprite(e->sprite);
                        e->sprite = NULL;
                        e->state  = ENEMY_STATE_INACTIVE;
                        offTime[i] = 0;
                        continue;
                    }
                } else offTime[i] = 0;
            }
            if (e->sprite)
                SPR_setPosition(e->sprite, e->x - cameraX + enemyShakeX(e),
                                e->y - e->footOffset - e->jumpZ - cameraY);
        }
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (!enemyCanBeHit(&enemies[i])) continue;
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

        // --- (18/10) Lanzas, balas y cuchillos ----------------------------------
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

        // --- (18/10) Columnas con laser (arriba) y bolas (viaje) ----------------
        thPillarsUpdate(pls, nPl, cameraX, cameraY);
        thBallsUpdate(pls, nPl, cameraX, cameraY, elevX, elevY,
                      phase == PH_RIDE && cameraY < L8_BALLS_STOP_Y);

        // --- Jefe: Traag -------------------------------------------------------
        if (!bossStarted && phase == PH_BOTTOM && botFired && botIdx == BOT_WAVES - 1 &&
            waveSpawned >= waveTot &&
            alive == 0 && cameraX >= L8_BOT_CAM_MAX_X && leadFeetX >= L8_BOSS_FEET_X) {
            bossStarted = TRUE;
            XGM2_setLoopNumber(-1);
            playMusicVol(music_boss, VOL_MUSIC);
            traagSpawn(&traag, (s16)(cameraX + 24), (s16)(cameraX + SCREEN_W - 24),
                       L8_BOT_LANE_T, L8_BOT_LANE_B, NULL);
        }
        if (bossStarted) {
            traagUpdate(&traag, pls, nPl, cameraX, cameraY);
            traagMissileUpdate(pls, nPl, cameraX, cameraY);
            s8 killer = -1;
            if (traagPlayerHits(&traag, pls, nPl, &killer) && killer >= 0)
                addPlayerScore(pls[(u8)killer], 5);
            if (traagIsGone(&traag)) { win = TRUE; running = FALSE; }
        }

        // --- Fondo, ascensor y sprites -----------------------------------------
        if (!PAL_isDoingFade()) techPalStep();     // (18/10)
        sbgUpdate2D(cameraX, cameraY);
        elevFine = elevUpdate((s16)(elevX - cameraX), DMA_QUEUE);
        applyScroll(cameraX, cameraY, elevFine, DMA_QUEUE);
        drawPlayers(pls, nPl, cameraX, cameraY);

        SPR_update();
        SYS_doVBlankProcess();
    }

    if (win && !allOut) {
        for (u8 k = 0; k < nPl; k++) playerPersistSave(pls[k]);
        XGM2_stop();
        XGM2_setLoopNumber(0);
        playMusicVol(music_scene_clear, VOL_MUSIC);
        u16 hold = (IS_PAL_SYSTEM ? 50 : 60) * L8_CLEAR_SECS;
        while (hold > 0) { hold--; SYS_doVBlankProcess(); }
        XGM2_stop();
        XGM2_setLoopNumber(-1);
    }

    if (phase == PH_RIDE) XGM2_stopPCM(L8_ELEV_SFX_CH);   // salio en pleno viaje
    thRelease();               // (18/10)
    shurikenReleaseAll();
    stageCamY = 0;
    traagRelease(&traag);
    DMA_setMaxQueueSizeToDefault();
    VDP_setTextPriority(0);
    VDP_setTextPalette(PAL0);
    VDP_setTextPlane(BG_A);
    clearScene();          // tambien vuelve el scroll a modo plano
    if (jump != PAUSE_NO_JUMP) return jump;
    if (win && !allOut) return SCENE_9_1;   // (18/10) 6-1 -> 6-2, sin titulo
    return SCENE_GAME_OVER;
}
