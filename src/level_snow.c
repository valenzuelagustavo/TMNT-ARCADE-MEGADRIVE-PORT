// ===========================================================================
// level_snow.c - Scene 3-1: CENTRAL PARK con nieve (del port de NES) (21/10)
// ===========================================================================
// Nivel del fondo que mando el artista. (21/10) Con OLEADAS DE PRUEBA de
// morados (2, 3, 4, 5 y 6). (23/10) Al final, el jefe TORA (tora.c) con el
// DISCO flotando; en dos tramos LLUEVEN ESFERAS DE HIELO. Ganado el jefe
// sigue la pantalla de la ciudad (SCENE_CUT_CITY). Se entra tambien desde la pausa
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
#include "player.h"            // playerAttackHitsBox, playerManhole*
#include "level1.h"            // light_out_bubble (el globo del pozo)
#include "tora.h"              // (23/10) el jefe y las esferas de hielo
#include "level_snow_far.h"    // (23/10, mapper) snow_disco, snow_disco_sh

#define SCREEN_W              320
static s16 a16(s16 v) { return (v < 0) ? (s16)-v : v; }
// LVLSNOW_W (ancho del nivel) sale de level_snow_bg.h: el generador puede
// alargar el fondo repitiendo su ciclo (EXTRA_CYCLES).
// Franja caminable (pies, y de mundo = y de la imagen). El piso arranca en
// y 124; los pies desde LVLSNOW_WALK_Y_MIN (132, en level_snow_bg.h). Los
// canteros no se pisan: el tope por columna (lvlSnowWalkTop, generado) los
// deja como pared y obliga a pasar por delante.
#define LVLSNOW_WALK_Y_MAX    216
// Con los pies del lider aca (y la camara en el final) se gana el nivel.
// (23/10) -96 -> -200: el jefe entra por la derecha.
#define LVLSNOW_END_FEET_X   (LVLSNOW_W - 200)

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
#define TRAC_BASE       200    // carril (y de mundo de las ruedas); (22/10) 184 -> 200, dos tiles mas abajo
#define TRAC_DEPTH       14    // pies a esto del carril: lo agarra
#define TRAC_SPEED_Q4    48    // 3 px por cuadro
#define TRAC_ANIM         6    // cuadros de juego por cuadro de la animacion
#define TRAC_BARS         4    // vida que saca
#define TRAC_HIT_X0      10    // la parte que atropella, dentro de los 256 px
#define TRAC_HIT_X1     250
#define TRAC_HALF       (LVLSNOW_TRAC_W / 2)
// (22/10) Mientras pasa suena el loop del ascensor del Technodrome (por ahora),
// como alla: CH2 (el de las voces) con prioridad baja; si una voz lo corta se
// relanza en el cuadro siguiente. Se corta cuando sale.
#define TRAC_SFX_CH     SOUND_PCM_CH2
#define TRAC_SFX_PRIO   6
// (22/10) Visibilidad A MANO de cada mitad: solo dentro de la pantalla y desde
// el segundo cuadro (el primero se suben sus tiles). La X de los sprites de la
// VDP es de 9 bits: la mitad derecha recien creada (x de pantalla ~456) daba
// la vuelta y asomaba un instante por la IZQUIERDA.
#define TRAC_SHOW_AGE   2

static void tracSfx(void) {
    XGM2_playPCMEx(elevator_tecnodrome_sfx, sizeof(elevator_tecnodrome_sfx),
                   TRAC_SFX_CH, TRAC_SFX_PRIO, TRUE, TRUE);
}

static struct {
    bool    on;
    Sprite* s[2];
    s32     xq;         // Q4, borde izquierdo de la celda (mundo)
    u16     t;
    u8      hitMask;
    bool    vis[2];
} trac;

static void tracRelease(void) {
    for (u16 k = 0; k < 2; k++)
        if (trac.s[k]) { SPR_releaseSprite(trac.s[k]); trac.s[k] = NULL; }
    if (trac.on) XGM2_stopPCM(TRAC_SFX_CH);
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
        SPR_setVisibility(trac.s[k], HIDDEN);
        trac.vis[k] = FALSE;
    }
    trac.on = TRUE;
    trac.xq = (s32)(camX + SCREEN_W + 8) << 4;
    trac.t = 0;
    trac.hitMask = 0;
    tracSfx();
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
    if (!XGM2_isPlayingPCM(1 << TRAC_SFX_CH)) tracSfx();   // una voz lo corto
    s16 fr = (s16)((trac.t / TRAC_ANIM) & 1);
    for (u16 k = 0; k < 2; k++) {
        s16 sx = (s16)(x + k * TRAC_HALF - camX);
        bool v = (trac.t >= TRAC_SHOW_AGE) && (sx < SCREEN_W) && (sx + TRAC_HALF > 0);
        if (v != trac.vis[k]) {
            trac.vis[k] = v;
            SPR_setVisibility(trac.s[k], v ? VISIBLE : HIDDEN);
        }
        SPR_setFrame(trac.s[k], fr);
        if (v) SPR_setPosition(trac.s[k], sx, (s16)(TRAC_BASE - LVLSNOW_TRAC_H));
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
    // (23/10) Nunca junto con la lluvia de esferas (las dos usan PAL3).
    if (type == SP_TRACTOR) return (toraIceAlive() == 0) && tracStart(camX);
    return TRUE;
}

static u16 lvlSnowSpecialAlive(void) { return trac.on ? 1 : 0; }

// No entro en la VRAM (2 jugadores): se saltea y vuelven los copos.
static void lvlSnowFallback(u8 type, s8 side, s16 x, s16 camX) {
    (void)type; (void)side; (void)x; (void)camX;
    tracRelease();
    if (snowOff) snowCreate();
}

// ===========================================================================
// (23/10) ALCANTARILLAS en los circulos de hielo del piso (los 4 del arte, en
// el primer tramo). Como las bocas de tormenta del 2-1: al pasar la tortuga
// que va adelante por trigX, de la boca sale un morado con la tapa y la tira
// hacia 'dir' (las 2 de mas atras atacan por la espalda); el agujero queda
// ABIERTO y la tortuga que lo pisa (caminando: saltando no) se cae adentro
// ("Duuuh, who put the light out"). La tapa cerrada (antes) y la boca
// abierta (despues) son sprites con tiles fijos en los extraTiles del nivel.
// ===========================================================================
#define MH_N             4
#define MH_HOLE_TILES   14          // snow_hole (rescomp)
#define MH_LID_TILES    12          // snow_lid
#define MH_HOLE_W       40
#define MH_HOLE_H       24
#define MH_LID_W        32
#define MH_LID_H        24
#define MH_HALF_W       12          // la trampa, como el 2-1 (LVL21_HOLE_*)
#define MH_HALF_H       10
#define MH_OUT_DY       22          // donde sale la tortuga, debajo del agujero
#define MH_MARGIN       48
#define MH_FOOT_DX      (PLAYER_SPRITE_W / 2)
#define MH_BUBBLE_W     96
#define MH_BUBBLE_DY   (-40)
enum { MH_DORMANT = 0, MH_PENDING, MH_DONE };
typedef struct { s16 x, y, trigX; s8 dir; } SnowHole;
// Centros medidos sobre "itens separados.png" (los circulos de hielo).
static const SnowHole holes[MH_N] = {
    //  x    y   trigX  dir
    { 314, 176,  200, -1 },
    {  54, 182,  260, +1 },          // por la espalda
    { 423, 166,  330, -1 },
    { 147, 165,  380, +1 },          // por la espalda
};
static u8      mhSt[MH_N];
static Sprite* mhSpr[MH_N];
static u8      mhLook[MH_N];        // 0 nada, 1 tapa, 2 boca
static Sprite* mhBubble;
static u16     mhHoleVram, mhLidVram;

static void mhInit(void) {
    mhHoleVram = (u16)(stageExtraVram() + LVLSNOW_FLAKES);
    mhLidVram  = (u16)(mhHoleVram + MH_HOLE_TILES);
    const TileSet* th = snow_hole.animations[0]->frames[0]->tileset;
    const TileSet* tl = snow_lid.animations[0]->frames[0]->tileset;
    if (th->numTile <= MH_HOLE_TILES) VDP_loadTileSet(th, mhHoleVram, DMA);
    if (tl->numTile <= MH_LID_TILES)  VDP_loadTileSet(tl, mhLidVram, DMA);
    for (u16 i = 0; i < MH_N; i++) { mhSt[i] = MH_DORMANT; mhSpr[i] = NULL; mhLook[i] = 0; }
    mhBubble = NULL;
    lidInit();
}

static void mhRelease(void) {
    for (u16 i = 0; i < MH_N; i++)
        if (mhSpr[i]) { SPR_releaseSprite(mhSpr[i]); mhSpr[i] = NULL; mhLook[i] = 0; }
    if (mhBubble) { SPR_releaseSprite(mhBubble); mhBubble = NULL; }
    lidReleaseAll();
}

static void mhUpdate(Player** pls, u8 nPl, s16 camX) {
    // Los pies del que va adelante.
    s16 lead = -32000;
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k])) continue;
        s16 f = (s16)(pls[k]->x + MH_FOOT_DX);
        if (f > lead) lead = f;
    }
    for (u16 i = 0; i < MH_N; i++) {
        const SnowHole* h = &holes[i];
        if (mhSt[i] == MH_DORMANT && lead >= h->trigX) mhSt[i] = MH_PENDING;
        if (mhSt[i] == MH_PENDING && stageSpawnManhole(h->x, h->y, h->dir)) mhSt[i] = MH_DONE;
        // La tapa hasta que sale el soldier (que la trae en sus cuadros) y
        // despues la boca abierta. Solo cerca de la camara.
        s16 sx = (s16)(h->x - camX);
        bool near = (sx > -MH_MARGIN && sx < SCREEN_W + MH_MARGIN);
        u8 look = near ? ((mhSt[i] == MH_DONE) ? 2 : 1) : 0;
        if (look != mhLook[i]) {
            if (mhSpr[i]) { SPR_releaseSprite(mhSpr[i]); mhSpr[i] = NULL; }
            if (look) {
                const SpriteDefinition* d = (look == 2) ? &snow_hole : &snow_lid;
                u16 v = (look == 2) ? mhHoleVram : mhLidVram;
                mhSpr[i] = SPR_addSpriteEx(d, -64, -64, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, v),
                                           SPR_FLAG_AUTO_VISIBILITY);
                if (mhSpr[i]) {
                    SPR_setAutoAnimation(mhSpr[i], FALSE);
                    SPR_setDepth(mhSpr[i], SPR_MAX_DEPTH);   // en el piso, detras de todo
                }
            }
            mhLook[i] = look;
        }
        if (mhSpr[i]) {
            s16 w = (look == 2) ? MH_HOLE_W : MH_LID_W;
            s16 hh = (look == 2) ? MH_HOLE_H : MH_LID_H;
            SPR_setPosition(mhSpr[i], (s16)(sx - w / 2), (s16)(h->y - hh / 2 - stageCamY));
        }
    }

    // Caerse en una boca abierta (saltando no: playerManholeFall se apoya en
    // playerCanBeHit, como en el 2-1).
    for (u8 k = 0; k < nPl; k++) {
        if (playerInManhole(pls[k])) continue;
        s16 fx = (s16)(pls[k]->x + MH_FOOT_DX), fy = pls[k]->y;
        for (u16 i = 0; i < MH_N; i++) {
            if (mhSt[i] != MH_DONE) continue;
            if (a16((s16)(fx - holes[i].x)) > MH_HALF_W) continue;
            if (a16((s16)(fy - holes[i].y)) > MH_HALF_H) continue;
            playerManholeFall(pls[k], (s16)(holes[i].x - MH_FOOT_DX),
                              (s16)(holes[i].y + MH_OUT_DY));
            break;
        }
    }

    // Globo "Duuuh, who put the light out" mientras alguna esta en el fondo.
    s8 talking = -1;
    for (u8 k = 0; k < nPl; k++) if (playerManholeSpeaking(pls[k])) { talking = (s8)k; break; }
    if (talking < 0) {
        if (mhBubble) { SPR_releaseSprite(mhBubble); mhBubble = NULL; }
    } else {
        if (!mhBubble && sprVramFits(light_out_bubble.maxNumTile)) {
            mhBubble = SPR_addSprite(&light_out_bubble, -128, -128, TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
            XGM2_playPCMEx(who_put_light_vo, sizeof(who_put_light_vo), SOUND_PCM_CH2, 15, FALSE, FALSE);
        }
        if (mhBubble) {
            s16 bx = (s16)(pls[(u8)talking]->x + MH_FOOT_DX - MH_BUBBLE_W / 2 - camX);
            if (bx < 4) bx = 4;
            if (bx > SCREEN_W - MH_BUBBLE_W - 4) bx = SCREEN_W - MH_BUBBLE_W - 4;
            s16 by = (s16)(pls[(u8)talking]->y + MH_BUBBLE_DY - stageCamY);
            if (by < 36) by = 36;
            SPR_setPosition(mhBubble, bx, by);
            SPR_setDepth(mhBubble, SPR_MIN_DEPTH);
        }
    }

    // Las tapas que tiran (como en el 2-1): un golpe cuerpo a cuerpo las
    // devuelve y ahi barren soldiers; si no, pegan a las tortugas.
    lidUpdate(camX, stageCamY);
    for (u8 k = 0; k < nPl; k++)
        if (lidReflectByPlayerAttack(pls[k], (s8)k))
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
    Enemy* en = stageEnemies();
    for (u16 i = 0; en && i < MAX_ENEMIES; i++) {
        if (!enemyCanBeHit(&en[i])) continue;
        s8 owner = -1;
        if (!lidHitsEnemy(i, getEnemyCenterX(&en[i]), getEnemyCenterY(&en[i]),
                          enemyBodyHalfW(&en[i]), &owner)) continue;
        damageEnemy(&en[i], LID_ENEMY_DMG);
        if (en[i].state == ENEMY_STATE_DEAD) {
            XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                           SOUND_PCM_CH3, 15, FALSE, FALSE);
            if (owner >= 0 && owner < (s8)nPl) addPlayerScore(pls[(u8)owner], 1);
        }
    }
    for (u8 k = 0; k < nPl; k++) {
        if (!playerCanBeHit(pls[k])) continue;
        s16 lx = 0;
        if (!lidHits(getPlayerHurtCX(pls[k]), getPlayerY(pls[k]), PLAYER_BODY_HALF_W, &lx)) continue;
        XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
        damagePlayer(pls[k], lx);
    }
}

// ===========================================================================
// (23/10) El TACHO de basura (snow_trash): como los conos del garage, un golpe
// lo hace salir volando en X hacia donde lo empujo y, volando, le pega a los
// soldiers que cruza. Al salir de la pantalla no vuelve.
// ===========================================================================
#define TRASH_X        790          // borde izquierdo de la celda (mundo)
#define TRASH_BASE     156          // borde de abajo (los pies)
#define TRASH_W         48
#define TRASH_H         56
#define TRASH_HALF_W    14          // la parte que se le puede pegar
#define TRASH_BODY_H    44
#define TRASH_SPEED      6          // px/f volando (el cono)
#define TRASH_ENEMY_DMG  2
#define TRASH_TOL_Y     14
enum { TR_IDLE = 0, TR_FLY, TR_GONE };
static struct { Sprite* s; s16 x; s8 dir, owner; u8 st, hitMask; } trash;

static void trashRelease(void) {
    if (trash.s) { SPR_releaseSprite(trash.s); trash.s = NULL; }
}

static void trashUpdate(Player** pls, u8 nPl, s16 camX) {
    if (trash.st == TR_GONE) return;
    s16 sx = (s16)(trash.x - camX);
    bool near = (sx > -TRASH_W - 16 && sx < SCREEN_W + 16);
    if (!near) {
        if (trash.st == TR_FLY) { trashRelease(); trash.st = TR_GONE; return; }
        trashRelease();
        return;
    }
    if (!trash.s) {
        if (!sprVramFits(snow_trash.maxNumTile)) return;
        trash.s = SPR_addSprite(&snow_trash, -64, -64, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
        if (!trash.s) return;
        SPR_setAutoAnimation(trash.s, FALSE);
    }
    s16 cx = (s16)(trash.x + TRASH_W / 2);
    if (trash.st == TR_IDLE) {
        for (u8 k = 0; k < nPl; k++) {
            if (!playerAttackHitsBox(pls[k], cx, TRASH_BASE, TRASH_HALF_W, TRASH_BODY_H)) continue;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
            s16 pcx = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
            trash.dir = (cx >= pcx) ? 1 : -1;
            trash.owner = (s8)k;
            trash.hitMask = 0;
            trash.st = TR_FLY;
            break;
        }
    } else {
        trash.x = (s16)(trash.x + trash.dir * TRASH_SPEED);
        cx = (s16)(trash.x + TRASH_W / 2);
        Enemy* en = stageEnemies();
        for (u16 e = 0; en && e < MAX_ENEMIES && e < 8; e++) {
            if (trash.hitMask & (1 << e)) continue;
            if (!enemyCanBeHit(&en[e])) continue;
            if (a16((s16)(getEnemyCenterX(&en[e]) - cx)) > TRASH_HALF_W + enemyBodyHalfW(&en[e])) continue;
            if (a16((s16)(getEnemyCenterY(&en[e]) - TRASH_BASE)) > TRASH_TOL_Y) continue;
            damageEnemy(&en[e], TRASH_ENEMY_DMG);
            trash.hitMask |= (u8)(1 << e);
            if (en[e].state == ENEMY_STATE_DEAD) {
                XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                               SOUND_PCM_CH3, 15, FALSE, FALSE);
                if (trash.owner >= 0 && trash.owner < (s8)nPl) addPlayerScore(pls[(u8)trash.owner], 1);
            }
        }
        sx = (s16)(trash.x - camX);
        if (sx + TRASH_W < -8 || sx > SCREEN_W + 8) { trashRelease(); trash.st = TR_GONE; return; }
    }
    SPR_setPosition(trash.s, sx, (s16)(TRASH_BASE - TRASH_H - stageCamY));
    SPR_setDepth(trash.s, (s16)-TRASH_BASE);
}

// ===========================================================================
// (23/10) LLUVIA DE ESFERAS DE HIELO: en dos tramos (camara pasando rainAt)
// caen RAIN_N esferas, de a RAIN_MAX a la vez, cerca de una tortuga al azar;
// la sombra avisa donde (tora.c). No clava la camara ni frena oleadas, pero
// no arranca con el tractor en pantalla (y el tractor la espera): PAL3.
// ===========================================================================
#define RAIN_ZONES      2
#define RAIN_N          8
#define RAIN_MAX        2
#define RAIN_GAP       30      // + random(RAIN_GAP_RND) entre una y otra
#define RAIN_GAP_RND   30
#define RAIN_SPREAD    30      // +- px alrededor de la tortuga
static const s16 rainAt[RAIN_ZONES] = { 1480, 2540 };
static u8  rainZone, rainLeft, rainCd;

static void rainUpdate(Player** pls, u8 nPl, s16 camX) {
    if (rainLeft == 0) {
        if (rainZone >= RAIN_ZONES || camX < rainAt[rainZone] || trac.on) return;
        rainZone++;
        rainLeft = RAIN_N;
        rainCd = 20;
        toraLoadPal();
        return;
    }
    if (rainCd) { rainCd--; return; }
    if (toraIceAlive() >= RAIN_MAX) return;
    u8 k = (u8)(random() % nPl), tries = nPl;
    while (tries-- && isPlayerGameOver(pls[k])) k = (u8)((k + 1) % nPl);
    s16 x = (s16)(getPlayerHurtCX(pls[k]) + (s16)(random() % (2 * RAIN_SPREAD + 1)) - RAIN_SPREAD);
    if (x < camX + 24) x = (s16)(camX + 24);
    if (x > camX + SCREEN_W - 24) x = (s16)(camX + SCREEN_W - 24);
    s16 lane = (s16)(getPlayerY(pls[k]) + (s16)(random() % 21) - 10);
    s16 top = stageWalkTopAt(x);
    if (lane < top) lane = top;
    if (lane > LVLSNOW_WALK_Y_MAX) lane = LVLSNOW_WALK_Y_MAX;
    toraIceRain(x, lane);              // sin VRAM: esa no cae
    rainLeft--;
    rainCd = (u8)(RAIN_GAP + random() % RAIN_GAP_RND);
}

// ---------------------------------------------------------------------------
// (23/10) EL DISCO: la esfera que flota a media altura al final del nivel
// (snow_disco, 3 cuadros, con su sombra en el piso). Decorado; (23/10) en
// PAL0, la del fondo, para no parpadear con el jefe. Aparece cuando entra en
// camara.
// ---------------------------------------------------------------------------
#define DISCO_X        (LVLSNOW_W - 104)   // centro (mundo)
#define DISCO_LANE      150                // donde esta la sombra
#define DISCO_TOP        54                // borde de arriba (pantalla), sin vaiven
#define DISCO_ANIM        8
static const s8 discoBob[16] = { 0,1,2,3,3,4,4,4,3,3,2,1,0,-1,-1,-1 };
static struct { Sprite* s; Sprite* sh; u16 t; } disco;

static void discoRelease(void) {
    if (disco.s)  { SPR_releaseSprite(disco.s);  disco.s  = NULL; }
    if (disco.sh) { SPR_releaseSprite(disco.sh); disco.sh = NULL; }
}

static void discoUpdate(s16 camX) {
    s16 sx = (s16)(DISCO_X - camX);
    if (!disco.s) {
        if (sx > SCREEN_W + 40 || trac.on) return;
        if (!sprVramFits((u16)(snow_disco.maxNumTile + snow_disco_sh.maxNumTile))) return;
        // (23/10) En PAL0 (pal_snow; gen_snow_boss.py lo remapea): con la
        // paleta del jefe parpadeaba junto con Tora cuando le queda poca vida.
        disco.s  = SPR_addSprite(&snow_disco, -128, -128, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
        disco.sh = SPR_addSprite(&snow_disco_sh, -128, -128, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
        if (!disco.s) { discoRelease(); return; }
        SPR_setAutoAnimation(disco.s, FALSE);
        SPR_setDepth(disco.s, -DISCO_LANE);
        if (disco.sh) SPR_setDepth(disco.sh, -DISCO_LANE + 1);
        disco.t = 0;
    }
    disco.t++;
    SPR_setFrame(disco.s, (s16)((disco.t / DISCO_ANIM) % 3));
    SPR_setPosition(disco.s, (s16)(sx - 32),
                    (s16)(DISCO_TOP + discoBob[(disco.t >> 3) & 15] - stageCamY));
    if (disco.sh) SPR_setPosition(disco.sh, (s16)(sx - 28), (s16)(DISCO_LANE - 8 - stageCamY));
}

// ---------------------------------------------------------------------------
// (23/10) Jefe: TORA (tora.c). Arena = la ultima pantalla.
// ---------------------------------------------------------------------------
static Tora tora;

static void bossInitSnow(void) {
    toraInit(&tora);
}

static void bossStartSnow(s16 camX, s16 levelW) {
    (void)levelW;
    // Menos copos: el jefe usa hasta 12 sprites de VDP y cada esfera 8.
    if (!snowOff) snowDestroy();
    snowN = SNOW_N_FEW;
    snowCreate();
    toraSpawn(&tora, (s16)(camX + 24), (s16)(camX + SCREEN_W - 24),
              LVLSNOW_WALK_Y_MIN, LVLSNOW_WALK_Y_MAX, stageWalkTopAt);
}

static bool bossUpdateSnow(Player** pls, u8 nPl, s16 camX) {
    toraUpdate(&tora, pls, nPl, camX);
    s8 killer = -1;
    if (toraPlayerHits(&tora, pls, nPl, &killer) && killer >= 0)
        addPlayerScore(pls[(u8)killer], 5);
    return toraIsGone(&tora) && toraIceAlive() == 0;
}

static bool bossDyingSnow(void) { return toraIsDying(&tora); }

static void bossReleaseSnow(void) { toraRelease(&tora); }

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
    mhInit();                       // (23/10) alcantarillas
    trash.s = NULL; trash.x = TRASH_X; trash.st = TR_IDLE;
    toraIceInit();                  // (23/10) lluvia de esferas, disco
    rainZone = rainLeft = rainCd = 0;
    disco.s = disco.sh = NULL;
}

static void lvlSnowUpdate(Player** pls, u8 nPl, s16 camX) {
    signsUpdate(pls, nPl, camX);
    tracUpdate(pls, nPl, camX);
    mhUpdate(pls, nPl, camX);       // (23/10)
    trashUpdate(pls, nPl, camX);
    rainUpdate(pls, nPl, camX);     // (23/10)
    toraIceUpdate(pls, nPl, camX);  // las de la lluvia y la del jefe
    discoUpdate(camX);
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
    mhRelease();
    trashRelease();
    toraIceReleaseAll();
    discoRelease();
}

static const StageLevel levelSnow = {
    .bgRaw        = &snowBg,
    .bands        = &snowBands,     // (22/10) parallax
    .fg           = NULL,
    .extraTiles   = LVLSNOW_FLAKES + MH_HOLE_TILES + MH_LID_TILES,   // (23/10) + alcantarillas
    .bgSlots      = LVLSNOW_BG_WORST + 8,
    .levelW       = LVLSNOW_W,
    .walkTop      = lvlSnowWalkTop, // (22/10) los canteros
    .walkCols     = LVLSNOW_BG_W,
    .walkYMin     = LVLSNOW_WALK_Y_MIN,
    .walkYMax     = LVLSNOW_WALK_Y_MAX,
    .waves        = wavesSnow,
    .nWaves       = sizeof(wavesSnow) / sizeof(wavesSnow[0]),
    .bossFeetX    = LVLSNOW_END_FEET_X,
    .bossMusic    = music_boss,     // (23/10) TORA
    .bossMusicVol = 80,
    .bossInit     = bossInitSnow,
    .bossStart    = bossStartSnow,
    .bossUpdate   = bossUpdateSnow,
    .bossDying    = bossDyingSnow,
    .bossRelease  = bossReleaseSnow,
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
