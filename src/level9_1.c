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
// (13/10) Al vencer a Shredder EXPLOTA EL TECNODROMO: cuando se esfuma, la
// sala tiembla, salen explosiones (la mediana, explosion_sprite, en PAL2) por
// toda la pantalla con flashes blancos, y al final todo se funde en blanco;
// sobre el blanco suena el jingle y despues la pantalla final.
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
#include "enemies.h"           // explosion_sprite (13/10)

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

// camX/camY: la camara (fija; se mueve solo con el temblor del final).
static void drawPlayers(Player** pls, u8 nPl, s16 camX, s16 camY) {
    for (u8 k = 0; k < nPl; k++)
        if (pls[k]->sprite)
            playerSetSpritePos(pls[k], pls[k]->x - camX,
                            pls[k]->y - PLAYER_FOOT_OFFSET - playerDrawZ(pls[k]) - camY);
}

// ---------------------------------------------------------------------------
// (13/10) EXPLOTA EL TECNODROMO
// ---------------------------------------------------------------------------
#define TD_SHAKE_T        240          // temblor + explosiones + flashes
#define TD_FADE_T          50          // despues: todo se funde en blanco
#define TD_BOOM_GAP_MIN     8          // ticks entre explosiones (al azar)
#define TD_BOOM_GAP_RND    10
#define TD_BOOM_T          42          // explosion_sprite: 7 frames x 6
#define TD_BOOMS            5
#define TD_SHAKE_AMP        3          // px
#define TD_WHITE            0xEEE

static struct { Sprite* s; s16 x, y; u16 t; } tdBoom[TD_BOOMS];

static void tdBoomAt(s16 x, s16 y) {
    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                   SOUND_PCM_CH3, 15, FALSE, FALSE);
    for (u16 i = 0; i < TD_BOOMS; i++) {
        if (tdBoom[i].s) continue;
        if (SPR_getLargestFreeVRAMBlock() < explosion_sprite.maxNumTile) return;
        tdBoom[i].s = SPR_addSprite(&explosion_sprite, -64, -64, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        if (!tdBoom[i].s) return;
        tdBoom[i].x = (s16)(x - 32);
        tdBoom[i].y = (s16)(y - 32);
        tdBoom[i].t = 0;
        SPR_setDepth(tdBoom[i].s, SPR_MIN_DEPTH + 1);
        return;
    }
}

static void tdBoomUpdate(s16 camX, s16 camY) {
    for (u16 i = 0; i < TD_BOOMS; i++) {
        if (!tdBoom[i].s) continue;
        if (++tdBoom[i].t >= TD_BOOM_T) { SPR_releaseSprite(tdBoom[i].s); tdBoom[i].s = NULL; continue; }
        SPR_setPosition(tdBoom[i].s, (s16)(tdBoom[i].x - camX), (s16)(tdBoom[i].y - camY));
    }
}

static void tdBoomReleaseAll(void) {
    for (u16 i = 0; i < TD_BOOMS; i++)
        if (tdBoom[i].s) { SPR_releaseSprite(tdBoom[i].s); tdBoom[i].s = NULL; }
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
    u8 phase = 0;                       // 0 espera, 1 Krang, 2 Shredder, 3 explota
    u16 tdT = 0, tdNext = 0;            // (13/10) explosion del tecnodromo
    u8  tdFlash = 0;
    for (u16 i = 0; i < TD_BOOMS; i++) tdBoom[i].s = NULL;
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
    drawPlayers(pls, nPl, L9_CAM_X, L9_CAM_Y);
    SPR_update();

    // (01/10) El tema NO arranca con la sala: arranca cuando cae Krang, y para
    // Shredder primero la risa (completa y con prioridad) y despues el tema
    // (boss_vo.h).
    XGM2_stop();

    static const u16 black[64] = { 0 };
    PAL_setColors(0, black, 64, DMA);
    PAL_fadeInAll(target, 20, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    // (13/10) Flash blanco de la explosion de Krang: la sala (PAL0) y el portal
    // (PAL3, y con el el color de fondo) en blanco; los sprites quedan igual.
    static const u16 white16[16] = {
        0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE,
        0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE
    };
    bool wasWhite = FALSE;

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

        bool bossDying = (phase == 3) || ((phase == 2) && shredderIsDying(&shred));
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
            // (13/10) La copia sale del cuerpo del verdadero (titilan y se
            // separan); si el esta en medio de un ataque, en el proximo frame.
            if (!cloneOut && shredderIsReady(&shred) && !shredderIsDying(&shred))
                cloneOut = shredderSpawnSplit(&clone, &shred, arenaL, arenaR, L9_LANE_T, L9_LANE_B);
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
            shredderFxUpdate();
            if (shredderIsGone(&shred)) {
                // (13/10) Se esfumo: explota el tecnodromo.
                shredderRelease(&clone);
                shredderRelease(&shred);
                phase = 3;
                tdT = 0;
                tdNext = 0;
                for (u16 i = 0; i < 16; i++) target[32 + i] = explosion_sprite.palette->data[i];
                PAL_setColors(32, &target[32], 16, DMA_QUEUE);
            }
        }

        s16 camX = L9_CAM_X, camY = L9_CAM_Y;
        if (phase == 3) {
            tdT++;
            if (tdT <= TD_SHAKE_T) {
                // Tiembla (planos y sprites juntos) y explota por todos lados.
                s16 sx = (s16)((random() % (TD_SHAKE_AMP * 2 + 1)) - TD_SHAKE_AMP);
                s16 sy = (s16)((random() % (TD_SHAKE_AMP * 2 + 1)) - TD_SHAKE_AMP);
                VDP_setHorizontalScroll(BG_A, sx);
                VDP_setHorizontalScroll(BG_B, sx);
                VDP_setVerticalScroll(BG_A, sy);
                VDP_setVerticalScroll(BG_B, sy);
                camX = (s16)(L9_CAM_X - sx);
                camY = (s16)(L9_CAM_Y + sy);
                if (tdT >= tdNext) {
                    tdBoomAt((s16)(L9_CAM_X + 24 + (random() % (SCREEN_W - 48))),
                             (s16)(L9_CAM_Y + 56 + (random() % 150)));
                    tdNext = (u16)(tdT + TD_BOOM_GAP_MIN + (random() % TD_BOOM_GAP_RND));
                }
                // Flashes blancos: uno fuerte al arrancar y despues sueltos.
                if (tdT == 1 || tdT == 60 || tdT == 110 || tdT == 150 ||
                    tdT == 185 || tdT == 210 || tdT == 228)
                    tdFlash = (tdT == 1) ? 6 : 3;
            } else if (tdT == TD_SHAKE_T + 1) {
                VDP_setHorizontalScroll(BG_A, 0);
                VDP_setHorizontalScroll(BG_B, 0);
                VDP_setVerticalScroll(BG_A, 0);
                VDP_setVerticalScroll(BG_B, 0);
                static u16 white64[64];
                for (u16 i = 0; i < 64; i++) white64[i] = TD_WHITE;
                tdFlash = 0;
                PAL_fadeToAll(white64, TD_FADE_T, TRUE);    // todo se funde en blanco
            } else if (!PAL_isDoingFade()) {
                win = TRUE;
                running = FALSE;
            }
            tdBoomUpdate(camX, camY);
        }

        if (phase == 3 && tdT > TD_SHAKE_T) {
            // fundiendo a blanco: no tocar las paletas
        } else if (tdFlash) {
            static const u16 white48[48] = {
                TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE,
                TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE,
                TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE,
                TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE,
                TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE,
                TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE, TD_WHITE
            };
            // Toda la pantalla (sala, tortugas, explosiones y portal) en blanco.
            PAL_setColors(0, white48, 48, DMA_QUEUE);
            PAL_setColors(48, white48, 16, DMA_QUEUE);
            if (--tdFlash == 0) PAL_setColors(0, target, 48, DMA_QUEUE);   // PAL3: portalCycle
            else { drawPlayers(pls, nPl, camX, camY); SPR_update(); SYS_doVBlankProcess(); continue; }
        }

        if (!(phase == 3 && tdT > TD_SHAKE_T))
            portalCycle(final_portal.palette->data, tick++);
        if (phase == 1 && krangWhiteFlash()) {
            PAL_setColors(0,  white16, 16, DMA_QUEUE);
            PAL_setColors(48, white16, 16, DMA_QUEUE);   // pisa el ciclo de este frame
            wasWhite = TRUE;
        } else if (wasWhite) {
            PAL_setColors(0, final_room.palette->data, 16, DMA_QUEUE);
            wasWhite = FALSE;                            // PAL3 la repone portalCycle
        }
        drawPlayers(pls, nPl, camX, camY);
        SPR_update();
        SYS_doVBlankProcess();
    }
    tdBoomReleaseAll();
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);

    if (win && !allOut) {
        for (u8 k = 0; k < nPl; k++) playerPersistSave(pls[k]);
        XGM2_stop();
        XGM2_setLoopNumber(0);
        playMusicVol(music_scene_clear, VOL_MUSIC);
        // (13/10) Sobre el blanco de la explosion (sin ciclar el portal).
        u16 hold = (IS_PAL_SYSTEM ? 50 : 60) * L9_CLEAR_SECS;
        while (hold > 0) {
            hold--;
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
