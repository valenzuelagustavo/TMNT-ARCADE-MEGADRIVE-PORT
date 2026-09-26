// ===========================================================================
// level4_1.c - Scene 4: el ESTACIONAMIENTO (garage) (26/09)
// ===========================================================================
// Segundo nivel con arte del proyecto del companero (Ray Project): el fondo
// bg_garage.png (1288x224, 16 colores, sin primer plano). El motor es
// stage_level.c, el mismo de la cloaca: aca solo van los datos y el jefe.
//
// CAMINABLE: la colision del companero es una sola franja pareja, del borde
// de abajo de los autos (128) al piso de la pantalla. Sin tabla: walkTop NULL
// hace que stageWalkTopAt devuelva walkYMin.
//
// JEFE: BEBOP (bebop.c, el mismo del 2-1) en una arena propia: aparece
// parado en el ASCENSOR del fondo a la derecha, se queda un instante mirando
// y salta al piso. En el arcade aca pelean Bebop y Rocksteady juntos, pero
// con dos tortugas la VRAM de sprites no da para los dos jefes juntos; por
// ahora va solo Bebop, como en la version de Ray.
// Musica: todavia no hay tema del garage; suena el del 2-1 (Downtown).
// Al ganar: Scene 5 (la autopista).
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level4_1.h"          // bg_garage (rescomp)
#include "stage_level.h"
#include "enemy.h"             // ENEMY_TYPE_*
#include "bebop.h"
#include "audio.h"

#define SCREEN_W            320
#define LVL41_W            1288
#define LVL41_CAM_MAX_X    (LVL41_W - SCREEN_W)     // 968
#define LVL41_WALK_Y_MIN    128
#define LVL41_WALK_Y_MAX    216

// ---------------------------------------------------------------------------
// OLEADAS
// ---------------------------------------------------------------------------
#define P ENEMY_TYPE_FOOT_SOLDIER
#define O ENEMY_TYPE_FOOT_SOLDIER_ORANGE
static const StageWave waves41[] = {
    {  160,   0, 3, { P, P, O },    { +1, -1, +1 } },
    {  440, 260, 3, { O, P, P },    { -1, +1, +1 } },
    {  720, 540, 4, { P, O, P, O }, { +1, -1, -1, +1 } },
    {  980, 800, 4, { O, P, O, P }, { -1, +1, +1, -1 } },
};
#undef P
#undef O

// ---------------------------------------------------------------------------
// Jefe: Bebop en el garage. La camara esta clavada en LVL41_CAM_MAX_X.
// ---------------------------------------------------------------------------
static s16 lvl41BotAt(s16 x) { (void)x; return LVL41_WALK_Y_MAX; }

static const BebopArena arena41 = {
    LVL41_CAM_MAX_X + 48, LVL41_CAM_MAX_X + SCREEN_W - 48,   // centro del cuerpo
    LVL41_WALK_Y_MIN + 4, LVL41_WALK_Y_MAX,
    stageWalkTopAt, lvl41BotAt,
    1106, 114,          // parado en el piso del ascensor
    1100, 172,          // salta al estacionamiento
    TRUE, 50
};

static Bebop bebop;

static void bossInit41(void) {
    bebopInit(&bebop);
}

static void bossStart41(s16 camX, s16 levelW) {
    (void)camX; (void)levelW;
    XGM2_playPCMEx(boss_scream_bebop_vo, sizeof(boss_scream_bebop_vo),
                   SOUND_PCM_CH2, 15, FALSE, FALSE);
    bebopSpawnArena(&bebop, &arena41);
}

static bool bossUpdate41(Player** pls, u8 nPl, s16 camX) {
    bebopUpdate(&bebop, pls, nPl, camX, 0);
    if (bebopCanBeHit(&bebop)) {
        s16 bcx = bebopGetCenterX(&bebop);
        s16 by  = bebopGetCenterY(&bebop);
        for (u8 k = 0; k < nPl; k++) {
            if (!playerAttackHitsBox(pls[k], bcx, by, BEBOP_BODY_HALF_W, BEBOP_BODY_H))
                continue;
            s16 dmg = isPlayerSpecialAttack(pls[k]) ? BEBOP_SPECIAL_DMG : 1;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                           SOUND_PCM_CH2, 15, FALSE, FALSE);
            if (bebopDamage(&bebop, dmg)) addPlayerScore(pls[k], 5);
            break;
        }
    }
    return bebopIsGone(&bebop);
}

static bool bossDying41(void) {
    return bebop.state == BEBOP_DEAD || bebop.state == BEBOP_GONE;
}

static void bossRelease41(void) {
    bebopRelease(&bebop);
}

static const StageLevel level41 = {
    .bg           = &bg_garage,
    .fg           = NULL,
    .bgSlots      = 640,            // >= 608, el peor caso medido
    .levelW       = LVL41_W,
    .walkTop      = NULL,           // franja pareja: walkYMin
    .walkCols     = 0,
    .walkYMin     = LVL41_WALK_Y_MIN,
    .walkYMax     = LVL41_WALK_Y_MAX,
    .waves        = waves41,
    .nWaves       = sizeof(waves41) / sizeof(waves41[0]),
    .bossFeetX    = 1060,
    .music        = music_stage2_1,
    .musicVol     = 90,
    .bossMusic    = music_boss,
    .bossMusicVol = 90,
    .bossInit     = bossInit41,
    .bossStart    = bossStart41,
    .bossUpdate   = bossUpdate41,
    .bossDying    = bossDying41,
    .bossRelease  = bossRelease41,
    .nextScene    = SCENE_5_1_TITLE,
};

SceneId showScene41() {
    return stageLevelRun(&level41);
}
