// ===========================================================================
// level_snow.c - Scene 3-1: CENTRAL PARK con nieve (del port de NES) (21/10)
// ===========================================================================
// Nivel del fondo que mando el artista. (21/10) Con OLEADAS DE PRUEBA de
// morados (2, 3, 4, 5 y 6). Todavia sin jefe (falta el arte: cartel de pizza,
// carro de nieve, el jefe): se gana llegando al final (bossFeetX) y sigue la
// pantalla de la ciudad (SCENE_CUT_CITY). Se entra tambien desde la pausa
// (SCENE 3 / 3-1). Motor: stage_level.c.
//
// (22/10) FONDO POR CAPAS CON PARALLAX (tools/gen_level_snow.py, desde las
// capas separadas del artista):
//   BG_B  banda 0, filas 0-9:   cielo + arboles + baranda de arriba, 1/4
//         banda 1, filas 10-15: el rio, 1/2
//   BG_A  el FRENTE (alambrado / baranda baja, piso, carteles, canteros y
//         circulos de hielo) a la velocidad de la camara, streameado con el
//         cache de stage_bg. Lo vacio deja ver BG_B.
//   HUD   en WINDOW (filas 0-3), para que se vea el cielo detras.
// El cielo va con los arboles porque las puntas nevadas y las cabezas de los
// faroles se meten en su franja sin caer en un borde de 8 px.
// Las bandas van ENTERAS en VRAM (LVLSNOW_BAND_TILES) y el frente por el
// cache (LVLSNOW_BG_WORST): ver level_snow_bg.h.
//
// (22/10) LARGO: el del nivel de NES (2649 px a 256 de ancho -> 3312). Con
// BANCOS pintados en el frente (obstaculos, como los canteros) y CARTELES
// "ONE WAY" (sprite snow_oneway) que dan una vuelta al golpearlos, como el del
// garage. Posiciones en el generador (BENCHES, SIGNS).
//
// NIEVE: sprites de 8x8 que comparten los LVLSNOW_FLAKES tiles de
// snow_flakes (sin VRAM de sprites propia: SPR_addSpriteEx sin auto-alloc).
// Con el parallax BG_A es el frente: el relleno aPat de antes ya no tiene
// plano libre. (22/10) Mas copiosa: SNOW_N_MANY copos con 1-2 jugadores y
// SNOW_N_FEW con 3-4 (cada copo es un sprite de VDP de los 80 que hay; las
// tortugas usan 8-11 y cada morado 6). Dos profundidades: un tercio CERCA
// (copos grandes, caen rapido, se corren con la camara) y el resto LEJOS
// (puntos, lentos, se corren a la mitad). Van AL FINAL de la lista de sprites
// (detras de los personajes): si una linea pasa los 20 sprites, el que no se
// dibuja es un copo y no una tortuga.
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level_snow.h"        // pal_snow, bg_snow_*, snow_flakes, snow_flake_spr (rescomp)
#include "level_snow_bg.h"     // medidas, bandas y obstaculos (generado)
#include "stage_level.h"
#include "audio.h"             // music_winter
#include "enemy.h"             // ENEMY_TYPE_*
#include "hud.h"               // numJugadores
#include "player.h"            // playerAttackHitsBox

#define SCREEN_W              320
// LVLSNOW_W (ancho del nivel) sale de level_snow_bg.h: el generador puede
// alargar el fondo repitiendo su ciclo (EXTRA_CYCLES).
// Franja caminable (pies, y de mundo = y de la imagen). El piso arranca en
// y 124; los pies desde LVLSNOW_WALK_Y_MIN (132, en level_snow_bg.h). Los
// canteros no se pisan: el tope por columna (lvlSnowWalkTop, generado) los
// deja como pared y obliga a pasar por delante.
#define LVLSNOW_WALK_Y_MAX    216
// Con los pies del lider aca (y la camara en el final) se gana el nivel.
#define LVLSNOW_END_FEET_X   (LVLSNOW_W - 96)

// (21/10) OLEADAS DE PRUEBA: solo morados, 2-3-4-5-6, repartidas en el ancho
// (la camara se clava en lockX hasta limpiarlas), entrando por los dos
// bordes. Tope de vivos a la vez propio del nivel (LVLSNOW_MAX_ALIVE_*): el
// resto espera su turno. Si no entran en la VRAM, stage_level espera a que
// caiga alguno y a los 2,5 s sin nadie vivo lo saltea.
#define LVLSNOW_MAX_ALIVE_1P   5
#define LVLSNOW_MAX_ALIVE_2P   3
// (22/10) EL TRACTOR (SP_TRACTOR, ver mas abajo) es una oleada propia: con la
// camara clavada cruza de derecha a izquierda y recien al salir se suelta la
// camara. Sale solo con la pantalla SIN soldiers (y la oleada siguiente no
// arranca mientras la camara siga clavada): nunca se cruzan. Dos veces, donde
// lo marca el mapa del NES (x 784-832 y 1813-1867 de 2649 -> ~1010 y ~2300).
// Las oleadas de soldiers quedan entre medio (cada una lejos de la anterior:
// mientras dura una, la siguiente no se puede disparar).
#define SP_TRACTOR  (STAGE_SPECIAL | 1)
#define P  ENEMY_TYPE_FOOT_SOLDIER
#define TR SP_TRACTOR
static const StageWave wavesSnow[] = {
    {  450,  270, 2, { P, P },             { +1, -1 },                 { 0 } },
    { 1000,  850, 1, { TR },               { +1 },                     { 0 } },
    { 1420, 1240, 3, { P, P, P },          { +1, +1, -1 },             { 0 } },
    { 1880, 1700, 4, { P, P, P, P },       { +1, -1, +1, -1 },         { 0 } },
    { 2290, 2140, 1, { TR },               { +1 },                     { 0 } },
    { 2700, 2520, 5, { P, P, P, P, P },    { +1, +1, -1, +1, -1 },     { 0 } },
    { 3060, 2880, 6, { P, P, P, P, P, P }, { +1, -1, +1, -1, +1, -1 }, { 0 } },
};
#undef P
#undef TR

static const SbgRaw snowBg = {
    (const u32*) bg_snow_tiles,
    (const u16*) bg_snow_map,
    LVLSNOW_BG_W, LVLSNOW_BG_ROWS,
    &pal_snow,
    NULL,
};

static const StageBand snowBandList[2] = {
    { LVLSNOW_FAR_ROW0, LVLSNOW_FAR_ROWS, LVLSNOW_FAR_W, LVLSNOW_FAR_NUM, LVLSNOW_FAR_DEN,
      (const u16*) bg_snow_band0_map },
    { LVLSNOW_RIV_ROW0, LVLSNOW_RIV_ROWS, LVLSNOW_RIV_W, LVLSNOW_RIV_NUM, LVLSNOW_RIV_DEN,
      (const u16*) bg_snow_band1_map },
};
static const StageBands snowBands = {
    (const u32*) bg_snow_bands_tiles, LVLSNOW_BAND_TILES, 2, snowBandList,
    LVLSNOW_BAND_WORST + 8,         // (22/10) cache: solo lo que se ve
};

// ---------------------------------------------------------------------------
// Nieve (sprites)
// ---------------------------------------------------------------------------
#define SNOW_N_MANY        28      // copos con 1-2 jugadores
#define SNOW_N_FEW         12      // con 3-4 (menos sprites de VDP libres)
#define SNOW_N             SNOW_N_MANY
#define SNOW_NEAR_DIV       3      // 1 de cada 3 es de los cercanos
#define SNOW_TOP           32      // debajo del HUD (filas 0-3)
#define SNOW_NEAR_VY_Q4    18      // cerca: 18..27 Q4 (1,1 .. 1,7 px por cuadro)
#define SNOW_NEAR_VY_RND   10
#define SNOW_FAR_VY_Q4      8      // lejos: 8..14 Q4 (0,5 .. 0,9 px por cuadro)
#define SNOW_FAR_VY_RND     7
#define SNOW_WIND_Q4       (-3)    // deriva lenta hacia la izquierda
#define SNOW_SWAY_SHIFT     3      // un paso del vaiven cada 8 cuadros
#define SNOW_WRAP_W       336      // 320 + 16: ancho del lazo en X

// Vaiven: medio seno de amplitud 3 px, 32 pasos (un ciclo ~4 s).
static const s8 swayTbl[32] = {
     0,  1,  1,  2,  2,  3,  3,  3,  3,  3,  3,  2,  2,  1,  1,  0,
     0, -1, -1, -2, -2, -3, -3, -3, -3, -3, -3, -2, -2, -1, -1,  0,
};

// Tiles de snow_flakes por profundidad: cerca los grandes, lejos los puntos.
static const u8 nearTiles[3] = { 3, 4, 5 };
static const u8 farTiles[5]  = { 0, 1, 2, 6, 7 };

typedef struct {
    Sprite* spr;
    s16     xq, yq;     // Q4, en pantalla
    u8      vy;         // Q4 por cuadro
    u8      ph;         // fase del vaiven
    bool    near;       // cerca (grande, rapido) o lejos
    bool    vis;
} Flake;
static Flake flakes[SNOW_N];
static u16   snowN;
static s16   snowCamX;
static u16   snowT;

static void flakeRespawn(Flake* f, bool anywhere) {
    f->xq = (s16)(((s16)(random() % SNOW_WRAP_W) - 8) << 4);
    f->yq = (s16)((anywhere ? (s16)(SNOW_TOP + random() % (224 - SNOW_TOP)) : (SNOW_TOP - 8)) << 4);
    f->vy = f->near ? (u8)(SNOW_NEAR_VY_Q4 + random() % SNOW_NEAR_VY_RND)
                    : (u8)(SNOW_FAR_VY_Q4 + random() % SNOW_FAR_VY_RND);
    f->ph = (u8)(random() & 31);
}

// ---------------------------------------------------------------------------
// Carteles ONE WAY (22/10): el del garage (level4_1.c, PK_ONEWAY) reducido a lo
// que hace aca. El sprite existe solo mientras el cartel asoma en camara (o
// mientras gira); al golpearlo da una vuelta (8 cuadros).
// ---------------------------------------------------------------------------
#define SIGN_TICKS          3     // cuadros de juego por cuadro de la vuelta
#define SIGN_FRAMES         8
#define SIGN_COOL          16     // entre golpes (un swing pega una vez)
#define SIGN_ARM_MARGIN    16     // px que tiene que asomar para existir
#define SIGN_HALF_W        10     // caja contra los golpes (como el garage)
#define SIGN_BODY_H        90

typedef struct {
    Sprite* spr;
    u8      spin;       // 1 = girando
    u8      timer;
    u8      cool;
} SnowSign;
static SnowSign signs[LVLSNOW_N_SIGNS];

static void signsInit(void) {
    for (u16 i = 0; i < LVLSNOW_N_SIGNS; i++) {
        signs[i].spr = NULL;
        signs[i].spin = signs[i].timer = signs[i].cool = 0;
    }
}

static void signsRelease(void) {
    for (u16 i = 0; i < LVLSNOW_N_SIGNS; i++)
        if (signs[i].spr) { SPR_releaseSprite(signs[i].spr); signs[i].spr = NULL; }
}

static void signsUpdate(Player** pls, u8 nPl, s16 camX) {
    for (u16 i = 0; i < LVLSNOW_N_SIGNS; i++) {
        SnowSign* s = &signs[i];
        s16 x    = lvlSnowSigns[i][0];
        s16 base = lvlSnowSigns[i][1];
        if (s->cool) s->cool--;
        bool near = s->spin ||
                    ((x + LVLSNOW_SIGN_W > camX + SIGN_ARM_MARGIN) &&
                     (x < camX + SCREEN_W - SIGN_ARM_MARGIN));
        if (!near) {
            if (s->spr) { SPR_releaseSprite(s->spr); s->spr = NULL; }
            continue;
        }
        if (!s->spr) {
            if (!sprVramFits(snow_oneway.maxNumTile)) continue;
            s->spr = SPR_addSprite(&snow_oneway, 0, -128, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
            if (!s->spr) continue;
            SPR_setAutoAnimation(s->spr, FALSE);
        }
        if (!s->cool) {
            s16 cx = (s16)(x + LVLSNOW_SIGN_POLE + 2);
            for (u8 k = 0; k < nPl; k++) {
                if (playerAttackHitsBox(pls[k], cx, base, SIGN_HALF_W, SIGN_BODY_H)) {
                    s->spin  = 1;
                    s->timer = 0;
                    s->cool  = SIGN_COOL;
                    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                                   SOUND_PCM_CH3, 15, FALSE, FALSE);
                    break;
                }
            }
        }
        if (s->spin && ++s->timer >= SIGN_TICKS * SIGN_FRAMES) {
            s->spin  = 0;
            s->timer = 0;
        }
        SPR_setFrame(s->spr, s->spin ? (s16)(s->timer / SIGN_TICKS) : 0);
        SPR_setPosition(s->spr, (s16)(x - camX), (s16)(base - LVLSNOW_SIGN_H));
        SPR_setDepth(s->spr, -base);
    }
}

// Crea los copos (o los vuelve a crear despues del tractor).
static bool snowOff;
static void snowCreate(void) {
    u16 vram = stageExtraVram();
    for (u16 i = 0; i < snowN; i++) {
        Flake* f = &flakes[i];
        f->near = (i % SNOW_NEAR_DIV) == 0;
        flakeRespawn(f, TRUE);
        u16 t = f->near ? nearTiles[i % 3] : farTiles[i % 5];
        // Tiles fijos (los de snow_flakes): sin auto-alloc ni auto-upload, asi
        // no gastan VRAM de sprites. Al final de la lista (detras de todos).
        f->spr = SPR_addSpriteEx(&snow_flake_spr, f->xq >> 4, f->yq >> 4,
                                 TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, vram + t), 0);
        f->vis = FALSE;
        if (f->spr) {
            SPR_setDepth(f->spr, SPR_MAX_DEPTH);
            SPR_setVisibility(f->spr, HIDDEN);
        }
    }
    snowOff = FALSE;
}

static void snowDestroy(void) {
    for (u16 i = 0; i < SNOW_N; i++) {
        if (flakes[i].spr) SPR_releaseSprite(flakes[i].spr);
        flakes[i].spr = NULL;
    }
    snowOff = TRUE;
}

// ---------------------------------------------------------------------------
// EL TRACTOR (22/10): snow_trator_l / _r (dos mitades de 128x112, PAL3 con su
// propia paleta). Cruza de derecha a izquierda por el carril TRAC_BASE; a la
// tortuga que agarra (pies a TRAC_DEPTH del carril) le saca TRAC_BARS de vida
// y la tira, una vez por pasada. Necesita ~365 tiles de sprites: sale con la
// pantalla sin soldiers y saca los copos mientras pasa (son 28 sprites de VDP
// de los 80). Si no entra (2 jugadores: las tortugas y sus HUD se comen la
// VRAM), stage_level lo saltea a los 2,5 s y la camara sigue.
// ---------------------------------------------------------------------------
#define TRAC_BASE       184    // carril (y de mundo de las ruedas)
#define TRAC_DEPTH       14    // pies a esto del carril: lo agarra
#define TRAC_SPEED_Q4    48    // 3 px por cuadro
#define TRAC_ANIM         6    // cuadros de juego por cuadro de la animacion
#define TRAC_BARS         4    // vida que saca
#define TRAC_HIT_X0      10    // la parte que atropella, dentro de los 256 px
#define TRAC_HIT_X1     250
#define TRAC_HALF       (LVLSNOW_TRAC_W / 2)

static struct {
    bool    on;
    Sprite* s[2];
    s32     xq;         // Q4, borde izquierdo de la celda (mundo)
    u16     t;
    u8      hitMask;
} trac;

static void tracRelease(void) {
    for (u16 k = 0; k < 2; k++)
        if (trac.s[k]) { SPR_releaseSprite(trac.s[k]); trac.s[k] = NULL; }
    trac.on = FALSE;
}

static bool tracStart(s16 camX) {
    if (trac.on) return TRUE;
    if (stageAliveEnemies() > 0) return FALSE;          // pantalla limpia
    if (!snowOff) snowDestroy();                         // libera sprites de VDP
    u16 need = (u16)(snow_trator_l.maxNumTile + snow_trator_r.maxNumTile);
    if (!sprVramFits(need)) return FALSE;
    PAL_setPalette(PAL3, snow_trator_l.palette->data, DMA);
    trac.s[0] = SPR_addSprite(&snow_trator_l, -128, -128, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    trac.s[1] = trac.s[0] ? SPR_addSprite(&snow_trator_r, -128, -128,
                                          TILE_ATTR(PAL3, FALSE, FALSE, FALSE)) : NULL;
    if (!trac.s[1]) { tracRelease(); return FALSE; }
    for (u16 k = 0; k < 2; k++) {
        SPR_setAutoAnimation(trac.s[k], FALSE);
        SPR_setDepth(trac.s[k], -TRAC_BASE);
    }
    trac.on = TRUE;
    trac.xq = (s32)(camX + SCREEN_W + 8) << 4;
    trac.t = 0;
    trac.hitMask = 0;
    return TRUE;
}

static void tracUpdate(Player** pls, u8 nPl, s16 camX) {
    if (!trac.on) return;
    trac.xq -= TRAC_SPEED_Q4;
    trac.t++;
    s16 x = (s16)(trac.xq >> 4);
    if (x + LVLSNOW_TRAC_W < camX - 8) {                // salio por la izquierda
        tracRelease();
        snowCreate();
        return;
    }
    s16 fr = (s16)((trac.t / TRAC_ANIM) & 1);
    for (u16 k = 0; k < 2; k++) {
        SPR_setFrame(trac.s[k], fr);
        SPR_setPosition(trac.s[k], (s16)(x + k * TRAC_HALF - camX),
                        (s16)(TRAC_BASE - LVLSNOW_TRAC_H));
    }
    for (u8 k = 0; k < nPl; k++) {
        if (trac.hitMask & (1 << k)) continue;
        if (!playerCanBeHit(pls[k])) continue;
        s16 dy = (s16)(getPlayerY(pls[k]) - TRAC_BASE);
        if (dy < -TRAC_DEPTH || dy > TRAC_DEPTH) continue;
        s16 pcx = getPlayerHurtCX(pls[k]);
        if (pcx + PLAYER_BODY_HALF_W < x + TRAC_HIT_X0 ||
            pcx - PLAYER_BODY_HALF_W > x + TRAC_HIT_X1) continue;
        // viene de la derecha: la empuja hacia la izquierda
        playerHitBarsKnockdown(pls[k], (s16)(pcx + 16), TRAC_BARS);
        trac.hitMask |= (u8)(1 << k);
    }
}

static bool lvlSnowSpecial(u8 type, s8 side, s16 x, s16 camX) {
    (void)side; (void)x;
    if (type == SP_TRACTOR) return tracStart(camX);
    return TRUE;
}

static u16 lvlSnowSpecialAlive(void) { return trac.on ? 1 : 0; }

// No entro en la VRAM (2 jugadores): se saltea y vuelven los copos.
static void lvlSnowFallback(u8 type, s8 side, s16 x, s16 camX) {
    (void)type; (void)side; (void)x; (void)camX;
    tracRelease();
    if (snowOff) snowCreate();
}

static void lvlSnowInit(void) {
    signsInit();
    trac.on = FALSE;
    trac.s[0] = trac.s[1] = NULL;
    VDP_loadTileData((const u32*) snow_flakes, stageExtraVram(), LVLSNOW_FLAKES, DMA);
    snowCamX = 0;
    snowT    = 0;
    snowN    = (numJugadores() > 2) ? SNOW_N_FEW : SNOW_N_MANY;
    for (u16 i = 0; i < SNOW_N; i++) flakes[i].spr = NULL;
    snowCreate();
}

static void lvlSnowUpdate(Player** pls, u8 nPl, s16 camX) {
    signsUpdate(pls, nPl, camX);
    tracUpdate(pls, nPl, camX);
    s16 dcam = (s16)(camX - snowCamX);       // la camara avanza: los copos quedan atras
    snowCamX = camX;
    snowT++;
    for (u16 i = 0; i < snowN; i++) {
        Flake* f = &flakes[i];
        if (!f->spr) continue;
        f->yq = (s16)(f->yq + f->vy);
        // cerca se corre con la camara entera; lejos, a la mitad
        f->xq = (s16)(f->xq + SNOW_WIND_Q4 - (f->near ? (dcam << 4) : (dcam << 3)));
        if ((f->yq >> 4) >= 224) flakeRespawn(f, FALSE);
        s16 x = (s16)(f->xq >> 4);
        if (x < -8)        f->xq = (s16)(f->xq + (SNOW_WRAP_W << 4));
        else if (x >= 328) f->xq = (s16)(f->xq - (SNOW_WRAP_W << 4));
        s16 y  = (s16)(f->yq >> 4);
        s16 sx = (s16)((f->xq >> 4) + swayTbl[((snowT >> SNOW_SWAY_SHIFT) + f->ph) & 31]);
        bool v = (y >= SNOW_TOP);
        if (v != f->vis) {
            f->vis = v;
            SPR_setVisibility(f->spr, v ? VISIBLE : HIDDEN);
        }
        if (v) SPR_setPosition(f->spr, sx, y);
    }
}

static void lvlSnowRelease(void) {
    signsRelease();
    tracRelease();
    snowDestroy();
}

static const StageLevel levelSnow = {
    .bgRaw        = &snowBg,
    .bands        = &snowBands,     // (22/10) parallax
    .fg           = NULL,
    .extraTiles   = LVLSNOW_FLAKES,
    .bgSlots      = LVLSNOW_BG_WORST + 8,
    .levelW       = LVLSNOW_W,
    .walkTop      = lvlSnowWalkTop, // (22/10) los canteros
    .walkCols     = LVLSNOW_BG_W,
    .walkYMin     = LVLSNOW_WALK_Y_MIN,
    .walkYMax     = LVLSNOW_WALK_Y_MAX,
    .waves        = wavesSnow,
    .nWaves       = sizeof(wavesSnow) / sizeof(wavesSnow[0]),
    .bossFeetX    = LVLSNOW_END_FEET_X,
    .music        = music_winter,
    .musicVol     = 80,
    .levelInit    = lvlSnowInit,
    .levelUpdate  = lvlSnowUpdate,
    .levelRelease = lvlSnowRelease,
    .nextScene    = SCENE_CUT_CITY,
    .maxAlive1P   = LVLSNOW_MAX_ALIVE_1P,
    .maxAlive2P   = LVLSNOW_MAX_ALIVE_2P,
    .specialSpawn = lvlSnowSpecial,     // (22/10) el tractor
    .specialAlive = lvlSnowSpecialAlive,
    .specialFallback = lvlSnowFallback,
};

SceneId showSceneSnow() {
    return stageLevelRun(&levelSnow);
}
