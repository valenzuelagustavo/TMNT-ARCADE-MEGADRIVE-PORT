// ===========================================================================
// level8_1.c - Scene 8: el TECHNODROME (27/09)
// ===========================================================================
// Sexto nivel con arte del proyecto del companero (Ray Project): una L enorme
// de 4672x2816 px. Arriba un pasillo, despues un POZO en diagonal a 45 grados
// y abajo la sala donde espera el General Traag. El pozo se baja en ASCENSOR:
//
//   ARRIBA   camara solo en X (camY fija). Oleadas de foot soldiers con la
//            camara trabada, como siempre. Al final del pasillo esta el
//            ascensor: cuando TODAS las tortugas en juego se paran encima un
//            momento, arranca.
//   VIAJE    el ascensor baja en diagonal y la camara con el (4 px por frame
//            en X y en Y): en pantalla el ascensor queda quieto y el pozo
//            pasa. Las tortugas viajan encima, sin poder bajarse.
//   ABAJO    camY fija otra vez; una oleada al llegar y, al fondo de la sala,
//            Traag (traag.c).
//
// PLANOS
//   BG_B  el mapa (4672x2816, 2316 tiles unicos) streameado en 2D por
//         stage_bg.c (sbgInitRaw2D): ventana de 42x29 celdas que sigue a la
//         camara en los dos ejes sobre el plano circular de 64x32. El indice
//         0 del mapa es el vacio: se ve el color de fondo (negro).
//   BG_A  filas 0-3: el HUD. Filas 12-27: el ASCENSOR (techno_elevator.png,
//         320x128) dibujado en columnas de PANTALLA, con el scroll fino de
//         su X (se redibuja cuando cruza un tile). Su Y en pantalla nunca
//         cambia: 96 arriba, 96 durante el viaje (baja con la camara) y 96
//         abajo. El mapa original lo traia dibujado en su lugar de arranque;
//         tools/gen_level8_1.py lo borra (ver ahi).
//
// Musica: todavia no hay tema del Technodrome; suena el del 2-1.
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level8_1.h"          // pal_techno, techno_tiles/map, techno_elevator
#include "level1.h"            // hud_font
#include "enemies.h"           // foot_soldier (paleta)
#include "player.h"
#include "hud.h"
#include "enemy.h"
#include "stage_bg.h"
#include "traag.h"
#include "pause_menu.h"
#include "audio.h"

#ifndef IS_PAL_SYSTEM
#define IS_PAL_SYSTEM IS_PALSYSTEM
#endif

extern u8 cantidadJugadores;

#define SCREEN_W            320
#define HUD_ROWS              4
#define ROWS                 28

// --- Mapa ------------------------------------------------------------------
#define L8_MAP_W            584          // tiles
#define L8_MAP_H            352
#define L8_W               (L8_MAP_W * 8)    // 4672
#define L8_BG_SLOTS         512          // >= 487, el peor caso medido en el recorrido
#define L8_BACKDROP          11          // PAL0[11]: negro

// --- Arriba ------------------------------------------------------------------
#define L8_TOP_CAM_Y         32
#define L8_TOP_CAM_MAX_X   1348          // la camara no pasa de aca arriba
#define L8_TOP_LANE_T       128          // pies (mundo)
#define L8_TOP_LANE_B       248

// --- Ascensor ------------------------------------------------------------------
#define L8_ELEV_X0         1408          // esquina (mundo), donde lo dibujaba el mapa
#define L8_ELEV_Y0          128
#define L8_ELEV_W           320
#define L8_ELEV_H           128
#define L8_ELEV_ROW          12          // fila de pantalla (siempre la misma)
#define L8_PAD_L             40          // zona "parado encima" (pies) dentro del ascensor
#define L8_PAD_R            264
#define L8_PAD_T             48
#define L8_PAD_B            108
#define L8_ARM_FRAMES        36          // todos encima este tiempo -> arranca
#define L8_RIDE_SPEED         4

// --- Abajo -------------------------------------------------------------------
#define L8_BOT_CAM_Y       2592          // 2816 - 224
#define L8_BOT_CAM_MAX_X   (L8_W - SCREEN_W)   // 4352
#define L8_BOT_LANE_T      2696
#define L8_BOT_LANE_B      2808
#define L8_BOSS_FEET_X     4420

// --- Juego -------------------------------------------------------------------
#define CAM_DEAD_ZONE_RIGHT 120
#define CAM_MAX_SPEED_X       4
#define FOOT_DX  (PLAYER_SPRITE_W / 2)
#define L8_MAX_ALIVE_1P       3
#define L8_MAX_ALIVE_2P       2
#define L8_START_X           40
#define L8_CLEAR_SECS         5
#define VOL_MUSIC            90

typedef enum { PH_TOP, PH_RIDE, PH_BOTTOM } TechnoPhase;

// ---------------------------------------------------------------------------
// OLEADAS (solo soldiers morados: los naranjas tiran shurikens y esos no
// saben de camara vertical). Arriba se disparan por los pies del lider; la
// de abajo, al llegar el ascensor.
// ---------------------------------------------------------------------------
typedef struct { s16 trigX, lockX; u8 n; s8 side[4]; } TechnoWave;
static const TechnoWave topWaves[] = {
    {  200,    0, 2, { +1, -1 } },
    {  560,  380, 3, { +1, -1, +1 } },
    {  940,  760, 3, { -1, +1, +1 } },
    { 1300, 1120, 4, { +1, -1, +1, -1 } },
};
#define TOP_WAVES ((s16)(sizeof(topWaves) / sizeof(topWaves[0])))
static const TechnoWave botWave = { 0, 0, 3, { +1, +1, -1 } };

static const SbgRaw technoMap = {
    (const u32*) techno_tiles,
    (const u16*) techno_map,
    L8_MAP_W, L8_MAP_H,
    &pal_techno,
};

// ---------------------------------------------------------------------------
// Ascensor en BG_A (columnas de pantalla + scroll fino)
// ---------------------------------------------------------------------------
static u16 elevVram;
static s16 elevOc;                 // columna de pantalla de su borde (tiles), 0x7FFF = sin dibujar
static bool elevDrawn;
static u16 elevRow[41];

static void elevClear(void) {
    if (!elevDrawn) return;
    VDP_clearTileMapRect(BG_A, 0, L8_ELEV_ROW, 41, L8_ELEV_H / 8);
    elevDrawn = FALSE;
}

static void elevBlit(s16 oc) {
    const TileMap* tm = techno_elevator.tilemap;
    for (u16 r = 0; r < tm->h; r++) {
        for (s16 c = 0; c < 41; c++) {
            s16 lx = (s16)(c - oc);
            u16 v = 0;
            if (lx >= 0 && lx < (s16)tm->w) {
                u16 e = tm->tilemap[(u32)r * tm->w + (u16)lx];
                u16 idx = e & TILE_INDEX_MASK;
                if (idx)
                    v = (u16)((e & (TILE_ATTR_VFLIP_MASK | TILE_ATTR_HFLIP_MASK))
                              | TILE_ATTR(PAL0, FALSE, FALSE, FALSE) | (elevVram + idx));
            }
            elevRow[c] = v;
        }
        VDP_setTileMapDataRow(BG_A, elevRow, (u16)(L8_ELEV_ROW + r), 0, 41, CPU);
    }
    elevDrawn = TRUE;
}

// sx = X del ascensor en pantalla. Devuelve el scroll fino para BG_A.
static s16 elevUpdate(s16 sx) {
    if (sx <= -L8_ELEV_W || sx >= SCREEN_W) {
        elevClear();
        elevOc = 0x7FFF;
        return 0;
    }
    s16 oc = (s16)(sx >> 3);            // piso (sx puede ser negativo)
    if (oc != elevOc || !elevDrawn) {
        elevBlit(oc);
        elevOc = oc;
    }
    return (s16)(sx & 7);
}

// Scroll por fila de tile. static: van por DMA_QUEUE.
static s16 scrA[ROWS], scrB[ROWS];

static void applyScroll(s16 camX, s16 camY, s16 elevFine, TransferMethod tm) {
    for (u16 r = 0; r < ROWS; r++) {
        scrB[r] = (s16)-camX;
        scrA[r] = (r < HUD_ROWS) ? 0 : elevFine;
    }
    VDP_setHorizontalScrollTile(BG_A, 0, scrA, ROWS, tm);
    VDP_setHorizontalScrollTile(BG_B, 0, scrB, ROWS, tm);
    if (tm == DMA_QUEUE) VDP_setVerticalScrollVSync(BG_B, (s16)(camY & 255));
    else                 VDP_setVerticalScroll(BG_B, (s16)(camY & 255));
}

// ---------------------------------------------------------------------------
// Helpers de jugadores
// ---------------------------------------------------------------------------
static bool onPad(const Player* p, s16 ex, s16 ey) {
    s16 fx = (s16)(p->x + FOOT_DX);
    return fx >= ex + L8_PAD_L && fx <= ex + L8_PAD_R &&
           p->y >= ey + L8_PAD_T && p->y <= ey + L8_PAD_B;
}

static void clampToPad(Player* p, s16 ex, s16 ey) {
    setPlayerLane(p, (s16)(ey + L8_PAD_T), (s16)(ey + L8_PAD_B));
    setPlayerLeftBound(p, (s16)(ex + L8_PAD_L - FOOT_DX));
    setPlayerRightBound(p, (s16)(ex + L8_PAD_R - FOOT_DX));
}

static void drawPlayers(Player** pls, u8 nPl, s16 camX, s16 camY) {
    for (u8 k = 0; k < nPl; k++)
        if (pls[k]->sprite)
            SPR_setPosition(pls[k]->sprite, pls[k]->x - camX,
                            pls[k]->y - PLAYER_FOOT_OFFSET - playerDrawZ(pls[k]) - camY);
}

static void spawnWaveEnemy(Enemy* e, s8 side, s16 camX, s16 laneT, s16 laneB) {
    u8 type = ENEMY_TYPE_FOOT_SOLDIER;
    s16 x = (side < 0) ? (s16)(camX - ENEMY_SPRITE_W_PURPLE) : (s16)(camX + SCREEN_W);
    s16 span = (s16)(laneB - laneT - 16);
    s16 y = (s16)(laneT + 8 + ((span > 0) ? (s16)(random() % (u16)span) : 0));
    if (side < 0) initEnemySomersaultSpawn(e, x, y, 1, PAL2, type);
    else          initEnemyKickSpawn(e, x, y, -1, PAL2, type);
    setEnemyBounds(e, laneT, laneB, 0, 0, L8_W);
}

// ===========================================================================
// La escena
// ===========================================================================
SceneId showScene81() {
    clearScene();

    const u8  nPlSetup  = numJugadores();
    const u16 barBlocks = (u16)((nPlSetup > 2) ? MAX_PLAYERS : 2) * HUD_VRAM_PER_PLAYER;
    const u16 elevTiles = techno_elevator.tileset->numTile;

    // VRAM: HUD | ascensor | cache del mapa | sprites.
    SPR_initEx((u16)(TILE_FONT_INDEX - (TILE_USER_INDEX + barBlocks + elevTiles
                                        + L8_BG_SLOTS)));

    VDP_setScreenWidth320();
    VDP_setPlaneSize(64, 32, TRUE);
    VDP_setScrollingMode(HSCROLL_TILE, VSCROLL_PLANE);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setBackgroundColor(L8_BACKDROP);

    u8   nPl = nPlSetup;
    bool dosJugadores = (nPl >= 2);
    s16  cameraX = 0;
    s16  cameraY = L8_TOP_CAM_Y;
    s16  elevX = L8_ELEV_X0, elevY = L8_ELEV_Y0;
    TechnoPhase phase = PH_TOP;

    u16 hudVram = TILE_USER_INDEX;
    elevVram = (u16)(TILE_USER_INDEX + barBlocks);
    VDP_loadTileSet(techno_elevator.tileset, elevVram, DMA);
    elevDrawn = FALSE;
    elevOc = 0x7FFF;
    sbgInitRaw2D(&technoMap, BG_B, PAL0, (u16)(elevVram + elevTiles), L8_BG_SLOTS,
                 cameraX, cameraY);
    s16 elevFine = elevUpdate((s16)(elevX - cameraX));
    applyScroll(cameraX, cameraY, elevFine, DMA);

    hudSetPlane(BG_A);
    hudInit();

    Player p1, p2, p3, p4;
    Player* pls[MAX_PLAYERS] = { &p1, &p2, &p3, &p4 };
    static const u8 frac[MAX_PLAYERS] = { 40, 65, 20, 85 };
    for (u8 k = 0; k < nPl; k++) {
        s16 y = (s16)(L8_TOP_LANE_T + ((L8_TOP_LANE_B - L8_TOP_LANE_T) * frac[k]) / 100);
        initPlayer(pls[k], playerChar(k), playerJoy(k), PAL1, L8_START_X, y);
        setPlayerLane(pls[k], L8_TOP_LANE_T, L8_TOP_LANE_B);
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
    static Enemy enemies[MAX_ENEMIES];
    for (u16 i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].state  = ENEMY_STATE_INACTIVE;
        enemies[i].sprite = NULL;
    }
    u16 maxAlive = dosJugadores ? L8_MAX_ALIVE_2P : L8_MAX_ALIVE_1P;

    static Traag traag;
    traagInit(&traag);
    bool bossStarted = FALSE;

    const TechnoWave* wave = NULL;     // oleada en curso
    s16 topIdx      = -1;              // ultima oleada de arriba disparada
    bool botFired   = FALSE;
    u8  waveSpawned = 0;
    s16 camLockX    = -1;
    u8  arm         = 0;               // frames con todos encima del ascensor

    // Revelado: PAL0 el mapa, PAL1 las tortugas, PAL2 los soldiers, PAL3 en
    // negro (la carga Traag al entrar).
    u16 target[64];
    for (u16 i = 0; i < 16; i++) {
        target[i]      = pal_techno.data[i];
        target[16 + i] = leo_player.palette->data[i];
        target[32 + i] = foot_soldier.palette->data[i];
        target[48 + i] = 0;
    }
    for (u8 k = 0; k < nPl; k++) updatePlayer(pls[k]);
    drawPlayers(pls, nPl, cameraX, cameraY);
    SPR_update();

    XGM2_setLoopNumber(-1);
    playMusicVol(music_stage2_1, VOL_MUSIC);

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
        for (u8 k = 0; k < nPl; k++) updatePlayer(pls[k]);

        s16 leadX = p1.x;
        {
            bool any = FALSE;
            for (u8 k = 0; k < nPl; k++) {
                if (isPlayerGameOver(pls[k])) continue;
                if (!any || pls[k]->x > leadX) leadX = pls[k]->x;
                any = TRUE;
            }
        }
        s16 leadFeetX = (s16)(leadX + FOOT_DX);

        u16 alive = 0;
        for (u16 i = 0; i < MAX_ENEMIES; i++)
            if (enemies[i].state != ENEMY_STATE_INACTIVE) alive++;

        // --- Fases ------------------------------------------------------------
        if (phase == PH_TOP) {
            s16 leadScreenX = (s16)(leadX - cameraX);
            if (leadScreenX > CAM_DEAD_ZONE_RIGHT) {
                s16 nc = (s16)(cameraX + (leadScreenX - CAM_DEAD_ZONE_RIGHT));
                if (nc - cameraX > CAM_MAX_SPEED_X) nc = (s16)(cameraX + CAM_MAX_SPEED_X);
                if (camLockX >= 0 && nc > camLockX) nc = camLockX;
                if (nc > L8_TOP_CAM_MAX_X) nc = L8_TOP_CAM_MAX_X;
                if (nc > cameraX) cameraX = nc;
            }
            // Oleadas de arriba.
            if ((wave == NULL || waveSpawned >= wave->n) && topIdx + 1 < TOP_WAVES &&
                leadFeetX >= topWaves[topIdx + 1].trigX) {
                topIdx++;
                wave = &topWaves[topIdx];
                waveSpawned = 0;
                camLockX = wave->lockX;
                if (camLockX < cameraX) camLockX = cameraX;
            }
            bool wavesDone = (topIdx == TOP_WAVES - 1 && wave && waveSpawned >= wave->n &&
                              alive == 0);
            // El ascensor arranca con todos los que siguen en juego encima.
            bool all = wavesDone;
            u8 inGame = 0;
            for (u8 k = 0; k < nPl && all; k++) {
                if (isPlayerGameOver(pls[k])) continue;
                inGame++;
                if (!onPad(pls[k], elevX, elevY)) all = FALSE;
            }
            if (all && inGame) {
                if (++arm >= L8_ARM_FRAMES) {
                    phase = PH_RIDE;
                    for (u8 k = 0; k < nPl; k++) clampToPad(pls[k], elevX, elevY);
                }
            } else {
                arm = 0;
            }
            if (phase == PH_TOP) {
                for (u8 k = 0; k < nPl; k++) {
                    setPlayerLane(pls[k], L8_TOP_LANE_T, L8_TOP_LANE_B);
                    setPlayerLeftBound(pls[k], cameraX);
                    // A la derecha del pasillo solo esta el ascensor.
                    s16 rb = (s16)(cameraX + SCREEN_W - PLAYER_SPRITE_W);
                    s16 padR = (s16)(elevX + L8_PAD_R - FOOT_DX);
                    setPlayerRightBound(pls[k], (rb < padR) ? rb : padR);
                }
            }
        } else if (phase == PH_RIDE) {
            s16 s = L8_RIDE_SPEED;
            if (cameraY + s > L8_BOT_CAM_Y) s = (s16)(L8_BOT_CAM_Y - cameraY);
            elevX += s; elevY += s;
            cameraX += s; cameraY += s;
            for (u8 k = 0; k < nPl; k++) {
                pls[k]->x += s;
                pls[k]->y += s;
                clampToPad(pls[k], elevX, elevY);
            }
            if (cameraY >= L8_BOT_CAM_Y) {
                phase = PH_BOTTOM;
                wave = &botWave;
                botFired = TRUE;
                waveSpawned = 0;
                camLockX = cameraX;
            }
        } else {
            s16 leadScreenX = (s16)(leadX - cameraX);
            if (leadScreenX > CAM_DEAD_ZONE_RIGHT) {
                s16 nc = (s16)(cameraX + (leadScreenX - CAM_DEAD_ZONE_RIGHT));
                if (nc - cameraX > CAM_MAX_SPEED_X) nc = (s16)(cameraX + CAM_MAX_SPEED_X);
                if (camLockX >= 0 && nc > camLockX) nc = camLockX;
                if (nc > L8_BOT_CAM_MAX_X) nc = L8_BOT_CAM_MAX_X;
                if (nc > cameraX) cameraX = nc;
            }
            for (u8 k = 0; k < nPl; k++) {
                setPlayerLane(pls[k], L8_BOT_LANE_T, L8_BOT_LANE_B);
                // A la izquierda del ascensor esta el pozo.
                s16 lb = (s16)(elevX + L8_PAD_L - FOOT_DX);
                setPlayerLeftBound(pls[k], (cameraX > lb) ? cameraX : lb);
                setPlayerRightBound(pls[k], cameraX + SCREEN_W - PLAYER_SPRITE_W);
            }
        }
        for (u8 k = 0; k < nPl; k++) setPlayerCamera(pls[k], cameraX);

        // --- P2 que se suma en plena partida (no durante el viaje) ------------
        if (nPl == 1 && phase != PH_RIDE) {
            u8 ch2 = p2JoinPoll(hudPlayerCol(1));
            if (ch2 != 0xFF) {
                cantidadJugadores = 2;
                initPlayer(&p2, ch2, playerJoy(1), PAL1,
                           (s16)(getPlayerWorldX(&p1) - 48), getPlayerY(&p1));
                setPlayerLane(&p2, phase == PH_TOP ? L8_TOP_LANE_T : L8_BOT_LANE_T,
                              phase == PH_TOP ? L8_TOP_LANE_B : L8_BOT_LANE_B);
                setPlayerEndWall(&p2, 0, 0);
                setPlayerCamera(&p2, cameraX);
                setPlayerLeftBound(&p2, cameraX);
                setPlayerRightBound(&p2, cameraX + SCREEN_W - PLAYER_SPRITE_W);
                hudPlayerInit(&huds[1], &p2, hudPlayerCol(1),
                              (u16)(hudVram + HUD_VRAM_PER_PLAYER));
                nPl          = 2;
                dosJugadores = TRUE;
                maxAlive     = L8_MAX_ALIVE_2P;
                resetEnemyAI(2);
            }
        }
        for (u8 k = 0; k < nPl; k++) hudPlayerUpdate(&huds[k]);

        bool dying = bossStarted && traagIsDying(&traag);
        if (!dying && continueStepAll(conts, pls, huds, nPl, fps)) { allOut = TRUE; break; }

        // --- Hacer entrar a los de la oleada en curso ------------------------
        if (wave && phase != PH_RIDE) {
            s16 lt = (phase == PH_TOP) ? L8_TOP_LANE_T : L8_BOT_LANE_T;
            s16 lb = (phase == PH_TOP) ? L8_TOP_LANE_B : L8_BOT_LANE_B;
            while (waveSpawned < wave->n && alive < maxAlive) {
                u16 i;
                for (i = 0; i < MAX_ENEMIES; i++)
                    if (enemies[i].state == ENEMY_STATE_INACTIVE) break;
                if (i >= MAX_ENEMIES) break;
                spawnWaveEnemy(&enemies[i], wave->side[waveSpawned], cameraX, lt, lb);
                waveSpawned++;
                alive++;
            }
            if (waveSpawned >= wave->n && alive == 0) camLockX = -1;
        }

        // --- Foot soldiers -----------------------------------------------------
        separateEnemies(enemies, MAX_ENEMIES);
        for (u16 i = 0; i < MAX_ENEMIES; i++) {
            Enemy* e = &enemies[i];
            if (e->state == ENEMY_STATE_INACTIVE) continue;
            setEnemyCamera(e, cameraX);
            updateEnemyN(e, pls, nPl);
            if (e->state == ENEMY_STATE_INACTIVE) continue;
            if (e->sprite)
                SPR_setPosition(e->sprite, e->x - cameraX,
                                e->y - e->footOffset - e->jumpZ - cameraY);
        }
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

        // --- Jefe: Traag -------------------------------------------------------
        if (!bossStarted && phase == PH_BOTTOM && botFired && waveSpawned >= botWave.n &&
            alive == 0 && cameraX >= L8_BOT_CAM_MAX_X && leadFeetX >= L8_BOSS_FEET_X) {
            bossStarted = TRUE;
            XGM2_setLoopNumber(-1);
            playMusicVol(music_boss, VOL_MUSIC);
            traagSpawn(&traag, (s16)(cameraX + 24), (s16)(cameraX + SCREEN_W - 24),
                       L8_BOT_LANE_T, L8_BOT_LANE_B, NULL);
        }
        if (bossStarted) {
            traagUpdate(&traag, pls, nPl, cameraX, cameraY);
            traagMissileUpdate(pls, nPl, cameraX, cameraY);
            s8 killer = -1;
            if (traagPlayerHits(&traag, pls, nPl, &killer) && killer >= 0)
                addPlayerScore(pls[(u8)killer], 5);
            if (traagIsGone(&traag)) { win = TRUE; running = FALSE; }
        }

        // --- Fondo, ascensor y sprites -----------------------------------------
        sbgUpdate2D(cameraX, cameraY);
        elevFine = elevUpdate((s16)(elevX - cameraX));
        applyScroll(cameraX, cameraY, elevFine, DMA_QUEUE);
        drawPlayers(pls, nPl, cameraX, cameraY);

        SPR_update();
        SYS_doVBlankProcess();
    }

    if (win && !allOut) {
        for (u8 k = 0; k < nPl; k++) playerPersistSave(pls[k]);
        XGM2_stop();
        XGM2_setLoopNumber(0);
        playMusicVol(music_scene_clear, VOL_MUSIC);
        u16 hold = (IS_PAL_SYSTEM ? 50 : 60) * L8_CLEAR_SECS;
        while (hold > 0) { hold--; SYS_doVBlankProcess(); }
        XGM2_stop();
        XGM2_setLoopNumber(-1);
    }

    traagRelease(&traag);
    VDP_setTextPriority(0);
    VDP_setTextPalette(PAL0);
    VDP_setTextPlane(BG_A);
    clearScene();          // tambien vuelve el scroll a modo plano
    if (jump != PAUSE_NO_JUMP) return jump;
    if (win && !allOut) return SCENE_GAME_OVER;   // (todavia no hay Scene 9)
    return SCENE_GAME_OVER;
}
