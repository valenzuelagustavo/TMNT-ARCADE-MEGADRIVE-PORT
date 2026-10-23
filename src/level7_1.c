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
// Musica (09/10): music_garage, "11 - Parking Garage (Scene 2-3) & The
// Factory (Scene 4)" -- el mismo tema que el garage 4-1, como en el arcade.
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level7_1.h"          // bg_factory, bg_factory_fg (rescomp)
#include "level7_1_limits.h"   // lvl71WalkTop (generado)
#include "level7_1_fgtop.h"    // (18/10) columnas en la franja del HUD (generado)
#include "stage_level.h"
#include "enemy.h"             // ENEMY_TYPE_*
#include "granitor.h"
#include "audio.h"
#include "enemies.h"           // (18/10) foot_soldier_white (paleta de PAL3)
#include "level3_1.h"          // (18/10) sewer_mouser(_boom): los Mousers
#include "baxter.h"            // (18/10) baxterRat*: los Mousers sueltos
#include "factory_fx.h"        // (18/10) lasers, capsula y abejas
#include "techno_hazards.h"    // (18/10) las columnas con laser del final

#define SCREEN_W            320
#define LVL71_W            2024

// (18/10) OLEADAS segun el video del arcade (seg_04 3:48 - seg_05 2:36). La
// camara del arcade se midio cuadro a cuadro (se frena en 0, 57, 185, 263,
// 386, 534, 593, 985, 1208, 1247, 1363, 1536 y 1806) y se paso al fondo del
// port (x 0,94: el ancho del arcade es 288). Lo que sale en cada parada:
//   0     3 morados                    57   morados + Mousers
//   185   naranja + 3 Mousers          263  2 naranjas y un morado
//   386   lanzas y morados (empiezan los lasers de la pared, NFP-1)
//   534   morados, lanza, naranja      593  2 naranjas, lanza, morado
//   985   2 amarillos (boomerang) + 2 morados
//   1208  5 morados                    1247 la CAPSULA del taladro: 4 abejas
//   1363  3 amarillos + 4 blancos del monociclo (aca: los del cuchillo)
//   1536  2 columnas con laser (techno_hazards) y la pizza (sin arte)
//   1806  Granitor cae del techo
#define P ENEMY_TYPE_FOOT_SOLDIER
#define O ENEMY_TYPE_FOOT_SOLDIER_ORANGE
#define S ENEMY_TYPE_FOOT_SOLDIER_SPEAR
#define Y ENEMY_TYPE_FOOT_SOLDIER_YELLOW
#define K ENEMY_TYPE_FOOT_SOLDIER_KNIFE
#define M SP_MOUSER
#define C SP_CAPSULE
#define SP_MOUSER   (STAGE_SPECIAL | 1)     // un Mouser por el borde (side)
#define SP_CAPSULE  (STAGE_SPECIAL | 2)     // la capsula en x[] con 4 abejas
#define CAP_X       1340
#define CAP_BASE_Y   100
#define CAP_BEES       8            // (18/10) 8 a la vez
static const StageWave waves71[] = {
    {  140,    0, 3, { P, P, P },             { +1, -1, +1 },                 { 0 } },
    {  240,   54, 5, { P, M, P, M, M },       { +1, +1, -1, +1, -1 },         { 0 } },
    {  360,  175, 4, { O, M, M, M },          { +1, +1, -1, +1 },             { 0 } },
    {  430,  247, 3, { O, O, P },             { +1, -1, +1 },                 { 0 } },
    {  545,  363, 6, { S, P, S, P, S, P },    { +1, -1, +1, +1, -1, +1 },     { 0 } },
    {  685,  502, 4, { P, S, O, P },          { +1, +1, -1, +1 },             { 0 } },
    {  745,  560, 4, { O, O, S, P },          { +1, -1, +1, +1 },             { 0 } },
    { 1105,  926, 4, { Y, P, Y, P },          { +1, -1, +1, -1 },             { 0 } },
    { 1315, 1135, 5, { P, P, P, P, P },       { +1, -1, +1, -1, +1 },         { 0 } },
    { 1355, 1172, 1, { C },                   { 0 },                          { CAP_X } },
    { 1465, 1281, 7, { Y, K, Y, K, Y, K, K }, { +1, +1, -1, +1, -1, +1, -1 }, { 0 } },
};
#undef P
#undef O
#undef S
#undef Y
#undef K
#undef M
#undef C

// (18/10) Las 2 columnas con laser del final (arcade: camara 1498-1600), las
// de techno_hazards.c. A x 1540/1580 quedan fuera de pantalla con la camara
// del jefe (1704): no chocan con la paleta de Granitor (PAL3).
static const ThPillarDef pillars71[2] = {
    { 1540, 168, 6, 72, 12, 20 },
    { 1580, 206, 4, 90, 18, 45 },
};

// (18/10) PLATAFORMA "NFP-1" (columnas 61-106 de la colision): arriba
// y 64..96, cara 96..128. Se sube saltando (tortugas) o con el saltito del
// escalon (soldiers): ver ledgeX0/X1 en stage_level.h.
#define PLAT_X0      488
#define PLAT_X1      856
#define PLAT_INSET    24            // borde de adelante: x 512..832
#define PLAT_TOP     94            // < 96: el piso de abajo de los costados llega a 96
#define PLAT_BOT    128

// Los Mousers (y Granitor) no usan el escalon: para ellos la plataforma es
// pared, se quedan en el piso de abajo.
static s16 walkTopLow71(s16 fx) {
    if (fx >= PLAT_X0 && fx < PLAT_X1) return PLAT_BOT;
    return stageWalkTopAt(fx);
}

static bool special71(u8 type, s8 side, s16 x, s16 camX) {
    if (type == SP_CAPSULE) return ffxCapsuleStart(x, CAP_BASE_Y, CAP_BEES);
    if (type == SP_MOUSER) {
        // Entra por el borde (en el arcade salen caminando de los costados).
        s16 mx = (s16)((side > 0) ? camX + SCREEN_W - 20 : camX + 20);
        s16 top = walkTopLow71(mx);
        s16 land = (s16)(top + 8 + (s16)(random() % (u16)(200 - top - 8)));
        return baxterRatSpawnAt(&sewer_mouser, &sewer_mouser_boom, mx, land, 0);
    }
    return TRUE;
}

static u16 specialAlive71(void) {
    return (u16)(baxterRatAliveCount() + ffxAlive());
}

// ---------------------------------------------------------------------------
// INTRO (18/10): los Mousers se llevan a Splinter
// ---------------------------------------------------------------------------
// Apenas arranca el nivel, el grupo de Mousers con Splinter a cuestas entra
// por la izquierda y cruza la pantalla hacia la derecha hasta salir. Recien
// ahi entran las tortugas, cayendo de un salto desde la izquierda (lo hace
// stage_level, ver introStep). Paleta propia en PAL3: en la fabrica no hay
// foot soldiers blancos y Granitor carga la suya al entrar.
#define INTRO_W          72        // celda del sprite
#define INTRO_FEET      176        // pies (y de pantalla del borde de abajo)
#define INTRO_SPEED_Q   448        // 1,75 px/f (8.8)
#define INTRO_ANIM_T      6        // frames de juego por cuadro de la anim
#define INTRO_DELAY      20        // un respiro despues del fundido

static Sprite* introSpr;
static s16 introX;
static u8  introAcc, introTick, introFrame;
static u16 introWait;

static void intro71Begin(u16* target) {
    for (u16 i = 0; i < 16; i++) target[48 + i] = mousers_splinter.palette->data[i];
    introSpr = SPR_addSprite(&mousers_splinter, -INTRO_W, INTRO_FEET - 64,
                             TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    introX = -INTRO_W;
    introAcc = 0; introTick = 0; introFrame = 0;
    introWait = INTRO_DELAY;
    if (introSpr) {
        SPR_setAutoAnimation(introSpr, FALSE);
        SPR_setAnimAndFrame(introSpr, 0, 0);
        SPR_setDepth(introSpr, (s16)(-INTRO_FEET));
    }
}

static bool intro71Step(s16 camX) {
    (void)camX;
    if (!introSpr) {                             // sin VRAM: sin intro
        PAL_setPalette(PAL3, foot_soldier_white.palette->data, DMA_QUEUE);
        return FALSE;
    }
    if (introWait) { introWait--; return TRUE; }
    u16 t = (u16)(introAcc + INTRO_SPEED_Q);
    introAcc = (u8)(t & 0xFF);
    introX = (s16)(introX + (s16)(t >> 8));
    if (++introTick >= INTRO_ANIM_T) {
        introTick = 0;
        if (++introFrame >= 3) introFrame = 0;
        SPR_setFrame(introSpr, introFrame);
    }
    if (introX >= SCREEN_W) {                    // ya salio por la derecha
        SPR_releaseSprite(introSpr);
        introSpr = NULL;
        // (18/10) PAL3 pasa a ser la del blanco: Mousers, abejas, blancos
        // del cuchillo y columnas (Granitor la pisa al entrar).
        PAL_setPalette(PAL3, foot_soldier_white.palette->data, DMA_QUEUE);
        return FALSE;
    }
    SPR_setPosition(introSpr, introX, (s16)(INTRO_FEET - 64));
    return TRUE;
}

// ---------------------------------------------------------------------------
// SPLINTER ATADO (18/10) en la pared del fondo de la sala de Granitor
// ---------------------------------------------------------------------------
// Cuelga de una soga (que sale desde arriba de la pantalla) a la derecha de
// la puerta, como en el arcade. Se queda quieto 3-4 s y despues forcejea
// (frames 1-2-3-2) un par de ciclos antes de volver a quedarse quieto.
// tools/gen_factory_splinter.py parte el dibujo en dos:
//   - el cuerpo (5x10 tiles, filas 4-13) va en BG_A como OVERLAY de
//     stage_level (prioridad baja, PAL1 = la de las tortugas): como sprite,
//     en la pelea el limite de pixeles por linea (Granitor + tortugas) se
//     comia media figura. Animar = cargar los 50 tiles del frame.
//   - la soga en la franja del HUD (filas 0-3, que en BG_A no scrollean) va
//     como un sprite de 8x32, que se pide solo mientras esta en pantalla.
#define SPL_COL          242       // columna de tile de mundo (x 1936): soga en x 1971
#define SPL_ROW            4
#define SPL_TW             5
#define SPL_TH            10
#define SPL_ROPE_DX       32       // x del sprite de la soga dentro del dibujo
#define SPL_STILL_MIN    200       // frames quieto: 200..263 (3,3 a 4,4 s)
#define SPL_ANIM_T         5       // frames de juego por cuadro del forcejeo
#define SPL_CYCLES         2       // ciclos de forcejeo

static const u8 splStruggle[] = { 1, 2, 3, 2 };

static Sprite* splRope;
static u16 splWait;
static u8  splStep, splTick;       // splStep: 0 = quieto; 1.. = paso del forcejeo
static u8  splFrame;

static void splinterSetFrame(u8 f) {
    splFrame = f;
    stageOverlayFrame(splinter_tide_ts.tiles + (u32)f * (SPL_TW * SPL_TH) * 8);
    if (splRope) SPR_setFrame(splRope, f);
}

static void splinterInit71(void) {
    splRope = NULL;
    splWait = SPL_STILL_MIN;
    splStep = 0;
    splTick = 0;
    splinterSetFrame(0);
}

static void splinterUpdate71(Player** pls, u8 nPl, s16 camX) {
    (void)pls; (void)nPl;

    // Quieto -> forcejeo (SPL_CYCLES vueltas de splStruggle) -> quieto
    if (splStep == 0) {
        if (splWait) splWait--;
        else { splStep = 1; splTick = 0; splinterSetFrame(splStruggle[0]); }
    } else if (++splTick >= SPL_ANIM_T) {
        splTick = 0;
        if (++splStep > SPL_CYCLES * sizeof(splStruggle)) {
            splStep = 0;
            splWait = (u16)(SPL_STILL_MIN + (random() & 63));
            splinterSetFrame(0);
        } else {
            splinterSetFrame(splStruggle[(splStep - 1) & 3]);
        }
    }

    // La soga de arriba (sprite), solo en pantalla.
    s16 rx = (s16)(SPL_COL * 8 + SPL_ROPE_DX - camX);
    if (rx <= -8 || rx >= SCREEN_W) {
        if (splRope) { SPR_releaseSprite(splRope); splRope = NULL; }
        return;
    }
    if (!splRope) {
        splRope = SPR_addSprite(&splinter_rope, rx, 0, TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
        if (!splRope) return;                    // sin VRAM: se reintenta
        SPR_setAutoAnimation(splRope, FALSE);
        SPR_setDepth(splRope, SPR_MAX_DEPTH);
        SPR_setFrame(splRope, splFrame);
    }
    SPR_setPosition(splRope, rx, 0);
}

static void splinterRelease71(void) {
    if (splRope) { SPR_releaseSprite(splRope); splRope = NULL; }
}

// ---------------------------------------------------------------------------
// Jefe: Granitor y sus llamas
// ---------------------------------------------------------------------------
static Granitor granitor;

static void bossInit71(void) {
    granitorInit(&granitor);
}

static void bossStart71(s16 camX, s16 levelW) {
    (void)levelW;
    // (18/10) Cae del techo frente a la puerta del ascensor (camX + 24 +
    // GRAN_DROP_DX = la x 200 de pantalla).
    granitorSpawn(&granitor, (s16)(camX + 24), (s16)(camX + SCREEN_W - 24),
                  LVL71_TOP_MIN, 216, walkTopLow71);
}

// ---------------------------------------------------------------------------
// (18/10) Ganchos del nivel: Splinter + Mousers + lasers/capsula/abejas +
// columnas.
// ---------------------------------------------------------------------------
static void levelInit71(void) {
    splinterInit71();
    ffxInit();
    baxterRatInitAll();
    thInit(stageExtraVram(), pillars71, 2, 78);   // 78: sin camara vertical
}

static void levelUpdate71(Player** pls, u8 nPl, s16 camX) {
    splinterUpdate71(pls, nPl, camX);
    ffxLasersUpdate(pls, nPl, camX);
    ffxUpdate(pls, nPl, camX, 96, 208);
    baxterRatSetArena(camX, (s16)(camX + SCREEN_W), LVL71_TOP_MIN, 216);
    baxterRatUpdateAll(pls, nPl, camX, walkTopLow71);
    baxterRatPlayerHits(pls, nPl);
    thPillarsUpdate(pls, nPl, camX, 0);
}

static void levelRelease71(void) {
    splinterRelease71();
    ffxRelease();
    baxterRatReleaseAll();
    thRelease();
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
    // (18/10) Las columnas llegan hasta arriba: lo que cae en las filas del
    // HUD (que en BG_A no scrollean) va como sprites (fgTop), como en la cloaca.
    .fgTop        = &factory_fg_top,
    .fgTopX       = lvl71FgTopX,
    .fgTopN       = LVL71_FGTOP_N,
    .fgTopF       = lvl71FgTopF,
    .fgTopFrames  = LVL71_FGTOP_FRAMES,
    .ovCol        = SPL_COL,           // (18/10) Splinter atado (overlay en BG_A)
    .ovRow        = SPL_ROW,
    .ovW          = SPL_TW,
    .ovH          = SPL_TH,
    .extraTiles   = TH_VRAM_TILES,     // (18/10) columnas con laser (techno_hazards)
    .bgSlots      = 384,            // >= 358, el peor caso medido
    .levelW       = LVL71_W,
    .walkTop      = lvl71WalkTop,
    .walkCols     = LVL71_COLS,
    .walkYMin     = LVL71_TOP_MIN,
    .walkYMax     = 216,
    .ledgeTop     = PLAT_TOP,          // (18/10) la plataforma NFP-1
    .ledgeBot     = PLAT_BOT,
    .ledgeX0      = PLAT_X0,
    .ledgeX1      = PLAT_X1,
    .ledgeInset   = PLAT_INSET,
    .waves        = waves71,
    .nWaves       = sizeof(waves71) / sizeof(waves71[0]),
    .bossFeetX    = 1800,
    .music        = music_garage,     // (09/10) "Parking Garage (2-3) & The Factory (4)"; antes sonaba el del 2-1
    .musicVol     = 80,
    .bossMusic    = music_boss,
    .bossMusicVol = 80,
    .bossInit     = bossInit71,
    .bossStart    = bossStart71,
    .bossUpdate   = bossUpdate71,
    .bossDying    = bossDying71,
    .bossRelease  = bossRelease71,
    .introBegin   = intro71Begin,      // (18/10) los Mousers se llevan a Splinter
    .introStep    = intro71Step,
    .levelInit    = levelInit71,       // (18/10) Splinter, Mousers, capsula, lasers, columnas
    .levelUpdate  = levelUpdate71,
    .levelRelease = levelRelease71,
    .specialSpawn = special71,
    .specialAlive = specialAlive71,
    .nextScene    = SCENE_CUT_SPLINTER,   // (18/10) 5-1 -> Splinter -> 5-2 (dojo)
};

SceneId showScene71() {
    return stageLevelRun(&level71);
}
