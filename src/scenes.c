#include "scenes.h"
#include "intro.h"   // sega_logo_spr, rocksteady_spr
#include "menus.h"   // logo, characters_greyscale, selector_turtle, character_selector, faces_hud
#include "level1.h"  // bg_level1 (IMAGE, 1376x224 — nivel completo), title_font, title_font_pal
#include "audio.h"   // music_sega, golpe, music_level1, select_music
#include "player.h"  // sistema del jugador (incluye chars.h internamente)
#include "enemy.h"   // sistema de enemigos

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
#define CAM_DEAD_ZONE_RIGHT  192    // Player X pantalla > este valor → scroll derecha
#define CAM_DEAD_ZONE_LEFT    80    // Player X pantalla < este valor → scroll izquierda

#define BG_PLANE_W           64     // Ancho del plano circular de fondo (tiles)

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
    return SCENE_PLAYER_SELECT;
}

// ---------------------------------------------------------------------------
// 2-4. Intros pendientes (stubs)
// ---------------------------------------------------------------------------
SceneId showKonamiIntro()  { return SCENE_SGDK; }
SceneId showSGDKIntro()    { return SCENE_INTRO_ARCADE; }
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

// Dibuja el texto letra a letra. Devuelve TRUE si se pidió saltar con START.
static bool drawTextTypewriter(const char* text, u16 x, u16 y) {
    char buf[2];
    buf[1] = 0;

    for (u16 i = 0; text[i] != 0; i++) {
        buf[0] = text[i];

        // Los espacios no se dibujan, pero sí consumen tiempo (ritmo natural)
        if (buf[0] != ' ')
            VDP_drawText(buf, x + i, y);

        // Espera entre letras, con posibilidad de saltar
        for (u16 f = 0; f < TITLE_CHAR_DELAY; f++) {
            if (JOY_readJoypad(JOY_1) & BUTTON_START)
                return TRUE;
            SYS_doVBlankProcess();
        }
    }
    return FALSE;
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
    skipped = drawTextTypewriter(line1, 16, 10);
    if (!skipped) skipped = drawTextTypewriter(line2, 11, 13);
    if (!skipped) skipped = drawTextTypewriter(line3, 14, 15);

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
// 8. Nivel 1 — fondo scrolleable + jugador
// ---------------------------------------------------------------------------
SceneId showLevel1() {
    clearScene();

    // --- Fondo con STREAMING de columnas (nivel completo de 1376px) ---
    // bgInit carga la paleta + tileset completo a VRAM y dibuja las primeras
    // 64 columnas en el plano circular BG_B. bgUpdate() revela columnas nuevas
    // a medida que la cámara avanza. PAL0 → fondo | PAL1 → tortugas (compartida).
    bgInit();

    // --- Música del nivel (volumen reducido, ver VOL_MUSIC_LEVEL1) ---
    playMusicVol(music_level1, VOL_MUSIC_LEVEL1);

    // --- Inicializar jugador(es) ---
    // Las 4 tortugas comparten la paleta unificada, así que P1 y P2 usan PAL1.
    bool dosJugadores = (cantidadJugadores == 2);

    Player p1;
    initPlayer(&p1, personajeSeleccionado, JOY_1, PAL1, 100, 182);
    setPlayerRightBound(&p1, LEVEL1_PIXEL_WIDTH - PLAYER_SPRITE_W);

    Player p2;
    if (dosJugadores) {
        initPlayer(&p2, personaje2Seleccionado, JOY_2, PAL1, 160, 182);
        setPlayerRightBound(&p2, LEVEL1_PIXEL_WIDTH - PLAYER_SPRITE_W);
    }

    // --- Definición de spawns (trigger-based) ---
    // Los enemigos aparecen cuando el borde derecho de la cámara supera
    // triggerX. Spawnean en spawnX (off-screen a la derecha).
    static const EnemySpawnDef spawnDefs[MAX_ENEMIES] = {
        { 400,  440, 182, 60 },
        { 550,  590, 170, 60 },
        { 700,  740, 182, 60 },
        { 850,  890, 174, 60 },
        { 1000, 1040, 182, 60 },
        { 1150, 1190, 170, 60 },
    };

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

        // 2. Cámara dead-zone: sigue al jugador que va MÁS ADELANTE (estilo
        //    arcade). Beat-em-up clásico: solo avanza a la derecha, nunca
        //    retrocede (por eso solo revelamos columnas nuevas a la derecha).
        s16 leadX = getPlayerWorldX(&p1);
        if (dosJugadores) {
            s16 x2 = getPlayerWorldX(&p2);
            if (x2 > leadX) leadX = x2;
        }
        s16 leadScreenX = leadX - cameraX;

        if (leadScreenX > CAM_DEAD_ZONE_RIGHT && cameraX < CAM_MAX_X) {
            cameraX += (leadScreenX - CAM_DEAD_ZONE_RIGHT);
            if (cameraX > CAM_MAX_X) cameraX = CAM_MAX_X;
        }

        // 3. Notificar a cada jugador la nueva posición de cámara y el borde
        //    izquierdo (ningún jugador puede salir por la izquierda de pantalla).
        setPlayerCamera(&p1, cameraX);
        setPlayerLeftBound(&p1, cameraX);
        if (dosJugadores) {
            setPlayerCamera(&p2, cameraX);
            setPlayerLeftBound(&p2, cameraX);
        }

        // 4. Spawner: activar enemigos cuando la cámara se acerca
        s16 screenRightEdge = cameraX + SCREEN_PIXEL_WIDTH;
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].state != ENEMY_STATE_INACTIVE) continue;
            if (screenRightEdge >= spawnDefs[i].triggerX) {
                initEnemySpawn(&enemies[i], spawnDefs[i].spawnX, spawnDefs[i].y, spawnDefs[i].patrolRange, PAL2);
            }
        }

        // 5. Actualizar enemigos con IA
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

        // 6. Colisiones: ataque del jugador → enemigos
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (!enemyCanBeHit(&enemies[i])) continue;

            bool hitP1 = FALSE, hitP2 = FALSE;

            if (isPlayerAttacking(&p1)) {
                s16 atkX = getPlayerWorldX(&p1);
                s16 atkY = getPlayerY(&p1);
                s16 atkLeft, atkRight;
                if (getPlayerDir(&p1) >= 0) {
                    atkLeft  = atkX + 40;
                    atkRight = atkX + 80;
                } else {
                    atkLeft  = atkX - 40;
                    atkRight = atkX + 20;
                }
                s16 atkTop    = atkY - 40;
                s16 atkBottom = atkY;

                s16 ex = getEnemyCenterX(&enemies[i]);
                s16 ey = getEnemyCenterY(&enemies[i]);

                if (ex >= atkLeft && ex <= atkRight && ey >= atkTop && ey <= atkBottom)
                    hitP1 = TRUE;
            }

            if (dosJugadores && !hitP1 && isPlayerAttacking(&p2)) {
                s16 atkX = getPlayerWorldX(&p2);
                s16 atkY = getPlayerY(&p2);
                s16 atkLeft, atkRight;
                if (getPlayerDir(&p2) >= 0) {
                    atkLeft  = atkX + 40;
                    atkRight = atkX + 80;
                } else {
                    atkLeft  = atkX - 40;
                    atkRight = atkX + 20;
                }
                s16 atkTop    = atkY - 40;
                s16 atkBottom = atkY;

                s16 ex = getEnemyCenterX(&enemies[i]);
                s16 ey = getEnemyCenterY(&enemies[i]);

                if (ex >= atkLeft && ex <= atkRight && ey >= atkTop && ey <= atkBottom)
                    hitP2 = TRUE;
            }

            if (hitP1 || hitP2)
                damageEnemy(&enemies[i], 1);
        }

        // 7. Revelar columnas nuevas del fondo y aplicar el scroll
        bgUpdate(cameraX);

        SPR_update();
        SYS_doVBlankProcess();
    }

    clearScene();
    return SCENE_LEVEL1;
}
