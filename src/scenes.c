#include "scenes.h"
#include "intro.h"   // sega_logo_spr, rocksteady_spr
#include "menus.h"   // logo, characters_greyscale, selector_turtle, character_selector, faces_hud
#include "level1.h"  // bg_level1 (IMAGE, 1376x224 — nivel completo), fire_tiles (TILESET, 8 frames de 64x64), hud_1p/hud_2p (SPRITE, 72x32, 4 anims), title_font, title_font_pal
#include "level2.h"  // bg_test (IMAGE, 440x192 — sala del nivel 2), smoke_tiles (TILESET, 8 frames de 64x64)
#include "audio.h"   // music_sega, golpe, music_level1, music_level2, music_boss, music_charselect, music_profiles, music_credits, music_ending
#include "player.h"  // sistema del jugador (incluye chars.h internamente)
#include "enemy.h"   // sistema de enemigos (incluye enemies.h → foot_soldier)
#include "robot.h"   // robot del látigo (mini-jefe del final; robot_whip, whip_waves)
#include "hud.h"     // HUD compartido (marcos, retratos, barra de vida, puntaje)
#include "pause_menu.h" // pausa con START del control 1 + selector de niveles (26/09)
#include "boss_vo.h"     // (01/10) voz de entrada de los jefes antes del tema
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
// Sparks: efecto de fuego detrás de las puertas rompibles, de los ascensores
// y del decorado fijo del piso (sparks_2)
// ---------------------------------------------------------------------------
// Sprite de 32x32 (4x4 tiles) en PAL2, ubicado detrás de cada door_lvl_1
// (más las variantes spark_ascensor y sparks_2, ver más abajo). La animación
// era originalmente por rotación de paleta: 4 cuadros que rotaban los
// índices 5-8 de PAL2 (colores de fuego del foot soldier morado). SE SACÓ
// (29/08): fire_tiles (el fuego de primer plano, SIEMPRE en pantalla) usa
// esos MISMOS índices para dibujarse, así que la rotación también le
// temblaba el color al fuego de fondo -- se notaba en pantalla. No hay una
// 5ta línea de paleta libre en el nivel para aislarlas (PAL0 fondo, PAL1
// tortugas, PAL2 foot soldiers (morado Y naranja)+fuego+chispas+robot, PAL3 foot soldier
// naranja: las 4 ya están repartidas).
//
// Fix: streaming de tiles REALES (mismo truco que fire_tiles/smoke_tiles),
// no rotación de CRAM. sparks_frames/sparks_2_frames/spark_ascensor_frames
// (ver level1.res) son tiras de 4 frames generadas por
// tools/gen_sparks_frames.py: en vez de rotar a qué color apunta cada
// índice, el script rota qué índice tiene cada PÍXEL (pixel de índice i en
// el frame f pasa a índice 5+((i-5+f)%4)) -- con la paleta fija en los
// valores de siempre, el resultado es pixel a pixel IDÉNTICO al de la
// rotación de CRAM, pero PAL2 nunca se toca en runtime.
//
// Las 3 variantes comparten el mismo timer/frame (van sincronizadas, como
// antes) pero cada una vive en su propio bloque FIJO de VRAM (sparksVramInd
// / elevSparkVramInd / sparks2VramInd, reservados en showScene11 después del
// HUD) para que TODAS las instancias de una misma variante (hasta 3 puertas,
// 2 ascensores) apunten al mismo streaming en vez de cada una cargar su
// propia copia -- ver SPR_addSpriteEx con SPR_FLAG_AUTO_VRAM_ALLOC y
// SPR_FLAG_AUTO_TILE_UPLOAD apagados.
//
// (24/09) Las chispas del ASCENSOR y del PISO se veian rotas (las de las
// puertas no). Los frames venian de un TILESET aparte, reordenados a mano
// "por columna", y ese orden solo coincide con el del hardware si el frame
// entra en UN sprite de 4x4 tiles como maximo -- el caso de las puertas.
// spark_ascensor (5x3) y sparks_2 (8x5) rescomp las parte en 2 y 3 sprites
// de hardware y ademas descarta los tiles vacios: 13 y 35 tiles, no 15 y 40.
// Ahora la tira de frames ES el recurso SPRITE (level1.res) y se streamea
// animations[0]->frames[f]->tileset: orden y cantidad los decide rescomp,
// y el armado de sprites de hardware del frame 0 vale para todos (los frames
// solo cambian indices de color, nunca la transparencia). Cada variante
// cicla con SU cantidad de frames (el ascensor trae 3, las otras 4).
// (26/09) Fuego NUEVO de la puerta (door_fire_gen.png, ver level1.res): el
// arte son 33x79 y cubre el hueco ENTERO del fondo, que mide eso mismo y va
// de x = centro-14 a centro+18 y de y = 49 a 127 (medido en los tres).
#define SPARKS_SPRITE_TOP_Y  49    // tope del hueco
#define SPARKS_LEFT_DX       14    // borde izquierdo del hueco = centro - 14
#define SPARKS_FRAME_SPEED    4    // Ticks entre cada frame (~6fps a 25fps, igual que antes)
// Tiles por frame = los que genero rescomp, NO ancho x alto (ver arriba).
#define SPARKS_TILES      (sparks.maxNumTile)          // 16 (4x4, puertas)
#define ELEV_SPARK_TILES  (spark_ascensor.maxNumTile)  // 13 (5x3 menos 2 vacios)
// (26/09) sparks (puertas) pasa a 50 tiles (5x10, todos llenos). sparks_2
// ya no existe: lo reemplaza floor_fire, que es un sprite comun (no se
// streamea) -- su bloque de 35 tiles vuelve al motor de sprites.

// (15/09) Con 4 jugadores se APAGAN los sparks de las PUERTAS y el del PISO
// (sparks_2), para darle aire a las cuatro tortugas. El
// del ASCENSOR se mantiene: es el que acompaña la apertura de los ascensores,
// que es un momento clave del nivel.
// Se gana por dos lados:
//   - Sus bloques de VRAM de fondo (16 + 40 = 56 tiles) no se reservan, y ese
//     espacio se lo queda el motor de sprites (el presupuesto se calcula
//     restando el final de los tiles de usuario, ver showScene11).
//   - Son SPRITES DE HARDWARE menos por scanline, que es el otro cuello de
//     botella cuando hay cuatro tortugas juntas.
static bool sparksDoorFloorOn = TRUE;

static u16 sparksVramInd;      // Bloque fijo de VRAM de sparks (puertas)
static u16 elevSparkVramInd;   // Bloque fijo de VRAM de spark_ascensor
static u16 sparksFrame;        // Contador compartido; cada variante hace % numFrame
static u16 sparksTimer;        // Ticks hasta el próximo frame

// Sube el frame 0 de las 3 tiras a sus bloques fijos de VRAM. Se llama una
// vez al arrancar el nivel (junto con fireInit/hudInit); las instancias de
// sprite se crean después apuntando a estos mismos índices (ver
// SPR_addSpriteEx + SPR_setVRAMTileIndex más abajo).
static void sparksLoadFrame(const SpriteDefinition* def, u16 vramInd,
                            u16 counter, TransferMethod tm) {
    const Animation* anim = def->animations[0];
    const TileSet* ts = anim->frames[counter % anim->numFrame]->tileset;
    VDP_loadTileData(ts->tiles, vramInd, ts->numTile, tm);
}

static void sparksStreamInit(u16 sparksInd, u16 elevInd, bool doorFloorOn) {
    sparksDoorFloorOn = doorFloorOn;
    sparksVramInd    = sparksInd;
    elevSparkVramInd = elevInd;
    sparksFrame = 0;
    sparksTimer = 0;

    if (!doorFloorOn) return;   // 4P: ningun bloque de spark esta reservado
    sparksLoadFrame(&spark_ascensor, elevSparkVramInd, 0, DMA);
    sparksLoadFrame(&sparks,         sparksVramInd,    0, DMA);
}

// Avanza el frame compartido de las 3 variantes. Sin SPR_addSprite de por
// medio: son sprites en modo manual (auto-upload apagado), así que esto es
// la ÚNICA fuente de sus tiles -- pisa los mismos bloques de VRAM con el
// frame siguiente via DMA, igual que fireUpdate/smokeUpdate.
static void sparksStreamUpdate(void) {
    if (++sparksTimer < SPARKS_FRAME_SPEED) return;
    sparksTimer = 0;

    // Ciclo de 21 = minimo comun multiplo de 3 (ascensor) y 7 (puertas,
    // arte del 26/09), asi ninguna pega un salto al dar la vuelta el contador.
    if (++sparksFrame >= 21) sparksFrame = 0;

    if (!sparksDoorFloorOn) return;
    sparksLoadFrame(&spark_ascensor, elevSparkVramInd, sparksFrame, DMA_QUEUE);
    sparksLoadFrame(&sparks,         sparksVramInd,    sparksFrame, DMA_QUEUE);
}

// Crea una instancia de sparks/spark_ascensor/sparks_2 en modo MANUAL: sin
// auto-alloc de VRAM ni auto-upload de tiles, fija al bloque compartido
// `vramInd` (streameado por sparksStreamUpdate). Varias instancias pueden
// apuntar al mismo bloque a la vez (p.ej. 2 puertas visibles juntas) sin
// costo extra de VRAM -- a diferencia de SPR_addSprite normal, que le
// reservaría una copia propia a cada instancia.
static Sprite* sparksAddSprite(const SpriteDefinition* sizeDef, u16 vramInd,
                               s16 x, s16 y) {
    return SPR_addSpriteEx(sizeDef, x, y,
                           TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, vramInd),
                           SPR_FLAG_AUTO_VISIBILITY);
}

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

// --- Dinamita de la ESCALERA (18/09) ---------------------------------------
// Ataque GUIONADO, una sola vez por partida, en la ZONA 2 -- que es justo
// donde la camara queda clavada en 300 y la escalera del fondo (x de mundo
// 505..620 sobre bg01_final.png) ocupa la mitad derecha de la pantalla, igual
// que en el arcade.
//
// El morado se asoma EN la escalera, pegado a la columna azul, y tira UN
// cartucho; despues pelea como cualquier otro. El cartucho cae SIEMPRE en el
// mismo punto: media pantalla, en la lane del medio de la franja caminable
// (142..200), que es donde se pelea.
//
// (18/09, 2da pasada) La posicion y la orientacion salen del montaje de
// referencia. Localizando su sprite pegado dentro de la captura (con la camara
// clavada en 300) da origen de frame en pantalla x=187 y pies en y=132, o sea
// mundo x=487, y SIN espejar.
//
// OJO CON EL ESPEJADO: el resto de las animaciones del morado miran a la
// DERECHA y se espejan con dir=-1, pero la de la dinamita esta dibujada
// mirando a la IZQUIERDA (el guante dorado adelante, a la izquierda; el brazo
// que tira sale hacia atras, arriba a la derecha). Asi que este spawn va con
// dir=+1, SIN espejar. Estaba al reves y por eso el guante quedaba del otro
// lado.
//
// La ALTURA la hace jumpZ, no la lane: la lane sigue siendo ENEMY_LANE_TOP
// (142) para que la IA y las colisiones sean las de siempre, y el sprite se
// dibuja TNT_THROWER_Z px mas arriba, que es lo que lo pone sobre el escalon.
// Al soltar el cartucho el offset baja de a 1px por frame: se "baja" de la
// escalera mientras se recompone.
#define TNT_THROWER_X    487    // x de mundo del frame (pantalla 187 con la camara en 300)
#define TNT_THROWER_Y    ENEMY_LANE_TOP
#define TNT_THROWER_Z     10    // pies dibujados en 132 (142 - 10)
#define TNT_LAND_X       420    // punto FIJO de caida (screen x 120 con la camara en 300)
#define TNT_LAND_Y       165    // lane donde se pelea (142..200). Mas abajo, el
                                // fuego del primer plano tapaba media explosion.
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
// Escalonado entre jugadores en la caminata final. Con 4 jugadores el ultimo
// queda a 3*OUTRO_FAN_X = 72px de la puerta y 3*OUTRO_FAN_Y = 18px mas al
// frente: todos dentro de la pantalla (camara fija en 1056, visible hasta
// 1376) y sin taparse entre si.
#define OUTRO_FAN_X          24
#define OUTRO_FAN_Y           6
#define OUTRO_DOOR_Y        150    // Lane de pies al llegar a la puerta

// ---------------------------------------------------------------------------
// Volumen de audio (0..100) — requiere el driver XGM2 (recursos XGM2 en
// audio.res). El XGM clásico no tiene control de volumen.
// ---------------------------------------------------------------------------
#define VOL_MUSIC_INTRO    90
#define VOL_MUSIC_SELECT   90
#define VOL_MUSIC_LEVEL1    80   // la música del nivel saturaba: bajada al 50%
#define VOL_MUSIC_LEVEL2    80
#define VOL_MUSIC_BOSS      80   // tema del jefe (Rocksteady)
// --- Ducking del tema del jefe mientras Rocksteady habla --------------------
// El voice over "SAY YOUR PRAYERS!" (say_your_p_sfx, PCM) arranca EXACTAMENTE
// en el mismo tick que music_boss, y aunque el PCM va en CH2 con prioridad 15
// (le gana al PCM de la musica), los canales FM del tema siguen sonando por
// debajo y se lo comen. Dos arreglos, los dos hacen falta:
//   1. res/audio/say_your_p.wav estaba grabado ~3x mas bajo que el resto de los
//      voice overs (pico 41%, RMS 7,4% contra ~100% / 23% de hang_on_april y
//      shredder_laugh). Se normalizo; el original quedo en say_your_p_orig.wav.
//   2. El tema entra BAJO (VOL_MUSIC_BOSS_DUCK) y recien sube al volumen normal
//      cuando el wav termino, con una rampa para que no sea un salto.
// El wav dura 1,76s (23552 bytes al rate del driver XGM2) = ~106 frames NTSC.
// (01/10) El ducking se reemplazo por boss_vo.c: el tema NO arranca hasta que
// la voz termino (y la voz va sola en CH1). VOL_MUSIC_BOSS_DUCK queda para el
// bajon del tema cuando muere el jefe.
#define VOL_MUSIC_BOSS_DUCK 20   // volumen del tema al morir el jefe
// Aire entre el tema del jefe y el de la cutscene de Shredder: 1 segundo justo
// (NTSC/PAL). Sin esto, el play de music_ending pisaba a music_boss en seco.
#define CUT_SILENCE_FRAMES  (IS_PAL_SYSTEM ? 50 : 60)
#define VOL_MUSIC_CREDITS  90
#define VOL_MUSIC_ENDING    80
// Corte de respaldo manual para "07 - April is Kidnapped (Cutscene).vgm"
// (music_ending), mismo problema y misma tecnica que music_intro_arcade en
// intro_arcade.c (ver el comentario grande ahi -- XGM2_setLoopNumber(0) va
// SIEMPRE antes del play, nunca despues: el driver latchea el loop en el
// instante del play, y llamarlo despues es un no-op silencioso. Bug
// encontrado 30/08: en showScene12 estaba al reves). Duracion medida del
// propio header VGM (campo "total # samples": 357945 @ 44100Hz = 487
// frames NTSC exactos, ~406 PAL) -- este es el punto real donde el archivo
// terminaria y volveria a su punto de loop. Un par de frames de margen
// para cortar ANTES de que se note.
#define MUSIC_ENDING_LEN (IS_PAL_SYSTEM ? 404 : 485)
#define VOL_SFX            100

// ---------------------------------------------------------------------------
// Estado global de selección (necesario entre escenas)
// ---------------------------------------------------------------------------
u8 personajeSeleccionado  = 0;  // P1: 0=Leo 1=Mike 2=Don 3=Raph (columnas de pantalla)
u8 personaje2Seleccionado = 3;  // P2: 0=Leo 1=Mike 2=Don 3=Raph (columnas de pantalla)
// (14/09) Modo secreto de CUATRO tortugas: cantidadJugadores puede valer 1..4.
// Con 4 no se pasa por la seleccion de personaje -- cada joystick tiene su
// tortuga fija (P1 Leo, P2 Mike, P3 Don, P4 Raph, ver showPlayerSelect).
u8 cantidadJugadores      = 1;  // 1..4 jugadores
u8 personaje3Seleccionado = 2;  // P3: Don  (fijo, solo se usa en modo 4P)
u8 personaje4Seleccionado = 3;  // P4: Raph (fijo, solo se usa en modo 4P)

// ---------------------------------------------------------------------------
// Joystick de cada jugador (indice 0..3)
// ---------------------------------------------------------------------------
// (15/09) ACA ESTABA EL BUG del modo de 4: el jugador 2 no se movia.
// Con un MULTITAP, SGDK **NO** numera los mandos JOY_1..JOY_4. Su tabla de
// traduccion (xlt_all en src/joy.c de SGDK v2.11, y la misma en
// readEa4WayPlay) es:
//
//     multitap en el PUERTO 1 -> JOY_1, JOY_3, JOY_4, JOY_5
//     multitap en el PUERTO 2 -> JOY_2, JOY_6, JOY_7, JOY_8
//
// JOY_2 queda RESERVADO para el mando directo del puerto 2. O sea que con el
// tap en el puerto 1 y el mapeo ingenuo JOY_1..JOY_4 pasaba exactamente lo que
// pasaba en las pruebas: el jugador 2 (JOY_2) no respondia NUNCA, y los jugadores 3 y
// 4 en realidad estaban leyendo los pads 2 y 3 del tap -- el pad 4 (JOY_5) no
// lo leia nadie. Tres se movian y uno no.
//
// Vale igual para el Sega Tap / TeamPlayer (PORT_TYPE_TEAMPLAYER) y para el EA
// 4-Way Play (PORT_TYPE_EA4WAYPLAY): los dos usan la misma tabla.
u16 playerJoy(u8 k) {
    static const u16 tap1[MAX_PLAYERS] = { JOY_1, JOY_3, JOY_4, JOY_5 };
    static const u16 tap2[MAX_PLAYERS] = { JOY_2, JOY_6, JOY_7, JOY_8 };
    static const u16 pads[MAX_PLAYERS] = { JOY_1, JOY_2, JOY_7, JOY_8 };

    if (k >= MAX_PLAYERS) k = MAX_PLAYERS - 1;

    // (17/09) Ya no se elige UNA tabla y listo: se arma la lista de indices que
    // PUEDEN tener mando y despues se descartan los que no lo tienen. Asi da
    // igual si quedo un multitap enchufado de cuando se probo el modo de 4:
    // los jugadores caen sobre los mandos que hay, en orden.
    u16 cand[8];
    u8  nc = 0;
    u8  t1 = JOY_getPortType(PORT_1);
    u8  t2 = JOY_getPortType(PORT_2);

    if (t1 == PORT_TYPE_TEAMPLAYER || t1 == PORT_TYPE_EA4WAYPLAY)
        for (u8 i = 0; i < MAX_PLAYERS; i++) cand[nc++] = tap1[i];
    else
        cand[nc++] = JOY_1;

    // El EA 4-Way Play ocupa LOS DOS puertos: no hay un "puerto 2" aparte.
    if (t1 != PORT_TYPE_EA4WAYPLAY) {
        if (t2 == PORT_TYPE_TEAMPLAYER)
            for (u8 i = 0; i < MAX_PLAYERS; i++) cand[nc++] = tap2[i];
        else
            cand[nc++] = JOY_2;
    }

    u16 vivos[8];
    u8  n = 0;
    for (u8 i = 0; i < nc; i++) {
        u8 tp = JOY_getJoypadType(cand[i]);
        if (tp == JOY_TYPE_PAD3 || tp == JOY_TYPE_PAD6) vivos[n++] = cand[i];
    }

    if (k < n)  return vivos[k];   // caso normal: mandos realmente enchufados
    if (k < nc) return cand[k];    // red de seguridad, por si la deteccion falla
    return pads[k];
}

// Personaje de cada jugador (indice 0..3).
u8 playerChar(u8 k) {
    switch (k) {
        case 1:  return personaje2Seleccionado;
        case 2:  return personaje3Seleccionado;
        case 3:  return personaje4Seleccionado;
        default: return personajeSeleccionado;
    }
}

// Cantidad de jugadores CLAMPEADA al maximo soportado.
u8 numJugadores(void) {
    u8 n = cantidadJugadores;
    if (n < 1) n = 1;
    if (n > MAX_PLAYERS) n = MAX_PLAYERS;
    return n;
}

// Continues disponibles en la partida (compartidos entre ambos jugadores).
// Se consumen al continuar; se reinician en la selección de personajes.
static u8 continuesLeft = 3;

// ---------------------------------------------------------------------------
// clearScene — limpieza completa entre escenas
// keepAudio = TRUE: no detiene la música (para transiciones con música continua,
//                    p.ej. nivel 2 → ending).
// ---------------------------------------------------------------------------
static void hudForgetSprites(void);

void clearSceneEx(bool keepAudio) {
    PAL_fadeOutAll(20, FALSE);
    while(PAL_isDoingFade()) {
        SYS_doVBlankProcess();
    }
    if (!keepAudio) XGM2_stop();
    SPR_reset();
    hudForgetSprites();   // (27/09) SPR_reset ya los libero: olvidar los punteros
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
    // (29/09) Sin plano WINDOW (la cloaca lo usa para el HUD con la camara
    // vertical): VPos 0 arriba = apagado.
    VDP_setWindowVPos(FALSE, 0);
    VDP_setWindowHPos(FALSE, 0);
    VDP_setBackgroundColor(0);
    hudSetPlane(BG_A);
    SYS_doVBlankProcess();
}

// ---------------------------------------------------------------------------
// Helper — reproducir un track XGM2 con volumen (FM + PSG en 0..100)
// ---------------------------------------------------------------------------
// Compartida con las escenas que viven en sus propios modulos (declarada en
// scenes.h). Antes era static; dejo de serlo cuando scene_profiles.c la necesito.
void playMusicVol(const u8* track, u16 vol) {
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
// lanes hasta salir por abajo (como en el arcade). Si toca a un jugador le
// resta 1 barra de vida (via damagePlayer, con sus i-frames -> un solo golpe
// por pasada). NO daña a los foot soldiers (30/08: antes
// los aplastaba; ver nota en ironBallUpdate).
//
// Dos bolas (30/08): MISMO arco (misma físca, misma
// escalera) pero cadencia distinta (IRON_BALL_PERIOD vs IRON_BALL_PERIOD2),
// para que no caigan siempre sincronizadas. Antes había una sola instancia
// como variable global; ahora las funciones toman un puntero a IronBall y
// se llaman una vez por bola (mismo patrón que robot/robot2 en showScene11).
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
#define IRON_BALL_PERIOD     180   // frames entre bolas de la 1ra (~6s a 60fps)
#define IRON_BALL_PERIOD2    130   // cadencia de la 2da bola (~2.2s a 60fps) --
                                   // distinta a propósito de IRON_BALL_PERIOD
                                   // para que no caigan siempre juntas.
// La bola SIEMPRE baja por la escalera del nivel (X de mundo FIJA, medida sobre
// bg01_completa.png: la escalera ocupa ~508..620). Nace arriba de todo y rueda
// en diagonal hacia el frente-derecha (ROLL>0), como en el arcade. Las DOS
// bolas comparten el mismo arco: misma escalera, misma física.
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

typedef struct {
    Sprite* sprite;
    s16     x;       // mundo, centro
    s16     y;       // línea de contacto (lane/pies)
    s16     z;       // altura del rebote (>= 0)
    s16     vz;      // velocidad vertical del rebote (+ = subiendo)
    bool    active;
    u16     timer;   // frames hasta el próximo spawn
    u16     period;  // cadencia de ESTA bola (IRON_BALL_PERIOD o _PERIOD2)
} IronBall;

static IronBall ironBall;
static IronBall ironBall2;

// Crea el sprite (oculto) UNA vez al iniciar el nivel. Usa PAL1 (tortugas), que
// ya cargó initPlayer -> llamar DESPUÉS de initPlayer, una vez por bola.
static void ironBallInit(IronBall* b, u16 period) {
    b->sprite = SPR_addSprite(&iron_ball, -IRON_BALL_SIZE, -IRON_BALL_SIZE,
                              TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
    if (b->sprite) {
        SPR_setAnim(b->sprite, 0);            // 2 frames girando (auto-anim)
        SPR_setVisibility(b->sprite, HIDDEN);
    }
    b->active = FALSE;
    b->period = period;
    b->timer  = period;
}

// TRUE si la bola (activa) golpea un objetivo con centro X 'cx' (mundo) y pies
// 'cfy'. Mide en profundidad + X; ignora la altura del rebote (z).
static bool ironBallHits(const IronBall* b, s16 cx, s16 cfy) {
    s16 dx = cx - b->x;  if (dx < 0) dx = -dx;
    s16 dy = cfy - b->y; if (dy < 0) dy = -dy;
    return (dx <= IRON_BALL_HIT_X && dy <= IRON_BALL_HIT_Y);
}

// Física + colisiones + render de UNA bola. Llamar una vez por frame por cada
// instancia (ironBall, ironBall2) en el nivel.
static void ironBallUpdate(IronBall* b, s16 cameraX, Player** pls, u8 nPl) {
    if (!b->sprite) return;

    // --- Spawn periódico desde la ESCALERA (una bola activa a la vez, POR
    //     INSTANCIA -- las dos bolas son independientes entre sí) ---
    if (!b->active) {
        if (b->timer > 0) b->timer--;
        if (b->timer == 0) {
            b->timer = b->period;   // reengancha el próximo ciclo (cadencia propia)
            // Solo cae si el alto de la escalera esta a la vista: la bola baja
            // SIEMPRE por esa escalera (X de mundo fija), no en lugares random.
            s16 stairScreenX = IRON_BALL_STAIRS_X - cameraX;
            if (stairScreenX >= IRON_BALL_ONSCREEN_MARGIN &&
                stairScreenX <= SCREEN_PIXEL_WIDTH - IRON_BALL_ONSCREEN_MARGIN) {
                b->x      = IRON_BALL_STAIRS_X;
                b->y      = IRON_BALL_START_Y;
                b->z      = 0;
                b->vz     = IRON_BALL_BOUNCE;   // arranca rebotando
                b->active = TRUE;
                SPR_setVisibility(b->sprite, VISIBLE);
            }
        }
        if (!b->active) return;
    }

    // --- Rebote vertical (z) sobre un "escalón" en z=0 ---
    b->z  += b->vz;
    b->vz -= IRON_BALL_GRAVITY;
    if (b->z <= 0) {
        b->z  = 0;
        b->vz = IRON_BALL_BOUNCE;
        XGM2_playPCMEx(iron_ball_sfx, sizeof(iron_ball_sfx), SOUND_PCM_CH3, 15, FALSE, FALSE);
    }

    // --- Descenso por la escalera + deriva horizontal ---
    b->y += IRON_BALL_FALL_SPEED;
    b->x += IRON_BALL_ROLL;

    // --- ¿Salió por abajo? -> apagar y esperar al próximo ciclo ---
    if (b->y >= IRON_BALL_EXIT_Y) {
        b->active = FALSE;
        SPR_setVisibility(b->sprite, HIDDEN);
        return;
    }

    // --- Colisiones ---
    // Jugador: 1 barra por pasada (los i-frames de damagePlayer evitan el
    // multi-golpe). attackerX = centro de la bola -> knockback alejándose.
    // NO daña a los foot soldiers (30/08 -- antes los
    // aplastaba con damageEnemy/IRON_BALL_ENEMY_DMG; se sacó esa colisión
    // por completo, la bola les pasa por encima sin efecto).
    for (u8 k = 0; k < nPl; k++) {
        s16 pcx = getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2;
        if (!playerCanBeHit(pls[k])) continue;
        if (!ironBallHits(b, pcx, getPlayerY(pls[k]))) continue;
        XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
        damagePlayer(pls[k], b->x);
    }

    // --- Render: pantalla = mundo - cámara; el rebote (z) sube el dibujo ---
    SPR_setPosition(b->sprite,
                    b->x - cameraX - IRON_BALL_HALF,
                    b->y - b->z - IRON_BALL_SIZE);
    SPR_setDepth(b->sprite, -(b->y));   // Y-sorting por profundidad
}

// Oculta la bola al terminar el nivel (que no quede congelada en la cutscene).
static void ironBallEnd(IronBall* b) {
    b->active = FALSE;
    if (b->sprite) SPR_setVisibility(b->sprite, HIDDEN);
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
// (las medidas del marco y los retratos viven ahora en src/hud.h)

static Sprite* hudSprite1    = NULL;
static Sprite* hudSprite2    = NULL;
static Sprite* portraitSpr1  = NULL;
static Sprite* portraitSpr2  = NULL;
// Modo 4 jugadores: cuatro marcos, sin retratos (ver hud.h).
static Sprite* hud4Spr[MAX_PLAYERS] = { NULL, NULL, NULL, NULL };
// El spritesheet de los marcos tiene las filas en orden Leo/Mike/Don/Raph del
// ARTE (0=Leo 1=Mike(rojo) 2=Don(purpura) 3=Raph(dorado)) y el cursor del
// juego usa 0=Leo 1=Mike 2=Don 3=Raph: esta tabla traduce.
static const u8 hudAnimForChar[] = { 0, 3, 2, 1 };

// (27/09) CUELGUE "ILLEGAL INSTRUCTION" en el 2-1 tras un continue.
// Estos punteros son STATIC y sobreviven a la escena, pero sus sprites NO:
// SPR_reset (clearScene) y SPR_initEx sueltan TODO el pool de sprites. La
// seleccion de personaje crea los retratos (hudInitPortraits) y nunca los
// soltaba a mano, asi que portraitSpr1 (y el 2 en 2P) quedaban apuntando al
// pool VIEJO. En el primer continue de la partida, hudPortraitShow(0) veia
// el puntero "no NULL", no creaba nada y hacia SPR_setAnim sobre memoria que
// ahora es de OTRO sprite; al confirmar, hudPortraitHide(0) hacia
// SPR_releaseSprite de esa direccion: soltaba un sprite ajeno (p.ej. el de la
// tortuga -> "el P1 desaparecio") o metia una direccion corrida en el pool.
// Despues dos objetos compartian el mismo Sprite, uno le pisaba los campos al
// otro y SPR_update terminaba saltando a un onFrameChange basura.
// Regla: todo Sprite* static se olvida (NULL) cuando se resetea el motor.
static bool hudFront = FALSE;       // ver hudFramesToFront

static void hudForgetSprites(void) {
    hudFront     = FALSE;
    hudSprite1   = NULL;
    hudSprite2   = NULL;
    portraitSpr1 = NULL;
    portraitSpr2 = NULL;
    for (u8 k = 0; k < MAX_PLAYERS; k++) hud4Spr[k] = NULL;
}

// Crea los marcos del HUD y los retratos de tortuga como sprites de alto
// nivel. En 1 jugador solo se crean los de P1. No consume VRAM de planos
// (los tiles viven en el area de sprites del motor, SPR_initEx). Los sprites
// se liberan solos en clearScene (SPR_reset) al terminar la escena.
// Columna de tile donde arranca el HUD del jugador k. Con 1-2 jugadores son
// las de siempre (los marcos); con 3-4, cuatro bloques pelados de 10 columnas.
u16 hudPlayerCol(u8 k) {
    if (numJugadores() > 2) return (u16)(HUD4_BASECOL0 + k * HUD4_BLOCK_COLS);
    return (k == 0) ? HUD_P1_BASECOL : HUD_P2_BASECOL;
}

void hudInit(void) {
    // hudInit va siempre despues del SPR_initEx de la escena: cualquier
    // puntero que quedara de antes es invalido (ver hudForgetSprites).
    hudForgetSprites();
    // (17/09) Los marcos bajan HUD_FRAME_Y (4px) para que el interior caiga
    // sobre la grilla de tiles -- ver la nota larga en hud.h -- y los RETRATOS
    // ya no se crean: el HUD de partida va limpio, como el arcade. Vuelven a
    // aparecer solo mientras alguien elige tortuga (continue o entrada del P2).
    //
    // MODO 4P (16/09): los cuatro marcos, pegados y centrados (x = 16/88/
    // 160/232). La fila de color la elige el personaje (hudAnimForChar:
    // 0=Leo 1=Mike 2=Don 3=Raph -> filas 0/3/2/1 del PNG).
    if (numJugadores() > 2) {
        static const u8 hudAnimForChar4[] = { 0, 3, 2, 1 };
        const SpriteDefinition* const marco[MAX_PLAYERS] =
            { &hud_1p, &hud_2p, &hud_3p, &hud_4p };
        for (u8 k = 0; k < MAX_PLAYERS; k++) {
            hud4Spr[k] = SPR_addSprite(marco[k], (s16)(HUD4_X0 + k * (HUD_TILE_W * 8)),
                                       HUD_FRAME_Y, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
            if (hud4Spr[k])
                SPR_setAnim(hud4Spr[k], hudAnimForChar4[playerChar(k) & 3]);
        }
        return;
    }

    hudSprite1 = SPR_addSprite(&hud_1p, HUD_P1_X, HUD_FRAME_Y,
                               TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    if (hudSprite1)
        SPR_setAnim(hudSprite1, personajeSeleccionado);

    // El marco del P2 se crea SIEMPRE (17/09): con dos jugadores es el suyo, y
    // con uno queda vacio para invitarlo a entrar ("PULSE / START", ver
    // p2JoinPoll). Son 36 tiles de sprite que en 1-2 jugadores sobran.
    hudSprite2 = SPR_addSprite(&hud_2p, HUD_P2_X, HUD_FRAME_Y,
                               TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    if (hudSprite2)
        SPR_setAnim(hudSprite2, (cantidadJugadores == 2) ? personaje2Seleccionado : 0);
}

// Retratos a pedido (17/09). Se crean solo mientras un jugador esta ELIGIENDO
// tortuga -- al continuar, o cuando el P2 se suma en plena partida -- y se
// sueltan al confirmar. Cuestan 16 tiles de sprite cada uno, que asi no se
// pagan durante toda la partida.
static Sprite* hudPortraitShow(u8 k, u8 ch) {
    Sprite** slot = (k == 0) ? &portraitSpr1 : &portraitSpr2;
    if (!*slot)
        *slot = SPR_addSprite(&turtle_portrait,
                              (k == 0) ? PORTRAIT_P1_X : PORTRAIT_P2_X, PORTRAIT_Y,
                              TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    if (*slot) {
        SPR_setAnim(*slot, ch);
        if (hudFront) SPR_setDepth(*slot, SPR_MIN_DEPTH);
    }
    return *slot;
}

void hudFramesToFront(void) {
    hudFront = TRUE;
    Sprite* s[] = { hudSprite1, hudSprite2, portraitSpr1, portraitSpr2,
                    hud4Spr[0], hud4Spr[1], hud4Spr[2], hud4Spr[3] };
    for (u8 i = 0; i < 8; i++)
        if (s[i]) SPR_setDepth(s[i], SPR_MIN_DEPTH);
}
static void hudPortraitHide(u8 k) {
    Sprite** slot = (k == 0) ? &portraitSpr1 : &portraitSpr2;
    if (*slot) { SPR_releaseSprite(*slot); *slot = NULL; }
}

// La pantalla de SELECCION DE PERSONAJE si quiere los retratos fijos: son su
// razon de ser. Se llama despues de hudInit().
void hudInitPortraits(void) {
    hudPortraitShow(0, personajeSeleccionado);
    if (cantidadJugadores == 2) hudPortraitShow(1, personaje2Seleccionado);
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
// Posiciones (en tiles) RELATIVAS a la columna donde arranca el marco.
// El interior útil es cols 1..7 (col 0 y col 8 son borde del marco).
#define HUD_SCORE_ROW   1   // fila superior; el puntaje se alinea a la derecha
#define HUD_SCORE_LEFT  4   // primera col libre de la fila superior (tras "1UP")
#define HUD_SCORE_RIGHT 8   // borde derecho (col 8); el puntaje termina en col 7
#define HUD_LIVES_COL   3   // (17/09) pegado a la barra, como el arcade
#define HUD_LIVES_ROW   2   // el dígito es de 2 filas: ocupa 2 y 3
#define HUD_BAR_COL     4
#define HUD_BAR_ROW     2   // la barra es de 2 filas: ocupa 2 y 3

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
// pisando los HPBAR_FRAME_TILES tiles del bloque. Los tiles de cada frame
// están contiguos y sin deduplicar en ROM (NONE NONE): frame N arranca en el
// tile N*HPBAR_FRAME_TILES -> N*HPBAR_FRAME_TILES*8 longwords.
// DMA_QUEUE: la transferencia se hace en el próximo vblank.
static void hpBarSetFrame(u16 barVram, u8 frame) {
    VDP_loadTileData(hp_bar.tiles + (u32)frame * HPBAR_FRAME_TILES * 8,
                     barVram, HPBAR_FRAME_TILES, DMA_QUEUE);
}

// Inicializa el bloque de barra de un jugador: carga el frame lleno a VRAM y
// dibuja su tilemap 4x2 en BG_A (prioridad alta, PAL1) dentro del marco.
// (26/09) Plano del HUD: BG_A salvo en los niveles con capa lejana (freeway),
// donde BG_A es la ruta y el HUD va en BG_B (prioridad alta, filas 0-3 fijas).
static VDPPlane hudPlane = BG_A;
void hudSetPlane(VDPPlane plane) { hudPlane = plane; }

static void hpBarInit(u16 barVram, u16 baseCol) {
    VDP_loadTileData(hp_bar.tiles, barVram, HPBAR_FRAME_TILES, DMA);
    // fillTileMapRectInc incrementa el índice tile a tile (fila por fila), el
    // mismo orden en que quedan los tiles de cada frame en el tileset.
    VDP_fillTileMapRectInc(hudPlane,
                           TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, barVram),
                           baseCol + HUD_BAR_COL, HUD_BAR_ROW,
                           HPBAR_FRAME_TILES_W, HPBAR_FRAME_TILES_H);
}

// Vidas: dígito de 8x16, mismo streaming que la barra. (26/09) En el color
// de la bandana de cada tortuga (lives_digits_turtles, ver level1.res).
static const u32* hudLivesTiles(u8 ch, u8 d) {
    if (d > 9) d = 9;
    if (ch > 3) return lives_digits.tiles + (u32)d * HUDLIVES_TILES * 8;
    return lives_digits_turtles.tiles + (u32)(ch * 10 + d) * HUDLIVES_TILES * 8;
}
static void hudLivesSetDigit(u16 vram, u8 ch, u8 d) {
    VDP_loadTileData(hudLivesTiles(ch, d), vram, HUDLIVES_TILES, DMA_QUEUE);
}
static void hudLivesInit(u16 vram, u16 baseCol, u8 ch) {
    VDP_loadTileData(hudLivesTiles(ch, 0), vram, HUDLIVES_TILES, DMA);
    VDP_fillTileMapRectInc(hudPlane,
                           TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, vram),
                           baseCol + HUD_LIVES_COL, HUD_LIVES_ROW,
                           HUDLIVES_TILES_W, HUDLIVES_TILES_H);
}

// Borra el contenido dinámico del HUD de un bloque (barra + vidas + puntaje).
// Lo usa el marco vacío del P2 en 1 jugador.
static void hudClearBlock(u16 baseCol) {
    VDP_clearTileMapRect(hudPlane, baseCol + 1, HUD_SCORE_ROW, HUD_TILE_W - 2, 3);
}

// Prepara el HUD de un jugador. Llamar DESPUÉS de initPlayer (PAL1 cargada) y
// de fijar paleta/plano de texto. Fuerza el primer dibujado de cada elemento.
void hudPlayerInit(HudPlayer* h, Player* pl, u16 baseCol, u16 barVram) {
    h->pl         = pl;
    h->baseCol    = baseCol;
    h->barVram    = barVram;
    h->livesVram  = (u16)(barVram + HPBAR_FRAME_TILES);
    h->lastHealth = -1;   // -1 = fuerza el primer redibujo
    h->lastLives  = -1;
    h->lastScore  = -1;
    hudClearBlock(baseCol);
    hpBarInit(barVram, baseCol);
    hudLivesInit(h->livesVram, baseCol, pl->charIndex);
}

// Redibuja SOLO los elementos que cambiaron. Llamar una vez por frame.
void hudPlayerUpdate(HudPlayer* h) {
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

    // (17/09) Las vidas son un DÍGITO pegado a la barra, como el arcade
    // (antes era un "x3" con la fuente del HUD). El tope de OPCIONES es 7, así
    // que un dígito alcanza y sobra.
    if (lives != h->lastLives) {
        hudLivesSetDigit(h->livesVram, h->pl->charIndex,
                         (u8)((lives < 0) ? 0 : lives));
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

// ContState / ContPlayer viven en hud.h (26/09: el 2-1 tambien los usa).

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
    // (26/09) Donde cayo puede haber quedado fuera de camara (el companero
    // siguio avanzando mientras estaba fuera): se lo trae adentro de los
    // bordes que el nivel le fijo este frame.
    s16 x = p->x;
    if (x < p->boundLeft)  x = p->boundLeft;
    if (x > p->boundRight) x = p->boundRight;
    // (01/10) initPlayer pone el lane y la pared diagonal del NIVEL 1 (los
    // demas niveles los fijan una sola vez, al arrancar, con setPlayerLane /
    // setPlayerEndWall). Al continuar en otro nivel el jugador quedaba
    // encerrado en la franja y la pared del 1-1: se guardan los limites que
    // tenia y se reponen despues de reinicializarlo.
    const s16 laneTop = p->laneTop, laneBottom = p->laneBottom;
    const s16 wallTop = p->wallXTop, wallBottom = p->wallXBottom;
    const s16 bLeft = p->boundLeft, bRight = p->boundRight;
    const s16 camOff = p->cameraOffsetX;
    if (p->sprite) SPR_releaseSprite(p->sprite);
    playerPersistClearOut(p);   // (01/10) si venia fuera del nivel anterior
    initPlayer(p, ch, joyId, PAL1, x, p->y);
    setPlayerLane(p, laneTop, laneBottom);
    setPlayerEndWall(p, wallTop, wallBottom);
    setPlayerLeftBound(p, bLeft);
    setPlayerRightBound(p, bRight);
    setPlayerCamera(p, camOff);
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
// Helpers del continue por jugador (14/09, para soportar 1..4).
// Con 3-4 jugadores NO hay marcos ni retratos (ver hudInit): se pasan NULL, y
// continuePoll ya los tiene guardados contra NULL.
static Sprite* contFrameSpr(u8 k) {
    if (numJugadores() > 2) return NULL;
    return (k == 0) ? hudSprite1 : hudSprite2;
}
static u8* contCharSel(u8 k) {
    switch (k) {
        case 1:  return &personaje2Seleccionado;
        case 2:  return &personaje3Seleccionado;
        case 3:  return &personaje4Seleccionado;
        default: return &personajeSeleccionado;
    }
}
// Personaje que NO se puede elegir al continuar (el del companero). Con mas de
// dos jugadores no tiene sentido bloquear uno solo, asi que no se bloquea
// ninguno (0xFF).
static u8 contOtherChar(u8 k, u8 nPl) {
    if (nPl != 2) return 0xFF;
    return (k == 0) ? personaje2Seleccionado : personajeSeleccionado;
}

// 'k' es el indice de jugador: hace falta para crear/soltar su retrato, que
// (17/09) solo existe mientras esta eligiendo tortuga.
static bool continuePoll(ContPlayer* c, Player* p, HudPlayer* h, Sprite* frameSpr,
                         u8 k, u16 joyId, u8* charSel, u8 otherChar, u16 fps) {
    const bool conRetrato = (numJugadores() <= 2);
    // Vivo: nada que hacer (y limpiar si quedó texto de un continue previo).
    if (!isPlayerGameOver(p)) {
        if (c->state != CONT_NONE) {
            contDrawText(h, -1);
            if (conRetrato) hudPortraitHide(k);
            c->state = CONT_NONE;
        }
        return FALSE;
    }

    u16 joy = JOY_readJoypad(joyId);

    // (01/10) SIN continues (o si ya venia fuera del nivel anterior) no hay
    // cuenta: queda fuera directamente y no se muestra nada. Antes el
    // "CONTINUE?" aparecia igual aunque no quedara ningun continue.
    if (c->state == CONT_NONE && (continuesLeft == 0 || p->outCarried)) {
        if (p->sprite) SPR_setVisibility(p->sprite, HIDDEN);
        c->state   = CONT_OUT;
        c->prevJoy = joy;
        return TRUE;
    }

    // Arranca la cuenta cuando el jugador acaba de caer.
    if (c->state == CONT_NONE) {
        c->state   = CONT_COUNTING;
        c->seconds = CONT_START_SECONDS;
        c->tick    = 0;
        c->prevJoy = 0;
        contDrawText(h, (s8)c->seconds);
        return FALSE;
    }

    if (c->state == CONT_OUT) {
        // Fuera de juego y sin sprite. START con continues lo vuelve a meter.
        if (continuesLeft > 0 && justPressedJoy(joy, c->prevJoy, BUTTON_START)) {
            continuesLeft--;
            c->state   = CONT_SELECTING;
            c->sel     = *charSel;
            c->prevJoy = joy;
            if (conRetrato) hudPortraitShow(k, c->sel);
            return FALSE;
        }
        c->prevJoy = joy;
        return TRUE;
    }

    if (c->state == CONT_COUNTING) {
        // (01/10) El companero gasto el ultimo continue mientras este contaba:
        // se corta la cuenta y queda fuera.
        if (continuesLeft == 0) {
            contDrawText(h, -1);
            if (p->sprite) SPR_setVisibility(p->sprite, HIDDEN);
            c->state   = CONT_OUT;
            c->prevJoy = joy;
            return TRUE;
        }
        // START del joystick del muerto + continues disponibles -> selección.
        if (continuesLeft > 0 && justPressedJoy(joy, c->prevJoy, BUTTON_START)) {
            continuesLeft--;
            c->state   = CONT_SELECTING;
            c->sel     = *charSel;
            c->prevJoy = joy;
            contDrawText(h, -1);
            if (conRetrato) hudPortraitShow(k, c->sel);
            return FALSE;
        }
        c->tick++;
        if (c->tick >= fps) {
            c->tick = 0;
            if (c->seconds > 0) {
                c->seconds--;
                contDrawText(h, (s8)c->seconds);
            } else {
                // Se mostró el 0 un segundo completo: quedó fuera. (26/09)
                // La tortuga tirada DESAPARECE (antes quedaba en pantalla
                // mientras el otro seguia jugando). Se oculta en vez de
                // soltarla: revivePlayer ya suelta y recrea el sprite si
                // despues vuelve a entrar.
                contDrawText(h, -1);
                if (p->sprite) SPR_setVisibility(p->sprite, HIDDEN);
                c->state   = CONT_OUT;
                c->prevJoy = joy;
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
    if (conRetrato) hudPortraitShow(k, c->sel);

    if (justPressedJoy(joy, c->prevJoy, BUTTON_START)) {
        *charSel = c->sel;
        if (frameSpr) SPR_setAnim(frameSpr, hudAnimForChar[c->sel & 3]);
        if (conRetrato) hudPortraitHide(k);
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

// Extremos en X (borde del frame) de los jugadores EN JUEGO, para la camara.
// Si no queda ninguno, los del jugador 1 (el nivel esta por terminar).
static void playersSpanX(Player** pls, u8 nPl, s16* lead, s16* trail) {
    bool any = FALSE;
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k])) continue;
        s16 xk = getPlayerWorldX(pls[k]);
        if (!any || xk > *lead)  *lead  = xk;
        if (!any || xk < *trail) *trail = xk;
        any = TRUE;
    }
    if (!any) *lead = *trail = getPlayerWorldX(pls[0]);
}

void contResetAll(ContPlayer* conts) {
    for (u8 k = 0; k < MAX_PLAYERS; k++) {
        ContPlayer cz = { CONT_NONE, 0, 0, 0, 0 };
        conts[k] = cz;
    }
}

bool continueStepAll(ContPlayer* conts, Player** pls, HudPlayer* huds,
                     u8 nPl, u16 fps) {
    bool allOut = TRUE;
    for (u8 k = 0; k < nPl; k++) {
        if (!continuePoll(&conts[k], pls[k], &huds[k], contFrameSpr(k), k,
                          playerJoy(k), contCharSel(k),
                          contOtherChar(k, nPl), fps))
            allOut = FALSE;
    }
    return allOut;
}

// ===========================================================================
// ENTRADA DEL PLAYER 2 EN PLENA PARTIDA (17/09)
// ===========================================================================
// Con UN jugador, el marco del P2 igual se dibuja (ver hudInit) y adentro
// parpadea "PULSE / START" -- dos lineas porque el interior son 7 columnas y
// "PULSE START" son 11 caracteres. Si el joystick 2 pulsa START, ese mismo
// marco se vuelve un mini selector de tortuga: izquierda/derecha cambian el
// retrato (que aparece en el borde derecho, el hueco que quedo libre al
// sacarlos del HUD) saltandose la tortuga del P1, y START confirma.
//
// El nivel NO se pausa: el P1 sigue jugando mientras el P2 elige, como en el
// arcade. La escena es la que decide donde aparece el jugador nuevo, porque
// cada nivel tiene sus propios limites; p2JoinPoll solo devuelve el personaje
// elegido el frame en que se confirma.
// ---------------------------------------------------------------------------
#define P2J_BLINK_FRAMES  24   // medio ciclo del parpadeo de la invitacion

typedef enum { P2J_INVITE, P2J_SELECTING, P2J_DONE } P2JoinState;

static P2JoinState p2jState;
static u8   p2jSel;
static u16  p2jPrevJoy;
static u16  p2jBlink;
static bool p2jTextOn;

void p2JoinReset(void) {
    p2jState   = P2J_INVITE;
    p2jSel     = 0;
    p2jPrevJoy = 0;
    p2jBlink   = 0;
    p2jTextOn  = FALSE;
}

// Las dos lineas van en las filas 2 y 3 -- las de la barra -- y NO en la 1:
// a la altura de la fila 1 el arte del marco todavia tiene la pestana del
// "2UP" sobre las columnas 1-2 y se comia media palabra (probado).
static void p2DrawInvite(u16 baseCol, bool on) {
    VDP_clearText(baseCol + 1, HUD_BAR_ROW,     HUD_TILE_W - 2);
    VDP_clearText(baseCol + 1, HUD_BAR_ROW + 1, HUD_TILE_W - 2);
    if (!on) return;
    VDP_drawText("PULSE", baseCol + 2, HUD_BAR_ROW);
    VDP_drawText("START", baseCol + 2, HUD_BAR_ROW + 1);
}

u8 p2JoinPoll(u16 baseCol) {
    u16 joy = JOY_readJoypad(playerJoy(1));

    if (p2jState == P2J_INVITE) {
        if (++p2jBlink >= P2J_BLINK_FRAMES) {
            p2jBlink  = 0;
            p2jTextOn = !p2jTextOn;
            p2DrawInvite(baseCol, p2jTextOn);
        }
        if (justPressedJoy(joy, p2jPrevJoy, BUTTON_START)) {
            // Arranca en la primera tortuga libre (la del P1 no se puede).
            p2jSel    = (u8)charMove((s8)personajeSeleccionado,
                                     (s8)personajeSeleccionado, +1);
            p2jState  = P2J_SELECTING;
            p2DrawInvite(baseCol, FALSE);
            hudPortraitShow(1, p2jSel);
            if (hudSprite2) SPR_setAnim(hudSprite2, hudAnimForChar[p2jSel & 3]);
        }
    } else if (p2jState == P2J_SELECTING) {
        if (justPressedJoy(joy, p2jPrevJoy, BUTTON_RIGHT))
            p2jSel = (u8)charMove((s8)p2jSel, (s8)personajeSeleccionado, +1);
        if (justPressedJoy(joy, p2jPrevJoy, BUTTON_LEFT))
            p2jSel = (u8)charMove((s8)p2jSel, (s8)personajeSeleccionado, -1);
        hudPortraitShow(1, p2jSel);
        if (hudSprite2) SPR_setAnim(hudSprite2, hudAnimForChar[p2jSel & 3]);

        if (justPressedJoy(joy, p2jPrevJoy, BUTTON_START)) {
            hudPortraitHide(1);
            p2jState   = P2J_DONE;
            p2jPrevJoy = joy;
            return p2jSel;
        }
    }

    p2jPrevJoy = joy;
    return 0xFF;
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
// Intro arcade (TMNT) -> src/intro_arcade.c
// ---------------------------------------------------------------------------
// showArcadeIntro() se mudo a su propio modulo (src/intro_arcade.c) cuando se
// reescribio siguiendo el analisis frame a frame de la intro original: son 7
// escenas con streaming vertical de la tira del dolly, sprites de las 4
// tortugas y tres layouts distintos de VRAM. El prototipo sigue en scenes.h.
// ---------------------------------------------------------------------------

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
// Inactividad que dispara el modo atracto (perfiles de las tortugas).
#define PLAYER_SELECT_IDLE_SECS   15
#define PLAYER_SELECT_IDLE_TICKS  ((IS_PAL_SYSTEM ? 50 : 60) * PLAYER_SELECT_IDLE_SECS)
// Frames que la pantalla se queda quieta al confirmar, para que se escuche el
// "COWABUNGA!" completo antes del cambio de escena (14/09). El wav dura 1,22s
// = 73 frames NTSC / 61 PAL; se toman 78 para dejar cola.
#define COWABUNGA_HOLD_FRAMES     78

// ---------------------------------------------------------------------------
// CODIGO SECRETO: modo de CUATRO tortugas (14/09)
// ---------------------------------------------------------------------------
// IZQ ABAJO IZQ DER ABAJO DER IZQ IZQ DER DER en la pantalla de cantidad de
// jugadores. Al acertarlo suena el "COWABUNGA!" y aparece la opcion "4
// TURTLES". El menu pasa de 3 a 4 filas, asi que se sube una fila (la de mas
// abajo ya estaba en la 26, la ultima de la pantalla de 28).
// OJO: ABAJO tambien mueve el cursor mientras se tipea el codigo. Es a
// proposito: asi funciona cualquier codigo de arcade, y no molesta porque al
// final se elige la opcion igual.
// El desbloqueo es STATIC: sobrevive a entrar y salir de OPCIONES o del modo
// atracto dentro de la misma partida encendida.
#define SECRET_LEN  10
static const u16 secretSeq[SECRET_LEN] = {
    BUTTON_LEFT, BUTTON_DOWN, BUTTON_LEFT,  BUTTON_RIGHT, BUTTON_DOWN,
    BUTTON_RIGHT, BUTTON_LEFT, BUTTON_LEFT, BUTTON_RIGHT, BUTTON_RIGHT
};
static bool secret4P = FALSE;

// (27/09) CODIGO KONAMI: prende el selector de niveles de la pausa (ver
// pause_menu.h). Arriba y abajo tambien mueven el cursor del menu, no
// importa: lo que cuenta es la secuencia. B y A no hacen nada en este menu.
#define KONAMI_LEN  10
static const u16 konamiSeq[KONAMI_LEN] = {
    BUTTON_UP,   BUTTON_UP,    BUTTON_DOWN, BUTTON_DOWN,
    BUTTON_LEFT, BUTTON_RIGHT, BUTTON_LEFT, BUTTON_RIGHT,
    BUTTON_B,    BUTTON_A
};

// Filas del menu segun este desbloqueado o no.
#define MENU_ROW0        22   // sin el secreto: 22, 24, 26
#define MENU_ROW0_4P     20   // con el secreto: 20, 22, 24, 26
#define MENU_CURSOR_ROW0    18
#define MENU_CURSOR_ROW0_4P 16

// Dibuja el menu de cantidad de jugadores. Con el secreto desbloqueado son 4
// filas (y arrancan una fila mas arriba, porque la ultima ya estaba al borde).
static void menuDraw4P(bool with4P) {
    // Limpiar las dos variantes: al desbloquear en vivo hay que borrar la
    // version de 3 filas antes de dibujar la de 4.
    for (u16 row = 20; row <= 26; row += 2) VDP_clearText(14, row, 12);
    u16 r = with4P ? MENU_ROW0_4P : MENU_ROW0;
    VDP_drawText("1 TURTLE",  14, r);
    VDP_drawText("2 TURTLES", 14, r + 2);
    if (with4P) {
        VDP_drawText("4 TURTLES", 14, r + 4);
        VDP_drawText("OPTIONS",   14, r + 6);
    } else {
        VDP_drawText("OPTIONS",   14, r + 4);
    }
}

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

    // Menu (3 filas, o 4 con el secreto de 4 jugadores desbloqueado).
    menuDraw4P(secret4P);

    Sprite *cursor = SPR_addSprite(&selector_turtle, 8 * 8,
                                   (secret4P ? MENU_CURSOR_ROW0_4P : MENU_CURSOR_ROW0) * 8,
                                   TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    PAL_setPalette(PAL1, selector_turtle.palette->data, CPU);

    u8  selectedOption = 0;
    u16 prev = 0;
    u8  secretStep = 0;   // progreso dentro de secretSeq
    u16 konamiLast[KONAMI_LEN] = { 0 };   // ultimas entradas (la mas nueva al final)

    // Modo ATRACTO: si nadie toca nada durante PLAYER_SELECT_IDLE_SECS, la
    // pantalla se va sola a los perfiles de las tortugas (SCENE_PROFILES), que
    // al volver reentra aca y reinicia la cuenta -> cada timeout muestra otra.
    u16 idleTicks = 0;
    bool goToProfiles = FALSE;

    // Esperar a que se suelte START para no confirmar al instante.
    while (JOY_readJoypad(playerJoy(0)) & BUTTON_START)
        SYS_doVBlankProcess();

    while (1) {
        // (17/09) por playerJoy(), no JOY_1 a mano: con un multitap enchufado
        // el mando del jugador 1 NO es JOY_1 (ver playerJoy).
        u16 value = JOY_readJoypad(playerJoy(0));

        // Cualquier boton de cualquiera de los dos joysticks resetea la cuenta
        if (value | JOY_readJoypad(playerJoy(1))) idleTicks = 0;
        else if (++idleTicks >= PLAYER_SELECT_IDLE_TICKS) { goToProfiles = TRUE; break; }

        // --- Codigo secreto de 4 jugadores ---
        // Se mira UNA direccion por frame (la que se acaba de presionar). Si
        // no es la esperada, el progreso se reinicia -- pero se vuelve a
        // probar contra el PRIMER paso, para que tipear el codigo dos veces
        // seguidas funcione sin tener que soltar nada.
        if (!secret4P) {
            u16 dir = 0;
            if      (justPressedJoy(value, prev, BUTTON_LEFT))  dir = BUTTON_LEFT;
            else if (justPressedJoy(value, prev, BUTTON_RIGHT)) dir = BUTTON_RIGHT;
            else if (justPressedJoy(value, prev, BUTTON_UP))    dir = BUTTON_UP;
            else if (justPressedJoy(value, prev, BUTTON_DOWN))  dir = BUTTON_DOWN;
            if (dir) {
                if (dir == secretSeq[secretStep]) secretStep++;
                else                              secretStep = (dir == secretSeq[0]) ? 1 : 0;
                if (secretStep >= SECRET_LEN) {
                    secret4P   = TRUE;
                    secretStep = 0;
                    XGM2_playPCMEx(cowabunga_vo, sizeof(cowabunga_vo),
                                   SOUND_PCM_CH2, 15, FALSE, FALSE);
                    menuDraw4P(TRUE);
                }
            }
        }

        // --- Codigo Konami: selector de niveles en la pausa ---
        // Una entrada por frame; se guardan las ultimas 10 y se comparan con
        // la secuencia (asi "arriba, arriba, arriba, abajo..." tambien vale:
        // cuentan las ultimas).
        if (!pauseLevelSelect()) {
            u16 in = 0;
            if      (justPressedJoy(value, prev, BUTTON_UP))    in = BUTTON_UP;
            else if (justPressedJoy(value, prev, BUTTON_DOWN))  in = BUTTON_DOWN;
            else if (justPressedJoy(value, prev, BUTTON_LEFT))  in = BUTTON_LEFT;
            else if (justPressedJoy(value, prev, BUTTON_RIGHT)) in = BUTTON_RIGHT;
            else if (justPressedJoy(value, prev, BUTTON_B))     in = BUTTON_B;
            else if (justPressedJoy(value, prev, BUTTON_A))     in = BUTTON_A;
            if (in) {
                bool match = TRUE;
                for (u16 i = 0; i < KONAMI_LEN - 1; i++) {
                    konamiLast[i] = konamiLast[i + 1];
                    if (konamiLast[i] != konamiSeq[i]) match = FALSE;
                }
                konamiLast[KONAMI_LEN - 1] = in;
                if (match && in == konamiSeq[KONAMI_LEN - 1]) {
                    pauseSetLevelSelect(TRUE);
                    XGM2_playPCMEx(cowabunga_vo, sizeof(cowabunga_vo),
                                   SOUND_PCM_CH2, 15, FALSE, FALSE);
                }
            }
        }

        u8 nOpts = secret4P ? 4 : 3;
        if (justPressedJoy(value, prev, BUTTON_UP))   selectedOption = (u8)((selectedOption + nOpts - 1) % nOpts);
        if (justPressedJoy(value, prev, BUTTON_DOWN)) selectedOption = (u8)((selectedOption + 1) % nOpts);
        if (selectedOption >= nOpts) selectedOption = 0;

        SPR_setPosition(cursor, 8 * 8,
                        ((secret4P ? MENU_CURSOR_ROW0_4P : MENU_CURSOR_ROW0)
                         + selectedOption * 2) * 8);

        if (value & BUTTON_START) break;

        prev = value;
        SPR_update();
        SYS_doVBlankProcess();
    }

    if (goToProfiles) {
        VDP_loadFont(&font_default, DMA);
        SPR_end();
        SPR_initEx(752);
        return SCENE_PROFILES;
    }

    // Sin el secreto: 0 -> 1 jugador | 1 -> 2 jugadores | 2 -> OPCIONES.
    // Con el secreto:  0 -> 1 | 1 -> 2 | 2 -> 4 jugadores | 3 -> OPCIONES.
    // (OPCIONES no toca cantidadJugadores: vuelve acá al salir.)
    u8 optOptions = secret4P ? 3 : 2;
    if (selectedOption < optOptions) {
        cantidadJugadores = (selectedOption == 2) ? 4 : (u8)(selectedOption + 1);
        if (cantidadJugadores == 4) {
            // El multitap ya se declaro al arrancar (ver main.c): los 4 mandos
            // salen del TeamPlayer del puerto 1 y se leen como JOY_1..JOY_4.
            // Tortuga fija por jugador: no se pasa por la seleccion de
            // personaje.
            personajeSeleccionado  = 0;   // Leo
            personaje2Seleccionado = 1;   // Mike
            personaje3Seleccionado = 2;   // Don
            personaje4Seleccionado = 3;   // Raph
        }
        // "COWABUNGA!" al confirmar la cantidad de jugadores (14/09).
        // Se sostiene la pantalla lo que dura el wav ANTES de irse a
        // la seleccion de personaje: la escena siguiente arranca su propia
        // musica con XGM2_play, que reinicia el driver y cortaria el PCM a
        // mitad. 1,22s a 60fps ≈ 73 frames; se dejan 78 de margen.
        XGM2_playPCMEx(cowabunga_vo, sizeof(cowabunga_vo),
                       SOUND_PCM_CH2, 15, FALSE, FALSE);
        for (u16 t = 0; t < COWABUNGA_HOLD_FRAMES; t++) {
            SPR_update();
            SYS_doVBlankProcess();
        }
    }

    // Restaurar fuente default.
    VDP_loadFont(&font_default, DMA);

    SPR_end();
    SPR_initEx(752);

    // En 4 jugadores se SALTEA la seleccion de tortuga: cada joystick ya tiene
    // la suya. OPCIONES es siempre la ultima fila.
    if (selectedOption == optOptions) return SCENE_OPTIONS;
    if (cantidadJugadores == 4) {
        // Mismo cierre que hace showCharSelect al confirmar: partida nueva.
        playerPersistReset();
        continuesLeft = 3;
        return SCENE_CINEMATIC_FIRE;
    }
    return SCENE_CHAR_SELECT;
}

// ---------------------------------------------------------------------------
// 5b. OPCIONES — LIVES (3/5/7), SOUNDTEST y EXIT
// ---------------------------------------------------------------------------
// Mismo look que la selección de players (logo de fondo + fuente arcade).
// Los textos en pantalla van en INGLÉS (igual que el resto del juego, que
// imita al arcade); los identificadores del código siguen en castellano.
// LIVES configura el global vidasIniciales (lo usan playerPersistReset y
// revivePlayer: aplica a partida nueva y a continues). SOUNDTEST reproduce
// los VGM del juego: A o C = play/stop, LEFT/RIGHT cambia de pista.
// EXIT (START sobre la fila, o B en cualquier fila) vuelve a la selección de
// cantidad de players.
// ---------------------------------------------------------------------------
#define OPT_ROW_VIDAS      0
#define OPT_ROW_SOUNDTEST  1
#define OPT_ROW_SALIR      2
#define OPT_ROW_COUNT      3

#define OPT_LABEL_COL      12   // columna de los labels
#define OPT_VALUE_COL      24   // columna del "< n >" de LIVES
// El soundtest necesita mas ancho que LIVES: el nombre de la pista mas largo
// ("APRILS ROOM" / "SCENE CLEAR") son 11 caracteres y ademas hay que dejar 3
// columnas para el ON/OFF. La pantalla tiene 40 columnas y el label termina en
// la 20, asi que el campo arranca en la 22 (antes que el de LIVES) y queda
// 22 + "< " + 11 + " >" = 37, con el ON/OFF en 37..39: justo justo.
// Si se agrega una pista con nombre mas largo hay que ACORTARLA, no ampliar
// esto: no queda una sola columna libre.
#define OPT_SND_COL        22   // columna del "< NOMBRE >" del soundtest
#define OPT_SND_NAME_MAX   11   // caracteres de nombre que entran
#define OPT_SND_FIELD_W    (OPT_SND_NAME_MAX + 4)
#define OPT_STATUS_COL     (OPT_SND_COL + OPT_SND_FIELD_W)  // ON/OFF
#define OPT_ROW_Y          20   // fila de texto de LIVES; las demás van +2
// El cursor (selector_turtle, 64x64) se dibuja 4 tiles arriba de la fila de
// texto, igual que en showPlayerSelect (fila de texto 22 -> cursor fila 18).
#define OPT_CURSOR_Y       (OPT_ROW_Y - 4)

// Pistas del sound test: nombre en pantalla (ASCII puro) + recurso XGM2.
typedef struct { const char* name; const u8* track; } SoundTrack;
// OJO: maximo OPT_SND_NAME_MAX caracteres, lo que pase de ahi se corta.
static const SoundTrack soundTracks[] = {
    { "FIRE!",        music_level1 },
    { "PROFILES",     music_profiles },
    { "APRILS ROOM",  music_level2 },
    { "CHAR SELECT",  music_charselect },
    { "FIGHT!",       music_boss },
    { "SCENE CLEAR",  music_scene_clear },
    { "DOWNTOWN",     music_stage2_1 },
    { "SEWERS",       music_level3 },
    { "GARAGE",       music_garage },
    { "HIGHWAY",      music_freeway },
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
    char buf[OPT_SND_FIELD_W + 1];
    u8 i = 0;
    const char* name = soundTracks[trackIdx].name;
    buf[i++] = '<'; buf[i++] = ' ';
    while (*name && i < OPT_SND_NAME_MAX + 2) buf[i++] = *name++;
    buf[i++] = ' '; buf[i++] = '>'; buf[i] = 0;
    // Se borra el campo ENTERO antes de escribir: los nombres tienen distinto
    // largo y si no quedan letras del anterior colgando a la derecha.
    VDP_clearText(OPT_SND_COL, OPT_ROW_Y + 2, OPT_SND_FIELD_W);
    VDP_drawText(buf, OPT_SND_COL, OPT_ROW_Y + 2);
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

    VDP_drawText("LIVES",     OPT_LABEL_COL, OPT_ROW_Y);
    VDP_drawText("SOUNDTEST", OPT_LABEL_COL, OPT_ROW_Y + 2);
    VDP_drawText("EXIT",      OPT_LABEL_COL, OPT_ROW_Y + 4);

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
                // setLoopNumber(-1) SIEMPRE antes del play: el driver latchea
                // el loop en el instante del play. Sin esto el soundtest se
                // queda con el ultimo loop que dejo la escena anterior y las
                // pistas cortas (SCENE CLEAR son 0,7s) sonaban una sola vez.
                if (sndPlaying) {
                    XGM2_setLoopNumber(-1);
                    playMusicVol(soundTracks[trackIdx].track, 100);
                }
                optDrawSound(trackIdx, sndPlaying);
            }
            if (justPressedJoy(value, prev, BUTTON_A) || justPressedJoy(value, prev, BUTTON_C)) {
                if (sndPlaying) { XGM2_stop(); sndPlaying = FALSE; }
                else {
                    XGM2_setLoopNumber(-1);
                    playMusicVol(soundTracks[trackIdx].track, 100);
                    sndPlaying = TRUE;
                }
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

    playMusicVol(music_charselect, VOL_MUSIC_SELECT);

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

    // Paletas: el HUD comparte la de las tortugas en PAL1. El selector de
    // personaje va en PAL3 para no pisar los colores del HUD.
    PAL_setPalette(PAL1, hud_1p.palette->data, DMA);
    PAL_setPalette(PAL2, faces_hud.palette->data, DMA);
    PAL_setPalette(PAL3, character_selector.palette->data, DMA);

    // Cargar marcos + retratos del HUD. Inicialmente muestran los personajes
    // que haya en las variables persistentes; luego se fuerza el frame según
    // la selección actual del cursor.
    hudInit();
    hudInitPortraits();   // esta pantalla SI muestra los retratos: es su razon de ser

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
        return SCENE_CINEMATIC_FIRE;
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
        // (17/09) por playerJoy(), no JOY_1/JOY_2 a mano: con un multitap los
        // mandos NO se numeran asi (ver playerJoy) y el P2 no podia elegir.
        u16 v1 = JOY_readJoypad(playerJoy(0));
        u16 v2 = JOY_readJoypad(playerJoy(1));

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
    return SCENE_CINEMATIC_FIRE;
}

// showFireCinematic() (SCENE_CINEMATIC_FIRE) vive en src/cinematic_fire.c:
// es la cinematica del rescate de April que va entre la seleccion de
// personaje y el titulo del nivel 1.

// ---------------------------------------------------------------------------
// 7. Título del nivel 1 — texto letra a letra con la fuente arcade
// ---------------------------------------------------------------------------
// La fuente (title_font en level1.res) está en orden ASCII 32..126, tiles de
// 8x8, así que se carga con VDP_loadFont y VDP_drawText funciona directo.
// ---------------------------------------------------------------------------
#define TITLE_CHAR_DELAY  5   // frames entre letra y letra (~12 letras/seg)

// Dibuja el texto letra a letra con 'delay' frames entre letras.
// Devuelve TRUE si se pidió saltar con START.
// ---------------------------------------------------------------------------
// HUD EN LAS PANTALLAS DE TITULO (01/10)
// ---------------------------------------------------------------------------
// Los titulos de nivel muestran el HUD de la partida (marcos, puntaje, vidas y
// barra) y la barra de cada jugador se RECARGA de a una raya mientras dura el
// titulo, como el arcade. La vida queda llena en el estado persistente, asi
// que el nivel arranca con la barra completa. Los que estan FUERA de juego
// (s_persistOut) muestran su HUD vacio y no se recargan.
//
// No hay Player de verdad (no hay tortugas en pantalla): el HUD se alimenta de
// un Player "de papel" con lo persistido (playerPersistPeek); hudPlayerUpdate
// solo lee vida, vidas, puntaje y charIndex.
//
// CHOQUE DE FUENTES: el puntaje del HUD se escribe con hud_font en la ranura
// de la fuente (VDP_drawText), y el titulo usa title_font. Mientras el HUD
// esta prendido, title_font va a TITLE_FONT_VRAM y el titulo se dibuja a mano
// (titleDrawText) con esos tiles, en PAL0 como siempre.
#define TITLE_FONT_VRAM     (TILE_USER_INDEX + MAX_PLAYERS * HUD_VRAM_PER_PLAYER)
#define TITLE_REFILL_TICKS  8     // frames por raya recargada (10 rayas ~1.3 s)

static bool      titleHudOn = FALSE;
static u8        titleNPl;
static u16       titleRefillTick;
static Player    titlePl[MAX_PLAYERS];
static HudPlayer titleHud[MAX_PLAYERS];

static void titleDrawText(const char* text, u16 x, u16 y) {
    if (!titleHudOn) { VDP_drawText(text, x, y); return; }
    for (u16 i = 0; text[i] != 0; i++) {
        u8 c = (u8)text[i];
        if (c < 32 || c > 126) c = 32;
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE,
                                              TITLE_FONT_VRAM + (c - 32)),
                         (u16)(x + i), y);
    }
}

static void titleHudBegin(void) {
    titleNPl = numJugadores();
    VDP_loadTileSet(&title_font, TITLE_FONT_VRAM, DMA);
    VDP_loadFont(&hud_font, DMA);
    VDP_setTextPlane(BG_A);
    VDP_setTextPriority(1);
    VDP_setTextPalette(PAL1);
    PAL_setColors(16, leo_player.palette->data, 16, DMA);   // PAL1: tortugas/HUD
    hudSetPlane(BG_A);
    hudInit();
    for (u8 k = 0; k < titleNPl; k++) {
        Player* p = &titlePl[k];
        memset(p, 0, sizeof(Player));
        u8 lives; u16 score; s16 hp; bool out;
        playerPersistPeek(k, &lives, &score, &hp, &out);
        p->lives     = lives;
        p->score     = score;
        p->health    = out ? 0 : hp;
        p->gameOver  = out;
        p->charIndex = (u8)(playerChar(k) & 3);
        hudPlayerInit(&titleHud[k], p, hudPlayerCol(k),
                      (u16)(TILE_USER_INDEX + k * HUD_VRAM_PER_PLAYER));
        hudPlayerUpdate(&titleHud[k]);
    }
    titleRefillTick = 0;
    titleHudOn = TRUE;
    SPR_update();
}

static bool titleHudFull(void) {
    for (u8 k = 0; k < titleNPl; k++)
        if (!titlePl[k].gameOver && titlePl[k].health < PLAYER_MAX_HEALTH) return FALSE;
    return TRUE;
}

// Un frame: una raya mas cada TITLE_REFILL_TICKS. 'fill' = llenar de una
// (se salteo el titulo con START).
static void titleHudStep(bool fill) {
    if (!titleHudOn) return;
    bool tick = (++titleRefillTick >= TITLE_REFILL_TICKS);
    if (tick) titleRefillTick = 0;
    for (u8 k = 0; k < titleNPl; k++) {
        Player* p = &titlePl[k];
        if (!p->gameOver && p->health < PLAYER_MAX_HEALTH && (tick || fill))
            p->health = fill ? PLAYER_MAX_HEALTH : (s16)(p->health + 1);
        hudPlayerUpdate(&titleHud[k]);
    }
    SPR_update();
}

static void titleHudEnd(void) {
    if (!titleHudOn) return;
    for (u8 k = 0; k < titleNPl; k++)
        if (!titlePl[k].gameOver) playerPersistSetHealth(k, PLAYER_MAX_HEALTH);
    titleHudOn = FALSE;
}

// Espera final del titulo: 'frames' y ademas, con el HUD prendido, que se
// termine de recargar la barra. START corta (y llena la barra de una).
// 'perFrame' corre cada frame (el 1-1 apaga ahi el tema de la cinematica).
static void titleHold(u16 frames, void (*perFrame)(void)) {
    while (frames > 0 || (titleHudOn && !titleHudFull())) {
        if (frames > 0) frames--;
        if (perFrame) perFrame();
        if (JOY_readJoypad(JOY_1) & BUTTON_START) { titleHudStep(TRUE); break; }
        titleHudStep(FALSE);
        SYS_doVBlankProcess();
    }
}

static bool drawTextTypewriter(const char* text, u16 x, u16 y, u16 delay) {
    char buf[2];
    buf[1] = 0;

    for (u16 i = 0; text[i] != 0; i++) {
        buf[0] = text[i];

        // Los espacios no se dibujan, pero sí consumen tiempo (ritmo natural)
        if (buf[0] != ' ')
            titleDrawText(buf, x + i, y);

        // Espera entre letras, con posibilidad de saltar
        for (u16 f = 0; f < delay; f++) {
            if (JOY_readJoypad(JOY_1) & BUTTON_START) {
                titleHudStep(TRUE);
                return TRUE;
            }
            titleHudStep(FALSE);   // (01/10) el HUD recarga mientras se escribe
            SYS_doVBlankProcess();
        }
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// 3. Pantalla de creditos y agradecimientos
// ---------------------------------------------------------------------------
// Usa la fuente arcade del título (title_font). OJO: la fuente cubre ASCII
// 32..126, por eso los textos van SIN acentos ni signos especiales (por eso
// STEPHANE y no STEPHANE con acento).
//
// (19/09) Reescrita en ingles, sin el bloque en espanol: equipo + creador de
// SGDK + la lista de ripeadores de sprites de la comunidad.
//
// La pantalla son 40 columnas x 28 filas. Cada linea se centra a mano:
//     x = (40 - strlen(texto)) / 2
// Si se toca un texto HAY QUE recalcular su x, no hay centrado automatico.
//
// OJO 1 - GLIFOS VACIOS: font_tmnt_arcade.png tiene los 95 tiles de ASCII
// 32..126, pero varios estan EN BLANCO y se dibujan como un espacio:
//     # $ % & * + / < = > @ [ \ ] ^ _ ` { | }
// Por eso la barra de "2D Assets / Background Artist" va como guion y el
// separador "* * *" que habia antes no se veia (se saco).
// Disponibles: letras, digitos y  ! " ' ( ) , - . : ; ?
//
// OJO 2 - INTERLINEADO: los glifos ocupan las 8 filas del tile, no tienen
// margen. Dos lineas en filas consecutivas se tocan. Van todas de dos en dos
// (2, 4, 6, ...): 13 lineas es el maximo que entra asi en las 28 filas.
// (26/09) Ahora van en las IMPARES (1..27): 14 lineas.
// ---------------------------------------------------------------------------
#define CREDITS_CHAR_DELAY  2   // Más rápido que el título (hay mucho texto)
#define SGDK_HOLD_SECS   4   // Segundos con el texto completo en pantalla

SceneId showSGDKIntro() {
    clearScene();

    // Fuente arcade + su paleta (blanco con sombreado azul, fondo negro)
    VDP_loadFont(&title_font, DMA);
    PAL_setColors(0, title_font_pal.data, title_font_pal.length, DMA);
    VDP_setTextPalette(PAL0);
    VDP_setBackgroundColor(0);

    // Líneas centradas en las 40 columnas de pantalla: x = (40 - len) / 2
    static const struct { const char* text; u16 x; u16 y; } lines[] = {
        //   texto                                     x   y     len
        // (26/09) 14 lineas en las filas impares 1..27 (antes 13 en 2..26)
        // para que entre Ray Castello; el "(SEGA GENESIS...)" de SGDK paso a
        // la misma linea que "CREATOR OF SGDK".
        { "CREDITS",                                  16,  1 },  //  7
        // --- Equipo ---
        { "GUSTAVO VALENZUELA",                       11,  3 },  // 18
        { "LEAD DEVELOPER - PROGRAMMER (SGDK)",        3,  5 },  // 34
        { "AND GRAPHICS ADAPTATION",                   8,  7 },  // 23
        { "ROBSON RICARDO",                           13,  9 },  // 14
        { "2D ASSETS - BACKGROUND ARTIST",             5, 11 },  // 29
        { "RAY CASTELLO",                             14, 13 },  // 12
        { "PROGRAMMING - LEVELS AND BOSSES",           4, 15 },  // 31
        { "STEPHANE DALLONGEVILLE",                    9, 17 },  // 22
        { "CREATOR OF SGDK (SEGA GENESIS DEV KIT)",    1, 19 },  // 38
        // --- Ripeadores de sprites de la comunidad ---
        { "SPRITE RIPS BY THE COMMUNITY",              6, 21 },  // 28
        { "ENSCRIPTURE - NAPALM - MONFRIEZ",           4, 23 },  // 31
        { "T0MISAURUS - SOMETHINGEVIL",                7, 25 },  // 26
        { "EASTX - DEATHBRINGER",                     10, 27 },  // 20
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
    u16 timer = (IS_PAL_SYSTEM ? 50 : 60) * SGDK_HOLD_SECS;
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
#define CREDITS_HOLD_SECS  2

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

    // Música: arranca UNA vez antes de la pausa final (se reproduce una sola
    // vez durante los CREDITS_HOLD_SECS segundos).
    XGM2_setLoopNumber(0);
    playMusicVol(music_credits, VOL_MUSIC_CREDITS);

    // Mantener en pantalla (el esqueleto se anima vía SPR_update; START corta)
    u16 timer = (IS_PAL_SYSTEM ? 50 : 60) * CREDITS_HOLD_SECS;
    while (timer > 0) {
        timer--;
        if (JOY_readJoypad(JOY_1) & BUTTON_START) break;
        SPR_update();
        SYS_doVBlankProcess();
    }

    XGM2_setLoopNumber(-1);   // restaurar loop infinito para la siguiente música
    VDP_loadFont(&font_default, DMA);
    clearScene();
    return SCENE_INTRO_ARCADE;
}

static bool title11MusicOff;
static void title11StopMusic(void) {
    if (!title11MusicOff && !XGM2_isPlaying()) { XGM2_stop(); title11MusicOff = TRUE; }
}

SceneId showScene11Title() {
    title11MusicOff = FALSE;
    // (18/09) keepAudio: la cinematica del rescate deja sonando el tema de la
    // intro y tiene que seguir durante todo el titulo. Se corta al final, ya
    // con la pantalla en negro, antes de que el nivel arranque music_level1.
    clearSceneEx(TRUE);

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

    titleHudBegin();   // (01/10) HUD de la partida + recarga de la barra

    // Aparición letra a letra (START saltea la animación)
    bool skipped;
    skipped = drawTextTypewriter(line1, 16, 10, TITLE_CHAR_DELAY);
    if (!skipped) skipped = drawTextTypewriter(line2, 11, 13, TITLE_CHAR_DELAY);
    if (!skipped) skipped = drawTextTypewriter(line3, 14, 15, TITLE_CHAR_DELAY);

    // Si salteó, mostramos el texto completo de una
    if (skipped) {
        titleDrawText(line1, 16, 10);
        titleDrawText(line2, 11, 13);
        titleDrawText(line3, 14, 15);
    }

    // Mantener el texto completo 2 segundos (60 fps NTSC / 50 fps PAL)
    //
    // (19/09) Acá también se APAGA el tema de la cinemática. Viene sonando
    // desde la mitad de la cinemática del rescate con XGM2_setLoopNumber(0),
    // o sea una sola pasada: cuando el VGM llega al final el driver deja de
    // sonar pero sigue cargado, así que en cuanto XGM2_isPlaying() da FALSE
    // se lo para de verdad. El flag evita mandarle el comando al Z80 en cada
    // frame una vez que ya está parado.
    titleHold((IS_PAL_SYSTEM ? 50 : 60) * 2, title11StopMusic);
    titleHudEnd();

    // Restaurar la fuente por defecto de SGDK para el resto del juego
    VDP_loadFont(&font_default, DMA);

    // IMPORTANTE: la cinemática dejó el driver en "una sola pasada". El nivel
    // llama a playMusicVol(music_level1) SIN tocar el loop, así que si no se
    // restaura acá el tema del nivel sonaría una vez y se cortaría.
    XGM2_setLoopNumber(-1);

    clearScene();
    return SCENE_1_1;
}

// ---------------------------------------------------------------------------
// 7 bis. Título de la SCENE 2 — mismo tratamiento que el de la Scene 1
// ---------------------------------------------------------------------------
// Entra justo después de la cutscene en la que Shredder se escapa por la
// ventana del edificio en llamas (showEnding). Misma fuente arcade
// (title_font), mismo efecto de aparición letra a letra y mismo skip por
// START. La segunda línea son 34 caracteres: entra justa en las 40 columnas
// de la pantalla arrancando en la columna 3.
// ---------------------------------------------------------------------------
SceneId showScene21Title() {
    clearScene();

    // Venimos de showEnding, donde START adelanta el hold final: esperar a que
    // lo suelten para no saltear también este título.
    while (JOY_readJoypad(JOY_1) & BUTTON_START)
        SYS_doVBlankProcess();

    VDP_loadFont(&title_font, DMA);
    PAL_setColors(0, title_font_pal.data, title_font_pal.length, DMA);
    VDP_setTextPalette(PAL0);
    VDP_setBackgroundColor(0);

    const char* line1 = "SCENE 2";
    const char* line2 = "C'MON, AFTER THAT SHREDDER CREEP!!";

    titleHudBegin();   // (01/10)

    bool skipped;
    skipped = drawTextTypewriter(line1, 16, 10, TITLE_CHAR_DELAY);
    if (!skipped) skipped = drawTextTypewriter(line2, 3, 13, TITLE_CHAR_DELAY);

    if (skipped) {
        titleDrawText(line1, 16, 10);
        titleDrawText(line2, 3, 13);
    }

    titleHold((IS_PAL_SYSTEM ? 50 : 60) * 2, NULL);
    titleHudEnd();

    VDP_loadFont(&font_default, DMA);

    clearScene();
    return SCENE_2_1;
}

// ---------------------------------------------------------------------------
// 7 ter. Títulos de las SCENE 3 a 9 (cloaca, garage, freeway, skate,
// fabrica, Technodrome, sala final) (26-27/09)
// ---------------------------------------------------------------------------
// Mismo tratamiento que los otros dos. La segunda línea es PROVISORIA: falta
// confirmar el texto exacto del arcade para cada escena.
#define SCENE3_TITLE_LINE2  "INTO THE SEWER!!"
#define SCENE4_TITLE_LINE2  "THE PARKING GARAGE!!"
#define SCENE5_TITLE_LINE2  "HIT THE FREEWAY!!"
#define SCENE6_TITLE_LINE2  "SKATE THE HIGHWAY!!"
#define SCENE7_TITLE_LINE2  "INTO THE FACTORY!!"
#define SCENE8_TITLE_LINE2  "WE GOTTA FIND THE TECHNODROME!"   
#define SCENE9_TITLE_LINE2  "SHOWDOWN WITH SHREDDER!!"

static SceneId showStageTitle(const char* line1, const char* line2, SceneId next) {
    clearScene();
    while (JOY_readJoypad(JOY_1) & BUTTON_START)
        SYS_doVBlankProcess();

    VDP_loadFont(&title_font, DMA);
    PAL_setColors(0, title_font_pal.data, title_font_pal.length, DMA);
    VDP_setTextPalette(PAL0);
    VDP_setBackgroundColor(0);

    u16 col1 = (u16)((40 - strlen(line1)) / 2);
    u16 col2 = (u16)((40 - strlen(line2)) / 2);

    titleHudBegin();   // (01/10)

    bool skipped;
    skipped = drawTextTypewriter(line1, col1, 10, TITLE_CHAR_DELAY);
    if (!skipped) skipped = drawTextTypewriter(line2, col2, 13, TITLE_CHAR_DELAY);
    if (skipped) {
        titleDrawText(line1, col1, 10);
        titleDrawText(line2, col2, 13);
    }

    titleHold((IS_PAL_SYSTEM ? 50 : 60) * 2, NULL);
    titleHudEnd();

    VDP_loadFont(&font_default, DMA);
    clearScene();
    return next;
}

SceneId showScene31Title() {
    return showStageTitle("SCENE 3", SCENE3_TITLE_LINE2, SCENE_3_1);
}

SceneId showScene41Title() {
    return showStageTitle("SCENE 4", SCENE4_TITLE_LINE2, SCENE_4_1);
}

SceneId showScene51Title() {
    return showStageTitle("SCENE 5", SCENE5_TITLE_LINE2, SCENE_5_1);
}

SceneId showScene61Title() {
    return showStageTitle("SCENE 6", SCENE6_TITLE_LINE2, SCENE_6_1);
}

SceneId showScene71Title() {
    return showStageTitle("SCENE 7", SCENE7_TITLE_LINE2, SCENE_7_1);
}

SceneId showScene81Title() {
    return showStageTitle("SCENE 8", SCENE8_TITLE_LINE2, SCENE_8_1);
}

SceneId showScene91Title() {
    return showStageTitle("SCENE 9", SCENE9_TITLE_LINE2, SCENE_9_1);
}

// ---------------------------------------------------------------------------
// 7 quater. Pantalla FINAL (27/09): despues de vencer a Shredder. Mismo
// tratamiento que los titulos; los textos son PROVISORIOS. Despues, los
// creditos del equipo (SCENE_SGDK), los del soundtrack y de vuelta a la intro.
// ---------------------------------------------------------------------------
#define THE_END_LINE1  "CONGRATULATIONS!!"
#define THE_END_LINE2  "STAY TUNED FOR UPDATES."
#define THE_END_LINE3  "THE END"
#define THE_END_SECS   8

SceneId showTheEnd() {
    clearScene();
    while (JOY_readJoypad(JOY_1) & BUTTON_START)
        SYS_doVBlankProcess();

    VDP_loadFont(&title_font, DMA);
    PAL_setColors(0, title_font_pal.data, title_font_pal.length, DMA);
    VDP_setTextPalette(PAL0);
    VDP_setBackgroundColor(0);

    const char* lines[3] = { THE_END_LINE1, THE_END_LINE2, THE_END_LINE3 };
    static const u16 rows[3] = { 9, 12, 17 };
    bool skipped = FALSE;
    for (u16 i = 0; i < 3 && !skipped; i++)
        skipped = drawTextTypewriter(lines[i], (u16)((40 - strlen(lines[i])) / 2), rows[i],
                                     TITLE_CHAR_DELAY);
    if (skipped)
        for (u16 i = 0; i < 3; i++)
            VDP_drawText(lines[i], (u16)((40 - strlen(lines[i])) / 2), rows[i]);

    XGM2_setLoopNumber(0);
    playMusicVol(music_ending, 90);
    u16 timer = (IS_PAL_SYSTEM ? 50 : 60) * THE_END_SECS;
    while (timer > 0) {
        timer--;
        if (JOY_readJoypad(JOY_1) & BUTTON_START) break;
        SYS_doVBlankProcess();
    }
    XGM2_stop();
    XGM2_setLoopNumber(-1);

    VDP_loadFont(&font_default, DMA);
    clearScene();
    return SCENE_SGDK;
}

// ---------------------------------------------------------------------------
// 8. Nivel 1 — fondo scrolleable + fuego en primer plano + jugador
// ---------------------------------------------------------------------------
SceneId showScene11() {
    clearScene();

    // --- Presupuesto del motor de sprites para ESTE nivel (15/09) ---
    // Se CALCULA, no se estima: el area de sprites vive en
    // [TILE_FONT_INDEX - size .. TILE_FONT_INDEX-1], asi que el tope seguro es
    // TILE_FONT_INDEX menos el ultimo tile de usuario que usa el nivel.
    // Los tiles de usuario del nivel 1 son, en este orden: fondo, fuego, un
    // bloque de barra de vida POR JUGADOR y los bloques de sparks.
    //
    // Antes esto era un 752 global (main.c) puesto a ojo, y hacia cuentas la
    // cosa no cerraba: con 1-2 jugadores el final real de los tiles de usuario
    // deja un tope de 749, o sea que el 752 ya se comia 3 tiles del bloque de
    // sparks_2. Ahora sale exacto para cualquier cantidad de jugadores.
    //
    // OJO: esto TIENE que ir antes de cualquier SPR_addSprite de la escena
    // (SPR_initEx resetea el motor y libera lo que hubiera).
    const u8   nPlSetup    = numJugadores();
    const bool sparksFullOn = (nPlSetup <= 2);   // 4P apaga puertas y piso
    const u16  bgUserTiles  = TILE_USER_INDEX + bg_level1.tileset->numTile
                              + FIRE_CELL_TILES;
    const u16  barTiles     = (u16)((nPlSetup > 2) ? MAX_PLAYERS : 2)
                              * HUD_VRAM_PER_PLAYER;
    // (16/09) Con 4 jugadores NO va ningun spark: ni puertas, ni piso, ni
    // ascensor. Sus tres bloques de VRAM de fondo (16+13+35 = 64 tiles) se los
    // queda entero el motor de sprites.
    const u16  sparkTiles   = sparksFullOn
                              ? (SPARKS_TILES + ELEV_SPARK_TILES)
                              : 0;
    SPR_initEx((u16)(TILE_FONT_INDEX - (bgUserTiles + barTiles + sparkTiles)));

    // --- Fondo con STREAMING de columnas (nivel completo de 1376px) ---
    // bgInit carga la paleta + tileset completo a VRAM y dibuja las primeras
    // 64 columnas en el plano circular BG_B. bgUpdate() revela columnas nuevas
    // a medida que la cámara avanza.
    // Mapa de paletas del nivel:
    //   PAL0 → fondo | PAL1 → tortugas | PAL2 → foot soldiers (morado + naranja) + fuego | PAL3 → foot soldier blanco
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
    // Dinamita del morado de la escalera (18/09): un solo cartucho, guionado.
    tntInit();

    // La paleta PAL3 (foot soldier BLANCO) la carga levelFadeIn al final del
    // setup. Antes era la del naranja: desde el 24/09 el naranja comparte la
    // paleta del morado (PAL2), asi que esta linea quedo para el blanco.

    // --- Música del nivel (los SFX por PCM siguen activos) ---
    playMusicVol(music_level1, VOL_MUSIC_LEVEL1);

    // --- Inicializar jugador(es) ---
    // Las 4 tortugas comparten la paleta unificada, así que P1 y P2 usan PAL1.
    // (14/09) nPl = jugadores presentes (1..4). 'dosJugadores' se mantiene con
    // el sentido de "hay mas de uno", que es como lo usa el resto del nivel.
    u8   nPl = numJugadores();
    bool dosJugadores = (nPl >= 2);

    // Los límites izquierdo/derecho reales se recalculan CADA frame en el
    // paso 3 del bucle (dependen de la cámara); acá solo el arranque.
    Player p1, p2, p3, p4;
    Player* pls[MAX_PLAYERS] = { &p1, &p2, &p3, &p4 };
    initPlayer(&p1, personajeSeleccionado, JOY_1, PAL1, 40, 182);   // 5 tiles desde el borde izq
    setPlayerRightBound(&p1, SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W);

    // Jugadores 2..4: se reparten en X para no nacer uno encima del otro.
    // (17/09) Nacian en 160 / 200 / 240, o sea FUERA de la franja muerta de la
    // camara (CAM_DEAD_ZONE_RIGHT = 120). Como la camara sigue al que mas
    // avanzo, el nivel arrancaba solo: 40px de scroll involuntario con 2
    // jugadores y 120px con 4. Ahora nacen pegados al P1 y todos adentro.
    {
        const s16 spreadX = (nPl > 2) ? 26 : 64;   // 104 / 66-92-118
        for (u8 k = 1; k < nPl; k++) {
            initPlayer(pls[k], playerChar(k), playerJoy(k), PAL1,
                       (s16)(40 + k * spreadX), 182);
            setPlayerRightBound(pls[k], SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W);
        }
    }

    // --- Bola de hierro (obstáculo que cae rebotando) ---
    // Comparte PAL1 (tortugas), ya cargada por initPlayer. Sprite oculto hasta
    // el primer spawn. Dos bolas independientes, mismo arco, cadencia distinta
    // (IRON_BALL_PERIOD / _PERIOD2 -- ver comentario grande más arriba).
    ironBallInit(&ironBall, IRON_BALL_PERIOD);
    ironBallInit(&ironBall2, IRON_BALL_PERIOD2);

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

    p2JoinReset();   // (17/09) invitacion "PULSE START" en el marco vacio del P2
    static HudPlayer huds[MAX_PLAYERS];
    for (u8 k = 0; k < nPl; k++)
        hudPlayerInit(&huds[k], pls[k], hudPlayerCol(k),
                      (u16)(hudVramFree + k * HUD_VRAM_PER_PLAYER));

    // --- Sparks (puertas + ascensores + decorado fijo): streaming de tiles ---
    // Reservan VRAM de fondo justo después de las 2 barras de vida (se
    // reserva SIEMPRE el bloque de 2 jugadores, haya 1 o 2, para que la
    // dirección no dependa de dosJugadores). Ver el comentario grande junto
    // a SPARKS_* más arriba: reemplaza la vieja rotación de PAL2.
    // (14/09) Se reserva UN bloque de barra POR JUGADOR presente, con un
    // minimo de 2 para que en 1-2 jugadores las direcciones queden EXACTAMENTE
    // donde estaban antes (el modo de 4 corre los sparks 8 tiles mas arriba).
    // (15/09) Y con 4 jugadores los bloques de las puertas y del piso NI SE
    // RESERVAN: el ascensor arranca directo, y esos 56 tiles se los queda el
    // motor de sprites (ver el calculo del presupuesto arriba).
    u16 barBlocks = (u16)((nPl > 2) ? MAX_PLAYERS : 2) * HUD_VRAM_PER_PLAYER;
    if (sparksFullOn)
        sparksStreamInit(hudVramFree + barBlocks,
                         hudVramFree + barBlocks + SPARKS_TILES,
                         TRUE);
    else
        sparksStreamInit(0, 0, FALSE);      // 4P: ningun spark

    // --- Estado de continues por jugador (cuenta regresiva + selección) ---
    static ContPlayer conts[MAX_PLAYERS];
    contResetAll(conts);

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

    static Enemy enemies[MAX_ENEMIES];
    for (u16 i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].state = ENEMY_STATE_INACTIVE;
        enemies[i].sprite = NULL;
    }

    // --- Robot(es) del látigo (mini-jefe del final del nivel) ---
    // 1 jugador -> 1 robot · 2 jugadores -> 2 (mismo X, distinto lane) ·
    // 4 jugadores -> CUATRO, uno por lane (pedido del 16/09: "quiero ver como
    // lo maneja la consola y si el parpadeo inevitable es tolerable").
    // Los que no se spawnean quedan ROBOT_INACTIVE, y robotUpdateN /
    // robotCanBeHit son no-op en ese estado.
    // nRobots: cuantos salen de verdad en esta partida.
    const u8 nRobots = (nPlSetup > 2) ? LEVEL1_MAX_ROBOTS : nPlSetup;
    static Robot robots[LEVEL1_MAX_ROBOTS];
    for (u8 r = 0; r < LEVEL1_MAX_ROBOTS; r++) robotInit(&robots[r]);

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
    // sparksTimer/sparksFrame ahora son de ámbito de archivo (los maneja
    // sparksStreamInit/Update, ya llamado más arriba junto al HUD).
    Sprite* sparkSpr[LEVEL1_DOOR_COUNT];
    for (u16 d = 0; d < LEVEL1_DOOR_COUNT; d++) sparkSpr[d] = NULL;

    // --- Fuego del piso (26/09): decorativo, en DOS puntos del suelo ---
    // Reemplaza a sparks_2. Sprite comun con auto-animacion (floor_fire, 9
    // frames de 56x32): la llama cambia de forma en cada frame y no se puede
    // streamear a un bloque compartido. Cada uno se crea al acercarse a la
    // camara y se suelta al alejarse, asi en general solo paga uno (25 tiles
    // como maximo). FLOOR_FIRE_X = centro del charco, FLOOR_FIRE_Y = su borde
    // de abajo (coordenadas de mundo; el piso del 1-1 va de y 142 a 200).
    // Van en la parte de ARRIBA del piso: de y=160 para abajo esta la banda
    // del fuego de primer plano (BG_A, prioridad alta), que los tapaba casi
    // enteros (probado con el y=194 de sparks_2). El primero queda en la X
    // de sparks_2; el segundo, entre la escalera y la segunda puerta. Para
    // moverlos alcanza con tocar estas tablas.
    // (15/09) Con 4 jugadores no se crean (ver sparksDoorFloorOn).
    #define FLOOR_FIRE_COUNT   2
    #define FLOOR_FIRE_HALF_W  26     // el charco mide 51..53 px
    #define FLOOR_FIRE_H       32
    #define FLOOR_FIRE_MARGIN  64
    static const s16 floorFireX[FLOOR_FIRE_COUNT] = { 362, 640 };
    static const s16 floorFireY[FLOOR_FIRE_COUNT] = { 158, 152 };
    Sprite* floorFireSpr[FLOOR_FIRE_COUNT] = { NULL, NULL };

    // --- Ascensores: 2 puertas animadas que se abren JUNTAS ---
    // elevPhase: 0=cerradas (esperando que ambas estén centradas) · 1=abriendo
    // (animación) · 2=remover + spawnear OLEADA 1 (2 de los ascensores + 2 por
    // los costados) · 3=oleada 1 activa, esperando que caigan todos · 4=oleada
    // 2 activa (emboscada completa por ambos lados, se dispara sola al limpiar
    // la 1) · 5=hecho, ambas oleadas despejadas (no vuelve a disparar).
    // Ampliado (30/08): antes elevPhase==2 terminaba el
    // encuentro con solo 2 enemigos; ahora es una emboscada de dos oleadas.
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
                foot_soldier_white.palette->data);

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
    // Un jugador se come UN solo golpe por explosion: la ventana de dano dura
    // varios frames y la invulnerabilidad podria no cubrirla entera.
    u8   tntHitMask = 0;

    // --- Bucle principal del nivel ---
    SceneId jump = PAUSE_NO_JUMP;   // (26/09) nivel elegido en el menu de pausa
    pauseReset();
    while (1) {
        // 0. Pausa (START del control 1). Va ANTES de los continues: el START
        //    que revive al jugador 1 no tiene que abrir la pausa.
        jump = pausePoll(pls, nPl);
        if (jump != PAUSE_NO_JUMP) break;

        // 1. Input y física de cada jugador
        for (u8 k = 0; k < nPl; k++) updatePlayer(pls[k]);

        // 2. Cámara dead-zone: la mueve el jugador que va MÁS ADELANTE
        //    (estilo arcade), pero SIN dejar nunca al rezagado fuera de
        //    pantalla: si el avance lo sacaría por la izquierda, la cámara
        //    se topea hasta que el otro también avance. Beat-em-up clásico:
        //    la cámara solo va a la derecha, nunca retrocede (por eso solo
        //    revelamos columnas nuevas a la derecha).
        // (26/09) Los que estan fuera de juego (sin vidas: contando el
        // CONTINUE? o ya fuera) no cuentan: su cuerpo no frena la camara.
        s16 leadX, trailX;
        playersSpanX(pls, nPl, &leadX, &trailX);
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
        for (u8 k = 1; k < nPl; k++) {
            setPlayerCamera(pls[k], cameraX);
            setPlayerLeftBound(pls[k], cameraX);
            setPlayerRightBound(pls[k], rightBound);
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
                    // Sparks: fuego detrás de la puerta (streaming compartido,
                    // ver sparksAddSprite -- ya no gasta VRAM propia).
                    if (sparksFullOn && !sparkSpr[d]) {
                        sparkSpr[d] = sparksAddSprite(&sparks, sparksVramInd,
                                                      screenX - SPARKS_LEFT_DX, SPARKS_SPRITE_TOP_Y);
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
                    SPR_setPosition(sparkSpr[d], screenX - SPARKS_LEFT_DX, SPARKS_SPRITE_TOP_Y);
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
                for (u8 k = 1; k < nPl; k++) {
                    s16 pdxk = getPlayerWorldX(pls[k]) + (PLAYER_SPRITE_W / 2) - doorCenterX[d];
                    if (pdxk < 0) pdxk = -pdxk;
                    if (pdxk < pdx) pdx = pdxk;
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
        if (elevPhase < 5) {
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
                        // (16/09) Con 4 jugadores tampoco va el spark del
                        // ascensor: son otros 15 tiles de
                        // fondo y unos cuantos sprites de hardware menos.
                        if (sparksFullOn && !elevSparkSpr[ev]) {
                            elevSparkSpr[ev] = sparksAddSprite(&spark_ascensor, elevSparkVramInd,
                                                               sx - ELEV_SPARK_HALF_W, ELEV_SPARK_TOP_Y);
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
                // Remover ambas puertas y spawnear la OLEADA 1: un foot
                // soldier de cada hueco de ascensor + 2 refuerzos entrando
                // por ambos lados de la cámara (30/08:
                // antes salían solo los 2 de los ascensores).
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
                {
                    // cameraLockX ya está fijo en ZONE4_ELEV_LOCK desde que
                    // se dispararon las puertas (elevPhase 0->1).
                    s16 camL = cameraLockX;
                    s16 camR = cameraLockX + SCREEN_PIXEL_WIDTH;
                    for (u16 s = 0; s < 2; s++) {
                        for (u16 i = 0; i < MAX_ENEMIES; i++) {
                            if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                                if (s == 0)
                                    initEnemySomersaultSpawn(&enemies[i], camL - ENEMY_SPRITE_W_PURPLE, 170,
                                                             1, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                                else
                                    initEnemyKickSpawn(&enemies[i], camR, 196,
                                                       -1, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                                activeEnemies++;
                                break;
                            }
                        }
                    }
                }
                elevPhase = 3;   // oleada 1 activa: esperar a que caigan todos
            } else if (elevPhase == 3) {
                // Oleada 1 despejada -> disparar la OLEADA 2, una emboscada
                // completa por ambos lados (30/08). Solo
                // al limpiar ESTA oleada se desbloquea la cámara (ver
                // combatZone == 7 más abajo).
                if (activeEnemies == 0) {
                    s16 camL = cameraLockX;
                    s16 camR = cameraLockX + SCREEN_PIXEL_WIDTH;
                    for (u16 s = 0; s < 4; s++) {
                        for (u16 i = 0; i < MAX_ENEMIES; i++) {
                            if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                                if (s == 0)
                                    initEnemySomersaultSpawn(&enemies[i], camL - ENEMY_SPRITE_W_PURPLE, 148,
                                                             1, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                                else if (s == 1)
                                    // Este era el 2do morado de la emboscada;
                                    // desde el 13/09 es el foot soldier BLANCO
                                    // de espada larga, que
                                    // entra saltando por la derecha. Desde el
                                    // 24/09 PAL3 es SOLO suya: el naranja se
                                    // mudo a PAL2 (comparte sheet de colores
                                    // con el morado).
                                    initEnemyWalkInSpawn(&enemies[i], camR, 163, -1, PAL3,   // (29/09) entra caminando
                                                         ENEMY_TYPE_FOOT_SOLDIER_WHITE);
                                else if (s == 2)
                                    initEnemySomersaultSpawn(&enemies[i], camL - ENEMY_SPRITE_W_PURPLE, 196,
                                                             1, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                                else
                                    initEnemyWalkInSpawn(&enemies[i], camR, 191,   // (29/09) caminando
                                                         -1, PAL2, ENEMY_TYPE_FOOT_SOLDIER_ORANGE);
                                activeEnemies++;
                                break;
                            }
                        }
                    }
                    elevPhase = 4;   // oleada 2 activa
                }
            } else if (elevPhase == 4) {
                // Oleada 2 despejada -> encuentro de ascensores terminado.
                if (activeEnemies == 0)
                    elevPhase = 5;   // hecho: no vuelve a disparar
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

            // --- Zona 1: cameraX >= 150 → 2 morados kick + 1 naranja kick + 1 morado espalda ---
            if (combatZone == 0 && cameraX >= ZONE1_CAM_LOCK) {
                combatZone = 1;
                cameraLockX = ZONE1_CAM_LOCK;
                camL = cameraLockX;
                camR = cameraLockX + SCREEN_PIXEL_WIDTH;
                for (s = 0; s < 4; s++) {
                    for (u16 i = 0; i < MAX_ENEMIES; i++) {
                        if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                            if (s == 0)
                                initEnemySomersaultSpawn(&enemies[i], camL - ENEMY_SPRITE_W_PURPLE, 160,
                                                         1, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                            else if (s == 1)
                                initEnemyKickSpawn(&enemies[i], camR, 160,
                                                   -1, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                            else if (s == 2)
                                initEnemyKickSpawn(&enemies[i], camR, 166,
                                                   -1, PAL2, ENEMY_TYPE_FOOT_SOLDIER_ORANGE);
                            else
                                // Cuarto morado (30/08): entra por
                                // la espalda con voltereta, Y=190 (>=24px de separación
                                // del resto de la oleada, vuelve a la proporción "2 y 2"
                                // del viejo sistema de oleadas antes de desactivarse).
                                initEnemySomersaultSpawn(&enemies[i], camL - ENEMY_SPRITE_W_PURPLE, 190,
                                                         1, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
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

            // --- Zona 2: cameraX >= 300 → 3 morados walk (izq Y=145, der Y=160 y Y=185)
            //     + (18/09) el DINAMITERO que se asoma por la escalera ---
            if (combatZone == 2 && cameraX >= ZONE2_CAM_LOCK) {
                cameraLockX = ZONE2_CAM_LOCK;
                combatZone = 3;
                camL = cameraLockX;
                camR = cameraLockX + SCREEN_PIXEL_WIDTH;
                // El de la escalera va PRIMERO: pide su VRAM antes que los
                // otros tres, que es la misma regla que se uso con los robots
                // del final (el que pide primero se la queda).
                for (u16 i = 0; i < MAX_ENEMIES; i++) {
                    if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                        initEnemyTntSpawn(&enemies[i], TNT_THROWER_X, TNT_THROWER_Y,
                                          +1, PAL2, TNT_LAND_X, TNT_LAND_Y);
                        enemies[i].jumpZ = TNT_THROWER_Z;
                        tntHitMask = 0;
                        activeEnemies++;
                        break;
                    }
                }
                for (s = 0; s < 3; s++) {
                    for (u16 i = 0; i < MAX_ENEMIES; i++) {
                        if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                            if (s == 0) {
                                // Voltereta desde la espalda: entra haciendo la
                                // voltereta y recién al terminar pasa a CHASE.
                                initEnemySomersaultSpawn(&enemies[i], camL - ENEMY_SPRITE_W_PURPLE, 145,
                                                         1, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                            } else if (s == 1) {
                                initEnemySpawn(&enemies[i], camR, 160,
                                               0, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                                enemies[i].dir = -1;
                                enemies[i].state = ENEMY_STATE_CHASE;
                            } else {
                                // Tercer morado (30/08): entra
                                // de frente, Y=185 (>=24px de separación del resto).
                                initEnemySpawn(&enemies[i], camR, 185,
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

            // --- Zona 3: cameraX >= 614 → 2 morados walk (izq Y=162, der Y=186) + 1 naranja walk der Y=150 ---
            if (combatZone == 4 && cameraX >= ZONE3_CAM_LOCK) {
                cameraLockX = ZONE3_CAM_LOCK;
                combatZone = 5;
                camL = cameraLockX;
                camR = cameraLockX + SCREEN_PIXEL_WIDTH;
                for (s = 0; s < 3; s++) {
                    for (u16 i = 0; i < MAX_ENEMIES; i++) {
                        if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                            if (s == 0) {
                                // Voltereta desde la espalda (entra del lado
                                // izquierdo de la cámara).
                                initEnemySomersaultSpawn(&enemies[i], camL - ENEMY_SPRITE_W_PURPLE, 162,
                                                         1, PAL2, ENEMY_TYPE_FOOT_SOLDIER);
                            } else if (s == 1) {
                                initEnemySpawn(&enemies[i], camR, 150,
                                               0, PAL2, ENEMY_TYPE_FOOT_SOLDIER_ORANGE);
                                enemies[i].dir = -1;
                                enemies[i].state = ENEMY_STATE_CHASE;
                            } else {
                                // Tercer enemigo (30/08): morado
                                // de frente, Y=186 (>=24px de separación del resto).
                                initEnemySpawn(&enemies[i], camR, 186,
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
            if (combatZone == 5 && activeEnemies == 0) {
                cameraLockX = -1;
                combatZone = 6;
            }

            // --- Zona 4 (ascensores): esperar centering → clear → desbloquear ---
            if (combatZone == 6 && elevPhase >= 1) {
                // Ascensores dispararon por centering y bloquearon cámara
                combatZone = 7;
            }
            if (combatZone == 7 && elevPhase >= 5) {
                // Las DOS oleadas del ascensor quedaron limpias → desbloquear
                // cámara, permitir avanzar (antes bastaba elevPhase>=2, o sea
                // la primera tanda de 2 enemigos; ahora hace falta llegar a
                // elevPhase==5, que el propio elevPhase==3/4 solo alcanza
                // cuando activeEnemies volvió a 0 dos veces seguidas).
                cameraLockX = -1;
                combatZone = 8;
            }

            // --- Zona 5 (robot): cameraX alcanza CAM_MAX_X → lock + spawn ---
            if (combatZone == 8 && cameraX >= CAM_MAX_X) {
                cameraLockX = ZONE5_ROBOT_LOCK;
                combatZone = 9;
                // (16/09) Primero los ROBOTS y despues el naranja de escolta:
                // el que pide VRAM de sprites primero se la queda.
                // Robot(es): todos en el mismo eje X, cada uno en su lane.
                //   1 jugador  -> 1 robot   · 2 jugadores -> 2
                //   4 jugadores-> 4 (16/09, pedido explicito para medir el
                //                    parpadeo real de la consola)
                // OJO VRAM: cada robot son 73 tiles de sprite en su frame
                // pico. Si el presupuesto (SPR_initEx) se agota, SPR_addSprite
                // devuelve NULL y un robot sin sprite NO ACTUA (robotUpdate
                // sale en la primera linea) -> el nivel quedaria imposible de
                // terminar. Por eso robotSpawn ahora marca ROBOT_GONE al que
                // no consiguio sprite (ver robot.c), y la victoria no lo
                // espera.
                {
                    static const s16 robotLane2[LEVEL1_MAX_ROBOTS] = {
                        ROBOT_SPAWN_Y, ROBOT_SPAWN_Y2, ROBOT_SPAWN_Y, ROBOT_SPAWN_Y2
                    };
                    static const s16 robotLane4[LEVEL1_MAX_ROBOTS] = {
                        ROBOT_SPAWN_Y_4P_0, ROBOT_SPAWN_Y_4P_1,
                        ROBOT_SPAWN_Y_4P_2, ROBOT_SPAWN_Y_4P_3
                    };
                    // Con cuatro, las lanes quedan a 19px una de otra: si
                    // ademas compartieran el eje X se verian como UN solo
                    // robot apilado. Se los separa en zigzag (+-56px).
                    static const s16 robotDx4[LEVEL1_MAX_ROBOTS] = { -56, 56, -56, 56 };
                    const s16* lane = (nPl > 2) ? robotLane4 : robotLane2;
                    for (u8 r = 0; r < nRobots; r++) {
                        if (robots[r].state != ROBOT_INACTIVE) continue;
                        s16 rx = ROBOT_SPAWN_CENTER + ((nPl > 2) ? robotDx4[r] : 0);
                        robotSpawn(&robots[r], rx, lane[r]);
                    }
                }

                // Naranja de escolta: entra desde la izquierda con el kick de
                // entrada (initEnemyKickSpawn lo deja en SPAWNING mientras
                // avanza desde fuera de pantalla; antes se creaba ya en CHASE
                // y el clamp de enemyMinX lo teletransportaba al borde -- bug
                // del 29/08).
                // (16/09) Con 4 jugadores NO sale: son 56 tiles de sprite y
                // los necesitan los cuatro robots (4x73). El presupuesto del
                // nivel en 4P es 820 y el pico ya es 4 tortugas (256) + 4
                // marcos de HUD (144) + 4 robots (292) = 692.
                if (nPl <= 2) {
                    for (u16 i = 0; i < MAX_ENEMIES; i++) {
                        if (enemies[i].state == ENEMY_STATE_INACTIVE) {
                            initEnemyWalkInSpawn(&enemies[i],   // (29/09) entra caminando
                                                 cameraLockX - ENEMY_SPRITE_W_ORANGE, 160,
                                                 1, PAL2, ENEMY_TYPE_FOOT_SOLDIER_ORANGE);
                            activeEnemies++;
                            break;
                        }
                    }
                }
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
            updateEnemyN(&enemies[i], pls, nPl);
        }

        // 5c. Robot del látigo (mini-jefe): spawneado por zona 4 (combatZone == 9).
        //     Solo update: la máquina de estados corre por su cuenta.
        for (u8 r = 0; r < nRobots; r++)
            robotUpdateN(&robots[r], cameraX, pls, nPl, fps);

        // 5d. Shurikens: actualizar posición, auto-destrucción off-screen.
        shurikenUpdate(cameraX);

        // 5e. Dinamita de la escalera (18/09): vuelo + explosión. Devuelve TRUE
        //     el frame del impacto, que es cuando va el SFX. La explosión se
        //     libera sola al terminar su animación.
        if (tntUpdate(cameraX)) {
            XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                           SOUND_PCM_CH3, 15, FALSE, FALSE);
            tntHitMask = 0;
        }
        // Daño de la explosión: sólo a las tortugas (los foot soldiers no se
        // dañan entre ellos, igual que las bolas de hierro). Un golpe por
        // jugador y por explosión.
        if (tntBlastActive()) {
            for (u8 k = 0; k < nPl; k++) {
                if (tntHitMask & (1 << k)) continue;
                if (!playerCanBeHit(pls[k])) continue;
                s16 pcx = getPlayerHurtCX(pls[k]);   // (02/10) centro de la hurtbox
                if (!tntBlastHits(pcx, getPlayerY(pls[k]), PLAYER_BODY_HALF_W)) continue;
                tntHitMask |= (u8)(1 << k);
                playerHitBars(pls[k], tntBlastX(), TNT_BLAST_DMG);
            }
        }

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

            // Golpe contra la HURTBOX del cuerpo del soldier (no contra el
            // borde transparente del frame): playerAttackHitsBox solapa la
            // caja del ataque con el cuerpo real segun el tipo.
            s16     dmg      = 0;
            Player* attacker = NULL;
            for (u8 k = 0; k < nPl; k++) {
                if (!playerAttackHitsEnemy(pls[k], &enemies[i])) continue;   // (02/10) hurtbox por tipo
                dmg = isPlayerSpecialAttack(pls[k]) ? ENEMY_HP : 1; attacker = pls[k];
                break;
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
                    // (27/09) El golpe que lo mata lo lanza (ver ENEMY_DEATH_PUSH).
                    enemyDeathPush(&enemies[i], (s16)(attacker->x + PLAYER_SPRITE_W / 2),
                                   getPlayerDir(attacker), isPlayerSpecialAttack(attacker));
                }
            }
        }

        // 6-robot. Ataque del jugador → robot del látigo. Cualquier golpe le
        //          saca uno (ROBOT_HP golpes). Los ataques del robot al jugador
        //          (láser/agarre) se resuelven dentro de robotUpdate. El golpe
        //          además lo hace retroceder lejos del atacante: se le pasa su
        //          X para calcular el sentido (ver robotDamage en robot.c).
        // (16/09) Un solo bucle para los N robots (antes habia dos bloques
        // copiados, uno por robot; con cuatro no escalaba).
        for (u8 r = 0; r < nRobots; r++) {
            if (!robotCanBeHit(&robots[r])) continue;
            s16 rx = robotGetCenterX(&robots[r]);
            s16 ry = robotGetCenterY(&robots[r]);
            s16     rdmg = 0;
            Player* ratt = NULL;
            for (u8 k = 0; k < nPl; k++) {
                if (!playerAttackHits(pls[k], rx, ry)) continue;
                rdmg = isPlayerSpecialAttack(pls[k]) ? ROBOT_SPECIAL_DMG : 1;
                ratt = pls[k];
                break;
            }
            if (rdmg > 0 && ratt) {
                robotDamage(&robots[r], rdmg, getPlayerWorldX(ratt) + PLAYER_SPRITE_W / 2);
                // Impacto de la patada con salto también contra el mini-jefe
                if (isPlayerJumpKicking(ratt))
                    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                if (robots[r].state == ROBOT_DEAD)
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

            // El swing es UNO: pega al primer jugador alcanzado y se consume.
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                if (!enemyTryHitPlayerBox(e, getPlayerHurtX(pls[k]), getPlayerY(pls[k]),
                                          PLAYER_BODY_HALF_W)) continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(pls[k], getEnemyCenterX(e));
                break;
            }
        }

        // 6b-bis. Shurikens → ataque del jugador: la hitbox del golpe ROMPE los
        //     shurikens que cruza (desaparecen sin dañar). Va ANTES de la
        //     colisión proyectil→jugador: un shuriken roto este frame no pega.
        {
            for (u8 k = 0; k < nPl; k++) {
                if (shurikenBreakByPlayerAttack(pls[k]))
                    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
            }
        }

        // 6b-bis. Shurikens → jugadores: colisión proyectil.
        {
            s16 hitX = 0;
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                if (!shurikenCheckHitPlayer(getPlayerWorldX(pls[k]), getPlayerY(pls[k]), &hitX)) continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(pls[k], hitX);
            }
        }

        // 6b-bis. Bolas de hierro (x2): spawn periódico, física de rebote y
        //     colisiones (resta 1 barra al jugador; NO afectan a los foot
        //     soldiers -- ver nota en ironBallUpdate, 30/08). Va ANTES del
        //     refresco del HUD para que el daño se vea el mismo frame, y
        //     antes del game over para que un golpe fatal cuente.
        ironBallUpdate(&ironBall, cameraX, pls, nPl);
        ironBallUpdate(&ironBall2, cameraX, pls, nPl);

        // 6c. HUD: refrescar barra de vida, vidas y puntaje (solo redibuja lo
        //     que cambió respecto del frame anterior).
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
                hudPlayerInit(&huds[1], &p2, hudPlayerCol(1),
                              (u16)(hudVramFree + HUD_VRAM_PER_PLAYER));
                nPl          = 2;
                dosJugadores = TRUE;
                resetEnemyAI(2);
            }
        }

        for (u8 k = 0; k < nPl; k++) hudPlayerUpdate(&huds[k]);

        // 6d. Continues: si un jugador cayó sin vidas, su marco muestra
        //     "CONTINUE?" con cuenta regresiva (START = seguir con tortuga
        //     nueva). Queda fuera solo cuando la cuenta llega a 0; en 2P el
        //     compañero vivo sigue jugando mientras tanto. El nivel termina
        //     cuando TODOS los jugadores quedaron fuera.
        if (continueStepAll(conts, pls, huds, nPl, fps)) break;

        // 6e. Victoria: robot(es) destruido(s) y sin enemigos en pantalla ->
        //     arranca la secuencia de salida (ver después del bucle). En 2
        //     jugadores hay que esperar a que AMBOS robots estén GONE.
        // (16/09) Con N robots: basta con que ninguno siga en juego. Los que
        // nunca se spawnearon quedan ROBOT_INACTIVE y los que se quedaron sin
        // VRAM de sprite, ROBOT_GONE (ver robotSpawn) -- ninguno traba la
        // victoria. Pero exigimos que AL MENOS UNO haya llegado a GONE, para
        // no dar por ganado el nivel antes de que aparezca el mini-jefe.
        bool robotsDone = (combatZone >= 9);
        bool algunoGone = FALSE;
        for (u8 r = 0; r < nRobots && robotsDone; r++) {
            if (robots[r].state == ROBOT_GONE) algunoGone = TRUE;
            else if (robots[r].state != ROBOT_INACTIVE) robotsDone = FALSE;
        }
        if (robotsDone && algunoGone && activeEnemies == 0) {
            XGM2_playPCMEx(scream_april, sizeof(scream_april), SOUND_PCM_CH2, 15, FALSE, FALSE);
            win = TRUE;
            break;
        }

        // 7. Revelar columnas nuevas del fondo y aplicar el scroll
        bgUpdate(cameraX);

        // 8. Animar el fuego del primer plano + su scroll de parallax (deriva
        //    con la cámara a FIRE_SCROLL_NUM/DEN de la velocidad del fondo)
        fireUpdate(cameraX);

        // 8b. Sparks: streaming de tiles constante durante el nivel (puertas +
        //     ascensores + sparks_2), ya NO toca CRAM -- ver sparksStreamUpdate.
        sparksStreamUpdate();

        // 8c. Fuego del piso: se crea cerca de camara, fijo en el mundo.
        for (u16 ff = 0; ff < FLOOR_FIRE_COUNT && sparksFullOn; ff++) {
            s16 fsx = floorFireX[ff] - cameraX;
            bool near = (fsx > -FLOOR_FIRE_MARGIN) &&
                        (fsx < SCREEN_PIXEL_WIDTH + FLOOR_FIRE_MARGIN);
            if (near && !floorFireSpr[ff]) {
                floorFireSpr[ff] = SPR_addSprite(&floor_fire, 0, 0,
                                                 TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
                if (floorFireSpr[ff]) SPR_setDepth(floorFireSpr[ff], -floorFireY[ff]);
            } else if (!near && floorFireSpr[ff]) {
                SPR_releaseSprite(floorFireSpr[ff]);
                floorFireSpr[ff] = NULL;
            }
            if (floorFireSpr[ff])
                SPR_setPosition(floorFireSpr[ff], fsx - FLOOR_FIRE_HALF_W,
                                floorFireY[ff] - FLOOR_FIRE_H);
        }

        SPR_update();
        SYS_doVBlankProcess();
    }

    // Las bolas no deben seguir vivas en la cutscene de victoria (evita que
    // queden congeladas en pantalla o golpeen durante el paseo scripteado del outro).
    ironBallEnd(&ironBall);
    ironBallEnd(&ironBall2);

    // Liberar shurikens activos (evita que queden volando en la cutscene).
    shurikenReleaseAll();
    tntReleaseAll();   // dinamita de la escalera (18/09)

    // Liberar sparks que pudieran quedar visibles.
    for (u16 d = 0; d < LEVEL1_DOOR_COUNT; d++) {
        if (sparkSpr[d]) { SPR_releaseSprite(sparkSpr[d]); sparkSpr[d] = NULL; }
    }
    for (u16 ev = 0; ev < LEVEL1_ELEV_COUNT; ev++) {
        if (elevSparkSpr[ev]) { SPR_releaseSprite(elevSparkSpr[ev]); elevSparkSpr[ev] = NULL; }
    }
    for (u16 ff = 0; ff < FLOOR_FIRE_COUNT; ff++)
        if (floorFireSpr[ff]) { SPR_releaseSprite(floorFireSpr[ff]); floorFireSpr[ff] = NULL; }
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
        for (u8 k = 1; k < nPl; k++) playerPersistSave(pls[k]);

        for (u8 k = 0; k < nPl; k++) setPlayerCamera(pls[k], cameraX);

        // 1) Quieto OUTRO_STAND_SECS segundos
        u16 standT = fps * OUTRO_STAND_SECS;
        while (standT-- > 0) {
            for (u8 k = 0; k < nPl; k++) playerCutsceneStand(pls[k]);
            bgUpdate(cameraX);
            fireUpdate(cameraX);
            SPR_update();
            SYS_doVBlankProcess();
        }

        // 2) Caminan TODOS hacia la puerta, en abanico para no encimarse.
        // (15/09) Antes el escalonado era de un ANCHO DE SPRITE por jugador
        // (104px), y con cuatro el tercero y el cuarto terminaban apuntando a
        // X 1035 y 931 -- o sea, FUERA de la pantalla por la izquierda, porque
        // la camara esta clavada en ZONE5_ROBOT_LOCK (1056). Se los veia
        // caminar para el otro lado (o directamente no se los veia) y parecia
        // que solo el jugador 1 iba a la puerta. Ahora el escalonado es de
        // OUTRO_FAN_X en X y OUTRO_FAN_Y en la lane: los cuatro quedan en el
        // mismo grupo delante de la puerta y todos dentro de cuadro.
        bool walking = TRUE;
        while (walking) {
            bool allArrived = TRUE;
            for (u8 k = 0; k < nPl; k++)
                if (!playerCutsceneWalkTo(pls[k],
                                          OUTRO_DOOR_X - (s16)k * OUTRO_FAN_X,
                                          OUTRO_DOOR_Y + (s16)k * OUTRO_FAN_Y))
                    allArrived = FALSE;
            walking = !allArrived;
            bgUpdate(cameraX);
            fireUpdate(cameraX);
            SPR_update();
            SYS_doVBlankProcess();
        }

        // 3) Fundido y al nivel 2 (pasillo en llamas, 2da parte)
        PAL_fadeOutAll(30, FALSE);
        while (PAL_isDoingFade()) SYS_doVBlankProcess();

        clearScene();
        return SCENE_1_2;
    }

    clearScene();
    return (jump != PAUSE_NO_JUMP) ? jump : SCENE_GAME_OVER;
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
// (22/09, rama bg-nivel1-2-paleta-unica) El fondo nuevo mide 224 = la
// pantalla entera: se dibuja desde la fila 0 y ya no queda la franja negra de
// arriba (antes el fondo era de 192 y arrancaba en la fila 4). Las 4 filas de
// la base (24-27) las sigue tapando el fuego. El offset se calcula del alto
// REAL del tilemap (ver bgInit2), asi que un fondo de 192 volveria solo a 4.
#define LEVEL2_BG_OFFSET_Y   (SCROLL_TILE_ROWS - bg_test.tilemap->h)

// --- Pared diagonal del sofa (esquina inferior-izq de la sala) ---
// El arte de bg_test.png tiene el sofa "cortado" contra el borde izquierdo,
// con el piso libre recortado en diagonal delante de el. No habia ningun
// chequeo de colision para esto (a diferencia del muro del fondo/puerta a
// la derecha, que ya viene resuelto por otro lado) y el player lo podia
// atravesar caminando. Mismo patron que levelEndWallX (res/player.c):
// interpolacion lineal de un limite en X segun la Y (profundidad) del
// jugador, medido a ojo sobre el overlay del 30/08.
// Si al jugarlo queda muy ajustado o muy suelto, tocar estas 2 constantes.
#define SOFA_WALL_X_TOP      75   // limite en X cuando Y=BOUND_LANE_TOP (fondo)
#define SOFA_WALL_X_BOTTOM   17   // limite en X cuando Y=BOUND_LANE_BOTTOM (frente)

static s16 sofaWallX(s16 y) {
    s32 laneRange = BOUND_LANE_BOTTOM - BOUND_LANE_TOP;
    s32 wallRange = SOFA_WALL_X_BOTTOM - SOFA_WALL_X_TOP;
    return SOFA_WALL_X_TOP + (s16)(wallRange * (y - BOUND_LANE_TOP) / laneRange);
}

// (22/09, rama bg-nivel1-2-paleta-unica) Humo NUEVO del artista: 3 frames de
// 128x64 (16x8 tiles) en la MISMA paleta que el fondo (PAL0), purpura con el
// borde de fuego. Cargado entero serian 128 tiles por frame contra los 64 del
// humo viejo, y la VRAM de usuario de este nivel no tiene ese lugar. Pero cada
// frame deduplicado son ~42 tiles (la mitad es purpura liso o vacio), asi que
// se streamea igual que antes, un frame a la vez, pero con sus tiles UNICOS
// (SMOKE2_TILES) y un MAPA propio (smoke2Map) que se vuelve a escribir en el
// plano en cada paso. Los genera tools/gen_nivel1_2_artista.py.
#include "smoke_lvl1_2.h"
#define SMOKE_CELL_TILES_W   SMOKE2_CELL_W   // 16
#define SMOKE_CELL_TILES_H   SMOKE2_CELL_H   // 8
#define SMOKE_CELL_TILES     64   // lugar reservado en VRAM (no cambia el layout;
                                  // el frame nuevo mas grande usa SMOKE2_TILES = 42)
#define SMOKE_FRAMES         SMOKE2_FRAMES   // 3
#define SMOKE_FRAME_INTERVAL 8    // Frames de juego entre cada frame de humo
// (22/09) El fondo lleva pintada una franja de violeta liso detras del HUD
// (filas 0-3), y en el plano de adelante (BG_A) va SOLO la parte animada del
// humo, justo debajo: las SMOKE2_SOLID_ROWS filas lisas de arriba de la celda
// se CORTARON (con ellas el humo bajaba hasta y=95 y tapaba
// demasiado fondo). El HUD (texto en BG_A, filas 0-3) no se toca. La union no
// se nota aunque BG_A tenga parallax y BG_B no: cae adentro del violeta liso.
#define SMOKE_Y_TILE         4    // justo debajo del HUD (filas 0-3)

// La cápsula del taladro (sprite de la fase 2) debe quedar DETRÁS del humo del
// techo. Como la prioridad del plano es global por TILE, solo las columnas del
// plano que cubren la cápsula se pintan con PRIORIDAD ALTA (TRUE): ahí el humo
// se dibuja delante de la cápsula (sprite, prioridad 0). El resto del humo
// sigue con prioridad baja (detrás de los sprites). Con la cámara bloqueada en
// 120 toda la pelea (fase 2), la cápsula anclada a pantalla (172..268, ±8 de
// temblor) cubre las columnas del plano (screen + 120) / 8 = 35..49.
// (22/09) Corregido: el humo de BG_A scrollea con PARALLAX (FIRE_SCROLL_NUM/
// DEN = 1/2 de la camara), asi que con la camara en 120 su scroll es 60, no
// 120. La cuenta vieja ((pantalla + 120) / 8 = 35..49) daba columnas corridas
// y cubria la pantalla 220..340 en vez de la capsula (160..272 con el
// temblor). Con 60: (160 + 60) / 8 = 27 .. (280 + 60) / 8 = 42.
#define SMOKE_FRONT_COL_MIN   27
#define SMOKE_FRONT_COL_MAX   42


static u16 smokeVramInd;   // Primer tile de VRAM de la celda del humo
static u16 smokeFrame;     // Frame de animación actual (0..7)
static u16 smokeTimer;     // Contador hasta el próximo paso
static s16 smokeScrollTbl[SMOKE_CELL_TILES_H];  // H-scroll de las 8 filas del humo
// Mapa de la banda del humo (64 columnas x 8 filas) ya con atributos, listo
// para mandarlo al plano por DMA. Se rearma en cada paso de animacion. Tiene
// que ser static: con DMA_QUEUE la transferencia sale en el vblank siguiente.
static u16 smokeMapBuf[SMOKE_CELL_TILES_H * BG_PLANE_W];

// Arma smokeMapBuf para el frame 'f': la celda de 16x8 repetida a lo ancho del
// plano. Prioridad alta en las columnas de la capsula (ver SMOKE_FRONT_COL_*).
static void smokeBuildMap(u16 f) {
    for (u16 r = 0; r < SMOKE_CELL_TILES_H; r++) {
        for (u16 col = 0; col < BG_PLANE_W; col++) {
            u8  t = smoke2Map[f][r][col % SMOKE_CELL_TILES_W];
            u16 v = 0;                                   // vacio: tile 0 (transparente)
            if (t != 0xFF) {
                bool front = (col >= SMOKE_FRONT_COL_MIN && col <= SMOKE_FRONT_COL_MAX);
                v = TILE_ATTR_FULL(PAL0, front, FALSE, FALSE, smokeVramInd + t);
            }
            smokeMapBuf[r * BG_PLANE_W + col] = v;
        }
    }
}

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

    // Frame 0 a VRAM (sus tiles unicos) y su mapa al plano, filas 4-11.
    // PRIORIDAD BAJA salvo en las columnas de la capsula (smokeBuildMap).
    VDP_loadTileData(smoke_tiles.tiles, vramInd, SMOKE2_TILES, DMA);
    smokeBuildMap(0);
    VDP_setTileMapDataRect(BG_A, smokeMapBuf, 0, SMOKE_Y_TILE,
                           BG_PLANE_W, SMOKE_CELL_TILES_H, BG_PLANE_W, DMA);
    // El scroll de la banda arranca en 0: fireInit ya puso TODA la tabla de
    // BG_A en 0, así que no hay que escribirla acá.
}

// Avanza la animación del humo + su scroll de parallax (misma técnica que el
// fuego). Recibe la cámara y se llama una vez por frame en el bucle del nivel.
static void smokeUpdate(s16 cameraX) {
    if (++smokeTimer >= SMOKE_FRAME_INTERVAL) {
        smokeTimer = 0;
        // 3 frames: ya no es potencia de 2, no sirve la mascara.
        if (++smokeFrame >= SMOKE_FRAMES) smokeFrame = 0;
        // Pisar los MISMOS tiles de VRAM con los del frame siguiente (el frame
        // N arranca en tiles + N*SMOKE2_TILES*8 longwords) Y reescribir el
        // mapa de la banda: cada frame tiene su propio orden de tiles. Los dos
        // van por DMA_QUEUE, o sea que salen en el MISMO vblank y nunca se ve
        // un mapa viejo con tiles nuevos.
        VDP_loadTileData(smoke_tiles.tiles + (smokeFrame * SMOKE2_TILES * 8),
                         smokeVramInd, SMOKE2_TILES, DMA_QUEUE);
        smokeBuildMap(smokeFrame);
        VDP_setTileMapDataRect(BG_A, smokeMapBuf, 0, SMOKE_Y_TILE,
                               BG_PLANE_W, SMOKE_CELL_TILES_H, BG_PLANE_W, DMA_QUEUE);
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
#define CAPSULA_TILE_H        15    // 120px (22/09: el humo subio, ver abajo)
// (22/09, rama bg-nivel1-2-paleta-unica) Arte nuevo del artista: la capsula
// ya NO la ubica el codigo sino el dibujo. tools/gen_nivel1_2_artista.py
// encuentra cada frame adentro del fondo y los deja en celdas de 96x120
// ancladas en el mundo en (288,40) -> en pantalla, con la camara en 120,
// (168,40). Antes era (172,51) a ojo. Arranca en y=40 porque ahi el humo
// (que ahora baja solo hasta ~y=45 opaco) todavia tapa su tope recortado.
#define CAPSULA_CENTER_X      216   // Centro en pantalla (x_mundo 336 - cámara 120)
#define CAPSULA_CENTER_Y      100
#define CAPSULA_SCREEN_X      (CAPSULA_CENTER_X - (CAPSULA_TILE_W * 8) / 2)   // 168
#define CAPSULA_SCREEN_Y      (CAPSULA_CENTER_Y - (CAPSULA_TILE_H * 8) / 2)   // 40
// Profundidad de la capsula. ANTES salia de -(CAPSULA_CENTER_Y) = -103, y al
// mover el centro a 108 quedo EMPATADA con Shredder (-APRIL_LANE_Y + 40 =
// -108): con el empate, Shredder pasaba por DETRAS de la capsula al saltar a la
// ventana (22/09). Ahora es fija y no depende de la
// geometria: -103 = detras de Shredder (-108), de los jugadores y del jefe
// (-y con y >= 118).
#define CAPSULA_DEPTH         (-103)
// El arte nuevo trae 8 frames del taladro + la capsula cerrada (9) y solo dos
// de la puerta (cerrada y abierta). 9 x 23 = 207 ticks ~ los 203 de antes, que
// es lo que dura drill.wav.
#define CAPSULA_FRAMES        9     // Frames del índice [0] (emergencia)
#define CAPSULA_FRAME_TICKS   23    // Frames de juego entre frames
#define CAPSULA_DOOR_FRAMES   2     // Frames del índice [1] (cerrada, abierta)
#define CAPSULA_DOOR_TICKS    28    // La puerta se abre a mitad de lo que tardaba antes
#define CAPSULA_SHAKE_AMP     8     // Amplitud del temblor de pantalla (px)

// Flash de paleta por HP bajo del jefe (efecto "quemado" brillante). Con poca
// vida la paleta de Rocksteady alterna entre la normal y una versión quemada
// cada ROCKSTEADY_FLASH_TICKS frames (4/4 constante, ver boss_flash.h).
// Umbral RELATIVO al HP total del jefe (el ultimo cuarto, BOSS_FLASH_DIV),
// no un numero suelto, para que no se desfase si cambia ROCKSTEADY_HP.
#define ROCKSTEADY_FLASH_HP        (ROCKSTEADY_HP / BOSS_FLASH_DIV)
#define ROCKSTEADY_FLASH_CRIT_HP   (ROCKSTEADY_HP / (BOSS_FLASH_DIV * 2))
#define ROCKSTEADY_FLASH_TICKS      BOSS_FLASH_TICKS
#define ROCKSTEADY_FLASH_CRIT_TICKS BOSS_FLASH_TICKS

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
#define SHREDDER_RAPTO_TICKS   6     // Ticks por cada frame 0 y 1 del Rapto — reducido ~0.4s
#define SHREDDER_JUMP_FRAMES   36    // Duración del vuelo en arco (frames) — reducido ~1s
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

SceneId showScene12() {
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
    // (14/09, revisado el 16/09) Con 3-4 jugadores las barras suben de 2 a 4
    // bloques (+8 tiles de usuario), asi que el techo baja de 773 a 765: se
    // usa 760, con el mismo margen de 5 que el 768 de 1-2 jugadores. Desde el
    // 16/09 el HUD de 4 tambien lleva los cuatro MARCOS (4x36 = 144 tiles de
    // sprite). Con cuatro tortugas de 64 el pico se pasa del presupuesto en
    // los momentos cargados y el motor deja de dibujar algun sprite -- es un
    // modo de PRUEBA y se acepto asi (parpadeo tolerable).
    SPR_initEx((numJugadores() > 2) ? 760 : 768);

    // --- Fondo (sala de 440px): dibujo completo + scroll (sin streaming) ---
    // Mapa de paletas (igual que el nivel 1):
    //   PAL0 → fondo | PAL1 → tortugas + humo | PAL2 → foot soldiers (morado + naranja) + fuego | PAL3 → foot soldier blanco
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

    // --- La paleta PAL3 (foot soldier BLANCO) la carga levelFadeIn al final
    //     del setup. El naranja se mudo a PAL2 el 24/09 ---

    // --- Música del nivel (los SFX por PCM siguen activos) ---
    playMusicVol(music_level2, VOL_MUSIC_LEVEL2);

    // --- Inicializar jugador(es) ---
    // (14/09) nPl = jugadores presentes (1..4). 'dosJugadores' se mantiene con
    // el sentido de "hay mas de uno", que es como lo usa el resto del nivel.
    u8   nPl = numJugadores();
    bool dosJugadores = (nPl >= 2);

    Player p1, p2, p3, p4;
    Player* pls[MAX_PLAYERS] = { &p1, &p2, &p3, &p4 };
    initPlayer(&p1, personajeSeleccionado, JOY_1, PAL1, 40, 182);
    setPlayerRightBound(&p1, SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W);

    // Jugadores 2..4: se reparten en X para no nacer uno encima del otro.
    // (17/09) Nacian en 160 / 200 / 240, o sea FUERA de la franja muerta de la
    // camara (CAM_DEAD_ZONE_RIGHT = 120). Como la camara sigue al que mas
    // avanzo, el nivel arrancaba solo: 40px de scroll involuntario con 2
    // jugadores y 120px con 4. Ahora nacen pegados al P1 y todos adentro.
    {
        const s16 spreadX = (nPl > 2) ? 26 : 64;   // 104 / 66-92-118
        for (u8 k = 1; k < nPl; k++) {
            initPlayer(pls[k], playerChar(k), playerJoy(k), PAL1,
                       (s16)(40 + k * spreadX), 182);
            setPlayerRightBound(pls[k], SCREEN_PIXEL_WIDTH - PLAYER_SPRITE_W);
        }
    }

    // --- April (rehén) atada al fondo de la sala ---
    // Decorativa: usa PAL1 (tortugas, ya cargada). Fondo del mundo (x=205,
    // lane 148) y con la MISMA convención de depth por Y que los jugadores/
    // el jefe (-y, ver player.c y rocksteady.c): antes tenía un +20 fijo que
    // la mandaba siempre detrás de cualquier jugador (el mínimo Y posible de
    // un jugador, BOUND_LANE_TOP=142, ya perdía contra 148-20=128). Ahora
    // tiene prioridad sobre una tortuga cuando esa tortuga
    // está más arriba en Y que ella (Y < 148): con -y puro, quien tenga
    // mayor Y (más "adelante" en el lane) dibuja al frente, igual que entre
    // dos jugadores o un jugador y un enemigo.
    static const s16 APRIL_WORLD_X    = 160;
    static const s16 APRIL_LANE_Y     = 148;
    static const s16 APRIL_FOOT_OFFSET = 58;
    Sprite* aprilSpr = SPR_addSprite(&april, APRIL_WORLD_X /*cámara en 0 al inicio*/,
                                     APRIL_LANE_Y - APRIL_FOOT_OFFSET,
                                     TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
    if (aprilSpr) SPR_setDepth(aprilSpr, -APRIL_LANE_Y);
    u16 aprilTimer = 0;

    // --- HUD dinámico: barra de vida + vidas + puntaje ---
    // Mismo esquema que el nivel 1: texto con hud_font en PAL1 (paleta de las
    // tortugas, ya cargada por initPlayer).
    VDP_loadFont(&hud_font, DMA);
    VDP_setTextPlane(BG_A);
    VDP_setTextPriority(1);
    VDP_setTextPalette(PAL1);

    p2JoinReset();   // (17/09) invitacion "PULSE START" en el marco vacio del P2
    static HudPlayer huds[MAX_PLAYERS];
    for (u8 k = 0; k < nPl; k++)
        hudPlayerInit(&huds[k], pls[k], hudPlayerCol(k),
                      (u16)(hudVramFree + k * HUD_VRAM_PER_PLAYER));

    // --- Estado de continues por jugador (cuenta regresiva + selección) ---
    static ContPlayer conts[MAX_PLAYERS];
    contResetAll(conts);

    // --- Pool de enemigos ---
    static Enemy enemies[MAX_ENEMIES];
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
    // (25/09) STATIC a proposito (igual que huds/conts/enemies/robots aca y en
    // showScene11): el stack de SGDK es de 2,5 KB (STACK_SIZE 0xA00) y estos
    // locales solos ocupaban ~1,4 KB por escena. Con 2 jugadores el stack se
    // pasaba y pisaba la marca de fin del heap (0xFFF5FE): MEM_pack, que
    // corre en el SPR_initEx del final del nivel, quedaba en un bucle
    // infinito -> juego colgado con Shredder en el aire. Todos se
    // reinicializan a mano justo despues de declararse.
    static Rocksteady boss;
    rocksteadyInit(&boss);
    rocksteadyBulletInit();
    u8      bossStage    = 0;
    u8      bossTimer    = 0;
    // Volumen actual de music_boss mientras se hace el ducking del taunt
    // (ver VOL_MUSIC_BOSS_DUCK). 0 = todavia no arranco el tema del jefe.
    u8      bossMusicVol = 0;
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
                foot_soldier_white.palette->data);

    // "Help me!" de April apenas se revela la sala (2da parte del nivel 1).
    // CH2 (no CH4: SOUND_PCM_CH4 no existe de verdad en el driver XGM2 de
    // SGDK -- solo tiene 3 canales PCM reales, PCM0..PCM2 = CH1..CH3 -- y
    // usarlo corrompia el comando del Z80, dejando un pitido constante;
    // bug encontrado 29/08, ver DEVLOG).
    XGM2_playPCMEx(help_me_april_vo, sizeof(help_me_april_vo), SOUND_PCM_CH2, 15, FALSE, FALSE);

    // --- Fases del nivel ---
    // 0 = oleada A (2 morados)  ·  1 = oleada B (2 blancos + 1 naranja)
    // 2 = sala libre (camara desbloqueada)  ·  3 = pelea con Rocksteady
    // La oleada B se agrego el 13/09: hasta entonces la
    // sala libre era la fase 1 y el jefe la 2, por eso todos los `phase == 3`
    // de mas abajo eran `phase == 2`.
    u8   phase       = 0;
    s16  cameraLockX = 0;    // >=0 = cameraX no puede superar este valor
    bool win         = FALSE;

    SceneId jump = PAUSE_NO_JUMP;   // (26/09) nivel elegido en el menu de pausa
    pauseReset();
    bossVoReset();
    while (1) {
        // 0. Pausa (START del control 1), antes de los continues.
        jump = pausePoll(pls, nPl);
        if (jump != PAUSE_NO_JUMP) break;
        bossVoUpdate();   // (01/10) el tema del jefe entra cuando termina su voz

        // 1. Input y física de cada jugador. Durante la cutscene de victoria la
        // tortuga deja de leer input y se queda congelada en el frame de
        // "caminar hacia arriba" (observando a Shredder llevarse a April).
        if (cutScene > 0) {
            // La tortuga viva se congela observando; la caída (game over) queda
            // tirada como estaba.
            for (u8 k = 0; k < nPl; k++)
                if (!isPlayerGameOver(pls[k])) playerCutsceneWatch(pls[k]);
        } else {
            for (u8 k = 0; k < nPl; k++) updatePlayer(pls[k]);
        }

        // Sofa de la esquina (no-walk): empuja al jugador fuera de la zona
        // diagonal si quedo mas a la izquierda de lo permitido para su Y.
        for (u8 k = 0; k < nPl; k++) {
            s16 wx = sofaWallX(pls[k]->y);
            if (pls[k]->x < wx) pls[k]->x = wx;
        }

        // 2. Cámara con dead-zone BIDIRECCIONAL (la sala se recorre de ida y
        //    vuelta). Derecha: igual que el nivel 1 (capped por cameraLockX).
        //    Izquierda: retrocede cuando el que va adelante queda muy atrás.
        // (26/09) Los que estan fuera de juego (sin vidas: contando el
        // CONTINUE? o ya fuera) no cuentan: su cuerpo no frena la camara.
        s16 leadX, trailX;
        playersSpanX(pls, nPl, &leadX, &trailX);
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
        } else if (leadScreenX < CAM_DEAD_ZONE_LEFT && cameraX > 0 && phase != 3) {
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
        for (u8 k = 1; k < nPl; k++) {
            setPlayerCamera(pls[k], cameraX);
            setPlayerLeftBound(pls[k], cameraX);
            setPlayerRightBound(pls[k], rightBound);
        }

        // 4. Conteo de enemigos activos.
        u16 activeEnemies = 0;
        for (u16 i = 0; i < MAX_ENEMIES; i++)
            if (enemies[i].state != ENEMY_STATE_INACTIVE) activeEnemies++;

        // 4b. Fases / oleadas.
        if (phase == 0 && activeEnemies == 0) {
            // --- Oleada A limpia -> OLEADA B ---
            // Dos foot soldiers BLANCOS (espada larga) entrando POR LA
            // IZQUIERDA y un NARANJA por la derecha, antes de Rocksteady
            // (13/09). La camara sigue bloqueada: es una
            // emboscada, no se puede escapar hacia adelante.
            // Los blancos entran SALTANDO (su anim 6, el arco de 107px), uno
            // por lane para que no vengan en fila india; el naranja entra con
            // su patada de siempre.
            {
                s16 camL = cameraX;
                s16 camR = cameraX + SCREEN_PIXEL_WIDTH;
                for (u16 s = 0; s < 3; s++) {
                    for (u16 i = 0; i < MAX_ENEMIES; i++) {
                        if (enemies[i].state != ENEMY_STATE_INACTIVE) continue;
                        // Los dos blancos nacen en la MISMA X (el clamp de
                        // enemyMinX no deja nacer mas alla de -w, asi que
                        // separarlos en X no serviria de nada) y se separan
                        // por LANE: uno al fondo y otro al frente.
                        if (s == 0)
                            initEnemyWalkInSpawn(&enemies[i],   // (29/09) entra caminando
                                camL - ENEMY_SPRITE_W_WHITE, 158, 1, PAL3,
                                ENEMY_TYPE_FOOT_SOLDIER_WHITE);
                        else if (s == 1)
                            initEnemyWalkInSpawn(&enemies[i],
                                camL - ENEMY_SPRITE_W_WHITE, 192, 1, PAL3,
                                ENEMY_TYPE_FOOT_SOLDIER_WHITE);
                        else
                            initEnemyWalkInSpawn(&enemies[i], camR, 174,   // (29/09) caminando
                                                 -1, PAL2, ENEMY_TYPE_FOOT_SOLDIER_ORANGE);
                        activeEnemies++;
                        break;
                    }
                }
            }
            phase = 1;
        } else if (phase == 1 && activeEnemies == 0) {
            // Oleada B limpia: recien ahi se desbloquea la camara (sala libre,
            // ida y vuelta) y se puede avanzar hacia el jefe.
            cameraLockX = -1;
            phase = 2;
        } else if (phase == 2 && cameraX >= LEVEL2_CAM_MAX_X) {
            // Llegó al límite de la sala: cámara bloqueada y comienza la pelea
            // contra Rocksteady. La cápsula del taladro aparece en pantalla
            // (oculta hasta el stage 1, cuando empieza a emerger del piso).
            cameraLockX = LEVEL2_CAM_MAX_X;
            phase = 3;
            bossStage = 0;
            bossTimer = 0;
            // SPR_addSpriteSafe (no SPR_addSprite): la capsula es GRANDE
            // (hasta 112 tiles en su frame mas grande, medido sobre el PNG
            // real -- el comentario viejo decia 106) y se crea reciEN
            // termina la oleada A, justo despues de que un monton de foot
            // soldiers chicos nacieron y murieron liberando/pisando VRAM.
            // SGDK avisa en su propia doc que SPR_addSprite puede fallar o
            // degradarse por FRAGMENTACION de VRAM aunque sobre espacio
            // libre en total (el hueco libre mas grande puede ser mas
            // chico que 112 tiles aunque haya de sobra repartido en varios
            // huecos) -- exactamente el sintoma reportado (parpadeo, se ve
            // mayormente transparente). SPR_addSpriteSafe reintenta con
            // SPR_defragVRAM() si la primera pasada falla.
            capsulaSpr = SPR_addSpriteSafe(&taladro_capsula, CAPSULA_SCREEN_X,
                                       CAPSULA_SCREEN_Y,
                                       TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
            if (capsulaSpr) {
                // OJO (29/08): SPR_MAX_DEPTH (0x7FFF) es el valor por
                // DEFECTO de cualquier sprite nuevo -- no la aleja de nada.
                // Ademas, como capsulaSpr se crea recien ACA (bien entrada
                // la fase 2), SGDK la inserta al FINAL de su lista interna
                // de sprites (orden de insercion, no de profundidad), asi
                // que es de las ultimas en procesarse en SPR_update(). SGDK
                // solo escribe hasta SAT_MAX_SIZE=80 "hardware sprites" por
                // frame (limite real de la VDP) y CUALQUIER sprite que quede
                // despues del cupo NO se dibuja ESE frame (se ve completa,
                // no a pedazos) -- exactamente el parpadeo reportado. Todo
                // lo demas en esta escena (jugadores, April, Rocksteady,
                // enemigos) usa profundidad = -(y) real; le damos a la
                // capsula una profundidad fija coherente con esa convencion
                // en lugar del maximo, para que un SPR_setDepth real la
                // reordene por profundidad (no por orden de insercion) y dejen
                // de competir sprites de fondo/HUD por su lugar en la SAT.
                SPR_setDepth(capsulaSpr, CAPSULA_DEPTH);
                SPR_setVisibility(capsulaSpr, HIDDEN);
            }
        } else if (cutScene == 0 && phase == 3 && bossSpawned &&
                   boss.state == ROCKSTEADY_GONE) {
            // Jefe muerto (el sprite ya se liberó solo al terminar la anim de
            // muerte): arranca la cutscene de victoria. Aplica aunque el jefe
            // muriera durante la introducción (bossStage < 99): se fuerza
            // bossStage = 99 para abortar el taunt y no esperar a que termine.
            // Se desactiva el flash de paleta para que nada pise la paleta de
            // Shredder en PAL3.
            bossFlashOn = 0;
            bossStage = 99;
            // (14/09) YA NO se devuelve el volumen al normal acá. Antes, si el
            // taunt se abortaba, el tema quedaba ducked y se restauraba a mano;
            // ahora el duck de la MUERTE tiene que sobrevivir hasta que la
            // cutscene corte la música (el frame siguiente, XGM2_stop en
            // cutScene 1), porque encima está sonando el grito del jefe. Subir
            // el volumen acá lo tapaba justo en el final.
            cutScene = 1;
            cutTimer = 0;
            // Si el jefe murió durante el taunt, suelta el globo de diálogo
            // que aún estuviera vivo (no debe verse en la cutscene).
            if (sayBubble) { SPR_releaseSprite(sayBubble); sayBubble = NULL; }
            bubblePhase = 3;
        }

        // 4c. Secuencia de introducción del jefe (fase 2, stages 0..3).
        if (phase == 3 && bossStage < 99) {
            switch (bossStage) {
                case 0:   // pausa dramática con la cápsula oculta
                    if (++bossTimer >= 30) { bossStage = 1; bossTimer = 0; }
                    break;
                case 1:   // la cápsula emerge del piso (frames 0..6) + temblor
                    // (27/09) SOLO al entrar al stage (capsulaFrame == 0). Antes
                    // la condicion era solo "bossTimer == 0", pero bossTimer
                    // vuelve a 0 cada CAPSULA_FRAME_TICKS al avanzar de frame:
                    // al frame siguiente se volvia a poner el FRAME 0 (casi
                    // vacio, la puntita del taladro). La capsula mostraba el
                    // frame bueno 1 solo frame de cada 23 -> el parpadeo
                    // durante la emergencia (quieta, en el stage 2, ya no
                    // pasaba por aca). Ademas reiniciaba el sonido del taladro
                    // cada 23 frames; ahora suena entero y, si termina o lo
                    // pisa otro sonido del canal (un golpe), se relanza al
                    // cambiar de frame. El de la puerta (stage 2) lo corta.
                    if (bossTimer == 0 && capsulaFrame == 0 && capsulaSpr) {
                        SPR_setVisibility(capsulaSpr, VISIBLE);
                        SPR_setAnimAndFrame(capsulaSpr, 0, 0);
                        XGM2_stop();   // cortar la música de fondo durante la secuencia
                        XGM2_playPCMEx(drill_sfx, sizeof(drill_sfx),
                                       SOUND_PCM_CH2, 15, FALSE, FALSE);
                    } else if (bossTimer == 0 && !XGM2_isPlayingPCM(SOUND_PCM_CH2_MSK)) {
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
                        // --- Tema del jefe ---------------------------------
                        // Arranca ACA, con la puerta de la capsula ya abierta
                        // (el stage 2 la abrio y espero a que sonara el wav de
                        // la puerta), y NO se vuelve a tocar en toda la pelea:
                        // el unico XGM2 posterior es music_ending, que lo pisa
                        // solo cuando el jefe ya murio y arranca la cutscene.
                        // La musica de fondo del nivel la habia cortado el
                        // stage 1 (XGM2_stop) al empezar a emerger el taladro.
                        //
                        // setLoopNumber(-1) SIEMPRE antes del play, nunca
                        // despues: el driver latchea el loop en el instante del
                        // play (misma trampa que documenta MUSIC_ENDING_LEN mas
                        // arriba, pero al reves -- aca queremos loop INFINITO,
                        // porque la pelea dura mas que los 32,5s del tema).
                        //
                        // (01/10) Ya NO arranca aca: primero se escucha el
                        // "SAY YOUR PRAYERS!" completo y recien despues entra
                        // el tema (boss_vo.c, mas abajo con bossVoStart). Antes
                        // entraba bajo (ducking) y subia con una rampa.
                        bossMusicVol = VOL_MUSIC_BOSS;

                        if (!bossSpawned) {
                            bossSpawned = TRUE;
                            // Paleta del jefe en PAL3. (03/10) Es la paleta
                            // compartida de los jefes (Bebop/Rocksteady/April)
                            // y usa el indice 1 (el negro de los contornos):
                            // ya no se fuerza a blanco, el HUD dibuja con PAL1.
                            PAL_setPalette(PAL3, rocksteady_boss.palette->data, DMA);
                            // Buffers del flash: normal + versión "quemada"
                            // (cada canal RGB duplicado, clampeado a 0xF). El
                            // índice 0 se mantiene transparente.
                            for (u16 ci = 0; ci < 16; ci++) {
                                u16 c = rocksteady_boss.palette->data[ci];
                                bossPal[ci] = c;
                                if (ci == 0) flashPal[ci] = bossPal[ci];
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
                        // Voz con prioridad (CH1, el tema parado) y el tema
                        // del jefe cuando termina (bossVoUpdate en el bucle).
                        bossVoStart(say_your_p_sfx, sizeof(say_your_p_sfx),
                                    music_boss, VOL_MUSIC_BOSS);

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
                    // (01/10) La rampa del ducking se fue: el tema ahora entra
                    // recien cuando termino la voz, ya a volumen normal.
                    // Espera a que termine el taunt (~2.8s) → empieza la batalla.
                    if (++bossTimer >= 170) { bossStage = 99; bossTimer = 0; }
                    break;
            }
        }

        // 4d. Cutscene de victoria: Shredder rapta a April (se dispara en 4b
        //     cuando el jefe llega a ROCKSTEADY_GONE).
        if (cutScene > 0) {
            switch (cutScene) {
                case 1: {   // Silencio, y recién ahí Shredder en la puerta (Idle)
                    // 1a. PRIMER FRAME: cortar el tema del jefe. La cutscene ya
                    //     no arranca encima de la música de la pelea (music_boss
                    //     venía sonando desde que se abrió la cápsula y el play
                    //     de music_ending la pisaba de golpe). Se corta, se deja
                    //     CUT_SILENCE_FRAMES de silencio con la escena quieta y
                    //     recién entonces entra Shredder con su tema.
                    if (cutTimer == 0) XGM2_stop();

                    // 1b. El segundo de aire. Nada más pasa en estos frames: el
                    //     jefe ya desapareció y Shredder todavía no existe.
                    //     (El corte de respaldo de MUSIC_ENDING_LEN que hay
                    //     después del switch no molesta acá: con el driver
                    //     parado, XGM2_isPlaying() da FALSE.)
                    if (++cutTimer < CUT_SILENCE_FRAMES) break;

                    // 1c. Entra Shredder + su tema.
                    {
                        // Paleta propia de Shredder en PAL3 (Rocksteady ya la
                        // liberó al morir); índice 1 blanco para el HUD.
                        PAL_setPalette(PAL3, shredder_lvl1.palette->data, DMA);
                        PAL_setColor(PAL3 * 16 + 1, 0x0EEE);
                        shredderX = SHREDDER_SPAWN_X;
                        shredderY = SHREDDER_FEET_Y - SHREDDER_FRAME_H;
                        shredderSpr = SPR_addSprite(&shredder_lvl1,
                                                    shredderX - cameraX, shredderY,
                                                    TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
                        // OJO DE ORDEN (bug encontrado 30/08, ver MUSIC_ENDING_LEN mas
                        // arriba): setLoopNumber SIEMPRE antes del play, nunca despues --
                        // estaba al reves y por eso el tema repetia en loop.
                        XGM2_setLoopNumber(0);
                        playMusicVol(music_ending, VOL_MUSIC_ENDING);
                        if (shredderSpr) {
                            // Detrás de April (su depth ahora es -APRIL_LANE_Y = -148,
                            // ver el SPR_addSprite de aprilSpr mas arriba): menor
                            // valor = delante, así que con -108 Shredder sigue
                            // quedando DETRÁS de ella (y de los jugadores, que
                            // usan -y con y >= 118) pero delante de la cápsula.
                            SPR_setDepth(shredderSpr, -APRIL_LANE_Y + 40);
                            SPR_setAutoAnimation(shredderSpr, FALSE);
                            SPR_setAnim(shredderSpr, 0);          // Idle [0] (sin flip: el arte ya mira a la izquierda)
                        }
                    }
                    // Ya se vio aparecer: pasa a caminar hacia April.
                    cutScene = 2; cutTimer = 0;
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
                    if (cutTimer == 0) {
                        if (shredderSpr)
                            SPR_setAnimAndFrame(shredderSpr, 2, 2);
                    }
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

            // Corte de respaldo del vgm de music_ending -- ver MUSIC_ENDING_LEN
            // mas arriba (mismo problema y misma tecnica que introTick en
            // intro_arcade.c). No depende de que XGM2_setLoopNumber(0) se
            // haya aplicado bien en el punto de loop del propio driver -- es
            // una garantia adicional, no un reemplazo. La musica sigue
            // sonando en showEnding() (clearSceneEx(TRUE) no corta audio al
            // salir de aca), asi que el mismo chequeo se repite alla.
            if (XGM2_isPlaying() && XGM2_getElapsed() >= MUSIC_ENDING_LEN)
                XGM2_stop();

            if (cutScene == 5)
                break;
        }

        // 5. Enemigos: separación + IA.
        separateEnemies(enemies, MAX_ENEMIES);
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].state == ENEMY_STATE_INACTIVE) continue;
            setEnemyCamera(&enemies[i], cameraX);
            updateEnemyN(&enemies[i], pls, nPl);
        }

        // 5b. Shurikens: actualizar posición, auto-destrucción off-screen.
        shurikenUpdate(cameraX);

        // 5c. Rocksteady (jefe de la fase 2).
        rocksteadyUpdateN(&boss, cameraX, pls, nPl);

        // DUCK del tema del jefe mientras suena su GRITO DE MUERTE (14/09). El
        // wav dura 101 frames y los primeros 42 (los de la anim de muerte)
        // competirian con music_boss a volumen pleno; despues de eso la
        // cutscene corta la musica sola (XGM2_stop en cutScene 1). Mismo
        // criterio que el taunt del principio: una voz no se gana subiendo el
        // PCM, se gana bajando los FM (mismo criterio de mezcla que en audio.res).
        if (boss.state == ROCKSTEADY_DEAD && bossMusicVol > VOL_MUSIC_BOSS_DUCK) {
            bossMusicVol = VOL_MUSIC_BOSS_DUCK;
            XGM2_setFMVolume(VOL_MUSIC_BOSS_DUCK);
            XGM2_setPSGVolume(VOL_MUSIC_BOSS_DUCK);
        }

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

            // Hurtbox del cuerpo (ver el mismo bloque del nivel 1).
            s16     dmg      = 0;
            Player* attacker = NULL;
            for (u8 k = 0; k < nPl; k++) {
                if (!playerAttackHitsEnemy(pls[k], &enemies[i])) continue;   // (02/10) hurtbox por tipo
                dmg = isPlayerSpecialAttack(pls[k]) ? ENEMY_HP : 1; attacker = pls[k];
                break;
            }

            if (dmg > 0) {
                damageEnemy(&enemies[i], dmg);
                if (attacker && isPlayerJumpKicking(attacker))
                    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                if (attacker && enemies[i].state == ENEMY_STATE_DEAD) {
                    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode), SOUND_PCM_CH3, 15, FALSE, FALSE);
                    addPlayerScore(attacker, 1);
                    // (27/09) El golpe que lo mata lo lanza (ver ENEMY_DEATH_PUSH).
                    enemyDeathPush(&enemies[i], (s16)(attacker->x + PLAYER_SPRITE_W / 2),
                                   getPlayerDir(attacker), isPlayerSpecialAttack(attacker));
                }
            }
        }

        // 6-boss. Colisiones: ataque del jugador → Rocksteady.
        // Normal −1 barra, patada en salto −ROCKSTEADY_JUMPKICK_DMG, especial
        // −ROCKSTEADY_SPECIAL_DMG. Golpear con la
        // patada voladora suena el "pum" (igual que contra los foot soldiers).
        // MISMA geometría box-vs-box que contra los soldiers: la caja del
        // ataque se SOLAPA con la hurtbox del cuerpo del jefe
        // (ROCKSTEADY_BODY_HALF_W). Sin casos por facing: antes un punto de
        // impacto "hundido" según hacia dónde miraba el jefe hacía que por
        // la espalda ningún golpe conectara y de frente se pegara desde más
        // lejos del contacto real (y asimétrico según el lado de ataque).
        if (rocksteadyCanBeHit(&boss)) {
            s16     bcx   = rocksteadyGetCenterX(&boss);
            s16     by    = rocksteadyGetCenterY(&boss);
            s16     bdmg = 0;
            Player* batt = NULL;
            if (playerAttackHitsBox(&p1, bcx, by, ROCKSTEADY_BODY_HALF_W,
                                    ROCKSTEADY_BODY_H)) {
                bdmg = isPlayerSpecialAttack(&p1) ? ROCKSTEADY_SPECIAL_DMG
                     : (isPlayerJumpKicking(&p1) ? ROCKSTEADY_JUMPKICK_DMG : 1);
                batt = &p1;
            } else {
                for (u8 k = 1; k < nPl; k++) {
                    if (!playerAttackHitsBox(pls[k], bcx, by, ROCKSTEADY_BODY_HALF_W,
                                             ROCKSTEADY_BODY_H)) continue;
                    bdmg = isPlayerSpecialAttack(pls[k]) ? ROCKSTEADY_SPECIAL_DMG
                         : (isPlayerJumpKicking(pls[k]) ? ROCKSTEADY_JUMPKICK_DMG : 1);
                    batt = pls[k];
                    break;
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

            // El swing es UNO: pega al primer jugador alcanzado y se consume.
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                if (!enemyTryHitPlayerBox(e, getPlayerHurtX(pls[k]), getPlayerY(pls[k]),
                                          PLAYER_BODY_HALF_W)) continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(pls[k], getEnemyCenterX(e));
                break;
            }
        }

        // 6c. Shurikens → ataque del jugador (rompe proyectiles) y → jugadores.
        {
            for (u8 k = 0; k < nPl; k++) {
                if (shurikenBreakByPlayerAttack(pls[k]))
                    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
            }
        }
        {
            s16 hitX = 0;
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                if (!shurikenCheckHitPlayer(getPlayerWorldX(pls[k]), getPlayerY(pls[k]), &hitX)) continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(pls[k], hitX);
            }
        }

        // 6c-bis. Balas del jefe → impacto contra los jugadores.
        {
            s16 hitX = 0;
            // playerCanBeHitAir y no playerCanBeHit (14/09): la bala YA filtra
            // por altura, y el tiro hacia arriba existe justamente para pegarle
            // al que salta. Con la regla general ("saltando no te pegan") el
            // antiaereo no podia conectar NUNCA.
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHitAir(pls[k])) continue;
                if (!rocksteadyBulletCheckHitPlayer(getPlayerWorldX(pls[k]), getPlayerY(pls[k]),
                                                   getPlayerJumpZ(pls[k]), &hitX)) continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                playerHitProjectile(pls[k], hitX, ROCKSTEADY_BULLET_DMG);
            }
        }

        // 6d. HUD: refrescar barra de vida, vidas y puntaje.
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
                hudPlayerInit(&huds[1], &p2, hudPlayerCol(1),
                              (u16)(hudVramFree + HUD_VRAM_PER_PLAYER));
                nPl          = 2;
                dosJugadores = TRUE;
                resetEnemyAI(2);
            }
        }

        for (u8 k = 0; k < nPl; k++) hudPlayerUpdate(&huds[k]);

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
            if (continueStepAll(conts, pls, huds, nPl, fps))
                break;
        }

        // 7. Scroll del fondo. Durante la emergencia de la cápsula (stage 1) se
        //    suma un temblor horizontal al scroll y a la X de la cápsula (es un
        //    sprite, no scrollea sola): el HUD y la cámara de juego NO tiemblan.
        s16 dispCam = cameraX;
        s16 capsX   = CAPSULA_SCREEN_X;
        if (phase == 3 && bossStage == 1) {
            s16 shake = capsuleShake(bossTimer);
            dispCam += shake;
            // OJO (29/08): iba "capsX += shake" -- signo al reves. El scroll
            // de fondo se escribe como -cameraX (bgUpdate2/fireUpdate/
            // smokeUpdate), asi que un punto fijo del MUNDO se mueve en
            // pantalla en la direccion CONTRARIA a "dispCam" (si dispCam
            // sube, el fondo se corre a la izquierda). La capsula es un
            // sprite posicionado en coordenadas de PANTALLA directas: para
            // que tiemble EN EL MISMO SENTIDO que el fondo/humo/fuego (y no
            // se desalinee de la mascara de prioridad del humo del techo,
            // que tapa su mitad superior -- ver smokeInit) hay que restar el
            // temblor, no sumarlo. Con el signo viejo, cápsula y fondo se
            // movían en sentidos OPUESTOS cada frame durante el temblor (hasta
            // 1.5x la amplitud de desfasaje relativo, por el parallax 1/2 del
            // humo/fuego), lo que hacía que la máscara de prioridad tapara una
            // porción variable e impredecible de la cápsula frame a frame --
            // el parpadeo/transparencia reportado. Bug real encontrado 29/08
            // tras descartar fragmentación de VRAM y el límite de 80 sprites
            // de hardware (el contador en pantalla confirmó solo 5-7 sprites
            // activos, muy lejos del límite).
            capsX -= shake;
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
        for (u8 k = 1; k < nPl; k++) playerPersistSave(pls[k]);

        // Si la cutscene ya fundió a negro (fade del final en el vuelo), la
        // pantalla ya está en negro: no volver a fadear.
        if (!winFade) {
            PAL_fadeOutAll(30, FALSE);
            while (PAL_isDoingFade()) SYS_doVBlankProcess();
        }

        clearSceneEx(TRUE);
        return SCENE_ENDING;
    }

    clearScene();
    return (jump != PAUSE_NO_JUMP) ? jump : SCENE_GAME_OVER;
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
    clearSceneEx(TRUE);               // limpiar sin cortar la música (viene de cutScene 4)
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
    while (PAL_isDoingFade()) {
        // Corte de respaldo de music_ending (ver MUSIC_ENDING_LEN) -- viene
        // sonando desde la cutscene de showScene12, clearSceneEx(TRUE) no la
        // cortó al entrar acá.
        if (XGM2_isPlaying() && XGM2_getElapsed() >= MUSIC_ENDING_LEN)
            XGM2_stop();
        SYS_doVBlankProcess();
    }

    // Mantener ~3 segundos (START adelanta). La risa de Shredder NO suena al
    // entrar a la escena: arranca ~1.5s después de verse la imagen.
    u16 timer      = (IS_PAL_SYSTEM ? 50 : 60) * 3;
    u16 laughDelay = 30;   // ~0.5s después de verse la imagen (reducido 1s)
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
        // Corte de respaldo de music_ending (ver MUSIC_ENDING_LEN): que
        // solo se escuche una vez, aunque este hold + la cutscene previa de
        // showScene12 sumen más que la duración del tema.
        if (XGM2_isPlaying() && XGM2_getElapsed() >= MUSIC_ENDING_LEN)
            XGM2_stop();
        if (JOY_readJoypad(JOY_1) & BUTTON_START) break;
        SYS_doVBlankProcess();
    }

    XGM2_setLoopNumber(-1);   // restaurar loop infinito para la siguiente escena
    SPR_initEx(600);                 // restaurar el motor de sprites (lo usan las escenas siguientes)
    clearScene();
    // La persecución sigue: de acá se encadena el título de la Scene 2 y
    // después el nivel de la calle (antes esto reiniciaba el juego).
    return SCENE_2_1_TITLE;
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
