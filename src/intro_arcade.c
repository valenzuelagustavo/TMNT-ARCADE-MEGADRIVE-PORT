// ===========================================================================
// intro_arcade.c - Intro arcade de TMNT (SCENE_INTRO_ARCADE)
// ===========================================================================
// Reconstruccion de la intro del arcade original a partir del analisis frame
// a frame (940 frames de video @ ~60.12 fps). Como esa cadencia esta a 0.3%
// del refresco NTSC de la MegaDrive, cada frame del analisis se toma como UN
// TICK de la consola: las constantes INTRO_T_* de abajo son, literalmente,
// los tiempos del arcade.
//
//   A  0..200    Skyline nocturno con la luna (tira quieta arriba de todo)
//   B  200..456  Dolly hacia el callejon (scroll vertical continuo)
//   -  456..461  Flash blanco: la tapa de la alcantarilla sale volando
//   C  461..548  Las 4 tortugas saltan afuera dentro del haz de luz
//   D  548..700  Los 4 retratos crecen desde su esquina en cuadrantes
//   E  700..822  Corte a celeste + banner "TEENAGE MUTANT NINJA"
//   F  822..940  Entra el logo "TURTLES" + copyright de Konami
//
// TECNICA CENTRAL (escenas A/B): la tira del dolly mide 256x1496 px = 32x187
// tiles y NO entra en ningun plano de la MegaDrive (el maximo es 64x64). Se
// resuelve igual que el fondo del nivel 1 pero girado 90 grados: los 1001
// tiles unicos se cargan UNA sola vez a VRAM y BG_B funciona como ventana
// circular de 32 filas (256 px); a medida que la camara baja se dibujan filas
// nuevas por el borde inferior pisando las que ya salieron por arriba. La
// camara nunca sube, asi que solo hay que revelar hacia abajo.
//
// PRESUPUESTO DE VRAM (plano 64x32 -> userTileMaxIndex ~1612):
//   dolly 1001 + haz 11 = 1012 tiles de usuario
//   sprites: 4 tortugas x 90 + tapa 32 = 392  ->  SPR_initEx(416)
//   total ~1444 de 1612. La escena D (378 tiles) y las E/F (293) recargan
//   VRAM desde cero en cada corte duro, tapadas por el fade.
//   Nubes de la escena A/B (44 + 44 = 88 tiles de sprite, dos instancias de
//   intro_nube_chica; ya no se usa intro_nube_grande) casi no suman a ese
//   pico: cada una se libera sola (SPR_releaseSprite) apenas sale de camara
//   (ver nubesUpdateDrift/nubesUpdateScroll), con una red de seguridad al
//   terminar el dolly por si alguna todavia no salio. Pueden convivir un
//   rato con la tapa (32 tiles) durante el dolly -- 88+32=120 tiles, lejos
//   del limite -- pero nunca con
//   las tortugas (90 c/u), que recien aparecen despues de esa red.
//
// Musica: "01 - Opening Demo" (music_intro_arcade en audio.res). Arranca
// INTRO_MUSIC_DELAY (1s) despues del setup de VRAM/paletas -- ese hueco
// cuenta como START-skippable, igual que el resto de la escena A -- y NO
// hace loop (XGM2_setLoopNumber(0) ANTES de playMusicVol -- el driver latchea
// el loop en el instante del play, ver comentario en showArcadeIntro(); mismo
// orden que music_credits en scenes.c): si el vgm termina antes de que el jugador corte, se queda en
// silencio en vez de repetirse. Se restaura el loop infinito por defecto
// (XGM2_setLoopNumber(-1)) antes de salir, para no afectar la musica de las
// escenas siguientes. clearScene() al salir (skip por START o fin normal)
// corta lo que quede sonando via XGM2_stop().
// START saltea la intro completa en cualquier momento.
// ===========================================================================

#include "scenes.h"
#include "intro_tmnt.h"   // intro_dolly, intro_luz, intro_tapa, intro_turtles,
                          // intro_quad, intro_banner, intro_logo, intro_konami,
                          // intro_nube_chica (dos instancias: izquierda y derecha)
#include "audio.h"        // music_intro_arcade ("01 - Opening Demo.vgm")

// ---------------------------------------------------------------------------
// Geometria de pantalla y planos
// ---------------------------------------------------------------------------
#define INTRO_SCREEN_W      256   // Modo H32: el arte fuente esta pensado a 256
#define INTRO_SCREEN_H      224
#define INTRO_SCREEN_ROWS    28   // 224 / 8
#define INTRO_PLANE_W        64   // Plano de 64 tiles de ancho (512 px)
#define INTRO_PLANE_H        32   // ...y 32 filas (256 px) = ventana circular
#define INTRO_SPR_TILES     416   // 4 tortugas (90 c/u) + tapa (32) + margen
#define NUBE_CHICA_W         88   // 11 tiles
#define NUBE_CHICA_H         32   // 4 tiles
// Posicion HORIZONTAL de arranque de cada nube (antes las dos arrancaban
// totalmente afuera de cuadro, -NUBE_CHICA_W / INTRO_SCREEN_W, y entraban
// desde el borde). Ahora arrancan ya bien visibles:
//   Izquierda: borde derecho de la nube tocando el borde izquierdo de la
//   luna (bbox de la luna en x, ver mas abajo: 70..89) -> 70 - NUBE_CHICA_W.
#define NUBE_START_X_IZQ     (70 - NUBE_CHICA_W)   // -18
//   Derecha: centrada en pantalla en vez de afuera del borde derecho.
#define NUBE_START_X_DER     ((INTRO_SCREEN_W - NUBE_CHICA_W) / 2)   // 84
// Dos instancias de la MISMA nube chica (ya no se usa intro_nube_grande):
// una entra por la izquierda y otra por la derecha, cada una a su propia
// altura respecto de la luna. bbox real de la luna en intro_dolly.png es
// x=70..89 y=19..43 (tools/gen_intro_assets.py la deja intacta, viene del
// arte fuente; 24px de alto).
// Izquierda: se mete unos px dentro de la mitad inferior de la luna (34..43,
// ~9px de solape de los 24px de alto) para taparla parcialmente cuando su
// deriva la cruza por delante.
#define NUBE_Y_IZQ            42   // subida 1 tile (8px) a pedido de Gustavo, 30/08 -- antes 50
// Derecha: casi pegada al borde inferior de la luna (43) pero sin solape,
// bastante mas cerca que el hueco original de "un tile por debajo" (48).
#define NUBE_Y_DER           30   // subida 1 tile (8px) a pedido de Gustavo, 30/08 -- antes 38
// Velocidad de la deriva horizontal, SOLO durante la escena A (cielo quieto):
// cada nube tiene su PROPIO acumulador fraccionario (estilo Bresenham, ver
// nubesUpdateDrift), asi pueden ir a velocidades distintas. Ya no hace falta
// que alcancen a cruzar toda la pantalla a tiempo (esa restriccion
// desaparecio cuando se cambio el mecanismo de salida, ver nubesUpdateScroll
// mas abajo), asi que se puede ir tan lento como se quiera.
// Izquierda: 1/3 de px/tick.
#define NUBE_SPEED_NUM_IZQ      1
#define NUBE_SPEED_DEN_IZQ      3
// Derecha: mas lenta todavia que la izquierda, 1/5 de px/tick.
#define NUBE_SPEED_NUM_DER      1
#define NUBE_SPEED_DEN_DER      5
#define INTRO_MUSIC_VOL      90   // Mismo nivel que el resto de la musica (VOL_MUSIC_* en scenes.c)
#define INTRO_MUSIC_DELAY    (IS_PAL_SYSTEM ? 50 : 60)   // 1s de espera antes de arrancar la musica
// Duracion real hasta que "01 - Opening Demo.vgm" se repite, CONFIRMADA por
// Gustavo en emulador (se escucha repetir justo cuando cae intro_banner.png,
// tick ~706 de escena - 60 de delay = ~646 frames de musica). Coincide con el
// campo "loop # samples" del header VGM (470400 @ 44100 Hz = 640 frames NTSC
// exactos, 533 PAL) -- el primer calculo (basado en "total # samples", 905)
// estaba mal: ese campo es la duracion de TODO el archivo doblado, no el
// punto de loop. Un par de frames de margen para cortar ANTES de que se
// note, no despues.
// Corte de respaldo manual: no depende de que XGM2_setLoopNumber(0) se haya
// aplicado bien en el punto de loop del propio driver (por ahora no se pudo
// confirmar que lo haga) -- este corte manda si o si.
#define INTRO_MUSIC_LEN      (IS_PAL_SYSTEM ? 528 : 633)

// ---------------------------------------------------------------------------
// Linea de tiempo (ticks NTSC = frames del analisis del arcade)
// ---------------------------------------------------------------------------
#define INTRO_T_FADE_IN      20   // Fundido de entrada (dentro de la escena A)
#define INTRO_T_SKYLINE      20   // A: skyline quieto (200->140->80->20, recortando de a 1s para comprimir la intro)
// B: el descenso dura lo que haga falta para mantener SIEMPRE la misma
// velocidad en px/tick que los 256 ticks originales sobre 1272 px, aunque se
// alargue la banda de lluvia (ver RAIN_* mas abajo).
#define INTRO_T_DOLLY       ((INTRO_DOLLY_END * 256) / 1272)
#define INTRO_T_COVER_HOLD    2   // Tapa quieta antes del flash
#define INTRO_T_FLASH         3   // Flash blanco
#define INTRO_T_BEAM         10   // El haz de luz crece
// Corte antes de mostrar la caida/aterrizaje: en t=32 las 4 tortugas estan
// entre 94% y 100% de su propia altura de pico (arrancan con delays
// distintos, no pegan el pico exacto todas juntas), la mejor ventana comun
// para cortar mientras siguen "arriba" y pasar derecho a la escena D.
#define INTRO_T_JUMP         33   // C: solo el salto hasta cerca del pico, sin caida ni pose
#define INTRO_T_QUAD_GROW    32   // D: crecen los 4 cuadrantes
#define INTRO_T_QUAD_HOLD   120   // D: cuadro fijo
#define INTRO_T_SKY_HOLD     15   // E: celeste liso antes del banner
#define INTRO_T_BANNER_FALL  17   // E: el banner cae desde fuera de cuadro (mas rapido que antes)
#define INTRO_T_BANNER       70   // E: banner fijo (recortado para que el logo aparezca antes, ver INTRO_MUSIC_LEN)
#define INTRO_T_LOGO_WIPE    13   // F: entra "TURTLES"
#define INTRO_T_LOGO_HOLD   105   // F: composicion final fija
// Total: 880 ticks (~14.7 s) con RAIN_EXTRA_LOOPS = 0 (200->140 en la escena A
// para el ajuste de sincro con la musica); cada vuelta extra de lluvia suma
// ~13 ticks al dolly (con 4 vueltas: ~931 ticks, ~15.5 s).

// Fundido final: mas largo y con un respiro en negro antes de la escena que
// sigue, para que el corte no se sienta seco.
#define INTRO_T_FADE_OUT     60   // ~1 s de fundido a negro
#define INTRO_T_BLACK_HOLD   24   // ...y ~0.4 s de negro pleno

// ---------------------------------------------------------------------------
// Escenas A/B - dolly
// ---------------------------------------------------------------------------
// ESTIRAR LA BANDA DE LLUVIA SIN PERDER VELOCIDAD
// La tira trae, entre las filas de tile 87 y 126, la banda de lluvia que en el
// arte suelto es `fondo_b` (304x64): un bloque de 8 filas (64 px) PERFECTAMENTE
// tileable, repetido 4 veces y media. Verificado tile a tile: las filas 95..102,
// 103..110 y 111..118 son identicas a las 87..94.
// Por eso el tramo se puede alargar repitiendo ese bloque N veces mas, en vez de
// bajar la velocidad de scroll: la camara sigue viajando a los mismos px/tick
// (la sensacion de velocidad no cambia) y simplemente pasa mas tiempo adentro de
// la lluvia. Como el bloque es tileable y el punto de insercion respeta su fase,
// no hay costura visible ni un solo tile extra en VRAM.
// Para regular el efecto se toca UNA constante: RAIN_EXTRA_LOOPS.
#define RAIN_ROW_START       87   // Primera fila del bloque que se repite
#define RAIN_PERIOD           8   // Alto del bloque en filas de tile (64 px)
#define RAIN_INSERT_ROW      95   // Donde se insertan las copias (en fase con 87)
#define RAIN_EXTRA_LOOPS      4   // <-- vueltas EXTRA de lluvia (0 = como el arcade)
#define RAIN_EXTRA_ROWS    (RAIN_EXTRA_LOOPS * RAIN_PERIOD)

// Recorrido total del dolly: la tira (1496 px) menos la pantalla, mas lo que
// agreguen las vueltas extra de lluvia.
#define INTRO_DOLLY_END    ((1496 - INTRO_SCREEN_H) + RAIN_EXTRA_ROWS * 8)

// ---------------------------------------------------------------------------
// Escena C - alcantarilla, haz y tortugas (coordenadas de PANTALLA, medidas
// sobre el ultimo cuadro de la tira: el pozo queda centrado abajo)
// ---------------------------------------------------------------------------
#define HOLE_CX             133   // Centro X del pozo
#define HOLE_CY             198   // Centro Y del pozo
#define TAPA_W               64   // El sprite de la tapa es de 64x32
#define TAPA_H               32
#define TAPA_REST_X         (HOLE_CX - TAPA_W / 2)
#define TAPA_REST_Y         (HOLE_CY - TAPA_H / 2)
#define TAPA_FLY_VX           2   // Deriva a la derecha mientras vuela
#define TAPA_FLY_VY          -7   // Velocidad de subida

#define BEAM_COL             13   // Columna del plano donde arranca el haz (x=104)
#define BEAM_ROW_SHIFT        2   // El haz se sube 2 filas para calzar en el pozo
#define BEAM_ROWS_TOTAL      26   // Filas visibles con el haz completo
#define BEAM_ROWS_MIN         3   // Solo la base redondeada
#define LUZ_BODY_ROW          0   // Fila del mapa de intro_luz que se repite
#define LUZ_BASE_ROW         25   // Primera de las 3 filas de la base

#define TURTLE_W             72   // Celda de la hoja reducida (9x10 tiles)
#define TURTLE_H             80
#define TURTLE_FOOT          73   // Linea de piso DENTRO de la celda (y=95 del arte)
#define TURTLE_FRAMES         8   // Frames aereos (los 1..8 de ANIM_JUMP)
#define TURTLE_LAND_FRAME     8   // Frame de aterrizaje (el 9 de ANIM_JUMP)
#define TURTLE_APEX          72   // Altura del arco, en px

// Destino de cada tortuga: X del CENTRO y Y de los PIES al aterrizar.
// Indices 0=Leo 1=Mike 2=Don 3=Raph (mismo orden que la hoja y que
// personajeSeleccionado). Leo queda al frente y aterriza ultimo.
static const s16 turtleDstX[4]  = { 156,  44, 100, 212 };
static const s16 turtleDstY[4]  = { 202, 194, 206, 198 };
static const s16 turtleDelay[4] = {  14,   0,   6,  10 };  // ticks de arranque
static const s16 turtleFlip[4]  = {   0,   1,   1,   0 };  // 1 = mira a la izquierda
static const s16 turtleTime[4]  = {  46,  52,  50,  48 };  // duracion del vuelo

// ---------------------------------------------------------------------------
// Escena D - cuadrantes
// ---------------------------------------------------------------------------
#define QUAD_COLS            16   // 128 px
#define QUAD_ROWS            14   // 112 px

// ---------------------------------------------------------------------------
// Escenas E/F - banner, logo y copyright
// ---------------------------------------------------------------------------
#define INTRO_SKY_PAL_INDEX  13   // Indice del celeste dentro de la PAL0 de E/F
                                  // (lo reporta tools/gen_intro_assets.py)
#define BANNER_COL            3    // (32 - 26) / 2 -> centrado
#define BANNER_ROW            5    // y = 40 (posicion final)
// La caida se hace con el scroll vertical de BG_A: con scrollA = S el banner se
// dibuja en la pantalla a y = (BANNER_ROW * 8) - S. Arranca en 64 (justo por
// encima del borde superior) y termina en 0.
#define BANNER_FALL_FROM     64
#define BANNER_BOUNCE         5    // Rebote en px al tocar su lugar
#define BANNER_BOUNCE_TICKS   8    // Duracion del rebote (parte de la caida)
#define LOGO_COL              0    // El logo mide 32 tiles: pantalla completa
#define LOGO_ROW              8    // y = 64
#define LOGO_TILE_ROWS       10
#define KONAMI_COL            5    // (32 - 21) / 2 -> centrado (x = 40)
#define KONAMI_ROW           18    // y = 144

// ===========================================================================
// Estado interno
// ===========================================================================
static const u16 *dollyMap;      // Tilemap completo en ROM (sin comprimir)
static u16        dollyMapW;     // 32 tiles
static u16        dollyMapH;     // 187 tiles
static u16        dollyAttr;     // Paleta + indice base en VRAM
static s16        dollyLastRow;  // Ultima fila FUENTE ya volcada al plano

static const u16 *luzMap;
static u16        luzAttr;

static Sprite    *tapaSpr;
static Sprite    *turtleSpr[4];
static Sprite    *nubeChicaSpr;
static Sprite    *nubeChica2Spr;
static s16        nubeChicaX;    // arranca en NUBE_START_X_IZQ, viaja a la derecha
static s16        nubeChica2X;   // arranca en NUBE_START_X_DER, viaja a la izquierda
static u8         nubeAccumIzq;   // acumulador fraccionario de NUBE_SPEED_NUM_IZQ/DEN_IZQ
static u8         nubeAccumDer;   // acumulador fraccionario de NUBE_SPEED_NUM_DER/DEN_DER
static bool       introMusicStopped;   // corte manual de respaldo, ver INTRO_MUSIC_LEN

static const u16 introWhite[64] = {
    0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE,
    0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE,
    0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE,
    0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE,
    0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE,
    0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE,
    0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE,
    0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE,
};
static const u16 introBlack[64] = { 0 };

// Un tick de intro. Devuelve TRUE si el jugador apreto START.
static bool introTick(void) {
    // Corte de respaldo: si la musica todavia estuviera sonando al llegar a
    // la duracion real de una pasada (INTRO_MUSIC_LEN, calculada del propio
    // header del vgm), se para a mano. No depende de que
    // XGM2_setLoopNumber(0) se haya aplicado correctamente en el punto de
    // loop del driver -- es una garantia adicional, no un reemplazo.
    if (!introMusicStopped && XGM2_isPlaying() &&
        (XGM2_getElapsed() >= INTRO_MUSIC_LEN)) {
        XGM2_stop();
        introMusicStopped = TRUE;
    }

    SPR_update();
    SYS_doVBlankProcess();
    return (JOY_readJoypad(JOY_1) & BUTTON_START) ? TRUE : FALSE;
}

// Espera n ticks sin hacer nada mas. TRUE = salteado.
static bool introWait(u16 ticks) {
    while (ticks--) if (introTick()) return TRUE;
    return FALSE;
}

// Escena A (cielo quieto, scrollY siempre 0): deriva horizontal lenta con
// acumulador fraccionario. Ademas de mover, chequea si alguna se fue por el
// COSTADO de la pantalla -- en la practica no debería pasar nunca en lo que
// dura la escena A (van demasiado lento y la escena es corta), es solo una
// red de seguridad por si el dia de mañana se alarga la escena o se sube la
// velocidad.
static void nubesUpdateDrift(void) {
    if (nubeChicaSpr) {
        nubeAccumIzq += NUBE_SPEED_NUM_IZQ;
        if (nubeAccumIzq >= NUBE_SPEED_DEN_IZQ) {
            nubeAccumIzq -= NUBE_SPEED_DEN_IZQ;
            nubeChicaX += 1;
        }
        if (nubeChicaX >= INTRO_SCREEN_W) {
            SPR_releaseSprite(nubeChicaSpr);
            nubeChicaSpr = NULL;
        } else {
            SPR_setPosition(nubeChicaSpr, nubeChicaX, NUBE_Y_IZQ);
        }
    }
    if (nubeChica2Spr) {
        nubeAccumDer += NUBE_SPEED_NUM_DER;
        if (nubeAccumDer >= NUBE_SPEED_DEN_DER) {
            nubeAccumDer -= NUBE_SPEED_DEN_DER;
            nubeChica2X -= 1;
        }
        if (nubeChica2X <= -NUBE_CHICA_W) {
            SPR_releaseSprite(nubeChica2Spr);
            nubeChica2Spr = NULL;
        } else {
            SPR_setPosition(nubeChica2Spr, nubeChica2X, NUBE_Y_DER);
        }
    }
}

// Escena B (dolly): las nubes son parte del cielo, igual que la luna -- y la
// luna, al ser parte de la misma tira que scrollea, se va de cuadro por
// ARRIBA a medida que "la camara" baja por el callejon (ver dollyUpdate). En
// vez de seguir moviendolas de costado (que las hacia ver como si "bajaran
// con la camara" en vez de irse con el resto del cielo), ahora se les
// congela la X que trajeron de la escena A y se les hace seguir el scroll
// vertical EXACTAMENTE como a la tapa de la alcantarilla mas abajo
// (tapaScreenY = mundo - scrollY): la Y en pantalla de cada nube es su altura
// original (NUBE_Y_IZQ/DER, que es literalmente su Y de "mundo" porque a
// scrollY=0 mundo y pantalla coinciden) menos scrollY. Se liberan solas
// cuando ya salieron entera por arriba (y + alto <= 0), con los mismos ~40
// ticks de margen que tarda el scroll en empezar a moverse de verdad -- muy
// rapido, no llegan a "acompañar" el resto del dolly.
static void nubesUpdateScroll(s16 scrollY) {
    if (nubeChicaSpr) {
        s16 y = NUBE_Y_IZQ - scrollY;
        if (y + NUBE_CHICA_H <= 0) {
            SPR_releaseSprite(nubeChicaSpr);
            nubeChicaSpr = NULL;
        } else {
            SPR_setPosition(nubeChicaSpr, nubeChicaX, y);
        }
    }
    if (nubeChica2Spr) {
        s16 y = NUBE_Y_DER - scrollY;
        if (y + NUBE_CHICA_H <= 0) {
            SPR_releaseSprite(nubeChica2Spr);
            nubeChica2Spr = NULL;
        } else {
            SPR_setPosition(nubeChica2Spr, nubeChica2X, y);
        }
    }
}

// Igual que introWait(), pero ademas hace avanzar (y eventualmente liberar)
// las nubes con nubesUpdateDrift(). Se usa durante el delay de la musica y
// el skyline quieto de la escena A.
static bool introWaitClouds(u16 ticks) {
    while (ticks--) {
        nubesUpdateDrift();
        if (introTick()) return TRUE;
    }
    return FALSE;
}

// ===========================================================================
// STREAMING VERTICAL DE LA TIRA DEL DOLLY
// ===========================================================================
// Mismo principio que el fondo del nivel 1, pero por FILAS en vez de columnas:
// el plano es circular de 32 filas, la fila fuente R vive siempre en la fila
// de plano (R & 31), y el scroll vertical (= scrollY) muestra la ventana
// correcta. Como el plano cubre 256 px y la pantalla 224, quedan 4 filas de
// colchon: se escriben filas hasta 2 por debajo del borde inferior visible,
// que nunca son las que se estan dibujando en pantalla.
// ---------------------------------------------------------------------------
// Fila VIRTUAL -> fila REAL de la tira. Antes del punto de insercion son la
// misma; adentro del tramo insertado se cicla el bloque de lluvia; despues se
// descuenta lo insertado y se sigue con la tira normal.
static u16 dollyMapRow(u16 virtualRow) {
    if (virtualRow < RAIN_INSERT_ROW) return virtualRow;
    if (virtualRow < RAIN_INSERT_ROW + RAIN_EXTRA_ROWS)
        return RAIN_ROW_START + ((virtualRow - RAIN_INSERT_ROW) & (RAIN_PERIOD - 1));
    return virtualRow - RAIN_EXTRA_ROWS;
}

// La fila virtual manda en la POSICION dentro del plano circular (asi el scroll
// vertical sigue siendo simplemente scrollY); el CONTENIDO sale de la fila real.
static void dollyDrawRow(u16 virtualRow) {
    const u16 *p = dollyMap + (u32)dollyMapRow(virtualRow) * dollyMapW;
    u16 destRow = virtualRow & (INTRO_PLANE_H - 1);
    for (u16 tx = 0; tx < dollyMapW; tx++)
        VDP_setTileMapXY(BG_B, dollyAttr + p[tx], tx, destRow);
}

static void dollyInit(void) {
    VDP_loadTileSet(intro_dolly.tileset, TILE_USER_INDEX, DMA);

    dollyMap  = intro_dolly.tilemap->tilemap;
    dollyMapW = intro_dolly.tilemap->w;
    dollyMapH = intro_dolly.tilemap->h;
    dollyAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX);

    for (u16 r = 0; r < INTRO_PLANE_H; r++) dollyDrawRow(r);
    dollyLastRow = INTRO_PLANE_H - 1;
    VDP_setVerticalScroll(BG_B, 0);
}

static void dollyUpdate(s16 scrollY) {
    s16 need = (scrollY >> 3) + INTRO_SCREEN_ROWS + 2;
    s16 lastVirtual = (s16)dollyMapH + RAIN_EXTRA_ROWS - 1;
    if (need > lastVirtual) need = lastVirtual;
    while (dollyLastRow < need) {
        dollyLastRow++;
        dollyDrawRow((u16)dollyLastRow);
    }
    // El registro de scroll vertical envuelve con el alto del plano (256 px)
    VDP_setVerticalScroll(BG_B, scrollY & (INTRO_PLANE_H * 8 - 1));
}

// ===========================================================================
// HAZ DE LUZ (BG_A, por encima del fondo y por DEBAJO de los sprites)
// ===========================================================================
// intro_luz es una columna de 8x28 tiles cuyas filas 0..24 son IDENTICAS
// (el cuerpo del haz) y cuyas filas 25..27 son la base redondeada. Se dibuja
// la base fija sobre el pozo y el cuerpo se repite hacia arriba: crecer el haz
// es solo dibujar mas filas de cuerpo, sin gastar un tile extra de VRAM.
// ---------------------------------------------------------------------------
static void beamDrawSrcRow(u16 srcRow, s16 destRow) {
    if (destRow < 0 || destRow >= INTRO_SCREEN_ROWS) return;
    const u16 *p = luzMap + (u32)srcRow * intro_luz.tilemap->w;
    for (u16 tx = 0; tx < intro_luz.tilemap->w; tx++)
        VDP_setTileMapXY(BG_A, luzAttr + p[tx], BEAM_COL + tx, (u16)destRow);
}

// rows = alto total del haz en tiles (BEAM_ROWS_MIN..BEAM_ROWS_TOTAL)
static void beamDraw(u16 rows) {
    s16 baseTop = INTRO_SCREEN_ROWS - 1 - BEAM_ROW_SHIFT - 2;   // fila 25 de la tira
    // Base redondeada (siempre en el mismo lugar, sobre el pozo)
    for (u16 i = 0; i < 3; i++)
        beamDrawSrcRow(LUZ_BASE_ROW + i, baseTop + (s16)i);
    // Cuerpo hacia arriba
    for (u16 i = 3; i < rows; i++)
        beamDrawSrcRow(LUZ_BODY_ROW, baseTop - (s16)(i - 2));
}

// ===========================================================================
// BLOQUE 1 - Escenas A, B y C (skyline -> dolly -> flash -> salto)
// ===========================================================================
static bool introBlockDolly(void) {
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_A, 0);

    dollyInit();

    // El haz vive en BG_A, justo despues de los tiles de la tira
    u16 luzVram = TILE_USER_INDEX + intro_dolly.tileset->numTile;
    VDP_loadTileSet(intro_luz.tileset, luzVram, DMA);
    luzMap  = intro_luz.tilemap->tilemap;
    luzAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, luzVram);

    // ---- Escena A: skyline (fundido de entrada + espera) ----
    // CRAM entera a negro y solo se funde PAL0 (el fondo). PAL1 (las tortugas)
    // se carga despues del flash, que es cuando entran en cuadro.
    PAL_setColors(0, introBlack, 64, DMA);
    PAL_fadeIn(0, 15, intro_dolly.palette->data, INTRO_T_FADE_IN, TRUE);

    // ---- Nubes: cruzan el cielo por debajo de la luna durante toda la
    // escena A (delay de musica + skyline quieto). Paleta PROPIA (PAL2): la
    // PAL0 del dolly/haz/tapa ya esta al limite de 16 colores (ver
    // tools/gen_intro_assets.py), no hay lugar para sumar las nubes ahi. Se
    // liberan antes de arrancar la escena B (ver mas abajo), asi que no
    // suman al pico de VRAM de sprites de tortugas+tapa.
    PAL_setPalette(PAL2, intro_nube_chica.palette->data, DMA);
    nubeChicaX  = NUBE_START_X_IZQ;
    nubeChica2X = NUBE_START_X_DER;
    nubeAccumIzq = 0;
    nubeAccumDer = 0;
    nubeChicaSpr  = SPR_addSprite(&intro_nube_chica, nubeChicaX, NUBE_Y_IZQ,
                                  TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    nubeChica2Spr = SPR_addSprite(&intro_nube_chica, nubeChica2X, NUBE_Y_DER,
                                  TILE_ATTR(PAL2, FALSE, FALSE, FALSE));

    // La musica arranca INTRO_MUSIC_DELAY (1s) despues de que el cielo ya es
    // visible (no antes): puesta antes del fade-in, la espera pasaba a
    // oscuras y en silencio, y quedaba pegada al instante en que aparece el
    // cielo -- indistinguible de "sin delay" para cualquiera mirando la
    // pantalla. START durante esta espera saltea la intro, igual que el
    // resto de la escena.
    if (introWaitClouds(INTRO_MUSIC_DELAY)) return FALSE;

    // OJO DE ORDEN: el comando "play" del driver XGM2 LATCHEA el numero de
    // loop en el instante en que se dispara (lee XGM2_PLAY_ARG_LOOP y lo copia
    // a su contador interno ahi mismo) -- si se llama DESPUES de
    // playMusicVol/XGM2_play no tiene ningun efecto sobre esta reproduccion.
    // setLoopNumber va SIEMPRE antes del play (igual que music_credits en
    // scenes.c; music_ending lo tiene al reves y por eso probablemente
    // tambien hace loop, aunque eso no se toco aca). Ademas es un no-op
    // silencioso si el driver Z80 cargado no es XGM2 en ese momento -- se
    // fuerza la carga a mano para no depender de que una escena previa ya
    // lo haya dejado puesto.
    Z80_loadDriver(Z80_DRIVER_XGM2, TRUE);
    XGM2_setLoopNumber(0);   // no repetir en loop si el tema termina antes que la escena
    introMusicStopped = FALSE;   // reset del corte manual de respaldo (ver introTick)
    playMusicVol(music_intro_arcade, INTRO_MUSIC_VOL);

    if (introWaitClouds(INTRO_T_SKYLINE)) return FALSE;

    // ---- Escena B: dolly hasta el callejon ----
    // Curva suave (smoothstep) sobre los 1272 px: la camara arranca despacio,
    // crucea a ~7 px/tick y frena al llegar al pozo.
    tapaSpr = SPR_addSprite(&intro_tapa, TAPA_REST_X, TAPA_REST_Y,
                            TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
    SPR_setVisibility(tapaSpr, HIDDEN);

    for (u16 t = 1; t <= INTRO_T_DOLLY; t++) {
        s32 u = ((s32)t * 256) / INTRO_T_DOLLY;                 // 0..256
        s32 s = (3 * u * u) / 256 - (u * u * u) / 32768;        // smoothstep 0..256
        s16 scrollY = (s16)(((s32)INTRO_DOLLY_END * s) / 256);
        dollyUpdate(scrollY);

        // La tapa aparece cuando el pozo entra en cuadro y se pega a el
        // (su Y de MUNDO es INTRO_DOLLY_END + TAPA_REST_Y)
        s16 tapaScreenY = (INTRO_DOLLY_END + TAPA_REST_Y) - scrollY;
        if (tapaScreenY < INTRO_SCREEN_H) {
            SPR_setVisibility(tapaSpr, VISIBLE);
            SPR_setPosition(tapaSpr, TAPA_REST_X, tapaScreenY);
        }
        nubesUpdateScroll(scrollY);
        if (introTick()) return FALSE;
    }
    // Red de seguridad: si alguna nube todavia no salio de cuadro sola para
    // cuando termina el dolly, se la fuerza aca (no debe convivir con la
    // tapa/tortugas que arrancan enseguida, por el presupuesto de VRAM). Con
    // nubesUpdateScroll ya salen del todo a los ~40 ticks de arrancar el
    // dolly (de 307 en total), asi que esto deberia ser un caso rarisimo.
    if (nubeChicaSpr)  { SPR_releaseSprite(nubeChicaSpr);  nubeChicaSpr  = NULL; }
    if (nubeChica2Spr) { SPR_releaseSprite(nubeChica2Spr); nubeChica2Spr = NULL; }
    dollyUpdate(INTRO_DOLLY_END);
    SPR_setPosition(tapaSpr, TAPA_REST_X, TAPA_REST_Y);
    SPR_setVisibility(tapaSpr, VISIBLE);
    if (introWait(INTRO_T_COVER_HOLD)) return FALSE;

    // ---- Transicion: flash blanco y la tapa sale volando ----
    PAL_setColors(0, introWhite, 64, DMA);
    if (introWait(INTRO_T_FLASH)) return FALSE;
    PAL_setPalette(PAL0, intro_dolly.palette->data, DMA);
    PAL_setPalette(PAL1, intro_turtles.palette->data, DMA);

    // ---- El haz crece mientras la tapa se va de cuadro ----
    s16 tapaX = TAPA_REST_X, tapaY = TAPA_REST_Y;
    for (u16 t = 0; t < INTRO_T_BEAM; t++) {
        u16 rows = BEAM_ROWS_MIN +
                   ((BEAM_ROWS_TOTAL - BEAM_ROWS_MIN) * (t + 1)) / INTRO_T_BEAM;
        beamDraw(rows);
        tapaX += TAPA_FLY_VX;
        tapaY += TAPA_FLY_VY;
        SPR_setPosition(tapaSpr, tapaX, tapaY);
        if (introTick()) return FALSE;
    }
    beamDraw(BEAM_ROWS_TOTAL);

    // ---- Escena C: salto simultaneo de las 4 tortugas ----
    for (u16 i = 0; i < 4; i++) {
        turtleSpr[i] = SPR_addSprite(&intro_turtles,
                                     HOLE_CX - TURTLE_W / 2, HOLE_CY - TURTLE_FOOT,
                                     TILE_ATTR(PAL1, FALSE, FALSE, turtleFlip[i]));
        SPR_setAnimAndFrame(turtleSpr[i], i, 0);
        SPR_setDepth(turtleSpr[i], (s16)i);   // Leo (0) al frente
        SPR_setVisibility(turtleSpr[i], HIDDEN);
    }

    for (u16 t = 0; t < INTRO_T_JUMP; t++) {
        // La tapa termina su vuelo y se suelta al salir de cuadro (si no,
        // quedaria congelada en el aire durante toda la escena C).
        if (tapaSpr) {
            tapaX += TAPA_FLY_VX;
            tapaY += TAPA_FLY_VY;
            if (tapaY + TAPA_H <= 0) {
                SPR_releaseSprite(tapaSpr);
                tapaSpr = NULL;
            } else {
                SPR_setPosition(tapaSpr, tapaX, tapaY);
            }
        }
        for (u16 i = 0; i < 4; i++) {
            s16 lt = (s16)t - turtleDelay[i];
            if (lt < 0) continue;
            SPR_setVisibility(turtleSpr[i], VISIBLE);

            s16 T = turtleTime[i];
            if (lt < T) {
                // Arco balistico: interpolacion lineal + parabola de altura
                s16 x  = HOLE_CX + (s16)(((s32)(turtleDstX[i] - HOLE_CX) * lt) / T);
                s16 fy = HOLE_CY + (s16)(((s32)(turtleDstY[i] - HOLE_CY) * lt) / T)
                       - (s16)(((s32)TURTLE_APEX * 4 * lt * (T - lt)) / ((s32)T * T));
                SPR_setPosition(turtleSpr[i], x - TURTLE_W / 2, fy - TURTLE_FOOT);
                u16 fr = (u16)(((s32)lt * TURTLE_FRAMES) / T);
                if (fr >= TURTLE_FRAMES) fr = TURTLE_FRAMES - 1;
                SPR_setAnimAndFrame(turtleSpr[i], i, fr);
            } else {
                // Ya aterrizo: pose de aterrizaje, quieta, con los pies en su lane
                SPR_setPosition(turtleSpr[i], turtleDstX[i] - TURTLE_W / 2,
                                              turtleDstY[i] - TURTLE_FOOT);
                SPR_setAnimAndFrame(turtleSpr[i], i, TURTLE_LAND_FRAME);
            }
        }
        if (introTick()) return FALSE;
    }
    return TRUE;
}

// ===========================================================================
// BLOQUE 2 - Escena D: los 4 retratos crecen desde su esquina
// ===========================================================================
// Corte duro a negro y recarga completa de VRAM (la tira del dolly ya no hace
// falta). Los rectangulos de color que crecen no son sprites escalados (el
// hardware no escala): se rellena el tilemap con el tile SOLIDO que la propia
// imagen ya tiene en la esquina de cada cuadrante, y al completarse se vuelca
// el cuadro real encima.
// ---------------------------------------------------------------------------
static bool introBlockQuad(void) {
    PAL_setColors(0, introBlack, 64, DMA);
    SYS_doVBlankProcess();

    for (u16 i = 0; i < 4; i++)
        if (turtleSpr[i]) { SPR_releaseSprite(turtleSpr[i]); turtleSpr[i] = NULL; }
    if (tapaSpr) { SPR_releaseSprite(tapaSpr); tapaSpr = NULL; }
    SPR_update();

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setVerticalScroll(BG_B, 0);

    VDP_loadTileSet(intro_quad.tileset, TILE_USER_INDEX, DMA);
    u16 attr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX);
    const u16 *map = intro_quad.tilemap->tilemap;
    u16 mw = intro_quad.tilemap->w;

    // Tile solido de cada cuadrante = el que la imagen tiene en su esquina
    // EXTERNA (arriba-izq, arriba-der, abajo-izq, abajo-der).
    u16 solid[4];
    solid[0] = attr + map[0];
    solid[1] = attr + map[mw - 1];
    solid[2] = attr + map[(u32)(INTRO_SCREEN_ROWS - 1) * mw];
    solid[3] = attr + map[(u32)(INTRO_SCREEN_ROWS - 1) * mw + (mw - 1)];

    PAL_setPalette(PAL0, intro_quad.palette->data, DMA);

    for (u16 t = 1; t <= INTRO_T_QUAD_GROW; t++) {
        u16 w = (QUAD_COLS * t) / INTRO_T_QUAD_GROW;
        u16 h = (QUAD_ROWS * t) / INTRO_T_QUAD_GROW;
        if (w && h) {
            VDP_fillTileMapRect(BG_B, solid[0], 0,             0,             w, h);
            VDP_fillTileMapRect(BG_B, solid[1], 2 * QUAD_COLS - w, 0,         w, h);
            VDP_fillTileMapRect(BG_B, solid[2], 0,             2 * QUAD_ROWS - h, w, h);
            VDP_fillTileMapRect(BG_B, solid[3], 2 * QUAD_COLS - w, 2 * QUAD_ROWS - h, w, h);
        }
        if (introTick()) return FALSE;
    }

    // Cuadro completo (los 4 retratos ya dentro de sus cuadrantes)
    VDP_setTileMapEx(BG_B, intro_quad.tilemap, attr,
                     0, 0, 0, 0, intro_quad.tilemap->w, intro_quad.tilemap->h, DMA);
    return !introWait(INTRO_T_QUAD_HOLD);
}

// ===========================================================================
// BLOQUE 3 - Escenas E y F: banner, logo "TURTLES" y copyright
// ===========================================================================
// El celeste de fondo NO gasta tiles: es el color de backdrop del VDP. Los
// tres recortes comparten la misma PAL0 (los genera el script con una paleta
// unica), asi que alcanza con cargar una sola linea.
// ---------------------------------------------------------------------------
static bool introBlockLogo(void) {
    PAL_setColors(0, introBlack, 64, DMA);
    SYS_doVBlankProcess();

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    u16 vram = TILE_USER_INDEX;
    VDP_loadTileSet(intro_banner.tileset, vram, DMA);
    u16 bannerAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, vram);
    vram += intro_banner.tileset->numTile;

    VDP_loadTileSet(intro_logo.tileset, vram, DMA);
    u16 logoAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, vram);
    vram += intro_logo.tileset->numTile;

    VDP_loadTileSet(intro_konami.tileset, vram, DMA);
    u16 konamiAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, vram);

    // Corte duro al celeste: paleta compartida + backdrop
    PAL_setPalette(PAL0, intro_banner.palette->data, DMA);
    VDP_setBackgroundColor(INTRO_SKY_PAL_INDEX);
    if (introWait(INTRO_T_SKY_HOLD)) return FALSE;

    // ---- Escena E: el banner "TEENAGE MUTANT NINJA" CAE desde fuera de cuadro ----
    // Se dibuja en BG_A y se lo hace bajar con el scroll vertical del plano, que
    // es PIXEL a pixel (redibujar el tilemap solo permitiria saltos de 8 px).
    // Como en BG_A no hay nada mas todavia, mover el plano entero es gratis.
    // Al aterrizar se vuelca a BG_B en su fila definitiva y BG_A queda limpio
    // para el wipe del logo.
    VDP_setTileMapEx(BG_A, intro_banner.tilemap, bannerAttr,
                     BANNER_COL, BANNER_ROW, 0, 0,
                     intro_banner.tilemap->w, intro_banner.tilemap->h, DMA);
    VDP_setVerticalScroll(BG_A, BANNER_FALL_FROM);

    {
        const u16 fall = INTRO_T_BANNER_FALL - BANNER_BOUNCE_TICKS;
        // Caida acelerada (gravedad): el desplazamiento va con el cuadrado del
        // tiempo, asi entra despacio y llega golpeando.
        for (u16 t = 1; t <= fall; t++) {
            s32 p = ((s32)t * 256) / fall;                       // 0..256
            s16 sy = (s16)(((s32)BANNER_FALL_FROM * (65536 - p * p)) / 65536);
            VDP_setVerticalScroll(BG_A, sy);
            if (introTick()) return FALSE;
        }
        // Rebote corto al tocar: sube BANNER_BOUNCE px y vuelve a su lugar.
        for (u16 t = 0; t < BANNER_BOUNCE_TICKS; t++) {
            u16 half = BANNER_BOUNCE_TICKS / 2;
            s16 sy = (t < half)
                   ? (s16)((BANNER_BOUNCE * (t + 1)) / half)
                   : (s16)((BANNER_BOUNCE * (BANNER_BOUNCE_TICKS - t - 1)) / half);
            VDP_setVerticalScroll(BG_A, sy);
            if (introTick()) return FALSE;
        }
    }

    // Aterrizo: pasa a BG_B (fijo) y BG_A vuelve a cero para el logo
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setTileMapEx(BG_B, intro_banner.tilemap, bannerAttr,
                     BANNER_COL, BANNER_ROW, 0, 0,
                     intro_banner.tilemap->w, intro_banner.tilemap->h, DMA);
    VDP_clearPlane(BG_A, TRUE);
    if (introWait(INTRO_T_BANNER)) return FALSE;

    // ---- Escena F: "TURTLES" entra revelandose de derecha a izquierda ----
    s16 cols = (s16)intro_logo.tilemap->w;
    s16 done = cols;
    for (u16 t = 1; t <= INTRO_T_LOGO_WIPE; t++) {
        s16 target = cols - (s16)(((s32)cols * t) / INTRO_T_LOGO_WIPE);
        while (done > target) {
            done--;
            VDP_setTileMapEx(BG_A, intro_logo.tilemap, logoAttr,
                             LOGO_COL + done, LOGO_ROW, done, 0,
                             1, intro_logo.tilemap->h, CPU);
        }
        if (introTick()) return FALSE;
    }

    // ---- Composicion final: se suma el copyright de Konami ----
    VDP_setTileMapEx(BG_B, intro_konami.tilemap, konamiAttr,
                     KONAMI_COL, KONAMI_ROW, 0, 0,
                     intro_konami.tilemap->w, intro_konami.tilemap->h, DMA);
    return !introWait(INTRO_T_LOGO_HOLD);
}

// ===========================================================================
// PUNTO DE ENTRADA DE LA ESCENA
// ===========================================================================
SceneId showArcadeIntro(void) {
    clearScene();

    VDP_setScreenWidth256();
    VDP_setPlaneSize(INTRO_PLANE_W, INTRO_PLANE_H, TRUE);
    SPR_initEx(INTRO_SPR_TILES);
    VDP_setBackgroundColor(0);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);

    tapaSpr = NULL;
    for (u16 i = 0; i < 4; i++) turtleSpr[i] = NULL;

    if (introBlockDolly())
        if (introBlockQuad())
            introBlockLogo();

    // ---- Salida: fundido a negro y vuelta al estado que esperan los menus ----
    XGM2_setLoopNumber(-1);   // restaurar loop infinito para la musica de las siguientes escenas

    // Se espera a que SUELTEN START para que el mismo pulso no saltee tambien
    // la seleccion de jugadores.
    while (JOY_readJoypad(JOY_1) & BUTTON_START) SYS_doVBlankProcess();

    // Fundido final largo + negro pleno: el corte a la escena siguiente no se
    // siente seco. (clearScene() al final vuelve a fundir, pero para entonces
    // la pantalla ya esta en negro y no se nota.)
    PAL_fadeOutAll(INTRO_T_FADE_OUT, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();
    for (u16 t = 0; t < INTRO_T_BLACK_HOLD; t++) {
        SPR_update();
        SYS_doVBlankProcess();
    }

    for (u16 i = 0; i < 4; i++)
        if (turtleSpr[i]) { SPR_releaseSprite(turtleSpr[i]); turtleSpr[i] = NULL; }
    if (tapaSpr) { SPR_releaseSprite(tapaSpr); tapaSpr = NULL; }
    // Por si START salteo la intro durante la escena A (las nubes normalmente
    // ya se liberan solas al final de esa escena, ver introBlockDolly).
    if (nubeChicaSpr)  { SPR_releaseSprite(nubeChicaSpr);  nubeChicaSpr  = NULL; }
    if (nubeChica2Spr) { SPR_releaseSprite(nubeChica2Spr); nubeChica2Spr = NULL; }
    SPR_update();
    SYS_doVBlankProcess();

    VDP_setBackgroundColor(0);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);
    SYS_doVBlankProcess();

    VDP_setScreenWidth320();
    VDP_setPlaneSize(32, 32, TRUE);
    SPR_initEx(752);
    clearScene();
    return SCENE_VRAM_CLEAR;
}
