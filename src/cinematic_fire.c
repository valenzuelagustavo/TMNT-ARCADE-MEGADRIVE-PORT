// ===========================================================================
// cinematic_fire.c - Cinematica del rescate de April (SCENE_CINEMATIC_FIRE)
// ===========================================================================
// Entra DESPUES de la seleccion de personaje y ANTES del titulo del nivel 1.
// Reconstruida a partir del analisis frame a frame del clip original
// (res/images/roof_april_scene/ANALISIS_INTRO_SGDK.md): 323 frames a 30 fps
// reales, tres escenas separadas por dos cortes duros a negro con wipe.
//
//   A    1..121   Calle: Splinter y las 4 tortugas ven el incendio, gritan y
//                 saltan a la fachada del edificio de April
//   -  122..133   Corte a negro + wipe diagonal desde la esquina sup. izq.
//   B  134..226   Tunel de techos: pasada de las 4 tortugas acercandose
//   -  227..239   Corte a negro + wipe
//   C  240..380   Azotea de April: llegan de a una y entran por el vano
//
// CADENCIA: el material fuente esta a 30 fps REALES (no doblado como la intro
// del titulo), asi que la cinematica corre a 30 fps efectivos: cada "frame"
// del analisis = DOS VBlanks NTSC (ver roofTick). Gracias a eso todas las
// constantes de tiempo de abajo son, literalmente, los numeros de frame del
// documento de analisis.
//
// "ZOOM" SIN ESCALADO: la MegaDrive no escala sprites. El acercamiento de la
// escena B se resuelve como en el arcade, por SUSTITUCION de sprite. Son tres
// escalones de la MISMA pose redibujada:
//   roof_tiny  48x64  ( 42 tiles)  en el punto de fuga
//   roof_far   72x88  ( 99 tiles)  a media distancia
//   roof_near 104x120 (195 tiles)  encima de la camara
// El generador centra las tres celdas por la caja de su contenido, asi que
// basta con dibujarlas en el mismo centro de pantalla para que el swap no
// pegue un salto.
//
// LA ESCENA C NO USA ESE ARTE: usa las hojas del JUEGO (chars.res, 104x104,
// las 21 animaciones de PlayerAnim). Es la escena que empalma con el gameplay,
// asi que las tortugas tienen que verse exactamente como en el nivel. Caen
// entrando en diagonal desde el humo de la esquina superior izquierda con el
// ULTIMO frame de ANIM_JUMP, y al tocar el piso arrancan la caminata normal
// (ANIM_WALK_FRONT, de perfil) derecho hacia la derecha hasta cruzar el vano.
// La trayectoria sale de una captura marcada a mano. Orden de
// llegada: Leo, Don, Raph, Mike. Raph demora un poco mas y Mike le cae
// encima: Mike queda tirado (ultimo frame de ANIM_HIT_BEHIND_2), se levanta
// con ANIM_GET_UP_2 y recien ahi camina.
//
// WIPE DE ENTRADA: el analisis lo describe como un iris que crece desde la
// esquina superior izquierda. No hace falta ni el plano Window ni una mascara
// de sprites: los tiles del fondo ya estan en VRAM, asi que "revelar" es
// simplemente ir volcando al tilemap el sub-rectangulo que ya entro. Lo que
// todavia no se dibujo son tiles 0 = color de backdrop = negro.
//
// ANCHO DE PANTALLA: los fondos de A y B son de 256 px (modo H32) y el de C
// de 320 (H40). El cambio de ancho se hace durante el negro de la transicion
// 2, asi que es invisible.
//
// PRESUPUESTO DE VRAM (plano 64x32 -> userTileMaxIndex ~1612):
//   fondo mas caro (escena A) 754 tiles + 152 del bloque de humo animado
//   (SMOKE_A_*, streameado justo despues del fondo, ver smokeAInit) = 906.
//   pico de sprites por escena (ver ROOF_SPR_TILES_A/B/C mas abajo):
//     A: 4 tortugas chicas (56 c/u) + Splinter (48) + globo (32) = 304
//     B: roof_tiny (42) + roof_far (99) + roof_near (195)       = 336
//     C: 1 near + 2 chicas (las llegadas van escalonadas)       = 393
//   FIX tiles corruptos en escena A: antes se reservaba UN solo bloque de
//   704 tiles (para cubrir el peor caso, escena C) durante TODA la
//   cinematica, aunque la A solo necesita 304. Eso dejaba solo ~154 tiles
//   de aire entre el fondo (754) y el bloque de sprites (704) contra el
//   techo de ~1612 -- justo donde aparecian los tiles corruptos (la franja
//   inferior y el borde diagonal de la vereda son, por orden de barrido,
//   de los tiles UNICOS de indice mas alto del fondo, los primeros en
//   pisar la zona de sprites si el margen real es mas chico de lo asumido).
//   Ahora cada escena reserva SOLO lo que necesita (con margen), asi la A
//   le devuelve al fondo el aire que la B/C no le hacen falta a ella.
//
// Sin musica (decision del usuario), igual que la intro arcade.
// START saltea la cinematica completa en cualquier momento.
// ===========================================================================

#include "scenes.h"
#include "roof_april.h"   // roof_bg_a/b/c, roof_climb, roof_climb_far, roof_tiny,
                          // roof_far, roof_near, roof_splinter, roof_baloon
#include "player.h"       // ANIM_*, PLAYER_SPRITE_W/PLAYER_FOOT_OFFSET y, via
                          // chars.h, las hojas del juego (leo_player, ...)
#include "audio.h"        // fire_vo, hang_on_april_vo (voice over de los globos)

// ---------------------------------------------------------------------------
// Geometria de pantalla y planos
// ---------------------------------------------------------------------------
#define ROOF_PLANE_W        64   // Plano circular 64x32, igual que el resto
#define ROOF_PLANE_H        32
#define ROOF_SCREEN_H      224
#define ROOF_SCREEN_ROWS    28   // 224 / 8
// Presupuesto de sprites POR ESCENA (antes: un solo ROOF_SPR_TILES=704
// para las tres). Cada roofScene*() vuelve a llamar SPR_initEx() al
// entrar (SPR_reset() ya invalida todo en roofCutToBlack), asi que
// pedir menos durante la A no le saca nada a la B/C.
#define ROOF_SPR_TILES_A     352   // pico real 304 (ver presupuesto) + margen
#define ROOF_SPR_TILES_B     384   // pico real 336 + margen
#define ROOF_SPR_TILES_C     448   // pico real 393 + margen

// Humo animado de la Escena A sobre el incendio: bloque de FONDO (no sprite),
// mismo truco que fire_strip.png/smoke_lvl1.png (scenes.c). 8x19 tiles fijos
// en x=184,y=0 (region de la llama en roof_bg_a.png). Con SPRITE los 3 frames
// quedarian los 3 residentes en VRAM a la vez (362 tiles); como bloque de
// fondo streameado solo 1 frame esta resiedente (152 tiles), reusando PAL0.
#define SMOKE_A_TILES_X       23   // 184px / 8
#define SMOKE_A_TILES_Y        0
#define SMOKE_A_TILES_W         8   // 64px / 8
#define SMOKE_A_TILES_H        19   // 152px / 8
#define SMOKE_A_TILES        (SMOKE_A_TILES_W * SMOKE_A_TILES_H)   // 152
#define SMOKE_A_FRAMES          3
#define SMOKE_A_FRAME_INTERVAL  6   // ticks de cinematica (30fps efectivos)

// Celdas de las hojas de sprites (en pixeles)
#define CLIMB_W             56
#define CLIMB_H             64
#define CLIMB_FAR_W         40
#define CLIMB_FAR_H         48
#define TINY_W              48
#define TINY_H              64
#define FAR_W               72
#define FAR_H               88
#define NEAR_W             104
#define NEAR_H             120
#define SPL_W               48
#define SPL_H               64
#define BAL_W               64
#define BAL_H               32

// ---------------------------------------------------------------------------
// Linea de tiempo (numeros de frame del analisis; 1 frame = 2 VBlanks NTSC)
// ---------------------------------------------------------------------------
#define T_FADE_IN            20   // Fundido de entrada de la escena A
#define T_A_TOTAL           121   // Duracion de la escena A

#define T_BAL_FIRE_IN        10   // "Fire!!" entra
#define T_BAL_FIRE_OUT       35   // ...y sale
#define T_BAL_HANG_IN        58   // "Hang on, April" entra
#define T_BAL_HANG_OUT       86   // ...y sale

// (18/09) La musica entra APENAS TERMINA el voice over "Hang on, April", que
// es lo que pidio Gustavo. Medido sobre el WAV de origen
// (res/audio/hang_on_april.wav, mono 11025 Hz, 9030 muestras): dura 0,819 s =
// 24,6 frames de cinematica, y arranca en T_BAL_HANG_IN. O sea que la voz se
// apaga en el frame 82,6; el 84 deja un respiro de dos frames y todavia cae
// con el globo en pantalla (sale dos frames despues, en T_BAL_HANG_OUT).
#define T_MUSIC_IN  (T_BAL_HANG_IN + 26)   // 84

// Mismo nivel que el resto de la musica del juego (VOL_MUSIC_* en scenes.c).
#define CINEMATIC_MUSIC_VOL  90
#define T_CLIMB_START        60   // El grupo toma impulso y salta
#define T_CLIMB_TIME         30   // Duracion del salto: UN solo arco. Antes 40:
                                  // acortado para que caigan un poco mas rapido
                                  // (mismo alto de arco, trayecto mas corto).
#define T_CLIMB_END        (T_CLIMB_START + T_CLIMB_TIME)   // 90
#define SPL_FRAME_IDLE        0   // Splinter arranca en su 1ra pose (reposo)
#define SPL_FRAME_ALERT       1   // ...y pasa a la 2da (baston en alto) apenas
                                  // aparece el globo "Fire!!", quedandose ahi
                                  // el resto de la escena (no respira).

#define T_BLACK              5    // Negro pleno de cada transicion
#define T_WIPE               8    // Crecimiento del wipe

#define T_PASS_TINY           8   // Escena B: tramo con el sprite mas chico
#define T_PASS_FAR            7   // ...con el mediano
#define T_PASS_NEAR           8   // ...y con el grande, hasta salir de cuadro
#define T_PASS  (T_PASS_TINY + T_PASS_FAR + T_PASS_NEAR)   // 23 por tortuga

#define T_C_FALL             16   // Escena C: duracion de la caida (la
                                  // diagonal es mas larga que la vertical
                                  // que habia antes)
#define T_C_DOWN             12   // Mike tirado en el piso
#define T_C_GETUP            10   // ...levantandose (2 frames x 5)
#define T_C_WALK             30   // Caminata desde el punto de caida al vano
#define T_C_HOLD             12   // Respiro con la azotea vacia antes de cortar

#define T_FADE_OUT           30   // Fundido final (en frames de 30 fps)
#define T_BLACK_HOLD         12

// ---------------------------------------------------------------------------
// ESCENA A - calle y fachada. Coordenadas medidas sobre roof_bg_a.png (256x224)
// ---------------------------------------------------------------------------
// La vereda gris ocupa todo el angulo inferior izquierdo; el edificio en
// llamas esta a la derecha y la columna de humo sube por x ~200..240.
#define SPL_X               12   // Esquina sup. izq. del sprite de Splinter
#define SPL_Y              150   // (queda parado sobre la vereda)
#define BAL_X               62   // Globo de dialogo, arriba y a la izquierda
#define BAL_Y               96   // del grupo (para que el salto no lo tape)

// Recorrido del grupo: 5 puntos = 4 saltos en diagonal hacia arriba-derecha,
// de saliente en saliente, hasta perderse en el humo. Son coordenadas del
// CENTRO en X y de los PIES en Y.
// UN SOLO ARCO, de la vereda a lo alto de la fachada. Antes eran 4 saltos
// encadenados y se leia como un rebote: ahora es un unico salto largo, con la
// parabola valiendo 0 en las dos puntas y CLIMB_APEX en el medio. Son
// coordenadas del CENTRO en X y de los PIES en Y.
#define CLIMB_FROM_X       124
#define CLIMB_FROM_Y       206
#define CLIMB_TO_X         208
#define CLIMB_TO_Y         104
#define CLIMB_APEX          70   // Cuanto sube el arco por encima de la recta
// A mitad del arco pasan de roof_climb a roof_climb_far (la misma pose a 2/3):
// ya estan sobre la fachada, lejos de camara, y con un solo tamano quedaban de
// cinco pisos de alto. El cambio cae en el VERTICE del salto, que es donde
// menos se nota. 100 = nunca cambia, 0 = siempre la chica.
#define CLIMB_SWAP_PCT      50

// Las 4 tortugas van en bloque pero LIGERAMENTE SEPARADAS (antes se fundian en
// una sola mancha). Leo (0) adelante, tapando en parte a los otros tres.
// El juego de offsets chico es el mismo escalado a 2/3.
// Separacion ampliada (~1.3x los valores originales) a pedido de Gustavo: se
// veian demasiado pegoteadas paradas al principio, antes de saltar.
static const s16 climbOffX[4]    = {   0, -28,  26, -53 };
static const s16 climbOffY[4]    = {   0,   6,  11,   4 };
static const s16 climbFarOffX[4] = {   0, -19,  17, -35 };
static const s16 climbFarOffY[4] = {   0,   4,   7,   3 };

// ---------------------------------------------------------------------------
// ESCENA B - tunel de techos. Coordenadas sobre roof_bg_b.png (256x224)
// ---------------------------------------------------------------------------
// El punto de fuga del "tunel" esta arriba al centro; el fondo NO se mueve:
// toda la sensacion de acercamiento la da el sprite (chico -> grande) y la
// aceleracion del recorrido (cuadratica, como en una perspectiva real).
// Los tres tramos son continuos: cada uno arranca donde termina el anterior,
// un poco mas abajo (el sprite crece, asi que su centro tiene que bajar para
// que los pies sigan la misma linea de fuga).
#define PASS_VP_X          128   // Punto de fuga
#define PASS_VP_Y           96
#define PASS_TINY_END_X    124
#define PASS_TINY_END_Y    130
#define PASS_FAR_X         122   // Arranque del tramo mediano
#define PASS_FAR_Y         138
#define PASS_FAR_END_X     112
#define PASS_FAR_END_Y     178
#define PASS_NEAR_X        104   // Arranque del tramo grande
#define PASS_NEAR_Y        192
#define PASS_END_X         -30   // Sale de cuadro por abajo-izquierda (el punto
#define PASS_END_Y         380   // final queda fuera para que no se corte a la
                                 // vista al empezar la pasada siguiente)

// Para que las 4 pasadas no sean calcadas, cada una se corre un poco de lado.
static const s16 passDrift[4] = { 0, 20, -16, 10 };

// ---------------------------------------------------------------------------
// ESCENA C - azotea de April. Coordenadas sobre roof_bg_c.png (320x224)
// ---------------------------------------------------------------------------
// El piso de la azotea ocupa el centro; el vano oscuro por donde entran esta a
// la derecha, contra el muro (su umbral cae en ~(238,156)). Las tortugas
// entran volando desde el humo de arriba-izquierda, aterrizan en el medio del
// techo y caminan derecho a la derecha hasta cruzarlo.
//
// Aca se usan las hojas del JUEGO, no el arte ripeado de la cinematica. El
// anclaje es por los PIES, igual que en el nivel: el sprite se dibuja en
// (x - PLAYER_SPRITE_W/2, y - PLAYER_FOOT_OFFSET).
// Trayectoria marcada por Gustavo sobre una captura (medida contra el fondo,
// 1:1 con los 320x224): las tortugas entran en diagonal desde el humo de la
// esquina superior izquierda y aterrizan en el medio de la azotea; de ahi
// caminan derecho a la derecha, hasta el vano.
//
// Los puntos que marco caen en (7,45) (39,61) (67,82) (92,99) (116,118)
// (137,135) y (154,161). El arranque de abajo esta sobre esa misma recta pero
// corrido hacia afuera de pantalla, para que el sprite ENTRE en cuadro en vez
// de aparecer de golpe ya medio visible.
#define C_FALL_FROM_X      -20   // Pies, fuera de cuadro por arriba-izquierda
#define C_FALL_FROM_Y       24
#define C_LAND_X           154   // Punto de caida, COMUN a las cuatro
#define C_LAND_Y           161   // ...linea de piso
#define C_DOOR_X           238   // Umbral del vano
#define C_DOOR_Y           156
// Caminan hacia la DERECHA y el arte de ANIM_WALK_FRONT ya mira para alla:
// no hace falta espejar. Poner TRUE si se invierte el sentido.
#define C_WALK_FLIP      FALSE

// Fuego animado de la Escena C: rotacion de paleta, AISLADA a la columna de
// tiles donde vive la llama (no a todo PAL0 -- ver por que abajo).
//
// roof_bg_c.png reusa los indices 5-8 para MAS de una cosa: verificado con
// PIL pixel a pixel, el indice 7 (salmon) es 100% pared/cornisa (0 pixeles
// en la llama) y el indice 8 (rojo oscuro) es sobre todo pared/sombra
// tambien (765 de 1642 pixeles fuera de la zona de la llama, esparcidos
// hasta el fondo del skyline). Rotar los 4 de punta a punta de PAL0 (primer
// intento) prendia fuego a la fachada entera de April, no solo a la llama
// -- el "resultado complicado" que reporto Gustavo.
//
// Los que SI son casi exclusivos de la llama son 5 (naranja) y 6 (amarillo):
// idx 5 no aparece ni un pixel mas alla de x=63; idx 6 tiene un puñado de
// puntitos sueltos en las ventanas del skyline (x>100) que quedan FUERA de
// la columna que remapeamos. Por eso el ciclo usa SOLO esos dos (un
// parpadeo naranja<->amarillo, tecnica estandar de "fire cycling" de 2
// colores) y ademas los aisla en una COPIA de PAL0 (ver fireCIsolate):
//   - PAL2 arranca como copia byte a byte de PAL0 (mismo puntero de origen
//     en roofBuildPal), asi que en cualquier tile que apunte a PAL2 en vez
//     de PAL0 los indices 0,1,2,3,4,7,8..15 se ven IDENTICOS a como los
//     pinto el artista.
//   - fireCIsolate() re-estampa SOLO la columna 0..FIRE_C_TILES_W-1 del
//     tilemap (donde esta la llama, x=0..63px, confirmado visualmente con
//     un overlay de los indices reales sobre el PNG) para que apunte a
//     PAL2 en vez de PAL0. El resto del fondo (pared, cornisa, skyline,
//     vano) sigue en PAL0, quieto.
//   - fireCUpdate() rota SOLO roofPal[FIRE_C_FIRST..+FIRE_C_COUNT) (los
//     indices 5-6 DENTRO de la linea PAL2) cada FIRE_C_CYCLE_INTERVAL
//     ticks. PAL0 nunca se toca -- nada fuera de la columna de la llama
//     puede parpadear, sin importar que reuse los mismos indices.
#define FIRE_C_TILES_X          0   // Columna de tiles de la llama (x=0px)
#define FIRE_C_TILES_Y          0
#define FIRE_C_TILES_W          8   // 64px / 8 -- SOLO la columna de la llama
#define FIRE_C_TILES_H         28   // 224px / 8 -- alto completo de pantalla
#define FIRE_C_PAL_LINE         2   // PAL2: copia aislada de PAL0 (ver arriba)
#define FIRE_C_FIRST      (FIRE_C_PAL_LINE * 16 + 5)   // CRAM 37: indice 5 de PAL2
#define FIRE_C_COUNT             2   // Indices 5 y 6 (naranja <-> amarillo)
#define FIRE_C_CYCLE_INTERVAL    4   // Ticks de cinematica entre cada paso

// Frames concretos de las hojas del juego (verificado: las cuatro sheets
// tienen las 21 filas alineadas y la misma cantidad de frames en estas).
#define C_JUMP_LAND_FRAME    9   // Ultimo frame de ANIM_JUMP  (10 frames)
#define C_DOWN_FRAME        11   // Ultimo de ANIM_HIT_BEHIND_2 (12) = tirada
#define C_WALK_ANIM  ANIM_WALK_FRONT   // Caminata normal, de perfil
#define C_WALK_FRAMES        8   // Frames de C_WALK_ANIM
#define C_GETUP_FRAMES       2   // Frames de ANIM_GET_UP_2
// Las hojas vienen con `time 5` (12 fps a 60 Hz). Como la cinematica corre a
// 30, la animacion se maneja a mano para que camine al mismo ritmo que en el
// juego en vez de a la mitad.
#define C_WALK_ANIM_PERIOD   3   // Frames de cinematica por frame de caminata
#define C_GETUP_ANIM_PERIOD  5

// Orden de llegada pedido: Leo, Don, Raph, Mike. Los indices son los del juego
// (0=Leo 1=Mike 2=Don 3=Raph, igual que personajeSeleccionado).
#define C_ARRIVALS           4
static const u8  arriveChar[C_ARRIVALS]  = {  0,  2,  3,  1 };
// Raph (3ra) demora un poco mas, y Mike arranca 4 frames despues que ella:
// como la caida dura lo mismo para todas, le cae justo encima al aterrizar.
static const s16 arriveStart[C_ARRIVALS] = {  6, 26, 58, 62 };
// Solo Mike queda tirado; Raph amortigua el golpe y sigue caminando.
static const u8  arriveDown[C_ARRIVALS]  = {  0,  0,  0,  1 };

// ===========================================================================
// Estado interno
// ===========================================================================
static Sprite *climbSpr[4];
static Sprite *climbFarSpr[4];
static Sprite *splinterSpr;
static Sprite *baloonSpr;
static Sprite *passTinySpr;
static Sprite *passFarSpr;
static Sprite *passNearSpr;
static Sprite *roofSpr[C_ARRIVALS];   // Escena C: uno por LLEGADA (no por
                                      // personaje). Se suelta apenas cruza el
                                      // vano, asi nunca hay mas de dos hojas
                                      // del juego (169 tiles c/u) en VRAM.

// Hojas del juego indexadas como personajeSeleccionado: 0=Leo 1=Mike 2=Don 3=Raph
static const SpriteDefinition* const charSheet[4] = {
    &leo_player, &mike_player, &don_player, &raph_player
};

static u16 roofPal[64];         // CRAM completa de la escena en curso
static const u16 roofBlack[64] = { 0 };

static u16 smokeAVramInd;   // Primer tile VRAM del bloque de humo (Escena A)
static u16 smokeAFrame;     // Frame de animacion actual (0..SMOKE_A_FRAMES-1)
static u16 smokeATimer;     // Cuenta hasta el proximo paso

static u16 fireCTimer;      // Escena C: cuenta hasta el proximo paso de rotacion

// ---------------------------------------------------------------------------
// Un "frame" de la cinematica = DOS VBlanks (30 fps efectivos sobre NTSC).
// Devuelve TRUE si algun jugador apreto START.
// ---------------------------------------------------------------------------
static bool roofSkipPressed(void) {
    return ((JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2)) & BUTTON_START)
           ? TRUE : FALSE;
}

static bool roofTick(void) {
    SPR_update();
    SYS_doVBlankProcess();
    SYS_doVBlankProcess();
    return roofSkipPressed();
}

static bool roofWait(u16 frames) {
    while (frames--) if (roofTick()) return TRUE;
    return FALSE;
}

// ---------------------------------------------------------------------------
// Armado de la CRAM: una linea por bloque logico. Pasar NULL deja la linea en
// negro (lo que hace que una escena no "herede" colores de la anterior).
// ---------------------------------------------------------------------------
static void roofBuildPal(const u16 *p0, const u16 *p1,
                         const u16 *p2, const u16 *p3) {
    const u16 *src[4] = { p0, p1, p2, p3 };
    for (u16 line = 0; line < 4; line++)
        for (u16 i = 0; i < 16; i++)
            roofPal[line * 16 + i] = src[line] ? src[line][i] : 0;
}

// ---------------------------------------------------------------------------
// Interpolacion: lineal y cuadratica (acelera) sobre 0..T
// ---------------------------------------------------------------------------
static s16 lerpS(s16 a, s16 b, s16 t, s16 T) {
    return a + (s16)(((s32)(b - a) * t) / T);
}

static s16 accelS(s16 a, s16 b, s16 t, s16 T) {
    return a + (s16)(((s32)(b - a) * t * t) / ((s32)T * T));
}

// ---------------------------------------------------------------------------
// Posicionar un sprite por su CENTRO (las hojas far/near vienen centradas por
// la caja del contenido, asi que el centro es el ancla comun de las dos).
// ---------------------------------------------------------------------------
static void sprCenter(Sprite *s, s16 cx, s16 cy, s16 w, s16 h) {
    SPR_setPosition(s, cx - (w >> 1), cy - (h >> 1));
}

// ===========================================================================
// FONDO: carga y wipe de entrada
// ===========================================================================
// Los tiles se cargan enteros de una (el fondo no se streamea: entra completo
// en VRAM) y el tilemap se vuelca por partes para el wipe.
// ---------------------------------------------------------------------------
static u16 roofBgLoad(const Image *img) {
    VDP_loadTileSet(img->tileset, TILE_USER_INDEX, DMA);
    return TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX);
}

static void roofBgDrawRect(const Image *img, u16 attr,
                           u16 x, u16 y, u16 w, u16 h) {
    if (!w || !h) return;
    VDP_setTileMapEx(BG_B, img->tilemap, attr, x, y, x, y, w, h, DMA);
}

// Wipe tipo iris que crece desde la esquina superior izquierda. Solo se dibuja
// lo NUEVO de cada paso (las columnas que se sumaron a lo alto de lo ya
// revelado, mas las filas que se sumaron a lo ancho de lo anterior), asi el
// DMA por frame queda chico.
static bool roofBgWipe(const Image *img, u16 attr) {
    const u16 cols = img->tilemap->w;
    const u16 rows = img->tilemap->h;
    u16 prevW = 0, prevH = 0;

    for (u16 t = 1; t <= T_WIPE; t++) {
        u16 w = (cols * t) / T_WIPE;
        u16 h = (rows * t) / T_WIPE;
        if (w > prevW) roofBgDrawRect(img, attr, prevW, 0, w - prevW, h);
        if (h > prevH) roofBgDrawRect(img, attr, 0, prevH, prevW, h - prevH);
        prevW = w; prevH = h;
        if (roofTick()) return FALSE;
    }
    roofBgDrawRect(img, attr, 0, 0, cols, rows);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Humo animado de la Escena A (ver defines SMOKE_A_* mas arriba). Bloque de
// FONDO fijo, streameado igual que el fuego/humo de scenes.c: un solo frame
// (152 tiles) vive en VRAM, el tilemap lo referencia una vez, y cada
// SMOKE_A_FRAME_INTERVAL ticks se pisa con el frame siguiente por DMA. No
// hace falta tocar la paleta: roof_bg_a_smoke comparte PAL0 (paleta forzada
// en tools/gen_roof_smoke.py para caer en las mismas entradas de CRAM que
// roof_bg_a).
// ---------------------------------------------------------------------------
static void smokeAInit(u16 vramInd) {
    smokeAVramInd = vramInd;
    smokeAFrame = 0;
    smokeATimer = 0;

    VDP_loadTileData(roof_bg_a_smoke.tiles, vramInd, SMOKE_A_TILES, DMA);
    VDP_fillTileMapRectInc(BG_B,
                            TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, vramInd),
                            SMOKE_A_TILES_X, SMOKE_A_TILES_Y,
                            SMOKE_A_TILES_W, SMOKE_A_TILES_H);
}

static void smokeAUpdate(void) {
    if (++smokeATimer < SMOKE_A_FRAME_INTERVAL) return;
    smokeATimer = 0;

    smokeAFrame++;
    if (smokeAFrame >= SMOKE_A_FRAMES) smokeAFrame = 0;

    VDP_loadTileData(roof_bg_a_smoke.tiles + ((u32)smokeAFrame * SMOKE_A_TILES * 8),
                      smokeAVramInd, SMOKE_A_TILES, DMA_QUEUE);
}

// ---------------------------------------------------------------------------
// Re-estampa SOLO la columna de tiles de la llama (FIRE_C_TILES_*) para que
// apunte a PAL2 en vez de PAL0. Se llama UNA vez, despues de que
// roofBgWipe ya dibujo el fondo entero en PAL0 -- esto no cambia que
// grafico muestra cada tile (mismos indices de tile, mismo `img->tilemap`
// como fuente), solo el bit de paleta de esa columna. El resto del fondo
// (pared, cornisa, skyline, vano) queda en PAL0 sin tocar.
// ---------------------------------------------------------------------------
static void fireCIsolate(const Image *img) {
    VDP_setTileMapEx(BG_B, img->tilemap,
                      TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, TILE_USER_INDEX),
                      FIRE_C_TILES_X, FIRE_C_TILES_Y,
                      FIRE_C_TILES_X, FIRE_C_TILES_Y,
                      FIRE_C_TILES_W, FIRE_C_TILES_H, DMA);
}

// ---------------------------------------------------------------------------
// Fuego animado de la Escena C (ver defines FIRE_C_* mas arriba). Rotacion de
// paleta pura sobre roofPal[FIRE_C_FIRST..+FIRE_C_COUNT), que vive en PAL2
// (la copia aislada de PAL0, ver fireCIsolate). Con FIRE_C_COUNT=2 esto es
// un simple ida-y-vuelta naranja<->amarillo (rotar un array de 2 elementos
// es intercambiarlos). Se pisa con PAL_setColors, NUNCA con roofBuildPal
// (que reconstruye las 4 lineas enteras desde los .res y nos haria perder
// la rotacion acumulada).
// ---------------------------------------------------------------------------
static void fireCUpdate(void) {
    if (++fireCTimer < FIRE_C_CYCLE_INTERVAL) return;
    fireCTimer = 0;

    u16 first = roofPal[FIRE_C_FIRST];
    for (u16 i = 0; i < FIRE_C_COUNT - 1; i++)
        roofPal[FIRE_C_FIRST + i] = roofPal[FIRE_C_FIRST + i + 1];
    roofPal[FIRE_C_FIRST + FIRE_C_COUNT - 1] = first;

    PAL_setColors(FIRE_C_FIRST, &roofPal[FIRE_C_FIRST], FIRE_C_COUNT, DMA);
}

// Corte duro a negro: apaga la CRAM, limpia planos y suelta los sprites que
// queden de la escena anterior. Criterio de retorno unico en todo el modulo:
// TRUE = seguir, FALSE = el jugador apreto START.
static bool roofCutToBlack(void) {
    PAL_setColors(0, roofBlack, 64, DMA);
    SYS_doVBlankProcess();
    // SPR_reset() invalida TODOS los Sprite* de la escena anterior: hay que
    // olvidarlos aca o quedan punteros colgados.
    SPR_reset();
    SPR_update();
    splinterSpr = baloonSpr = NULL;
    passTinySpr = passFarSpr = passNearSpr = NULL;
    for (u16 i = 0; i < 4; i++) { climbSpr[i] = NULL; climbFarSpr[i] = NULL; }
    for (u16 i = 0; i < C_ARRIVALS; i++) roofSpr[i] = NULL;
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    SYS_doVBlankProcess();
    return !roofWait(T_BLACK);
}

// ===========================================================================
// ESCENA A - Calle y fachada del edificio de April en llamas (frames 1..121)
// ===========================================================================
// Plano fijo, sin scroll: todo lo que se mueve son sprites. Splinter queda
// congelado en su 2da pose; los globos aparecen y desaparecen segun la tabla del
// analisis; el grupo de 4 tortugas pega UN salto largo hacia arriba-derecha.
// ---------------------------------------------------------------------------
// Dibuja el grupo con la hoja grande o la chica segun `big`. La otra queda
// oculta: los 8 sprites viven toda la escena A (344 tiles de sprite en total),
// asi no hay altas y bajas de VRAM en el medio del salto.
static void climbSetPose(s16 cx, s16 cy, u16 frame, bool big) {
    for (u16 i = 0; i < 4; i++) {
        if (big) {
            SPR_setPosition(climbSpr[i], cx + climbOffX[i] - CLIMB_W / 2,
                                         cy + climbOffY[i] - CLIMB_H);
            SPR_setAnimAndFrame(climbSpr[i], i, frame);
        } else {
            SPR_setPosition(climbFarSpr[i], cx + climbFarOffX[i] - CLIMB_FAR_W / 2,
                                            cy + climbFarOffY[i] - CLIMB_FAR_H);
            SPR_setAnimAndFrame(climbFarSpr[i], i, frame);
        }
        SPR_setVisibility(climbSpr[i],    big ? VISIBLE : HIDDEN);
        SPR_setVisibility(climbFarSpr[i], big ? HIDDEN  : VISIBLE);
    }
}

static void climbHideAll(void) {
    for (u16 i = 0; i < 4; i++) {
        SPR_setVisibility(climbSpr[i],    HIDDEN);
        SPR_setVisibility(climbFarSpr[i], HIDDEN);
    }
}

static bool roofSceneA(void) {
    SPR_initEx(ROOF_SPR_TILES_A);
    u16 attr = roofBgLoad(&roof_bg_a);
    VDP_setTileMapEx(BG_B, roof_bg_a.tilemap, attr, 0, 0, 0, 0,
                     roof_bg_a.tilemap->w, roof_bg_a.tilemap->h, DMA);
    // El bloque de humo va JUSTO despues del ultimo tile del fondo: no le
    // pisa nada al fondo (que ya declaro sus 754 propios) ni al presupuesto
    // de sprites (que vive en el otro extremo de la VRAM, ver SPR_initEx).
    smokeAInit(TILE_USER_INDEX + roof_bg_a.tileset->numTile);

    // PAL0 fondo | PAL1 las 4 tortugas (paleta unificada) | PAL2 Splinter |
    // PAL3 globos. Se arma la CRAM entera y se funde desde negro.
    roofBuildPal(roof_bg_a.palette->data, roof_climb.palette->data,
                 roof_splinter.palette->data, roof_baloon.palette->data);

    splinterSpr = SPR_addSprite(&roof_splinter, SPL_X, SPL_Y,
                                TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (!splinterSpr) return FALSE;
    // Arranca en la pose de reposo (frame 0); pasa a la de alerta cuando
    // aparece el globo "Fire!!" (ver el bucle de abajo) y se queda ahi.
    SPR_setAnimAndFrame(splinterSpr, 0, SPL_FRAME_IDLE);
    SPR_setDepth(splinterSpr, -1);   // Splinter, en la vereda, delante de todos

    baloonSpr = SPR_addSprite(&roof_baloon, BAL_X, BAL_Y,
                              TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    if (!baloonSpr) return FALSE;
    SPR_setVisibility(baloonSpr, HIDDEN);

    // El grupo arranca quieto en el primer punto del recorrido, apoyado.
    // Leo (0) al frente: SPR_setDepth mas chico = mas adelante.
    for (u16 i = 0; i < 4; i++) {
        climbSpr[i]    = SPR_addSprite(&roof_climb, 0, 0,
                                       TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
        climbFarSpr[i] = SPR_addSprite(&roof_climb_far, 0, 0,
                                       TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
        if (!climbSpr[i] || !climbFarSpr[i]) return FALSE;
        SPR_setDepth(climbSpr[i],    (s16)i);
        SPR_setDepth(climbFarSpr[i], (s16)i);
    }
    climbSetPose(CLIMB_FROM_X, CLIMB_FROM_Y, 0, TRUE);

    SPR_update();
    PAL_setColors(0, roofBlack, 64, DMA);
    PAL_fadeIn(0, 63, roofPal, T_FADE_IN * 2, TRUE);

    for (u16 f = 0; f < T_A_TOTAL; f++) {
        // --- Globos de dialogo + pose de Splinter ---
        if (f == T_BAL_FIRE_IN) {
            SPR_setAnimAndFrame(baloonSpr, 0, 0);       // "Fire!!"
            SPR_setVisibility(baloonSpr, VISIBLE);
            // Voice over "Fire!!" en sincro con el globo. CH2 (no CH4: el
            // driver XGM2 de SGDK solo implementa 3 canales PCM reales,
            // PCM0..PCM2 = SOUND_PCM_CH1..CH3 -- SOUND_PCM_CH4 esta declarado
            // en sound.h pero el Z80 no lo atiende, y usarlo corrompe el
            // comando del driver -- bug encontrado 29/08, ver DEVLOG).
            XGM2_playPCMEx(fire_vo, sizeof(fire_vo), SOUND_PCM_CH2, 15, FALSE, FALSE);
            // Splinter pasa a la 2da pose (baston en alto) justo cuando
            // aparece este globo, y se queda ahi el resto de la escena.
            SPR_setAnimAndFrame(splinterSpr, 0, SPL_FRAME_ALERT);
        } else if (f == T_BAL_FIRE_OUT) {
            SPR_setVisibility(baloonSpr, HIDDEN);
        } else if (f == T_BAL_HANG_IN) {
            SPR_setAnimAndFrame(baloonSpr, 0, 1);       // "Hang on, April"
            SPR_setVisibility(baloonSpr, VISIBLE);
            // Voice over "Hang on, April" en sincro con el globo (CH2, ver nota arriba).
            XGM2_playPCMEx(hang_on_april_vo, sizeof(hang_on_april_vo), SOUND_PCM_CH2, 15, FALSE, FALSE);
        } else if (f == T_MUSIC_IN) {
            // Tema de la intro arcade ("01 - Opening Demo"), que hasta ahora
            // solo sonaba en SCENE_INTRO_ARCADE. Entra sin pisar la voz: el
            // VO ya termino (ver T_MUSIC_IN).
            //
            // setLoopNumber va SIEMPRE ANTES del play -- el driver latchea el
            // numero de loops en el instante del play (misma trampa que
            // music_credits y music_scene_clear).
            //
            // CON LOOP. Medido sobre la grabacion del emulador: desde aca
            // hasta que arranca el nivel pasan 18,4 s y el VGM dura 15,08 s,
            // asi que sin loop quedaban 3,1 s de silencio en la cola del
            // titulo. El tema trae punto de loop (loopSamples = 470400 =
            // 10,67 s), asi que repite solo y no se nota.
            XGM2_setLoopNumber(-1);
            playMusicVol(music_intro_arcade, CINEMATIC_MUSIC_VOL);
        } else if (f == T_BAL_HANG_OUT) {
            SPR_setVisibility(baloonSpr, HIDDEN);
        }

        // --- El grupo salta: UN solo arco hasta lo alto de la fachada ---
        if (f >= T_CLIMB_START && f < T_CLIMB_END) {
            s16 lt = (s16)(f - T_CLIMB_START);
            s16 x = lerpS(CLIMB_FROM_X, CLIMB_TO_X, lt, T_CLIMB_TIME);
            s16 y = lerpS(CLIMB_FROM_Y, CLIMB_TO_Y, lt, T_CLIMB_TIME);
            // Parabola: 0 en las dos puntas, maxima en el medio
            y -= (s16)(((s32)CLIMB_APEX * 4 * lt * (T_CLIMB_TIME - lt))
                       / ((s32)T_CLIMB_TIME * T_CLIMB_TIME));
            // Frame 0 = apoyado (impulso, solo el primer frame), 1 = en el aire
            u16 pose = (lt == 0) ? 0 : 1;
            bool big = (bool)((lt * 100) < (T_CLIMB_TIME * CLIMB_SWAP_PCT));
            climbSetPose(x, y, pose, big);
        } else if (f == T_CLIMB_END) {
            // Llegaron arriba, ya adentro del humo: se los deja de ver.
            climbHideAll();
        }

        smokeAUpdate();

        if (roofTick()) return FALSE;
    }
    return TRUE;
}

// ===========================================================================
// ESCENA B - Tunel de techos: pasada de las 4 tortugas (frames 134..226)
// ===========================================================================
// Fondo totalmente fijo. Cada tortuga aparece chica en el punto de fuga, se
// acerca acelerando y a mitad de camino se cambia el sprite chico por el
// grande (el "zoom" del arcade) hasta salir de cuadro por abajo-izquierda.
// Los dos sprites se crean UNA vez para toda la escena y se reusan en las 4
// pasadas cambiandoles la fila de animacion: asi no hay altas y bajas de VRAM
// en el medio de la escena.
// ---------------------------------------------------------------------------
static bool roofSceneB(void) {
    SPR_initEx(ROOF_SPR_TILES_B);
    u16 attr = roofBgLoad(&roof_bg_b);
    roofBuildPal(roof_bg_b.palette->data, roof_far.palette->data, NULL, NULL);
    PAL_setColors(0, roofPal, 64, DMA);

    // Los tres tamanos se crean UNA vez para toda la escena y se reusan en las
    // 4 pasadas cambiandoles la fila de animacion: asi no hay altas y bajas de
    // VRAM de sprite en el medio de la escena.
    passTinySpr = SPR_addSprite(&roof_tiny, -TINY_W, -TINY_H,
                                TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
    passFarSpr  = SPR_addSprite(&roof_far,  -FAR_W,  -FAR_H,
                                TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
    passNearSpr = SPR_addSprite(&roof_near, -NEAR_W, -NEAR_H,
                                TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
    if (!passTinySpr || !passFarSpr || !passNearSpr) return FALSE;
    SPR_setVisibility(passTinySpr, HIDDEN);
    SPR_setVisibility(passFarSpr,  HIDDEN);
    SPR_setVisibility(passNearSpr, HIDDEN);
    SPR_update();

    if (!roofBgWipe(&roof_bg_b, attr)) return FALSE;

    for (u16 i = 0; i < 4; i++) {
        s16 drift = passDrift[i];
        SPR_setAnimAndFrame(passTinySpr, i, 0);
        SPR_setAnimAndFrame(passFarSpr,  i, 0);
        SPR_setAnimAndFrame(passNearSpr, i, 0);

        for (u16 t = 0; t < T_PASS; t++) {
            // Un solo sprite visible por vez: el swap ES el "zoom".
            Sprite *cur;
            s16 cx, cy, w, h;

            if (t < T_PASS_TINY) {
                // --- Punto de fuga: sale del fondo del tunel ---
                s16 lt = (s16)t;
                cur = passTinySpr; w = TINY_W; h = TINY_H;
                cx = accelS(PASS_VP_X, PASS_TINY_END_X + drift, lt, T_PASS_TINY);
                cy = accelS(PASS_VP_Y, PASS_TINY_END_Y,         lt, T_PASS_TINY);
            } else if (t < T_PASS_TINY + T_PASS_FAR) {
                // --- Media distancia ---
                s16 lt = (s16)(t - T_PASS_TINY);
                cur = passFarSpr; w = FAR_W; h = FAR_H;
                cx = accelS(PASS_FAR_X + drift, PASS_FAR_END_X + drift, lt, T_PASS_FAR);
                cy = accelS(PASS_FAR_Y,         PASS_FAR_END_Y,         lt, T_PASS_FAR);
            } else {
                // --- Encima de la camara: pasa por delante y sale de cuadro ---
                s16 lt = (s16)(t - T_PASS_TINY - T_PASS_FAR);
                cur = passNearSpr; w = NEAR_W; h = NEAR_H;
                cx = accelS(PASS_NEAR_X + drift, PASS_END_X + drift, lt, T_PASS_NEAR);
                cy = accelS(PASS_NEAR_Y,         PASS_END_Y,         lt, T_PASS_NEAR);
            }

            SPR_setVisibility(passTinySpr, (cur == passTinySpr) ? VISIBLE : HIDDEN);
            SPR_setVisibility(passFarSpr,  (cur == passFarSpr)  ? VISIBLE : HIDDEN);
            SPR_setVisibility(passNearSpr, (cur == passNearSpr) ? VISIBLE : HIDDEN);
            sprCenter(cur, cx, cy, w, h);

            if (roofTick()) return FALSE;
        }
    }

    SPR_setVisibility(passTinySpr, HIDDEN);
    SPR_setVisibility(passFarSpr,  HIDDEN);
    SPR_setVisibility(passNearSpr, HIDDEN);
    return TRUE;
}

// ===========================================================================
// ESCENA C - Azotea de April, de cerca
// ===========================================================================
// Aca ya no se usa el arte ripeado de la cinematica sino las hojas del JUEGO
// (chars.res): esta escena empalma con el gameplay y las tortugas tienen que
// verse como en el nivel.
//
// Cada llegada es siempre la misma secuencia:
//   cae (ultimo frame de ANIM_JUMP) -> [Mike: tirada + ANIM_GET_UP_2] ->
//   ANIM_WALK_FRONT hasta el vano -> se suelta el sprite al cruzarlo
//
// Las cuatro caen en el MISMO punto, en el medio de la azotea, pero
// escalonadas: cuando una toca el piso la anterior ya va por dos tercios del
// camino, asi que las caminatas no se pisan. La unica excepcion es a proposito: Raph demora, y Mike aterriza 4
// frames despues que ella, todavia encima suyo.
//
// Como mucho hay DOS tortugas vivas a la vez (338 tiles de sprite).
// ---------------------------------------------------------------------------
// Anclaje por los pies + profundidad por altura: la que esta mas abajo en
// pantalla tapa a las de atras (y a Mike, que cae ultimo y mas abajo, lo deja
// justo por delante de Raph).
static void roofPlaceFeet(Sprite *s, s16 fx, s16 fy) {
    SPR_setPosition(s, fx - PLAYER_SPRITE_W / 2, fy - PLAYER_FOOT_OFFSET);
    SPR_setDepth(s, (s16)(ROOF_SCREEN_H - fy));
}

static bool roofSceneC(void) {
    SPR_initEx(ROOF_SPR_TILES_C);
    u16 attr = roofBgLoad(&roof_bg_c);
    // PAL1 = paleta de las tortugas del juego (las 4 hojas comparten una sola,
    // por eso alcanza con la de Leo, igual que en el nivel). PAL2 arranca
    // como COPIA de PAL0 (mismo puntero de origen): la usa fireCIsolate
    // para la columna de la llama, ver defines FIRE_C_* mas arriba.
    roofBuildPal(roof_bg_c.palette->data, leo_player.palette->data,
                 roof_bg_c.palette->data, NULL);
    PAL_setColors(0, roofPal, 64, DMA);

    for (u16 i = 0; i < C_ARRIVALS; i++) roofSpr[i] = NULL;
    SPR_update();
    fireCTimer = 0;

    if (!roofBgWipe(&roof_bg_c, attr)) return FALSE;
    fireCIsolate(&roof_bg_c);

    // La escena dura lo que tarde la ultima en cruzar el vano, mas un respiro.
    u16 total = 0;
    for (u16 i = 0; i < C_ARRIVALS; i++) {
        u16 end = (u16)arriveStart[i] + T_C_FALL + T_C_WALK
                + (arriveDown[i] ? (T_C_DOWN + T_C_GETUP) : 0);
        if (end > total) total = end;
    }
    total += T_C_HOLD;

    for (u16 f = 0; f < total; f++) {
        for (u16 i = 0; i < C_ARRIVALS; i++) {
            s16 e = (s16)f - arriveStart[i];
            if (e < 0) continue;

            // Limites de los tramos de ESTA llegada (los dos del medio solo
            // existen para la que queda tirada).
            const s16 endFall  = T_C_FALL;
            const s16 endDown  = endFall + (arriveDown[i] ? T_C_DOWN  : 0);
            const s16 endGetUp = endDown + (arriveDown[i] ? T_C_GETUP : 0);
            const s16 endWalk  = endGetUp + T_C_WALK;

            if (e >= endWalk) {                       // ya cruzo el vano
                if (roofSpr[i]) {
                    SPR_releaseSprite(roofSpr[i]);
                    roofSpr[i] = NULL;
                }
                continue;
            }
            if (!roofSpr[i]) {
                roofSpr[i] = SPR_addSprite(charSheet[arriveChar[i]], 0, 0,
                                 TILE_ATTR(PAL1, FALSE, FALSE, C_WALK_FLIP));
                if (!roofSpr[i]) continue;
                // Control MANUAL de frame durante toda la llegada (caida
                // congelada en el frame de aterrizaje, tirada/levantada de
                // Mike, caminata a mano): sin esto, el motor de sprites
                // arrancaria a reproducir la fila que se le asigne con su
                // propio `time` en vez de quedarse en el frame que pedimos.
                SPR_setAutoAnimation(roofSpr[i], FALSE);
            }

            if (e < endFall) {
                // --- Entra en diagonal desde el humo con el ULTIMO frame del
                //     salto, CONGELADO durante toda la caida (recien se pasa
                //     a caminar en el frame exacto en que toca el piso, mas
                //     abajo). En X va parejo y en Y acelera un poco: es el
                //     final de un salto, no una caida vertical. ---
                s16 fx = lerpS(C_FALL_FROM_X, C_LAND_X, e, T_C_FALL);
                s16 fy = (s16)((lerpS(C_FALL_FROM_Y, C_LAND_Y, e, T_C_FALL)
                              + accelS(C_FALL_FROM_Y, C_LAND_Y, e, T_C_FALL)) / 2);
                SPR_setAnimAndFrame(roofSpr[i], ANIM_JUMP, C_JUMP_LAND_FRAME);
                roofPlaceFeet(roofSpr[i], fx, fy);
            } else if (e < endDown) {
                // --- Mike: cayo encima de Raph y quedo tirado ---
                SPR_setAnimAndFrame(roofSpr[i], ANIM_HIT_BEHIND_2, C_DOWN_FRAME);
                roofPlaceFeet(roofSpr[i], C_LAND_X, C_LAND_Y);
            } else if (e < endGetUp) {
                // --- ...y se levanta ---
                u16 fr = (u16)((e - endDown) / C_GETUP_ANIM_PERIOD);
                if (fr >= C_GETUP_FRAMES) fr = C_GETUP_FRAMES - 1;
                SPR_setAnimAndFrame(roofSpr[i], ANIM_GET_UP_2, fr);
                roofPlaceFeet(roofSpr[i], C_LAND_X, C_LAND_Y);
            } else {
                // --- "Toca el piso": recien ACA arranca a caminar ---
                // Recta pura del punto de caida al umbral (antes tenian un
                // desvio en Y por "carril" para no pisarse entre si al
                // cruzarse; a pedido de Gustavo se saco: se veia como un
                // zigzageo, y ya alcanza con que arriveStart/arriveDown las
                // separe en el tiempo para que no se superpongan).
                s16 lt = e - endGetUp;
                s16 fx = lerpS(C_LAND_X, C_DOOR_X, lt, T_C_WALK);
                s16 fy = lerpS(C_LAND_Y, C_DOOR_Y, lt, T_C_WALK);
                u16 fr = (u16)((lt / C_WALK_ANIM_PERIOD) % C_WALK_FRAMES);
                SPR_setAnimAndFrame(roofSpr[i], C_WALK_ANIM, fr);
                roofPlaceFeet(roofSpr[i], fx, fy);
            }
        }

        fireCUpdate();

        if (roofTick()) return FALSE;
    }
    return TRUE;
}

// ===========================================================================
// PUNTO DE ENTRADA DE LA ESCENA
// ===========================================================================
SceneId showFireCinematic(void) {
    clearScene();

    // Escenas A y B a 256 px (su arte esta hecho a ese ancho); la C pasa a
    // 320 durante el negro de la segunda transicion.
    VDP_setScreenWidth256();
    VDP_setPlaneSize(ROOF_PLANE_W, ROOF_PLANE_H, TRUE);
    // El SPR_initEx real de cada escena lo hace roofSceneA/B/C() al entrar
    // (presupuesto ajustado por escena, ver ROOF_SPR_TILES_A/B/C).
    VDP_setBackgroundColor(0);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);

    splinterSpr = baloonSpr = NULL;
    passTinySpr = passFarSpr = passNearSpr = NULL;
    for (u16 i = 0; i < 4; i++) { climbSpr[i] = NULL; climbFarSpr[i] = NULL; }
    for (u16 i = 0; i < C_ARRIVALS; i++) roofSpr[i] = NULL;

    if (roofSceneA() && roofCutToBlack() && roofSceneB() && roofCutToBlack()) {
        // El cambio de ancho pasa desapercibido: la pantalla esta en negro y
        // todavia no se dibujo nada del fondo de la escena C.
        VDP_setScreenWidth320();
        roofSceneC();
    }

    // ---- Salida ----
    // Se espera a que SUELTEN START para que el mismo pulso no saltee tambien
    // el titulo del nivel.
    while (roofSkipPressed()) SYS_doVBlankProcess();

    PAL_fadeOutAll(T_FADE_OUT * 2, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();
    roofWait(T_BLACK_HOLD);

    VDP_setScreenWidth320();
    SPR_reset();
    SPR_update();
    SYS_doVBlankProcess();

    splinterSpr = baloonSpr = NULL;
    passTinySpr = passFarSpr = passNearSpr = NULL;
    for (u16 i = 0; i < 4; i++) { climbSpr[i] = NULL; climbFarSpr[i] = NULL; }
    for (u16 i = 0; i < C_ARRIVALS; i++) roofSpr[i] = NULL;

    // Se vuelve al presupuesto de sprites del juego y al estado que espera el
    // titulo del nivel (plano circular 64x32, como lo deja la seleccion de
    // personaje).
    SPR_initEx(752);
    // (18/09) keepAudio: el tema de la intro arranca a mitad de la cinematica
    // y tiene que seguir sonando durante el titulo del nivel, igual que en el
    // arcade. Lo corta showScene11Title al salir, justo antes de que el nivel
    // ponga music_level1.
    clearSceneEx(TRUE);
    return SCENE_1_1_TITLE;
}
