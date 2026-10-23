#ifndef _STAGE_LEVEL_H_
#define _STAGE_LEVEL_H_

#include <genesis.h>
#include "scenes.h"
#include "player.h"
#include "stage_bg.h"
#include "enemy.h"   // Enemy (stageEnemies)

// ===========================================================================
// STAGE_LEVEL — nivel generico de scroll horizontal (26/09)
// ===========================================================================
// Salio de la Scene 3 (sewer) cuando llego la Scene 4 (garage): todo lo que
// no depende del escenario vive aca y cada nivel es una tabla de datos mas
// los ganchos de su jefe.
//
//   - Fondo con cache de tiles por columna (stage_bg.c) en BG_B, PAL0; el
//     color de fondo del VDP es PAL0[0] (los rips usan el indice 0 como
//     color de verdad).
//   - Primer plano opcional en BG_A con prioridad alta (las tortugas pasan
//     por detras), sin las filas del HUD. BG_A y BG_B scrollean por fila de
//     tile: filas 0-3 fijas (HUD), el resto con la camara.
//   - Franja caminable: tope por columna de 8 px (tabla del nivel) y un piso.
//   - (28/09, sewer) ESCALON opcional: una banda de profundidad que parte la
//     franja en dos pisos (la vereda y el canal). Caminando no se cruza: de
//     arriba hacia abajo uno se deja caer, y de abajo hacia arriba hay que
//     SALTAR (como la cornisa del 2-1). Los soldiers lo cruzan con un
//     saltito. Ver ledgeTop/ledgeBot.
//   - Tortugas 1..4, HUD, P2 que se suma, continues, pausa, especial.
//   - Oleadas de foot soldiers con camara bloqueada hasta limpiarlas.
//   - (26/09, freeway) Variante con CAPA LEJANA: si el nivel trae 'far', el
//     fondo streameado va en BG_A (baja prioridad, indice 0 transparente) y
//     la capa lejana en BG_B con parallax y PAL3; el HUD y el texto pasan a
//     BG_B (filas 0-3 fijas, prioridad alta). No admite primer plano.
//   - Al final, el jefe: el nivel lo arranca con bossStart cuando las
//     oleadas estan limpias y la camara llego al fondo, y lo actualiza con
//     bossUpdate hasta que devuelve TRUE (derrotado y ya fuera de escena).
// ===========================================================================

// (12/10) Hasta 8 por oleada (las del sewer del arcade traen hasta 6). Igual
// nunca hay mas vivos que STAGE_MAX_ALIVE_*: el resto espera su turno.
#define STAGE_WAVE_MAX 8
// side: -1 entra por la izquierda, +1 por la derecha, STAGE_IN_WATER sale del
// AGUA (sewer) en la X de mundo x[] con un salpicon. STAGE_FROM_BACK (12/10)
// aparece en el FONDO del piso (walkYMin) en la X de mundo x[]: los que en el
// arcade bajan de la caja del camion o del hueco del auto (garage).
#define STAGE_IN_WATER  2
#define STAGE_FROM_BACK 3
// (12/10) Para los actores de un nivel que sueltan soldiers (stageSpawnEnemyAt):
// STAGE_JUMP_IN salta (el morado) o baja caminando (los demas) en (x, y) --
// los que saltan del auto de la autopista --; STAGE_IN_PLACE aparece ahi
// caminando (el que ya tiro el misil pasa a ser un morado comun).
#define STAGE_JUMP_IN   4
#define STAGE_IN_PLACE  5
// (12/10) type >= STAGE_SPECIAL en una oleada no es un soldier: es un ACTOR del
// nivel (auto, motos, el del misil...). Lo arranca L->specialSpawn (FALSE =
// todavia no, se reintenta el frame siguiente) y L->specialAlive dice cuantos
// siguen en juego (cuentan como vivos: la camara no se suelta).
#define STAGE_SPECIAL   0x80
typedef struct {
    s16 trigX;                  // pies del lider (mundo) que la disparan
    s16 lockX;                  // camara clavada aca mientras dure
    u8  n;
    u8  type[STAGE_WAVE_MAX];   // ENEMY_TYPE_*
    s8  side[STAGE_WAVE_MAX];   // -1 izquierda, +1 derecha, STAGE_IN_WATER, STAGE_FROM_BACK
    s16 x[STAGE_WAVE_MAX];      // X de mundo de las que salen del agua / del fondo
} StageWave;

// (22/10) BANDAS DE PARALLAX en BG_B. Cada banda es una franja de filas de
// tile del plano que se mueve a num/den de la camara, con su propio mapa (w
// celdas de ancho, rows de alto; u16 por celda como stage_raw: indice en los
// bits 0-12, flips en 13/14; indice 0 = vacio). Las columnas se dibujan a
// medida que entran, cada banda con su X. Conviene que cada banda se repita en
// X con un periodo multiplo de 8.
// (22/10) slots == 0: los tiles de todas las bandas van ENTEROS en VRAM.
// slots > 0: pasan por un CACHE de 'slots' tiles (como el de stage_bg): un
// tile se sube cuando entra la primera columna que lo usa y se suelta cuando
// sale la ultima. 'slots' tiene que cubrir el peor caso en pantalla (lo mide
// el generador del nivel); si faltara, la celda sale vacia.
#define STAGE_MAX_BANDS 4
typedef struct {
    u16        row0, rows;    // filas de tile del plano
    u16        w;             // ancho del mapa (celdas); se repite si hace falta
    u16        num, den;      // velocidad: num/den de la camara
    const u16* map;
} StageBand;
typedef struct {
    const u32*       tiles;
    u16              nTiles;
    u16              n;
    const StageBand* band;
    u16              slots;   // (22/10) 0 = enteras; > 0 = cache de ese tamano
} StageBands;

typedef struct {
    // --- Escenario ---
    const Image*  bg;           // IMAGE NONE (se streamea), o NULL si va bgRaw
    const SbgRaw* bgRaw;        // fondo en formato ancho (> 2048 tiles), o NULL
    const Image*  fg;           // IMAGE NONE, o NULL
    // (28/09) El primer plano en BG_A NO se dibuja en las filas 0-3 (son del
    // HUD y no scrollean). Lo que caiga ahi va como SPRITES de 8x32: un
    // frame de fgTop por columna de tile, en la X de mundo fgTopX[i]. NULL/0
    // = nada. Van delante de los personajes y detras de los marcos del HUD.
    const SpriteDefinition* fgTop;
    const s16*    fgTopX;
    u16           fgTopN;
    // (29/09) fgTopF != NULL: fgTop trae un frame por columna DISTINTA y
    // fgTopF[i] dice cual usa la columna i. Los tiles de cada frame se cargan
    // UNA vez (fgTopFrames frames, VRAM de planos) y los sprites los comparten.
    const u8*     fgTopF;
    u16           fgTopFrames;
    // (18/10) OVERLAY en BG_A (prioridad BAJA: detras de los sprites, delante
    // del fondo): un rectangulo de ovW x ovH tiles con la esquina en la
    // columna de tile de mundo ovCol, fila ovRow (>= las del HUD). Se dibuja
    // con el streaming de columnas del primer plano (tambien en modo liviano)
    // con PAL1 (la de las tortugas) y tiles propios en VRAM de planos
    // (ovW*ovH). El nivel carga el frame que quiera con stageOverlayFrame.
    // Para dibujos quietos/animados en la pared que como sprite se comeria el
    // limite de pixeles por linea (Splinter atado en la fabrica). ovW 0 = nada.
    s16           ovCol;
    u16           ovRow, ovW, ovH;
    // (18/10) TILES EXTRA reservados para el nivel (fuera del pool de
    // sprites, despues del overlay): sprites con tiles compartidos cargados a
    // mano (las columnas con laser de la fabrica). Base: stageExtraVram().
    u16           extraTiles;
    // (29/09) CAMARA VERTICAL. camYMin < 0: el fondo (y el primer plano) tiene
    // -camYMin px mas ARRIBA de la y = 0 del mundo (fila 0 de la imagen =
    // y camYMin) y la camara sube hasta ahi siguiendo a las tortugas. Alto de
    // la imagen: 224 - camYMin (maximo 256 = el plano de 32 filas). El HUD pasa
    // al plano WINDOW (filas 0-3), asi BG_A puede scrollear en vertical entero.
    // 0 = sin camara vertical (como siempre).
    s16           camYMin;
    // (01/10) camYMax > 0: el fondo tiene camYMax px mas ABAJO de lo que entra
    // en pantalla y la camara baja hasta ahi siguiendo a la tortuga mas baja
    // (fila 0 de la imagen = y 0, como siempre). Alto: 224 + camYMax. Tambien
    // manda el HUD a WINDOW. 0 = nada.
    s16           camYMax;
    // (01/10) Regiones del fondo con variantes (ver SbgRegion en stage_bg.h).
    const SbgRegion* bgRegions;
    u16           nBgRegions;
    const Image*  far;          // capa lejana (entera en VRAM, PAL3), o NULL
    // (12/10) farPal0: la capa lejana va con PAL0 (su imagen ya trae los indices
    // de la paleta del fondo) y PAL3 queda libre; pal3 = lo que se carga en
    // PAL3 al revelar (NULL = negro, como siempre).
    bool          farPal0;
    const u16*    pal3;
    u16           farDiv;       // parallax: la capa lejana anda camX / farDiv
    u16           farRowShift;  // filas de la capa lejana que se saltean arriba
    u16           backdrop;     // indice de CRAM del color de fondo (0 = PAL0[0])
    u16           bgSlots;      // cache del fondo (>= peor caso en 42 columnas)
    s16           levelW;       // ancho en px
    const u8*     walkTop;      // tope caminable por columna de 8 px
    u16           walkCols;
    s16           walkYMin;     // franja de profundidad del motor (pies)
    s16           walkYMax;
    // (28/09) Escalon: pies <= ledgeTop es el piso de ARRIBA, >= ledgeBot el
    // de ABAJO; entre medio es la cara del escalon (solo se pasa en el aire).
    // ledgeBot = 0 -> sin escalon.
    s16           ledgeTop;
    s16           ledgeBot;
    // (18/10, fabrica) Escalon de un TRAMO: la cara existe solo con los pies
    // en [ledgeX0, ledgeX1) (0,0 = todo el ancho, como el sewer). Ahi el tope
    // caminable es el de la plataforma; fuera del tramo la banda es piso
    // comun. Arriba los costados van en diagonal (como el arte): el borde de
    // adelante (y = ledgeTop) va de X0 + ledgeInset a X1 - ledgeInset y se
    // abre 1 px por cada px hacia el fondo. Se sube saltando y se baja por el
    // borde de adelante; los costados de arriba son pared.
    s16           ledgeX0;
    s16           ledgeX1;
    s16           ledgeInset;
    // --- Guion ---
    const StageWave* waves;
    u16           nWaves;
    s16           bossFeetX;    // pies del lider para que entre el jefe
    // --- Audio ---
    const u8*     music;
    u16           musicVol;
    const u8*     bossMusic;
    u16           bossMusicVol;
    // (01/10) TRUE = el jefe tiene voice over de entrada: stage_level NO
    // arranca bossMusic en el bossStart, lo hace el nivel con bossVoStart
    // cuando el jefe aparece (primero la voz completa, despues el tema).
    bool          bossMusicByVo;
    // --- Jefe (todos pueden ser NULL: sin jefe, el nivel se gana al llegar) ---
    void (*bossInit)(void);
    void (*bossStart)(s16 camX, s16 levelW);
    bool (*bossUpdate)(Player** pls, u8 nPl, s16 camX);   // TRUE = terminado
    bool (*bossDying)(void);    // TRUE mientras muere (no se evaluan continues)
    void (*bossRelease)(void);
    // --- Ganchos propios del nivel (29/09; todos pueden ser NULL) ---
    // levelInit: despues de crear jugadores y HUD. levelUpdate: cada frame,
    // despues de los enemigos. levelRelease: al salir, antes de clearScene.
    void (*levelInit)(void);
    // (18/10) INTRO del nivel (la fabrica: los Mousers llevandose a Splinter).
    // Si introStep != NULL: los jugadores arrancan ESCONDIDOS, introBegin
    // (puede ser NULL) puede tocar la paleta del fundido de entrada (target,
    // 64 colores), y despues del fundido se llama a introStep una vez por
    // frame hasta que devuelva FALSE. Ahi las tortugas entran cayendo de un
    // salto desde la izquierda (como en el 2-1) y arranca el nivel.
    void (*introBegin)(u16* target);
    bool (*introStep)(s16 camX);
    void (*levelUpdate)(Player** pls, u8 nPl, s16 camX);
    void (*levelRelease)(void);
    // (12/10) FONDO QUE CORRE SOLO (Scene 6, patinetas): autoScrollQ != 0 =
    // la camara del juego queda fija (levelW = 320) y lo que avanza es el
    // fondo, autoScrollQ (Q8 px/f) por frame, repitiendose en X. El avance
    // (stageProgress) reemplaza a los pies del lider: dispara las oleadas
    // (trigX) y el final (bossFeetX). skateLift != 0: tortugas y soldiers
    // andan en patineta (se dibujan asi de mas arriba y no caminan).
    u16           autoScrollQ;
    // (14/10) Velocidad a la que SE VE correr el fondo (Q8 px/f); 0 = la de
    // autoScrollQ. Separada del avance para poder darle mas sensacion de
    // velocidad sin que las oleadas lleguen antes ni el nivel dure menos.
    u16           autoScrollVisQ;
    s16           skateLift;
    // (12/10) Actores de oleada (ver STAGE_SPECIAL). Pueden ser NULL.
    bool (*specialSpawn)(u8 type, s8 side, s16 x, s16 camX);
    u16  (*specialAlive)(void);
    // (18/10) Si un actor no puede arrancar (casi siempre: no hay VRAM de
    // sprites, p.ej. con 4 jugadores) y no queda nadie vivo, a los
    // STAGE_VRAM_SKIP frames se lo saltea para que la oleada (y la camara) no
    // se trabe. Antes de saltearlo se llama a esto, si no es NULL, para que el
    // nivel ponga algo en su lugar (el auto: sus foot soldiers igual entran).
    void (*specialFallback)(u8 type, s8 side, s16 x, s16 camX);
    // --- Salida ---
    SceneId       nextScene;    // al ganar
    // (21/10, nieve) RELLENO DE BG_A: un patron de aPatW x aPatH celdas (los
    // dos tienen que dividir al plano de 64x32) que se dibuja UNA vez en todo
    // BG_A al arrancar y queda fijo en el plano. Cada celda: 0 = vacio, k =
    // el tile k-1 de los extraTiles del nivel (los carga el nivel en levelInit
    // en stageExtraVram()), con PAL0 y prioridad aPatHigh. Como el plano
    // entero es el patron, BG_A se puede correr con stageAOffX / stageAOffY
    // sin redibujar nada: la nieve que cae. Solo para niveles SIN primer plano
    // ni overlay (fg == NULL, ovW == 0), que son los que dejan BG_A libre.
    const u8*     aPat;
    u16           aPatW, aPatH;
    bool          aPatHigh;
    // (21/10) TOPE DE SOLDIERS VIVOS A LA VEZ para este nivel, con 1 jugador y
    // con 2 o mas. 0 = el de siempre (STAGE_MAX_ALIVE_1P / _2P en
    // stage_level.c, pensado para la VRAM de los fondos mas caros). Nunca pasa
    // de MAX_ENEMIES. Ojo que antes que la VRAM aparecen el limite de sprites
    // por linea (parpadeo) y la CPU (slowdown): afinarlo probando.
    u16           maxAlive1P;
    u16           maxAlive2P;
    // (22/10) PARALLAX: con bands != NULL el fondo streameado (bgRaw/bg) va en
    // BG_A como FRENTE (lo vacio, indice 0, deja ver BG_B), BG_B lleva las
    // bandas y el HUD pasa a WINDOW. Sin primer plano ni capa lejana (fg/far
    // NULL) ni relleno aPat. Las bandas siguen en el modo liviano (3-4 jug.).
    const StageBands* bands;
} StageLevel;

// (21/10) Corrimiento extra de BG_A (px) sobre el de la camara, para el
// relleno aPat: X suma al scroll horizontal (positivo = el patron va a la
// derecha) y Y lo baja (positivo = el patron baja). Los pone el nivel en
// levelUpdate; stage_level los vuelve a 0 al entrar y al salir. Solo en la
// variante sin capa lejana.
extern s16 stageAOffX, stageAOffY;

SceneId stageLevelRun(const StageLevel* L);

// Tope caminable del nivel en curso (para los jefes y sus secuaces).
s16 stageWalkTopAt(s16 worldX);

// (01/10) Arreglo de MAX_ENEMIES foot soldiers del nivel en curso.
Enemy* stageEnemies(void);

// (12/10) Suelta un soldier fuera de las oleadas (los actores del nivel): en
// (x, y) de mundo con side STAGE_JUMP_IN / STAGE_IN_PLACE, o por un borde de
// la camara (-1 / +1). FALSE si no se puede ahora (ya hay STAGE_MAX_ALIVE_*
// vivos -- salvo STAGE_IN_PLACE, que reemplaza a un actor --, no hay lugar o
// no entra su hoja en la VRAM): se reintenta despues.
bool stageSpawnEnemyAt(u8 type, s8 side, s16 x, s16 y);
// (18/10) Carga los ovW*ovH tiles de un frame del overlay (ver ovW).
void stageOverlayFrame(const u32* tiles);
// (18/10) Primer tile de los extraTiles del nivel en curso.
u16 stageExtraVram(void);
// (18/10) TEMBLOR de pantalla: 'frames' cuadros de sacudida horizontal de los
// planos (fondo y primer plano, no el HUD ni los sprites), como la capsula
// del 1-2. Si ya estaba temblando se queda con el mas largo.
void stageShake(u8 frames);
// Soldiers vivos (estado != INACTIVE).
u16 stageAliveEnemies(void);
// (12/10) Avance del fondo en los niveles con autoScrollQ (px).
s16 stageProgress(void);

#endif
