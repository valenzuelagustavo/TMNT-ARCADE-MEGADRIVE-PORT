#include "scenes.h"
#include "intro.h"   // sega_logo_spr, rocksteady_spr
#include "menus.h"   // logo, characters_greyscale, selector_turtle, character_selector, faces_hud
#include "level1.h"  // bg_level1 (IMAGE, 1376x224 — nivel completo), fire_tiles (TILESET, 8 frames de 64x64), title_font, title_font_pal
#include "audio.h"   // music_sega, golpe, music_level1, select_music
#include "player.h"  // sistema del jugador (incluye chars.h internamente)
#include "enemy.h"   // sistema de enemigos (incluye enemies.h → foot_soldier)

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

#define BG_PLANE_W           64     // Ancho del plano circular de fondo (tiles)

// ---------------------------------------------------------------------------
// Volumen de audio (0..100) — requiere el driver XGM2 (recursos XGM2 en
// audio.res). El XGM clásico no tiene control de volumen.
// ---------------------------------------------------------------------------
#define VOL_MUSIC_INTRO    100
#define VOL_MUSIC_SELECT   90
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
static const u16* bgMapData;   // tilemap completo en ROM (sin comprimir)
static u16        bgMapW;      // ancho del mapa en tiles (172)
static u16        bgMapH;      // alto del mapa en tiles (28)
static u16        bgBaseAttr;  // atributo base: paleta + índice base en VRAM
static s16        bgLastCol;   // última columna FUENTE ya volcada al plano

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
    VDP_setHorizontalScroll(BG_B, -cameraX);
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
// Ventajas sobre el truco del scroll: entra en VRAM, todas las celdas quedan
// EN FASE, y el scroll de BG_A queda LIBRE (para un HUD futuro, por ejemplo).
// El fuego queda fijo en la banda inferior aunque la cámara recorra el nivel.
// Paleta: el fuego COMPARTE la paleta de los foot soldiers → PAL2.
// ---------------------------------------------------------------------------
#define FIRE_CELL_TILES_W    8    // Celda de fuego: 8 tiles de ancho (64px)
#define FIRE_CELL_TILES_H    8    // 8 tiles de alto (64px)
#define FIRE_CELL_TILES      (FIRE_CELL_TILES_W * FIRE_CELL_TILES_H)   // 64
#define FIRE_FRAMES          8    // Frames de animación en fire_strip.png
#define FIRE_FRAME_INTERVAL  8    // Frames de juego entre cada frame de fuego
#define FIRE_Y_TILE          ((224 / 8) - FIRE_CELL_TILES_H)  // 20: banda inferior

static u16 fireVramInd;  // Primer tile de VRAM de la celda del fuego
static u16 fireFrame;    // Frame de animación actual (0..7)
static u16 fireTimer;    // Contador hasta el próximo paso

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
    VDP_setHorizontalScroll(BG_A, 0);
}

// Avanza la animación del fuego. Llamar una vez por frame en el bucle del nivel.
static void fireUpdate() {
    if (++fireTimer >= FIRE_FRAME_INTERVAL) {
        fireTimer = 0;
        fireFrame = (fireFrame + 1) & (FIRE_FRAMES - 1);
        // Pisar los MISMOS 64 tiles de VRAM con el frame siguiente. Cada tile
        // son 8 longwords → el frame N arranca en tiles + N*64*8. DMA_QUEUE:
        // la transferencia real (2KB) se hace en el próximo vblank.
        VDP_loadTileData(fire_tiles.tiles + (fireFrame * FIRE_CELL_TILES * 8),
                         fireVramInd, FIRE_CELL_TILES, DMA_QUEUE);
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
    //   PAL0 → fondo | PAL1 → tortugas | PAL2 → foot soldiers + fuego | PAL3 → flash de golpe
    bgInit();

    // --- Fuego en primer plano (BG_A, prioridad alta) ---
    // Los tiles del fuego van a VRAM justo después del tileset del fondo.
    fireInit(TILE_USER_INDEX + bg_level1.tileset->numTile);

    // Paleta "flash" (silueta blanca al recibir golpe) en PAL3, la única libre.
    initEnemyFlashPalette(PAL3);

    // Estado global de la IA de grupo: contador de atacantes simultáneos y
    // reparto de targets entre los jugadores presentes (1 o 2).
    resetEnemyAI(cantidadJugadores);

    // --- Música del nivel (volumen reducido, ver VOL_MUSIC_LEVEL1) ---
    playMusicVol(music_level1, VOL_MUSIC_LEVEL1);

    // --- Inicializar jugador(es) ---
    // Las 4 tortugas comparten la paleta unificada, así que P1 y P2 usan PAL1.
    bool dosJugadores = (cantidadJugadores == 2);

    // Los límites izquierdo/derecho reales se recalculan CADA frame en el
    // paso 3 del bucle (dependen de la cámara); acá solo el arranque.
    Player p1;
    initPlayer(&p1, personajeSeleccionado, JOY_1, PAL1, 100, 182);
    setPlayerRightBound(&p1, SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W);

    Player p2;
    if (dosJugadores) {
        initPlayer(&p2, personaje2Seleccionado, JOY_2, PAL1, 160, 182);
        setPlayerRightBound(&p2, SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W);
    }

    // --- Definición de spawns por OLEADAS (trigger-based) ---
    // Cada punto del nivel dispara una oleada: varias entradas con el mismo
    // triggerX. side +1 = entra de FRENTE (off-screen derecha), -1 = por la
    // ESPALDA (off-screen izquierda). Las Y son todas distintas dentro de la
    // oleada (separadas ≥24px, más que ENEMY_SEPARATE_Y) para que no vengan
    // en fila india. La X real se calcula al spawnear, relativa a la cámara.
    // Primera oleada: 3 (2 frente + 1 espalda). El resto: 4 (2 y 2).
    // Si una oleada no tiene lugar (tope MAX_ACTIVE_ENEMIES), las entradas
    // quedan pendientes y van entrando a medida que caen los anteriores.
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

    Enemy enemies[MAX_ENEMIES];
    for (u16 i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].state = ENEMY_STATE_INACTIVE;
        enemies[i].sprite = NULL;
    }

    s16 cameraX = 0;   // Borde izquierdo de la cámara en coordenadas de mundo
    bgUpdate(0);       // Scroll inicial

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
            if (dosJugadores) {
                // Tope por el rezagado: su frame nunca pasa el borde izquierdo
                s16 camCap = trailX - CAM_TRAIL_MARGIN;
                if (newCam > camCap) newCam = camCap;
            }
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

        // 4. Spawner: activar enemigos cuando la cámara se acerca.
        //    - spawnUsed: cada spawn dispara una sola vez → los muertos no vuelven.
        //    - MAX_ACTIVE_ENEMIES: tope de foot soldiers simultáneos. Un spawn
        //      que exceda el tope queda pendiente (no consume su spawnUsed) y
        //      dispara recién cuando muere alguno.
        s16 screenRightEdge = cameraX + SCREEN_PIXEL_WIDTH;

        u16 activeEnemies = 0;
        for (u16 i = 0; i < MAX_ENEMIES; i++)
            if (enemies[i].state != ENEMY_STATE_INACTIVE) activeEnemies++;

        for (u16 s = 0; s < LEVEL1_SPAWN_COUNT; s++) {
            if (activeEnemies >= MAX_ACTIVE_ENEMIES) break;
            if (spawnUsed[s]) continue;
            if (screenRightEdge < spawnDefs[s].triggerX) continue;

            // X de spawn fuera de pantalla según el flanco, relativa a la
            // cámara ACTUAL (una entrada pendiente que entra tarde spawnea
            // igual de bien: siempre justo fuera del borde).
            s16 sx = (spawnDefs[s].side > 0)
                   ? (screenRightEdge + 8)                    // de frente
                   : (cameraX - ENEMY_SPRITE_W - 8);          // por la espalda

            // Buscar un slot libre en el pool de enemigos
            for (u16 i = 0; i < MAX_ENEMIES; i++) {
                if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                    initEnemySpawn(&enemies[i], sx, spawnDefs[s].y, 60, PAL2);
                    // Los de oleada nacen PERSIGUIENDO (si patrullaran, los
                    // de la espalda se quedarían caminando fuera de cámara
                    // hasta que el jugador retroceda a su rango de aggro).
                    enemies[i].state = ENEMY_STATE_CHASE;
                    spawnUsed[s] = TRUE;
                    activeEnemies++;
                    break;
                }
            }
        }

        // 5. Actualizar enemigos con IA.
        //    Primero la separación de grupo (que no se apilen entre ellos) y
        //    después el update individual de cada uno.
        separateEnemies(enemies, MAX_ENEMIES);

        s16 p1wx = getPlayerWorldX(&p1);
        s16 p1wy = getPlayerY(&p1);
        s16 p2wx = p1wx, p2wy = p1wy;
        if (dosJugadores) {
            p2wx = getPlayerWorldX(&p2);
            p2wy = getPlayerY(&p2);
        }
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].state == ENEMY_STATE_INACTIVE) continue;
            setEnemyCamera(&enemies[i], cameraX);
            updateEnemy(&enemies[i], p1wx, p1wy, p2wx, p2wy, dosJugadores);
        }

        // 6. Colisiones: ataque del jugador → enemigos.
        //    playerAttackHits mide desde el CENTRO de la tortuga, con alcance
        //    frontal real (64px, más que el rango de ataque del foot soldier)
        //    y tolerancia simétrica en profundidad. Incluye la patada en
        //    salto, que antes no golpeaba.
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (!enemyCanBeHit(&enemies[i])) continue;

            s16 ex = getEnemyCenterX(&enemies[i]);
            s16 ey = getEnemyCenterY(&enemies[i]);

            if (playerAttackHits(&p1, ex, ey) ||
                (dosJugadores && playerAttackHits(&p2, ex, ey)))
                damageEnemy(&enemies[i], 1);
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
                damagePlayer(&p1, getEnemyCenterX(e));
            } else if (dosJugadores && playerCanBeHit(&p2) &&
                       enemyTryHitPlayer(e, getPlayerWorldX(&p2), getPlayerY(&p2))) {
                damagePlayer(&p2, getEnemyCenterX(e));
            }
        }

        // 7. Revelar columnas nuevas del fondo y aplicar el scroll
        bgUpdate(cameraX);

        // 8. Animar el fuego del primer plano (scroll de BG_A, independiente
        //    del scroll de cámara que usa BG_B)
        fireUpdate();

        SPR_update();
        SYS_doVBlankProcess();
    }

    clearScene();
    return SCENE_LEVEL1;
}
