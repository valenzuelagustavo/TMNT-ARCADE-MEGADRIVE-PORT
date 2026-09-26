// ===========================================================================
// stage_level.c — nivel generico de scroll horizontal. Ver stage_level.h.
// ===========================================================================
// Nacio como src/level3_1.c (la cloaca) y se partio en dos el 26/09 al llegar
// la Scene 4: aca queda el motor; en cada levelN_M.c, los datos y el jefe.
// ===========================================================================

#include <genesis.h>
#include "stage_level.h"
#include "level1.h"            // hud_font
#include "enemies.h"           // foot_soldier (paleta)
#include "hud.h"
#include "enemy.h"
#include "stage_bg.h"
#include "pause_menu.h"
#include "audio.h"

#ifndef IS_PAL_SYSTEM
#define IS_PAL_SYSTEM IS_PALSYSTEM
#endif

extern u8 cantidadJugadores;

#define SCREEN_W            320
#define STAGE_HUD_ROWS        4     // filas de BG_A que no scrollean (HUD)
#define STAGE_ROWS           28
#define CAM_DEAD_ZONE_RIGHT 120
#define CAM_MAX_SPEED_X       4
#define FOOT_DX  (PLAYER_SPRITE_W / 2)
#define STAGE_MAX_ALIVE_1P    3     // presupuesto de VRAM de sprites (como el 2-1)
#define STAGE_MAX_ALIVE_2P    2
#define STAGE_START_X        40
#define STAGE_CLEAR_SECS      5
#define VOL_MUSIC_CLEAR      90

static const StageLevel* cur;       // el nivel en curso

// ---------------------------------------------------------------------------
// Caminable
// ---------------------------------------------------------------------------
s16 stageWalkTopAt(s16 fx) {
    if (!cur || !cur->walkTop) return cur ? cur->walkYMin : 0;
    s16 c = (s16)(fx >> 3);
    if (c < 0) c = 0;
    if (c >= (s16)cur->walkCols) c = (s16)(cur->walkCols - 1);
    return (s16)cur->walkTop[c];
}

// Revierte lo justo (primero la Y, despues la X) si el paso dejo los pies por
// encima del tope de su columna. Nunca teletransporta.
static void clampWalk(s16* x, s16* y, s16 px, s16 py, s16 footDx) {
    if (*y >= stageWalkTopAt(*x + footDx)) return;
    if (py >= stageWalkTopAt(*x + footDx)) { *y = py; return; }
    if (*y >= stageWalkTopAt(px + footDx)) { *x = px; return; }
    *x = px;
    *y = py;
    s16 t = stageWalkTopAt(*x + footDx);   // red de seguridad
    if (*y < t) *y = t;
}

// ---------------------------------------------------------------------------
// Primer plano (BG_A, prioridad alta, sin las filas del HUD)
// ---------------------------------------------------------------------------
static u16 fgVram;
static s16 fgColRight, fgColLeft;
static u16 fgBuf[32];

static void fgDrawCol(s16 col) {
    const TileMap* tm = cur->fg->tilemap;
    for (u16 r = STAGE_HUD_ROWS; r < STAGE_ROWS; r++) {
        u16 v = 0;
        if (col >= 0 && col < (s16)tm->w && r < tm->h) {
            u16 e   = tm->tilemap[(u32)r * tm->w + (u16)col];
            u16 idx = e & TILE_INDEX_MASK;
            if (idx)   // tile 0 = vacio (rescomp lo deja primero)
                v = (u16)((e & (TILE_ATTR_VFLIP_MASK | TILE_ATTR_HFLIP_MASK))
                          | TILE_ATTR(PAL0, TRUE, FALSE, FALSE) | (fgVram + idx));
        }
        fgBuf[r - STAGE_HUD_ROWS] = v;
    }
    VDP_setTileMapDataRect(BG_A, fgBuf, (u16)(col & 63), STAGE_HUD_ROWS,
                           1, STAGE_ROWS - STAGE_HUD_ROWS, 1, CPU);
}

static void fgUpdate(s16 camX) {
    if (!cur->fg) return;
    s16 wantL = (s16)(camX >> 3);
    s16 wantR = (s16)(wantL + SBG_WIN_COLS - 1);
    while (fgColRight < wantR) fgDrawCol(++fgColRight);
    while (fgColLeft > wantL)  fgDrawCol(--fgColLeft);
    if (fgColLeft < wantL) fgColLeft = wantL;
    if (fgColRight > wantR) fgColRight = wantR;
}

// Capa lejana (BG_B, PAL3, baja prioridad): entera en VRAM, filas 4-27 del
// plano (0-3 son del HUD), envuelve cada farW columnas.
static void farDraw(u16 vram) {
    const TileMap* tm = cur->far->tilemap;
    for (u16 r = STAGE_HUD_ROWS; r < STAGE_ROWS; r++) {
        u16 sr = (u16)(r + cur->farRowShift);
        for (u16 c = 0; c < 64; c++) {
            u16 v = 0;
            if (sr < tm->h) {
                u16 e = tm->tilemap[(u32)sr * tm->w + (c % tm->w)];
                v = (u16)((e & (TILE_ATTR_VFLIP_MASK | TILE_ATTR_HFLIP_MASK))
                          | TILE_ATTR(PAL3, FALSE, FALSE, FALSE)
                          | (vram + (e & TILE_INDEX_MASK)));
            }
            fgBuf[c & 31] = v;
            if ((c & 31) == 31)
                VDP_setTileMapDataRect(BG_B, fgBuf, (u16)(c - 31), r, 32, 1, 32, CPU);
        }
    }
}

// Scroll por fila de tile. static: van por DMA_QUEUE.
static s16 scrA[STAGE_ROWS], scrB[STAGE_ROWS];

static void applyScroll(s16 camX, TransferMethod tm) {
    s16 farX = cur->far ? (s16)-(camX / (s16)(cur->farDiv ? cur->farDiv : 1)) : 0;
    for (u16 r = 0; r < STAGE_ROWS; r++) {
        if (cur->far) {
            scrA[r] = (s16)-camX;
            scrB[r] = (r < STAGE_HUD_ROWS) ? 0 : farX;
        } else {
            scrB[r] = (s16)-camX;
            scrA[r] = (r < STAGE_HUD_ROWS) ? 0 : (s16)-camX;
        }
    }
    VDP_setHorizontalScrollTile(BG_A, 0, scrA, STAGE_ROWS, tm);
    VDP_setHorizontalScrollTile(BG_B, 0, scrB, STAGE_ROWS, tm);
}

// ---------------------------------------------------------------------------
// Spawn de un soldier de oleada: entra por el borde de la camara, por la
// izquierda con la voltereta y por la derecha con la patada (spawns del 1-1).
// ---------------------------------------------------------------------------
static void spawnWaveEnemy(Enemy* e, u8 type, s8 side, s16 camX) {
    s16 w  = (type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) ? ENEMY_SPRITE_W_ORANGE
                                                      : ENEMY_SPRITE_W_PURPLE;
    s16 x  = (side < 0) ? (s16)(camX - w) : (s16)(camX + SCREEN_W);
    s16 fx = (s16)((side < 0) ? camX + 24 : camX + SCREEN_W - 24);
    s16 top = stageWalkTopAt(fx);
    s16 span = (s16)(cur->walkYMax - 8 - (top + 8));
    s16 y = (s16)(top + 8 + ((span > 0) ? (s16)(random() % (u16)span) : 0));
    if (side < 0) initEnemySomersaultSpawn(e, x, y, 1, PAL2, type);
    else          initEnemyKickSpawn(e, x, y, -1, PAL2, type);
    setEnemyBounds(e, cur->walkYMin, cur->walkYMax, 0, 0, cur->levelW);
}

// ===========================================================================
// El nivel
// ===========================================================================
SceneId stageLevelRun(const StageLevel* L) {
    cur = L;
    clearScene();

    const u8  nPlSetup  = numJugadores();
    const u16 barBlocks = (u16)((nPlSetup > 2) ? MAX_PLAYERS : 2) * HUD_VRAM_PER_PLAYER;
    // En la variante con capa lejana, su tileset ocupa el lugar del primer plano.
    const Image* extra  = L->far ? L->far : L->fg;
    const u16 fgTiles   = extra ? extra->tileset->numTile : 0;
    const VDPPlane hudPl = L->far ? BG_B : BG_A;
    const s16 camMaxX   = (s16)(L->levelW - SCREEN_W);

    // VRAM: HUD | primer plano | cache del fondo | sprites.
    SPR_initEx((u16)(TILE_FONT_INDEX - (TILE_USER_INDEX + barBlocks + fgTiles
                                        + L->bgSlots)));

    VDP_setScreenWidth320();
    VDP_setPlaneSize(64, 32, TRUE);
    VDP_setScrollingMode(HSCROLL_TILE, VSCROLL_PLANE);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setBackgroundColor((u8)L->backdrop);   // 0 = PAL0[0]: los rips lo usan como color

    u8   nPl = nPlSetup;
    bool dosJugadores = (nPl >= 2);
    s16  cameraX = 0;

    u16 hudVram = TILE_USER_INDEX;
    fgVram = (u16)(TILE_USER_INDEX + barBlocks);
    if (extra) VDP_loadTileSet(extra->tileset, fgVram, DMA);
    if (L->far) farDraw(fgVram);
    if (L->bgRaw)
        sbgInitRaw(L->bgRaw, L->far ? BG_A : BG_B, PAL0, (u16)(fgVram + fgTiles),
                   L->bgSlots, cameraX);
    else
        sbgInit(L->bg, L->far ? BG_A : BG_B, PAL0, (u16)(fgVram + fgTiles),
                L->bgSlots, cameraX);
    fgColLeft  = 0;
    fgColRight = -1;
    fgUpdate(cameraX);
    applyScroll(cameraX, DMA);

    hudSetPlane(hudPl);
    hudInit();

    Player p1, p2, p3, p4;
    Player* pls[MAX_PLAYERS] = { &p1, &p2, &p3, &p4 };
    for (u8 k = 0; k < nPl; k++) {
        // Lanes repartidas en la franja (arriba de todo nadie queda comodo).
        s16 top = stageWalkTopAt(STAGE_START_X + FOOT_DX);
        s16 span = (s16)(L->walkYMax - top);
        static const u8 frac[MAX_PLAYERS] = { 40, 65, 20, 85 };   // % de la franja
        s16 y = (s16)(top + (span * frac[k]) / 100);
        initPlayer(pls[k], playerChar(k), playerJoy(k), PAL1, STAGE_START_X, y);
        setPlayerLane(pls[k], L->walkYMin, L->walkYMax);
        setPlayerEndWall(pls[k], 0, 0);
        setPlayerLeftBound(pls[k], cameraX);
        setPlayerRightBound(pls[k], cameraX + SCREEN_W - PLAYER_SPRITE_W);
        setPlayerCamera(pls[k], cameraX);
    }

    VDP_loadFont(&hud_font, DMA);
    VDP_setTextPlane(hudPl);
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
    u16 maxAlive = dosJugadores ? STAGE_MAX_ALIVE_2P : STAGE_MAX_ALIVE_1P;

    if (L->bossInit) L->bossInit();
    bool bossStarted = FALSE;

    s16 waveIdx     = -1;       // ultima oleada disparada
    u8  waveSpawned = 0;
    s16 camLockX    = -1;       // -1 = camara libre

    // Revelado: PAL0 el fondo, PAL1 las tortugas, PAL2 los foot soldiers,
    // PAL3 en negro (la carga el jefe al entrar).
    u16 target[64];
    for (u16 i = 0; i < 16; i++) {
        target[i]      = L->bgRaw ? L->bgRaw->pal->data[i] : L->bg->palette->data[i];
        target[16 + i] = leo_player.palette->data[i];
        target[32 + i] = foot_soldier.palette->data[i];
        target[48 + i] = L->far ? L->far->palette->data[i] : 0;
    }
    for (u8 k = 0; k < nPl; k++) updatePlayer(pls[k]);   // coloca los sprites
    SPR_update();

    XGM2_setLoopNumber(-1);
    playMusicVol(L->music, L->musicVol);

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
            if (nc > camMaxX) nc = camMaxX;
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
                setPlayerLane(&p2, L->walkYMin, L->walkYMax);
                setPlayerEndWall(&p2, 0, 0);
                setPlayerCamera(&p2, cameraX);
                setPlayerLeftBound(&p2, cameraX);
                setPlayerRightBound(&p2, cameraX + SCREEN_W - PLAYER_SPRITE_W);
                hudPlayerInit(&huds[1], &p2, hudPlayerCol(1),
                              (u16)(hudVram + HUD_VRAM_PER_PLAYER));
                nPl          = 2;
                dosJugadores = TRUE;
                maxAlive     = STAGE_MAX_ALIVE_2P;
                resetEnemyAI(2);
            }
        }
        for (u8 k = 0; k < nPl; k++) hudPlayerUpdate(&huds[k]);

        // Continues (con el jefe muriendo no se evaluan).
        bool dying = (bossStarted && L->bossDying && L->bossDying());
        if (!dying && continueStepAll(conts, pls, huds, nPl, fps)) { allOut = TRUE; break; }

        // --- Oleadas --------------------------------------------------------
        u16 alive = 0;
        for (u16 i = 0; i < MAX_ENEMIES; i++)
            if (enemies[i].state != ENEMY_STATE_INACTIVE) alive++;

        s16 leadFeetX = leadX + FOOT_DX;
        if ((waveIdx < 0 || waveSpawned >= L->waves[waveIdx].n) &&
            waveIdx + 1 < (s16)L->nWaves &&
            leadFeetX >= L->waves[waveIdx + 1].trigX) {
            waveIdx++;
            waveSpawned = 0;
            camLockX = L->waves[waveIdx].lockX;
            if (camLockX < cameraX) camLockX = cameraX;
        }
        if (waveIdx >= 0) {
            const StageWave* w = &L->waves[waveIdx];
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
            if (waveSpawned >= w->n && alive == 0) camLockX = -1;   // limpia
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

        // --- Jefe ------------------------------------------------------------
        bool wavesDone = (L->nWaves == 0) ||
                         (waveIdx == (s16)L->nWaves - 1 &&
                          waveSpawned >= L->waves[waveIdx].n && alive == 0);
        if (!bossStarted && wavesDone && cameraX >= camMaxX &&
            leadFeetX >= L->bossFeetX) {
            bossStarted = TRUE;
            if (L->bossStart) {
                if (L->bossMusic) {
                    XGM2_setLoopNumber(-1);
                    playMusicVol(L->bossMusic, L->bossMusicVol);
                }
                L->bossStart(cameraX, L->levelW);
            }
        }
        if (bossStarted) {
            if (!L->bossUpdate || L->bossUpdate(pls, nPl, cameraX)) {
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
        u16 hold = (IS_PAL_SYSTEM ? 50 : 60) * STAGE_CLEAR_SECS;
        while (hold > 0) { hold--; SYS_doVBlankProcess(); }
        XGM2_stop();
        XGM2_setLoopNumber(-1);
    }

    shurikenReleaseAll();
    if (L->bossRelease) L->bossRelease();
    VDP_setTextPriority(0);
    VDP_setTextPalette(PAL0);
    VDP_setTextPlane(BG_A);
    clearScene();          // tambien vuelve el scroll a modo plano y el HUD a BG_A
    cur = NULL;
    if (jump != PAUSE_NO_JUMP) return jump;
    if (win && !allOut) return L->nextScene;
    return SCENE_GAME_OVER;
}
