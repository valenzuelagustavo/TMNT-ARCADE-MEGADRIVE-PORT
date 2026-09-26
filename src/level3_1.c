// ===========================================================================
// level3_1.c - Scene 3: la CLOACA (sewer) (26/09)
// ===========================================================================
// Primer nivel armado con material del proyecto del companero (Ray Project):
// el fondo bg_sewer.png (1248x224, 16 colores), el primer plano de caños
// bg_sewer_fg.png y su colision pintada (col_sewer.bin -> level3_1_limits.c,
// ver tools/gen_level3_1.py).
//
// Desde la Scene 4 el motor vive en stage_level.c: aca solo quedan los datos
// del nivel (fondo, franja caminable, oleadas, musica) y los ganchos del jefe.
//
// FONDO: 2037 tiles unicos, lo streamea stage_bg.c (peor caso en 42
// columnas: 689). PRIMER PLANO: los caños (74 tiles) en BG_A con prioridad
// alta; las tortugas pasan por detras.
//
// JEFE: BAXTER STOCKMAN (baxter.c). Con las oleadas limpias y la camara en el
// fondo entra volando por la izquierda; el nivel se gana cuando su nave
// explota y no quedan ratas. PAL3 es suya.
// Musica: todavia no hay tema del sewer; suena el del 2-1 (Downtown).
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level3_1.h"          // bg_sewer, bg_sewer_fg (rescomp)
#include "level3_1_limits.h"   // lvl31WalkTop (generado)
#include "stage_level.h"
#include "enemy.h"             // ENEMY_TYPE_*
#include "baxter.h"
#include "audio.h"

#define SCREEN_W            320
#define LVL31_W            1248

// ---------------------------------------------------------------------------
// OLEADAS: se disparan cuando los PIES del que va adelante pasan trigX; la
// camara queda clavada en lockX hasta que caen todos los de la oleada.
// ---------------------------------------------------------------------------
#define P ENEMY_TYPE_FOOT_SOLDIER
#define O ENEMY_TYPE_FOOT_SOLDIER_ORANGE
static const StageWave waves31[] = {
    {  150,   0, 2, { P, P },       { +1, +1 } },
    {  420, 240, 3, { P, O, P },    { -1, +1, +1 } },
    {  700, 500, 4, { O, P, P, O }, { -1, +1, -1, +1 } },
    {  980, 780, 4, { P, P, O, P }, { +1, +1, -1, -1 } },
};
#undef P
#undef O

// ---------------------------------------------------------------------------
// Ganchos del jefe: Baxter y sus ratas
// ---------------------------------------------------------------------------
static Baxter baxter;

static void bossInit31(void) {
    baxterInit(&baxter);
    baxterRatInitAll();
}

static void bossStart31(s16 camX, s16 levelW) {
    (void)levelW;
    baxterSpawn(&baxter, (s16)(camX + 16), (s16)(camX + SCREEN_W - 16), 96, 216);
}

static bool bossUpdate31(Player** pls, u8 nPl, s16 camX) {
    baxterUpdate(&baxter, pls, nPl, camX);
    baxterRatUpdateAll(pls, nPl, camX, stageWalkTopAt);
    s8 killer = -1;
    if (baxterPlayerHits(&baxter, pls, nPl, &killer) && killer >= 0)
        addPlayerScore(pls[(u8)killer], 5);
    baxterRatPlayerHits(pls, nPl);
    return baxterIsGone(&baxter) && baxterRatAliveCount() == 0;
}

static bool bossDying31(void) {
    return baxter.state == BAXTER_DEAD || baxter.state == BAXTER_GONE;
}

static void bossRelease31(void) {
    baxterRelease(&baxter);
}

static const StageLevel level31 = {
    .bg           = &bg_sewer,
    .fg           = &bg_sewer_fg,
    .bgSlots      = 704,            // >= 689, el peor caso medido
    .levelW       = LVL31_W,
    .walkTop      = lvl31WalkTop,
    .walkCols     = LVL31_COLS,
    .walkYMin     = 96,             // tope de la vereda (pies)
    .walkYMax     = 216,            // piso del canal
    .waves        = waves31,
    .nWaves       = sizeof(waves31) / sizeof(waves31[0]),
    .bossFeetX    = 1000,
    .music        = music_stage2_1,
    .musicVol     = 90,
    .bossMusic    = music_boss,
    .bossMusicVol = 90,
    .bossInit     = bossInit31,
    .bossStart    = bossStart31,
    .bossUpdate   = bossUpdate31,
    .bossDying    = bossDying31,
    .bossRelease  = bossRelease31,
    .nextScene    = SCENE_4_1_TITLE,
};

SceneId showScene31() {
    return stageLevelRun(&level31);
}
