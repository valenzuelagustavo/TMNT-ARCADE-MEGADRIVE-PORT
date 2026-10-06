// ===========================================================================
// level6_1.c - Scene 6: la segunda autopista (skate) (26/09)
// ===========================================================================
// Cuarto nivel con arte del proyecto del companero (Ray Project): la autopista
// larga (6376 px) con el guardarrail de rejas y, arriba, el cielo con nubes y
// el skyline en parallax. Mismo motor y misma variante que la Scene 5
// (stage_level.c con CAPA LEJANA: la ruta en BG_A, el cielo en BG_B con PAL3,
// el HUD en BG_B).
//
// En el arcade este tramo se hace en PATINETA (tortugas y foot soldiers sobre
// skates). Todavia no hay sprites de eso: por ahora se camina, como en la
// version de Ray.
//
// CAMINABLE: la calzada, del borde de abajo del guardarrail al piso.
// Sin jefe: el nivel se gana con las oleadas limpias al llegar al final, y
// sigue la Scene 7 (la fabrica).
// Musica: todavia no hay tema propio; suena el del 2-1.
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level6_1.h"          // bg_skate, bg_skate_far (rescomp)
#include "stage_level.h"
#include "enemy.h"             // ENEMY_TYPE_*
#include "audio.h"

#define LVL61_W            6376

#define P ENEMY_TYPE_FOOT_SOLDIER
#define O ENEMY_TYPE_FOOT_SOLDIER_ORANGE
static const StageWave waves61[] = {
    {  200,    0, 3, { P, P, O },    { +1, -1, +1 } },
    {  760,  580, 3, { O, P, P },    { -1, +1, +1 } },
    { 1340, 1160, 4, { P, O, P, O }, { +1, -1, +1, -1 } },
    { 1920, 1740, 4, { P, P, O, P }, { -1, +1, +1, -1 } },
    { 2500, 2320, 4, { O, P, O, P }, { +1, -1, -1, +1 } },
    { 3080, 2900, 4, { P, O, P, P }, { -1, +1, -1, +1 } },
    { 3660, 3480, 4, { O, O, P, P }, { +1, +1, -1, -1 } },
    { 4240, 4060, 4, { P, O, P, O }, { -1, +1, +1, -1 } },
    { 4820, 4640, 4, { O, P, P, O }, { +1, -1, +1, -1 } },
    { 5400, 5220, 4, { P, O, O, P }, { -1, +1, -1, +1 } },
    { 5980, 6056, 4, { O, P, O, P }, { +1, -1, +1, -1 } },
};
#undef P
#undef O

static const StageLevel level61 = {
    .bg           = &bg_skate,
    .fg           = NULL,
    .far          = &bg_skate_far,
    .farDiv       = 3,
    .farRowShift  = 2,              // el skyline asoma justo arriba del guardarrail
    .backdrop     = 48 + 14,        // PAL3[14]: el celeste de arriba del skyline
    .bgSlots      = 448,            // >= 415, el peor caso medido
    .levelW       = LVL61_W,
    .walkTop      = NULL,
    .walkCols     = 0,
    .walkYMin     = 130,            // borde de abajo del guardarrail
    .walkYMax     = 216,
    .waves        = waves61,
    .nWaves       = sizeof(waves61) / sizeof(waves61[0]),
    .bossFeetX    = 6280,
    .music        = music_skate,
    .musicVol     = 80,
    .nextScene    = SCENE_7_1_TITLE,
};

SceneId showScene61() {
    return stageLevelRun(&level61);
}
