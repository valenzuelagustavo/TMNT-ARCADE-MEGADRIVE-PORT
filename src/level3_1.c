// ===========================================================================
// level3_1.c - Scene 3: la CLOACA (sewer) (26/09)
// ===========================================================================
// Primer nivel armado con material del proyecto del companero (Ray Project):
// el fondo bg_sewer.png (1248x224, 16 colores), el primer plano de caños
// bg_sewer_fg.png y su colision pintada (col_sewer.bin -> level3_1_limits.c,
// ver tools/gen_level3_1.py). Todo lo demas es nuestro motor: tortugas de 1 a
// 4, HUD, continues, pausa, especial, foot soldiers con su IA y oleadas con
// camara bloqueada como en el 1-1.
//
// FONDO: 2037 tiles unicos, no entra en VRAM. Lo streamea stage_bg.c con un
// cache de tiles por columna (el peor caso medido en 42 columnas son 689
// unicos; el cache tiene LVL31_BG_SLOTS). Va en BG_B con la paleta del fondo.
//
// PRIMER PLANO: los caños que bajan por delante (74 tiles, entran enteros).
// Van en BG_A con PRIORIDAD ALTA: las tortugas y los soldiers pasan POR
// DETRAS, como en el arcade. Para que el HUD (tambien en BG_A, filas 0-3) no
// se mueva, BG_A scrollea POR FILA DE TILES: filas 0-3 fijas, 4-27 con la
// camara (el mismo truco que el fuego del 1-1). Por eso los caños no se
// dibujan en las filas 0-3: ahi manda el HUD.
//
// COLOR 0 DEL FONDO: el rip usa el indice 0 como color de verdad (un marron
// de los ladrillos). En BG_B el 0 es transparente y deja ver el color de
// fondo del VDP, que se fija a PAL0[0]: se ve el mismo marron.
//
// CAMINABLE: una franja por columna, del tope de la tabla (vereda, 96 px) al
// piso del canal (LVL31_WALK_Y_MAX). Al final la vereda se corta en diagonal
// y solo queda el canal.
//
// JEFE: BAXTER STOCKMAN (baxter.c, portado del companero). Con las oleadas
// limpias y la camara en el fondo, entra volando por la izquierda con el tema
// de jefe; el nivel se gana cuando su nave explota. PAL3 es suya.
// Musica: todavia no hay tema del sewer; suena el del 2-1 (Downtown).
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level3_1.h"          // bg_sewer, bg_sewer_fg (rescomp)
#include "level3_1_limits.h"   // lvl31WalkTop (generado)
#include "level1.h"            // hud_font
#include "enemies.h"           // foot_soldier (paleta)
#include "player.h"
#include "hud.h"
#include "enemy.h"
#include "stage_bg.h"
#include "baxter.h"            // jefe (fase 4)
#include "pause_menu.h"
#include "audio.h"

#ifndef IS_PAL_SYSTEM
#define IS_PAL_SYSTEM IS_PALSYSTEM
#endif

extern u8 cantidadJugadores;

#define SCREEN_W            320
#define LVL31_W            1248
#define LVL31_CAM_MAX_X    (LVL31_W - SCREEN_W)     // 928
#define LVL31_BG_SLOTS      704     // >= 689, el peor caso medido
#define LVL31_HUD_ROWS        4     // filas de BG_A que no scrollean (HUD)
#define LVL31_ROWS           28

#define LVL31_WALK_Y_MIN     96     // tope de la vereda (pies)
#define LVL31_WALK_Y_MAX    216     // piso del canal (pies; 224 = borde de pantalla)

#define CAM_DEAD_ZONE_RIGHT 120
#define CAM_MAX_SPEED_X       4
#define FOOT_DX  (PLAYER_SPRITE_W / 2)

#define LVL31_MAX_ALIVE_1P    3     // mismo presupuesto que el 2-1 (ver SPR_initEx)
#define LVL31_MAX_ALIVE_2P    2

#define LVL31_START_X        40
#define LVL31_CLEAR_SECS      5
#define LVL31_BOSS_FEET_X  1000     // pies del lider para que entre Baxter
#define VOL_MUSIC_BOSS       90
#define VOL_MUSIC_SEWER      90
#define VOL_MUSIC_CLEAR      90

// ---------------------------------------------------------------------------
// OLEADAS: se disparan cuando los PIES del que va adelante pasan trigX; la
// camara queda clavada en lockX hasta que caen todos los de la oleada. Los
// enemigos entran por los bordes de la camara: por la izquierda con la
// voltereta, por la derecha con la patada (los mismos spawns del 1-1).
// ---------------------------------------------------------------------------
#define WAVE_MAX 4
typedef struct {
    s16 trigX;
    s16 lockX;
    u8  n;
    u8  type[WAVE_MAX];    // ENEMY_TYPE_*
    s8  side[WAVE_MAX];    // -1 entra por la izquierda, +1 por la derecha
} Lvl31Wave;

#define P ENEMY_TYPE_FOOT_SOLDIER
#define O ENEMY_TYPE_FOOT_SOLDIER_ORANGE
static const Lvl31Wave waves[] = {
    {  150,   0, 2, { P, P },       { +1, +1 } },
    {  420, 240, 3, { P, O, P },    { -1, +1, +1 } },
    {  700, 500, 4, { O, P, P, O }, { -1, +1, -1, +1 } },
    {  980, 780, 4, { P, P, O, P }, { +1, +1, -1, -1 } },
};
#undef P
#undef O
#define WAVE_COUNT ((u16)(sizeof(waves) / sizeof(waves[0])))

// ---------------------------------------------------------------------------
// Caminable
// ---------------------------------------------------------------------------
static s16 walkTopAt(s16 fx) {
    s16 c = (s16)(fx >> 3);
    if (c < 0) c = 0;
    if (c >= LVL31_COLS) c = LVL31_COLS - 1;
    return (s16)lvl31WalkTop[c];
}

// Revierte lo justo (primero la Y, despues la X) si el paso dejo los pies por
// encima del tope de su columna. Nunca teletransporta.
static void clampWalk(s16* x, s16* y, s16 px, s16 py, s16 footDx) {
    if (*y >= walkTopAt(*x + footDx)) return;
    if (py >= walkTopAt(*x + footDx)) { *y = py; return; }
    if (*y >= walkTopAt(px + footDx)) { *x = px; return; }
    *x = px;
    *y = py;
    // Red de seguridad (nacio adentro de la pared): se lo baja al tope.
    s16 t = walkTopAt(*x + footDx);
    if (*y < t) *y = t;
}

// ---------------------------------------------------------------------------
// Primer plano (caños) en BG_A
// ---------------------------------------------------------------------------
static u16 fgVram;
static s16 fgColRight, fgColLeft;
static u16 fgBuf[LVL31_ROWS];

static void fgDrawCol(s16 col) {
    const TileMap* tm = bg_sewer_fg.tilemap;
    for (u16 r = LVL31_HUD_ROWS; r < LVL31_ROWS; r++) {
        u16 v = 0;
        if (col >= 0 && col < (s16)tm->w && r < tm->h) {
            u16 e   = tm->tilemap[(u32)r * tm->w + (u16)col];
            u16 idx = e & TILE_INDEX_MASK;
            if (idx)   // tile 0 = vacio (rescomp lo deja primero)
                v = (u16)((e & (TILE_ATTR_VFLIP_MASK | TILE_ATTR_HFLIP_MASK))
                          | TILE_ATTR(PAL0, TRUE, FALSE, FALSE) | (fgVram + idx));
        }
        fgBuf[r - LVL31_HUD_ROWS] = v;
    }
    VDP_setTileMapDataRect(BG_A, fgBuf, (u16)(col & 63), LVL31_HUD_ROWS,
                           1, LVL31_ROWS - LVL31_HUD_ROWS, 1, CPU);
}

static void fgUpdate(s16 camX) {
    s16 wantL = (s16)(camX >> 3);
    s16 wantR = (s16)(wantL + SBG_WIN_COLS - 1);
    while (fgColRight < wantR) fgDrawCol(++fgColRight);
    while (fgColLeft > wantL)  fgDrawCol(--fgColLeft);
    if (fgColLeft < wantL) fgColLeft = wantL;
    if (fgColRight > wantR) fgColRight = wantR;
}

// Scroll por fila de tile: BG_B entero con la camara; BG_A fijo en las filas
// del HUD y con la camara en el resto. Tienen que ser static: DMA_QUEUE.
static s16 scrA[LVL31_ROWS], scrB[LVL31_ROWS];

static void applyScroll(s16 camX, TransferMethod tm) {
    for (u16 r = 0; r < LVL31_ROWS; r++) {
        scrB[r] = (s16)-camX;
        scrA[r] = (r < LVL31_HUD_ROWS) ? 0 : (s16)-camX;
    }
    VDP_setHorizontalScrollTile(BG_A, 0, scrA, LVL31_ROWS, tm);
    VDP_setHorizontalScrollTile(BG_B, 0, scrB, LVL31_ROWS, tm);
}

// ---------------------------------------------------------------------------
// Spawn de un soldier de oleada
// ---------------------------------------------------------------------------
static void spawnWaveEnemy(Enemy* e, u8 type, s8 side, s16 camX) {
    s16 w  = (type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) ? ENEMY_SPRITE_W_ORANGE
                                                      : ENEMY_SPRITE_W_PURPLE;
    s16 x  = (side < 0) ? (s16)(camX - w) : (s16)(camX + SCREEN_W);
    s16 fx = (s16)(((side < 0) ? camX + 24 : camX + SCREEN_W - 24));
    s16 top = walkTopAt(fx);
    s16 span = (s16)(LVL31_WALK_Y_MAX - 8 - (top + 8));
    s16 y = (s16)(top + 8 + ((span > 0) ? (s16)(random() % (u16)span) : 0));
    if (side < 0) initEnemySomersaultSpawn(e, x, y, 1, PAL2, type);
    else          initEnemyKickSpawn(e, x, y, -1, PAL2, type);
    setEnemyBounds(e, LVL31_WALK_Y_MIN, LVL31_WALK_Y_MAX, 0, 0, LVL31_W);
}

// ===========================================================================
// La escena
// ===========================================================================
SceneId showScene31() {
    clearScene();

    const u8  nPlSetup = numJugadores();
    const u16 barBlocks = (u16)((nPlSetup > 2) ? MAX_PLAYERS : 2) * HUD_VRAM_PER_PLAYER;
    const u16 fgTiles   = bg_sewer_fg.tileset->numTile;

    // VRAM: HUD | caños | cache del fondo | sprites. Con 2 jugadores:
    // 1440 - 16 - 20 - 74 - 704 = 626 tiles de sprites (el 2-1 tiene 636).
    SPR_initEx((u16)(TILE_FONT_INDEX - (TILE_USER_INDEX + barBlocks + fgTiles
                                        + LVL31_BG_SLOTS)));

    VDP_setScreenWidth320();
    VDP_setPlaneSize(64, 32, TRUE);
    VDP_setScrollingMode(HSCROLL_TILE, VSCROLL_PLANE);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setBackgroundColor(0);      // PAL0[0]: el marron que el rip usa como color

    u8   nPl = nPlSetup;
    bool dosJugadores = (nPl >= 2);
    s16  cameraX = 0;

    u16 hudVram = TILE_USER_INDEX;
    fgVram = (u16)(TILE_USER_INDEX + barBlocks);
    VDP_loadTileSet(bg_sewer_fg.tileset, fgVram, DMA);
    sbgInit(&bg_sewer, BG_B, PAL0, (u16)(fgVram + fgTiles), LVL31_BG_SLOTS, cameraX);
    fgColLeft  = 0;
    fgColRight = -1;
    fgUpdate(cameraX);
    applyScroll(cameraX, DMA);

    hudInit();

    Player p1, p2, p3, p4;
    Player* pls[MAX_PLAYERS] = { &p1, &p2, &p3, &p4 };
    static const s16 startY[MAX_PLAYERS] = { 150, 176, 128, 198 };
    for (u8 k = 0; k < nPl; k++) {
        initPlayer(pls[k], playerChar(k), playerJoy(k), PAL1,
                   LVL31_START_X, startY[k]);
        setPlayerLane(pls[k], LVL31_WALK_Y_MIN, LVL31_WALK_Y_MAX);
        setPlayerEndWall(pls[k], 0, 0);
        setPlayerLeftBound(pls[k], cameraX);
        setPlayerRightBound(pls[k], cameraX + SCREEN_W - PLAYER_SPRITE_W);
        setPlayerCamera(pls[k], cameraX);
    }

    VDP_loadFont(&hud_font, DMA);
    VDP_setTextPlane(BG_A);
    VDP_setTextPriority(1);
    VDP_setTextPalette(PAL1);

    p2JoinReset();
    static HudPlayer huds[MAX_PLAYERS];
    static ContPlayer conts[MAX_PLAYERS];
    contResetAll(conts);
    const u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    for (u8 k = 0; k < nPl; k++)
        hudPlayerInit(&huds[k], pls[k], hudPlayerCol(k),
                      (u16)(hudVram + k * HUD_VRAM_PER_PLAYER));

    resetEnemyAI(cantidadJugadores);
    shurikenInit();
    static Enemy enemies[MAX_ENEMIES];
    for (u16 i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].state  = ENEMY_STATE_INACTIVE;
        enemies[i].sprite = NULL;
    }
    u16 maxAlive = dosJugadores ? LVL31_MAX_ALIVE_2P : LVL31_MAX_ALIVE_1P;

    static Baxter baxter;
    baxterInit(&baxter);
    baxterRatInitAll();
    bool bossStarted = FALSE;

    // Oleadas: la que esta en curso, cuantos de ella ya entraron, y el tope
    // de camara mientras dure (-1 = libre).
    s16 waveIdx     = -1;       // ultima oleada disparada
    u8  waveSpawned = 0;
    s16 camLockX    = -1;

    // Revelado: PAL0 el fondo, PAL1 las tortugas, PAL2 los foot soldiers,
    // PAL3 libre (sera la de Baxter).
    u16 target[64];
    for (u16 i = 0; i < 16; i++) {
        target[i]      = bg_sewer.palette->data[i];
        target[16 + i] = leo_player.palette->data[i];
        target[32 + i] = foot_soldier.palette->data[i];
        target[48 + i] = 0;
    }
    for (u8 k = 0; k < nPl; k++) updatePlayer(pls[k]);   // coloca los sprites
    SPR_update();

    XGM2_setLoopNumber(-1);
    playMusicVol(music_stage2_1, VOL_MUSIC_SEWER);

    static const u16 black[64] = { 0 };
    PAL_setColors(0, black, 64, DMA);
    PAL_fadeInAll(target, 20, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    bool running = TRUE;
    bool allOut  = FALSE;
    bool win     = FALSE;
    SceneId jump = PAUSE_NO_JUMP;
    pauseReset();
    while (running) {
        jump = pausePoll(pls, nPl);
        if (jump != PAUSE_NO_JUMP) break;

        // --- Jugadores -----------------------------------------------------
        for (u8 k = 0; k < nPl; k++) {
            s16 prevX = pls[k]->x, prevY = pls[k]->y;
            updatePlayer(pls[k]);
            clampWalk(&pls[k]->x, &pls[k]->y, prevX, prevY, FOOT_DX);
        }

        // --- Camara: dead-zone a la derecha, nunca vuelve -------------------
        s16 leadX = p1.x;
        {
            bool any = FALSE;
            for (u8 k = 0; k < nPl; k++) {
                if (isPlayerGameOver(pls[k])) continue;
                if (!any || pls[k]->x > leadX) leadX = pls[k]->x;
                any = TRUE;
            }
        }
        s16 leadScreenX = leadX - cameraX;
        if (leadScreenX > CAM_DEAD_ZONE_RIGHT) {
            s16 nc = cameraX + (leadScreenX - CAM_DEAD_ZONE_RIGHT);
            if (nc - cameraX > CAM_MAX_SPEED_X) nc = cameraX + CAM_MAX_SPEED_X;
            if (camLockX >= 0 && nc > camLockX) nc = camLockX;
            if (nc > LVL31_CAM_MAX_X) nc = LVL31_CAM_MAX_X;
            if (nc > cameraX) cameraX = nc;
        }
        for (u8 k = 0; k < nPl; k++) {
            setPlayerCamera(pls[k], cameraX);
            setPlayerLeftBound(pls[k], cameraX);
            setPlayerRightBound(pls[k], cameraX + SCREEN_W - PLAYER_SPRITE_W);
            // updatePlayer ya lo dibujo, pero antes del recorte de la pared y
            // del movimiento de camara de este frame: se lo reubica.
            if (pls[k]->sprite)
                SPR_setPosition(pls[k]->sprite, pls[k]->x - cameraX,
                                pls[k]->y - PLAYER_FOOT_OFFSET - playerDrawZ(pls[k]));
        }

        // --- P2 que se suma en plena partida --------------------------------
        if (nPl == 1) {
            u8 ch2 = p2JoinPoll(hudPlayerCol(1));
            if (ch2 != 0xFF) {
                cantidadJugadores = 2;
                initPlayer(&p2, ch2, playerJoy(1), PAL1,
                           (s16)(getPlayerWorldX(&p1) - 48), getPlayerY(&p1));
                setPlayerLane(&p2, LVL31_WALK_Y_MIN, LVL31_WALK_Y_MAX);
                setPlayerEndWall(&p2, 0, 0);
                setPlayerCamera(&p2, cameraX);
                setPlayerLeftBound(&p2, cameraX);
                setPlayerRightBound(&p2, cameraX + SCREEN_W - PLAYER_SPRITE_W);
                hudPlayerInit(&huds[1], &p2, hudPlayerCol(1),
                              (u16)(hudVram + HUD_VRAM_PER_PLAYER));
                nPl          = 2;
                dosJugadores = TRUE;
                maxAlive     = LVL31_MAX_ALIVE_2P;
                resetEnemyAI(2);
            }
        }
        for (u8 k = 0; k < nPl; k++) hudPlayerUpdate(&huds[k]);

        // Continues (con la nave de Baxter ya explotando no se evaluan).
        if (baxter.state != BAXTER_DEAD && baxter.state != BAXTER_GONE &&
            continueStepAll(conts, pls, huds, nPl, fps)) { allOut = TRUE; break; }

        // --- Oleadas --------------------------------------------------------
        u16 alive = 0;
        for (u16 i = 0; i < MAX_ENEMIES; i++)
            if (enemies[i].state != ENEMY_STATE_INACTIVE) alive++;

        s16 leadFeetX = leadX + FOOT_DX;
        // Disparar la siguiente, si la anterior ya termino de entrar.
        if ((waveIdx < 0 || waveSpawned >= waves[waveIdx].n) &&
            waveIdx + 1 < (s16)WAVE_COUNT &&
            leadFeetX >= waves[waveIdx + 1].trigX) {
            waveIdx++;
            waveSpawned = 0;
            camLockX = waves[waveIdx].lockX;
            if (camLockX < cameraX) camLockX = cameraX;
        }
        // Hacer entrar a los que falten, respetando el tope de VRAM.
        if (waveIdx >= 0) {
            const Lvl31Wave* w = &waves[waveIdx];
            while (waveSpawned < w->n && alive < maxAlive) {
                u16 i;
                for (i = 0; i < MAX_ENEMIES; i++)
                    if (enemies[i].state == ENEMY_STATE_INACTIVE) break;
                if (i >= MAX_ENEMIES) break;
                spawnWaveEnemy(&enemies[i], w->type[waveSpawned],
                               w->side[waveSpawned], cameraX);
                waveSpawned++;
                alive++;
            }
            // Oleada limpia: la camara se libera.
            if (waveSpawned >= w->n && alive == 0) camLockX = -1;
        }

        // --- Foot soldiers ---------------------------------------------------
        separateEnemies(enemies, MAX_ENEMIES);
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            Enemy* e = &enemies[i];
            if (e->state == ENEMY_STATE_INACTIVE) continue;
            s16 prevEX = e->x, prevEY = e->y;
            setEnemyCamera(e, cameraX);
            updateEnemyN(e, pls, nPl);
            if (e->state == ENEMY_STATE_INACTIVE) continue;
            clampWalk(&e->x, &e->y, prevEX, prevEY, (s16)(getEnemyCenterX(e) - e->x));
            if (e->sprite)
                SPR_setPosition(e->sprite, e->x - cameraX,
                                e->y - e->footOffset - e->jumpZ);
        }

        // Golpe del jugador al soldier.
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            if (!enemyCanBeHit(&enemies[i])) continue;
            s16 ex = getEnemyCenterX(&enemies[i]);
            s16 ey = getEnemyCenterY(&enemies[i]);
            s16 hw = enemyBodyHalfW(&enemies[i]);
            s16 bh = enemyBodyH(&enemies[i]);
            Player* att = NULL;
            s16 dmg = 0;
            for (u8 k = 0; k < nPl; k++) {
                if (!playerAttackHitsBox(pls[k], ex, ey, hw, bh)) continue;
                dmg = isPlayerSpecialAttack(pls[k]) ? ENEMY_HP : 1; att = pls[k];
                break;
            }
            if (dmg > 0) {
                damageEnemy(&enemies[i], dmg);
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                               SOUND_PCM_CH2, 15, FALSE, FALSE);
                if (att && enemies[i].state == ENEMY_STATE_DEAD) {
                    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                                   SOUND_PCM_CH3, 15, FALSE, FALSE);
                    addPlayerScore(att, 1);
                }
            }
        }

        // Golpe del soldier al jugador.
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            Enemy* e = &enemies[i];
            if (e->state != ENEMY_STATE_ATTACK) continue;
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                if (!enemyTryHitPlayerBox(e, getPlayerWorldX(pls[k]), getPlayerY(pls[k]),
                                          PLAYER_BODY_HALF_W)) continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                               SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(pls[k], getEnemyCenterX(e));
                break;
            }
        }

        // Shurikens del naranja.
        shurikenUpdate(cameraX);
        for (u8 k = 0; k < nPl; k++)
            if (shurikenBreakByPlayerAttack(pls[k]))
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                               SOUND_PCM_CH2, 15, FALSE, FALSE);
        {
            s16 hitX = 0;
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                if (!shurikenCheckHitPlayer(getPlayerWorldX(pls[k]), getPlayerY(pls[k]), &hitX))
                    continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                               SOUND_PCM_CH2, 15, FALSE, FALSE);
                damagePlayer(pls[k], hitX);
            }
        }

        // --- Jefe: Baxter ---------------------------------------------------
        // Entra con todas las oleadas adentro y muertas, la camara en el
        // fondo y el lider cerca de la salida. La camara ya no se mueve.
        if (!bossStarted && waveIdx == (s16)WAVE_COUNT - 1 &&
            waveSpawned >= waves[waveIdx].n && alive == 0 &&
            cameraX >= LVL31_CAM_MAX_X && leadFeetX >= LVL31_BOSS_FEET_X) {
            bossStarted = TRUE;
            XGM2_setLoopNumber(-1);
            playMusicVol(music_boss, VOL_MUSIC_BOSS);
            baxterSpawn(&baxter, (s16)(cameraX + 16), (s16)(cameraX + SCREEN_W - 16),
                        LVL31_WALK_Y_MIN, LVL31_WALK_Y_MAX);
        }
        if (bossStarted) {
            baxterUpdate(&baxter, pls, nPl, cameraX);
            baxterRatUpdateAll(pls, nPl, cameraX, walkTopAt);
            s8 killer = -1;
            if (baxterPlayerHits(&baxter, pls, nPl, &killer) && killer >= 0)
                addPlayerScore(pls[(u8)killer], 5);
            baxterRatPlayerHits(pls, nPl);
            if (baxterIsGone(&baxter) && baxterRatAliveCount() == 0) {
                win = TRUE;
                running = FALSE;
            }
        }

        // --- Fondo -----------------------------------------------------------
        sbgUpdate(cameraX);
        fgUpdate(cameraX);
        applyScroll(cameraX, DMA_QUEUE);

        SPR_update();
        SYS_doVBlankProcess();
    }

    if (win && !allOut) {
        for (u8 k = 0; k < nPl; k++) playerPersistSave(pls[k]);
        XGM2_stop();
        XGM2_setLoopNumber(0);
        playMusicVol(music_scene_clear, VOL_MUSIC_CLEAR);
        u16 hold = (IS_PAL_SYSTEM ? 50 : 60) * LVL31_CLEAR_SECS;
        while (hold > 0) { hold--; SYS_doVBlankProcess(); }
        XGM2_stop();
        XGM2_setLoopNumber(-1);
    }

    shurikenReleaseAll();
    baxterRelease(&baxter);
    VDP_setTextPriority(0);
    VDP_setTextPalette(PAL0);
    clearScene();          // tambien vuelve el scroll a modo plano
    if (jump != PAUSE_NO_JUMP) return jump;
    return SCENE_GAME_OVER;   // (todavia no hay Scene 4)
}
