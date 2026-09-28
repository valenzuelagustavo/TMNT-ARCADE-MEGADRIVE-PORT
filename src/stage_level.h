#ifndef _STAGE_LEVEL_H_
#define _STAGE_LEVEL_H_

#include <genesis.h>
#include "scenes.h"
#include "player.h"
#include "stage_bg.h"

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

#define STAGE_WAVE_MAX 4
typedef struct {
    s16 trigX;                  // pies del lider (mundo) que la disparan
    s16 lockX;                  // camara clavada aca mientras dure
    u8  n;
    u8  type[STAGE_WAVE_MAX];   // ENEMY_TYPE_*
    s8  side[STAGE_WAVE_MAX];   // -1 entra por la izquierda, +1 por la derecha
} StageWave;

typedef struct {
    // --- Escenario ---
    const Image*  bg;           // IMAGE NONE (se streamea), o NULL si va bgRaw
    const SbgRaw* bgRaw;        // fondo en formato ancho (> 2048 tiles), o NULL
    const Image*  fg;           // IMAGE NONE, o NULL
    const Image*  far;          // capa lejana (entera en VRAM, PAL3), o NULL
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
    // --- Guion ---
    const StageWave* waves;
    u16           nWaves;
    s16           bossFeetX;    // pies del lider para que entre el jefe
    // --- Audio ---
    const u8*     music;
    u16           musicVol;
    const u8*     bossMusic;
    u16           bossMusicVol;
    // --- Jefe (todos pueden ser NULL: sin jefe, el nivel se gana al llegar) ---
    void (*bossInit)(void);
    void (*bossStart)(s16 camX, s16 levelW);
    bool (*bossUpdate)(Player** pls, u8 nPl, s16 camX);   // TRUE = terminado
    bool (*bossDying)(void);    // TRUE mientras muere (no se evaluan continues)
    void (*bossRelease)(void);
    // --- Salida ---
    SceneId       nextScene;    // al ganar
} StageLevel;

SceneId stageLevelRun(const StageLevel* L);

// Tope caminable del nivel en curso (para los jefes y sus secuaces).
s16 stageWalkTopAt(s16 worldX);

#endif
