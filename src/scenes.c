#include "scenes.h"
#include "intro.h"   // sega_logo_spr, rocksteady_spr
#include "menus.h"   // logo, characters_greyscale, selector_turtle, character_selector, faces_hud
#include "level1.h"  // bg_level1 (IMAGE, 1376x224 — nivel completo), fire_tiles (TILESET, 8 frames de 64x64), hud_1p/hud_2p (SPRITE, 72x32, 4 anims), title_font, title_font_pal
#include "level2.h"  // bg_test (IMAGE, 440x192 — sala del nivel 2), smoke_tiles (TILESET, 8 frames de 64x64)
#include "intro_tmnt.h"  // Intro arcade: intro_sky, intro_buildings, intro_speed, intro_street (IMAGE)
#include "audio.h"   // music_sega, golpe, music_level1, select_music
#include "player.h"  // sistema del jugador (incluye chars.h internamente)
#include "enemy.h"   // sistema de enemigos (incluye enemies.h → foot_soldier)
#include "robot.h"   // robot del látigo (mini-jefe del final; robot_whip, whip_waves)
#include "rocksteady.h"  // jefe final del nivel 2 (cápsula del taladro; rocksteady_boss, boss_bullet)

// Compatibilidad entre versiones de SGDK (el macro cambió de nombre)
#ifndef IS_PAL_SYSTEM
#define IS_PAL_SYSTEM IS_PALSYSTEM
#endif

// ---------------------------------------------------------------------------
// Nivel 1 — constantes de cámara y mundo
// ---------------------------------------------------------------------------
#define LEVEL1_PIXEL_WIDTH   1376   // Ancho del fondo completo (172 tiles x 8px)
#define SCREEN_PIXEL_WIDTH   320    // Ancho visible de la MegaDrive
#define CAM_MAX_X            (LEVEL1_PIXEL_WIDTH - SCREEN_PIXEL_WIDTH)  // 1056
// Dead-zone derecha: medida sobre el borde IZQUIERDO del frame del jugador
// (el centro del personaje queda en +52). Con 120, el scroll arranca cuando
// el personaje pasa apenas la mitad de la pantalla (centro ~172 de 320).
#define CAM_DEAD_ZONE_RIGHT  120    // Player X pantalla > este valor → scroll derecha
#define CAM_DEAD_ZONE_LEFT    80    // Player X pantalla < este valor → scroll izquierda
// 2P: la cámara nunca avanza si va a dejar al jugador rezagado fuera de
// pantalla; su frame conserva como mínimo este margen desde el borde izquierdo
#define CAM_TRAIL_MARGIN       8
#define CAM_MAX_SPEED          4    // max pixels de scroll por frame (suaviza desbloqueo de cámara)

#define BG_PLANE_W           64     // Ancho del plano circular de fondo (tiles)

// ---------------------------------------------------------------------------
// Intro scriptada del nivel — globo de diálogo "Attack!!" + voice over
// ---------------------------------------------------------------------------
// APENAS arranca el nivel (sin esperar nada): aparece el globo (attack_bubble,
// 64x32), suena el voice over (attack_vo, PCM 8-bit) y ya está spawneado el
// primer foot soldier en el borde derecho, entrando hacia el jugador. El globo
// va en posición FIJA de pantalla, INDEPENDIENTE del jugador y de la cámara:
// solo corre su ciclo por tiempo (fijo → parpadeo → desaparece). Comparte la
// paleta de las tortugas (PAL1) → no gasta línea de paleta.
#define BUBBLE_SOLID_SECS    2      // Segundos fijo en pantalla (cubre el VO ~0.73s + margen)
#define BUBBLE_BLINK_TOGGLE  4      // Frames por semiciclo de parpadeo (~7-8 Hz)
#define BUBBLE_X_TILES       3      // X FIJA de pantalla: 3 tiles desde el borde izquierdo
#define BUBBLE_SCREEN_Y     74      // Y FIJA de pantalla (altura ya aprobada, indep. del jugador)

// ---------------------------------------------------------------------------
// Puertas del nivel como spawn points (huecos "ACA" del fondo)
// ---------------------------------------------------------------------------
// Centros de mundo de los 3 huecos de puerta abierta, medidos sobre
// bg01_completa.png. Sobre cada uno se dibuja door_lvl_1 (40x80, PAL0 = paleta
// del fondo). Al acercarse el jugador, la puerta se remueve y un foot soldier
// la rompe (initEnemyDoorSpawn). Cada puerta dispara UNA sola vez.
#define LEVEL1_DOOR_COUNT     3
#define DOOR_SPRITE_TOP_Y    48    // Y de pantalla del tope del sprite (el hueco va de y=50 a 128)
#define DOOR_HALF_W          20    // door_lvl_1 = 40px de ancho → mitad, para centrarlo en el hueco
#define DOOR_TRIGGER_DIST   110    // El player a < esto (|dx| centro↔centro) arma el spawn
#define DOOR_VIS_MARGIN      48    // Crea/suelta el sprite de la puerta según cercanía a la pantalla

// ---------------------------------------------------------------------------
// Sparks: efecto de fuego detrás de las puertas rompibles
// ---------------------------------------------------------------------------
// Sprite de 32x32 (2x2 tiles) en PAL2, ubicado detrás de cada door_lvl_1.
// La animación es puramente por rotación de paleta: 4 cuadros que rotan los
// índices 5-8 de PAL2 (colores de fuego del foot soldier morado).
#define SPARKS_SPRITE_TOP_Y  56    // Centrado verticalmente en la puerta (puerta va de 48 a 128)
#define SPARKS_HALF_W        16    // 32px / 2
#define SPARKS_PAL_FRAME_COUNT 4   // Cuadros de rotación de paleta
#define SPARKS_PAL_SPEED      4    // Ticks entre cada rotación (~6fps a 25fps)
#define SPARKS_PAL_IDX_START  5    // Primer índice de paleta a rotar
#define SPARKS_PAL_IDX_COUNT  4    // Cantidad de índices a rotar (5,6,7,8)

// Paleta del foot soldier morado (copia de PAL2 base, que rescomp genera).
// Los 4 cuadros rotan los índices 5-8 para simular fuego. Solo se escriben
// los 4 colores que cambian; el resto de PAL2 se deja intacto.
static const u16 sparksPalAnim[SPARKS_PAL_FRAME_COUNT][SPARKS_PAL_IDX_COUNT] = {
    { 0x008E, 0x00AE, 0x00AE, 0x06CE },   // cuadro 0
    { 0x06CE, 0x008E, 0x00AE, 0x00AE },   // cuadro 1
    { 0x00AE, 0x06CE, 0x008E, 0x00AE },   // cuadro 2
    { 0x00AE, 0x00AE, 0x06CE, 0x008E },   // cuadro 3
};

// ---------------------------------------------------------------------------
// Puertas de ASCENSOR (2 huecos anchos del fondo) — spawn animado
// ---------------------------------------------------------------------------
// Dos instancias del sprite ascensor_door (48x80, 4 frames, PAL0) sobre los
// huecos de ascensor (centros de mundo 972 y 1100). Cuando AMBAS quedan
// centradas en la cámara se abren (animación de apertura); al terminar se
// remueven y de cada hueco sale un foot soldier (BREAK_DOOR frames 3-4).
// Dispara UNA sola vez.
#define LEVEL1_ELEV_COUNT     2
#define ELEV_SPRITE_TOP_Y    48    // Y de pantalla del tope del sprite (el hueco va de y=51 a 127)
#define ELEV_HALF_W          24    // ascensor_door = 48px de ancho → mitad, para centrar en el hueco
#define ELEV_SPARK_HALF_W    20    // spark_ascensor = 40px de ancho → mitad
#define ELEV_SPARK_TOP_Y    104    //Parte inferior de la puerta (48+80-24=104), fuego asoma abajo
#define ELEV_CENTER_MIN      40    // "centradas": ambos centros con screenX ≥ esto...
#define ELEV_CENTER_MAX     280    // ...y ≤ esto (ambas puertas cómodamente dentro de la pantalla)
#define ELEV_DOOR_ANIM_TIME  32    // Duración de la animación de apertura (4 frames x 8 ticks)

// ---------------------------------------------------------------------------
// Zonas de combate: la cámara se bloquea y spawnean enemigos
// ---------------------------------------------------------------------------
// Cada zona bloquea la cámara en un cameraX fijo. La cámara se desbloquea
// cuando activeEnemies == 0. Las coordenadas se calculan a partir del
// borde derecho de la cámara (edgeX - SCREEN_PIXEL_WIDTH = cameraX).
#define ZONE1_CAM_LOCK   150    // borde derecho = 470
#define ZONE2_CAM_LOCK   300    // borde derecho = 620
#define ZONE3_CAM_LOCK   614    // borde derecho = 934
#define ZONE4_ELEV_LOCK  880    // cameraX fijo donde frena la zona de ascensores
#define ZONE5_ROBOT_LOCK 1056   // cameraX fijo donde frena la zona del robot (= CAM_MAX_X)

// ---------------------------------------------------------------------------
// Secuencia de SALIDA del nivel (tras matar al robot) — AJUSTE FINO
// ---------------------------------------------------------------------------
// Al terminar (robot muerto, sin enemigos) el jugador queda quieto un momento
// y luego camina SOLO (sin control) hacia la puerta del muro del final, y ahí
// se corta la escena para pasar a la cutscene final.
#define OUTRO_STAND_SECS      1    // Segundos quieto antes de caminar
#define OUTRO_DOOR_X       1243    // X de mundo destino (frente a la puerta del muro)
#define OUTRO_DOOR_Y        150    // Lane de pies al llegar a la puerta

// ---------------------------------------------------------------------------
// Volumen de audio (0..100) — requiere el driver XGM2 (recursos XGM2 en
// audio.res). El XGM clásico no tiene control de volumen.
// ---------------------------------------------------------------------------
#define VOL_MUSIC_INTRO    100
#define VOL_MUSIC_SELECT   100
#define VOL_MUSIC_LEVEL1    80   // la música del nivel saturaba: bajada al 50%
#define VOL_SFX            100

// ---------------------------------------------------------------------------
// Estado global de selección (necesario entre escenas)
// ---------------------------------------------------------------------------
u8 personajeSeleccionado  = 0;  // P1: 0=Leo 1=Mike 2=Don 3=Raph (columnas de pantalla)
u8 personaje2Seleccionado = 3;  // P2: 0=Leo 1=Mike 2=Don 3=Raph (columnas de pantalla)
u8 cantidadJugadores      = 1;  // 1 o 2 jugadores

// Continues disponibles en la partida (compartidos entre ambos jugadores).
// Se consumen al continuar; se reinician en la selección de personajes.
static u8 continuesLeft = 3;

// ---------------------------------------------------------------------------
// clearScene — limpieza completa entre escenas
// ---------------------------------------------------------------------------
void clearScene() {
    PAL_fadeOutAll(20, FALSE);
    while(PAL_isDoingFade()) {
        SYS_doVBlankProcess();
    }
    XGM2_stop();
    SPR_reset();
    // Vaciar YA la tabla de sprites del VDP: SPR_reset() limpia el estado
    // interno del motor (y los tiles del region de sprites) pero NO pisa la
    // SAT en VRAM hasta el proximo SPR_update. Sin este flush, los sprites de
    // la escena anterior seguian visibles durante el setup de la siguiente
    // (p.ej. el esqueleto de los creditos asomando en la seleccion de players).
    // SPR_update() con 0 sprites escribe una SAT vacia (todo oculto).
    SPR_update();
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    // Resetear el scroll de ambos planos. El nivel deja BG_B scrolleado en
    // -cameraX (y BG_A en 0); sin este reset, la escena siguiente hereda ese
    // desplazamiento y su contenido aparece corrido (p.ej. el logo TMNT del
    // menú, tras un game over que reinicia el juego).
    // Volver al modo de scroll POR PLANO: el nivel lo pone POR TILE (para el
    // fuego); sin este reset las demas escenas quedarian en modo tile y sus
    // VDP_setHorizontalScroll (que escriben un solo valor) no scrollearian bien.
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);
    VDP_setBackgroundColor(0);
    SYS_doVBlankProcess();
}

// ---------------------------------------------------------------------------
// Helper — reproducir un track XGM2 con volumen (FM + PSG en 0..100)
// ---------------------------------------------------------------------------
static void playMusicVol(const u8* track, u16 vol) {
    XGM2_setFMVolume(vol);
    XGM2_setPSGVolume(vol);
    XGM2_play(track);
}

// ---------------------------------------------------------------------------
// Helper — detección de flanco (botón recién presionado este frame)
// ---------------------------------------------------------------------------
static bool justPressedJoy(u16 joy, u16 prev, u16 button) {
    return (bool)((joy & button) && !(prev & button));
}

// Mueve la selección en 'dir' (+1/-1) sin pisar al otro jugador ni salir de
// rango [0..3]. Si la celda contigua está ocupada por el otro jugador, la
// saltea. Como hay 4 personajes y 2 jugadores, siempre queda una celda libre.
static s8 charMove(s8 self, s8 other, s8 dir) {
    s8 c = self + dir;
    if (c < 0 || c > 3) return self;
    if (c == other) { c += dir; if (c < 0 || c > 3) return self; }
    return c;
}

// ===========================================================================
// STREAMING DE FONDO — recorrido del nivel completo (más ancho que el plano)
// ===========================================================================
// El fondo (1376px) no entra en ningún plano de la MegaDrive. La técnica:
//  1. Se cargan TODOS los tiles únicos (~495) a VRAM una sola vez.
//  2. El plano BG_B es circular de 64 tiles (512px). Se dibujan columnas
//     nuevas por el borde derecho a medida que la cámara avanza, reescribiendo
//     columnas viejas que ya quedaron fuera de pantalla a la izquierda.
//  3. El scroll horizontal (-cameraX) se encarga de mostrar la ventana correcta.
// Como es beat-em-up, la cámara nunca retrocede → solo revelamos a la derecha.
// ---------------------------------------------------------------------------
// Filas de tile visibles en pantalla (224px / 8). Es el largo de las tablas
// de scroll horizontal POR TILE que alimentan BG_B (fondo) y BG_A (fuego).
#define SCROLL_TILE_ROWS  (224 / 8)   // 28

static const u16* bgMapData;   // tilemap completo en ROM (sin comprimir)
static u16        bgMapW;      // ancho del mapa en tiles (172)
static u16        bgMapH;      // alto del mapa en tiles (28)
static u16        bgBaseAttr;  // atributo base: paleta + índice base en VRAM
static s16        bgLastCol;   // última columna FUENTE ya volcada al plano
static s16        bgScrollTbl[SCROLL_TILE_ROWS];  // tabla H-scroll por tile de BG_B

// Vuelca una columna del mapa fuente (srcCol) en su posición circular del plano
static void bgDrawColumn(u16 srcCol) {
    u16 destCol = srcCol & (BG_PLANE_W - 1);
    const u16* p = bgMapData + srcCol;   // primer tile de esa columna
    for (u16 ty = 0; ty < bgMapH; ty++) {
        VDP_setTileMapXY(BG_B, bgBaseAttr + p[ty * bgMapW], destCol, ty);
    }
}

// ---------------------------------------------------------------------------
// Fade-in de nivel desde negro. Todos los helpers de setup evitan cargar
// paletas a CRAM (que queda negra tras clearScene), asi el setup completo de
// tiles/tilemaps/sprites transcurre invisible; aca se componen las 4 paletas
// en RAM y la escena se revela con un fundido (misma tecnica que el ending).
// Antes de fundir se fuerza CRAM a negro por si algun helper intermedio cargo
// paleta por DMA (p.ej. initEnemySpawn recarga PAL2 en cada spawn): sin esto,
// esa carga se veria un frame a color pleno durante el setup.
// ---------------------------------------------------------------------------
#define LEVEL_FADE_FRAMES 20
static void levelFadeIn(const u16* pal0, const u16* pal1,
                        const u16* pal2, const u16* pal3) {
    u16 target[64];
    for (u16 i = 0; i < 16; i++) {
        target[i]      = pal0[i];
        target[16 + i] = pal1[i];
        target[32 + i] = pal2[i];
        target[48 + i] = pal3[i];
    }
    static const u16 black[64] = { 0 };
    PAL_setColors(0, black, 64, DMA);
    PAL_fadeInAll(target, LEVEL_FADE_FRAMES, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();
}

// Inicializa el fondo del nivel: tileset a VRAM y primeras columnas
// (la paleta PAL0 la carga levelFadeIn al final del setup).
static void bgInit() {
    VDP_setPlaneSize(BG_PLANE_W, 32, TRUE);   // plano circular 64x32 (default seguro)

    VDP_loadTileSet(bg_level1.tileset, TILE_USER_INDEX, DMA);

    bgMapData  = bg_level1.tilemap->tilemap;
    bgMapW     = bg_level1.tilemap->w;
    bgMapH     = bg_level1.tilemap->h;
    bgBaseAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX);

    // Dibujar las primeras 64 columnas (o menos si el mapa fuera más corto)
    u16 initCols = (bgMapW < BG_PLANE_W) ? bgMapW : BG_PLANE_W;
    for (u16 c = 0; c < initCols; c++) bgDrawColumn(c);
    bgLastCol = (s16)initCols - 1;
}

// Revela las columnas necesarias para la posición de cámara y aplica el scroll
static void bgUpdate(s16 cameraX) {
    // Columna fuente que debe estar lista: borde derecho visible + 1 de margen
    s16 need = (cameraX >> 3) + (SCREEN_PIXEL_WIDTH >> 3) + 1;
    if (need > (s16)bgMapW - 1) need = (s16)bgMapW - 1;
    while (bgLastCol < need) {
        bgLastCol++;
        bgDrawColumn((u16)bgLastCol);
    }
    // BG_B scrollea a velocidad de mundo. Como el fuego obliga a poner el scroll
    // horizontal en modo POR TILE (y ese modo es GLOBAL a los dos planos), aca ya
    // no alcanza VDP_setHorizontalScroll (escribe una sola entrada): hay que
    // alimentar la tabla completa de BG_B con todas las filas al mismo -cameraX.
    for (u16 i = 0; i < SCROLL_TILE_ROWS; i++) bgScrollTbl[i] = -cameraX;
    VDP_setHorizontalScrollTile(BG_B, 0, bgScrollTbl, SCROLL_TILE_ROWS, DMA_QUEUE);
}

// ===========================================================================
// FUEGO EN PRIMER PLANO — animación por STREAMING de tiles (DMA)
// ===========================================================================
// El plan original era el truco del scroll de BG_A (dibujar la tira completa
// de 8 frames y correr el scroll de a -64px). Se DESCARTÓ por VRAM: la tira
// entera son ~400 tiles únicos que, sumados al fondo (~495) y a los sprites
// de 104x104 (2 tortugas + 4 foot soldiers ≈ 540 tiles), desbordan los ~1400
// tiles de la VRAM. Técnica definitiva:
//  1. En VRAM vive UN solo frame del fuego: una celda de 64x64px = 64 tiles.
//  2. El tilemap de BG_A referencia esos MISMOS 64 tiles repetidos a lo ancho
//     del plano (8 celdas), con PRIORIDAD ALTA → se ve delante de BG_B y de
//     todos los sprites (que van con prioridad baja). Se dibuja UNA sola vez.
//  3. Cada FIRE_FRAME_INTERVAL frames de juego se PISAN esos 64 tiles con los
//     del frame siguiente. fire_tiles es un TILESET sin comprimir NI
//     deduplicar (NONE NONE en level1.res): los 64 tiles de cada frame están
//     contiguos en ROM y se indexan directo. Son 2KB por la cola DMA cada 8
//     frames — despreciable para el presupuesto de vblank.
// Ventajas sobre el truco del scroll: entra en VRAM y todas las celdas quedan
// EN FASE. Ademas, la banda del fuego SI scrollea (parallax): BG_A pasa a modo
// de scroll horizontal POR TILE, asi las filas del fuego se desplazan con la
// camara mientras las del HUD (arriba) quedan fijas. Como la celda de 64px se
// repite en todo el plano circular (512px = 8x64), el scroll envuelve sin
// costura y el fuego parece parte del mundo (como en el arcade).
// Paleta: el fuego COMPARTE la paleta de los foot soldiers → PAL2.
// ---------------------------------------------------------------------------
#define FIRE_CELL_TILES_W    8    // Celda de fuego: 8 tiles de ancho (64px)
#define FIRE_CELL_TILES_H    8    // 8 tiles de alto (64px)
#define FIRE_CELL_TILES      (FIRE_CELL_TILES_W * FIRE_CELL_TILES_H)   // 64
#define FIRE_FRAMES          8    // Frames de animación en fire_strip.png
#define FIRE_FRAME_INTERVAL  8    // Frames de juego entre cada frame de fuego
#define FIRE_Y_TILE          ((224 / 8) - FIRE_CELL_TILES_H)  // 20: banda inferior

// Parallax del fuego: scrollea a FIRE_SCROLL_NUM/FIRE_SCROLL_DEN de la camara.
//   1/1 = anclado al mundo (igual que el fondo) | 1/2 = deriva suave ("pequeño")
// Como la celda se repite, esto solo cambia la VELOCIDAD de deriva, no la fase.
#define FIRE_SCROLL_NUM      1
#define FIRE_SCROLL_DEN      2

static u16 fireVramInd;  // Primer tile de VRAM de la celda del fuego
static u16 fireFrame;    // Frame de animación actual (0..7)
static u16 fireTimer;    // Contador hasta el próximo paso
static s16 fireScrollTbl[FIRE_CELL_TILES_H];  // H-scroll por tile de las 8 filas del fuego

// Carga el frame 0 y dibuja la celda repetida a lo ancho del plano, pegada al
// borde inferior. 'vramInd' es el primer tile libre (después del fondo).
static void fireInit(u16 vramInd) {
    fireVramInd = vramInd;
    fireFrame   = 0;
    fireTimer   = 0;

    // La paleta PAL2 (fuego + foot soldiers) la carga levelFadeIn al final
    // del setup; acá solo se preparan tiles y tilemap.
    // Frame 0 a VRAM (64 tiles)
    VDP_loadTileData(fire_tiles.tiles, vramInd, FIRE_CELL_TILES, DMA);

    // Tilemap: la celda de 8x8 tiles repetida en las 64 columnas del plano.
    // fillTileMapRectInc incrementa el índice tile a tile en el mismo orden
    // (fila por fila) en que rescomp exporta el TILESET.
    for (u16 block = 0; block < BG_PLANE_W / FIRE_CELL_TILES_W; block++) {
        VDP_fillTileMapRectInc(BG_A,
                               TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE, vramInd),
                               block * FIRE_CELL_TILES_W, FIRE_Y_TILE,
                               FIRE_CELL_TILES_W, FIRE_CELL_TILES_H);
    }

    // BG_A pasa a scroll horizontal POR TILE: la banda del fuego se desplaza
    // (fireUpdate) mientras el HUD queda clavado. El modo es GLOBAL a ambos
    // planos -> por eso bgUpdate() ahora alimenta la tabla completa de BG_B.
    VDP_setScrollingMode(HSCROLL_TILE, VSCROLL_PLANE);
    // Toda la tabla de BG_A arranca en 0 (HUD + banda vacia + fuego); las filas
    // del fuego las va pisando fireUpdate() con el offset de parallax.
    s16 zero[SCROLL_TILE_ROWS];
    for (u16 i = 0; i < SCROLL_TILE_ROWS; i++) zero[i] = 0;
    VDP_setHorizontalScrollTile(BG_A, 0, zero, SCROLL_TILE_ROWS, DMA);
}

// Avanza la animación del fuego + su scroll de parallax. Recibe la cámara y se
// llama una vez por frame en el bucle del nivel.
static void fireUpdate(s16 cameraX) {
    if (++fireTimer >= FIRE_FRAME_INTERVAL) {
        fireTimer = 0;
        fireFrame = (fireFrame + 1) & (FIRE_FRAMES - 1);
        // Pisar los MISMOS 64 tiles de VRAM con el frame siguiente. Cada tile
        // son 8 longwords → el frame N arranca en tiles + N*64*8. DMA_QUEUE:
        // la transferencia real (2KB) se hace en el próximo vblank.
        VDP_loadTileData(fire_tiles.tiles + (fireFrame * FIRE_CELL_TILES * 8),
                         fireVramInd, FIRE_CELL_TILES, DMA_QUEUE);
    }

    // Scroll de parallax de la banda: las 8 filas del fuego al mismo offset.
    s16 fscroll = (s16)(-(((s32)cameraX * FIRE_SCROLL_NUM) / FIRE_SCROLL_DEN));
    for (u16 i = 0; i < FIRE_CELL_TILES_H; i++) fireScrollTbl[i] = fscroll;
    VDP_setHorizontalScrollTile(BG_A, FIRE_Y_TILE, fireScrollTbl,
                                FIRE_CELL_TILES_H, DMA_QUEUE);
}

// ===========================================================================
// BOLA DE HIERRO — obstáculo que cae rebotando por las escaleras
// ===========================================================================
// Una esfera de metal de 32x32 (2 frames girando, paleta de las tortugas PAL1)
// aparece cada IRON_BALL_PERIOD frames en lo alto de la ESCALERA del nivel (X de
// mundo fija) y BAJA rebotando en DIAGONAL hacia el frente-derecha, cruzando las
// lanes hasta salir por abajo (como en el arcade). Si toca a
// un jugador le resta 1 barra de vida (via damagePlayer, con sus i-frames -> un
// solo golpe por pasada); si toca a un foot soldier, lo aplasta.
//
// Modelo de coordenadas (igual que enemigos/jugador):
//   x = MUNDO, centro de la bola (pantalla = x - cameraX) -> queda anclada al
//       mundo: si la cámara scrollea durante la caída, la bola scrollea con él.
//   y = línea de CONTACTO en el eje vertical (misma escala que la lane/pies);
//       baja IRON_BALL_FALL_SPEED px/frame -> el descenso por la escalera.
//   z = altura del rebote sobre el contacto (offset VISUAL, como jumpZ); rebota
//       contra un "escalón" en z=0. La profundidad para el Y-sorting es 'y'.
// La colisión se mide en profundidad (|feetY - y|) + X de mundo: la bola pega a
// lo que esté a su MISMA profundidad y solapado en X, ignorando z (el cuerpo de
// los personajes es alto y el rebote nunca lo supera).
// ---------------------------------------------------------------------------
#define IRON_BALL_SIZE        32   // px (4x4 tiles), lado del frame
#define IRON_BALL_HALF        16
#define IRON_BALL_PERIOD     180   // frames entre bolas (~6s a 60fps)
// La bola SIEMPRE baja por la escalera del nivel (X de mundo FIJA, medida sobre
// bg01_completa.png: la escalera ocupa ~508..620). Nace arriba de todo y rueda
// en diagonal hacia el frente-derecha (ROLL>0), como en el arcade.
#define IRON_BALL_STAIRS_X   535   // X de mundo del alto de la escalera (spawn)
#define IRON_BALL_START_Y     44   // Y del primer escalon (parte alta de la escalera)
#define IRON_BALL_EXIT_Y     236   // Y a la que ya salió por abajo -> se apaga
#define IRON_BALL_FALL_SPEED   2   // px/frame que desciende la línea de contacto
#define IRON_BALL_GRAVITY      1   // px/frame^2 del rebote
#define IRON_BALL_BOUNCE       10   // impulso de rebote hacia arriba (apex ~18px)
#define IRON_BALL_ROLL         1   // px/frame de deriva a la DERECHA (diagonal escalera)
#define IRON_BALL_ONSCREEN_MARGIN 40  // solo cae si el alto de la escalera esta en pantalla
#define IRON_BALL_HIT_X       26   // |dx| centro a centro (mundo) para golpear
#define IRON_BALL_HIT_Y       22   // |dy| en profundidad (pies) para golpear
#define IRON_BALL_ENEMY_DMG   ENEMY_HP   // aplasta al foot soldier de una

static struct {
    Sprite* sprite;
    s16     x;       // mundo, centro
    s16     y;       // línea de contacto (lane/pies)
    s16     z;       // altura del rebote (>= 0)
    s16     vz;      // velocidad vertical del rebote (+ = subiendo)
    bool    active;
    u16     timer;   // frames hasta el próximo spawn
} ironBall;

// Crea el sprite (oculto) UNA vez al iniciar el nivel. Usa PAL1 (tortugas), que
// ya cargó initPlayer -> llamar DESPUÉS de initPlayer.
static void ironBallInit() {
    ironBall.sprite = SPR_addSprite(&iron_ball, -IRON_BALL_SIZE, -IRON_BALL_SIZE,
                                    TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
    if (ironBall.sprite) {
        SPR_setAnim(ironBall.sprite, 0);            // 2 frames girando (auto-anim)
        SPR_setVisibility(ironBall.sprite, HIDDEN);
    }
    ironBall.active = FALSE;
    ironBall.timer  = IRON_BALL_PERIOD;
}

// TRUE si la bola (activa) golpea un objetivo con centro X 'cx' (mundo) y pies
// 'cfy'. Mide en profundidad + X; ignora la altura del rebote (z).
static bool ironBallHits(s16 cx, s16 cfy) {
    s16 dx = cx - ironBall.x;  if (dx < 0) dx = -dx;
    s16 dy = cfy - ironBall.y; if (dy < 0) dy = -dy;
    return (dx <= IRON_BALL_HIT_X && dy <= IRON_BALL_HIT_Y);
}

// Física + colisiones + render de la bola. Llamar una vez por frame en el nivel.
static void ironBallUpdate(s16 cameraX, Player* p1, Player* p2, bool twoPlayers,
                           Enemy* list, u16 count) {
    if (!ironBall.sprite) return;

    // --- Spawn periódico desde la ESCALERA (una bola a la vez) ---
    if (!ironBall.active) {
        if (ironBall.timer > 0) ironBall.timer--;
        if (ironBall.timer == 0) {
            ironBall.timer = IRON_BALL_PERIOD;   // reengancha el próximo ciclo
            // Solo cae si el alto de la escalera esta a la vista: la bola baja
            // SIEMPRE por esa escalera (X de mundo fija), no en lugares random.
            s16 stairScreenX = IRON_BALL_STAIRS_X - cameraX;
            if (stairScreenX >= IRON_BALL_ONSCREEN_MARGIN &&
                stairScreenX <= SCREEN_PIXEL_WIDTH - IRON_BALL_ONSCREEN_MARGIN) {
                ironBall.x      = IRON_BALL_STAIRS_X;
                ironBall.y      = IRON_BALL_START_Y;
                ironBall.z      = 0;
                ironBall.vz     = IRON_BALL_BOUNCE;   // arranca rebotando
                ironBall.active = TRUE;
                SPR_setVisibility(ironBall.sprite, VISIBLE);
            }
        }
        if (!ironBall.active) return;
    }

    // --- Rebote vertical (z) sobre un "escalón" en z=0 ---
    ironBall.z  += ironBall.vz;
    ironBall.vz -= IRON_BALL_GRAVITY;
    if (ironBall.z <= 0) {
        ironBall.z  = 0;
        ironBall.vz = IRON_BALL_BOUNCE;
        XGM2_playPCMEx(iron_ball_sfx, sizeof(iron_ball_sfx), SOUND_PCM_CH3, 15, FALSE, FALSE);
    }

    // --- Descenso por la escalera + deriva horizontal ---
    ironBall.y += IRON_BALL_FALL_SPEED;
    ironBall.x += IRON_BALL_ROLL;

    // --- ¿Salió por abajo? -> apagar y esperar al próximo ciclo ---
    if (ironBall.y >= IRON_BALL_EXIT_Y) {
        ironBall.active = FALSE;
        SPR_setVisibility(ironBall.sprite, HIDDEN);
        return;
    }

    // --- Colisiones ---
    // Jugador: 1 barra por pasada (los i-frames de damagePlayer evitan el
    // multi-golpe). attackerX = centro de la bola -> knockback alejándose.
    s16 p1cx = getPlayerWorldX(p1) + PLAYER_SPRITE_W / 2;
    if (playerCanBeHit(p1) && ironBallHits(p1cx, getPlayerY(p1))) {
        XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
        damagePlayer(p1, ironBall.x);
    }
    if (twoPlayers) {
        s16 p2cx = getPlayerWorldX(p2) + PLAYER_SPRITE_W / 2;
        if (playerCanBeHit(p2) && ironBallHits(p2cx, getPlayerY(p2))) {
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
            damagePlayer(p2, ironBall.x);
        }
    }
    // Foot soldiers: la bola los aplasta (sin dar puntos a nadie).
    for (u16 i = 0; i < count; i++) {
        Enemy* e = &list[i];
        if (enemyCanBeHit(e) && ironBallHits(getEnemyCenterX(e), getEnemyCenterY(e))) {
            if (damageEnemy(e, IRON_BALL_ENEMY_DMG))
                XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode), SOUND_PCM_CH3, 15, FALSE, FALSE);
        }
    }

    // --- Render: pantalla = mundo - cámara; el rebote (z) sube el dibujo ---
    SPR_setPosition(ironBall.sprite,
                    ironBall.x - cameraX - IRON_BALL_HALF,
                    ironBall.y - ironBall.z - IRON_BALL_SIZE);
    SPR_setDepth(ironBall.sprite, -(ironBall.y));   // Y-sorting por profundidad
}

// Oculta la bola al terminar el nivel (que no quede congelada en la cutscene).
static void ironBallEnd() {
    ironBall.active = FALSE;
    if (ironBall.sprite) SPR_setVisibility(ironBall.sprite, HIDDEN);
}

// ===========================================================================
// HUD — marcos de P1 y P2 en la franja superior de 32px
// ===========================================================================
// El fondo del nivel deja libres sus 4 primeras filas de tiles (32px) y los
// marcos del HUD (72x32) van ahi como SPRITES: los sprites siempre quedan por
// encima de los planos, asi el marco queda sobre la accion sin depender de la
// prioridad de BG ni de gastar VRAM de tiles de fondo. P1 pegado al borde
// izquierdo, P2 al derecho. Comparten la paleta de las tortugas (PAL1, que
// carga initPlayer) -> no consumen linea de paleta propia.
// Cada marco es un SPRITE de 4 animaciones de 1 frame (una por tortuga, en
// orden de personaje). Se elige con SPR_setAnim(sprite, personajeElegido).
// Los CONTENIDOS (vidas, puntos, barra de vida) se dibujan aparte como tiles
// de BG_A (ver seccion siguiente).
// ---------------------------------------------------------------------------
#define HUD_TILE_W         9                       // Ancho del marco en tiles (72px)
#define HUD_P1_X           40                      // P1 corrido hacia adentro (libera 40px del borde izq)
#define HUD_P2_X           (SCREEN_PIXEL_WIDTH - (HUD_TILE_W * 8) - 40)  // 208: P2 corrido hacia adentro
#define HUD_P1_BASECOL     (HUD_P1_X / 8)          // 5: columna de tile donde arranca el marco P1
#define HUD_P2_BASECOL     (HUD_P2_X / 8)          // 26: idem P2
#define PORTRAIT_P1_X      0                       // Retrato P1 en el borde izquierdo liberado
#define PORTRAIT_P2_X      (SCREEN_PIXEL_WIDTH - 32)  // 288: retrato P2 en el borde derecho liberado
#define PORTRAIT_Y         0

static Sprite* hudSprite1    = NULL;
static Sprite* hudSprite2    = NULL;
static Sprite* portraitSpr1  = NULL;
static Sprite* portraitSpr2  = NULL;

// Crea los marcos del HUD y los retratos de tortuga como sprites de alto
// nivel. En 1 jugador solo se crean los de P1. No consume VRAM de planos
// (los tiles viven en el area de sprites del motor, SPR_initEx). Los sprites
// se liberan solos en clearScene (SPR_reset) al terminar la escena.
static void hudInit(void) {
    hudSprite1 = SPR_addSprite(&hud_1p, HUD_P1_X, 0, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    if (hudSprite1)
        SPR_setAnim(hudSprite1, personajeSeleccionado);

    portraitSpr1 = SPR_addSprite(&turtle_portrait, PORTRAIT_P1_X, PORTRAIT_Y,
                                 TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    if (portraitSpr1)
        SPR_setAnim(portraitSpr1, personajeSeleccionado);

    if (cantidadJugadores == 2) {
        hudSprite2 = SPR_addSprite(&hud_2p, HUD_P2_X, 0, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
        if (hudSprite2)
            SPR_setAnim(hudSprite2, personaje2Seleccionado);

        portraitSpr2 = SPR_addSprite(&turtle_portrait, PORTRAIT_P2_X, PORTRAIT_Y,
                                     TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
        if (portraitSpr2)
            SPR_setAnim(portraitSpr2, personaje2Seleccionado);
    }
}

// ===========================================================================
// HUD — contenido dinámico: barra de vida + vidas + puntaje
// ===========================================================================
// Todo entra en el marco original (72x32), que deja 2 filas de tiles de
// interior útil. Distribución estilo arcade (compacta):
//   fila 1 -> PUNTAJE (arriba, alineado a la derecha, cols 4..7 del marco)
//   fila 2 -> [VIDAS]  [BARRA]       vidas a la IZQUIERDA, barra a la derecha
//
// La BARRA DE VIDA se dibuja como TILES en BG_A (prioridad alta), NO como
// sprite: no consume presupuesto del motor de sprites (SPR_initEx) ni depende
// del layering sprite/plano. La barra es de 32x8 (una fila de tiles): un frame
// (4x1 = 4 tiles) vive en VRAM por jugador y, al recibir un golpe, se pisa con
// el frame siguiente via DMA (misma técnica de streaming que el fuego).
// Comparte PAL1 (paleta de las tortugas): el PNG está indexado en esa misma
// paleta.
//
// VIDAS y PUNTAJE van como TEXTO con la fuente arcade del HUD (hud_font, via
// VDP_drawText) sobre BG_A. Se dibujan en PAL1 (paleta de las tortugas): la
// fuente está indexada sobre esa misma paleta (indices 11/13 -> lavanda/gris).
// ---------------------------------------------------------------------------
#define HPBAR_FRAME_TILES_W  4                                            // 32px
#define HPBAR_FRAME_TILES_H  1                                            // 8px
#define HPBAR_FRAME_TILES    (HPBAR_FRAME_TILES_W * HPBAR_FRAME_TILES_H)  // 4

// Posiciones (en tiles) RELATIVAS a la columna donde arranca el marco.
// El interior útil es cols 1..7 (col 0 y col 8 son borde del marco).
#define HUD_SCORE_ROW   1   // fila superior; el puntaje se alinea a la derecha
#define HUD_SCORE_LEFT  4   // primera col libre de la fila superior (tras "1UP")
#define HUD_SCORE_RIGHT 8   // borde derecho (col 8); el puntaje termina en col 7
#define HUD_LIVES_COL   1
#define HUD_LIVES_ROW   2
#define HUD_BAR_COL     4
#define HUD_BAR_ROW     2

// Estado del HUD de un jugador: cachea lo último dibujado para redibujar solo
// cuando cambia (evita reescribir VRAM cada frame).
typedef struct {
    Player* pl;
    u16     baseCol;    // columna de tile donde arranca el marco (0 = P1)
    u16     barVram;    // primer tile de VRAM del bloque de la barra (8 tiles)
    s16     lastHealth;
    s16     lastLives;
    s32     lastScore;
} HudPlayer;

// Convierte un u16 a decimal sin ceros a la izquierda. Devuelve la longitud.
static u16 uintToDec(u16 v, char* out) {
    char tmp[6];
    u16  n = 0;
    if (v == 0) { out[0] = '0'; out[1] = 0; return 1; }
    while (v > 0) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    for (u16 i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = 0;
    return n;
}

// Carga en VRAM el frame 'frame' de la barra (0 = llena .. 10 = vacía),
// pisando los 8 tiles del bloque. Los tiles de cada frame están contiguos y
// sin deduplicar en ROM (NONE NONE): frame N arranca en tile N*8 -> N*8*8
// longwords. DMA_QUEUE: la transferencia (256B) se hace en el próximo vblank.
static void hpBarSetFrame(u16 barVram, u8 frame) {
    VDP_loadTileData(hp_bar.tiles + (u32)frame * HPBAR_FRAME_TILES * 8,
                     barVram, HPBAR_FRAME_TILES, DMA_QUEUE);
}

// Inicializa el bloque de barra de un jugador: carga el frame lleno a VRAM y
// dibuja su tilemap 4x2 en BG_A (prioridad alta, PAL1) dentro del marco.
static void hpBarInit(u16 barVram, u16 baseCol) {
    VDP_loadTileData(hp_bar.tiles, barVram, HPBAR_FRAME_TILES, DMA);
    // fillTileMapRectInc incrementa el índice tile a tile (fila por fila), el
    // mismo orden en que quedan los 8 tiles del frame en el tileset.
    VDP_fillTileMapRectInc(BG_A,
                           TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, barVram),
                           baseCol + HUD_BAR_COL, HUD_BAR_ROW,
                           HPBAR_FRAME_TILES_W, HPBAR_FRAME_TILES_H);
}

// Prepara el HUD de un jugador. Llamar DESPUÉS de initPlayer (PAL1 cargada) y
// de fijar paleta/plano de texto. Fuerza el primer dibujado de cada elemento.
static void hudPlayerInit(HudPlayer* h, Player* pl, u16 baseCol, u16 barVram) {
    h->pl         = pl;
    h->baseCol    = baseCol;
    h->barVram    = barVram;
    h->lastHealth = -1;   // -1 = fuerza el primer redibujo
    h->lastLives  = -1;
    h->lastScore  = -1;
    hpBarInit(barVram, baseCol);
}

// Redibuja SOLO los elementos que cambiaron. Llamar una vez por frame.
static void hudPlayerUpdate(HudPlayer* h) {
    s16 hp    = getPlayerHealth(h->pl);
    s16 lives = (s16)getPlayerLives(h->pl);
    s32 score = (s32)getPlayerScore(h->pl);

    if (hp != h->lastHealth) {
        s16 frame = PLAYER_MAX_HEALTH - hp;
        if (frame < 0)                 frame = 0;
        if (frame > PLAYER_MAX_HEALTH) frame = PLAYER_MAX_HEALTH;
        hpBarSetFrame(h->barVram, (u8)frame);
        h->lastHealth = hp;
    }

    if (lives != h->lastLives) {
        char buf[6];
        buf[0] = 'x';
        uintToDec((u16)lives, buf + 1);            // p.ej. "x3"
        VDP_clearText(h->baseCol + HUD_LIVES_COL, HUD_LIVES_ROW, 3);
        VDP_drawText(buf, h->baseCol + HUD_LIVES_COL, HUD_LIVES_ROW);
        h->lastLives = lives;
    }

    if (score != h->lastScore) {
        char buf[6];
        u16  len = uintToDec((u16)score, buf);   // 1..5 dígitos
        u16  field = HUD_SCORE_RIGHT - HUD_SCORE_LEFT;   // 4 tiles (cols 4..7)
        if (len > field) len = field;                    // no invadir el "1UP"
        // Alinear a la derecha: el último dígito queda en la col 7.
        VDP_clearText(h->baseCol + HUD_SCORE_LEFT, HUD_SCORE_ROW, field);
        VDP_drawText(buf, h->baseCol + HUD_SCORE_RIGHT - len, HUD_SCORE_ROW);
        h->lastScore = score;
    }
}

// ===========================================================================
// SISTEMA DE CONTINUES — "CONTINUE?" con cuenta regresiva en el HUD
// ===========================================================================
// Cuando un jugador se queda sin vidas, su marco de HUD muestra "CONTINUE?"
// con una cuenta de 9 a 0 (~1s por dígito). El nivel SIGUE corriendo (estilo
// arcade): el compañero vivo puede seguir jugando. Si el muerto presiona
// START (de SU joystick) con continues disponibles, entra a la selección de
// tortuga: el retrato del borde cambia con los direccionales (saltando la
// tortuga del otro jugador) y START confirma -> revive donde cayó con vidas
// y barra completas. Si la cuenta llega a 0 sin continuar, el jugador queda
// fuera; en 2P el compañero vivo sigue solo.
// ---------------------------------------------------------------------------
#define CONT_START_SECONDS  9   // Cuenta inicial
// El texto del continue se dibuja CENTRADO en el hueco entre los dos marcos
// del HUD (pantalla de 40 columnas; los marcos ocupan 5..13 y 26..34, el
// centro queda ~20): "CONTINUE?" (9 chars) + el dígito de la cuenta van
// juntos desde la columna 15, en la fila 1 (HUD_SCORE_ROW). Ambos jugadores
// usan la misma posición central.
#define CONT_MSG_COL        15  // Columna inicial del mensaje centrado
#define CONT_MSG_TOTAL_W    10  // "CONTINUE?" (9) + dígito (1)

typedef enum { CONT_NONE, CONT_COUNTING, CONT_SELECTING } ContState;

typedef struct {
    ContState state;
    u8  seconds;     // 9..0 (CONT_COUNTING)
    u16 tick;        // Frames hasta el próximo segundo
    u8  sel;         // Selección actual (CONT_SELECTING)
    u16 prevJoy;     // Estado previo del joystick del muerto
} ContPlayer;

// Dibuja/limpia el texto "CONTINUE?" + cuenta, centrado entre los HUD.
// seconds >= 0 dibuja etiqueta + dígito; seconds < 0 solo limpia todo el
// mensaje (no redibuja la etiqueta: era el bug de las letras fantasma que
// quedaban al continuar).
static void contDrawText(HudPlayer* h, s8 seconds) {
    (void)h;
    VDP_clearText(CONT_MSG_COL, HUD_SCORE_ROW, CONT_MSG_TOTAL_W);
    if (seconds < 0) return;
    VDP_drawText("CONTINUE?", CONT_MSG_COL, HUD_SCORE_ROW);
    char buf[2];
    buf[0] = (char)('0' + seconds);
    buf[1] = 0;
    VDP_drawText(buf, CONT_MSG_COL + 9, HUD_SCORE_ROW);
}

// Revive al jugador con una tortuga nueva (continue): libera el sprite KO,
// re-inicializa con el personaje elegido en el lugar donde cayó y restaura
// vidas/barra completas con i-frames para no morir al instante.
static void revivePlayer(Player* p, u8 ch, u16 joyId) {
    if (p->sprite) SPR_releaseSprite(p->sprite);
    initPlayer(p, ch, joyId, PAL1, p->x, p->y);
    p->lives      = vidasIniciales;
    p->health     = PLAYER_MAX_HEALTH;
    p->gameOver   = FALSE;
    p->invincible = PLAYER_RESPAWN_INVINCIBLE;
    p->blinkTimer = PLAYER_RESPAWN_INVINCIBLE;
}

// Procesa un frame del continue de un jugador. Devuelve TRUE si el jugador
// quedó fuera permanentemente (no continuó a tiempo). 'charSel' es un puntero
// al global del personaje de este jugador (se actualiza al confirmar);
// 'otherChar' es el personaje del OTRO jugador (se saltea al seleccionar; en
// 1P pasar 0xFF para no saltar ninguno). 'fps' da el ritmo de la cuenta.
static bool continuePoll(ContPlayer* c, Player* p, HudPlayer* h, Sprite* frameSpr,
                         Sprite* portrait, u16 joyId, u8* charSel, u8 otherChar, u16 fps) {
    // Vivo: nada que hacer (y limpiar si quedó texto de un continue previo).
    if (!isPlayerGameOver(p)) {
        if (c->state != CONT_NONE) {
            contDrawText(h, -1);
            c->state = CONT_NONE;
        }
        return FALSE;
    }

    u16 joy = JOY_readJoypad(joyId);

    // Arranca la cuenta cuando el jugador acaba de caer.
    if (c->state == CONT_NONE) {
        c->state   = CONT_COUNTING;
        c->seconds = CONT_START_SECONDS;
        c->tick    = 0;
        c->prevJoy = 0;
        contDrawText(h, (s8)c->seconds);
        return FALSE;
    }

    if (c->state == CONT_COUNTING) {
        // START del joystick del muerto + continues disponibles -> selección.
        if (continuesLeft > 0 && justPressedJoy(joy, c->prevJoy, BUTTON_START)) {
            continuesLeft--;
            c->state   = CONT_SELECTING;
            c->sel     = *charSel;
            c->prevJoy = joy;
            contDrawText(h, -1);
            return FALSE;
        }
        c->tick++;
        if (c->tick >= fps) {
            c->tick = 0;
            if (c->seconds > 0) {
                c->seconds--;
                contDrawText(h, (s8)c->seconds);
            } else {
                // Se mostró el 0 un segundo completo: quedó fuera.
                contDrawText(h, -1);
                return TRUE;
            }
        }
        c->prevJoy = joy;
        return FALSE;
    }

    // CONT_SELECTING: direccionales cambian el retrato (sin pisar al otro).
    if (justPressedJoy(joy, c->prevJoy, BUTTON_RIGHT))
        c->sel = (u8)charMove((s8)c->sel, (s8)otherChar, +1);
    if (justPressedJoy(joy, c->prevJoy, BUTTON_LEFT))
        c->sel = (u8)charMove((s8)c->sel, (s8)otherChar, -1);
    if (portrait) SPR_setAnim(portrait, c->sel);

    if (justPressedJoy(joy, c->prevJoy, BUTTON_START)) {
        *charSel = c->sel;
        if (frameSpr) SPR_setAnim(frameSpr, c->sel);
        revivePlayer(p, c->sel, joyId);
        // Forzar el redibujo del HUD (vidas/barra cambiaron) y limpiar texto.
        hudPlayerInit(h, h->pl, h->baseCol, h->barVram);
        contDrawText(h, -1);
        c->state = CONT_NONE;
        return FALSE;
    }

    c->prevJoy = joy;
    return FALSE;
}

// ---------------------------------------------------------------------------
// 1. Intro SEGA — Rocksteady choca el logo
// ---------------------------------------------------------------------------
SceneId showSegaIntro() {
    Sprite *segaLogo   = SPR_addSprite(&sega_logo_spr,  104, 92, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
    Sprite *rocksteady = SPR_addSprite(&rocksteady_spr, -90, 80, TILE_ATTR(PAL1, FALSE, FALSE, FALSE));

    PAL_setPalette(PAL0, sega_logo_spr.palette->data, DMA);
    PAL_setPalette(PAL1, rocksteady_spr.palette->data, DMA);

    s16  rockX  = -90;
    u16  timerKO = 0;
    u16  estado  = 0;  // 0:corriendo 1:impacto 2:KO 3:fade

    SPR_setAnim(segaLogo,   0);
    SPR_setAnim(rocksteady, 0);
    playMusicVol(music_sega, VOL_MUSIC_INTRO);

    while (1) {
        if (estado == 0) {
            rockX += 3;
            if (rockX >= 40) {
                estado = 1;
                SPR_setAnim(segaLogo,   1);
                SPR_setAnim(rocksteady, 1);
                XGM2_stop();
                playMusicVol(golpe, VOL_SFX);
            }
        } else if (estado == 1) {
            if (++timerKO > 20) { estado = 2; SPR_setAnim(rocksteady, 2); timerKO = 0; }
        } else if (estado == 2) {
            if (++timerKO > 60) { estado = 3; PAL_fadeOutAll(30, FALSE); }
        } else if (estado == 3) {
            if (!PAL_isDoingFade()) break;
        }

        SPR_setPosition(rocksteady, rockX, 80);
        SPR_update();
        SYS_doVBlankProcess();
    }

    if (segaLogo)   SPR_releaseSprite(segaLogo);
    if (rocksteady) SPR_releaseSprite(rocksteady);
    SPR_update();
    SYS_doVBlankProcess();

    clearScene();
    // Konami es todavía un stub que pasa de largo a la pantalla SGDK
    return SCENE_KONAMI;
}

// ---------------------------------------------------------------------------
// 2/4. Intros pendientes (stubs) — showSGDKIntro ya está implementada más
// abajo (necesita drawTextTypewriter, definida junto al título del nivel).
// ---------------------------------------------------------------------------
SceneId showKonamiIntro()  { return SCENE_SGDK; }

// ---------------------------------------------------------------------------
// Intro arcade (TMNT) — 4 fases: cielo → edificios → líneas → calle
// ---------------------------------------------------------------------------
// Fondo: cielo (BG_B, estático) + contenido dinámico (BG_A).
// Edificios: scroll vertical de BG_A de 0→288 px (cámara desciende).
// Líneas de velocidad: tiling a velocidad alta durante 3 s.
// Calle: scroll de BG_A de 0→164 px hasta alinear fondo con borde inferior,
//        pausa 2 s, fade a negro.
// START saltea la secuencia.
// ---------------------------------------------------------------------------
#define INTRO_SKY_X         0
#define INTRO_SKY_X2        32
#define INTRO_BUILDING_X    4
#define INTRO_STREET_X      4
#define INTRO_SPEED_COLS    32
#define INTRO_SPEED_ROWS    8
#define INTRO_HOLD_SECS     1
#define INTRO_END_HOLD      2
#define INTRO_SPEED_SECS    3
#define INTRO_MIN_SPEED     2
#define INTRO_MAX_SPEED     8

SceneId showArcadeIntro() {
    clearScene();
    SPR_initEx(420);
    VDP_setPlaneSize(BG_PLANE_W, 64, TRUE);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setBackgroundColor(0);

    const u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    const u16 baseTiles = TILE_USER_INDEX;
    const u16 skyTileCount = 34;   // intro_sky unique tiles (1088 / 32)
    const u16 otherTileBase = baseTiles + skyTileCount;

    // ---- Fase 0: Cielo (BG_B, estático) ----
    u16 skyAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, baseTiles);
    VDP_drawImageEx(BG_B, &intro_sky, skyAttr, INTRO_SKY_X,  0, FALSE, TRUE);
    VDP_drawImageEx(BG_B, &intro_sky, skyAttr, INTRO_SKY_X2, 0, FALSE, TRUE);
    PAL_setPalette(PAL0, intro_sky.palette->data, CPU);
    PAL_fadeIn(0, 15, intro_sky.palette->data, 20, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    u16 holdTimer = fps * INTRO_HOLD_SECS;
    while (holdTimer > 0) {
        if (JOY_readJoypad(JOY_1) & BUTTON_START) goto fin;
        holdTimer--;
        SYS_doVBlankProcess();
    }

    // ---- Fase 1: Edificios (BG_A, scroll vertical) ----
    VDP_clearPlane(BG_A, TRUE);
    {
        u16 buildAttr = TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, otherTileBase);
        VDP_drawImageEx(BG_A, &intro_buildings, buildAttr, INTRO_BUILDING_X,    0, FALSE, TRUE);
        VDP_drawImageEx(BG_A, &intro_buildings, buildAttr, INTRO_BUILDING_X + 32, 0, FALSE, TRUE);
    }
    PAL_setPalette(PAL0, intro_sky.palette->data,       DMA);
    PAL_setPalette(PAL1, intro_buildings.palette->data, DMA);
    VDP_setVerticalScroll(BG_A, 0);

    {
        s16 scrollY = 0;
        const s16 scrollMax = 288;
        const s16 scrollMid = scrollMax / 2;
        while (scrollY < scrollMax) {
            if (JOY_readJoypad(JOY_1) & BUTTON_START) goto fin;
            s16 speed;
            if (scrollY < scrollMid)
                speed = INTRO_MIN_SPEED + ((INTRO_MAX_SPEED - INTRO_MIN_SPEED) * scrollY) / scrollMid;
            else
                speed = INTRO_MAX_SPEED - ((INTRO_MAX_SPEED - INTRO_MIN_SPEED) * (scrollY - scrollMid)) / scrollMid;
            scrollY += speed;
            if (scrollY > scrollMax) scrollY = scrollMax;
            VDP_setVerticalScroll(BG_A, scrollY);
            SYS_doVBlankProcess();
        }
    }

    // ---- Fase 2: Líneas de velocidad (BG_A, tiling + scroll rápido) ----
    VDP_clearPlane(BG_A, TRUE);
    {
        u16 speedAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, otherTileBase);
        for (u16 r = 0; r < 64; r += INTRO_SPEED_ROWS)
            for (u16 c = 0; c < 64; c += INTRO_SPEED_COLS)
                VDP_drawImageEx(BG_A, &intro_speed, speedAttr, c, r, FALSE, TRUE);
    }
    PAL_setPalette(PAL0, intro_sky.palette->data, DMA);
    VDP_setVerticalScroll(BG_A, 0);

    {
        u16 speedTimer = fps * INTRO_SPEED_SECS;
        s16 scrollY = 0;
        while (speedTimer > 0) {
            if (JOY_readJoypad(JOY_1) & BUTTON_START) goto fin;
            speedTimer--;
            scrollY += 6;
            VDP_setVerticalScroll(BG_A, scrollY);
            SYS_doVBlankProcess();
        }
    }

    // ---- Fase 3: Calle (BG_A, scroll hasta alinear fondo) ----

    VDP_clearPlane(BG_A, TRUE);
    {
        u16 streetAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, otherTileBase);
        VDP_drawImageEx(BG_A, &intro_street, streetAttr, INTRO_STREET_X,    0, FALSE, TRUE);
        VDP_drawImageEx(BG_A, &intro_street, streetAttr, INTRO_STREET_X + 32, 0, FALSE, TRUE);
    }
    PAL_setPalette(PAL0, intro_sky.palette->data, DMA);
    VDP_setVerticalScroll(BG_A, 0);

    {
        s16 scrollY = 0;
        const s16 scrollMax = 168;  // 392 - 224
        while (scrollY < scrollMax) {
            if (JOY_readJoypad(JOY_1) & BUTTON_START) goto fin;
            scrollY += 3;
            if (scrollY > scrollMax) scrollY = scrollMax;
            VDP_setVerticalScroll(BG_A, scrollY);
            SYS_doVBlankProcess();
        }

        holdTimer = fps * INTRO_END_HOLD;
        while (holdTimer > 0) {
            if (JOY_readJoypad(JOY_1) & BUTTON_START) goto fin;
            holdTimer--;
            SYS_doVBlankProcess();
        }
    }

fin:
    while (JOY_readJoypad(JOY_1) & BUTTON_START)
        SYS_doVBlankProcess();

    PAL_fadeOutAll(10, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);
    SYS_doVBlankProcess();
    VDP_setPlaneSize(32, 32, TRUE);
    SPR_initEx(752);
    clearScene();
    return SCENE_VRAM_CLEAR;
}

// ---------------------------------------------------------------------------
// 5a. Escena BUFFER: borrado total de VRAM entre la intro y los menús
// ---------------------------------------------------------------------------
// En hardware real la VRAM conserva todo lo que escribieron las escenas
// previas (los emuladores arrancan con la VRAM en 0 y lo enmascaran). Además
// la tabla de sprites (SAT) cambia de dirección según el tamaño de plano
// (0xAC00 en la intro 64x64 -> 0xF400 en los menús 64x32) y queda leyendo
// datos viejos hasta el primer SPR_update. Eso corrompía el menú de players
// en hardware real (el fondo aparecía un instante y desaparecía). Esta escena
// no dibuja nada: pone a CERO los 64KB de VRAM (tiles, tilemaps, SAT y tabla
// HSCROLL) y la CRAM, y deja el VDP en el estado default que esperan los menús.
// ---------------------------------------------------------------------------
static const u16 vramClearBlackPal[64] = { 0 };   // CRAM entera a negro

SceneId showVramClear() {
    clearScene();

    // Soltar el motor de sprites y asegurarse de que NO quede ningún DMA
    // encolado que aterrice DESPUÉS del borrado (lo re-ensuciaría).
    SPR_end();
    SYS_doVBlankProcess();
    DMA_clearQueue();

    // Display apagado: el fill corre a máxima velocidad y no se ve nada raro.
    VDP_setEnable(FALSE);

    // Layout de plano que usan los menús (SAT en 0xF400, HSCROLL en 0xF000).
    VDP_setPlaneSize(BG_PLANE_W, 32, TRUE);

    // Borrado TOTAL de la VRAM: len = 0 es el valor especial del DMA fill
    // para 0x10000 bytes (64KB) — es lo mismo que hace VDP_resetScreen() de
    // SGDK al arrancar. Una SAT en cero = lista terminada = sin sprites.
    DMA_doVRamFill(0, 0, 0, 1);
    VDP_waitDMACompletion();

    // CRAM a negro (las 4 paletas completas), escritura directa sin fade.
    PAL_setColors(0, vramClearBlackPal, 64, CPU);

    // Scroll y modo normalizados.
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);
    VDP_setBackgroundColor(0);

    // Motor de sprites en el estado default del juego (los menús lo ajustan).
    SPR_initEx(752);

    VDP_setEnable(TRUE);
    SYS_doVBlankProcess();
    SYS_doVBlankProcess();

    return SCENE_PLAYER_SELECT;
}

// ---------------------------------------------------------------------------
// 5. Selección de número de jugadores
// ---------------------------------------------------------------------------
SceneId showPlayerSelect() {
    clearScene();
    // El fondo 320x224 necesita muchos tiles de usuario. Reducimos el
    // presupuesto de sprites para que los tiles del logo no se superpongan.
    SPR_end();
    SPR_initEx(420);
    // Plano de 64 tiles de ancho para que quepa el fondo completo.
    VDP_setPlaneSize(BG_PLANE_W, 32, TRUE);

    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    // Estabilización: dar tiempo a que queden libres DMA y comandos CPU.
    SYS_doVBlankProcess();
    SYS_doVBlankProcess();

    VDP_setBackgroundColor(0);
    PAL_setPalette(PAL0, logo.palette->data, CPU);

    // Fondo: 320x224 px en BG_B. CPU para evitar condiciones de carrera.
    VDP_loadTileSet(logo.tileset, TILE_USER_INDEX, CPU);
    VDP_setTileMapEx(BG_B, logo.tilemap,
                     TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX),
                     0, 0, 0, 0, logo.tilemap->w, logo.tilemap->h, CPU);

    // Fuente arcade en PAL2. OJO: title_font_pal tiene 64 entradas
    // (rescomp exporta la paleta completa del PNG). Si copiamos las 64 con
    // PAL_setColors a partir de PAL2 (índice 32), la escritura envuelve en
    // CRAM (que es de 64 colores) y pisa PAL0/PAL1 → el fondo desaparece.
    // Usamos PAL_setPalette que escribe exactamente 16 colores (una línea).
    VDP_loadFont(&title_font, CPU);
    PAL_setPalette(PAL2, title_font_pal.data, CPU);
    VDP_setTextPalette(PAL2);

    VDP_drawText("1 TORTUGA",  14, 22);
    VDP_drawText("2 TORTUGAS", 14, 24);
    VDP_drawText("OPCIONES",   14, 26);

    Sprite *cursor = SPR_addSprite(&selector_turtle, 8 * 8, 18 * 8, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    PAL_setPalette(PAL1, selector_turtle.palette->data, CPU);

    u8  selectedOption = 0;
    u16 prev = 0;

    // Esperar a que se suelte START para no confirmar al instante.
    while (JOY_readJoypad(JOY_1) & BUTTON_START)
        SYS_doVBlankProcess();

    while (1) {
        u16 value = JOY_readJoypad(JOY_1);

        if (justPressedJoy(value, prev, BUTTON_UP))   selectedOption = (selectedOption + 2) % 3;
        if (justPressedJoy(value, prev, BUTTON_DOWN)) selectedOption = (selectedOption + 1) % 3;

        SPR_setPosition(cursor, 8 * 8, (18 + selectedOption * 2) * 8);

        if (value & BUTTON_START) break;

        prev = value;
        SPR_update();
        SYS_doVBlankProcess();
    }

    // selectedOption 0 → 1 jugador | 1 → 2 jugadores | 2 → OPCIONES
    // (OPCIONES no toca cantidadJugadores: vuelve acá al salir).
    if (selectedOption < 2)
        cantidadJugadores = selectedOption + 1;

    // Restaurar fuente default.
    VDP_loadFont(&font_default, DMA);

    SPR_end();
    SPR_initEx(752);

    return (selectedOption == 2) ? SCENE_OPTIONS : SCENE_CHAR_SELECT;
}

// ---------------------------------------------------------------------------
// 5b. OPCIONES — VIDAS (3/5/7), SOUNDTEST y SALIR
// ---------------------------------------------------------------------------
// Mismo look que la selección de players (logo de fondo + fuente arcade).
// VIDAS configura el global vidasIniciales (lo usan playerPersistReset y
// revivePlayer: aplica a partida nueva y a continues). SOUNDTEST reproduce
// los VGM de los niveles: A o C = play/stop, LEFT/RIGHT cambia de pista
// (por ahora solo FIRE! del nivel 1). SALIR (START sobre la fila, o B en
// cualquier fila) vuelve a la selección de cantidad de players.
// ---------------------------------------------------------------------------
#define OPT_ROW_VIDAS      0
#define OPT_ROW_SOUNDTEST  1
#define OPT_ROW_SALIR      2
#define OPT_ROW_COUNT      3

#define OPT_LABEL_COL      12   // columna de los labels
#define OPT_VALUE_COL      24   // columna de los valores < ... >
#define OPT_STATUS_COL     34   // columna del indicador ON/OFF del soundtest
#define OPT_ROW_Y          20   // fila de texto de VIDAS; las demás van +2
// El cursor (selector_turtle, 64x64) se dibuja 4 tiles arriba de la fila de
// texto, igual que en showPlayerSelect (fila de texto 22 -> cursor fila 18).
#define OPT_CURSOR_Y       (OPT_ROW_Y - 4)

// Pistas del sound test: nombre en pantalla (ASCII puro) + recurso XGM2.
typedef struct { const char* name; const u8* track; } SoundTrack;
static const SoundTrack soundTracks[] = {
    { "FIRE!", music_level1 },   // música del nivel 1
};
#define SOUND_TRACK_COUNT  (sizeof(soundTracks) / sizeof(soundTracks[0]))

// Redibuja el valor de la fila VIDAS ("< 3 >" — ancho fijo, pisa solo).
static void optDrawLives(u8 lives) {
    char buf[6];
    buf[0] = '<'; buf[1] = ' ';
    buf[2] = (char)('0' + lives);
    buf[3] = ' '; buf[4] = '>'; buf[5] = 0;
    VDP_drawText(buf, OPT_VALUE_COL, OPT_ROW_Y);
}

// Redibuja la fila SOUNDTEST: nombre de la pista + indicador ON/OFF.
static void optDrawSound(u8 trackIdx, bool playing) {
    char buf[16];
    u8 i = 0;
    const char* name = soundTracks[trackIdx].name;
    buf[i++] = '<'; buf[i++] = ' ';
    while (*name && i < 12) buf[i++] = *name++;
    buf[i++] = ' '; buf[i++] = '>'; buf[i] = 0;
    VDP_clearText(OPT_VALUE_COL, OPT_ROW_Y + 2, 10);
    VDP_drawText(buf, OPT_VALUE_COL, OPT_ROW_Y + 2);
    VDP_clearText(OPT_STATUS_COL, OPT_ROW_Y + 2, 3);
    VDP_drawText(playing ? "ON" : "OFF", OPT_STATUS_COL, OPT_ROW_Y + 2);
}

SceneId showOptions() {
    clearScene();
    // Mismo criterio de VRAM que showPlayerSelect: presupuesto de sprites
    // reducido para dar aire al fondo 320x224.
    SPR_end();
    SPR_initEx(420);
    VDP_setPlaneSize(BG_PLANE_W, 32, TRUE);

    // En hardware real, el DMA puede quedar desincronizado entre escenas.
    // Usamos CPU para las operaciones críticas de setup para garantizar
    // sincronización, igual que en showPlayerSelect.
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    // Frame de estabilización.
    SYS_doVBlankProcess();
    SYS_doVBlankProcess();

    VDP_setBackgroundColor(0);
    PAL_setPalette(PAL0, logo.palette->data, CPU);

    // Cargar logo con CPU para evitar condiciones de carrera con DMA.
    VDP_loadTileSet(logo.tileset, TILE_USER_INDEX, CPU);
    VDP_setTileMapEx(BG_B, logo.tilemap,
                     TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX),
                     0, 0, 0, 0, logo.tilemap->w, logo.tilemap->h, CPU);

    VDP_loadFont(&title_font, CPU);
    // OJO: title_font_pal tiene 64 entradas. Copiar las 64 a partir de PAL2
    // envuelve en CRAM y pisa PAL0/PAL1 (igual que el bug de showPlayerSelect).
    PAL_setPalette(PAL2, title_font_pal.data, CPU);
    VDP_setTextPalette(PAL2);

    VDP_drawText("VIDAS",     OPT_LABEL_COL, OPT_ROW_Y);
    VDP_drawText("SOUNDTEST", OPT_LABEL_COL, OPT_ROW_Y + 2);
    VDP_drawText("SALIR",     OPT_LABEL_COL, OPT_ROW_Y + 4);

    Sprite *cursor = SPR_addSprite(&selector_turtle, 4 * 8, OPT_CURSOR_Y * 8, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    PAL_setPalette(PAL1, selector_turtle.palette->data, DMA);

    // VIDAS arranca reflejando el valor actual del global.
    static const u8 livesValues[] = {3, 5, 7};
    u8 livesIdx = (vidasIniciales >= 7) ? 2 : (vidasIniciales >= 5) ? 1 : 0;
    u8 trackIdx = 0;
    // Estado del soundtest. Se lleva LOCAL: XGM2_isPlaying() lee el status del
    // Z80 y tarda ~1 frame en reflejar un play/stop (el indicador mentiría).
    bool sndPlaying = FALSE;

    optDrawLives(livesValues[livesIdx]);
    optDrawSound(trackIdx, FALSE);

    u8  sel  = 0;
    u16 prev = 0;

    // START/B pueden venir presionados del menú de players: esperar release.
    while (JOY_readJoypad(JOY_1) & (BUTTON_START | BUTTON_B))
        SYS_doVBlankProcess();

    while (1) {
        u16 value = JOY_readJoypad(JOY_1);

        if (justPressedJoy(value, prev, BUTTON_UP))   sel = (sel + OPT_ROW_COUNT - 1) % OPT_ROW_COUNT;
        if (justPressedJoy(value, prev, BUTTON_DOWN)) sel = (sel + 1) % OPT_ROW_COUNT;

        if (sel == OPT_ROW_VIDAS) {
            bool changed = FALSE;
            if (justPressedJoy(value, prev, BUTTON_LEFT))  { livesIdx = (livesIdx + 2) % 3; changed = TRUE; }
            if (justPressedJoy(value, prev, BUTTON_RIGHT)) { livesIdx = (livesIdx + 1) % 3; changed = TRUE; }
            if (changed) {
                vidasIniciales = livesValues[livesIdx];
                optDrawLives(vidasIniciales);
            }
        } else if (sel == OPT_ROW_SOUNDTEST) {
            bool trackChanged = FALSE;
            if (justPressedJoy(value, prev, BUTTON_LEFT))  { trackIdx = (trackIdx + SOUND_TRACK_COUNT - 1) % SOUND_TRACK_COUNT; trackChanged = TRUE; }
            if (justPressedJoy(value, prev, BUTTON_RIGHT)) { trackIdx = (trackIdx + 1) % SOUND_TRACK_COUNT; trackChanged = TRUE; }
            if (trackChanged) {
                // Si estaba sonando, arrancar la pista nueva.
                if (sndPlaying) playMusicVol(soundTracks[trackIdx].track, 100);
                optDrawSound(trackIdx, sndPlaying);
            }
            if (justPressedJoy(value, prev, BUTTON_A) || justPressedJoy(value, prev, BUTTON_C)) {
                if (sndPlaying) { XGM2_stop(); sndPlaying = FALSE; }
                else { playMusicVol(soundTracks[trackIdx].track, 100); sndPlaying = TRUE; }
                optDrawSound(trackIdx, sndPlaying);
            }
        }

        SPR_setPosition(cursor, 4 * 8, (OPT_CURSOR_Y + sel * 2) * 8);

        // SALIR: START sobre la fila SALIR, o B en cualquier fila.
        if (((value & BUTTON_START) && sel == OPT_ROW_SALIR) || (value & BUTTON_B))
            break;

        prev = value;
        SPR_update();
        SYS_doVBlankProcess();
    }

    // Cortar la música del soundtest y esperar release para que el mismo
    // botón no atraviese la pantalla de players al volver.
    XGM2_stop();
    while (JOY_readJoypad(JOY_1) & (BUTTON_START | BUTTON_B))
        SYS_doVBlankProcess();

    // Restaurar la fuente default y el presupuesto de sprites del juego.
    VDP_loadFont(&font_default, DMA);
    SPR_end();
    SPR_initEx(752);

    return SCENE_PLAYER_SELECT;
}

// ---------------------------------------------------------------------------
// 6. Selección de personaje
// ---------------------------------------------------------------------------
SceneId showCharSelect() {
    clearScene();
    // El fondo también es ahora 320x224 px (40x28 tiles), así que usamos el
    // mismo plano 64x32 que la selección de players.
    VDP_setPlaneSize(BG_PLANE_W, 32, TRUE);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    PAL_setPalette(PAL0, characters_greyscale.palette->data, DMA);
    VDP_drawImageEx(BG_B, &characters_greyscale, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX), 0, 0, FALSE, TRUE);

    playMusicVol(select_music, VOL_MUSIC_SELECT);

    const s16 charPosX[] = {8, 88, 168, 248};
    const s16 charPosY   = 48;

    // Mapa columna → fila de la sheet de caras (sprite_sheet_faces.png).
    // Columnas en pantalla: 0=Leo 1=Mike 2=Don 3=Raph.
    // Filas de caras:       0=azul(Leo) 1=dorado(Raph) 2=púrpura(Don) 3=naranja(Mike).
    const u8  faceRow[]  = {0, 3, 2, 1};
    const s16 faceXoff   = 16;   // centra la cara de 32px sobre la columna de 64px
    const s16 faceY      = 26;   // se apoya en la parte superior del retrato elegido

    // HUD en la parte superior, igual que en los niveles. El spritesheet de los
    // marcos tiene los frames en orden de personaje (0=Leo, 1=Mike, 2=Don, 3=Raph),
    // pero en esta pantalla el cursor recorre: 0=Leo, 1=Mike, 2=Don, 3=Raph.
    // Mapeo de índice de cursor → frame del HUD.
    const u8 hudAnimForChar[] = {0, 3, 2, 1};

    // Paletas: el HUD comparte la de las tortugas en PAL1. El selector de
    // personaje va en PAL3 para no pisar los colores del HUD.
    PAL_setPalette(PAL1, hud_1p.palette->data, DMA);
    PAL_setPalette(PAL2, faces_hud.palette->data, DMA);
    PAL_setPalette(PAL3, character_selector.palette->data, DMA);

    // Cargar marcos + retratos del HUD. Inicialmente muestran los personajes
    // que haya en las variables persistentes; luego se fuerza el frame según
    // la selección actual del cursor.
    hudInit();

    // -----------------------------------------------------------------------
    // MODO 1 JUGADOR
    // -----------------------------------------------------------------------
    if (cantidadJugadores == 1) {
        Sprite* cursor          = SPR_addSprite(&character_selector, charPosX[0], charPosY, TILE_ATTR(PAL3, TRUE, FALSE, FALSE));
        Sprite* turtle_face_hud = SPR_addSprite(&faces_hud,          charPosX[0] + faceXoff, faceY, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
        SPR_setDepth(cursor, 1);            // cursor/retrato coloreado detrás
        SPR_setDepth(turtle_face_hud, 0);   // la cara va adelante

        s8   sel        = 0;
        u16  prev       = 0;
        SPR_setAnim(cursor,          sel);
        SPR_setAnim(turtle_face_hud, faceRow[sel]);
        if (hudSprite1)     SPR_setAnim(hudSprite1,     hudAnimForChar[sel]);
        if (portraitSpr1)   SPR_setAnim(portraitSpr1,   sel);

        while (1) {
            u16 v = JOY_readJoypad(JOY_1);

            if (justPressedJoy(v, prev, BUTTON_RIGHT) && sel < 3) sel++;
            if (justPressedJoy(v, prev, BUTTON_LEFT)  && sel > 0) sel--;

            SPR_setAnim(cursor,          sel);
            SPR_setAnim(turtle_face_hud, faceRow[sel]);
            if (hudSprite1)     SPR_setAnim(hudSprite1,     hudAnimForChar[sel]);
            if (portraitSpr1)   SPR_setAnim(portraitSpr1,   sel);
            SPR_setPosition(cursor, charPosX[sel], charPosY);
            SPR_setPosition(turtle_face_hud, charPosX[sel] + faceXoff, faceY);

            if (v & BUTTON_START) { personajeSeleccionado = sel; break; }

            prev = v;
            SPR_update();
            SYS_doVBlankProcess();
        }

        XGM2_stop();
        PAL_fadeOutAll(20, FALSE);
        while (PAL_isDoingFade()) SYS_doVBlankProcess();

        if (cursor)          SPR_releaseSprite(cursor);
        if (turtle_face_hud) SPR_releaseSprite(turtle_face_hud);
        VDP_clearPlane(BG_A, TRUE);
        VDP_clearPlane(BG_B, TRUE);
        SPR_update();
        SYS_doVBlankProcess();

        // Nueva partida: reiniciar vidas/puntaje persistentes y continues
        // antes del nivel 1.
        playerPersistReset();
        continuesLeft = 3;
        clearScene();
        return SCENE_LEVEL1_TITLE;
    }

    // -----------------------------------------------------------------------
    // MODO 2 JUGADORES — JOY_1 = P1, JOY_2 = P2. No pueden elegir el mismo.
    // Cada uno confirma con START; cuando ambos confirman, se avanza.
    // -----------------------------------------------------------------------
    s8   sel1 = 0, sel2 = 3;          // empiezan en personajes distintos
    bool ready1 = FALSE, ready2 = FALSE;
    u16  prev1 = 0, prev2 = 0;

    Sprite* cur1  = SPR_addSprite(&character_selector, charPosX[sel1], charPosY, TILE_ATTR(PAL3, TRUE, FALSE, FALSE));
    Sprite* cur2  = SPR_addSprite(&character_selector, charPosX[sel2], charPosY, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    Sprite* face1 = SPR_addSprite(&faces_hud, charPosX[sel1] + faceXoff, faceY, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
    Sprite* face2 = SPR_addSprite(&faces_hud, charPosX[sel2] + faceXoff, faceY, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));

    // Las caras van adelante; los cursores/retratos coloreados, detrás.
    SPR_setDepth(cur1, 1);  SPR_setDepth(cur2, 1);
    SPR_setDepth(face1, 0); SPR_setDepth(face2, 0);

    SPR_setAnim(cur1, sel1);  SPR_setAnim(face1, faceRow[sel1]);
    SPR_setAnim(cur2, sel2);  SPR_setAnim(face2, faceRow[sel2]);
    if (hudSprite1)   SPR_setAnim(hudSprite1,   hudAnimForChar[sel1]);
    if (hudSprite2)   SPR_setAnim(hudSprite2,   hudAnimForChar[sel2]);
    if (portraitSpr1) SPR_setAnim(portraitSpr1, sel1);
    if (portraitSpr2) SPR_setAnim(portraitSpr2, sel2);

    while (1) {
        u16 v1 = JOY_readJoypad(JOY_1);
        u16 v2 = JOY_readJoypad(JOY_2);

        // --- Jugador 1 (mientras no haya confirmado) ---
        if (!ready1) {
            if (justPressedJoy(v1, prev1, BUTTON_RIGHT)) sel1 = charMove(sel1, sel2, +1);
            if (justPressedJoy(v1, prev1, BUTTON_LEFT))  sel1 = charMove(sel1, sel2, -1);
            SPR_setAnim(cur1, sel1);
            SPR_setAnim(face1, faceRow[sel1]);
            if (hudSprite1)   SPR_setAnim(hudSprite1,   hudAnimForChar[sel1]);
            if (portraitSpr1) SPR_setAnim(portraitSpr1, sel1);
            SPR_setPosition(cur1, charPosX[sel1], charPosY);
            SPR_setPosition(face1, charPosX[sel1] + faceXoff, faceY);
            if (justPressedJoy(v1, prev1, BUTTON_START)) ready1 = TRUE;
        }

        // --- Jugador 2 (mientras no haya confirmado) ---
        if (!ready2) {
            if (justPressedJoy(v2, prev2, BUTTON_RIGHT)) sel2 = charMove(sel2, sel1, +1);
            if (justPressedJoy(v2, prev2, BUTTON_LEFT))  sel2 = charMove(sel2, sel1, -1);
            SPR_setAnim(cur2, sel2);
            SPR_setAnim(face2, faceRow[sel2]);
            if (hudSprite2)   SPR_setAnim(hudSprite2,   hudAnimForChar[sel2]);
            if (portraitSpr2) SPR_setAnim(portraitSpr2, sel2);
            SPR_setPosition(cur2, charPosX[sel2], charPosY);
            SPR_setPosition(face2, charPosX[sel2] + faceXoff, faceY);
            if (justPressedJoy(v2, prev2, BUTTON_START)) ready2 = TRUE;
        }

        if (ready1 && ready2) {
            personajeSeleccionado  = sel1;
            personaje2Seleccionado = sel2;
            break;
        }

        prev1 = v1;
        prev2 = v2;
        SPR_update();
        SYS_doVBlankProcess();
    }

    XGM2_stop();
    PAL_fadeOutAll(20, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    if (cur1)  SPR_releaseSprite(cur1);
    if (cur2)  SPR_releaseSprite(cur2);
    if (face1) SPR_releaseSprite(face1);
    if (face2) SPR_releaseSprite(face2);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    SPR_update();
    SYS_doVBlankProcess();

    // Nueva partida: reiniciar vidas/puntaje persistentes y continues
    // antes del nivel 1.
    playerPersistReset();
    continuesLeft = 3;
    return SCENE_LEVEL1_TITLE;
}

SceneId showFireCinematic() { return SCENE_LEVEL1_TITLE; }

// ---------------------------------------------------------------------------
// 7. Título del nivel 1 — texto letra a letra con la fuente arcade
// ---------------------------------------------------------------------------
// La fuente (title_font en level1.res) está en orden ASCII 32..126, tiles de
// 8x8, así que se carga con VDP_loadFont y VDP_drawText funciona directo.
// ---------------------------------------------------------------------------
#define TITLE_CHAR_DELAY  5   // frames entre letra y letra (~12 letras/seg)

// Dibuja el texto letra a letra con 'delay' frames entre letras.
// Devuelve TRUE si se pidió saltar con START.
static bool drawTextTypewriter(const char* text, u16 x, u16 y, u16 delay) {
    char buf[2];
    buf[1] = 0;

    for (u16 i = 0; text[i] != 0; i++) {
        buf[0] = text[i];

        // Los espacios no se dibujan, pero sí consumen tiempo (ritmo natural)
        if (buf[0] != ' ')
            VDP_drawText(buf, x + i, y);

        // Espera entre letras, con posibilidad de saltar
        for (u16 f = 0; f < delay; f++) {
            if (JOY_readJoypad(JOY_1) & BUTTON_START)
                return TRUE;
            SYS_doVBlankProcess();
        }
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// 3. Pantalla SGDK — créditos de herramientas y agradecimientos (bilingüe)
// ---------------------------------------------------------------------------
// Usa la fuente arcade del título (title_font). OJO: la fuente cubre ASCII
// 32..126, por eso los textos van SIN acentos ni signos especiales.
// ---------------------------------------------------------------------------
#define CREDITS_CHAR_DELAY  2   // Más rápido que el título (hay mucho texto)
#define CREDITS_HOLD_SECS   4   // Segundos con el texto completo en pantalla

SceneId showSGDKIntro() {
    clearScene();

    // Fuente arcade + su paleta (blanco con sombreado azul, fondo negro)
    VDP_loadFont(&title_font, DMA);
    PAL_setColors(0, title_font_pal.data, title_font_pal.length, DMA);
    VDP_setTextPalette(PAL0);
    VDP_setBackgroundColor(0);

    // Líneas centradas en las 40 columnas de pantalla: x = (40 - len) / 2
    static const struct { const char* text; u16 x; u16 y; } lines[] = {
        // --- Español ---
        { "ESTE JUEGO FUE CREADO CON SGDK",          5,  4 },
        { "SGDK ES OBRA DE STEPHANE DALLONGEVILLE",  1,  6 },
        { "GRACIAS A NAPALM",                       12,  8 },
        { "POR EL RIPEO DE LOS SPRITES",             6,  9 },
        { "DESARROLLADO POR GUSTAVO VALENZUELA",     2, 11 },
        // --- Separador ---
        { "* * *",                                  17, 14 },
        // --- English ---
        { "THIS GAME WAS MADE WITH SGDK",            6, 17 },
        { "SGDK BY STEPHANE DALLONGEVILLE",          5, 19 },
        { "THANKS TO NAPALM FOR THE SPRITE RIPS",    2, 21 },
        { "DEVELOPED BY GUSTAVO VALENZUELA",         4, 23 },
    };
    const u16 numLines = sizeof(lines) / sizeof(lines[0]);

    // Aparición letra a letra; START saltea y muestra todo de una
    bool skipped = FALSE;
    for (u16 i = 0; i < numLines && !skipped; i++)
        skipped = drawTextTypewriter(lines[i].text, lines[i].x, lines[i].y, CREDITS_CHAR_DELAY);

    if (skipped)
        for (u16 i = 0; i < numLines; i++)
            VDP_drawText(lines[i].text, lines[i].x, lines[i].y);

    // Si salteó con START, esperar a que lo suelte para que el mismo press
    // no corte también la pausa final
    while (JOY_readJoypad(JOY_1) & BUTTON_START)
        SYS_doVBlankProcess();

    // Mantener el texto completo en pantalla (START corta)
    u16 timer = (IS_PAL_SYSTEM ? 50 : 60) * CREDITS_HOLD_SECS;
    while (timer > 0) {
        timer--;
        if (JOY_readJoypad(JOY_1) & BUTTON_START) break;
        SYS_doVBlankProcess();
    }

    // Restaurar la fuente por defecto de SGDK para el resto del juego
    VDP_loadFont(&font_default, DMA);

    clearScene();
    return SCENE_CREDITS;
}

// ---------------------------------------------------------------------------
// Creditos — reconocimiento al adaptador musical
// ---------------------------------------------------------------------------
// Nombre del artista (logo de 200px) centrado con el esqueleto animado al
// lado (bloque de 280px centrado: logo x=20..220, esqueleto x=220..300).
// Texto con la fuente arcade (title_font, ASCII 32..126, sin acentos).
// ---------------------------------------------------------------------------
#define CREDITS_LOGO_X  20
#define CREDITS_LOGO_Y  128
#define CREDITS_SKEL_X  220
#define CREDITS_SKEL_Y  100
#define CREDITS_HOLD_SECS  4

SceneId showCredits() {
    clearScene();

    // Fuente arcade + su paleta (blanco con sombreado azul, fondo negro)
    VDP_loadFont(&title_font, DMA);
    PAL_setColors(0, title_font_pal.data, title_font_pal.length, DMA);
    VDP_setTextPalette(PAL0);
    VDP_setBackgroundColor(0);

    // Nombre del artista (logo) y esqueleto animado: paletas propias en
    // PAL1/PAL2 (PAL0 queda para el texto).
    Sprite* logo = SPR_addSprite(&sansenpai_logo, CREDITS_LOGO_X, CREDITS_LOGO_Y, TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
    Sprite* skel = SPR_addSprite(&skeleton_music, CREDITS_SKEL_X, CREDITS_SKEL_Y, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (logo) PAL_setPalette(PAL1, sansenpai_logo.palette->data, DMA);
    if (skel) {
        PAL_setPalette(PAL2, skeleton_music.palette->data, DMA);
        SPR_setAnim(skel, 0);
    }

    // Líneas centradas en las 40 columnas: x = (40 - len) / 2
    const char* line1 = "ORIGINAL SOUNDTRACK ADAPTATION";   // 28 chars → x=6
    const char* line2 = "& ARRANGEMENTS BY:";                // 18 chars → x=11

    bool skipped;
    skipped = drawTextTypewriter(line1, 6, 5, TITLE_CHAR_DELAY);
    if (!skipped) skipped = drawTextTypewriter(line2, 11, 7, TITLE_CHAR_DELAY);

    if (skipped) {
        VDP_drawText(line1, 6, 5);
        VDP_drawText(line2, 11, 7);
    }

    // Si salteó con START, esperar a que lo suelte para que el mismo press
    // no corte también la pausa final
    while (JOY_readJoypad(JOY_1) & BUTTON_START)
        SYS_doVBlankProcess();

    // Mantener en pantalla (el esqueleto se anima vía SPR_update; START corta)
    u16 timer = (IS_PAL_SYSTEM ? 50 : 60) * CREDITS_HOLD_SECS;
    while (timer > 0) {
        timer--;
        if (JOY_readJoypad(JOY_1) & BUTTON_START) break;
        SPR_update();
        SYS_doVBlankProcess();
    }

    VDP_loadFont(&font_default, DMA);
    clearScene();
    return SCENE_INTRO_ARCADE;
}

SceneId showLevel1Title() {
    clearScene();

    // Esperar a que se suelte START: venimos de confirmar personaje con
    // START y, si sigue apretado, saltearía el título sin querer.
    while (JOY_readJoypad(JOY_1) & BUTTON_START)
        SYS_doVBlankProcess();

    // Cargar la fuente arcade en VRAM (ocupa el lugar de la fuente de SGDK)
    VDP_loadFont(&title_font, DMA);

    // Paleta de la fuente (blanco con sombreado azul; color 0 = negro).
    // PAL_setColors respeta la longitud real de la paleta exportada (4 colores).
    PAL_setColors(0, title_font_pal.data, title_font_pal.length, DMA);
    VDP_setTextPalette(PAL0);
    VDP_setBackgroundColor(0);

    const char* line1 = "SCENE 1";
    const char* line2 = "FIRE! WE GOTTA GET";
    const char* line3 = "APRIL OUT!!";

    // Aparición letra a letra (START saltea la animación)
    bool skipped;
    skipped = drawTextTypewriter(line1, 16, 10, TITLE_CHAR_DELAY);
    if (!skipped) skipped = drawTextTypewriter(line2, 11, 13, TITLE_CHAR_DELAY);
    if (!skipped) skipped = drawTextTypewriter(line3, 14, 15, TITLE_CHAR_DELAY);

    // Si salteó, mostramos el texto completo de una
    if (skipped) {
        VDP_drawText(line1, 16, 10);
        VDP_drawText(line2, 11, 13);
        VDP_drawText(line3, 14, 15);
    }

    // Mantener el texto completo 2 segundos (60 fps NTSC / 50 fps PAL)
    u16 timer = (IS_PAL_SYSTEM ? 50 : 60) * 2;
    while (timer > 0) {
        timer--;
        if (JOY_readJoypad(JOY_1) & BUTTON_START) break;
        SYS_doVBlankProcess();
    }

    // Restaurar la fuente por defecto de SGDK para el resto del juego
    VDP_loadFont(&font_default, DMA);

    clearScene();
    return SCENE_LEVEL1;
}

// ---------------------------------------------------------------------------
// 8. Nivel 1 — fondo scrolleable + fuego en primer plano + jugador
// ---------------------------------------------------------------------------
SceneId showLevel1() {
    clearScene();

    // --- Fondo con STREAMING de columnas (nivel completo de 1376px) ---
    // bgInit carga la paleta + tileset completo a VRAM y dibuja las primeras
    // 64 columnas en el plano circular BG_B. bgUpdate() revela columnas nuevas
    // a medida que la cámara avanza.
    // Mapa de paletas del nivel:
    //   PAL0 → fondo | PAL1 → tortugas | PAL2 → foot soldiers + fuego | PAL3 → foot soldier naranja
    bgInit();
    // clearScene deja BG_A en el plano 32x32 anterior; al agrandarlo a 64x32
    // las columnas 32..63 pueden contener basura de escenas anteriores (por
    // ejemplo el HUD de una intro) que se ve como una franja vertical en el
    // centro del nivel. Limpiamos todo BG_A antes de dibujar el fuego.
    VDP_clearPlane(BG_A, TRUE);

    // --- Fuego en primer plano (BG_A, prioridad alta) ---
    // Los tiles del fuego van a VRAM justo después del tileset del fondo.
    fireInit(TILE_USER_INDEX + bg_level1.tileset->numTile);

    // --- Marcos del HUD (sprites de alto nivel, franja superior de 32px) ---
    // Los marcos ya NO consumen tiles de plano: viven en el area de sprites
    // (SPR_initEx). El primer tile libre de BG es el que sigue al fuego; ahi
    // van los bloques de la barra de vida (8 tiles por jugador).
    hudInit();
    u16 hudVramFree = TILE_USER_INDEX + bg_level1.tileset->numTile + FIRE_CELL_TILES;

    // Estado global de la IA de grupo: contador de atacantes simultáneos y
    // reparto de targets entre los jugadores presentes (1 o 2).
    resetEnemyAI(cantidadJugadores);

    // Shurikens: resetear el sistema de proyectiles del foot soldier naranja.
    shurikenInit();

    // La paleta PAL3 (foot soldier naranja + texto del HUD) la carga
    // levelFadeIn al final del setup.

    // --- Música del nivel (los SFX por PCM siguen activos) ---
    playMusicVol(music_level1, VOL_MUSIC_LEVEL1);

    // --- Inicializar jugador(es) ---
    // Las 4 tortugas comparten la paleta unificada, así que P1 y P2 usan PAL1.
    bool dosJugadores = (cantidadJugadores == 2);

    // Los límites izquierdo/derecho reales se recalculan CADA frame en el
    // paso 3 del bucle (dependen de la cámara); acá solo el arranque.
    Player p1;
    initPlayer(&p1, personajeSeleccionado, JOY_1, PAL1, 40, 182);   // 5 tiles desde el borde izq
    setPlayerRightBound(&p1, SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W);

    Player p2;
    if (dosJugadores) {
        initPlayer(&p2, personaje2Seleccionado, JOY_2, PAL1, 160, 182);
        setPlayerRightBound(&p2, SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W);
    }

    // --- Bola de hierro (obstáculo que cae rebotando) ---
    // Comparte PAL1 (tortugas), ya cargada por initPlayer. Sprite oculto hasta
    // el primer spawn (cada IRON_BALL_PERIOD frames).
    ironBallInit();

    // --- HUD dinámico: barra de vida + vidas + puntaje ---
    // El texto (vidas/puntaje) va con la fuente arcade del HUD (hud_font) sobre
    // BG_A, con prioridad alta (delante de los sprites) y en PAL1 (paleta de
    // las tortugas: la fuente está indexada sobre esa misma paleta). La barra
    // usa PAL1 (ya cargada por initPlayer). Cada jugador tiene su bloque de
    // barra en VRAM: P1 en hudVramFree, P2 a +8.
    VDP_loadFont(&hud_font, DMA);
    VDP_setTextPlane(BG_A);
    VDP_setTextPriority(1);
    VDP_setTextPalette(PAL1);

    HudPlayer hud1;
    hudPlayerInit(&hud1, &p1, HUD_P1_BASECOL, hudVramFree);
    HudPlayer hud2;
    if (dosJugadores)
        hudPlayerInit(&hud2, &p2, HUD_P2_BASECOL, hudVramFree + HPBAR_FRAME_TILES);

    // --- Estado de continues por jugador (cuenta regresiva + selección) ---
    ContPlayer cont1 = { CONT_NONE, 0, 0, 0, 0 };
    ContPlayer cont2 = { CONT_NONE, 0, 0, 0, 0 };

    // --- Definición de spawns por OLEADAS (trigger-based) ---
    // DESACTIVADOS por ahora (a pedido): el nivel sólo tiene el foot soldier de
    // la intro, los de las puertas y los de los ascensores. Todo el sistema de
    // oleadas queda envuelto en #if 0 para reactivarlo/rediseñarlo más adelante.
    // Cada punto del nivel dispara una oleada: varias entradas con el mismo
    // triggerX. side +1 = entra de FRENTE (off-screen derecha), -1 = por la
    // ESPALDA (off-screen izquierda). Las Y son todas distintas dentro de la
    // oleada (separadas ≥24px, más que ENEMY_SEPARATE_Y) para que no vengan
    // en fila india. La X real se calcula al spawnear, relativa a la cámara.
    // Primera oleada: 3 (2 frente + 1 espalda). El resto: 4 (2 y 2).
    // Si una oleada no tiene lugar (tope MAX_ACTIVE_ENEMIES), las entradas
    // quedan pendientes y van entrando a medida que caen los anteriores.
#if 0  // ---- OLEADAS DESACTIVADAS (rediseño pendiente) ----
    static const EnemySpawnDef spawnDefs[] = {
        // Punto 1 — 3 enemigos
        {  400, +1, 158 }, {  400, +1, 186 }, {  400, -1, 172 },
        // Punto 2 — 4 enemigos
        {  550, +1, 152 }, {  550, +1, 180 }, {  550, -1, 164 }, {  550, -1, 190 },
        // Punto 3 — 4 enemigos
        {  700, +1, 160 }, {  700, +1, 188 }, {  700, -1, 154 }, {  700, -1, 178 },
        // Punto 4 — 4 enemigos
        {  850, +1, 150 }, {  850, +1, 176 }, {  850, -1, 162 }, {  850, -1, 190 },
        // Punto 5 — 4 enemigos
        { 1000, +1, 156 }, { 1000, +1, 184 }, { 1000, -1, 150 }, { 1000, -1, 174 },
        // Punto 6 — 4 enemigos
        { 1150, +1, 166 }, { 1150, +1, 190 }, { 1150, -1, 152 }, { 1150, -1, 180 },
    };
    #define LEVEL1_SPAWN_COUNT (sizeof(spawnDefs) / sizeof(spawnDefs[0]))

    // Cada spawn dispara UNA sola vez: un enemigo muerto no reaparece.
    bool spawnUsed[LEVEL1_SPAWN_COUNT];
    for (u16 i = 0; i < LEVEL1_SPAWN_COUNT; i++) spawnUsed[i] = FALSE;
#endif  // ---- fin OLEADAS DESACTIVADAS ----

    Enemy enemies[MAX_ENEMIES];
    for (u16 i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].state = ENEMY_STATE_INACTIVE;
        enemies[i].sprite = NULL;
    }

    // --- Robot del látigo (mini-jefe del final del nivel) ---
    Robot robot;
    robotInit(&robot);

    // --- Puertas: spawn points sobre los huecos "ACA" del fondo ---
    // doorSpr: sprite de la puerta cerrada (se crea/suelta según visibilidad,
    // para no gastar VRAM de sprites con puertas fuera de pantalla).
    // doorArmed: el jugador ya pasó cerca (queda "armada" aunque se aleje).
    // doorTriggered: ya spawneó su foot soldier (no vuelve a disparar).
    static const s16 doorCenterX[LEVEL1_DOOR_COUNT] = { 429, 718, 846 };
    Sprite* doorSpr[LEVEL1_DOOR_COUNT];
    bool    doorArmed[LEVEL1_DOOR_COUNT];
    bool    doorTriggered[LEVEL1_DOOR_COUNT];
    for (u16 d = 0; d < LEVEL1_DOOR_COUNT; d++) {
        doorSpr[d] = NULL; doorArmed[d] = FALSE; doorTriggered[d] = FALSE;
    }

    // --- Sparks: efecto de fuego detrás de cada puerta rompible ---
    Sprite* sparkSpr[LEVEL1_DOOR_COUNT];
    for (u16 d = 0; d < LEVEL1_DOOR_COUNT; d++) sparkSpr[d] = NULL;
    u16 sparksTimer = 0;   // Ticks para la próxima rotación de paleta
    u16 sparksFrame = 0;   // Cuadro actual de la rotación (0..3)

    // --- Sparks 2: efecto decorativo fijo en el mundo ---
    #define SPARKS2_WORLD_X  330
    #define SPARKS2_WORLD_Y  154
    Sprite* sparks2Spr = SPR_addSprite(&sparks_2,
                                       SPARKS2_WORLD_X, SPARKS2_WORLD_Y,
                                       TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (sparks2Spr) SPR_setDepth(sparks2Spr, -SPARKS2_WORLD_Y);

    // --- Ascensores: 2 puertas animadas que se abren JUNTAS ---
    // elevPhase: 0=cerradas (esperando que ambas estén centradas) · 1=abriendo
    // (animación) · 2=remover + spawnear · 3=hecho (no vuelve a disparar).
    static const s16 elevCenterX[LEVEL1_ELEV_COUNT] = { 972, 1100 };
    Sprite* elevSpr[LEVEL1_ELEV_COUNT];
    for (u16 ev = 0; ev < LEVEL1_ELEV_COUNT; ev++) elevSpr[ev] = NULL;
    Sprite* elevSparkSpr[LEVEL1_ELEV_COUNT];
    for (u16 ev = 0; ev < LEVEL1_ELEV_COUNT; ev++) elevSparkSpr[ev] = NULL;
    u8  elevPhase = 0;
    u16 elevTimer = 0;

    s16 cameraX = 0;   // Borde izquierdo de la cámara en coordenadas de mundo
    bgUpdate(0);       // Scroll inicial

    // --- Fade-in desde negro: todo el setup (fondo, fuego, HUD, jugadores) se
    // cargó con la CRAM negra, así que quedó invisible. Acá se revela la escena.
    // PAL1 = paleta unificada de las tortugas (la misma que cargaba initPlayer).
    levelFadeIn(bg_level1.palette->data,
                leo_player.palette->data,
                foot_soldier.palette->data,
                foot_soldier_orange.palette->data);

    // --- Intro scriptada: se dispara YA, apenas arranca el nivel ---
    // Globo + voice over + primer foot soldier, sin esperar nada. El globo va en
    // posición FIJA de pantalla (independiente del jugador y de la cámara); en
    // el bucle solo corre su ciclo por tiempo (fijo → parpadeo → desaparece).
    const u16 fps          = IS_PAL_SYSTEM ? 50 : 60;
    const u16 bubbleSolidF = fps * BUBBLE_SOLID_SECS;    // tiempo fijo en pantalla
    const u16 bubbleBlinkF = (fps * 3) / 4;              // ~0.75s de parpadeo

    // Globo en posición fija (PAL1 = paleta de las tortugas; depth mínimo →
    // siempre delante de sprites y planos).
    Sprite* bubble = SPR_addSprite(&attack_bubble,
                                   BUBBLE_X_TILES * 8, BUBBLE_SCREEN_Y,
                                   TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    if (bubble) SPR_setDepth(bubble, SPR_MIN_DEPTH);

    // Voice over: PCM 13.3 kHz, canal 2, prioridad máxima (15) para que suene
    // aunque la música reserve ese canal PCM.
    XGM2_playPCMEx(attack_vo, sizeof(attack_vo), SOUND_PCM_CH2, 15, FALSE, FALSE);

    // Primer foot soldier: ya spawneado y VISIBLE pegado al borde derecho de la
    // pantalla, entrando (CHASE) hacia el jugador.
    for (u16 i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].state == ENEMY_STATE_INACTIVE) {
            initEnemySpawn(&enemies[i],
                           cameraX + SCREEN_PIXEL_WIDTH - ENEMY_SPRITE_W_PURPLE,
                           180, 60, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
            enemies[i].state = ENEMY_STATE_CHASE;
            break;
        }
    }

    u8  introPhase = 1;   // 1=globo fijo 2=parpadeo 3=terminado
    u16 introTimer = 0;

    bool win = FALSE;     // TRUE al matar al robot y limpiar enemigos (fin del nivel)

    // --- "HURRY UP!": aparece si la camara no avanza durante N segundos ---
    #define HURRY_CAM_STILL_SECS  6
    #define HURRY_X               256
    #define HURRY_Y               40
    Sprite* hurrySpr = NULL;
    u16     hurryTimer = 0;
    s16     hurryLastCamX = -1;
    const u16 hurryStillFrames = fps * HURRY_CAM_STILL_SECS;

    // --- Zonas de combate ---
    s16  cameraLockX = -1;    // -1 = sin bloqueo; >=0 = cameraX no puede superar este valor
    u8   combatZone = 0;      // Zona actual (0-9)

    // --- Bucle principal del nivel ---
    while (1) {
        // 1. Input y física de cada jugador
        updatePlayer(&p1);
        if (dosJugadores) updatePlayer(&p2);

        // 2. Cámara dead-zone: la mueve el jugador que va MÁS ADELANTE
        //    (estilo arcade), pero SIN dejar nunca al rezagado fuera de
        //    pantalla: si el avance lo sacaría por la izquierda, la cámara
        //    se topea hasta que el otro también avance. Beat-em-up clásico:
        //    la cámara solo va a la derecha, nunca retrocede (por eso solo
        //    revelamos columnas nuevas a la derecha).
        s16 leadX  = getPlayerWorldX(&p1);
        s16 trailX = leadX;
        if (dosJugadores) {
            s16 x2 = getPlayerWorldX(&p2);
            if (x2 > leadX)  leadX  = x2;
            if (x2 < trailX) trailX = x2;
        }
        s16 leadScreenX = leadX - cameraX;

        if (leadScreenX > CAM_DEAD_ZONE_RIGHT && cameraX < CAM_MAX_X) {
            s16 newCam = cameraX + (leadScreenX - CAM_DEAD_ZONE_RIGHT);
            if (newCam > CAM_MAX_X) newCam = CAM_MAX_X;
            if (cameraLockX >= 0 && newCam > cameraLockX) newCam = cameraLockX;
            if (dosJugadores) {
                // Tope por el rezagado: su frame nunca pasa el borde izquierdo
                s16 camCap = trailX - CAM_TRAIL_MARGIN;
                if (newCam > camCap) newCam = camCap;
            }
            if (newCam - cameraX > CAM_MAX_SPEED) newCam = cameraX + CAM_MAX_SPEED;
            if (newCam > cameraX) cameraX = newCam;   // nunca retrocede
        }

        // 2b. "HURRY UP!": si la camara NO se movio durante 6 segundos,
        //     mostrar el aviso en la esquina superior derecha (debajo del HUD).
        //     Desaparece en cuanto la camara vuelve a avanzar.
        if (cameraX != hurryLastCamX) {
            hurryTimer = 0;
            if (hurrySpr) { SPR_releaseSprite(hurrySpr); hurrySpr = NULL; }
        } else {
            if (++hurryTimer >= hurryStillFrames) {
                if (!hurrySpr) {
                    hurrySpr = SPR_addSprite(&hurry_sheet, HURRY_X, HURRY_Y,
                                             TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
                    if (hurrySpr) SPR_setDepth(hurrySpr, SPR_MIN_DEPTH);
                }
            }
        }
        hurryLastCamX = cameraX;

        // 3. Notificar a cada jugador la cámara y los bordes de movimiento.
        //    - Izquierdo: nadie sale de pantalla por la izquierda (= cameraX).
        //    - Derecho: nadie sale por la DERECHA (= borde visible). Clave en
        //      2P: cuando la cámara queda topeada por el rezagado, el que va
        //      adelante choca contra el borde de pantalla en vez de seguir
        //      caminando fuera de ella. Al final del nivel coincide con el
        //      límite del mundo (CAM_MAX_X + 320 - 104 = 1376 - 104).
        s16 rightBound = cameraX + SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W;
        setPlayerCamera(&p1, cameraX);
        setPlayerLeftBound(&p1, cameraX);
        setPlayerRightBound(&p1, rightBound);
        if (dosJugadores) {
            setPlayerCamera(&p2, cameraX);
            setPlayerLeftBound(&p2, cameraX);
            setPlayerRightBound(&p2, rightBound);
        }

        // 3b. Intro scriptada: el globo (ya creado y posicionado antes del
        //     bucle) solo cumple su ciclo por tiempo. Posición FIJA → no se
        //     reposiciona; es independiente del jugador y de la cámara.
        if (introPhase == 1) {
            // Globo fijo en pantalla el tiempo definido.
            if (++introTimer >= bubbleSolidF) { introPhase = 2; introTimer = 0; }
        } else if (introPhase == 2) {
            // Parpadeo antes de irse: alterna visibilidad cada
            // BUBBLE_BLINK_TOGGLE frames.
            if (bubble)
                SPR_setVisibility(bubble,
                    ((introTimer / BUBBLE_BLINK_TOGGLE) & 1) ? HIDDEN : VISIBLE);
            if (++introTimer >= bubbleBlinkF) {
                if (bubble) { SPR_releaseSprite(bubble); bubble = NULL; }
                introPhase = 3;   // terminado: no vuelve a entrar
            }
        }

        // 4. Conteo de enemigos activos (para el gating de spawns por puertas).
        //    Los spawns por OLEADAS quedaron DESACTIVADOS (ver #if 0 arriba):
        //    por ahora sólo están el foot soldier de la intro, los de las
        //    puertas y los de los ascensores.
        u16 activeEnemies = 0;
        for (u16 i = 0; i < MAX_ENEMIES; i++)
            if (enemies[i].state != ENEMY_STATE_INACTIVE) activeEnemies++;

        // 4b. Puertas como spawn points. Para cada puerta no disparada:
        //     - muestra su sprite (door_lvl_1) mientras está cerca de pantalla
        //       (fuera de eso lo suelta, para no gastar VRAM de sprites);
        //     - se "arma" cuando el jugador pasa cerca (queda armada aunque se
        //       aleje);
        //     - una vez armada, en cuanto hay cupo de activos spawnea un foot
        //       soldier que ROMPE la puerta, remueve el sprite y no vuelve a
        //       disparar.
        for (u16 d = 0; d < LEVEL1_DOOR_COUNT; d++) {
            s16 screenX = doorCenterX[d] - cameraX;   // centro del hueco en pantalla

            bool nearScreen = (screenX > -DOOR_VIS_MARGIN) &&
                              (screenX < SCREEN_PIXEL_WIDTH + DOOR_VIS_MARGIN);

            if (!doorTriggered[d]) {
                if (nearScreen && !doorSpr[d]) {
                    doorSpr[d] = SPR_addSprite(&door_lvl_1,
                                               screenX - DOOR_HALF_W, DOOR_SPRITE_TOP_Y,
                                               TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
                    if (doorSpr[d]) SPR_setDepth(doorSpr[d], SPR_MAX_DEPTH - 1);
                    // Sparks: fuego detrás de la puerta (misma paleta PAL2, fondo).
                    if (!sparkSpr[d]) {
                        sparkSpr[d] = SPR_addSprite(&sparks,
                                                    screenX - SPARKS_HALF_W, SPARKS_SPRITE_TOP_Y,
                                                    TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
                        if (sparkSpr[d]) SPR_setDepth(sparkSpr[d], SPR_MAX_DEPTH);
                    }
                } else if (!nearScreen && doorSpr[d]) {
                    SPR_releaseSprite(doorSpr[d]);
                    doorSpr[d] = NULL;
                    if (sparkSpr[d]) { SPR_releaseSprite(sparkSpr[d]); sparkSpr[d] = NULL; }
                }
                if (doorSpr[d])
                    SPR_setPosition(doorSpr[d], screenX - DOOR_HALF_W, DOOR_SPRITE_TOP_Y);
            }

            // Spark sigue vivo después del trigger: mantenerlo fijo en el mundo
            // y liberarlo cuando salga de cámara.
            if (sparkSpr[d]) {
                if (nearScreen) {
                    SPR_setPosition(sparkSpr[d], screenX - SPARKS_HALF_W, SPARKS_SPRITE_TOP_Y);
                } else {
                    SPR_releaseSprite(sparkSpr[d]);
                    sparkSpr[d] = NULL;
                }
            }

            // Armar por cercanía del jugador (el más cercano en 2P): centro de
            // la tortuga (frame +52) contra el centro del hueco.
            if (!doorTriggered[d]) {
                s16 pdx = getPlayerWorldX(&p1) + (PLAYER_SPRITE_W / 2) - doorCenterX[d];
                if (pdx < 0) pdx = -pdx;
                if (dosJugadores) {
                    s16 pdx2 = getPlayerWorldX(&p2) + (PLAYER_SPRITE_W / 2) - doorCenterX[d];
                    if (pdx2 < 0) pdx2 = -pdx2;
                    if (pdx2 < pdx) pdx = pdx2;
                }
                if (!doorArmed[d] && pdx < DOOR_TRIGGER_DIST) doorArmed[d] = TRUE;

                // Disparar el spawn cuando esté armada y haya cupo (respeta el tope
                // de foot soldiers simultáneos, igual que las oleadas).
                if (doorArmed[d] && activeEnemies < MAX_ACTIVE_ENEMIES) {
                    for (u16 i = 0; i < MAX_ENEMIES; i++) {
                        if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                            initEnemyDoorSpawn(&enemies[i], doorCenterX[d], PAL2);
                            activeEnemies++;
                            doorTriggered[d] = TRUE;
                            if (doorSpr[d]) { SPR_releaseSprite(doorSpr[d]); doorSpr[d] = NULL; }
                            break;
                        }
                    }
                }
            }
        }

        // 4c. Ascensores: dos puertas que se abren JUNTAS cuando ambas quedan
        //     centradas en la cámara; al terminar la animación se remueven y de
        //     cada hueco sale un foot soldier (BREAK_DOOR frames 3-4).
        //     Sparks de ascensor: fuego fijo en el hueco, se crea con la puerta
        //     y persiste después de que se remueve, se libera al salir de cámara.
        if (elevPhase < 3) {
            // Crear/mantener los sprites de ambas puertas mientras estén cerca
            // de pantalla (frame 0 = cerrada, auto-animación congelada).
            if (elevPhase <= 1) {
                for (u16 ev = 0; ev < LEVEL1_ELEV_COUNT; ev++) {
                    s16 sx = elevCenterX[ev] - cameraX;
                    bool nearScr = (sx > -DOOR_VIS_MARGIN) &&
                                   (sx < SCREEN_PIXEL_WIDTH + DOOR_VIS_MARGIN);
                    if (nearScr && !elevSpr[ev]) {
                        elevSpr[ev] = SPR_addSprite(&ascensor_door,
                                                    sx - ELEV_HALF_W, ELEV_SPRITE_TOP_Y,
                                                    TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
                        if (elevSpr[ev]) {
                            SPR_setDepth(elevSpr[ev], SPR_MAX_DEPTH - 1);
                            SPR_setAutoAnimation(elevSpr[ev], FALSE);
                        }
                        if (!elevSparkSpr[ev]) {
                            elevSparkSpr[ev] = SPR_addSprite(&spark_ascensor,
                                                              sx - ELEV_SPARK_HALF_W, ELEV_SPARK_TOP_Y,
                                                              TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
                            if (elevSparkSpr[ev]) SPR_setDepth(elevSparkSpr[ev], SPR_MAX_DEPTH);
                        }
                    } else if (!nearScr && elevSpr[ev] && elevPhase == 0) {
                        SPR_releaseSprite(elevSpr[ev]); elevSpr[ev] = NULL;
                    }
                    if (elevSpr[ev])
                        SPR_setPosition(elevSpr[ev], sx - ELEV_HALF_W, ELEV_SPRITE_TOP_Y);
                }
            }

            if (elevPhase == 0) {
                // Disparo: AMBOS centros dentro de la banda central de pantalla
                // y ambas puertas presentes (visibles).
                s16 sx0 = elevCenterX[0] - cameraX;
                s16 sx1 = elevCenterX[1] - cameraX;
                bool centered = (sx0 >= ELEV_CENTER_MIN && sx0 <= ELEV_CENTER_MAX) &&
                                (sx1 >= ELEV_CENTER_MIN && sx1 <= ELEV_CENTER_MAX);
                if (centered && elevSpr[0] && elevSpr[1]) {
                    for (u16 ev = 0; ev < LEVEL1_ELEV_COUNT; ev++) {
                        SPR_setAutoAnimation(elevSpr[ev], TRUE);
                        SPR_setAnimationLoop(elevSpr[ev], FALSE);
                        SPR_setAnimAndFrame(elevSpr[ev], 0, 0);   // abrir desde el frame 0
                    }
                    elevTimer = ELEV_DOOR_ANIM_TIME;
                    elevPhase = 1;
                    // Bloquear cámara en posición fija (más adelante que el centering)
                    cameraLockX = ZONE4_ELEV_LOCK;
                }
            } else if (elevPhase == 1) {
                // Esperar a que termine la animación de apertura.
                if (elevTimer > 0) elevTimer--;
                else               elevPhase = 2;
            } else if (elevPhase == 2) {
                // Remover ambas puertas y spawnear un foot soldier de cada hueco.
                for (u16 ev = 0; ev < LEVEL1_ELEV_COUNT; ev++) {
                    if (elevSpr[ev]) { SPR_releaseSprite(elevSpr[ev]); elevSpr[ev] = NULL; }
                    for (u16 i = 0; i < MAX_ENEMIES; i++) {
                        if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                            initEnemyElevatorSpawn(&enemies[i], elevCenterX[ev], PAL2);
                            activeEnemies++;
                            break;
                        }
                    }
                }
                elevPhase = 3;   // hecho: no vuelve a disparar
            }
        }

        // Sparks de ascensor: mantener fijos en el mundo y liberar al salir de cámara.
        for (u16 ev = 0; ev < LEVEL1_ELEV_COUNT; ev++) {
            if (elevSparkSpr[ev]) {
                s16 sx = elevCenterX[ev] - cameraX;
                bool nearScr = (sx > -DOOR_VIS_MARGIN) &&
                               (sx < SCREEN_PIXEL_WIDTH + DOOR_VIS_MARGIN);
                if (nearScr) {
                    SPR_setPosition(elevSparkSpr[ev], sx - ELEV_SPARK_HALF_W, ELEV_SPARK_TOP_Y - 8);
                } else {
                    SPR_releaseSprite(elevSparkSpr[ev]);
                    elevSparkSpr[ev] = NULL;
                }
            }
        }

        // 4d. Zonas de combate: bloqueo de cámara y spawns secuenciales.
        //     combatZone: 0=pre-1, 1=z1 activa, 2=heading z2, 3=z2 activa,
        //     4=heading z3, 5=z3 activa, 6=heading z4, 7=z4 ascensores,
        //     8=z4 delay, 9=z4 robot+naranja.
        {
            s16 camL, camR;
            u16 s;

            // --- Zona 1: cameraX >= 150 → 2 morados kick + 1 naranja kick ---
            if (combatZone == 0 && cameraX >= ZONE1_CAM_LOCK) {
                combatZone = 1;
                cameraLockX = ZONE1_CAM_LOCK;
                camL = cameraLockX;
                camR = cameraLockX + SCREEN_PIXEL_WIDTH;
                for (s = 0; s < 3; s++) {
                    for (u16 i = 0; i < MAX_ENEMIES; i++) {
                        if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                            if (s == 0)
                                initEnemySomersaultSpawn(&enemies[i], camL - ENEMY_SPRITE_W_PURPLE, 160,
                                                         1, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                            else if (s == 1)
                                initEnemyKickSpawn(&enemies[i], camR, 160,
                                                   -1, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                            else
                                initEnemyKickSpawn(&enemies[i], camR, 166,
                                                   -1, PAL3, ENEMY_TYPE_FOOT_SOLDIER_ORANGE);
                            activeEnemies++;
                            break;
                        }
                    }
                }
            }
            if (combatZone == 1 && activeEnemies == 0) {
                cameraLockX = -1;
                combatZone = 2;
            }

            // --- Zona 2: cameraX >= 300 → 2 morados walk (izq Y=145, der Y=160) ---
            if (combatZone == 2 && cameraX >= ZONE2_CAM_LOCK) {
                cameraLockX = ZONE2_CAM_LOCK;
                combatZone = 3;
                camL = cameraLockX;
                camR = cameraLockX + SCREEN_PIXEL_WIDTH;
                for (s = 0; s < 2; s++) {
                    for (u16 i = 0; i < MAX_ENEMIES; i++) {
                        if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                            if (s == 0) {
                                // Voltereta desde la espalda: entra haciendo la
                                // voltereta y recién al terminar pasa a CHASE.
                                initEnemySomersaultSpawn(&enemies[i], camL - ENEMY_SPRITE_W_PURPLE, 145,
                                                         1, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                            } else {
                                initEnemySpawn(&enemies[i], camR, 160,
                                               0, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                                enemies[i].dir = -1;
                                enemies[i].state = ENEMY_STATE_CHASE;
                            }
                            activeEnemies++;
                            break;
                        }
                    }
                }
            }
            if (combatZone == 3 && activeEnemies == 0) {
                cameraLockX = -1;
                combatZone = 4;
            }

            // --- Zona 3: cameraX >= 614 → 1 morado walk izq Y=162 + 1 naranja walk der Y=150 ---
            if (combatZone == 4 && cameraX >= ZONE3_CAM_LOCK) {
                cameraLockX = ZONE3_CAM_LOCK;
                combatZone = 5;
                camL = cameraLockX;
                camR = cameraLockX + SCREEN_PIXEL_WIDTH;
                for (s = 0; s < 2; s++) {
                    for (u16 i = 0; i < MAX_ENEMIES; i++) {
                        if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                            if (s == 0) {
                                // Voltereta desde la espalda (entra del lado
                                // izquierdo de la cámara).
                                initEnemySomersaultSpawn(&enemies[i], camL - ENEMY_SPRITE_W_PURPLE, 162,
                                                         1, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                            } else {
                                initEnemySpawn(&enemies[i], camR, 150,
                                               0, PAL3, ENEMY_TYPE_FOOT_SOLDIER_ORANGE);
                                enemies[i].dir = -1;
                                enemies[i].state = ENEMY_STATE_CHASE;
                            }
                            activeEnemies++;
                            break;
                        }
                    }
                }
            }
            if (combatZone == 5 && activeEnemies == 0) {
                cameraLockX = -1;
                combatZone = 6;
            }

            // --- Zona 4 (ascensores): esperar centering → clear → desbloquear ---
            if (combatZone == 6 && elevPhase >= 1) {
                // Ascensores dispararon por centering y bloquearon cámara
                combatZone = 7;
            }
            if (combatZone == 7 && elevPhase >= 2 && activeEnemies == 0) {
                // Ascensores limpiados → desbloquear cámara, permitir avanzar
                cameraLockX = -1;
                combatZone = 8;
            }

            // --- Zona 5 (robot): cameraX alcanza CAM_MAX_X → lock + spawn ---
            if (combatZone == 8 && cameraX >= CAM_MAX_X) {
                cameraLockX = ZONE5_ROBOT_LOCK;
                combatZone = 9;
                // Spawn 1 naranja walk desde la izquierda Y=160
                for (u16 i = 0; i < MAX_ENEMIES; i++) {
                    if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                        initEnemySpawn(&enemies[i],
                                       cameraLockX - ENEMY_SPRITE_W_ORANGE, 160,
                                       0, PAL3, ENEMY_TYPE_FOOT_SOLDIER_ORANGE);
                        enemies[i].dir = 1;
                        enemies[i].state = ENEMY_STATE_CHASE;
                        activeEnemies++;
                        break;
                    }
                }
                // Robot
                if (robot.state == ROBOT_INACTIVE)
                    robotSpawn(&robot, ROBOT_SPAWN_CENTER);
            }
            // Zona 9: esperar robot muerto + sin enemigos → victoria (check en sección 6e)
        }

        // 5. Actualizar enemigos con IA.
        // 5. Actualizar enemigos con IA. updateEnemy recibe los Player* (los
        //    usa el agarre por la espalda del morado); la separación va primero.
        separateEnemies(enemies, MAX_ENEMIES);

        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].state == ENEMY_STATE_INACTIVE) continue;
            setEnemyCamera(&enemies[i], cameraX);
            updateEnemy(&enemies[i], &p1, &p2, dosJugadores);
        }

        // 5c. Robot del látigo (mini-jefe): spawneado por zona 4 (combatZone == 9).
        //     Solo update: la máquina de estados corre por su cuenta.
        robotUpdate(&robot, cameraX, &p1, dosJugadores ? &p2 : NULL, dosJugadores, fps);

        // 5d. Shurikens: actualizar posición, auto-destrucción off-screen.
        shurikenUpdate(cameraX);

        // 6. Colisiones: ataque del jugador → enemigos.
        //    playerAttackHits mide desde el CENTRO de la tortuga, con alcance
        //    frontal real (64px, más que el rango de ataque del foot soldier)
        //    y tolerancia simétrica en profundidad. Incluye la patada en
        //    salto, que antes no golpeaba.
        //    El ESPECIAL (botón A o B+C) mata al foot soldier de un golpe:
        //    aplica ENEMY_HP de daño en vez de 1.
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (!enemyCanBeHit(&enemies[i])) continue;

            s16 ex = getEnemyCenterX(&enemies[i]);
            s16 ey = getEnemyCenterY(&enemies[i]);

            s16     dmg      = 0;
            Player* attacker = NULL;
            if (playerAttackHits(&p1, ex, ey)) {
                dmg = isPlayerSpecialAttack(&p1) ? ENEMY_HP : 1; attacker = &p1;
            } else if (dosJugadores && playerAttackHits(&p2, ex, ey)) {
                dmg = isPlayerSpecialAttack(&p2) ? ENEMY_HP : 1; attacker = &p2;
            }

            if (dmg > 0) {
                damageEnemy(&enemies[i], dmg);
                // Mismo golpe seco de los foot soldiers, pero cuando el impacto
                // vino de la patada con salto de la tortuga. Los i-frames del
                // enemigo (ENEMY_INVINCIBLE) evitan que se repita cada frame.
                if (attacker && isPlayerJumpKicking(attacker))
                    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                // El enemigo pasó enemyCanBeHit (estaba vivo). Si este golpe lo
                // dejó en DEAD, es una baja: +1 punto al jugador que lo remató.
                if (attacker && enemies[i].state == ENEMY_STATE_DEAD) {
                    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode), SOUND_PCM_CH3, 15, FALSE, FALSE);
                    addPlayerScore(attacker, 1);
                }
            }
        }

        // 6-robot. Ataque del jugador → robot del látigo. Golpe normal −1,
        //          especial −ROBOT_SPECIAL_DMG. Los ataques del robot al jugador
        //          (láser/agarre) se resuelven dentro de robotUpdate.
        if (robotCanBeHit(&robot)) {
            s16 rx = robotGetCenterX(&robot);
            s16 ry = robotGetCenterY(&robot);
            s16     rdmg = 0;
            Player* ratt = NULL;
            if (playerAttackHits(&p1, rx, ry)) {
                rdmg = isPlayerSpecialAttack(&p1) ? ROBOT_SPECIAL_DMG : 1; ratt = &p1;
            } else if (dosJugadores && playerAttackHits(&p2, rx, ry)) {
                rdmg = isPlayerSpecialAttack(&p2) ? ROBOT_SPECIAL_DMG : 1; ratt = &p2;
            }
            if (rdmg > 0) {
                robotDamage(&robot, rdmg);
                // Impacto de la patada con salto también contra el mini-jefe
                if (ratt && isPlayerJumpKicking(ratt))
                    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                if (ratt && robot.state == ROBOT_DEAD)
                    addPlayerScore(ratt, 5);   // baja del mini-jefe
            }
        }

        // 6b. Colisiones: ataques de los foot soldiers → jugadores.
        //     enemyTryHitPlayer marca el swing como usado (un golpe por
        //     ataque) y damagePlayer se encarga de la anim de hit correcta
        //     (frente/espalda), el knockback y los i-frames. Se chequea
        //     playerCanBeHit ANTES para no gastar el golpe contra un
        //     jugador invulnerable o en el aire.
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            Enemy* e = &enemies[i];
            if (e->state != ENEMY_STATE_ATTACK) continue;

            if (playerCanBeHit(&p1) &&
                enemyTryHitPlayer(e, getPlayerWorldX(&p1), getPlayerY(&p1))) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(&p1, getEnemyCenterX(e));
            } else if (dosJugadores && playerCanBeHit(&p2) &&
                       enemyTryHitPlayer(e, getPlayerWorldX(&p2), getPlayerY(&p2))) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(&p2, getEnemyCenterX(e));
            }
        }

        // 6b-bis. Shurikens → ataque del jugador: la hitbox del golpe ROMPE los
        //     shurikens que cruza (desaparecen sin dañar). Va ANTES de la
        //     colisión proyectil→jugador: un shuriken roto este frame no pega.
        {
            if (shurikenBreakByPlayerAttack(&p1)) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
            }
            if (dosJugadores && shurikenBreakByPlayerAttack(&p2)) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
            }
        }

        // 6b-bis. Shurikens → jugadores: colisión proyectil.
        {
            s16 hitX = 0;
            if (playerCanBeHit(&p1) &&
                shurikenCheckHitPlayer(getPlayerWorldX(&p1), getPlayerY(&p1), &hitX)) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(&p1, hitX);
            }
            if (dosJugadores && playerCanBeHit(&p2) &&
                shurikenCheckHitPlayer(getPlayerWorldX(&p2), getPlayerY(&p2), &hitX)) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(&p2, hitX);
            }
        }

        // 6b-bis. Bola de hierro: spawn periódico, física de rebote y
        //     colisiones (resta 1 barra al jugador; aplasta foot soldiers).
        //     Va ANTES del refresco del HUD para que el daño se vea el
        //     mismo frame, y antes del game over para que un golpe fatal cuente.
        ironBallUpdate(cameraX, &p1, &p2, dosJugadores, enemies, MAX_ENEMIES);

        // 6c. HUD: refrescar barra de vida, vidas y puntaje (solo redibuja lo
        //     que cambió respecto del frame anterior).
        hudPlayerUpdate(&hud1);
        if (dosJugadores) hudPlayerUpdate(&hud2);

        // 6d. Continues: si un jugador cayó sin vidas, su marco muestra
        //     "CONTINUE?" con cuenta regresiva (START = seguir con tortuga
        //     nueva). Queda fuera solo cuando la cuenta llega a 0; en 2P el
        //     compañero vivo sigue jugando mientras tanto. El nivel termina
        //     cuando TODOS los jugadores quedaron fuera.
        bool out1 = continuePoll(&cont1, &p1, &hud1, hudSprite1, portraitSpr1,
                                 JOY_1, &personajeSeleccionado,
                                 dosJugadores ? personaje2Seleccionado : 0xFF, fps);
        bool out2 = FALSE;
        if (dosJugadores)
            out2 = continuePoll(&cont2, &p2, &hud2, hudSprite2, portraitSpr2,
                                JOY_2, &personaje2Seleccionado,
                                personajeSeleccionado, fps);
        if (out1 && (!dosJugadores || out2))
            break;

        // 6e. Victoria: robot destruido y sin enemigos en pantalla -> arranca la
        //     secuencia de salida (ver después del bucle).
        if (robot.state == ROBOT_GONE && activeEnemies == 0) {
            XGM2_playPCMEx(scream_april, sizeof(scream_april), SOUND_PCM_CH2, 15, FALSE, FALSE);
            win = TRUE;
            break;
        }

        // 7. Revelar columnas nuevas del fondo y aplicar el scroll
        bgUpdate(cameraX);

        // 8. Animar el fuego del primer plano + su scroll de parallax (deriva
        //    con la cámara a FIRE_SCROLL_NUM/DEN de la velocidad del fondo)
        fireUpdate(cameraX);

        // 8b. Sparks: rotación de paleta constante durante el nivel.
        //     Escribe directamente en CRAM (PAL2, índices 5-8) cada SPARKS_PAL_SPEED ticks.
        {
            if (++sparksTimer >= SPARKS_PAL_SPEED) {
                sparksTimer = 0;
                sparksFrame = (sparksFrame + 1) % SPARKS_PAL_FRAME_COUNT;
                // PAL2 empieza en CRAM index 32; +5 = index 37
                PAL_setColors(32 + SPARKS_PAL_IDX_START,
                              sparksPalAnim[sparksFrame],
                              SPARKS_PAL_IDX_COUNT, DMA);
            }
        }

        // 8c. Sparks 2: reposicionar sprite decorativo fijo en el mundo.
        if (sparks2Spr)
            SPR_setPosition(sparks2Spr, SPARKS2_WORLD_X - cameraX, SPARKS2_WORLD_Y);

        SPR_update();
        SYS_doVBlankProcess();
    }

    // La bola no debe seguir viva en la cutscene de victoria (evita que quede
    // congelada en pantalla o golpee durante el paseo scripteado del outro).
    ironBallEnd();

    // Liberar shurikens activos (evita que queden volando en la cutscene).
    shurikenReleaseAll();

    // Liberar sparks que pudieran quedar visibles.
    for (u16 d = 0; d < LEVEL1_DOOR_COUNT; d++) {
        if (sparkSpr[d]) { SPR_releaseSprite(sparkSpr[d]); sparkSpr[d] = NULL; }
    }
    for (u16 ev = 0; ev < LEVEL1_ELEV_COUNT; ev++) {
        if (elevSparkSpr[ev]) { SPR_releaseSprite(elevSparkSpr[ev]); elevSparkSpr[ev] = NULL; }
    }
    if (sparks2Spr) { SPR_releaseSprite(sparks2Spr); sparks2Spr = NULL; }
    if (hurrySpr)   { SPR_releaseSprite(hurrySpr);   hurrySpr   = NULL; }

    // Restaurar atributos de texto por defecto para el resto de las escenas
    // (el HUD los dejó en prioridad alta / PAL3).
    VDP_setTextPriority(0);
    VDP_setTextPalette(PAL0);

    // ---------------------------------------------------------------------
    // SECUENCIA DE SALIDA (victoria): la tortuga queda quieta un momento y
    // luego camina SOLA (sin control) hacia la puerta del muro del final.
    // El fondo y el fuego siguen animando; al llegar, fundido a negro y a la
    // cutscene final.
    // ---------------------------------------------------------------------
    if (win) {
        // Victoria: guardar vidas/puntaje para que persistan al nivel 2.
        playerPersistSave(&p1);
        if (dosJugadores) playerPersistSave(&p2);

        setPlayerCamera(&p1, cameraX);
        if (dosJugadores) setPlayerCamera(&p2, cameraX);

        // 1) Quieto OUTRO_STAND_SECS segundos
        u16 standT = fps * OUTRO_STAND_SECS;
        while (standT-- > 0) {
            playerCutsceneStand(&p1);
            if (dosJugadores) playerCutsceneStand(&p2);
            bgUpdate(cameraX);
            fireUpdate(cameraX);
            SPR_update();
            SYS_doVBlankProcess();
        }

        // 2) Camina hacia la puerta (P2 un poco por detrás para no encimarse)
        bool walking = TRUE;
        while (walking) {
            bool a1 = playerCutsceneWalkTo(&p1, OUTRO_DOOR_X, OUTRO_DOOR_Y);
            bool a2 = TRUE;
            if (dosJugadores)
                a2 = playerCutsceneWalkTo(&p2, OUTRO_DOOR_X - PLAYER_SPRITE_W, OUTRO_DOOR_Y);
            walking = !(a1 && a2);
            bgUpdate(cameraX);
            fireUpdate(cameraX);
            SPR_update();
            SYS_doVBlankProcess();
        }

        // 3) Fundido y al nivel 2 (pasillo en llamas, 2da parte)
        PAL_fadeOutAll(30, FALSE);
        while (PAL_isDoingFade()) SYS_doVBlankProcess();

        clearScene();
        return SCENE_LEVEL2;
    }

    clearScene();
    return SCENE_GAME_OVER;
}

// ===========================================================================
// Nivel 2 — pasillo en llamas, 2da parte (sala cerrada)
// ===========================================================================
// bg_test (440x192) es MÁS angosto que el nivel 1 pero sigue sin entrar en
// pantalla (440 > 320). La diferencia: sus 55 columnas sí caben en el plano
// circular de 64 tiles, así que se dibuja COMPLETO una sola vez y solo se
// scrollea (sin streaming de columnas). La cámara es BIDIRECCIONAL (la sala
// se recorre de ida y vuelta) y topeada en 0..LEVEL2_CAM_MAX_X.
//
// El humo (smoke_lvl1, tira vertical de 8 frames de 64x64) se anima por
// STREAMING igual que el fuego, pero en una banda de 64px justo debajo del
// HUD (filas 4-11) y con PRIORIDAD BAJA: queda DETRÁS de los sprites.
// Comparte la paleta de las tortugas (PAL1).
// ---------------------------------------------------------------------------
#define LEVEL2_PIXEL_WIDTH   440
#define LEVEL2_CAM_MAX_X     (LEVEL2_PIXEL_WIDTH - SCREEN_PIXEL_WIDTH)  // 120
// El fondo (192px = 24 filas) es más bajo que la pantalla (224px = 28 filas).
// Se dibuja PEGADO AL BORDE INFERIOR (filas 4..27); la franja de arriba
// (filas 0-3) queda libre para el HUD y las 4 filas de la base (24-27) las
// tapa el fuego. offset = 28 - 24 = 4.
#define LEVEL2_BG_OFFSET_Y   (SCROLL_TILE_ROWS - (192 / 8))  // 4

#define SMOKE_CELL_TILES_W   8    // Celda de humo: 8 tiles de ancho (64px)
#define SMOKE_CELL_TILES_H   8    // 8 tiles de alto (64px)
#define SMOKE_CELL_TILES     (SMOKE_CELL_TILES_W * SMOKE_CELL_TILES_H)  // 64
#define SMOKE_FRAMES         8    // Frames de animación en smoke_lvl1.png
#define SMOKE_FRAME_INTERVAL 8    // Frames de juego entre cada frame de humo
#define SMOKE_Y_TILE         4    // Banda 64px justo debajo del HUD (filas 0-3)

// La cápsula del taladro (sprite de la fase 2) debe quedar DETRÁS del humo del
// techo. Como la prioridad del plano es global por TILE, solo las columnas del
// plano que cubren la cápsula se pintan con PRIORIDAD ALTA (TRUE): ahí el humo
// se dibuja delante de la cápsula (sprite, prioridad 0). El resto del humo
// sigue con prioridad baja (detrás de los sprites). Con la cámara bloqueada en
// 120 toda la pelea (fase 2), la cápsula anclada a pantalla (172..268, ±8 de
// temblor) cubre las columnas del plano (screen + 120) / 8 = 35..49.
#define SMOKE_FRONT_COL_MIN   35
#define SMOKE_FRONT_COL_MAX   49

static u16 smokeVramInd;   // Primer tile de VRAM de la celda del humo
static u16 smokeFrame;     // Frame de animación actual (0..7)
static u16 smokeTimer;     // Contador hasta el próximo paso
static s16 smokeScrollTbl[SMOKE_CELL_TILES_H];  // H-scroll de las 8 filas del humo

// Fondo del nivel 2: paleta, tileset a VRAM y TODAS las columnas al plano.
// A diferencia de bgInit (streaming), acá no hay columnas por revelar: el mapa
// (55x24) entra en el plano circular de 64. Los índices del tilemap NO son
// secuenciales, así que se copia tile por tile. La imagen (24 filas) se dibuja
// PEGADA AL BORDE INFERIOR: las filas del mapa van a destRow = r + LEVEL2_BG_OFFSET_Y.
static void bgInit2(void) {
    VDP_setPlaneSize(BG_PLANE_W, 32, TRUE);   // plano circular 64x32

    // La paleta PAL0 la carga levelFadeIn al final del setup.
    VDP_loadTileSet(bg_test.tileset, TILE_USER_INDEX, DMA);

    u16 attrBase = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX);
    u16 w = bg_test.tilemap->w;
    u16 h = bg_test.tilemap->h;
    const u16* map = bg_test.tilemap->tilemap;
    for (u16 c = 0; c < w; c++)
        for (u16 r = 0; r < h; r++)
            VDP_setTileMapXY(BG_B, attrBase + map[r * w + c], c, r + LEVEL2_BG_OFFSET_Y);
}

// Scroll del fondo del nivel 2: solo alimenta la tabla H-scroll de BG_B (el
// mapa ya está completo en el plano, no hay columnas nuevas que revelar).
static void bgUpdate2(s16 cameraX) {
    for (u16 i = 0; i < SCROLL_TILE_ROWS; i++) bgScrollTbl[i] = -cameraX;
    VDP_setHorizontalScrollTile(BG_B, 0, bgScrollTbl, SCROLL_TILE_ROWS, DMA_QUEUE);
}

// Carga el frame 0 del humo y dibuja la celda repetida a lo ancho del plano,
// en la banda superior (debajo del HUD). 'vramInd' es el primer tile libre
// (después del fondo). NO carga paleta: el humo comparte PAL1 (tortugas), ya
// cargada por initPlayer -> llamar DESPUÉS de initPlayer.
static void smokeInit(u16 vramInd) {
    smokeVramInd = vramInd;
    smokeFrame   = 0;
    smokeTimer   = 0;

    // Frame 0 a VRAM (64 tiles)
    VDP_loadTileData(smoke_tiles.tiles, vramInd, SMOKE_CELL_TILES, DMA);

    // Tilemap: celda 8x8 repetida en las 64 columnas del plano, filas 4-11.
    // PRIORIDAD BAJA (FALSE) -> el humo queda DETRÁS de los sprites, salvo en
    // las columnas de la cápsula del taladro (SMOKE_FRONT_COL_MIN..MAX), que se
    // pintan con PRIORIDAD ALTA (TRUE) para que la cápsula salga DETRÁS del humo.
    // Se escribe columna por columna (no por bloque) porque la celda NO es
    // secuencial: el tile (col,row) de la celda está en (col%8) + row*8.
    for (u16 col = 0; col < BG_PLANE_W; col++) {
        bool front = (col >= SMOKE_FRONT_COL_MIN && col <= SMOKE_FRONT_COL_MAX);
        for (u16 r = 0; r < SMOKE_CELL_TILES_H; r++)
            VDP_setTileMapXY(BG_A,
                             TILE_ATTR_FULL(PAL1, front, FALSE, FALSE,
                                            vramInd + (r * SMOKE_CELL_TILES_W) +
                                            (col % SMOKE_CELL_TILES_W)),
                             col, SMOKE_Y_TILE + r);
    }
    // El scroll de la banda arranca en 0: fireInit ya puso TODA la tabla de
    // BG_A en 0, así que no hay que escribirla acá.
}

// Avanza la animación del humo + su scroll de parallax (misma técnica que el
// fuego). Recibe la cámara y se llama una vez por frame en el bucle del nivel.
static void smokeUpdate(s16 cameraX) {
    if (++smokeTimer >= SMOKE_FRAME_INTERVAL) {
        smokeTimer = 0;
        smokeFrame = (smokeFrame + 1) & (SMOKE_FRAMES - 1);
        // Pisar los MISMOS 64 tiles de VRAM con el frame siguiente. El frame N
        // arranca en tiles + N*64*8 longwords. DMA_QUEUE: transferencia en el
        // próximo vblank.
        VDP_loadTileData(smoke_tiles.tiles + (smokeFrame * SMOKE_CELL_TILES * 8),
                         smokeVramInd, SMOKE_CELL_TILES, DMA_QUEUE);
    }

    // Scroll de parallax de la banda: mismas constantes que el fuego.
    s16 sscroll = (s16)(-(((s32)cameraX * FIRE_SCROLL_NUM) / FIRE_SCROLL_DEN));
    for (u16 i = 0; i < SMOKE_CELL_TILES_H; i++) smokeScrollTbl[i] = sscroll;
    VDP_setHorizontalScrollTile(BG_A, SMOKE_Y_TILE, smokeScrollTbl,
                                SMOKE_CELL_TILES_H, DMA_QUEUE);
}

// ---------------------------------------------------------------------------
// Cápsula del taladro del jefe Rocksteady (fase 2, como SPRITE)
// ---------------------------------------------------------------------------
// La cápsula (taladro_capsula, 96x104) es un SPRITE: índice [0] = 7 frames de
// emergencia con la puerta cerrada (sale del piso mientras tiembla la pantalla)
// e índice [1] = puerta abierta, congelado por el resto de la pelea (Rocksteady
// sale por esa puerta). Se agrega ANCLADA A PANTALLA en (172,51) (= centro
// 220,103, la posición del taladro subida 4 tiles): la cámara queda bloqueada en
// LEVEL2_CAM_MAX_X (120) toda la pelea, así que el centro de mundo es 220+120 =
// 340 = ROCKSTEADY_TALADRO_X. Su base (y 155..159) queda justo encima de la
// banda de fuego (BG_A, prioridad alta, y 160+), así la cápsula parece "salir
// del piso".
// PRIORIDAD BAJA (FALSE) + SPR_MAX_DEPTH → detrás de jugadores/jefe (que usan
// -y). time = 0 en res/level2.res: la animación se controla MANUAL con
// SPR_setAnimAndFrame sincronizada con el temblor, y el frame final queda fijo.
#define CAPSULA_TILE_W        12    // 96px
#define CAPSULA_TILE_H        13    // 104px
#define CAPSULA_CENTER_X      220   // Centro en pantalla (x_mundo 340 - cámara 120)
#define CAPSULA_CENTER_Y      103   // Centro (subido 4 tiles / 32px respecto al arcade)
#define CAPSULA_SCREEN_X      (CAPSULA_CENTER_X - (CAPSULA_TILE_W * 8) / 2)   // 172
#define CAPSULA_SCREEN_Y      (CAPSULA_CENTER_Y - (CAPSULA_TILE_H * 8) / 2)   // 51
#define CAPSULA_FRAMES        7     // Frames del índice [0] (emergencia)
#define CAPSULA_FRAME_TICKS   29    // Frames de juego entre frames (~203 total ≈ duración de drill.wav)
#define CAPSULA_DOOR_FRAMES   4     // Frames del índice [1] (apertura de la puerta)
#define CAPSULA_DOOR_TICKS    14    // Frames de juego entre frames de apertura (~0.23s)
#define CAPSULA_SHAKE_AMP     8     // Amplitud del temblor de pantalla (px)

// Flash de paleta por HP bajo del jefe (efecto "quemado" brillante). Con <= 20
// HP la paleta de Rocksteady alterna entre la normal y una versión quemada cada
// ROCKSTEADY_FLASH_TICKS frames; con <= 10 HP (crítico) alterna cada
// ROCKSTEADY_FLASH_CRIT_TICKS (más rápido). El índice 1 (texto del HUD) queda
// blanco en ambas paletas, así el HUD no parpadea.
#define ROCKSTEADY_FLASH_HP        20
#define ROCKSTEADY_FLASH_CRIT_HP   10
#define ROCKSTEADY_FLASH_TICKS      8
#define ROCKSTEADY_FLASH_CRIT_TICKS 3

// Temblor horizontal determinista para la emergencia del taladro. Devuelve un
// offset en -AMP..+AMP según el tick. Se suma SOLO al scroll (fondo/fuego/humo)
// y a la X de la cápsula (es un sprite, no scrollea sola), no a la cámara de
// juego ni al HUD.
static s16 capsuleShake(u8 tick) {
    return (s16)((((tick * 13) + (tick >> 1)) % (CAPSULA_SHAKE_AMP * 2 + 1))
                 - CAPSULA_SHAKE_AMP);
}

// ---------------------------------------------------------------------------
// Cutscene de victoria — Shredder rapta a April
// ---------------------------------------------------------------------------
// Al morir Rocksteady la tortuga se queda quieta en el frame de "caminar hacia
// arriba" (observando) y Shredder sale de la cápsula del taladro (sprite
// shredder_lvl1, 72x80, paleta PROPIA en PAL3): camina por el lane de April
// (148) hacia la izquierda, la toma por detrás (Rapto: los frames 0-1 incluyen
// a April DENTRO del sprite, por eso se libera el sprite propio de April), y el
// frame 2 (pose de salto) queda CONGELADO mientras Shredder vuela en arco hacia
// la ventana del extremo derecho. Al salir por la ventana → SCENE_ENDING.
// El ancla del sprite es el tope izquierdo: X de MUNDO, Y de pantalla.
//   - Idle [0]: 1 frame | Walk [1]: 6 frames | Rapto [2]: 3 frames.
//   - El arte ya trae la dirección correcta por animación: Walk [1] mira a la
//     IZQUIERDA (hacia April) y el último frame del Rapto mira a la DERECHA
//     (de frente al saltar). NO se aplica SPR_setHFlip en ningún momento.
//   - April está parada en mundo (205, 148); su contenido ocupa 205..269 con
//     centro 237. El centro del cuerpo de Shredder en el frame está en ~+33 de
//     su ancla, así que el ancla de agarre (158) lo centra sobre April; el
//     frame 1 de Rapto muestra la cabeza de April en ~+21 del ancla → queda
//     exactamente sobre la posición que tenía el sprite de April (205+32=237).
//   - El salto es de MUNDO 158→356 (pantalla 96→336, cámara 120): cruza todo el
//     hueco abierto de la derecha (bg_test: cielo abierto en mundo 56..440,
//     y 128..186) y termina en el hueco de la ventana. Y de ancla 68→70
//     (pies 148→150, dentro de la banda del cielo); el ápice del arco sube a
//     ~25 (pies ~105). OJO: pantalla = mundo − cámara (no al revés).
#define SHREDDER_FRAME_W       72    // 9 tiles
#define SHREDDER_FRAME_H       80    // 10 tiles
#define SHREDDER_FEET_Y        (148) // Lane de April: pisa el mismo piso que ella
// 304 = 340 - 36: el ancla es el tope IZQUIERDO, así que para que el CUERPO
// (72px) quede sobre la puerta/cápsula (centro mundo 340, pantalla 220) el
// ancla debe ir 36px (media anchura) a la izquierda del centro.
#define SHREDDER_SPAWN_X       (ROCKSTEADY_TALADRO_X - SHREDDER_FRAME_W / 2)  // 304
#define SHREDDER_GRAB_X        (158) // Ancla detrás de April (ver comentario arriba)
#define SHREDDER_WALK_SPEED    3     // px/frame caminando hacia April
#define SHREDDER_RAPTO_TICKS   12    // Ticks por cada frame 0 y 1 del Rapto
#define SHREDDER_JUMP_FRAMES   54    // Duración del vuelo en arco (frames)
#define SHREDDER_JUMP_X_END    356   // X de mundo al salir (pantalla 336: FUERA por la derecha)
#define SHREDDER_JUMP_Y_END    70    // Y de ancla al salir (pies ~150, en la banda del cielo)
#define SHREDDER_ARC_HEIGHT    24    // Elevación extra del ápice del arco (px)
// Fade a negro del final: arranca cuando el ancla de Shredder llega a 3 tiles
// (24px) del extremo del nivel (440). Con la cámara fija en 120, el sprite está
// entonces casi fuera de pantalla (pantalla 296..368: quedan ~24px visibles) y
// el fundido tapa la carga de SCENE_ENDING.
#define SHREDDER_FADE_START_X  (LEVEL2_PIXEL_WIDTH - 24)   // 416
#define SHREDDER_FADE_FRAMES   30                          // ~0.5s


// ---------------------------------------------------------------------------
// Escena completa del nivel 2. Flujo:
//   fase 0: oleada A (2 morados entrando de frente). Cámara bloqueada en 0:
//           no se sale de la primera pantalla.
//   fase 1: oleada A limpia -> sala libre (ida y vuelta, cámara 0..120).
//   fase 2: al llegar al límite -> cámara bloqueada + oleada B (2 naranjas de
//           frente + 1 morado que entra por la espalda).
//   fase 3: oleada B limpia -> victoria -> cutscene final (Shredder/April).
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Globo de diálogo "SAY YOUR PRAYERS!" del jefe (say_your_prayers, 96x32)
// ---------------------------------------------------------------------------
// Aparece apenas Rocksteady emerge de la cápsula (cámara bloqueada en 120 →
// posición de pantalla FIJA), el mismo tick en que suena say_your_p_sfx.
// Mismo ciclo que el globo "Attack!!" del nivel 1: sólido → parpadeo → se
// suelta. 120+45 = 165 frames encajan dentro de los 170 del taunt
// (ROCKSTEADY_EMERGE_STAND).
// Posición ajustada en emulador: tope a 16px sobre el tope del frame del jefe
// (el globo de 32px solapa ~16px la cabeza) y corrido 16px a la izquierda del
// centro del cuerpo — 4 tiles abajo y 2 a la izquierda del tiro original.
#define BOSS_BUBBLE_W        96    // ancho del globo (12 tiles)
#define BOSS_BUBBLE_Y_OFFSET  -16   // tope del globo: tope del jefe - 16px
#define BOSS_BUBBLE_X_OFFSET  -16   // 2 tiles (16px) a la izquierda del CENTRO del cuerpo
#define BOSS_BUBBLE_SOLID_F      120   // ~2s sólido (cubre el inicio del taunt)
#define BOSS_BUBBLE_BLINK_F      45    // ~0.75s de parpadeo antes de irse
#define BOSS_BUBBLE_TOGGLE       4     // frames por semiciclo de parpadeo (~7-8 Hz)

SceneId showLevel2() {
    clearScene();

    // --- Motor de sprites con presupuesto seguro para el nivel 2 ---
    // Los tiles de usuario (fondo 467 + humo 64 + fuego 64 + barra HUD 8, en
    // ese orden desde TILE_USER_INDEX=64) terminan en el tile 666. El área de
    // sprites arranca en TILE_FONT_INDEX(1440) - size: para NO pisar los tiles
    // de usuario (la animación del humo/fuego los reescribe cada 8 frames) el
    // presupuesto debe ser <= 773 (1440 - 667). Con SPR_initEx(768) la región
    // es [672..1439]. Pico real de sprites (maxNumTile por frame, el motor hace
    // streaming): HUD 35 + retrato 16 (x2 jugadores) + tortugas 64 (x2) +
    // soldados ~56 c/u + April 28 + cápsula 106 + Rocksteady 68 ≈ 700 <= 768.
    // Se restaura a 752 al salir de la escena.
    SPR_initEx(768);

    // --- Fondo (sala de 440px): dibujo completo + scroll (sin streaming) ---
    // Mapa de paletas (igual que el nivel 1):
    //   PAL0 → fondo | PAL1 → tortugas + humo | PAL2 → foot soldiers + fuego | PAL3 → foot soldier naranja
    bgInit2();

    // --- Fuego en primer plano (BG_A, prioridad alta) ---
    // VRAM de usuario: fondo, luego humo (64), luego fuego (64), luego barra.
    u16 bgTiles = TILE_USER_INDEX + bg_test.tileset->numTile;
    fireInit(bgTiles + FIRE_CELL_TILES);

    // --- Humo del techo (BG_A, prioridad baja → detrás de los sprites) ---
    // DESPUÉS de fireInit: éste resetea toda la tabla de scroll de BG_A.
    smokeInit(bgTiles);

    // --- Marcos del HUD (sprites de alto nivel, franja superior de 32px) ---
    hudInit();
    u16 hudVramFree = bgTiles + (FIRE_CELL_TILES * 2);

    // --- Estado global de la IA de grupo y proyectiles ---
    resetEnemyAI(cantidadJugadores);
    shurikenInit();

    // --- La paleta PAL3 (foot soldier naranja + texto del HUD) la carga
    //     levelFadeIn al final del setup ---

    // --- Música del nivel (los SFX por PCM siguen activos) ---
    playMusicVol(music_level1, VOL_MUSIC_LEVEL1);

    // --- Inicializar jugador(es) ---
    bool dosJugadores = (cantidadJugadores == 2);

    Player p1;
    initPlayer(&p1, personajeSeleccionado, JOY_1, PAL1, 40, 182);
    setPlayerRightBound(&p1, SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W);

    Player p2;
    if (dosJugadores) {
        initPlayer(&p2, personaje2Seleccionado, JOY_2, PAL1, 160, 182);
        setPlayerRightBound(&p2, SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W);
    }

    // --- April (rehén) atada al fondo de la sala ---
    // Decorativa: usa PAL1 (tortugas, ya cargada). Fondo del mundo (x=205,
    // lane 148) y con DEPTH FIJA por detrás de toda la acción (SPR_setDepth
    // con valor MENOS negativo que el de los jugadores/el jefe, que usan -y):
    // así queda detrás aunque se agregue antes o después que ellos.
    static const s16 APRIL_WORLD_X    = 160;
    static const s16 APRIL_LANE_Y     = 148;
    static const s16 APRIL_FOOT_OFFSET = 58;
    Sprite* aprilSpr = SPR_addSprite(&april, APRIL_WORLD_X /*cámara en 0 al inicio*/,
                                     APRIL_LANE_Y - APRIL_FOOT_OFFSET,
                                     TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
    if (aprilSpr) SPR_setDepth(aprilSpr, -APRIL_LANE_Y + 20);
    u16 aprilTimer = 0;

    // --- HUD dinámico: barra de vida + vidas + puntaje ---
    // Mismo esquema que el nivel 1: texto con hud_font en PAL1 (paleta de las
    // tortugas, ya cargada por initPlayer).
    VDP_loadFont(&hud_font, DMA);
    VDP_setTextPlane(BG_A);
    VDP_setTextPriority(1);
    VDP_setTextPalette(PAL1);

    HudPlayer hud1;
    hudPlayerInit(&hud1, &p1, HUD_P1_BASECOL, hudVramFree);
    HudPlayer hud2;
    if (dosJugadores)
        hudPlayerInit(&hud2, &p2, HUD_P2_BASECOL, hudVramFree + HPBAR_FRAME_TILES);

    // --- Estado de continues por jugador (cuenta regresiva + selección) ---
    ContPlayer cont1 = { CONT_NONE, 0, 0, 0, 0 };
    ContPlayer cont2 = { CONT_NONE, 0, 0, 0, 0 };

    // --- Pool de enemigos ---
    Enemy enemies[MAX_ENEMIES];
    for (u16 i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].state = ENEMY_STATE_INACTIVE;
        enemies[i].sprite = NULL;
    }

    s16 cameraX = 0;   // Borde izquierdo de la cámara en coordenadas de mundo
    bgUpdate2(0);      // Scroll inicial

    // Ritmo de frames (lo usa la cuenta regresiva del continue).
    const u16 fps = IS_PAL_SYSTEM ? 50 : 60;

    // --- Jefe Rocksteady + secuencia de la cápsula del taladro (fase 2) ---
    // bossStage: 0=pausa dramática · 1=la cápsula emerge del piso (índice [0],
    // frames 0..6) mientras tiembla la pantalla y suena drill_sfx · 2=la
    // puerta se abre (índice [1]) + suena capsule_door_sfx (~2.2s) · 3=aparece
    // Rocksteady en la puerta, QUIETO reproduciendo su IDLE + suena
    // say_your_p_sfx (~2.8s) · 99=pelea en curso (esperar victoria). La cápsula
    // queda congelada con la puerta abierta el resto del nivel.
    Rocksteady boss;
    rocksteadyInit(&boss);
    rocksteadyBulletInit();
    u8      bossStage    = 0;
    u8      bossTimer    = 0;
    u8      capsulaFrame = 0;
    bool    bossSpawned  = FALSE;
    Sprite* capsulaSpr   = NULL;
    u16     bossPal[16];      // Paleta normal de Rocksteady (PAL3)
    u16     flashPal[16];     // Versión "quemada" (brillante) para el flash por HP bajo
    u8      bossFlashTick = 0;
    u8      bossFlashOn   = 0;

    // --- Globo de diálogo "SAY YOUR PRAYERS!" (aparece con el jefe, en la
    //     puerta; ciclo sólido → parpadeo → se suelta, igual que el del nivel 1)
    Sprite* sayBubble   = NULL;
    u8      bubblePhase = 0;   // 0=inactivo 1=sólido 2=parpadeo 3=terminado
    u16     bubbleTimer = 0;

    // --- Cutscene de victoria: Shredder rapta a April ---
    // 0 = inactiva · 1 = Shredder aparece en la puerta (Idle) · 2 = camina hacia
    // April · 3 = Rapto frames 0-1 (libera el sprite de April) · 4 = frame 2
    // congelado + vuelo en arco · 5 = salió por la ventana → victoria.
    u8      cutScene    = 0;
    u8      cutTimer    = 0;
    bool    winFade     = FALSE;  // ya se disparó el fade a negro del final
    Sprite* shredderSpr = NULL;
    s16     shredderX   = 0;   // Ancla del sprite de Shredder (X de mundo)
    s16     shredderY   = 0;   // Ancla del sprite de Shredder (Y de pantalla)

    // --- Oleada A: 2 foot soldiers morados entrando de FRENTE ---
    // Ambos caminan (CHASE) hacia el jugador desde el borde derecho, lanes
    // distintas para que no vengan en fila india.
    {
        s16 camR = cameraX + SCREEN_PIXEL_WIDTH;
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                initEnemySpawn(&enemies[i], camR - ENEMY_SPRITE_W_PURPLE, 160,
                               60, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                enemies[i].dir = -1;
                enemies[i].state = ENEMY_STATE_CHASE;
                break;
            }
        }
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                initEnemySpawn(&enemies[i], camR, 186,
                               60, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                enemies[i].dir = -1;
                enemies[i].state = ENEMY_STATE_CHASE;
                break;
            }
        }
    }

    // --- Fade-in desde negro: todo el setup (fondo, fuego, humo, HUD,
    // jugadores, oleada A) se cargó con la CRAM negra, así que quedó
    // invisible. Acá se revela la escena. ---
    levelFadeIn(bg_test.palette->data,
                leo_player.palette->data,
                foot_soldier.palette->data,
                foot_soldier_orange.palette->data);

    // --- Fases del nivel ---
    u8   phase       = 0;    // 0=oleada A · 1=sala libre · 2=oleada B · 3=victoria
    s16  cameraLockX = 0;    // >=0 = cameraX no puede superar este valor
    bool win         = FALSE;

    while (1) {
        // 1. Input y física de cada jugador. Durante la cutscene de victoria la
        // tortuga deja de leer input y se queda congelada en el frame de
        // "caminar hacia arriba" (observando a Shredder llevarse a April).
        if (cutScene > 0) {
            // La tortuga viva se congela observando; la caída (game over) queda
            // tirada como estaba.
            if (!isPlayerGameOver(&p1)) playerCutsceneWatch(&p1);
            if (dosJugadores && !isPlayerGameOver(&p2)) playerCutsceneWatch(&p2);
        } else {
            updatePlayer(&p1);
            if (dosJugadores) updatePlayer(&p2);
        }

        // 2. Cámara con dead-zone BIDIRECCIONAL (la sala se recorre de ida y
        //    vuelta). Derecha: igual que el nivel 1 (capped por cameraLockX).
        //    Izquierda: retrocede cuando el que va adelante queda muy atrás.
        s16 leadX  = getPlayerWorldX(&p1);
        s16 trailX = leadX;
        if (dosJugadores) {
            s16 x2 = getPlayerWorldX(&p2);
            if (x2 > leadX)  leadX  = x2;
            if (x2 < trailX) trailX = x2;
        }
        s16 leadScreenX = leadX - cameraX;

        if (leadScreenX > CAM_DEAD_ZONE_RIGHT && cameraX < LEVEL2_CAM_MAX_X) {
            s16 newCam = cameraX + (leadScreenX - CAM_DEAD_ZONE_RIGHT);
            if (newCam > LEVEL2_CAM_MAX_X) newCam = LEVEL2_CAM_MAX_X;
            if (cameraLockX >= 0 && newCam > cameraLockX) newCam = cameraLockX;
            if (dosJugadores) {
                // Tope por el rezagado: su frame nunca pasa el borde izquierdo
                s16 camCap = trailX - CAM_TRAIL_MARGIN;
                if (newCam > camCap) newCam = camCap;
            }
            if (newCam - cameraX > CAM_MAX_SPEED) newCam = cameraX + CAM_MAX_SPEED;
            if (newCam > cameraX) cameraX = newCam;   // nunca retrocede
        } else if (leadScreenX < CAM_DEAD_ZONE_LEFT && cameraX > 0 && phase != 2) {
            // Retroceder en la sala (prohibido durante la pelea contra el jefe:
            // el taladro en BG_A está anclado a la pantalla y se alinearía mal).
            s16 newCam = cameraX - (CAM_DEAD_ZONE_LEFT - leadScreenX);
            if (newCam < 0) newCam = 0;
            if (newCam < cameraX) cameraX = newCam;   // retroceder en la sala
        }

        // 3. Notificar a cada jugador la cámara y los bordes de movimiento.
        s16 rightBound = cameraX + SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W;
        setPlayerCamera(&p1, cameraX);
        setPlayerLeftBound(&p1, cameraX);
        setPlayerRightBound(&p1, rightBound);
        if (dosJugadores) {
            setPlayerCamera(&p2, cameraX);
            setPlayerLeftBound(&p2, cameraX);
            setPlayerRightBound(&p2, rightBound);
        }

        // 4. Conteo de enemigos activos.
        u16 activeEnemies = 0;
        for (u16 i = 0; i < MAX_ENEMIES; i++)
            if (enemies[i].state != ENEMY_STATE_INACTIVE) activeEnemies++;

        // 4b. Fases / oleadas.
        if (phase == 0 && activeEnemies == 0) {
            // Oleada A limpia: desbloquear la cámara (sala libre, ida y vuelta)
            cameraLockX = -1;
            phase = 1;
        } else if (phase == 1 && cameraX >= LEVEL2_CAM_MAX_X) {
            // Llegó al límite de la sala: cámara bloqueada y comienza la pelea
            // contra Rocksteady. La cápsula del taladro aparece en pantalla
            // (oculta hasta el stage 1, cuando empieza a emerger del piso).
            cameraLockX = LEVEL2_CAM_MAX_X;
            phase = 2;
            bossStage = 0;
            bossTimer = 0;
            capsulaSpr = SPR_addSprite(&taladro_capsula, CAPSULA_SCREEN_X,
                                       CAPSULA_SCREEN_Y,
                                       TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
            if (capsulaSpr) {
                SPR_setDepth(capsulaSpr, SPR_MAX_DEPTH);
                SPR_setVisibility(capsulaSpr, HIDDEN);
            }
        } else if (cutScene == 0 && phase == 2 && bossSpawned &&
                   boss.state == ROCKSTEADY_GONE) {
            // Jefe muerto (el sprite ya se liberó solo al terminar la anim de
            // muerte): arranca la cutscene de victoria. Aplica aunque el jefe
            // muriera durante la introducción (bossStage < 99): se fuerza
            // bossStage = 99 para abortar el taunt y no esperar a que termine.
            // Se desactiva el flash de paleta para que nada pise la paleta de
            // Shredder en PAL3.
            bossFlashOn = 0;
            bossStage = 99;
            cutScene = 1;
            cutTimer = 0;
            // Si el jefe murió durante el taunt, suelta el globo de diálogo
            // que aún estuviera vivo (no debe verse en la cutscene).
            if (sayBubble) { SPR_releaseSprite(sayBubble); sayBubble = NULL; }
            bubblePhase = 3;
        }

        // 4c. Secuencia de introducción del jefe (fase 2, stages 0..3).
        if (phase == 2 && bossStage < 99) {
            switch (bossStage) {
                case 0:   // pausa dramática con la cápsula oculta
                    if (++bossTimer >= 30) { bossStage = 1; bossTimer = 0; }
                    break;
                case 1:   // la cápsula emerge del piso (frames 0..6) + temblor
                    if (bossTimer == 0 && capsulaSpr) {
                        SPR_setVisibility(capsulaSpr, VISIBLE);
                        SPR_setAnimAndFrame(capsulaSpr, 0, 0);
                        XGM2_stop();   // cortar la música de fondo durante la secuencia
                        XGM2_playPCMEx(drill_sfx, sizeof(drill_sfx),
                                       SOUND_PCM_CH2, 15, FALSE, FALSE);
                    }
                    if (++bossTimer >= CAPSULA_FRAME_TICKS) {
                        bossTimer = 0;
                        if (capsulaFrame < CAPSULA_FRAMES - 1) {
                            capsulaFrame++;
                            if (capsulaSpr)
                                SPR_setAnimAndFrame(capsulaSpr, 0, capsulaFrame);
                        } else {
                            bossStage = 2; bossTimer = 0;
                        }
                    }
                    break;
                case 2:   // la puerta se abre (índice [1], CAPSULA_DOOR_FRAMES frames) + suena capsule_door
                    if (bossTimer == 0 && capsulaSpr) {
                        SPR_setAnimAndFrame(capsulaSpr, 1, 0);
                        XGM2_playPCMEx(capsule_door_sfx, sizeof(capsule_door_sfx),
                                       SOUND_PCM_CH2, 15, FALSE, FALSE);
                    }
                    // Avanza un frame cada CAPSULA_DOOR_TICKS y queda FIJO en el
                    // último (CAPSULA_DOOR_FRAMES-1) hasta el final de la secuencia
                    // (y de la pelea: nada más toca la cápsula después).
                    if (bossTimer > 0 && (bossTimer % CAPSULA_DOOR_TICKS) == 0 && capsulaSpr) {
                        u16 doorFrame = bossTimer / CAPSULA_DOOR_TICKS;
                        if (doorFrame >= CAPSULA_DOOR_FRAMES) doorFrame = CAPSULA_DOOR_FRAMES - 1;
                        SPR_setAnimAndFrame(capsulaSpr, 1, doorFrame);
                    }
                    // Espera a que suene el arranque de la puerta (~1.2s, ya
                    // reducido 1s respecto a la duración completa del wav).
                    if (++bossTimer >= 70) { bossStage = 3; bossTimer = 0; }
                    break;
                case 3:   // Rocksteady aparece en la puerta, quieto (IDLE) + say_your_p
                    if (bossTimer == 0) {
                        if (!bossSpawned) {
                            bossSpawned = TRUE;
                            // Paleta del jefe en PAL3 (índice 1 blanco: HUD).
                            PAL_setPalette(PAL3, rocksteady_boss.palette->data, DMA);
                            PAL_setColor(PAL3 * 16 + 1, 0x0EEE);
                            // Buffers del flash: normal + versión "quemada"
                            // (cada canal RGB duplicado, clampeado a 0xF). El
                            // índice 0 se mantiene transparente y el 1 blanco.
                            for (u16 ci = 0; ci < 16; ci++) {
                                u16 c = rocksteady_boss.palette->data[ci];
                                bossPal[ci] = (ci == 1) ? 0x0EEE : c;
                                if (ci == 0 || ci == 1) flashPal[ci] = bossPal[ci];
                                else {
                                    u16 r = (c >> 8)  & 0xF, g = (c >> 4) & 0xF, b = c & 0xF;
                                    r = (r << 1) | (r >> 3);  if (r > 0xF) r = 0xF;
                                    g = (g << 1) | (g >> 3);  if (g > 0xF) g = 0xF;
                                    b = (b << 1) | (b >> 3);  if (b > 0xF) b = 0xF;
                                    flashPal[ci] = (r << 8) | (g << 4) | b;
                                }
                            }
                            rocksteadySpawn(&boss);   // se queda parado en la puerta (IDLE)
                        }
                        XGM2_playPCMEx(say_your_p_sfx, sizeof(say_your_p_sfx),
                                       SOUND_PCM_CH2, 15, FALSE, FALSE);

                        // Globo de diálogo justo cuando el jefe aparece (mismo
                        // tick que el wav). Posición FIJA de pantalla: tope a
                        // BOSS_BUBBLE_Y_OFFSET del tope del frame del jefe
                        // (cámara bloqueada en 120 → ancla del jefe x-cam = 156,
                        // tope y = 52) y BOSS_BUBBLE_X_OFFSET del centro del cuerpo.
                        if (!sayBubble) {
                            s16 bossTopY = boss.y - ROCKSTEADY_FOOT_OFFSET;
                            s16 bubbleY  = bossTopY + BOSS_BUBBLE_Y_OFFSET;
                            // Centrado en el cuerpo (mitad del frame − mitad del
                            // globo) MÁS el offset a la izquierda del centro.
                            s16 bubbleX  = (boss.x - cameraX) + ROCKSTEADY_FRAME_W / 2
                                           - BOSS_BUBBLE_W / 2 + BOSS_BUBBLE_X_OFFSET;
                            sayBubble = SPR_addSprite(&say_your_prayers,
                                                      bubbleX, bubbleY,
                                                      TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
                            if (sayBubble) {
                                SPR_setDepth(sayBubble, SPR_MIN_DEPTH);
                                bubblePhase = 1;
                                bubbleTimer = 0;
                            }
                        }
                    }
                    // Ciclo del globo: sólido → parpadeo → se suelta.
                    if (bubblePhase == 1) {
                        if (++bubbleTimer >= BOSS_BUBBLE_SOLID_F) {
                            bubblePhase = 2; bubbleTimer = 0;
                        }
                    } else if (bubblePhase == 2) {
                        if (sayBubble)
                            SPR_setVisibility(sayBubble,
                                ((bubbleTimer / BOSS_BUBBLE_TOGGLE) & 1) ? HIDDEN : VISIBLE);
                        if (++bubbleTimer >= BOSS_BUBBLE_BLINK_F) {
                            if (sayBubble) { SPR_releaseSprite(sayBubble); sayBubble = NULL; }
                            bubblePhase = 3;
                        }
                    }
                    // Espera a que termine el taunt (~2.8s) → empieza la batalla.
                    if (++bossTimer >= 170) { bossStage = 99; bossTimer = 0; }
                    break;
            }
        }

        // 4d. Cutscene de victoria: Shredder rapta a April (se dispara en 4b
        //     cuando el jefe llega a ROCKSTEADY_GONE).
        if (cutScene > 0) {
            switch (cutScene) {
                case 1: {   // Shredder aparece en la puerta de la cápsula (Idle)
                    if (cutTimer == 0) {
                        // Paleta propia de Shredder en PAL3 (Rocksteady ya la
                        // liberó al morir); índice 1 blanco para el HUD.
                        PAL_setPalette(PAL3, shredder_lvl1.palette->data, DMA);
                        PAL_setColor(PAL3 * 16 + 1, 0x0EEE);
                        shredderX = SHREDDER_SPAWN_X;
                        shredderY = SHREDDER_FEET_Y - SHREDDER_FRAME_H;
                        shredderSpr = SPR_addSprite(&shredder_lvl1,
                                                    shredderX - cameraX, shredderY,
                                                    TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
                        if (shredderSpr) {
                            // Detrás de April (depth -APRIL_LANE_Y+20 = -128):
                            // menor valor = delante, así que con -108 Shredder
                            // queda DETRÁS de ella (y de los jugadores, que usan
                            // -y con y >= 118) pero delante de la cápsula.
                            SPR_setDepth(shredderSpr, -APRIL_LANE_Y + 40);
                            SPR_setAutoAnimation(shredderSpr, FALSE);
                            SPR_setAnim(shredderSpr, 0);          // Idle [0] (sin flip: el arte ya mira a la izquierda)
                        }
                    }
                    // Pausa dramática corta mirando la escena, luego camina.
                    if (++cutTimer >= 20) { cutScene = 2; cutTimer = 0; }
                    break;
                }
                case 2: {   // Camina (Walk [1]) por el lane de April. El spawn está a la
                            // derecha (sobre la cápsula) y April a la izquierda: hay que
                            // caminar en AMBOS sentidos (dx < 0 aquí).
                    if (cutTimer == 0 && shredderSpr) {
                        SPR_setAutoAnimation(shredderSpr, TRUE);   // walk = 6 frames ~10 fps
                        SPR_setAnim(shredderSpr, 1);
                    }
                    s16 dx = SHREDDER_GRAB_X - shredderX;
                    if (dx == 0) {
                        cutScene = 3; cutTimer = 0;
                    } else {
                        s16 step = (abs(dx) > SHREDDER_WALK_SPEED) ? SHREDDER_WALK_SPEED : abs(dx);
                        shredderX += (dx > 0) ? step : -step;
                        if (shredderSpr)
                            SPR_setPosition(shredderSpr, shredderX - cameraX, shredderY);
                    }
                    break;
                }
                case 3: {   // Rapto [2]: la toma por detrás (frames 0-1 incluyen a
                            // April DENTRO del sprite) → se libera el sprite propio.
                    if (cutTimer == 0) {
                        if (shredderSpr) {
                            SPR_setAutoAnimation(shredderSpr, FALSE);  // frames MANUALES
                            SPR_setAnimAndFrame(shredderSpr, 2, 0);    // arte a la derecha (de frente al salir), sin flip
                        }
                        if (aprilSpr) { SPR_releaseSprite(aprilSpr); aprilSpr = NULL; }
                        XGM2_playPCMEx(scream_april, sizeof(scream_april),
                                       SOUND_PCM_CH3, 15, FALSE, FALSE);
                    } else if (cutTimer == SHREDDER_RAPTO_TICKS) {
                        if (shredderSpr) SPR_setAnimAndFrame(shredderSpr, 2, 1);
                    }
                    if (++cutTimer >= SHREDDER_RAPTO_TICKS * 2) {
                        cutScene = 4; cutTimer = 0;
                    }
                    break;
                }
                case 4: {   // Frame 2 del Rapto CONGELADO (pose de salto) + arco
                            // hacia la ventana del extremo derecho. A 3 tiles del
                            // borde del nivel arranca el fade a negro (corre por
                            // VBlank mientras el sprite sigue volando); al
                            // completarse → victoria.
                    if (cutTimer == 0 && shredderSpr)
                        SPR_setAnimAndFrame(shredderSpr, 2, 2);
                    if (cutTimer < SHREDDER_JUMP_FRAMES) {
                        u16 t = cutTimer;
                        s16 sx = SHREDDER_GRAB_X;
                        s16 sy = SHREDDER_FEET_Y - SHREDDER_FRAME_H;
                        s16 ex = SHREDDER_JUMP_X_END;
                        s16 ey = SHREDDER_JUMP_Y_END;
                        s16 px = sx + ((s16)(ex - sx) * t) / SHREDDER_JUMP_FRAMES;
                        s16 py = sy + ((s16)(ey - sy) * t) / SHREDDER_JUMP_FRAMES;
                        // Ápice del arco: sube más en la mitad del vuelo (la
                        // parábola t*(N-t) vale 0 en los extremos y máximo en t=N/2).
                        s16 arc = (SHREDDER_ARC_HEIGHT * (s16)t * (s16)(SHREDDER_JUMP_FRAMES - t))
                                  / ((SHREDDER_JUMP_FRAMES * SHREDDER_JUMP_FRAMES) / 4);
                        py -= arc;
                        shredderX = px;
                        shredderY = py;
                        if (shredderSpr)
                            SPR_setPosition(shredderSpr, px - cameraX, py);
                        cutTimer++;
                        // Fade a negro cuando el sprite está a 3 tiles del extremo:
                        // el sprite sigue saliendo por la ventana mientras la
                        // pantalla se funde, y el negro tapa la carga de la
                        // escena siguiente (sin "pop" en la transición).
                        if (!winFade && shredderX >= SHREDDER_FADE_START_X) {
                            PAL_fadeOutAll(SHREDDER_FADE_FRAMES, FALSE);
                            winFade = TRUE;
                        }
                    } else if (winFade) {
                        // El arco terminó (el sprite ya salió por la ventana):
                        // esperar a que el fade a negro se complete antes de
                        // cortar a la escena siguiente.
                        if (!PAL_isDoingFade()) {
                            cutScene = 5;
                            win = TRUE;
                        }
                    } else {
                        cutScene = 5;   // salió sin fade (fallback, no debería pasar)
                        // Marcar la victoria ANTES de salir del bucle: el
                        // `if (cutScene == 5) break;` de abajo rompe el while
                        // en el MISMO frame, así que el case 5 nunca corre.
                        win = TRUE;
                    }
                    break;
                }
                case 5:   // (no-op: la victoria se marcó en el case 4)
                    break;
            }
            if (cutScene == 5)
                break;
        }

        // 5. Enemigos: separación + IA.
        separateEnemies(enemies, MAX_ENEMIES);
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].state == ENEMY_STATE_INACTIVE) continue;
            setEnemyCamera(&enemies[i], cameraX);
            updateEnemy(&enemies[i], &p1, &p2, dosJugadores);
        }

        // 5b. Shurikens: actualizar posición, auto-destrucción off-screen.
        shurikenUpdate(cameraX);

        // 5c. Rocksteady (jefe de la fase 2).
        rocksteadyUpdate(&boss, cameraX, &p1, &p2, dosJugadores);

        // 5d. Balas del disparo del jefe.
        rocksteadyBulletUpdate(cameraX);

        // 5e. Flash de paleta por HP bajo de Rocksteady: alterna entre la paleta
        //     normal y la "quemada" cada ROCKSTEADY_FLASH_TICKS frames (más
        //     rápido cuando queda <= ROCKSTEADY_FLASH_CRIT_HP). Fuera del umbral
        //     o con el jefe muerto, restaura la paleta normal.
        if (boss.state != ROCKSTEADY_INACTIVE && boss.state != ROCKSTEADY_GONE &&
            boss.hp > 0) {
            u8 interval = (boss.hp <= ROCKSTEADY_FLASH_CRIT_HP)
                        ? ROCKSTEADY_FLASH_CRIT_TICKS
                        : ((boss.hp <= ROCKSTEADY_FLASH_HP) ? ROCKSTEADY_FLASH_TICKS : 0);
            if (interval > 0) {
                if (bossFlashTick > 0) bossFlashTick--;
                if (bossFlashTick == 0) {
                    bossFlashTick = interval;
                    bossFlashOn ^= 1;
                    PAL_setPalette(PAL3, bossFlashOn ? flashPal : bossPal, DMA);
                }
            } else if (bossFlashOn) {
                bossFlashOn = 0;
                PAL_setPalette(PAL3, bossPal, DMA);
            }
        } else if (bossFlashOn) {
            bossFlashOn = 0;
            PAL_setPalette(PAL3, bossPal, DMA);
        }

        // 6. Colisiones: ataque del jugador → enemigos.
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (!enemyCanBeHit(&enemies[i])) continue;

            s16 ex = getEnemyCenterX(&enemies[i]);
            s16 ey = getEnemyCenterY(&enemies[i]);

            s16     dmg      = 0;
            Player* attacker = NULL;
            if (playerAttackHits(&p1, ex, ey)) {
                dmg = isPlayerSpecialAttack(&p1) ? ENEMY_HP : 1; attacker = &p1;
            } else if (dosJugadores && playerAttackHits(&p2, ex, ey)) {
                dmg = isPlayerSpecialAttack(&p2) ? ENEMY_HP : 1; attacker = &p2;
            }

            if (dmg > 0) {
                damageEnemy(&enemies[i], dmg);
                if (attacker && isPlayerJumpKicking(attacker))
                    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                if (attacker && enemies[i].state == ENEMY_STATE_DEAD) {
                    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode), SOUND_PCM_CH3, 15, FALSE, FALSE);
                    addPlayerScore(attacker, 1);
                }
            }
        }

        // 6-boss. Colisiones: ataque del jugador → Rocksteady.
        // Normal −1 barra, especial −ROCKSTEADY_SPECIAL_DMG. Golpear con la
        // patada voladora suena el "pum" (igual que contra los foot soldiers).
        // Hitbox más chica: el punto de impacto se hunde ROCKSTEADY_HIT_INSET
        // px dentro del cuerpo (alejado del jugador), hay que llegar más cerca.
        if (rocksteadyCanBeHit(&boss)) {
            s16     bcx = rocksteadyGetCenterX(&boss);
            s16     by  = rocksteadyGetCenterY(&boss);
            s16     p1cx = p1.x + PLAYER_SPRITE_W / 2;
            s16     bx1  = bcx + ((bcx >= p1cx) ? ROCKSTEADY_HIT_INSET : -ROCKSTEADY_HIT_INSET);
            s16     bdmg = 0;
            Player* batt = NULL;
            if (playerAttackHits(&p1, bx1, by)) {
                bdmg = isPlayerSpecialAttack(&p1) ? ROCKSTEADY_SPECIAL_DMG : 1;
                batt = &p1;
            } else if (dosJugadores) {
                s16 p2cx = p2.x + PLAYER_SPRITE_W / 2;
                s16 bx2  = bcx + ((bcx >= p2cx) ? ROCKSTEADY_HIT_INSET : -ROCKSTEADY_HIT_INSET);
                if (playerAttackHits(&p2, bx2, by)) {
                    bdmg = isPlayerSpecialAttack(&p2) ? ROCKSTEADY_SPECIAL_DMG : 1;
                    batt = &p2;
                }
            }
            if (bdmg > 0) {
                rocksteadyDamage(&boss, bdmg);
                // Golpe al jefe: "pum" propio de Rocksteady (distinto del de los
                // foot soldiers, que usa hit_turtles).
                XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 15, FALSE, FALSE);
                if (batt && isPlayerJumpKicking(batt))
                    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                if (batt && boss.state == ROCKSTEADY_DEAD)
                    addPlayerScore(batt, 10);   // baja del jefe final
            }
        }

        // 6b. Colisiones: ataques de los foot soldiers → jugadores.
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            Enemy* e = &enemies[i];
            if (e->state != ENEMY_STATE_ATTACK) continue;

            if (playerCanBeHit(&p1) &&
                enemyTryHitPlayer(e, getPlayerWorldX(&p1), getPlayerY(&p1))) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(&p1, getEnemyCenterX(e));
            } else if (dosJugadores && playerCanBeHit(&p2) &&
                       enemyTryHitPlayer(e, getPlayerWorldX(&p2), getPlayerY(&p2))) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(&p2, getEnemyCenterX(e));
            }
        }

        // 6c. Shurikens → ataque del jugador (rompe proyectiles) y → jugadores.
        {
            if (shurikenBreakByPlayerAttack(&p1)) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
            }
            if (dosJugadores && shurikenBreakByPlayerAttack(&p2)) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
            }
        }
        {
            s16 hitX = 0;
            if (playerCanBeHit(&p1) &&
                shurikenCheckHitPlayer(getPlayerWorldX(&p1), getPlayerY(&p1), &hitX)) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(&p1, hitX);
            }
            if (dosJugadores && playerCanBeHit(&p2) &&
                shurikenCheckHitPlayer(getPlayerWorldX(&p2), getPlayerY(&p2), &hitX)) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(&p2, hitX);
            }
        }

        // 6c-bis. Balas del jefe → el ataque del jugador las rompe (antes de
        //         chequear impacto contra el jugador) y → los jugadores.
        {
            if (rocksteadyBulletBreakByPlayerAttack(&p1)) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
            }
            if (dosJugadores && rocksteadyBulletBreakByPlayerAttack(&p2)) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
            }
        }
        {
            s16 hitX = 0;
            if (playerCanBeHit(&p1) &&
                rocksteadyBulletCheckHitPlayer(getPlayerWorldX(&p1), getPlayerY(&p1), &hitX)) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(&p1, hitX);
            }
            if (dosJugadores && playerCanBeHit(&p2) &&
                rocksteadyBulletCheckHitPlayer(getPlayerWorldX(&p2), getPlayerY(&p2), &hitX)) {
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(&p2, hitX);
            }
        }

        // 6d. HUD: refrescar barra de vida, vidas y puntaje.
        hudPlayerUpdate(&hud1);
        if (dosJugadores) hudPlayerUpdate(&hud2);

        // 6d-bis. April (rehén): bobbing suave de "respirando".
        {
            s16 bob = (s16)((aprilTimer >> 3) & 7);
            if (bob > 3) bob = 7 - bob;
            aprilTimer++;
            if (aprilSpr)
                SPR_setPosition(aprilSpr, APRIL_WORLD_X - cameraX,
                                APRIL_LANE_Y - APRIL_FOOT_OFFSET + bob - 2);
        }

        // 6e. Continues: igual que en el nivel 1 (marco con "CONTINUE?" y
        //     cuenta regresiva). NO aplica durante la cutscene de victoria
        //     (la tortuga está congelada observando y no puede morir) ni
        //     mientras el jefe ya está muriendo/murió (un KO simultáneo del
        //     jugador en la misma frame en que muere el jefe no debe
        //     convertir la victoria en un game over).
        bool bossDown = (boss.state == ROCKSTEADY_DEAD ||
                         boss.state == ROCKSTEADY_GONE);
        if (cutScene == 0 && !bossDown) {
            bool out1 = continuePoll(&cont1, &p1, &hud1, hudSprite1, portraitSpr1,
                                     JOY_1, &personajeSeleccionado,
                                     dosJugadores ? personaje2Seleccionado : 0xFF, fps);
            bool out2 = FALSE;
            if (dosJugadores)
                out2 = continuePoll(&cont2, &p2, &hud2, hudSprite2, portraitSpr2,
                                    JOY_2, &personaje2Seleccionado,
                                    personajeSeleccionado, fps);
            if (out1 && (!dosJugadores || out2))
                break;
        }

        // 7. Scroll del fondo. Durante la emergencia de la cápsula (stage 1) se
        //    suma un temblor horizontal al scroll y a la X de la cápsula (es un
        //    sprite, no scrollea sola): el HUD y la cámara de juego NO tiemblan.
        s16 dispCam = cameraX;
        s16 capsX   = CAPSULA_SCREEN_X;
        if (phase == 2 && bossStage == 1) {
            s16 shake = capsuleShake(bossTimer);
            dispCam += shake;
            capsX   += shake;
        }
        if (capsulaSpr) SPR_setPosition(capsulaSpr, capsX, CAPSULA_SCREEN_Y);
        bgUpdate2(dispCam);

        // 8. Animar el fuego del primer plano + su scroll de parallax.
        fireUpdate(dispCam);

        // 8b. Animar el humo del techo + su scroll de parallax.
        smokeUpdate(dispCam);

        SPR_update();
        SYS_doVBlankProcess();
    }

    // Liberar shurikens y balas activos (evita que queden volando en la cutscene).
    rocksteadyBulletReleaseAll();
    shurikenReleaseAll();

    // Restaurar el motor de sprites al presupuesto global de 752 tiles (este
    // nivel lo subió a 852 para la cápsula del taladro del jefe). SPR_initEx
    // libera los sprites que quedaran vivos.
    SPR_initEx(752);

    // Restaurar atributos de texto por defecto para el resto de las escenas
    // (el HUD los dejó en prioridad alta / PAL3).
    VDP_setTextPriority(0);
    VDP_setTextPalette(PAL0);

    // Secuencia de salida (victoria): fundido y a la cutscene final (Shredder
    // se lleva a April).
    if (win) {
        // Victoria: guardar vidas/puntaje persistentes (por si se rejuega o hay
        // más niveles; el estado se resetea en la selección de personajes).
        playerPersistSave(&p1);
        if (dosJugadores) playerPersistSave(&p2);

        // Si la cutscene ya fundió a negro (fade del final en el vuelo), la
        // pantalla ya está en negro: no volver a fadear.
        if (!winFade) {
            PAL_fadeOutAll(30, FALSE);
            while (PAL_isDoingFade()) SYS_doVBlankProcess();
        }

        clearScene();
        return SCENE_ENDING;
    }

    clearScene();
    return SCENE_GAME_OVER;
}

// ---------------------------------------------------------------------------
// Cutscene final — Shredder rapta a April (imagen combinada de 2 planos)
// ---------------------------------------------------------------------------
// BG_B_final = fondo (plano BG_B, su paleta en PAL0); BG_A_final = ENCIMA
// (plano BG_A, su paleta en PAL1, índice 0 transparente). Juntas forman una
// imagen de ~32 colores. Entre las dos suman ~1000 tiles, así que se LIBERA la
// VRAM de sprites (SPR_end) mientras se muestran y se restaura antes de volver.
// Permanece ~5 s y reinicia el juego (vuelve al logo de SEGA).
SceneId showEnding() {
    clearScene();
    SPR_end();                       // libera la VRAM de sprites (no se usan acá)

    VDP_setBackgroundColor(0);

    // Se cargan las DOS imágenes con las PALETAS EN NEGRO (clearScene ya las
    // dejó así tras el fade). Como forman UNA sola imagen, no hay que dejar ver
    // el estado intermedio: mientras la paleta esté en negro no se ve nada,
    // aunque el DMA/descompresión de BG_A tarde un poco más que el de BG_B.
    VDP_drawImageEx(BG_B, &bg_b_final,
                    TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX),
                    0, 0, FALSE, TRUE);
    // BG_A_final ENCIMA (plano BG_A, PAL1). Sus tiles van después de los del
    // fondo; el índice 0 (transparente) deja ver el fondo.
    u16 aInd = TILE_USER_INDEX + bg_b_final.tileset->numTile;
    VDP_drawImageEx(BG_A, &bg_a_final,
                    TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, aInd),
                    0, 0, FALSE, TRUE);

    // Con las DOS ya cargadas, recién ahí se REVELA la imagen completa de una,
    // con un fade-in conjunto de ambas paletas (PAL0 = fondo, PAL1 = overlay).
    // Así nunca se ve "BG_B sola" antes de que entre BG_A.
    u16 combinedPal[32];
    for (u16 i = 0; i < 16; i++) {
        combinedPal[i]      = bg_b_final.palette->data[i];
        combinedPal[16 + i] = bg_a_final.palette->data[i];
    }
    PAL_fadeIn(0, 31, combinedPal, 20, FALSE);   // ~0.33s, las dos paletas juntas

    // Esperar a que la imagen esté COMPLETAMENTE visible (el fade corre por
    // VBlank) antes de contar el retraso de la risa.
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    // Mantener ~5 segundos (START adelanta). La risa de Shredder NO suena al
    // entrar a la escena: arranca ~1.5s después de verse la imagen.
    u16 timer      = (IS_PAL_SYSTEM ? 50 : 60) * 5;
    u16 laughDelay = 90;
    bool laughed   = FALSE;
    while (timer > 0) {
        timer--;
        if (!laughed) {
            if (laughDelay > 0) laughDelay--;
            else {
                XGM2_playPCMEx(shredder_laugh_sfx, sizeof(shredder_laugh_sfx),
                               SOUND_PCM_CH2, 15, FALSE, FALSE);
                laughed = TRUE;
            }
        }
        if (JOY_readJoypad(JOY_1) & BUTTON_START) break;
        SYS_doVBlankProcess();
    }

    SPR_initEx(600);                 // restaurar el motor de sprites (lo usan las escenas siguientes)
    clearScene();
    return SCENE_SEGA;               // reinicia el juego
}

// ---------------------------------------------------------------------------
// 9. Game Over — "GAME OVER" sobre fondo negro, luego reinicia el juego
// ---------------------------------------------------------------------------
SceneId showGameOver() {
    clearScene();

    // Fuente por defecto (blanca). clearScene dejó todas las paletas en negro,
    // así que ponemos blanco en el índice que usa la fuente default (15).
    VDP_loadFont(&font_default, DMA);
    VDP_setTextPlane(BG_A);
    VDP_setTextPriority(0);
    VDP_setTextPalette(PAL0);
    VDP_setBackgroundColor(0);
    PAL_setColor(15, 0x0EEE);   // blanco

    // "GAME OVER" (9 chars) centrado en las 40 columnas: x = (40-9)/2 ≈ 15
    VDP_drawText("GAME OVER", 15, 13);

    // Mantener en pantalla ~4 segundos (START adelanta)
    u16 timer = (IS_PAL_SYSTEM ? 50 : 60) * 4;
    while (timer > 0) {
        timer--;
        if (JOY_readJoypad(JOY_1) & BUTTON_START) break;
        SYS_doVBlankProcess();
    }
    // Esperar a que se suelte START para no saltear la intro siguiente
    while (JOY_readJoypad(JOY_1) & BUTTON_START)
        SYS_doVBlankProcess();

    clearScene();
    return SCENE_SEGA;   // reiniciar el juego desde el logo de SEGA
}
