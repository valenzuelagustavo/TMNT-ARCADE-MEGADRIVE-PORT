// ===========================================================================
// level2_1.c - Nivel 2-1: la calle (SCENE 2 del arcade)
// ===========================================================================
// Entra despues del titulo "SCENE 2 / C'MON, AFTER THAT SHREDDER CREEP!!", que
// a su vez viene de la cutscene en la que Shredder se escapa por la ventana.
//
// EL RECORRIDO ES UNA "L", como en todos los beat'em up de la epoca: se avanza
// a la derecha por la vereda de arriba, en la esquina la calle dobla y se baja
// en diagonal, y abajo se vuelve a avanzar a la derecha hasta el auto quemado.
// Los tramos estan marcados en el arte por las zonas transparentes: el rip del
// arcade solo tiene pintado lo que la camara llega a mostrar.
//
// TRES COSAS QUE HACEN A ESTE NIVEL DISTINTO DEL 1-1
//
// 1. EL FONDO SE STREAMEA POR TILES, NO POR COLUMNAS.
//    El 1-1 carga su tileset entero (~495 tiles) una sola vez. Aca el nivel
//    completo son ~1983 tiles unicos y no entran: con SPR_initEx(496) quedan
//    LVL21_RING_TILES (880) para el fondo. tools/gen_level2_1_bg.py parte el
//    recorrido en LVL21_SECTIONS secciones y este modulo las va cargando en un
//    buffer CIRCULAR a medida que la camara avanza, pisando las que ya
//    quedaron atras. El generador SIMULA este mismo algoritmo y falla si
//    alguna celda visible fuera a quedar apuntando a tiles ya pisados, asi que
//    si el .c generado existe, la combinacion de constantes es segura.
//
//    Una seccion puede quedar PARTIDA en el wrap del ring (arranca al final
//    del buffer y sigue al principio). Se carga en dos DMA y se lee con un
//    modulo: sin eso habria que desperdiciar la cola del buffer y el pico de
//    VRAM no entraba.
//
// 2. EL PLANO SE REVELA EN LOS DOS EJES.
//    BG_B es circular de 64x32 (512x256 px) y la camara se mueve en X y en Y,
//    asi que hay que ir dibujando columnas nuevas por la derecha Y filas
//    nuevas por abajo. Se mantiene dibujada solo la ventana visible + 1 celda
//    de margen (LVL21_VIS_COLS x LVL21_VIS_ROWS): dibujar las 64x32 celdas del
//    plano gastaria tiles en filas que la camara nunca muestra.
//
// 3. LA CAMARA BAJA LA ESQUINA EN DIAGONAL, COMO EL ARCADE.
//    Gustavo trajo 16 capturas del arcade original; localizadas dentro del PNG
//    del nivel, los matches limpios caen sobre una recta:
//        camX = 0,803 * camY + 1289
//    O sea que el arcade NO hace un codo: se va corriendo a la derecha 0,8px
//    por cada px que baja. La primera version clavaba camX en 1536 durante el
//    descenso y por eso asomaba un cuadro negro arriba a la derecha (ahi el
//    arte llega solo a x=1735 y el viewport pedia hasta 1855).
//
//    lvl21CamFloor/lvl21CamCeil, generadas por el script alrededor de esa
//    recta, acotan camX para cada camY de modo que el viewport NUNCA tenga un
//    solo pixel sin arte. Y camY tiene freno propio: no puede adelantarse a
//    camX (ver el bucle), porque el corredor se estrecha al bajar.
//
//    (17/09) LOS CUATRO ESCALONES. El techo del corredor no es una rampa: es
//    una ESCALERA, y sus peldanos son exactamente los "topes" que Gustavo
//    marco con flechas negras sobre el mapa:
//
//        camY  32..103  ->  tope camX 1344   (piso 1254, el del arte)
//        camY 104..223  ->  tope camX 1416   (piso 1344 = tope anterior)
//        camY 224..343  ->  tope camX 1544   (piso 1416)
//        camY 344..383  ->  tope camX 1633   (piso 1544)
//        camY 384..416  ->  tope camX 2240   (piso 1633) = LVL21_CAM_X_MAX
//
//    El PISO de cada peldano es el TOPE DEL ANTERIOR, y camY tiene el freno de
//    no adelantarse a camX (ver el bucle). Las dos cosas juntas dan el
//    recorrido del arcade: la camara avanza a la derecha hasta el tope del
//    escalon en el que esta y ahi se clava, el jugador queda topado contra el
//    borde derecho de la pantalla, y la unica forma de seguir es BAJAR; recien
//    cuando los pies del que lidera cruzan la franja muerta, camY baja, el
//    techo sube al peldano siguiente y la camara vuelve a correrse. Los cinco
//    numeros salen solos de la geometria del arte: no hay ninguno a mano.
//
//    Y de paso es lo que hace que el fondo ENTRE EN VRAM. Con el piso pelado
//    del arte (1254..1304 durante todo el descenso) la camara podia bajar
//    pegada a la izquierda, y el streaming tendria que tener cargada la union
//    de todas las posiciones posibles: 960 tiles contra los 768 que hay.
//
//    DOS BUGS QUE ARREGLO ESTO (17/09):
//    a) El techo se clavaba en camX=2118 y la camara no llegaba NUNCA a
//       LVL21_CAM_X_MAX, asi que la condicion de fin de nivel no disparaba y
//       el mapa no se veia hasta el final. La causa no era la camara sino el
//       rip: usa el INDICE 0 tambien como NEGRO (contornos de techo, juntas de
//       2px entre edificios, la diagonal del alero del final) y el generador
//       contaba esos pixeles como "zona sin arte". Ahora el corredor se
//       calcula sobre una mascara con los huecos finos cerrados (close_holes
//       en el script); los pixeles del PNG no se tocan, asi que esos negros se
//       siguen viendo negros.
//    b) Durante todo el descenso asomaba una BANDA NEGRA pegada al borde
//       derecho. Ahi la camara no mostraba "el fondo sin arte": mostraba
//       celdas que el generador nunca le habia asignado seccion, porque
//       repartia las secciones simulando UNA trayectoria de camara (la pegada
//       al piso) y la partida real va pegada al techo. El generador ya no
//       simula un camino: calcula la envolvente de todos los posibles.
//
// PENDIENTE A PROPOSITO: los foot soldiers spawnean al azar solo para probar
// (sin oleadas guionadas) y no hay musica de nivel: falta el VGM del Stage 2.
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level2_1.h"        // lvl21_pal, lvl21_sec00..10 (generado por rescomp)
#include "level2_1_map.h"    // lvl21Map, tablas de secciones y de camara
#include "level2_1_limits.h" // lvl21WalkTop/Bot + la cornisa (generado)
#include "level1.h"          // hud_font (la fuente del HUD vive en el nivel 1)
#include "player.h"
#include "hud.h"
#include "enemy.h"    // foot soldiers
#include "audio.h"    // music_scene_clear

#ifndef IS_PAL_SYSTEM
#define IS_PAL_SYSTEM IS_PALSYSTEM
#endif

extern u8 personajeSeleccionado;
extern u8 personaje2Seleccionado;
extern u8 cantidadJugadores;

#define SCREEN_PIXEL_WIDTH   320
#define SCREEN_PIXEL_HEIGHT  224
#define BG_PLANE_W           64      // plano circular BG_B: 64x32 tiles
#define BG_PLANE_H           32

// Ventana que se mantiene dibujada en el plano (visible + 1 de margen).
// TIENE que coincidir con VIS_COLS/VIS_ROWS de gen_level2_1_bg.py: el
// generador valida el ring simulando exactamente esta ventana.
#define LVL21_VIS_COLS       41
#define LVL21_VIS_ROWS       29

// Dead-zones de camara. La horizontal es la misma del nivel 1 (el scroll
// arranca cuando el personaje pasa la mitad de la pantalla). La vertical es
// ancha a proposito: tiene que cubrir TODA la franja de profundidad de un
// tramo, asi la camara no tiembla cuando el jugador camina hacia el fondo o
// hacia el frente, y solo baja cuando se mete en la diagonal.
// Poner en 1 para ver camX / camY / el tope del corredor en pantalla.
#define LVL21_CAM_DEBUG        0

#define CAM_DEAD_ZONE_RIGHT  120
#define CAM_DEAD_ZONE_TOP    118     // pies en pantalla por encima -> sube
#define CAM_DEAD_ZONE_BOT    218     // pies en pantalla por debajo -> baja
#define CAM_MAX_SPEED_X        4
#define CAM_MAX_SPEED_Y        3

// ---------------------------------------------------------------------------
// El area caminable (coordenadas de MUNDO, y = PIES del personaje)
// ---------------------------------------------------------------------------
// (16/09) Antes eran TRES PIEZAS A MANO: dos rectangulos y una diagonal de 45
// grados, con ocho numeros de calibracion que habia que retocar cada vez que
// se tocaba el arte. Ahora sale del dibujo.
//
// Gustavo pinto de MAGENTA, encima del arte, el "limite de pared" de todo el
// nivel ("Stage 2-_LIMITES.png"). tools/gen_level2_1_limits.py lee ese PNG y
// arma dos tablas de 320 entradas (una por tile de ancho): para cada columna,
// el primer Y de pies caminable (el borde inferior del magenta) y el ultimo
// (el ultimo pixel dibujado de esa columna menos un margen: debajo de la calle
// el rip no tiene arte, asi que ese es el borde de la cuneta).
//
// Se gana precision en todos lados -- la tabla sigue los escalones reales de
// las fachadas y la plaza abierta del codo, que la diagonal de 45 grados
// recortaba de mas -- y cuesta 1280 bytes de ROM.
//
// Ademas del piso hay una PLATAFORMA: la cornisa sobre los portones
// (LVL21_PLAT_*). Esta por DETRAS del limite de pared, asi que solo se llega
// saltando; caminando hacia abajo (o saliendose por un costado) uno se deja
// caer a la calle. Ver walkable()/playerFallTo().

// ---------------------------------------------------------------------------
// Foot soldiers al azar (provisorio, para testear)
// ---------------------------------------------------------------------------
// No hay oleadas guionadas todavia: cada LVL21_SPAWN_EVERY px de avance de
// camara entra un soldier por un costado de la pantalla, a una profundidad
// sorteada dentro del poligono de la calle. El tope simultaneo lo fija la VRAM
// de sprites (ver SPR_initEx mas abajo), no el diseno.
#define LVL21_SPAWN_EVERY    220   // px de avance entre spawns
#define LVL21_MAX_ALIVE_1P     3   // tope de soldiers vivos con 1 jugador
#define LVL21_MAX_ALIVE_2P     2   // ... y con 2 (cada tortuga come 169 tiles)
#define LVL21_SPAWN_MARGIN    72   // px fuera de pantalla por donde entran

// Fin del nivel: X de PIES a partir del cual, con la camara en su tope, se
// da por terminado el nivel (el auto quemado del final).
// (17/09) Con la camara en 2240 el borde derecho de la pantalla deja los pies
// en 2240 + 320 - 104 + 52 = 2508, o sea 8px de margen contra los 2500 viejos:
// habia que apretar CONTRA la pared exacta para que disparara. 2470 da 38px,
// que es medio paso: llegar al borde derecho alcanza.
#define LVL21_END_X         2470

// Trepada de los foot soldiers a la cornisa: px de profundidad por frame.
// A ~78px de pared son unos 13 frames, que es lo que dura un salto.
#define LVL21_CLIMB_SPEED      6

// --- Ayuda para subirse a la cornisa ---------------------------------------
// La cuenta pelada no daba: un salto dura 14 frames de subida + 4 de flote +
// 18 de caida = 36, y con PLAYER_SPEED 2 eso son 72px de profundidad, contra
// los 77 que hay entre la calle (y=180) y la cornisa (y=103). O sea que
// subirse era imposible por 5px.
// En vez de tocar la fisica del salto (que es de todo el juego), la ayuda es
// LOCAL: mientras se esta en el aire, sobre la cornisa y APRETANDO ARRIBA, se
// suman LVL21_LEDGE_PULL px por frame de subida en los ultimos
// LVL21_LEDGE_ASSIST px. Se ve como que la tortuga "estira" para agarrarse, y
// no se dispara sola: hay que estar pidiendo arriba.
#define LVL21_LEDGE_ASSIST    48
#define LVL21_LEDGE_PULL       2
// Y al aterrizar, unos pocos px de tolerancia para no perder el salto por
// nada (se sube el resto de una).
#define LVL21_LEDGE_SNAP      10

// Fin del nivel: cuantos segundos queda la imagen congelada con el jingle.
#define LVL21_CLEAR_SECS       5
#define VOL_MUSIC_SCENE_CLEAR 90

// Punto de partida (el jugador sale del edificio, a la izquierda del todo)
#define START_P1_X        40
#define START_P1_Y       210
#define START_P2_Y       230

// (17/09) SEPARACION AL NACER. El P2 nacia en x=160, o sea FUERA de la franja
// muerta de la camara (CAM_DEAD_ZONE_RIGHT = 120). Como la camara sigue al que
// mas avanzo, el nivel arrancaba solo, con un scroll involuntario de 40px --
// y con 4 jugadores el ultimo nacia en x=240 y el tiron era de 120px. Ahora
// los jugadores 2..4 nacen PEGADOS al P1 y todos dentro de la franja muerta:
//   2 jugadores: P2 en x=104
//   3-4:         66 / 92 / 118
// Los de atras ademas se corren un poco en profundidad para no encimarse.
#define START_SPREAD_X_2P  64
#define START_SPREAD_X_4P  26
#define START_DEPTH_STEP    8

// ---------------------------------------------------------------------------
// Estado del streaming de fondo
// ---------------------------------------------------------------------------
// (17/09) La tabla la genera el script junto con el .res: la cantidad de
// secciones cambia con SEC_SPAN_PX y escrita a mano se desincronizaba.
#include "level2_1_sections.h"

static u16 bgVramBase;                     // primer tile de VRAM del ring
static u16 secBase[LVL21_SECTIONS];        // offset dentro del ring, por seccion
static u16 ringPos;                        // proxima posicion libre del ring
static u16 nextSection;                    // proxima seccion a reservar
static u16 loadSection;                    // seccion que se esta copiando ahora
static u16 loadDone;                       // tiles ya copiados de esa seccion

static s16 planeCol, planeRow;             // ultima columna / fila de MUNDO dibujada

// Traduce una celda del mundo a un atributo de tilemap.
static u16 lvl21CellAttr(s16 col, s16 row) {
    if (col < 0 || col >= LVL21_MAP_W || row < 0 || row >= LVL21_MAP_H)
        return 0;
    u16 v = lvl21Map[row * LVL21_MAP_W + col];
    if (!(v & 0x8000))                     // celda de cielo -> tile 0 (negro)
        return 0;
    // (17/09) 6 bits de seccion + 9 de indice (antes 5 + 10): con el corredor
    // clavado hacen falta mas secciones, y ninguna pasa de 512 tiles.
    u16 b = secBase[(v >> 9) & 0x3F] + (v & 0x1FF);
    if (b >= LVL21_RING_TILES) b -= LVL21_RING_TILES;   // la seccion daba la vuelta
    return TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, bgVramBase + b);
}

static void lvl21DrawCell(s16 col, s16 row) {
    VDP_setTileMapXY(BG_B, lvl21CellAttr(col, row),
                     (u16)(col & (BG_PLANE_W - 1)),
                     (u16)(row & (BG_PLANE_H - 1)));
}

// Reserva en el ring el lugar de la seccion 's' y arranca su copia. La copia
// real la hace lvl21StreamUpdate() de a LVL21_LOAD_TILES_PER_FRAME por frame,
// para no clavar 11KB de DMA en un solo vblank.
static void lvl21BeginSection(u16 s) {
    secBase[s]  = ringPos;
    ringPos     = ringPos + lvl21SecTiles[s];
    if (ringPos >= LVL21_RING_TILES) ringPos -= LVL21_RING_TILES;
    loadSection = s;
    loadDone    = 0;
}

// Copia el siguiente pedazo de la seccion en curso. Devuelve TRUE si termino.
// 'tm' es DMA_QUEUE para el goteo de todos los frames (se reparte en el
// vblank) y DMA para los flush de emergencia, donde hay que tener los tiles YA
// y no se puede esperar al proximo vblank.
static bool lvl21StreamUpdate(TransferMethod tm) {
    if (loadSection >= LVL21_SECTIONS) return TRUE;

    u16 total = lvl21SecTiles[loadSection];
    if (loadDone >= total) return TRUE;

    u16 chunk = total - loadDone;
    if (chunk > LVL21_LOAD_TILES_PER_FRAME) chunk = LVL21_LOAD_TILES_PER_FRAME;

    u16 dst = secBase[loadSection] + loadDone;
    if (dst >= LVL21_RING_TILES) dst -= LVL21_RING_TILES;

    // Si el pedazo cruza el final del ring, se parte en dos.
    u16 tail = LVL21_RING_TILES - dst;
    if (chunk > tail) {
        VDP_loadTileData(lvl21Sec[loadSection]->tiles + loadDone * 8,
                         bgVramBase + dst, tail, tm);
        VDP_loadTileData(lvl21Sec[loadSection]->tiles + (loadDone + tail) * 8,
                         bgVramBase, chunk - tail, tm);
    } else {
        VDP_loadTileData(lvl21Sec[loadSection]->tiles + loadDone * 8,
                         bgVramBase + dst, chunk, tm);
    }
    loadDone += chunk;
    return loadDone >= total;
}

// Copia YA, de una, todo lo que quede de la seccion en curso. Se usa en el
// setup (la pantalla esta en negro) y cuando el revelado del plano alcanza a
// una seccion que todavia no termino de copiarse.
static void lvl21FlushSection(void) {
    while (!lvl21StreamUpdate(DMA)) { /* sigue copiando */ }
}

// Reserva las secciones que el progreso de camara ya alcanzo. 'prog' es
// camX + camY, la misma metrica que usa el generador.
static void lvl21EnsureSections(s16 prog, bool immediate) {
    while (nextSection < LVL21_SECTIONS &&
           (s16)lvl21SecStart[nextSection] - LVL21_PRELOAD_PX <= prog) {
        lvl21FlushSection();          // terminar la anterior antes de pisar el ring
        lvl21BeginSection(nextSection);
        nextSection++;
        if (immediate) lvl21FlushSection();
    }
}

static void lvl21BgInit(u16 vramBase, s16 camX, s16 camY) {
    bgVramBase  = vramBase;
    ringPos     = 0;
    nextSection = 0;
    loadSection = LVL21_SECTIONS;     // "nada en curso"
    loadDone    = 0;
    for (u16 i = 0; i < LVL21_SECTIONS; i++) secBase[i] = 0;

    VDP_setPlaneSize(BG_PLANE_W, BG_PLANE_H, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    lvl21EnsureSections(camX + camY, TRUE);

    planeCol = (camX + SCREEN_PIXEL_WIDTH - 1) >> 3;
    planeRow = (camY + SCREEN_PIXEL_HEIGHT - 1) >> 3;
    for (s16 c = planeCol - LVL21_VIS_COLS + 1; c <= planeCol; c++)
        for (s16 r = planeRow - LVL21_VIS_ROWS + 1; r <= planeRow; r++)
            if (c >= 0 && r >= 0) lvl21DrawCell(c, r);
}

// Revela lo que haga falta para esta posicion de camara y aplica el scroll.
static void lvl21BgUpdate(s16 camX, s16 camY) {
    lvl21EnsureSections(camX + camY, FALSE);
    lvl21StreamUpdate(DMA_QUEUE);

    s16 needCol = (camX + SCREEN_PIXEL_WIDTH - 1) >> 3;
    s16 needRow = (camY + SCREEN_PIXEL_HEIGHT - 1) >> 3;

    // Si el revelado alcanza a una seccion a medio copiar, hay que terminarla
    // antes de escribir tilemap que la referencie.
    if (needCol > planeCol || needRow > planeRow) lvl21FlushSection();

    while (planeCol < needCol) {
        planeCol++;
        for (s16 r = planeRow - LVL21_VIS_ROWS + 1; r <= planeRow; r++)
            if (r >= 0) lvl21DrawCell(planeCol, r);
    }
    while (planeRow < needRow) {
        planeRow++;
        for (s16 c = planeCol - LVL21_VIS_COLS + 1; c <= planeCol; c++)
            if (c >= 0) lvl21DrawCell(c, planeRow);
    }

    VDP_setHorizontalScroll(BG_B, -camX);
    VDP_setVerticalScroll(BG_B, camY);
}

// ---------------------------------------------------------------------------
// Corredor de camara
// ---------------------------------------------------------------------------
static s16 camFloorX(s16 camY) {
    s16 i = (camY - LVL21_CAM_Y_MIN) >> 3;
    if (i < 0) i = 0;
    if (i >= LVL21_CAM_ROWS) i = LVL21_CAM_ROWS - 1;
    return (s16)lvl21CamFloor[i];
}

static s16 camCeilX(s16 camY) {
    s16 i = (camY - LVL21_CAM_Y_MIN) >> 3;
    if (i < 0) i = 0;
    if (i >= LVL21_CAM_ROWS) i = LVL21_CAM_ROWS - 1;
    return (s16)lvl21CamCeil[i];
}

// ---------------------------------------------------------------------------
// Area caminable: la tabla generada + la cornisa
// ---------------------------------------------------------------------------
// (fx, fy) = punto de PIES: el centro horizontal del frame del personaje.
static inline u16 walkCol(s16 fx) {
    if (fx < 0) return 0;
    u16 c = (u16)fx / LVL21_LIM_STEP;
    return (c >= LVL21_LIM_COLS) ? (u16)(LVL21_LIM_COLS - 1) : c;
}
static inline s16 walkTopAt(s16 fx) { return (s16)lvl21WalkTop[walkCol(fx)]; }
static inline s16 walkBotAt(s16 fx) { return (s16)lvl21WalkBot[walkCol(fx)]; }

// Calle (el piso de siempre). Una columna sin calle tiene top > bot.
static bool onStreet(s16 fx, s16 fy) {
    if (fx < 0 || fx >= LVL21_WORLD_W) return FALSE;
    return (fy >= walkTopAt(fx) && fy <= walkBotAt(fx));
}

static inline bool inPlatX(s16 fx) {
    return (fx >= LVL21_PLAT_X0 && fx <= LVL21_PLAT_X1);
}
static inline bool inPlatY(s16 fy) {
    return (fy >= LVL21_PLAT_Y0 && fy <= LVL21_PLAT_Y1);
}

// Que puede pisar/atravesar un personaje. 'air' = esta en el aire.
// Entre la cornisa y la calle hay ~78px de PARED (los portones): solo se
// pueden cruzar EN EL AIRE, que es lo que hace que a la plataforma haya que
// subir saltando y que bajarse sea dejarse caer.
static bool walkable(s16 fx, s16 fy, bool air) {
    if (onStreet(fx, fy)) return TRUE;
    if (!inPlatX(fx))     return FALSE;
    if (inPlatY(fy))      return TRUE;
    return (air && fy > LVL21_PLAT_Y1 && fy < walkTopAt(fx));
}

#define FOOT_DX  (PLAYER_SPRITE_W / 2)   // de x (borde del frame) al eje de los pies

// Para un X de pies dado, devuelve una Y valida dentro de la calle (o -1 si esa
// columna no tiene calle). Se usa para soltar a los foot soldiers en un punto
// caminable: la calle se corre con X, asi que la profundidad buena depende de X.
static s16 streetFeetY(s16 fx, u16 seed) {
    s16 lo = walkTopAt(fx), hi = walkBotAt(fx);
    if (lo > hi) return -1;
    if (hi - lo < 8) return lo;
    return lo + 4 + (s16)(seed % (u16)(hi - lo - 7));
}

// Recorta una posicion al area caminable. Se llama DESPUES de update*, con la
// posicion que tenia antes: si el movimiento completo no entra, se prueba solo
// el eje X y despues solo el eje Y, de forma que caminar en diagonal contra el
// borde de la vereda "resbale" en vez de frenarse en seco.
static void clampToWalk(s16* px, s16* py, s16 prevX, s16 prevY,
                        s16 footDx, bool air) {
    if (walkable(*px + footDx, *py, air)) return;
    if (walkable(*px + footDx, prevY, air)) { *py = prevY; return; }
    if (walkable(prevX + footDx, *py, air)) { *px = prevX; return; }
    *px = prevX;
    *py = prevY;
}

// Un paso de jugador: recorte + entrada/salida de la cornisa.
// 'onPlat' entra con el estado del frame anterior y sale con el nuevo.
static void playerStepStreet(Player* p, s16 prevX, s16 prevY,
                             bool* onPlat, bool* wasAir) {
    bool air = isPlayerJumping(p);
    s16  fx  = p->x + FOOT_DX;

    // BAJARSE de la cornisa: caminando hacia abajo, o saliendose por un
    // costado. No se frena: se deja caer a la calle.
    if (*onPlat && !air && (p->y > LVL21_PLAT_Y1 || !inPlatX(fx))) {
        if (p->y < LVL21_PLAT_Y0) p->y = LVL21_PLAT_Y0;   // no deberia pasar
        playerFallTo(p, walkTopAt(fx));
        *onPlat = FALSE;
        *wasAir = TRUE;
        return;
    }

    // SUBIRSE: el tiron extra de los ultimos px, mientras se pide arriba.
    if (air && inPlatX(fx) && (JOY_readJoypad(p->joyId) & BUTTON_UP) &&
        p->y > LVL21_PLAT_Y1 && p->y <= LVL21_PLAT_Y1 + LVL21_LEDGE_ASSIST) {
        p->y -= LVL21_LEDGE_PULL;
        if (p->y < LVL21_PLAT_Y1) p->y = LVL21_PLAT_Y1;
    }

    clampToWalk(&p->x, &p->y, prevX, prevY, FOOT_DX, air);
    fx = p->x + FOOT_DX;

    // ATERRIZAJE: recien ahi se decide si quedo arriba o abajo.
    if (*wasAir && !air) {
        if (inPlatX(fx) && p->y <= LVL21_PLAT_Y1 + LVL21_LEDGE_SNAP &&
            p->y >= LVL21_PLAT_Y0) {
            if (p->y > LVL21_PLAT_Y1) p->y = LVL21_PLAT_Y1;
            *onPlat = TRUE;                 // clavo el salto en la cornisa
        } else {
            *onPlat = FALSE;
            if (!onStreet(fx, p->y))        // aterrizo contra los portones
                playerFallTo(p, walkTopAt(fx));
        }
    }
    *wasAir = isPlayerJumping(p);
}

// ===========================================================================
// La escena
// ===========================================================================
SceneId showScene21() {
    clearScene();

    // Presupuesto de VRAM de sprites. Con planos de 64x32 el area de usuario va
    // de TILE_USER_INDEX (16) a TILE_FONT_INDEX (1440); de ahi salen, en este
    // orden, las barras de vida del HUD, el ring del fondo y, al final, los
    // sprites. (17/09) La cuenta se hace ACA con los propios #define en vez de
    // estar escrita a mano: cuando el HUD paso de 8 a 10 tiles por jugador el
    // numero fijo (616) se quedo viejo y el ring del fondo y el motor de
    // sprites se pisaban 12 tiles.
    //
    //   2 jugadores: 1440 - 16 - 20 - 768 = 636
    //   4 jugadores: 1440 - 16 - 40 - 768 = 616
    //
    // Con 636 entran 2 tortugas (169 tiles cada una) + los 2 marcos del HUD
    // (36 cada uno) = 410, los 2 retratos que aparecen al continuar (16 cada
    // uno) y 2 foot soldiers morados (80 cada uno). En 1 jugador sobra para 4.
    const u16 hudTiles = (u16)(((numJugadores() > 2) ? MAX_PLAYERS : 2)
                               * HUD_VRAM_PER_PLAYER);
    SPR_initEx((u16)(TILE_FONT_INDEX - (TILE_USER_INDEX + hudTiles
                                        + LVL21_RING_TILES)));

    VDP_setScreenWidth320();
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setBackgroundColor(0);
    VDP_clearPlane(BG_A, TRUE);

    // (15/09) N jugadores (1..4), igual que scenes.c. Este modulo se habia
    // quedado en 2 y por eso el modo de 4 "se volvia de dos" al entrar acá.
    u8   nPl = numJugadores();
    bool dosJugadores = (nPl >= 2);

    s16 cameraX = 0;
    s16 cameraY = LVL21_CAM_Y_MIN;

    // El fondo arranca despues de los tiles reservados al HUD (las dos barras
    // de vida). clearScene dejo las paletas en negro, asi que todo el setup
    // pasa invisible y la escena se revela con un fade al final.
    u16 hudVram = TILE_USER_INDEX;
    // Un bloque de barra POR JUGADOR (minimo 2, para que con 1-2 jugadores el
    // fondo arranque exactamente donde arrancaba antes).
    u16 barBlocks = (u16)((nPl > 2) ? MAX_PLAYERS : 2) * HUD_VRAM_PER_PLAYER;
    u16 bgVram  = TILE_USER_INDEX + barBlocks;
    lvl21BgInit(bgVram, cameraX, cameraY);

    hudInit();

    Player p1, p2, p3, p4;
    Player* pls[MAX_PLAYERS] = { &p1, &p2, &p3, &p4 };
    // Estado de la cornisa por jugador: si esta parado arriba y si venia del
    // aire (para detectar el frame del aterrizaje).
    bool plOnPlat[MAX_PLAYERS] = { FALSE, FALSE, FALSE, FALSE };
    bool plWasAir[MAX_PLAYERS] = { FALSE, FALSE, FALSE, FALSE };
    for (u8 k = 0; k < nPl; k++) {
        // La calle no es un pasillo recto: al motor se le da la franja de
        // profundidad COMPLETA (la cornisa incluida) y el recorte fino lo hace
        // clampToWalk() contra la tabla de limites. Los jugadores 2..4 se
        // escalonan para no nacer encimados, pero SIN salirse de la franja
        // muerta de la camara (ver START_SPREAD_X_*).
        const s16 spreadX = (nPl > 2) ? START_SPREAD_X_4P : START_SPREAD_X_2P;
        initPlayer(pls[k], playerChar(k), playerJoy(k), PAL1,
                   (s16)(START_P1_X + k * spreadX),
                   (s16)(k ? START_P2_Y + (k - 1) * START_DEPTH_STEP
                           : START_P1_Y));
        setPlayerLane(pls[k], LVL21_WALK_Y_MIN, LVL21_WALK_Y_MAX);
        setPlayerEndWall(pls[k], 0, 0);           // este nivel no tiene pared diagonal
        setPlayerRightBound(pls[k], SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W);
    }

    VDP_loadFont(&hud_font, DMA);
    VDP_setTextPlane(BG_A);
    VDP_setTextPriority(1);
    VDP_setTextPalette(PAL1);

    p2JoinReset();   // (17/09) invitacion "PULSE START" en el marco vacio del P2
    HudPlayer huds[MAX_PLAYERS];
    for (u8 k = 0; k < nPl; k++)
        hudPlayerInit(&huds[k], pls[k], hudPlayerCol(k),
                      (u16)(hudVram + k * HUD_VRAM_PER_PLAYER));

    // --- Foot soldiers (provisorio: spawn al azar, sin oleadas guionadas) ---
    resetEnemyAI(cantidadJugadores);
    // static, no en el stack: 8 Enemy son ~1KB y esta funcion ya tiene dos
    // Player y dos HudPlayer encima.
    static Enemy enemies[MAX_ENEMIES];
    // Cornisa: mismo estado que los jugadores, mas la trepada en curso
    // (0 = en el piso o ya arriba; >0 = subiendo; <0 = bajando).
    static bool eOnPlat[MAX_ENEMIES];
    static s8   eClimb[MAX_ENEMIES];
    for (u16 i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].state  = ENEMY_STATE_INACTIVE;
        enemies[i].sprite = NULL;
        eOnPlat[i] = FALSE;
        eClimb[i]  = 0;
    }
    const u16 maxAlive = dosJugadores ? LVL21_MAX_ALIVE_2P : LVL21_MAX_ALIVE_1P;
    (void)dosJugadores;
    s16 nextSpawnAt = LVL21_SPAWN_EVERY;   // progreso de camara del proximo spawn
    u16 rng = 0x2B7D;                      // LFSR de 16 bits: el VDP no tiene random

    // Revelado: PAL0 el fondo, PAL1 la de las tortugas (la dejo cargada
    // initPlayer), PAL2 la de los foot soldiers.
    u16 target[64];
    for (u16 i = 0; i < 16; i++) {
        target[i]      = lvl21_pal.data[i];
        target[16 + i] = leo_player.palette->data[i];
        target[32 + i] = foot_soldier.palette->data[i];
        target[48 + i] = 0;
    }
    // Colocar los sprites en pantalla ANTES del fundido: updatePlayer solo
    // sabe de cameraX, asi que el desplazamiento vertical de la camara hay que
    // aplicarlo a mano (lo mismo que se hace despues en cada frame).
    for (u8 k = 0; k < nPl; k++)
        if (pls[k]->sprite)
            SPR_setPosition(pls[k]->sprite, pls[k]->x - cameraX,
                            pls[k]->y - PLAYER_FOOT_OFFSET - cameraY);
    SPR_update();

    static const u16 black[64] = { 0 };
    PAL_setColors(0, black, 64, DMA);
    PAL_fadeInAll(target, 20, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    // --- Bucle principal ---------------------------------------------------
    bool running = TRUE;
    while (running) {
        for (u8 k = 0; k < nPl; k++) {
            s16 prevX = pls[k]->x, prevY = pls[k]->y;
            updatePlayer(pls[k]);
            playerStepStreet(pls[k], prevX, prevY, &plOnPlat[k], &plWasAir[k]);
        }

        // --- Camara ------------------------------------------------------
        // Lidera el que va mas adelante (el que mas avanzo en la "L", o sea
        // el de mayor x + y).
        s16 leadX = p1.x, leadY = p1.y;
        for (u8 k = 1; k < nPl; k++)
            if ((pls[k]->x + pls[k]->y) > (leadX + leadY)) { leadX = pls[k]->x; leadY = pls[k]->y; }

        // Vertical: solo se mueve cuando los pies salen de la franja muerta.
        s16 feetScreenY = leadY - cameraY;
        s16 newCamY = cameraY;
        if (feetScreenY > CAM_DEAD_ZONE_BOT) newCamY = leadY - CAM_DEAD_ZONE_BOT;
        else if (feetScreenY < CAM_DEAD_ZONE_TOP) newCamY = leadY - CAM_DEAD_ZONE_TOP;
        if (newCamY < LVL21_CAM_Y_MIN) newCamY = LVL21_CAM_Y_MIN;
        if (newCamY > LVL21_CAM_Y_MAX) newCamY = LVL21_CAM_Y_MAX;
        if (newCamY - cameraY >  CAM_MAX_SPEED_Y) newCamY = cameraY + CAM_MAX_SPEED_Y;
        if (newCamY < cameraY) newCamY = cameraY;      // la camara nunca sube

        // FRENO: camY no puede adelantarse a camX. El corredor se estrecha al
        // bajar (camFloorX crece), y camX solo puede seguirlo a CAM_MAX_SPEED_X
        // por frame: si camY bajara primero, durante esos frames el viewport
        // quedaria a la izquierda del arte y asomaria el fondo negro. Asi que
        // camY espera a que camX llegue.
        while (newCamY > cameraY && camFloorX(newCamY) > cameraX + CAM_MAX_SPEED_X)
            newCamY--;
        cameraY = newCamY;

        // Horizontal: dead-zone a la derecha, igual que el nivel 1.
        s16 leadScreenX = leadX - cameraX;
        s16 newCamX = cameraX;
        if (leadScreenX > CAM_DEAD_ZONE_RIGHT)
            newCamX = cameraX + (leadScreenX - CAM_DEAD_ZONE_RIGHT);
        if (newCamX - cameraX > CAM_MAX_SPEED_X) newCamX = cameraX + CAM_MAX_SPEED_X;

        // El corredor manda: bajar obliga a correrse a la derecha, y el techo
        // frena el avance hasta que la camara termine de bajar.
        s16 loX = camFloorX(cameraY);
        s16 hiX = camCeilX(cameraY);
        if (newCamX < loX) {
            newCamX = cameraX + CAM_MAX_SPEED_X;
            if (newCamX > loX) newCamX = loX;
        }
        if (newCamX > hiX) newCamX = hiX;
        if (newCamX > LVL21_CAM_X_MAX) newCamX = LVL21_CAM_X_MAX;
        if (newCamX < cameraX) newCamX = cameraX;      // nunca retrocede
        cameraX = newCamX;

        // --- Limites del jugador contra la camara -------------------------
        s16 rightBound = cameraX + SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W;
        for (u8 k = 0; k < nPl; k++) {
            setPlayerCamera(pls[k], cameraX);
            setPlayerLeftBound(pls[k], cameraX);
            setPlayerRightBound(pls[k], rightBound);
        }

        // El scroll vertical del fondo NO mueve los sprites: hay que restarlo
        // a mano al dibujar al jugador (updatePlayer solo sabe de cameraX).
        for (u8 k = 0; k < nPl; k++)
            if (pls[k]->sprite)
                SPR_setPosition(pls[k]->sprite, pls[k]->x - cameraX,
                                pls[k]->y - PLAYER_FOOT_OFFSET - pls[k]->jumpZ - cameraY);

        // --- Foot soldiers -------------------------------------------------
        u16 alive = 0;
        for (u16 i = 0; i < MAX_ENEMIES; i++)
            if (enemies[i].state != ENEMY_STATE_INACTIVE) alive++;

        // Spawn: cada LVL21_SPAWN_EVERY px de avance de camara, si hay lugar.
        s16 progress = cameraX + cameraY;
        if (progress >= nextSpawnAt) {
            nextSpawnAt = progress + LVL21_SPAWN_EVERY;
            if (alive < maxAlive) {
                for (u16 i = 0; i < MAX_ENEMIES; i++) {
                    if (enemies[i].state != ENEMY_STATE_INACTIVE) continue;
                    // LFSR x^16+x^14+x^13+x^11+1: alcanza y sobra para sortear
                    // el lado de entrada y la profundidad.
                    rng = (u16)((rng >> 1) ^ (u16)(-(rng & 1) & 0xB400));
                    bool fromRight = (rng & 1) != 0;
                    s16  sx = fromRight ? (cameraX + SCREEN_PIXEL_WIDTH + LVL21_SPAWN_MARGIN)
                                        : (cameraX - LVL21_SPAWN_MARGIN);
                    // La profundidad se sortea sobre la calle A LA ALTURA del
                    // jugador, no del punto de entrada: en la diagonal el
                    // poligono se corre con X y el soldier caeria fuera.
                    s16 fy = streetFeetY(p1.x + FOOT_DX, rng >> 4);
                    if (fy < 0) break;             // esa columna no tiene calle
                    initEnemySpawn(&enemies[i], sx, fy, 48, PAL2,
                                   ENEMY_TYPE_FOOT_SOLDIER);
                    // La calle no es el pasillo del nivel 1: franja de
                    // profundidad completa, sin pared diagonal, y el ancho real.
                    setEnemyBounds(&enemies[i], LVL21_WALK_Y_MIN, LVL21_WALK_Y_MAX,
                                   0, 0, LVL21_MAP_W * 8);
                    eOnPlat[i] = FALSE;
                    eClimb[i]  = 0;
                    break;
                }
            }
        }

        separateEnemies(enemies, MAX_ENEMIES);
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            Enemy* e = &enemies[i];
            if (e->state == ENEMY_STATE_INACTIVE) continue;
            s16 prevEX = e->x, prevEY = e->y;
            setEnemyCamera(e, cameraX);
            updateEnemyN(e, pls, nPl);

            // --- La cornisa, para los soldiers -----------------------------
            // updateEnemyN persigue al jugador en X y en Y sin saber nada de
            // la pared, asi que el que tiene a su tortuga arriba se queda
            // pegado al limite de la vereda. Cuando eso pasa, TREPA: se le
            // interpola la profundidad desde la calle hasta la cornisa a
            // LVL21_CLIMB_SPEED px por frame (unos 13 frames, lo mismo que
            // dura un salto), y en pantalla se lo ve subir. Si su tortuga se
            // baja, hace el camino inverso.
            s16 ecx = getEnemyCenterX(e);
            u8  tk  = (e->target < nPl) ? e->target : 0;
            bool tgtArriba = plOnPlat[tk];

            if (eClimb[i] == 0) {
                if (!eOnPlat[i] && tgtArriba && inPlatX(ecx) &&
                    e->y <= walkTopAt(ecx) + 2) {
                    eClimb[i] = 1;                    // arranca a subir
                } else if (eOnPlat[i] && (!tgtArriba || !inPlatX(ecx))) {
                    eClimb[i] = -1;                   // se baja
                }
            }

            if (eClimb[i] > 0) {                      // SUBIENDO
                e->y -= LVL21_CLIMB_SPEED;
                if (e->y <= LVL21_PLAT_Y1) {
                    e->y = LVL21_PLAT_Y1;
                    eOnPlat[i] = TRUE;
                    eClimb[i]  = 0;
                }
            } else if (eClimb[i] < 0) {               // BAJANDO
                s16 destino = walkTopAt(ecx);
                e->y += LVL21_CLIMB_SPEED;
                if (e->y >= destino) {
                    e->y = destino;
                    eOnPlat[i] = FALSE;
                    eClimb[i]  = 0;
                }
            } else {
                // Recorte normal contra la tabla. Estando en la cornisa se le
                // permite la franja de la plataforma; en el piso, la calle.
                clampToWalk(&e->x, &e->y, prevEX, prevEY,
                            (s16)(ecx - e->x), FALSE);
                if (eOnPlat[i] && !inPlatY(e->y)) eOnPlat[i] = FALSE;
            }

            // La Y de pantalla va corregida por el scroll vertical
            // (updateEnemy solo sabe de camX) y por jumpZ (el blanco salta).
            if (e->sprite && e->state != ENEMY_STATE_INACTIVE)
                SPR_setPosition(e->sprite,
                                e->x - cameraX,
                                e->y - e->footOffset - e->jumpZ - cameraY);
        }

        // Golpe del jugador al soldier.
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (!enemyCanBeHit(&enemies[i])) continue;
            s16 ex = getEnemyCenterX(&enemies[i]);
            s16 ey = getEnemyCenterY(&enemies[i]);
            s16 hw = enemyBodyHalfW(&enemies[i]);
            s16 bh = enemyBodyH(&enemies[i]);
            Player* att = NULL;
            s16 dmg = 0;
            for (u8 k = 0; k < nPl; k++) {
                if (!playerAttackHitsBox(pls[k], ex, ey, hw, bh)) continue;
                dmg = isPlayerSpecialAttack(pls[k]) ? ENEMY_HP : 1; att = pls[k];
                break;
            }
            if (dmg > 0) {
                damageEnemy(&enemies[i], dmg);
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                               SOUND_PCM_CH2, 15, FALSE, FALSE);
                if (att && enemies[i].state == ENEMY_STATE_DEAD) {
                    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                                   SOUND_PCM_CH3, 15, FALSE, FALSE);
                    addPlayerScore(att, 1);
                }
            }
        }

        // Golpe del soldier al jugador.
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            Enemy* e = &enemies[i];
            if (e->state != ENEMY_STATE_ATTACK) continue;
            // El swing es UNO: pega al primer jugador alcanzado y se consume.
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                if (!enemyTryHitPlayerBox(e, getPlayerWorldX(pls[k]), getPlayerY(pls[k]),
                                          PLAYER_BODY_HALF_W)) continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                               SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(pls[k], getEnemyCenterX(e));
                break;
            }
        }

        lvl21BgUpdate(cameraX, cameraY);

#if LVL21_CAM_DEBUG
        // Overlay de calibracion: camX/camY y el techo del corredor en la fila
        // de abajo. Sirve para cotejar los "topes" contra el mapa anotado.
        {
            char dbg[32];
            sprintf(dbg, "X%4d Y%3d T%4d ", cameraX, cameraY, camCeilX(cameraY));
            VDP_drawText(dbg, 2, 26);
        }
#endif

        // --- Entrada del P2 en plena partida (17/09) -----------------------
        // Con un solo jugador, su marco muestra "PULSE / START" y el joystick
        // 2 puede sumarse eligiendo una tortuga distinta a la del P1. El nivel
        // NO se pausa: sigue corriendo mientras elige (ver p2JoinPoll).
        if (nPl == 1) {
            u8 ch2 = p2JoinPoll(hudPlayerCol(1));
            if (ch2 != 0xFF) {
                personaje2Seleccionado = ch2;
                cantidadJugadores      = 2;
                initPlayer(&p2, ch2, playerJoy(1), PAL1,
                           (s16)(getPlayerWorldX(&p1) - 48), getPlayerY(&p1));
                setPlayerRightBound(&p2, SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W);
                setPlayerLane(&p2, LVL21_WALK_Y_MIN, LVL21_WALK_Y_MAX);
                setPlayerEndWall(&p2, 0, 0);
                hudPlayerInit(&huds[1], &p2, hudPlayerCol(1),
                              (u16)(hudVram + HUD_VRAM_PER_PLAYER));
                nPl          = 2;
                dosJugadores = TRUE;
                resetEnemyAI(2);
            }
        }

        for (u8 k = 0; k < nPl; k++) hudPlayerUpdate(&huds[k]);

        // --- Fin del nivel -------------------------------------------------
        // La camara llego al tope y el jugador esta pegado al borde derecho.
        // Se mide sobre los PIES (leadX es el borde del frame de 104px), que es
        // la misma coordenada con la que trabaja el poligono.
        if (cameraX >= LVL21_CAM_X_MAX && leadX + FOOT_DX >= LVL21_END_X)
            running = FALSE;

        // Atajo de calibracion: START + A corta el nivel sin recorrerlo entero.
        u16 joy = JOY_readJoypad(JOY_1);
        if ((joy & BUTTON_START) && (joy & BUTTON_A)) running = FALSE;

        SPR_update();
        SYS_doVBlankProcess();
    }

    // --- Fin del nivel: imagen CONGELADA + jingle de nivel completado -------
    // Nada se actualiza: ni jugador, ni enemigos, ni camara, ni fondo. Solo se
    // sigue llamando a SYS_doVBlankProcess para que el VDP y el driver de audio
    // sigan corriendo, asi que la pantalla queda clavada en el ultimo frame.
    //
    // music_scene_clear son ~4s SIN punto de loop: setLoopNumber(0) va SIEMPRE
    // ANTES del play (el driver latchea el numero de loops en el instante del
    // play), o el tema se repite para siempre.
    XGM2_stop();
    XGM2_setLoopNumber(0);
    playMusicVol(music_scene_clear, VOL_MUSIC_SCENE_CLEAR);

    u16 hold = (IS_PAL_SYSTEM ? 50 : 60) * LVL21_CLEAR_SECS;
    while (hold > 0) {
        hold--;
        SYS_doVBlankProcess();
    }
    XGM2_stop();

    clearScene();
    return SCENE_GAME_OVER;   // y showGameOver() vuelve al logo de SEGA
}
