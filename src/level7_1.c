// ===========================================================================
// level7_1.c - Scene 7: la FABRICA (factory) (26/09)
// ===========================================================================
// Quinto nivel con arte del proyecto del companero (Ray Project): el fondo de
// la fabrica (2024x224), las dos columnas de reticulado que pasan por delante
// y su colision pintada (col_factory.bin -> level7_1_limits.c, ver
// tools/gen_level7_1.py). Motor: stage_level.c, variante normal (fondo en
// BG_B, primer plano en BG_A con prioridad alta, como la cloaca).
//
// JEFE: el TENIENTE GRANITOR (granitor.c, portado del companero). Con las
// oleadas limpias y la camara en el fondo entra caminando por la derecha; el
// nivel se gana cuando se desarma (y sigue la Scene 8). PAL3 es suya.
// Musica: todavia no hay tema de la fabrica; suena el del 2-1.
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level7_1.h"          // bg_factory, bg_factory_fg (rescomp)
#include "level7_1_limits.h"   // lvl71WalkTop (generado)
#include "stage_level.h"
#include "enemy.h"             // ENEMY_TYPE_*
#include "granitor.h"
#include "audio.h"

#define SCREEN_W            320
#define LVL71_W            2024

#define P ENEMY_TYPE_FOOT_SOLDIER
#define O ENEMY_TYPE_FOOT_SOLDIER_ORANGE
static const StageWave waves71[] = {
    {  200,    0, 3, { P, P, O },    { +1, -1, +1 } },
    {  560,  380, 3, { O, P, P },    { -1, +1, +1 } },
    {  920,  740, 4, { P, O, P, O }, { +1, -1, +1, -1 } },
    { 1300, 1120, 4, { O, P, O, P }, { -1, +1, -1, +1 } },
    { 1680, 1500, 4, { P, O, P, O }, { +1, +1, -1, -1 } },
};
#undef P
#undef O

// ---------------------------------------------------------------------------
// Jefe: Granitor y sus llamas
// ---------------------------------------------------------------------------
static Granitor granitor;

static void bossInit71(void) {
    granitorInit(&granitor);
}

static void bossStart71(s16 camX, s16 levelW) {
    (void)levelW;
    granitorSpawn(&granitor, (s16)(camX + 24), (s16)(camX + SCREEN_W - 24),
                  LVL71_TOP_MIN, 216, stageWalkTopAt);
}

static bool bossUpdate71(Player** pls, u8 nPl, s16 camX) {
    granitorUpdate(&granitor, pls, nPl, camX);
    granitorFlameUpdate(pls, nPl, camX);
    s8 killer = -1;
    if (granitorPlayerHits(&granitor, pls, nPl, &killer) && killer >= 0)
        addPlayerScore(pls[(u8)killer], 5);
    return granitorIsGone(&granitor);
}

static bool bossDying71(void) {
    return granitorIsDying(&granitor);
}

static void bossRelease71(void) {
    granitorRelease(&granitor);
}

static const StageLevel level71 = {
    .bg           = &bg_factory,
    .fg           = &bg_factory_fg,
    .bgSlots      = 384,            // >= 358, el peor caso medido
    .levelW       = LVL71_W,
    .walkTop      = lvl71WalkTop,
    .walkCols     = LVL71_COLS,
    .walkYMin     = LVL71_TOP_MIN,
    .walkYMax     = 216,
    .waves        = waves71,
    .nWaves       = sizeof(waves71) / sizeof(waves71[0]),
    .bossFeetX    = 1800,
    .music        = music_stage2_1,
    .musicVol     = 90,
    .bossMusic    = music_boss,
    .bossMusicVol = 90,
    .bossInit     = bossInit71,
    .bossStart    = bossStart71,
    .bossUpdate   = bossUpdate71,
    .bossDying    = bossDying71,
    .bossRelease  = bossRelease71,
    .nextScene    = SCENE_8_1_TITLE,
};

SceneId showScene71() {
    return stageLevelRun(&level71);
}
