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
// filas 192..213 del fondo = Y de mundo 160..181; el 29/09 se agrando un tile
// hacia abajo, hasta 189, a pedido de Gustavo) no se pisa: desde la vereda
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
#include "player.h"

#define SCREEN_W            320
#define LVL31_W            1248
#define LVL31_LEDGE_TOP     160     // ultima Y de pies de la vereda
#define LVL31_LEDGE_BOT     190     // primera Y de pies del canal (29/09: +1 tile)

static const SbgRaw sewer31 = {
    (const u32*) bg_sewer_tiles,
    (const u16*) bg_sewer_map,
    LVL31_W / 8, 32,        // (29/09) 32 filas: el fondo entero hasta la fila 255
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
// MISILES DEL AGUA (29/09, pedido de Gustavo)
// ---------------------------------------------------------------------------
// Si una tortuga se queda en el CANAL (pies >= LVL31_LEDGE_BOT) unos 4
// segundos, sale un misil del agua. Uno cada 4 segundos mientras siga en el
// agua; subirse a la vereda reinicia la cuenta.
//   - (29/09) EMERGE: aparece del lado de la pantalla MAS LEJANO a la
//     tortuga (SW_MIS_EDGE px adentro del borde), por su lane, saliendo del
//     agua desde abajo (SW_MIS_Z0) y sube hasta SW_MIS_Z. Recien ahi sale
//     derecho en X hacia la tortuga hasta dejar la camara. Mientras sube
//     no pega.
//   - Pega a las TORTUGAS (1 barra); a los foot soldiers no les hace nada.
//   - Saltando por encima (pies a mas de SW_MIS_CLEAR) se lo esquiva.
//   - Al pegar desaparece y en su lugar queda la explosion (7 frames, una
//     vez); al terminar se suelta el sprite.
// El arte (el de Traag, ver tools/gen_sewer_missile.py) va en PAL2: el misil
// mira a la DERECHA y la explosion impacta de derecha a izquierda, asi que
// el misil se espeja si va a la izquierda y la explosion si iba a la derecha.
// VRAM: 32 tiles por misil y 16 por explosion; a lo sumo uno de cada por
// tortuga (el misil cruza la pantalla en ~1,5 s y sale otro cada 4 s).
#define SW_WATER_SECS       4
#define SW_MIS_MAX          MAX_PLAYERS
#define SW_MIS_SPEED        4       // px/frame
#define SW_MIS_Z           28       // altura de vuelo sobre los pies (antes 18)
#define SW_MIS_Z0         (-10)     // arranca por debajo de la linea del agua
#define SW_MIS_RISE         2       // px/frame que sube al emerger
#define SW_MIS_EDGE        40       // px adentro del borde donde emerge
#define SW_MIS_HALF_X      14       // |dx| con el centro de la tortuga para pegar
#define SW_MIS_TOL_Y       12       // |dy| de lane para pegar
#define SW_MIS_CLEAR       40       // con los pies mas alto que esto, pasa por abajo
#define SW_MIS_DMG          1
#define SW_EXPL_FRAMES      7
#define SW_EXPL_TICKS       4       // frames de juego por frame de la explosion

// ---------------------------------------------------------------------------
// AGUA ANIMADA (29/09): como en el arcade, los dos colores del agua que no son
// el violeta se intercambian cada SW_WATER_CYCLE frames. El agua tiene sus
// propios indices en PAL0 (1 y 14, ver tools/gen_level3_1_bg.py): los grises
// originales (4 y 5) siguen siendo de los caños y las escaleras, que no se
// tocan. Son 2 palabras de CRAM por cambio, por la cola de DMA.
// ---------------------------------------------------------------------------
#define SW_WATER_A          1
#define SW_WATER_B         14
#define SW_WATER_CYCLE      8

static u16 swWaterTick;
static u8  swWaterPhase;

static void swWaterUpdate(void) {
    if (++swWaterTick < SW_WATER_CYCLE) return;
    swWaterTick = 0;
    swWaterPhase ^= 1;
    // Por la cola de DMA (en el VBlank): escribir la CRAM en pleno cuadro
    // deja puntitos en la imagen en un Mega Drive real. static: la cola lee
    // el buffer recien en el VBlank.
    static u16 ca, cb;
    const u16* c = pal_sewer.data;
    ca = swWaterPhase ? c[SW_WATER_B] : c[SW_WATER_A];
    cb = swWaterPhase ? c[SW_WATER_A] : c[SW_WATER_B];
    PAL_setColors(SW_WATER_A, &ca, 1, DMA_QUEUE);
    PAL_setColors(SW_WATER_B, &cb, 1, DMA_QUEUE);
}

static u16 swWater[MAX_PLAYERS];
// (29/09) Con el jefe no salen misiles nuevos (los que ya estan terminan):
// ademas de no meterse en la pelea, la VRAM de sprites no da para las dos
// cosas desde que la camara vertical agrando el cache del fondo.
static bool swBoss;
static struct { Sprite* spr; s16 x, lane, z; s8 dir; bool rising; } swMis[SW_MIS_MAX];
static struct { Sprite* spr; s16 x, lane; u8 frame, tick; } swBoom[SW_MIS_MAX];

static void swInit(void) {
    swBoss       = FALSE;
    swWaterTick  = 0;
    swWaterPhase = 0;
    for (u16 i = 0; i < MAX_PLAYERS; i++) swWater[i] = 0;
    for (u16 i = 0; i < SW_MIS_MAX; i++) { swMis[i].spr = NULL; swBoom[i].spr = NULL; }
}

static void swRelease(void) {
    for (u16 i = 0; i < SW_MIS_MAX; i++) {
        if (swMis[i].spr)  { SPR_releaseSprite(swMis[i].spr);  swMis[i].spr  = NULL; }
        if (swBoom[i].spr) { SPR_releaseSprite(swBoom[i].spr); swBoom[i].spr = NULL; }
    }
}

static void swFire(const Player* p, s16 camX) {
    for (u16 i = 0; i < SW_MIS_MAX; i++) {
        if (swMis[i].spr) continue;
        s16 px = (s16)(getPlayerWorldX(p) + PLAYER_SPRITE_W / 2);
        // Emerge del lado mas LEJANO a la tortuga y va hacia ella.
        s8  dir = (px - camX < SCREEN_W / 2) ? -1 : 1;
        s16 x   = (dir < 0) ? (s16)(camX + SCREEN_W - SW_MIS_EDGE)
                            : (s16)(camX + SW_MIS_EDGE);
        Sprite* s = SPR_addSprite(&sewer_missil, -64, -64, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        if (!s) return;                     // sin VRAM: este no sale
        SPR_setAnimationLoop(s, TRUE);
        SPR_setHFlip(s, dir < 0);           // el arte mira a la derecha
        swMis[i].spr  = s;
        swMis[i].x    = x;
        swMis[i].lane = getPlayerY(p);
        swMis[i].dir  = dir;
        swMis[i].z    = SW_MIS_Z0;
        swMis[i].rising = TRUE;
        SPR_setPosition(s, (s16)(x - camX - 32), (s16)(swMis[i].lane - SW_MIS_Z0 - 16 - stageCamY));
        return;
    }
}

static void swBoomAt(s16 x, s16 lane, s8 dir) {
    for (u16 i = 0; i < SW_MIS_MAX; i++) {
        if (swBoom[i].spr) continue;
        Sprite* s = SPR_addSprite(&sewer_explosao, -32, -32, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        if (!s) return;
        SPR_setAutoAnimation(s, FALSE);
        SPR_setAnimAndFrame(s, 0, 0);
        SPR_setHFlip(s, dir > 0);           // el arte impacta de derecha a izquierda
        swBoom[i].spr   = s;
        swBoom[i].x     = x;
        swBoom[i].lane  = lane;
        swBoom[i].frame = 0;
        swBoom[i].tick  = 0;
        XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                       SOUND_PCM_CH3, 13, FALSE, FALSE);
        return;
    }
}

static void swUpdate(Player** pls, u8 nPl, s16 camX) {
    const u16 period = (u16)((IS_PAL_SYSTEM ? 50 : 60) * SW_WATER_SECS);

    swWaterUpdate();

    // Cuenta del agua por tortuga.
    for (u8 k = 0; k < nPl; k++) {
        const Player* p = pls[k];
        if (isPlayerGameOver(p) || !p->sprite) { swWater[k] = 0; continue; }
        s16 y = getPlayerY(p);
        if (y >= LVL31_LEDGE_BOT && !swBoss) {
            if (++swWater[k] >= period) { swWater[k] = 0; swFire(p, camX); }
        } else if (y <= LVL31_LEDGE_TOP) {
            swWater[k] = 0;                 // arriba de la vereda: se corta
        }
    }

    // Misiles.
    for (u16 i = 0; i < SW_MIS_MAX; i++) {
        if (!swMis[i].spr) continue;
        if (swMis[i].rising) {                      // saliendo del agua
            swMis[i].z += SW_MIS_RISE;
            if (swMis[i].z >= SW_MIS_Z) { swMis[i].z = SW_MIS_Z; swMis[i].rising = FALSE; }
            SPR_setPosition(swMis[i].spr, (s16)(swMis[i].x - camX - 32),
                            (s16)(swMis[i].lane - swMis[i].z - 16 - stageCamY));
            SPR_setDepth(swMis[i].spr, (s16)(-(swMis[i].lane) - 1));
            continue;
        }
        swMis[i].x += swMis[i].dir * SW_MIS_SPEED;
        if (swMis[i].x < camX - 48 || swMis[i].x > camX + SCREEN_W + 48) {
            SPR_releaseSprite(swMis[i].spr);
            swMis[i].spr = NULL;
            continue;
        }
        bool hit = FALSE;
        for (u8 k = 0; k < nPl && !hit; k++) {
            Player* p = pls[k];
            if (!playerCanBeHitAir(p)) continue;
            if (getPlayerJumpZ(p) > SW_MIS_CLEAR) continue;         // lo salto
            s16 px = (s16)(getPlayerWorldX(p) + PLAYER_SPRITE_W / 2);
            s16 dx = (s16)(px - swMis[i].x), dy = (s16)(getPlayerY(p) - swMis[i].lane);
            if (dx < 0) dx = (s16)-dx;
            if (dy < 0) dy = (s16)-dy;
            if (dx >= SW_MIS_HALF_X || dy >= SW_MIS_TOL_Y) continue;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
            playerHitProjectile(p, swMis[i].x, SW_MIS_DMG);
            hit = TRUE;
        }
        if (hit) {
            swBoomAt(swMis[i].x, swMis[i].lane, swMis[i].dir);
            SPR_releaseSprite(swMis[i].spr);
            swMis[i].spr = NULL;
            continue;
        }
        SPR_setPosition(swMis[i].spr, (s16)(swMis[i].x - camX - 32),
                        (s16)(swMis[i].lane - swMis[i].z - 16 - stageCamY));
        SPR_setDepth(swMis[i].spr, (s16)(-(swMis[i].lane) - 1));
    }

    // Explosiones: una pasada de los 7 frames y se sueltan.
    for (u16 i = 0; i < SW_MIS_MAX; i++) {
        if (!swBoom[i].spr) continue;
        if (++swBoom[i].tick >= SW_EXPL_TICKS) {
            swBoom[i].tick = 0;
            if (++swBoom[i].frame >= SW_EXPL_FRAMES) {
                SPR_releaseSprite(swBoom[i].spr);
                swBoom[i].spr = NULL;
                continue;
            }
            SPR_setFrame(swBoom[i].spr, swBoom[i].frame);
        }
        SPR_setPosition(swBoom[i].spr, (s16)(swBoom[i].x - camX - 16),
                        (s16)(swBoom[i].lane - SW_MIS_Z - 16 - stageCamY));
        SPR_setDepth(swBoom[i].spr, (s16)(-(swBoom[i].lane) - 2));
    }
}

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
    swBoss = TRUE;
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
    .fgTopF       = lvl31FgTopF,    // frames compartidos (6 columnas distintas)
    .fgTopFrames  = LVL31_FGTOP_FRAMES,
    .camYMin      = -32,            // (29/09) la camara sube 32 px: se ve el techo
    .bgSlots      = 864,            // >= 861, el peor caso medido (fondo de 32 filas)
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
    .levelInit    = swInit,
    .levelUpdate  = swUpdate,
    .levelRelease = swRelease,
    .nextScene    = SCENE_4_1_TITLE,
};

SceneId showScene31() {
    return stageLevelRun(&level31);
}
