// ===========================================================================
// level3_1.c - Scene 3: la CLOACA (sewer) (26/09)
// ===========================================================================
// Primer nivel armado con material del proyecto del companero (Ray Project).
// (28/09) Fondo y primer plano NUEVOS, sacados de la hoja "Arcade - Teenage
// Mutant Ninja Turtles - Backgrounds - Stage 3.png" (tools/gen_level3_1_bg.py);
// la colision sigue siendo la pintada de Ray (col_sewer.bin ->
// level3_1_limits.c, ver tools/gen_level3_1.py).
//
// Desde la Scene 4 el motor vive en stage_level.c: aca solo quedan los datos
// del nivel (fondo, franja caminable, oleadas, musica) y los ganchos del jefe.
//
// FONDO: 2253 tiles unicos, en el formato ancho de stage_bg.c (no entra en
// un IMAGE); peor caso en 42 columnas: 796. PRIMER PLANO: los caños (74
// tiles) en BG_A con prioridad alta; las tortugas pasan por detras.
//
// ESCALON (28/09): la vereda y el canal ya no son una sola franja. La cara
// del escalon (el recuadro negro que pinto Gustavo en "ejemplo de escalon.png":
// filas 192..213 del fondo = Y de mundo 160..181) no se pisa: desde la vereda
// uno se deja caer al agua, y desde el agua hay que SALTAR para subir (ver
// ledgeTop/ledgeBot en stage_level.h).
//
// JEFE: BAXTER STOCKMAN (baxter.c). Con las oleadas limpias y la camara en el
// fondo entra volando por la izquierda; el nivel se gana cuando su nave
// explota y no quedan ratas. PAL3 es suya.
// Musica: music_level3 (res/audio.res), el tema del sewer.
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level3_1.h"          // pal_sewer, bg_sewer_tiles/map, bg_sewer_fg (rescomp)
#include "level3_1_limits.h"   // lvl31WalkTop (generado)
#include "level3_1_fgtop.h"    // lvl31FgTopX (generado, tools/gen_level3_1_bg.py)
#include "stage_level.h"
#include "enemy.h"             // ENEMY_TYPE_*
#include "baxter.h"
#include "audio.h"

#define SCREEN_W            320
#define LVL31_W            1248
#define LVL31_LEDGE_TOP     160     // ultima Y de pies de la vereda
#define LVL31_LEDGE_BOT     182     // primera Y de pies del canal

static const SbgRaw sewer31 = {
    (const u32*) bg_sewer_tiles,
    (const u16*) bg_sewer_map,
    LVL31_W / 8, 28,
    &pal_sewer,
};

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
    .bgRaw        = &sewer31,
    .fg           = &bg_sewer_fg,
    .fgTop        = &sewer_fg_top,  // los caños siguen en la franja del HUD
    .fgTopX       = lvl31FgTopX,
    .fgTopN       = LVL31_FGTOP_N,
    .bgSlots      = 800,            // >= 796, el peor caso medido
    .levelW       = LVL31_W,
    .walkTop      = lvl31WalkTop,
    .walkCols     = LVL31_COLS,
    .walkYMin     = 96,             // tope de la vereda (pies)
    .walkYMax     = 216,            // piso del canal
    .ledgeTop     = LVL31_LEDGE_TOP,
    .ledgeBot     = LVL31_LEDGE_BOT,
    .waves        = waves31,
    .nWaves       = sizeof(waves31) / sizeof(waves31[0]),
    .bossFeetX    = 1000,
    .music        = music_level3,
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
