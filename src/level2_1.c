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
// (19/09) YA NO HAY SPAWN AL AZAR: los enemigos son los que Gustavo marco
// sobre el arte -- cinco bocas de tormenta y un tirador de dinamita. Ver el
// bloque "ENEMIGOS GUIONADOS" mas abajo.
//
// (24/09) Musica de nivel: "08 - Downtown (Stage 2-1)" (music_stage2_1).
// Arranca con el fundido de entrada y la corta el tema del jefe.
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
#include "bebop.h"    // jefe del nivel (24/09)
#include "meters_2_1.h" // parquimetros (25/09)
#include "pause_menu.h" // pausa con START del control 1 + selector de niveles (26/09)
#include "audio.h"    // music_stage2_1, music_scene_clear, music_boss, boss_scream_bebop_vo

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
// ENEMIGOS GUIONADOS (19/09)
// ---------------------------------------------------------------------------
// (19/09) SE ELIMINO el spawn al azar que habia (un soldier cada 220px de
// avance de camara, entrando por un costado). Gustavo marco sobre el arte las
// posiciones reales y mientras se prueban no tiene que haber nada mas en
// pantalla que las enturbie.
//
// BOCAS DE TORMENTA. En el mapa hay cinco alcantarillas por las que sale un
// foot soldier morado con la anim 17: levanta la tapa y la tira en linea recta
// hacia la tortuga. Cada una tiene:
//
//   holeX/holeY  centro del agujero MEDIDO SOBRE EL ARTE
//                (res/images/lvl_2_scene/"Stage 2-_16_colors_v2.png"; las
//                coordenadas del PNG son directamente las de mundo, el
//                generador no aplica ningun offset).
//   trigX        X de PIES del jugador que la dispara.
//   dir          +1 tira la tapa a la derecha, -1 a la izquierda.
//
// La PRIMERA es la unica que ataca POR LA ESPALDA: se dispara cuando los
// jugadores YA PASARON la tapa (trigX 174 > holeX 96), asi que la tapa sale
// hacia la derecha. (25/09) Era 283 y el agujero quedaba fuera de cuadro al
// disparar; 174 es la X de pies que marco Gustavo en una captura (Leo parado
// a ~80 px de la boca), y con la camara arrancando en 32 la boca queda a la
// vista en pantalla x=64. Las otras cuatro se disparan cuando el jugador ESTA POR
// LLEGAR, con LVL21_MANHOLE_LEAD px de anticipo, y tiran hacia la izquierda.
//
// POR QUE 120 PX DE ANTICIPO. La camara sigue al lider con una franja muerta de
// CAM_DEAD_ZONE_RIGHT (120) medida sobre el BORDE del frame de 104px, o sea
// que en marcha camX = piesLider - 172. La boca de tormenta cae entonces en
//     pantallaX = holeX - camX = LVL21_MANHOLE_LEAD + 172
// Con 120 eso da 292: el frame del soldier (64px, centrado) ocupa 260..324, o
// sea que se ve salir entero salvo 4px. Subirlo de 148 lo deja FUERA de cuadro
// cuando dispara.
// (19/09) Era 96. Se subio 24 al agregarle el salto de salida: la animacion
// paso de 48 a 64 ticks, y esos 16 frames de mas son 32px que el jugador
// avanza antes de que salga la tapa. Con 120 la distancia a la que tira la
// tapa vuelve a ser la de antes.
#define LVL21_MANHOLES        5
#define LVL21_MANHOLE_LEAD  120   // px de anticipo (bocas 2..5)

// De la coordenada del agujero a la del enemigo:
//   X  el frame del morado son 64px y la tapa esta centrada en el -> -32
//   Y  'y' es la LANE (los PIES). En el frame 0 la tapa ocupa y=62..79, o sea
//      18px pegados al borde de abajo; para que caiga centrada en el agujero
//      los pies van 10px por debajo de su centro.
//      OJO: al enemigo se le pasa la lane FINAL, o sea con ENEMY_MANHOLE_HOP_DROP
//      ya sumado -- sale de un salto y aterriza por delante de la boca. Mientras
//      dura el salto el sprite se dibuja mas arriba con jumpZ, que es altura
//      visual y no toca el area caminable (ver enemy.h).
#define LVL21_MANHOLE_DX    (-(ENEMY_SPRITE_W_PURPLE / 2))
#define LVL21_MANHOLE_DY      10

// La tapa CERRADA que se ve mientras el soldier todavia no salio. El arte del
// fondo tiene las bocas de tormenta ABIERTAS (elipse oscura), asi que sin esto
// la tapa aparece de la nada en el frame 0 de la animacion. Se dibuja con
// lid_sprite (12 tiles) en vez de con un foot_soldier congelado (80 tiles):
// es el mismo dibujo y cuesta la sexta parte de VRAM.
// Solo se instancia la de las bocas que estan cerca de camara.
#define LVL21_MANHOLE_ARM_MARGIN  64   // px fuera de pantalla que siguen armadas

// EL QUE TIRA DINAMITA. Misma logica que el de la escalera del 1-1 (ver
// TNT_THROWER_* en scenes.c): sale con la anim 16, tira UN cartucho a un punto
// FIJO y despues persigue como cualquiera. El flip va en la MISMA direccion
// que en el 1-1 (dir = +1): la anim de la dinamita esta dibujada mirando a la
// IZQUIERDA, al reves que el resto del sheet, asi que +1 es la que hace que
// mire hacia donde vienen las tortugas y el cartucho salga hacia la izquierda.
//
// Las distancias al punto de caida son las mismas del 1-1, medidas desde el
// CENTRO del que tira: -99 px en X y +23 en profundidad.
//
// (19/09) Corrido un tile a la IZQUIERDA y dos tiles ARRIBA, a ojo de Gustavo
// sobre la captura: 379 -> 371 -> 363 en X, y los 16px de alto van por
// LVL21_TNT_Z, NO bajandole la lane. Motivo: la tabla de limites da
// walkTop = 160 en toda esa cuadra, o sea que una lane de 155 cae DENTRO de la
// pared -- y clampToWalk no teletransporta, revierte: el soldier quedaria
// clavado ahi para siempre en cuanto intentara moverse. jumpZ es altura
// visual, no toca el area caminable, y ademas el bloque de SPAWNING de
// enemy.c ya lo baja a 0 despues de tirar el cartucho, o sea que "baja del
// portal" a la calle -- exactamente lo mismo que hace el de la escalera del
// 1-1 con TNT_THROWER_Z.
#define LVL21_TNT_CENTER_X   363   // lo que marca la flecha sobre el arte, -1 tile
#define LVL21_TNT_Y          171   // pies (la punta de la flecha) -- lane REAL
#define LVL21_TNT_Z           16   // 2 tiles de alto visual (el vano del portal)
#define LVL21_TNT_X         (LVL21_TNT_CENTER_X - ENEMY_SPRITE_W_PURPLE / 2)
#define LVL21_TNT_LAND_X    (LVL21_TNT_CENTER_X - 99)
#define LVL21_TNT_LAND_Y    (LVL21_TNT_Y + 23)
#define LVL21_TNT_TRIG_X    (LVL21_TNT_CENTER_X - 96)
#define LVL21_TNT_DIR        (+1)
#define LVL21_TNT_DMG_BARS    2    // igual que en el 1-1

// --- CAERSE POR UNA BOCA DE TORMENTA DESTAPADA (20/09) ---------------------
// Una vez que el soldier salio y tiro la tapa, el agujero queda ABIERTO y es
// una trampa: la tortuga que lo pise se cae adentro, pierde
// PLAYER_MANHOLE_DMG barras y se come la secuencia de la anim 21 (ver
// player.h). Antes de que salga el soldier la tapa esta puesta y no pasa nada,
// asi que la trampa solo existe con mhState == MH_DONE.
//
// SALTANDO NO SE CAE: playerCanBeHit() devuelve FALSE en STATE_JUMPING, y
// playerManholeFall se apoya en eso. Sale gratis y es la mecanica del arcade.
//
// El agujero mide 31x18 px. La caja de la trampa es un poco mas angosta en X
// (que haya que pisarlo de verdad) y un poco mas generosa en Y (la lane se
// mueve de a 2px y con 9 se podia cruzar en diagonal sin tocarlo).
#define LVL21_HOLE_HALF_W    12
#define LVL21_HOLE_HALF_H    10
// Donde reaparece al salir: los mismos px por debajo del centro del agujero
// para las cinco. Tiene que ser mayor que LVL21_HOLE_HALF_H o vuelve a caer en
// el acto -- que es justo lo que pidio Gustavo evitar.
#define LVL21_HOLE_OUT_DY    22

// Globo "Duuuh, who put the light out": se crea cuando una tortuga toca fondo
// y se suelta cuando empieza a salir. Uno solo aunque caigan las dos (son 48
// tiles de VRAM de sprites y el voice over es uno).
#define LVL21_BUBBLE_W       96
#define LVL21_BUBBLE_H       32
#define LVL21_BUBBLE_DY     (-40)   // borde superior respecto del centro del agujero
#define LVL21_BUBBLE_MARGIN    4    // no se pega a los bordes de pantalla

// Tope de foot soldiers vivos a la vez. No es diseno: es VRAM de sprites (ver
// la cuenta del SPR_initEx mas abajo). Un spawn guionado que llegue con el
// tope lleno NO se pierde: queda PENDIENTE y entra en cuanto se libera un
// lugar, asi no se saltea un tramo del guion por una casualidad de timing.
#define LVL21_MAX_ALIVE_1P     3
#define LVL21_MAX_ALIVE_2P     2

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
#define VOL_MUSIC_LEVEL2_1    90   // igual que el tema del 1-1 (VOL_MUSIC_LEVEL1)
#define VOL_MUSIC_BOSS_2_1    90   // tema del jefe, igual que el de Rocksteady

// (25/09) La camara ARRANCA corrida a la derecha (antes en 0) y las tortugas
// ENTRAN CAYENDO desde arriba por el lado izquierdo de la pantalla: nacen
// LVL21_DROP_Z px sobre su lane (fuera de cuadro) y bajan con la caida del
// salto, derivando LVL21_DROP_DRIFT px por frame hacia la derecha para que se
// lea como un salto desde la izquierda y no como una plomada. Los jugadores
// 2..4 caen escalonados (LVL21_DROP_STAGGER mas arriba cada uno).
// Con la camara en 32 el P1 nace con el borde del frame en pantalla x=8, y con
// la deriva aterriza unos 32 px mas a la derecha: todos siguen DENTRO de la
// franja muerta (ver la nota de abajo), asi que el nivel no scrollea solo.
#define LVL21_CAM_START_X   32
#define LVL21_DROP_Z       190   // alcanza para arrancar entero fuera de cuadro
#define LVL21_DROP_STAGGER  36
#define LVL21_DROP_DRIFT     1
#define LVL21_DROP_TICKS    60   // frames durante los que se aplica la deriva

// Punto de partida (el jugador sale del edificio, a la izquierda del todo)
// (26/09) TODOS caen en la MISMA X, cada uno en su lane: con la separacion en X
// el P2 aterrizaba mas a la derecha y, con la deriva de la caida, empujaba la
// camara. La calle en esa zona va de y=160 a 250.
#define START_P1_X        40
#define START_P1_Y       210
#define START_P2_Y       232     // un poco mas abajo que el P1
#define START_P3_Y       188
#define START_P4_Y       248

// (17/09) El P2 nacia fuera de la franja muerta de la camara
// (CAM_DEAD_ZONE_RIGHT = 120) y el nivel arrancaba con un scroll involuntario.
// Se lo acerco en X; desde el 26/09 directamente nacen todos en la misma X
// (ver arriba), separados solo en profundidad.

// ---------------------------------------------------------------------------
// Estado del streaming de fondo
// ---------------------------------------------------------------------------
// (17/09) La tabla la genera el script junto con el .res: la cantidad de
// secciones cambia con SEC_SPAN_PX y escrita a mano se desincronizaba.
#include "level2_1_sections.h"

// ---------------------------------------------------------------------------
// Tabla de bocas de tormenta (coordenadas medidas sobre el PNG del nivel)
// ---------------------------------------------------------------------------
typedef struct {
    s16 holeX, holeY;   // centro del agujero, en pixeles de mundo
    s16 trigX;          // X de PIES del lider que la dispara
    s8  dir;            // +1 = tira la tapa a la derecha, -1 = a la izquierda
} Manhole;

static const Manhole manholes[LVL21_MANHOLES] = {
    //  holeX holeY  trigX   dir
    {     96,  189,   174,  +1 },   // POR LA ESPALDA: se dispara ya pasada
    {    480,  189,   384,  -1 },   // 480 - 96
    {    768,  157,   672,  -1 },
    {   1240,  205,  1144,  -1 },
    {   2456,  589,  2360,  -1 },
};

// Estado de cada boca a lo largo del nivel.
#define MH_DORMANT   0   // todavia no la disparo nadie
#define MH_PENDING   1   // disparada, esperando lugar en el pool de enemigos
#define MH_DONE      2   // el soldier ya salio (o murio): no vuelve

static u8      mhState[LVL21_MANHOLES];
static Sprite* mhLid[LVL21_MANHOLES];      // tapa cerrada (solo cerca de camara)

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

    // (27/09) PARPADEO ARRIBA: el scroll se escribia en el acto, en pleno
    // cuadro. Las lineas que el VDP ya habia dibujado quedaban con el valor
    // del frame anterior y el resto con el nuevo: mientras la camara se mueve,
    // la franja de arriba del fondo "salta" un frame atrasada. Es el mismo
    // problema que resolvio Ray en su version de esta calle (bgUpdate3, en su
    // scenes.c): el scroll va a la cola y se aplica en el VBlank, junto con
    // los tiles y los sprites de ese frame.
    VDP_setHorizontalScrollVSync(BG_B, -camX);
    VDP_setVerticalScrollVSync(BG_B, camY);
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

static inline s16 absS16(s16 v) { return (v < 0) ? (s16)-v : v; }

// (19/09) Aca vivia streetFeetY(fx, seed), que devolvia una profundidad valida
// sorteada para la columna fx. La usaba SOLO el spawn al azar, que se saco al
// pasar a enemigos guionados. Si mas adelante hacen falta oleadas que entren
// por los costados, esta en el historial de git (commit del 19/09).

// Recorta una posicion al area caminable. Se llama DESPUES de update*, con la
// posicion que tenia antes: si el movimiento completo no entra, se prueba solo
// el eje X y despues solo el eje Y, de forma que caminar en diagonal contra el
// borde de la vereda "resbale" en vez de frenarse en seco.
// (25/09) Ademas de la tabla, las BASES de los parquimetros parados son
// solidas para el que camina (saltando se los pasa por arriba). Si el
// personaje YA estaba adentro de una base -- nacio ahi o lo empujo un golpe --
// se ignoran los parquimetros para dejarlo salir: clampToWalk revierte, no
// teletransporta, y sin esto quedaria clavado para siempre.
static bool canStep(s16 fx, s16 fy, bool air, bool ignoreMeters) {
    if (!walkable(fx, fy, air)) return FALSE;
    if (!air && !ignoreMeters && metersBlock(fx, fy)) return FALSE;
    return TRUE;
}

static void clampToWalk(s16* px, s16* py, s16 prevX, s16 prevY,
                        s16 footDx, bool air) {
    bool free = air || metersBlock(prevX + footDx, prevY);
    if (canStep(*px + footDx, *py, air, free)) return;
    if (canStep(*px + footDx, prevY, air, free)) { *py = prevY; return; }
    if (canStep(prevX + footDx, *py, air, free)) { *px = prevX; return; }
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

// ---------------------------------------------------------------------------
// Bocas de tormenta: la tapa cerrada y el disparo del soldier
// ---------------------------------------------------------------------------
// La tapa cerrada existe SOLO mientras la boca esta cerca de camara. Son 12
// tiles cada una y con las distancias de la tabla nunca hay mas de dos armadas
// a la vez, pero igual no tiene sentido pagarlas durante todo el nivel.
// ---------------------------------------------------------------------------
// TV de la vidriera "ELECTRONICS" (26/09)
// ---------------------------------------------------------------------------
// Arte del proyecto del companero: 4 frames de 56x48 que tapan EXACTO la
// pantalla azul oscura del televisor del fondo (mundo 480,72; medido contra
// "Stage 2-_16_colors_v2.png"). April hablando y Shredder que interrumpe.
//
// Paleta: la propia del sprite, en PAL3. En el 2-1 esa linea es la del jefe,
// pero Bebop recien aparece al final (bebopSpawn carga la suya), y para
// entonces la tele quedo muy atras.
//
// VRAM: son 42 tiles de sprites y el presupuesto del nivel esta hecho a la
// medida de 2 tortugas + 2 soldiers (ver SPR_initEx). La tele es decorado:
// solo se crea si, despues de crearla, siguen entrando los soldiers que
// todavia pueden aparecer; y si un spawn se queda sin lugar, se suelta antes
// (ver tvUpdate / tvYield). Asi nunca le saca el lugar a un enemigo.
//
// (27/09, Gustavo) UNA SOLA VEZ: la tele se prende cuando una tortuga (el
// centro de su cuerpo) llega a 2 tiles en X del televisor, pasa los 4 frames
// una vez (April, April, April, Shredder) y el sprite se suelta para siempre:
// queda la pantalla apagada del fondo. Si al llegar no hay VRAM, se prende
// en cuanto la haya (mientras siga en pantalla); si un spawn la desaloja a
// mitad de camino, no vuelve.
#define LVL21_TV_X       480
#define LVL21_TV_Y        72
#define LVL21_TV_W        56
#define LVL21_TV_MARGIN   32
#define LVL21_TV_NEAR     16      // 2 tiles en X desde el borde del televisor
#define LVL21_TV_FRAMES    4
#define LVL21_TV_TICKS    30      // ticks por frame (el time del .res)

typedef enum { TV_OFF, TV_PENDING, TV_PLAYING, TV_DONE } TvState;
static Sprite* tvSpr;
static TvState tvState;
static u16     tvTick;

static u16 soldierMaxTiles(void) {
    u16 a = foot_soldier.maxNumTile, b = foot_soldier_orange.maxNumTile;
    return (a > b) ? a : b;
}

// Suelta la tele si al proximo soldier no le alcanza la VRAM de sprites.
static void tvRelease(void) {
    if (tvSpr) { SPR_releaseSprite(tvSpr); tvSpr = NULL; }
}

static void tvYield(u16 alive, u16 maxAlive) {
    if (!tvSpr || alive >= maxAlive) return;
    if (SPR_getLargestFreeVRAMBlock() >= soldierMaxTiles()) return;
    tvRelease();
    tvState = TV_DONE;
}

static void tvUpdate(Player** pls, u8 nPl, s16 camX, s16 camY, u16 alive, u16 maxAlive,
                     bool allow) {
    if (tvState == TV_DONE) return;
    s16 sx = (s16)(LVL21_TV_X - camX);

    // Disparo: alguna tortuga en juego a 2 tiles (o menos) del televisor.
    if (tvState == TV_OFF) {
        for (u8 k = 0; k < nPl; k++) {
            if (isPlayerGameOver(pls[k])) continue;
            s16 cx = (s16)(pls[k]->x + PLAYER_SPRITE_W / 2);
            if (cx >= LVL21_TV_X - LVL21_TV_NEAR &&
                cx <= LVL21_TV_X + LVL21_TV_W + LVL21_TV_NEAR) {
                tvState = TV_PENDING;
                break;
            }
        }
    }

    if (tvState == TV_PENDING) {
        bool onScreen = sx > -(LVL21_TV_W + LVL21_TV_MARGIN) &&
                        sx < (s16)(SCREEN_PIXEL_WIDTH + LVL21_TV_MARGIN);
        if (!allow || !onScreen) { tvState = TV_DONE; return; }   // se la perdio
        u16 need = tv_april.maxNumTile;
        if (alive < maxAlive) need += (u16)((maxAlive - alive) * soldierMaxTiles());
        if (SPR_getFreeVRAM() >= need &&
            SPR_getLargestFreeVRAMBlock() >= tv_april.maxNumTile) {
            tvSpr = SPR_addSprite(&tv_april, 0, 0, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
            if (tvSpr) {
                SPR_setDepth(tvSpr, SPR_MAX_DEPTH);
                SPR_setAutoAnimation(tvSpr, FALSE);     // los frames van a mano
                SPR_setAnimAndFrame(tvSpr, 0, 0);
                tvTick = 0;
                tvState = TV_PLAYING;
            }
        }
    }

    if (tvState == TV_PLAYING) {
        if (!tvSpr) { tvState = TV_DONE; return; }
        u16 f = (u16)(tvTick / LVL21_TV_TICKS);
        if (f >= LVL21_TV_FRAMES) {                     // termino: se va para siempre
            tvRelease();
            tvState = TV_DONE;
            return;
        }
        SPR_setFrame(tvSpr, (s16)f);
        tvTick++;
        SPR_setPosition(tvSpr, sx, (s16)(LVL21_TV_Y - camY));
    }
}

static void mhReset(void) {
    for (u16 i = 0; i < LVL21_MANHOLES; i++) {
        mhState[i] = MH_DORMANT;
        mhLid[i]   = NULL;
    }
}

static void mhReleaseLid(u16 i) {
    if (mhLid[i]) SPR_releaseSprite(mhLid[i]);
    mhLid[i] = NULL;
}

static void mhReleaseAll(void) {
    for (u16 i = 0; i < LVL21_MANHOLES; i++) mhReleaseLid(i);
}

// Crea / mueve / libera las tapas cerradas segun donde esta la camara.
static void mhUpdateLids(s16 camX, s16 camY) {
    for (u16 i = 0; i < LVL21_MANHOLES; i++) {
        if (mhState[i] != MH_DORMANT) { mhReleaseLid(i); continue; }

        s16 sx = (s16)(manholes[i].holeX - camX);
        bool cerca = (sx > -(LID_W / 2 + LVL21_MANHOLE_ARM_MARGIN)) &&
                     (sx <  (s16)(SCREEN_PIXEL_WIDTH + LID_W / 2
                                  + LVL21_MANHOLE_ARM_MARGIN));
        if (!cerca) { mhReleaseLid(i); continue; }

        if (!mhLid[i])
            mhLid[i] = SPR_addSprite(&lid_sprite, 0, 0,
                                     TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        if (!mhLid[i]) continue;     // sin VRAM: se ve la boca abierta, nada mas

        SPR_setPosition(mhLid[i], (s16)(sx - LID_W / 2),
                        (s16)(manholes[i].holeY - LID_H / 2 - camY));
        // Esta EN EL PISO: que cualquier cosa que pise la misma lane le pase
        // por encima (depth mayor = mas atras).
        SPR_setDepth(mhLid[i],
                     (s16)(-(manholes[i].holeY + LVL21_MANHOLE_DY) + 2));
    }
}

// ===========================================================================
// La escena
// ===========================================================================
SceneId showScene21() {
    clearScene();

    // (27/09) Cola de DMA mas larga, como en la version de Ray (stInit en su
    // scenes.c): con la cola por defecto (80) el goteo del fondo + los sprites
    // + el scroll pueden llenarla, y lo que no entra se DESCARTA (tiles que no
    // llegan). Cambiar el tamano vacia la cola: va antes de encolar nada.
    DMA_setMaxQueueSize(192);

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

    s16 cameraX = LVL21_CAM_START_X;
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
        // clampToWalk() contra la tabla de limites. Todos nacen en la misma
        // X y en distinta lane (ver START_P*_Y).
        static const s16 startY[MAX_PLAYERS] = {
            START_P1_Y, START_P2_Y, START_P3_Y, START_P4_Y };
        initPlayer(pls[k], playerChar(k), playerJoy(k), PAL1,
                   START_P1_X, startY[k]);
        setPlayerLane(pls[k], LVL21_WALK_Y_MIN, LVL21_WALK_Y_MAX);
        setPlayerEndWall(pls[k], 0, 0);           // este nivel no tiene pared diagonal
        setPlayerRightBound(pls[k], cameraX + SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W);
        setPlayerLeftBound(pls[k], cameraX);
        setPlayerCamera(pls[k], cameraX);
        playerDropIn(pls[k], (s16)(LVL21_DROP_Z + k * LVL21_DROP_STAGGER));
    }
    u16 dropTicks = LVL21_DROP_TICKS;

    VDP_loadFont(&hud_font, DMA);
    VDP_setTextPlane(BG_A);
    VDP_setTextPriority(1);
    VDP_setTextPalette(PAL1);

    p2JoinReset();   // (17/09) invitacion "PULSE START" en el marco vacio del P2
    static HudPlayer huds[MAX_PLAYERS];
    // (26/09) CONTINUE? por jugador, igual que en el 1-1 y el 1-2. Antes el
    // 2-1 no tenia: el que se quedaba sin vidas quedaba tirado para siempre
    // y, si caian todos, el nivel no terminaba nunca.
    static ContPlayer conts[MAX_PLAYERS];
    contResetAll(conts);
    const u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    bool allOut = FALSE;
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
    u16 maxAlive = dosJugadores ? LVL21_MAX_ALIVE_2P : LVL21_MAX_ALIVE_1P;

    // Guion: bocas de tormenta + el tirador de dinamita. tntInit/lidInit dejan
    // los dos proyectiles en OFF (son estado de modulo, no del stack: si una
    // partida anterior murio con algo en el aire hay que limpiarlo).
    mhReset();
    tvSpr = NULL;     // la tele se prende una vez al acercarse (tvUpdate)
    tvState = TV_OFF;
    tvTick = 0;
    tntInit();
    lidInit();
    metersInit();
    // --- Jefe (24/09) ------------------------------------------------------
    // Vive en el stack de la escena como todo lo demas; bebopInit ademas deja
    // el pool de disparos limpio.
    static Bebop bebop;
    bebopInit(&bebop);
    bool bossStarted = FALSE;
    u8 tntState   = MH_DORMANT;            // el tirador usa los mismos estados
    u8 tntHitMask = 0;                     // un golpe de explosion por jugador
    Sprite* lightBubble = NULL;            // globo de la caida por la alcantarilla

    // Revelado: PAL0 el fondo, PAL1 la de las tortugas (la dejo cargada
    // initPlayer), PAL2 la de los foot soldiers.
    u16 target[64];
    for (u16 i = 0; i < 16; i++) {
        target[i]      = lvl21_pal.data[i];
        target[16 + i] = leo_player.palette->data[i];
        target[32 + i] = foot_soldier.palette->data[i];
        // (24/09) PAL3 es la del JEFE. En este nivel no hay foot soldier
        // blanco ni naranja, asi que la linea estaba en negro y Bebop se la
        // queda entera. Se carga desde el fade y no al aparecer el jefe: son
        // 16 colores que no molestan a nadie mientras no haya nada dibujado
        // con PAL3.
        // (26/09) Hasta que aparece Bebop, PAL3 es de la TV de la vidriera
        // (tv_april); bebopSpawn carga la del jefe al entrar.
        target[48 + i] = tv_april.palette->data[i];
    }
    // Colocar los sprites en pantalla ANTES del fundido: updatePlayer solo
    // sabe de cameraX, asi que el desplazamiento vertical de la camara hay que
    // aplicarlo a mano (lo mismo que se hace despues en cada frame).
    for (u8 k = 0; k < nPl; k++)
        if (pls[k]->sprite)
            SPR_setPosition(pls[k]->sprite, pls[k]->x - cameraX,
                            pls[k]->y - PLAYER_FOOT_OFFSET - playerDrawZ(pls[k]) - cameraY);
    SPR_update();

    // Musica del nivel. setLoopNumber(-1) SIEMPRE antes del play (el driver
    // latchea el numero de loops en el play): la escena anterior puede haber
    // dejado el driver en "una sola pasada".
    XGM2_setLoopNumber(-1);
    playMusicVol(music_stage2_1, VOL_MUSIC_LEVEL2_1);

    static const u16 black[64] = { 0 };
    PAL_setColors(0, black, 64, DMA);
    PAL_fadeInAll(target, 20, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    // --- Bucle principal ---------------------------------------------------
    bool running = TRUE;
    SceneId jump = PAUSE_NO_JUMP;   // (26/09) nivel elegido en el menu de pausa
    pauseReset();
    while (running) {
        // Pausa (START del control 1). Primero de todo en el frame.
        jump = pausePoll(pls, nPl);
        if (jump != PAUSE_NO_JUMP) break;

        // Deriva de la entrada cayendo: solo mientras siguen en el aire.
        if (dropTicks > 0) {
            dropTicks--;
            for (u8 k = 0; k < nPl; k++)
                if (isPlayerJumping(pls[k]))
                    pls[k]->x = (s16)(pls[k]->x + LVL21_DROP_DRIFT);
        }
        for (u8 k = 0; k < nPl; k++) {
            // Dentro del pozo la secuencia corre sola y NO se lee input.
            if (playerInManhole(pls[k])) { playerManholeStep(pls[k]); continue; }

            s16 prevX = pls[k]->x, prevY = pls[k]->y;
            updatePlayer(pls[k]);
            playerStepStreet(pls[k], prevX, prevY, &plOnPlat[k], &plWasAir[k]);

            // ¿Piso una boca de tormenta ya destapada? (saltando no cuenta:
            // playerManholeFall se apoya en playerCanBeHit, que descarta
            // STATE_JUMPING).
            if (plOnPlat[k]) continue;              // arriba de la cornisa no hay pozos
            s16 fx = pls[k]->x + FOOT_DX, fy = pls[k]->y;
            for (u16 m = 0; m < LVL21_MANHOLES; m++) {
                if (mhState[m] != MH_DONE) continue;            // todavia tapada
                if (absS16((s16)(fx - manholes[m].holeX)) > LVL21_HOLE_HALF_W) continue;
                if (absS16((s16)(fy - manholes[m].holeY)) > LVL21_HOLE_HALF_H) continue;
                playerManholeFall(pls[k],
                                  (s16)(manholes[m].holeX - FOOT_DX),
                                  (s16)(manholes[m].holeY + LVL21_HOLE_OUT_DY));
                break;
            }
        }

        // --- Camara ------------------------------------------------------
        // Lidera el que va mas adelante (el que mas avanzo en la "L", o sea
        // el de mayor x + y).
        // (26/09) Los que estan sin vidas no lideran: su cuerpo no arrastra
        // la camara (si no queda ninguno en juego, manda el P1).
        s16 leadX = p1.x, leadY = p1.y;
        {
            bool any = FALSE;
            for (u8 k = 0; k < nPl; k++) {
                if (isPlayerGameOver(pls[k])) continue;
                if (!any || (pls[k]->x + pls[k]->y) > (leadX + leadY)) {
                    leadX = pls[k]->x; leadY = pls[k]->y;
                }
                any = TRUE;
            }
        }

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
                                pls[k]->y - PLAYER_FOOT_OFFSET - playerDrawZ(pls[k]) - cameraY);

        // --- Globo "who put the light out" (20/09) -------------------------
        // Se muestra mientras ALGUNA tortuga esta en el fondo del pozo (fase 2
        // de la secuencia, el frame vacio). Uno solo aunque caigan las dos: son
        // 48 tiles de VRAM y el voice over es uno. Se ancla a la tortuga que
        // cayo, que durante toda la secuencia no se mueve del agujero.
        {
            s8 talking = -1;
            for (u8 k = 0; k < nPl; k++)
                if (playerManholeSpeaking(pls[k])) { talking = (s8)k; break; }

            if (talking < 0) {
                if (lightBubble) { SPR_releaseSprite(lightBubble); lightBubble = NULL; }
            } else {
                if (!lightBubble) {
                    lightBubble = SPR_addSprite(&light_out_bubble, 0, 0,
                                                TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
                    // El voice over arranca con el globo, una sola vez.
                    XGM2_playPCMEx(who_put_light_vo, sizeof(who_put_light_vo),
                                   SOUND_PCM_CH2, 15, FALSE, FALSE);
                }
                if (lightBubble) {
                    s16 bx = (s16)(pls[talking]->x + FOOT_DX - LVL21_BUBBLE_W / 2 - cameraX);
                    if (bx < LVL21_BUBBLE_MARGIN) bx = LVL21_BUBBLE_MARGIN;
                    if (bx > (s16)(SCREEN_PIXEL_WIDTH - LVL21_BUBBLE_W - LVL21_BUBBLE_MARGIN))
                        bx = (s16)(SCREEN_PIXEL_WIDTH - LVL21_BUBBLE_W - LVL21_BUBBLE_MARGIN);
                    s16 by = (s16)(pls[talking]->y + LVL21_BUBBLE_DY - cameraY);
                    if (by < LVL21_BUBBLE_MARGIN) by = LVL21_BUBBLE_MARGIN;
                    SPR_setPosition(lightBubble, bx, by);
                    SPR_setDepth(lightBubble, SPR_MIN_DEPTH);   // por encima de todo
                }
            }
        }

        // --- Foot soldiers -------------------------------------------------
        u16 alive = 0;
        for (u16 i = 0; i < MAX_ENEMIES; i++)
            if (enemies[i].state != ENEMY_STATE_INACTIVE) alive++;
        tvYield(alive, maxAlive);   // la tele no le quita lugar a un spawn

        // --- Guion de enemigos (19/09) -------------------------------------
        // El lider en X es el que dispara los eventos: se mide sobre los PIES,
        // que es la coordenada con la que estan marcadas las posiciones en el
        // arte (leadX es el borde del frame de 104px).
        s16 leadFeetX = leadX + FOOT_DX;
        for (u8 k = 0; k < nPl; k++) {
            s16 f = pls[k]->x + FOOT_DX;
            if (f > leadFeetX) leadFeetX = f;
        }

        // Bocas de tormenta: pasar a PENDING al cruzar el gatillo y entrar en
        // cuanto haya lugar en el pool.
        // (24/09) Con la pelea del jefe en curso el guion de la calle queda
        // CONGELADO: ni bocas de tormenta ni dinamita. No es solo estetico --
        // el jefe se lleva 77 tiles de sprites y un soldier de 80 mas no entra
        // en el presupuesto del nivel.
        for (u16 m = 0; m < LVL21_MANHOLES && !bossStarted; m++) {
            if (mhState[m] == MH_DONE) continue;
            if (mhState[m] == MH_DORMANT) {
                if (leadFeetX < manholes[m].trigX) continue;
                mhState[m] = MH_PENDING;
                mhReleaseLid(m);           // la tapa pasa a ser la del sprite
            }
            if (alive >= maxAlive) continue;          // se reintenta el frame que viene
            for (u16 i = 0; i < MAX_ENEMIES; i++) {
                if (enemies[i].state != ENEMY_STATE_INACTIVE) continue;
                initEnemyManholeSpawn(&enemies[i],
                                      (s16)(manholes[m].holeX + LVL21_MANHOLE_DX),
                                      (s16)(manholes[m].holeY + LVL21_MANHOLE_DY
                                            + ENEMY_MANHOLE_HOP_DROP),
                                      manholes[m].dir, PAL2);
                // La calle no es el pasillo del nivel 1: franja de profundidad
                // completa, sin pared diagonal, y el ancho real.
                setEnemyBounds(&enemies[i], LVL21_WALK_Y_MIN, LVL21_WALK_Y_MAX,
                               0, 0, LVL21_MAP_W * 8);
                eOnPlat[i] = FALSE;
                eClimb[i]  = 0;
                mhState[m] = MH_DONE;
                alive++;
                break;
            }
        }

        // El tirador de dinamita: mismo mecanismo, un solo evento.
        if (tntState != MH_DONE && !bossStarted) {
            if (tntState == MH_DORMANT && leadFeetX >= LVL21_TNT_TRIG_X)
                tntState = MH_PENDING;
            if (tntState == MH_PENDING && alive < maxAlive) {
                for (u16 i = 0; i < MAX_ENEMIES; i++) {
                    if (enemies[i].state != ENEMY_STATE_INACTIVE) continue;
                    initEnemyTntSpawn(&enemies[i], LVL21_TNT_X, LVL21_TNT_Y,
                                      LVL21_TNT_DIR, PAL2,
                                      LVL21_TNT_LAND_X, LVL21_TNT_LAND_Y);
                    // Alto del vano del portal: visual, igual que el escalon
                    // del 1-1. enemy.c lo baja solo despues del lanzamiento.
                    enemies[i].jumpZ = LVL21_TNT_Z;
                    setEnemyBounds(&enemies[i], LVL21_WALK_Y_MIN, LVL21_WALK_Y_MAX,
                                   0, 0, LVL21_MAP_W * 8);
                    eOnPlat[i] = FALSE;
                    eClimb[i]  = 0;
                    tntState = MH_DONE;
                    alive++;
                    break;
                }
            }
        }

        // Tapas cerradas de las bocas que todavia no salieron.
        mhUpdateLids(cameraX, cameraY);
        tvUpdate(pls, nPl, cameraX, cameraY, alive, maxAlive, !bossStarted);

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

        // --- Proyectiles del guion (19/09) ---------------------------------
        // Dinamita: mismo bloque que el 1-1, pero con tntUpdateEx porque aca la
        // camara tambien baja. Devuelve TRUE el frame exacto del impacto.
        if (tntUpdateEx(cameraX, cameraY))
            XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                           SOUND_PCM_CH3, 15, FALSE, FALSE);
        // Un jugador se come UN solo golpe por explosion: el estallido dura 24
        // frames quieto, asi que sin la mascara le sacaria una barra por frame.
        if (tntBlastActive()) {
            for (u8 k = 0; k < nPl; k++) {
                if (tntHitMask & (u8)(1 << k)) continue;
                if (!playerCanBeHit(pls[k])) continue;
                if (!tntBlastHits((s16)(getPlayerWorldX(pls[k]) + FOOT_DX), getPlayerY(pls[k]),
                                  PLAYER_BODY_HALF_W)) continue;
                playerHitBars(pls[k], tntBlastX(), LVL21_TNT_DMG_BARS);
                tntHitMask |= (u8)(1 << k);
            }
        } else {
            tntHitMask = 0;
        }

        // (24/09) Con la pelea del jefe en curso el guion de la calle queda
        // congelado: nada de bocas de tormenta ni dinamita.
        // Tapas de alcantarilla: vuelan solas y se liberan al salir de camara.
        // No se consumen al pegar (siguen de largo, como en el arcade): de que
        // un mismo jugador no se coma dos golpes seguidos se encarga la
        // invencibilidad de playerCanBeHit().
        lidUpdate(cameraX, cameraY);

        // (23/09) Golpe cuerpo a cuerpo a una tapa en vuelo: cambia de sentido
        // en X y pasa a ser de las tortugas. Va ANTES del chequeo contra el
        // jugador para que la tapa que se acaba de devolver no le pegue en el
        // mismo frame. La patada voladora no la devuelve (lo resuelve enemy.c).
        for (u8 k = 0; k < nPl; k++) {
            if (lidReflectByPlayerAttack(pls[k], (s8)k))
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                               SOUND_PCM_CH2, 15, FALSE, FALSE);
        }

        // Tapa devuelta contra los soldiers: LID_ENEMY_DMG de vida por soldier,
        // una vez por tapa. La tapa NO se consume: sigue de largo y puede
        // barrer a mas de uno, igual que cuando venia de frente.
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (!enemyCanBeHit(&enemies[i])) continue;
            s8 owner = -1;
            if (!lidHitsEnemy(i, getEnemyCenterX(&enemies[i]),
                              getEnemyCenterY(&enemies[i]),
                              enemyBodyHalfW(&enemies[i]), &owner)) continue;
            damageEnemy(&enemies[i], LID_ENEMY_DMG);
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                           SOUND_PCM_CH2, 15, FALSE, FALSE);
            if (enemies[i].state == ENEMY_STATE_DEAD) {
                XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                               SOUND_PCM_CH3, 15, FALSE, FALSE);
                if (owner >= 0 && owner < (s8)nPl) addPlayerScore(pls[owner], 1);
            }
        }

        for (u8 k = 0; k < nPl; k++) {
            if (!playerCanBeHit(pls[k])) continue;
            s16 lx = 0;
            // (25/09) lidHits espera el CENTRO del jugador; se le pasaba el
            // borde izquierdo del frame (getPlayerWorldX), 52 px corrido.
            if (!lidHits((s16)(getPlayerWorldX(pls[k]) + FOOT_DX), getPlayerY(pls[k]),
                         PLAYER_BODY_HALF_W, &lx)) continue;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                           SOUND_PCM_CH2, 15, FALSE, FALSE);
            damagePlayer(pls[k], lx);     // una barra, como pidio Gustavo
        }

        // --- Parquimetros (25/09) -------------------------------------------
        // Un golpe de tortuga los arranca y salen volando en la direccion del
        // golpe; volando MATAN a los foot soldiers que tocan (una vez por
        // soldier). Parados, sus bases las frena clampToWalk.
        if (metersPlayerHits(pls, nPl))
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                           SOUND_PCM_CH2, 15, FALSE, FALSE);
        metersUpdate(cameraX, cameraY);
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (!enemyCanBeHit(&enemies[i])) continue;
            s8 owner = -1;
            if (!metersHitEnemy(i, getEnemyCenterX(&enemies[i]),
                                getEnemyCenterY(&enemies[i]),
                                enemyBodyHalfW(&enemies[i]), &owner)) continue;
            damageEnemy(&enemies[i], (s16)(enemies[i].hp > 0 ? enemies[i].hp : ENEMY_HP));
            XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                           SOUND_PCM_CH3, 15, FALSE, FALSE);
            if (owner >= 0 && owner < (s8)nPl) addPlayerScore(pls[owner], 1);
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
                maxAlive     = LVL21_MAX_ALIVE_2P;   // la segunda tortuga come
                                                     // 169 tiles de sprites
                resetEnemyAI(2);
            }
        }

        for (u8 k = 0; k < nPl; k++) hudPlayerUpdate(&huds[k]);

        // Continues: si TODOS quedaron fuera, game over. Con el jefe ya
        // cayendo no se evalua (un KO simultaneo no le gana a la victoria).
        if (bebop.state != BEBOP_DEAD && bebop.state != BEBOP_GONE &&
            continueStepAll(conts, pls, huds, nPl, fps)) {
            allOut = TRUE;
            break;
        }

        // --- JEFE: Bebop (24/09) -------------------------------------------
        // Llegar al borde derecho con la camara en su tope ya NO termina el
        // nivel: arranca la pelea. La camara se queda donde esta (ya estaba
        // clavada en su tope) y el nivel recien cierra cuando el jefe cae.
        if (!bossStarted && cameraX >= LVL21_CAM_X_MAX &&
            leadX + FOOT_DX >= LVL21_END_X) {
            bossStarted = TRUE;
            // Los soldiers que hayan quedado vivos se van: el jefe es el
            // sprite mas grande de la escena (77 tiles) y con dos tortugas el
            // presupuesto no da para los dos. Se sueltan ANTES de crearlo.
            for (u16 i = 0; i < MAX_ENEMIES; i++) {
                if (enemies[i].state == ENEMY_STATE_INACTIVE) continue;
                if (enemies[i].sprite) SPR_releaseSprite(enemies[i].sprite);
                enemies[i].sprite = NULL;
                enemies[i].state  = ENEMY_STATE_INACTIVE;
            }
            lidReleaseAll();
            tntInit();
            XGM2_playPCMEx(boss_scream_bebop_vo, sizeof(boss_scream_bebop_vo),
                           SOUND_PCM_CH2, 15, FALSE, FALSE);
            playMusicVol(music_boss, VOL_MUSIC_BOSS_2_1);
            bebopSpawn(&bebop);
        }

        if (bossStarted) {
            bebopUpdate(&bebop, pls, nPl, cameraX, cameraY);

            // Golpe de la tortuga al jefe. Misma geometria que contra los
            // soldiers (solape de cajas), asi que por la espalda tambien pega.
            if (bebopCanBeHit(&bebop)) {
                s16 bcx = bebopGetCenterX(&bebop);
                s16 by  = bebopGetCenterY(&bebop);
                for (u8 k = 0; k < nPl; k++) {
                    if (!playerAttackHitsBox(pls[k], bcx, by,
                                             BEBOP_BODY_HALF_W, BEBOP_BODY_H))
                        continue;
                    s16 dmg = isPlayerSpecialAttack(pls[k]) ? BEBOP_SPECIAL_DMG : 1;
                    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                                   SOUND_PCM_CH2, 15, FALSE, FALSE);
                    if (bebopDamage(&bebop, dmg)) addPlayerScore(pls[k], 5);
                    break;
                }
            }

            if (bebopIsGone(&bebop)) running = FALSE;
        }

        // (26/09) El viejo atajo START + A (cortar el nivel) se fue: ahora START
        // abre la pausa, y desde ahi se salta a cualquier nivel.

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
    // (26/09) Si se salio por el menu de pausa, o todos quedaron fuera de
    // juego, no hay jingle: se va directo.
    if (jump == PAUSE_NO_JUMP && !allOut) {
        XGM2_stop();
        XGM2_setLoopNumber(0);
        playMusicVol(music_scene_clear, VOL_MUSIC_SCENE_CLEAR);

        u16 hold = (IS_PAL_SYSTEM ? 50 : 60) * LVL21_CLEAR_SECS;
        while (hold > 0) {
            hold--;
            SYS_doVBlankProcess();
        }
        XGM2_stop();
        XGM2_setLoopNumber(-1);   // el jingle lo dejo en 0: restaurar para lo que siga
    }

    // Soltar lo que el guion pudiera haber dejado vivo: los proyectiles y las
    // tapas cerradas son estado de MODULO (static), no del stack, asi que si no
    // se liberan aca la proxima partida arranca con sprites fantasma.
    tntReleaseAll();
    lidReleaseAll();
    mhReleaseAll();
    metersReleaseAll();
    tvRelease();
    bebopRelease(&bebop);      // el jefe y sus aros (tambien si el nivel se
                               // corta por game over en plena pelea)
    if (lightBubble) { SPR_releaseSprite(lightBubble); lightBubble = NULL; }

    // (26/09) Ganado (Bebop cayo): las vidas y el puntaje siguen en la
    // Scene 3. Por game over (todos fuera) se va a GAME OVER como siempre.
    bool won = (jump == PAUSE_NO_JUMP && !allOut);
    if (won)
        for (u8 k = 0; k < nPl; k++) playerPersistSave(pls[k]);

    clearScene();
    DMA_setMaxQueueSizeToDefault();
    if (jump != PAUSE_NO_JUMP) return jump;
    return won ? SCENE_3_1_TITLE : SCENE_GAME_OVER;
}
