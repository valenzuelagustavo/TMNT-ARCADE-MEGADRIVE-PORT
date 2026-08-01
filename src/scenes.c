#include "scenes.h"
#include "intro.h"   // sega_logo_spr, rocksteady_spr
#include "menus.h"   // logo, characters_greyscale, selector_turtle, character_selector, faces_hud
#include "level1.h"  // bg_level1 (IMAGE, 1376x224 — nivel completo), fire_tiles (TILESET, 8 frames de 64x64), hud_1p/hud_2p (SPRITE, 72x32, 4 anims), title_font, title_font_pal
#include "level2.h"  // bg_test (IMAGE, 440x192 — sala del nivel 2), smoke_tiles (TILESET, 8 frames de 64x64)
#include "audio.h"   // music_sega, golpe, music_level1, select_music
#include "player.h"  // sistema del jugador (incluye chars.h internamente)
#include "enemy.h"   // sistema de enemigos (incluye enemies.h → foot_soldier)
#include "robot.h"   // robot del látigo (mini-jefe del final; robot_whip, whip_waves)

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
#define VOL_MUSIC_LEVEL1    40   // la música del nivel saturaba: bajada al 50%
#define VOL_SFX            100

// ---------------------------------------------------------------------------
// Estado global de selección (necesario entre escenas)
// ---------------------------------------------------------------------------
u8 personajeSeleccionado  = 0;  // P1: 0=Leo 1=Mike 2=Don 3=Raph (columnas de pantalla)
u8 personaje2Seleccionado = 3;  // P2: 0=Leo 1=Mike 2=Don 3=Raph (columnas de pantalla)
u8 cantidadJugadores      = 1;  // 1 o 2 jugadores

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

// Inicializa el fondo del nivel: paleta, tileset a VRAM y primeras columnas
static void bgInit() {
    VDP_setPlaneSize(BG_PLANE_W, 32, TRUE);   // plano circular 64x32 (default seguro)

    PAL_setPalette(PAL0, bg_level1.palette->data, DMA);
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

    // El fuego comparte paleta con el foot soldier. Se carga acá porque el
    // primer spawn de enemigos puede tardar varios segundos en dispararse.
    PAL_setPalette(PAL2, foot_soldier.palette->data, DMA);

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
#define HUD_TILE_W     9                                              // Ancho del marco en tiles (72px)
#define HUD_P1_X       0                                              // P1 pegado al borde izquierdo
#define HUD_P2_X       (SCREEN_PIXEL_WIDTH - (HUD_TILE_W * 8))        // P2 pegado al borde derecho

static Sprite* hudSprite1 = NULL;
static Sprite* hudSprite2 = NULL;

// Crea los marcos del HUD como sprites de alto nivel. En 1 jugador solo se
// crea el de P1. No consume VRAM de planos (los tiles viven en el area de
// sprites del motor, SPR_initEx). Los sprites se liberan solos en clearScene
// (SPR_reset) al terminar la escena.
static void hudInit(void) {
    hudSprite1 = SPR_addSprite(&hud_1p, HUD_P1_X, 0, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    if (hudSprite1)
        SPR_setAnim(hudSprite1, personajeSeleccionado);

    if (cantidadJugadores == 2) {
        hudSprite2 = SPR_addSprite(&hud_2p, HUD_P2_X, 0, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
        if (hudSprite2)
            SPR_setAnim(hudSprite2, personaje2Seleccionado);
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
// VIDAS y PUNTAJE van como TEXTO con la fuente por defecto (VDP_drawText) sobre
// BG_A. Se dibujan en PAL3; el color 1 se sobrescribe a blanco (0x0EEE) tras
// cargar la paleta del foot soldier naranja, así el texto sale blanco.
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
SceneId showArcadeIntro()  { return SCENE_PLAYER_SELECT; }

// ---------------------------------------------------------------------------
// 5. Selección de número de jugadores
// ---------------------------------------------------------------------------
SceneId showPlayerSelect() {
    clearScene();

    VDP_setBackgroundColor(0x0040);
    PAL_setPalette(PAL0, logo.palette->data, DMA);
    VDP_drawImageEx(BG_B, &logo, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX), 3, 0, FALSE, TRUE);

    VDP_drawText("1 TORTUGA",  14, 18);
    VDP_drawText("2 TORTUGAS", 14, 20);
    VDP_drawText("Desarrollado por: Gustavo Valenzuela", 2, 26);

    Sprite *cursor = SPR_addSprite(&selector_turtle, 8 * 8, 14 * 8, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    PAL_setPalette(PAL1, selector_turtle.palette->data, DMA);

    u8 selectedOption = 0;

    while (1) {
        u16 value = JOY_readJoypad(JOY_1);

        if (value & BUTTON_UP)   selectedOption = 0;
        if (value & BUTTON_DOWN) selectedOption = 1;

        SPR_setPosition(cursor, 8 * 8, (14 + selectedOption * 2) * 8);

        if (value & BUTTON_START) break;

        SPR_update();
        SYS_doVBlankProcess();
    }

    // selectedOption 0 → 1 jugador | 1 → 2 jugadores
    cantidadJugadores = selectedOption + 1;

    return SCENE_CHAR_SELECT;
}

// ---------------------------------------------------------------------------
// 6. Selección de personaje
// ---------------------------------------------------------------------------
SceneId showCharSelect() {
    clearScene();

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

    PAL_setPalette(PAL1, character_selector.palette->data, DMA);
    PAL_setPalette(PAL2, faces_hud.palette->data, DMA);

    // -----------------------------------------------------------------------
    // MODO 1 JUGADOR
    // -----------------------------------------------------------------------
    if (cantidadJugadores == 1) {
        Sprite* cursor          = SPR_addSprite(&character_selector, charPosX[0], charPosY, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
        Sprite* turtle_face_hud = SPR_addSprite(&faces_hud,          charPosX[0] + faceXoff, faceY, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
        SPR_setDepth(cursor, 1);            // cursor/retrato coloreado detrás
        SPR_setDepth(turtle_face_hud, 0);   // la cara va adelante

        s8   sel        = 0;
        u16  prev       = 0;
        SPR_setAnim(cursor,          sel);
        SPR_setAnim(turtle_face_hud, faceRow[sel]);

        while (1) {
            u16 v = JOY_readJoypad(JOY_1);

            if (justPressedJoy(v, prev, BUTTON_RIGHT) && sel < 3) sel++;
            if (justPressedJoy(v, prev, BUTTON_LEFT)  && sel > 0) sel--;

            SPR_setAnim(cursor,          sel);
            SPR_setAnim(turtle_face_hud, faceRow[sel]);
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

        return SCENE_LEVEL1_TITLE;
    }

    // -----------------------------------------------------------------------
    // MODO 2 JUGADORES — JOY_1 = P1, JOY_2 = P2. No pueden elegir el mismo.
    // Cada uno confirma con START; cuando ambos confirman, se avanza.
    // -----------------------------------------------------------------------
    s8   sel1 = 0, sel2 = 3;          // empiezan en personajes distintos
    bool ready1 = FALSE, ready2 = FALSE;
    u16  prev1 = 0, prev2 = 0;

    Sprite* cur1  = SPR_addSprite(&character_selector, charPosX[sel1], charPosY, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    Sprite* cur2  = SPR_addSprite(&character_selector, charPosX[sel2], charPosY, TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
    Sprite* face1 = SPR_addSprite(&faces_hud, charPosX[sel1] + faceXoff, faceY, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
    Sprite* face2 = SPR_addSprite(&faces_hud, charPosX[sel2] + faceXoff, faceY, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));

    // Las caras van adelante; los cursores/retratos coloreados, detrás.
    SPR_setDepth(cur1, 1);  SPR_setDepth(cur2, 1);
    SPR_setDepth(face1, 0); SPR_setDepth(face2, 0);

    SPR_setAnim(cur1, sel1);  SPR_setAnim(face1, faceRow[sel1]);
    SPR_setAnim(cur2, sel2);  SPR_setAnim(face2, faceRow[sel2]);

    while (1) {
        u16 v1 = JOY_readJoypad(JOY_1);
        u16 v2 = JOY_readJoypad(JOY_2);

        // --- Jugador 1 (mientras no haya confirmado) ---
        if (!ready1) {
            if (justPressedJoy(v1, prev1, BUTTON_RIGHT)) sel1 = charMove(sel1, sel2, +1);
            if (justPressedJoy(v1, prev1, BUTTON_LEFT))  sel1 = charMove(sel1, sel2, -1);
            SPR_setAnim(cur1, sel1);
            SPR_setAnim(face1, faceRow[sel1]);
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

    // Cargar la paleta del foot soldier naranja en PAL3 ahora, para que
    // el texto del HUD (VDP_setTextPalette(PAL3)) tenga una paleta válida
    // desde el primer frame. El PNG tiene blanco (0x0EEE) en el índice 1
    // para que VDP_drawText lo use como color de texto.
    PAL_setPalette(PAL3, foot_soldier_orange.palette->data, DMA);

    // --- Música del nivel (volumen reducido, ver VOL_MUSIC_LEVEL1) ---
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
    // El texto (vidas/puntaje) va con la fuente por defecto sobre BG_A, con
    // prioridad alta (delante de los sprites) y en PAL3 (blanco puro -> texto
    // blanco). La barra usa PAL1 (ya cargada por initPlayer). Cada jugador
    // tiene su bloque de barra en VRAM: P1 en hudVramFree, P2 a +8.
    VDP_setTextPlane(BG_A);
    VDP_setTextPriority(1);
    VDP_setTextPalette(PAL3);

    HudPlayer hud1;
    hudPlayerInit(&hud1, &p1, 0, hudVramFree);
    HudPlayer hud2;
    if (dosJugadores)
        hudPlayerInit(&hud2, &p2, 40 - HUD_TILE_W, hudVramFree + HPBAR_FRAME_TILES);

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
    #define SPARKS2_WORLD_Y  160
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
                    SPR_setPosition(elevSparkSpr[ev], sx - ELEV_SPARK_HALF_W, ELEV_SPARK_TOP_Y);
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

        // 6d. Game over: sin vidas ni barra. En 2P, cuando ambos cayeron.
        //     Por ahora vuelve a la pantalla inicial (todavía sin pantalla de
        //     Game Over ni animación de muerte).
        if (isPlayerGameOver(&p1) && (!dosJugadores || isPlayerGameOver(&p2)))
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

    PAL_setPalette(PAL0, bg_test.palette->data, DMA);
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
    // PRIORIDAD BAJA (FALSE) -> el humo queda DETRÁS de los sprites.
    for (u16 block = 0; block < BG_PLANE_W / SMOKE_CELL_TILES_W; block++) {
        VDP_fillTileMapRectInc(BG_A,
                               TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, vramInd),
                               block * SMOKE_CELL_TILES_W, SMOKE_Y_TILE,
                               SMOKE_CELL_TILES_W, SMOKE_CELL_TILES_H);
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
// Escena completa del nivel 2. Flujo:
//   fase 0: oleada A (2 morados entrando de frente). Cámara bloqueada en 0:
//           no se sale de la primera pantalla.
//   fase 1: oleada A limpia -> sala libre (ida y vuelta, cámara 0..120).
//   fase 2: al llegar al límite -> cámara bloqueada + oleada B (2 naranjas de
//           frente + 1 morado que entra por la espalda).
//   fase 3: oleada B limpia -> victoria -> cutscene final (Shredder/April).
// ---------------------------------------------------------------------------
SceneId showLevel2() {
    clearScene();

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

    // --- PAL3: foot soldier naranja + texto del HUD en blanco ---
    PAL_setPalette(PAL3, foot_soldier_orange.palette->data, DMA);

    // --- Música (misma del pasillo en llamas, volumen reducido) ---
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

    // --- HUD dinámico: barra de vida + vidas + puntaje ---
    VDP_setTextPlane(BG_A);
    VDP_setTextPriority(1);
    VDP_setTextPalette(PAL3);

    HudPlayer hud1;
    hudPlayerInit(&hud1, &p1, 0, hudVramFree);
    HudPlayer hud2;
    if (dosJugadores)
        hudPlayerInit(&hud2, &p2, 40 - HUD_TILE_W, hudVramFree + HPBAR_FRAME_TILES);

    // --- Pool de enemigos ---
    Enemy enemies[MAX_ENEMIES];
    for (u16 i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].state = ENEMY_STATE_INACTIVE;
        enemies[i].sprite = NULL;
    }

    s16 cameraX = 0;   // Borde izquierdo de la cámara en coordenadas de mundo
    bgUpdate2(0);      // Scroll inicial

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

    // --- Fases del nivel ---
    u8   phase       = 0;    // 0=oleada A · 1=sala libre · 2=oleada B · 3=victoria
    s16  cameraLockX = 0;    // >=0 = cameraX no puede superar este valor
    bool win         = FALSE;

    while (1) {
        // 1. Input y física de cada jugador
        updatePlayer(&p1);
        if (dosJugadores) updatePlayer(&p2);

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
        } else if (leadScreenX < CAM_DEAD_ZONE_LEFT && cameraX > 0) {
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
            // Llegó al límite del nivel: oleada B (2 naranjas de frente + 1
            // morado que entra por la espalda) y cámara bloqueada ahí.
            cameraLockX = LEVEL2_CAM_MAX_X;
            phase = 2;
            s16 camL = cameraX;
            s16 camR = cameraX + SCREEN_PIXEL_WIDTH;
            // 2 naranjas (PAL3) desde el borde derecho, lanes distintas.
            for (u16 s = 0; s < 2; s++) {
                for (u16 i = 0; i < MAX_ENEMIES; i++) {
                    if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                        initEnemySpawn(&enemies[i], camR + (s * 48), 158,
                                       0, PAL3, ENEMY_TYPE_FOOT_SOLDIER_ORANGE);
                        enemies[i].dir = -1;
                        enemies[i].state = ENEMY_STATE_CHASE;
                        break;
                    }
                }
            }
            // 1 morado que entra por la espalda (voltereta desde la izquierda).
            for (u16 i = 0; i < MAX_ENEMIES; i++) {
                if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                    initEnemySomersaultSpawn(&enemies[i], camL - ENEMY_SPRITE_W_PURPLE, 148,
                                             1, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                    break;
                }
            }
        } else if (phase == 2 && activeEnemies == 0) {
            // Oleada B limpia: victoria.
            win = TRUE;
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

        // 6d. HUD: refrescar barra de vida, vidas y puntaje.
        hudPlayerUpdate(&hud1);
        if (dosJugadores) hudPlayerUpdate(&hud2);

        // 6e. Game over: sin vidas ni barra. En 2P, cuando ambos cayeron.
        if (isPlayerGameOver(&p1) && (!dosJugadores || isPlayerGameOver(&p2)))
            break;

        // 7. Scroll del fondo.
        bgUpdate2(cameraX);

        // 8. Animar el fuego del primer plano + su scroll de parallax.
        fireUpdate(cameraX);

        // 8b. Animar el humo del techo + su scroll de parallax.
        smokeUpdate(cameraX);

        SPR_update();
        SYS_doVBlankProcess();
    }

    // Liberar shurikens activos (evita que queden volando en la cutscene).
    shurikenReleaseAll();

    // Restaurar atributos de texto por defecto para el resto de las escenas
    // (el HUD los dejó en prioridad alta / PAL3).
    VDP_setTextPriority(0);
    VDP_setTextPalette(PAL0);

    // Secuencia de salida (victoria): fundido y a la cutscene final (Shredder
    // se lleva a April).
    if (win) {
        PAL_fadeOutAll(30, FALSE);
        while (PAL_isDoingFade()) SYS_doVBlankProcess();

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

    // Mantener ~5 segundos (START adelanta)
    u16 timer = (IS_PAL_SYSTEM ? 50 : 60) * 5;
    while (timer > 0) {
        timer--;
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
