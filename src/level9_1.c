// ===========================================================================
// level9_1.c - Scene 9: la SALA DEL PORTAL, pelea final con Shredder (27/09)
// ===========================================================================
// Arte del proyecto del companero (Ray Project, lvl_8_destroyer): la sala
// (final_room.png, 512x256) con el PORTAL a la Dimension X detras
// (final_portal.png, que cicla colores como en la version de Ray).
//
// CAMARA FIJA: se ve la franja x 96..416, y 32..256 del mundo (el portal
// centrado). Sin scroll no hay nada que acomodar:
//   BG_A  la sala (PAL0), baja prioridad; el indice 0 son los huecos del
//         portal y de las dos ventanitas. Filas 0-3: el HUD encima.
//   BG_B  el portal (PAL3). Su indice 0 es un color de las rayas: el color
//         de fondo del VDP se fija a PAL3[0], asi cicla con el resto.
// PAL2 es de Krang y despues de Shredder (en la sala no hay foot soldiers).
//
// JEFES (06/10, como en el remake de PC): primero KRANG (krang.c), que cae
// del techo al rato de entrar. Al vencerlo estalla y su cabeza se escapa
// hablando; recien entonces aparece SHREDDER (shredder_boss.c) delante del
// portal, con la risa, y al lado su COPIA. Al vencer al verdadero: jingle y
// la pantalla final (showTheEnd).
// Musica: el tema de jefe con cada uno (se corta cuando cae Krang).
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level9_1.h"          // final_room, final_portal (rescomp)
#include "level1.h"            // hud_font
#include "player.h"
#include "hud.h"
#include "shredder_boss.h"
#include "krang.h"             // (06/10)
#include "pause_menu.h"
#include "audio.h"
#include "boss_vo.h"           // (01/10)

#ifndef IS_PAL_SYSTEM
#define IS_PAL_SYSTEM IS_PALSYSTEM
#endif

extern u8 cantidadJugadores;

#define SCREEN_W          320
#define L9_CAM_X           96          // camara fija (mundo)
#define L9_CAM_Y           32
#define L9_COL0            (L9_CAM_X / 8)
#define L9_ROW0            (L9_CAM_Y / 8)
#define L9_LANE_T         148          // pies (mundo)
#define L9_LANE_B         248
#define L9_START_X        (L9_CAM_X + 24)
#define L9_BOSS_X         256          // delante del portal
#define L9_BOSS_Y         170
#define L9_BOSS_DELAY      90          // frames antes de que aparezca
#define L9_KRANG_LANE_T   172          // Krang es alto: no sube tanto (HUD), y las
                                       // tortugas tampoco mientras pelean con el
#define L9_CLEAR_SECS       5
#define VOL_MUSIC          80

// Ciclo de colores del portal (el de Ray): rota las rayas y titila un trio.
static const u8 portalStripe[] = { 0, 3, 6, 7, 10, 1 };
#define PORTAL_STRIPE_N 6

static void portalCycle(const u16* base, u16 tick) {
    static u16 tmp[16];
    for (u16 i = 0; i < 16; i++) tmp[i] = base[i];
    u16 sh = (u16)((tick >> 2) % PORTAL_STRIPE_N);
    for (u16 i = 0; i < PORTAL_STRIPE_N; i++)
        tmp[portalStripe[i]] = base[portalStripe[(i + sh) % PORTAL_STRIPE_N]];
    if (tick & 8) {
        u16 c = tmp[4];
        tmp[4] = tmp[2];
        tmp[2] = tmp[15];
        tmp[15] = c;
    }
    PAL_setColors(48, tmp, 16, DMA_QUEUE);
}

static void drawPlayers(Player** pls, u8 nPl) {
    for (u8 k = 0; k < nPl; k++)
        if (pls[k]->sprite)
            SPR_setPosition(pls[k]->sprite, pls[k]->x - L9_CAM_X,
                            pls[k]->y - PLAYER_FOOT_OFFSET - playerDrawZ(pls[k]) - L9_CAM_Y);
}

// Copia la ventana visible de un IMAGE (40x28 tiles desde L9_COL0/L9_ROW0) a
// las columnas/filas 0.. del plano.
static void drawWindow(VDPPlane plane, const Image* img, u16 pal, u16 vram, u16 fromRow) {
    const TileMap* tm = img->tilemap;
    static u16 row[40];
    for (u16 r = fromRow; r < 28; r++) {
        for (u16 c = 0; c < 40; c++) {
            u16 e = tm->tilemap[(u32)(r + L9_ROW0) * tm->w + (c + L9_COL0)];
            row[c] = (u16)((e & (TILE_ATTR_VFLIP_MASK | TILE_ATTR_HFLIP_MASK))
                           | TILE_ATTR(pal, FALSE, FALSE, FALSE)
                           | (vram + (e & TILE_INDEX_MASK)));
        }
        VDP_setTileMapDataRow(plane, row, r, 0, 40, CPU);
    }
}

SceneId showScene91() {
    clearScene();

    const u8  nPlSetup  = numJugadores();
    const u16 barBlocks = (u16)((nPlSetup > 2) ? MAX_PLAYERS : 2) * HUD_VRAM_PER_PLAYER;
    const u16 roomTiles = final_room.tileset->numTile;
    const u16 portTiles = final_portal.tileset->numTile;

    // VRAM: HUD | sala | portal | sprites.
    SPR_initEx((u16)(TILE_FONT_INDEX - (TILE_USER_INDEX + barBlocks + roomTiles + portTiles)));

    VDP_setScreenWidth320();
    VDP_setPlaneSize(64, 32, TRUE);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setBackgroundColor(48);        // PAL3[0]: una de las rayas del portal

    u16 hudVram  = TILE_USER_INDEX;
    u16 roomVram = (u16)(TILE_USER_INDEX + barBlocks);
    u16 portVram = (u16)(roomVram + roomTiles);
    VDP_loadTileSet(final_room.tileset, roomVram, DMA);
    VDP_loadTileSet(final_portal.tileset, portVram, DMA);
    drawWindow(BG_B, &final_portal, PAL3, portVram, 0);
    drawWindow(BG_A, &final_room, PAL0, roomVram, 0);

    hudSetPlane(BG_A);
    hudInit();

    u8 nPl = nPlSetup;
    Player p1, p2, p3, p4;
    Player* pls[MAX_PLAYERS] = { &p1, &p2, &p3, &p4 };
    static const u8 frac[MAX_PLAYERS] = { 40, 65, 20, 85 };
    for (u8 k = 0; k < nPl; k++) {
        s16 y = (s16)(L9_LANE_T + ((L9_LANE_B - L9_LANE_T) * frac[k]) / 100);
        initPlayer(pls[k], playerChar(k), playerJoy(k), PAL1, L9_START_X, y);
        setPlayerLane(pls[k], L9_LANE_T, L9_LANE_B);
        setPlayerEndWall(pls[k], 0, 0);
        setPlayerLeftBound(pls[k], L9_CAM_X);
        setPlayerRightBound(pls[k], L9_CAM_X + SCREEN_W - PLAYER_SPRITE_W);
        setPlayerCamera(pls[k], L9_CAM_X);
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

    static Krang krang;
    static ShredderBoss shred, clone;
    krangInit(&krang);
    shredderInit(&shred);
    shredderInit(&clone);
    u8 phase = 0;                       // 0 espera, 1 Krang, 2 Shredder
    s16 laneTop = L9_LANE_T;
    bool cloneOut = FALSE, cloneGone = FALSE;
    const s16 arenaL = L9_CAM_X + 24, arenaR = L9_CAM_X + SCREEN_W - 24;

    // Revelado: PAL0 la sala, PAL1 las tortugas, PAL2 en negro (la cargan
    // Krang y despues Shredder al aparecer), PAL3 el portal.
    u16 target[64];
    for (u16 i = 0; i < 16; i++) {
        target[i]      = final_room.palette->data[i];
        target[16 + i] = leo_player.palette->data[i];
        target[32 + i] = 0;
        target[48 + i] = final_portal.palette->data[i];
    }
    for (u8 k = 0; k < nPl; k++) updatePlayer(pls[k]);
    drawPlayers(pls, nPl);
    SPR_update();

    // (01/10) El tema NO arranca con la sala: arranca cuando cae Krang, y para
    // Shredder primero la risa (completa y con prioridad) y despues el tema
    // (boss_vo.h).
    XGM2_stop();

    static const u16 black[64] = { 0 };
    PAL_setColors(0, black, 64, DMA);
    PAL_fadeInAll(target, 20, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    bool running = TRUE;
    bool allOut  = FALSE;
    bool win     = FALSE;
    u16  tick    = 0;
    SceneId jump = PAUSE_NO_JUMP;
    pauseReset();
    bossVoReset();
    while (running) {
        jump = pausePoll(pls, nPl);
        if (jump != PAUSE_NO_JUMP) break;
        bossVoUpdate();

        for (u8 k = 0; k < nPl; k++) updatePlayer(pls[k]);

        if (nPl == 1) {
            u8 ch2 = p2JoinPoll(hudPlayerCol(1));
            if (ch2 != 0xFF) {
                cantidadJugadores = 2;
                initPlayer(&p2, ch2, playerJoy(1), PAL1,
                           (s16)(getPlayerWorldX(&p1) - 48), getPlayerY(&p1));
                setPlayerLane(&p2, laneTop, L9_LANE_B);
                setPlayerEndWall(&p2, 0, 0);
                setPlayerCamera(&p2, L9_CAM_X);
                setPlayerLeftBound(&p2, L9_CAM_X);
                setPlayerRightBound(&p2, L9_CAM_X + SCREEN_W - PLAYER_SPRITE_W);
                hudPlayerInit(&huds[1], &p2, hudPlayerCol(1),
                              (u16)(hudVram + HUD_VRAM_PER_PLAYER));
                nPl = 2;
            }
        }
        for (u8 k = 0; k < nPl; k++) hudPlayerUpdate(&huds[k]);

        bool bossDying = (phase == 2) && shredderIsDying(&shred);
        if (!bossDying && continueStepAll(conts, pls, huds, nPl, fps)) {
            allOut = TRUE;
            break;
        }

        if (phase == 0 && tick >= L9_BOSS_DELAY) {
            phase = 1;
            krangSpawn(&krang, L9_BOSS_X, arenaL, arenaR, L9_KRANG_LANE_T, L9_LANE_B);
            laneTop = L9_KRANG_LANE_T;
            for (u8 k = 0; k < nPl; k++) setPlayerLane(pls[k], laneTop, L9_LANE_B);
            XGM2_setLoopNumber(-1);
            playMusicVol(music_boss, VOL_MUSIC);
        }
        if (phase == 1) {
            // Primero los golpes de las tortugas y despues la IA (asi el golpe
            // entra en el mismo frame en que el jefe decidiria atacar).
            s8 killer = -1;
            if (krangPlayerHits(&krang, pls, nPl, &killer) && killer >= 0)
                addPlayerScore(pls[(u8)killer], 10);
            krangUpdate(&krang, pls, nPl, L9_CAM_X, L9_CAM_Y);
            krangShotsUpdate(pls, nPl, L9_CAM_X, L9_CAM_Y);
            if (krangIsGone(&krang)) {
                // La cabeza se fue: aparece Shredder.
                krangRelease(&krang);
                phase = 2;
                laneTop = L9_LANE_T;
                for (u8 k = 0; k < nPl; k++) setPlayerLane(pls[k], laneTop, L9_LANE_B);
                shredderSpawn(&shred, L9_BOSS_X, L9_BOSS_Y, arenaL, arenaR, L9_LANE_T, L9_LANE_B, FALSE);
                bossVoStart(shredder_laugh_sfx, sizeof(shredder_laugh_sfx),
                            music_boss, VOL_MUSIC);   // (01/10) risa y despues el tema
            }
        }
        if (phase == 2) {
            if (!cloneOut && shredderIsReady(&shred) && !shredderIsDying(&shred)) {
                cloneOut = TRUE;
                shredderSpawn(&clone, (s16)(L9_BOSS_X - SHRED_CLONE_DX), L9_BOSS_Y,
                              arenaL, arenaR, L9_LANE_T, L9_LANE_B, TRUE);
            }
            s8 killer = -1;
            if (shredderPlayerHits(&shred, pls, nPl, &killer) && killer >= 0)
                addPlayerScore(pls[(u8)killer], 10);
            killer = -1;
            if (cloneOut && shredderPlayerHits(&clone, pls, nPl, &killer) && killer >= 0)
                addPlayerScore(pls[(u8)killer], 2);
            shredderUpdate(&shred, pls, nPl, L9_CAM_X, L9_CAM_Y);
            if (cloneOut) shredderUpdate(&clone, pls, nPl, L9_CAM_X, L9_CAM_Y);
            if (shredderIsDying(&shred) && !cloneGone) {
                cloneGone = TRUE;               // la copia se va con el
                shredderVanish(&clone);
            }
            if (shredderIsGone(&shred)) { win = TRUE; running = FALSE; }
        }

        portalCycle(final_portal.palette->data, tick++);
        drawPlayers(pls, nPl);
        SPR_update();
        SYS_doVBlankProcess();
    }

    if (win && !allOut) {
        for (u8 k = 0; k < nPl; k++) playerPersistSave(pls[k]);
        XGM2_stop();
        XGM2_setLoopNumber(0);
        playMusicVol(music_scene_clear, VOL_MUSIC);
        u16 hold = (IS_PAL_SYSTEM ? 50 : 60) * L9_CLEAR_SECS;
        while (hold > 0) {
            hold--;
            portalCycle(final_portal.palette->data, tick++);
            SYS_doVBlankProcess();
        }
        XGM2_stop();
        XGM2_setLoopNumber(-1);
    }

    krangRelease(&krang);
    shredderRelease(&clone);
    shredderRelease(&shred);
    VDP_setTextPriority(0);
    VDP_setTextPalette(PAL0);
    VDP_setTextPlane(BG_A);
    clearScene();
    if (jump != PAUSE_NO_JUMP) return jump;
    if (win && !allOut) return SCENE_THE_END;
    return SCENE_GAME_OVER;
}
