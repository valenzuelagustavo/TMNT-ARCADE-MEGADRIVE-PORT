// ===========================================================================
// level5_1.c - Scene 5: la AUTOPISTA (freeway) (26/09)
// ===========================================================================
// Tercer nivel con arte del proyecto del companero (Ray Project): la ruta con
// los edificios y el skyline de atras con parallax. Motor: stage_level.c en su
// variante con CAPA LEJANA:
//   - BG_A: la ruta (streameada, PAL0). Tiene 2369 tiles unicos, mas de los
//     que admite un IMAGE: va en el formato ancho de stage_bg (BIN de tiles y
//     mapa, ver tools/gen_level5_1.py). Su indice 0 es transparente: por los
//     huecos entre edificios se ve el skyline.
//   - BG_B: el skyline (bg_freeway_far, PAL3) a 1/3 de la camara, y en las
//     filas 0-3 el HUD con prioridad alta.
//   - Color de fondo del VDP: el celeste del cielo del skyline (PAL3[2]),
//     asi el cielo sigue por detras del HUD.
//
// CAMINABLE: la calzada, del borde de abajo del guardarrail al piso.
// Sin jefe: el nivel se gana con las oleadas limpias al llegar al final, y
// sigue la Scene 6 (la segunda autopista).
// Musica (01/10): "13 - Highway Blockade (Scene 3-1)" (music_freeway).
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level5_1.h"          // pal_freeway, bg_freeway_tiles/map, bg_freeway_far
#include "stage_level.h"
#include "enemy.h"             // ENEMY_TYPE_*
#include "audio.h"

#define LVL51_W            2104

static const SbgRaw road51 = {
    (const u32*) bg_freeway_tiles,
    (const u16*) bg_freeway_map,
    LVL51_W / 8, 28,
    &pal_freeway,
};

#define P ENEMY_TYPE_FOOT_SOLDIER
#define O ENEMY_TYPE_FOOT_SOLDIER_ORANGE
static const StageWave waves51[] = {
    {  160,    0, 3, { P, P, O },    { +1, -1, +1 } },
    {  500,  320, 3, { O, P, O },    { -1, +1, +1 } },
    {  860,  680, 4, { P, O, P, P }, { +1, -1, +1, -1 } },
    { 1240, 1060, 4, { O, P, O, P }, { -1, +1, -1, +1 } },
    { 1620, 1440, 4, { P, O, O, P }, { +1, +1, -1, -1 } },
    { 1960, 1784, 4, { O, P, P, O }, { -1, +1, -1, +1 } },
};
#undef P
#undef O

static const StageLevel level51 = {
    .bg           = NULL,
    .bgRaw        = &road51,
    .fg           = NULL,
    .far          = &bg_freeway_far,
    .farDiv       = 3,
    .farRowShift  = 4,              // el skyline asoma justo arriba del guardarrail
    .backdrop     = 48 + 2,         // PAL3[2]: el celeste del cielo
    .bgSlots      = 736,            // >= 715, el peor caso medido
    .levelW       = LVL51_W,
    .walkTop      = NULL,
    .walkCols     = 0,
    .walkYMin     = 112,            // borde de abajo del guardarrail
    .walkYMax     = 216,
    .waves        = waves51,
    .nWaves       = sizeof(waves51) / sizeof(waves51[0]),
    .bossFeetX    = 2000,
    .music        = music_freeway,
    .musicVol     = 80,
    .nextScene    = SCENE_6_1_TITLE,
};

SceneId showScene51() {
    return stageLevelRun(&level51);
}
