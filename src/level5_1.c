// ===========================================================================
// level5_1.c - Scene 5: la AUTOPISTA (freeway) (26/09; arcade 12/10)
// ===========================================================================
// Tercer nivel con arte del proyecto del companero (Ray Project): la ruta con
// los edificios y el skyline de atras con parallax. Motor: stage_level.c en su
// variante con CAPA LEJANA:
//   - BG_A: la ruta (streameada, PAL0). Tiene 2369 tiles unicos, mas de los
//     que admite un IMAGE: va en el formato ancho de stage_bg (BIN de tiles y
//     mapa, ver tools/gen_level5_1.py). Su indice 0 es transparente: por los
//     huecos entre edificios se ve el skyline.
//   - BG_B: el skyline (bg_freeway_far_p0) a 1/3 de la camara, y en las
//     filas 0-3 el HUD con prioridad alta. (12/10) Va con PAL0: la imagen
//     ya trae los indices de la paleta de la ruta (tools/gen_level5_actors.py)
//     y PAL3 queda para el auto, las motos y los foot soldiers blancos.
//   - Color de fondo del VDP: el celeste del cielo (PAL0[4]).
//
// (12/10) GUION DEL ARCADE (gameplayARCADE/tmnt.avi, 830-1065 s; ver
// claude/autopista-arcade-oleadas-actores-12-10.md). La camara del arcade
// recorre 2058 px y la del port 1784: las X del video se escalan por 0,867.
// Ademas de las oleadas hay ACTORES (STAGE_SPECIAL en la tabla):
//   - AUTO (rojo / amarillo / azul) lleno de foot soldiers: cruza la ruta de
//     izquierda a derecha y los soldiers SALTAN de el mientras pasa; si el
//     auto se va y quedan, entran caminando por la izquierda. Atropella.
//   - MOTOS (2 o 3, una por carril): cruzan de izquierda a derecha y voltean
//     a la tortuga que pisan. Un golpe las hace explotar.
//   - El del MISIL / la GOMA: un morado que entra por la derecha con el
//     misil (o la goma) arriba de la cabeza, se alinea, lo tira y pasa a ser
//     un morado comun. (13/10) La bomba cae en arco y explota en el piso (la
//     explosion de Baxter); la goma cae, parpadea en el piso y desaparece.
//   - SP_WAIT: espera que no quede nadie (los blancos y el auto/motos se
//     pisarian PAL3: SIEMPRE un SP_WAIT entre unos y otros).
//   - La PIZZA tirada en la ruta (cuadro 6560 del tramo) con su globo.
// El nivel termina con las oleadas limpias al llegar al final (sin jefe) y
// sigue la Scene 6 (la parte de las patinetas).
// Musica (01/10): "13 - Highway Blockade (Scene 3-1)" (music_freeway).
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level5_1.h"          // pal_freeway, bg_freeway_*, fw_* (rescomp)
#include "stage_level.h"
#include "enemy.h"             // ENEMY_TYPE_*, sprVramFits
#include "enemies.h"           // explosion_sprite
#include "props_2_1.h"         // pizza_time_bubble
#include "audio.h"

#define SCREEN_W           320
#define LVL51_W            2104
#define LVL51_WALK_Y_MIN    112
#define LVL51_WALK_Y_MAX    216

static const SbgRaw road51 = {
    (const u32*) bg_freeway_tiles,
    (const u16*) bg_freeway_map,
    LVL51_W / 8, 28,
    &pal_freeway,
};

static s16 a16(s16 v) { return (v < 0) ? (s16)-v : v; }

// ===========================================================================
// ACTORES
// ===========================================================================
#define SP_WAIT        (STAGE_SPECIAL + 0)
#define SP_CAR_RED     (STAGE_SPECIAL + 1)
#define SP_CAR_BLUE    (STAGE_SPECIAL + 2)
#define SP_CAR_YELLOW  (STAGE_SPECIAL + 3)
#define SP_BIKES2      (STAGE_SPECIAL + 4)
#define SP_BIKES3      (STAGE_SPECIAL + 5)
#define SP_MISSILE     (STAGE_SPECIAL + 6)
#define SP_TIRE        (STAGE_SPECIAL + 7)

static void sfxHit(void) {
    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
}

// --- Explosiones (la del TNT, PAL2) -----------------------------------------
#define BOOM_MAX     3
#define BOOM_T      42      // 7 frames de 6
static struct { Sprite* s; s16 x, y; u16 t; } boom[BOOM_MAX];

static void boomAt(s16 cx, s16 feet) {
    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                   SOUND_PCM_CH3, 15, FALSE, FALSE);
    for (u16 i = 0; i < BOOM_MAX; i++) {
        if (boom[i].s) continue;
        if (!sprVramFits(explosion_sprite.maxNumTile)) return;
        boom[i].s = SPR_addSprite(&explosion_sprite, -64, -64, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        if (!boom[i].s) return;
        boom[i].x = (s16)(cx - 32); boom[i].y = (s16)(feet - 60); boom[i].t = 0;
        SPR_setDepth(boom[i].s, SPR_MIN_DEPTH + 1);
        return;
    }
}

static void boomUpdate(s16 camX) {
    for (u16 i = 0; i < BOOM_MAX; i++) {
        if (!boom[i].s) continue;
        if (++boom[i].t >= BOOM_T) { SPR_releaseSprite(boom[i].s); boom[i].s = NULL; continue; }
        SPR_setPosition(boom[i].s, (s16)(boom[i].x - camX), (s16)(boom[i].y - stageCamY));
    }
}

// ---------------------------------------------------------------------------
// AUTO con foot soldiers (fw_car_l / fw_car_r: dos mitades de 96x104, una anim
// por color). Arcade (cuadros 1880-1990): entra por la izquierda, cruza la
// pantalla en ~100 frames y los soldiers saltan mientras pasa.
// ---------------------------------------------------------------------------
#define CAR_W          192
#define CAR_FEET       176     // borde de abajo del sprite (mundo)
#define CAR_Q          896     // 3,5 px/f
#define CAR_LEAP_GAP    20     // frames entre un salto y el siguiente
#define CAR_LEAP_X0     40     // saltan con el centro del auto en pantalla en 40..280
#define CAR_LEAP_X1    280
#define CAR_HIT_TOP     44     // franja de pies que atropella, sobre CAR_FEET
#define CAR_HIT_BOT      6
#define CAR_HIT_X0      10
#define CAR_HIT_X1     182
#define CAR_BARS         2
enum { CAR_RED = 0, CAR_BLUE = 1, CAR_YELLOW = 2 };
// Los que saltan de cada auto (arcade): rojo 4 morados, azul 3 del martillo,
// amarillo 2 morados.
static const u8 carRiders[3]    = { 4, 3, 2 };
static const u8 carRiderType[3] = { ENEMY_TYPE_FOOT_SOLDIER, ENEMY_TYPE_FOOT_SOLDIER_HAMMER,
                                    ENEMY_TYPE_FOOT_SOLDIER };
static struct {
    u8 on; u8 color; s16 x; u8 acc; Sprite* s[2];
    u8 left;        // los que todavia tienen que saltar
    u8 gap; u8 hitMask;
} car;

static void carRelease(void) {
    for (u16 k = 0; k < 2; k++) if (car.s[k]) { SPR_releaseSprite(car.s[k]); car.s[k] = NULL; }
}

static bool carStart(u8 color, s16 camX) {
    if (car.on || car.left) return FALSE;
    if (!sprVramFits((u16)(fw_car_l.maxNumTile + fw_car_r.maxNumTile))) return FALSE;
    PAL_setPalette(PAL3, fw_car_l.palette->data, DMA);
    car.s[0] = SPR_addSprite(&fw_car_l, -128, -128, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    car.s[1] = car.s[0] ? SPR_addSprite(&fw_car_r, -128, -128, TILE_ATTR(PAL3, FALSE, FALSE, FALSE)) : NULL;
    if (!car.s[0] || !car.s[1]) { carRelease(); return FALSE; }
    for (u16 k = 0; k < 2; k++) {
        SPR_setAutoAnimation(car.s[k], FALSE);
        SPR_setAnimAndFrame(car.s[k], color, 0);
        SPR_setDepth(car.s[k], (s16)(-CAR_FEET - 8));
    }
    car.on = 1; car.color = color; car.x = (s16)(camX - CAR_W); car.acc = 0;
    car.left = carRiders[color]; car.gap = CAR_LEAP_GAP; car.hitMask = 0;
    return TRUE;
}

static void carUpdate(Player** pls, u8 nPl, s16 camX) {
    if (car.on) {
        u16 t = (u16)(car.acc + CAR_Q);
        car.acc = (u8)(t & 0xFF);
        car.x = (s16)(car.x + (s16)(t >> 8));
        // Los soldiers saltan del auto.
        s16 sc = (s16)(car.x + CAR_W / 2 - camX);
        if (car.gap) car.gap--;
        if (car.left && !car.gap && sc >= CAR_LEAP_X0 && sc <= CAR_LEAP_X1) {
            s16 lx = (s16)(car.x + 50 + (s16)(random() % 90));
            s16 ly = (s16)(CAR_FEET - 10 + (s16)(random() % 30));
            if (stageSpawnEnemyAt(carRiderType[car.color], STAGE_JUMP_IN, lx, ly)) {
                car.left--;
                car.gap = CAR_LEAP_GAP;
            }
        }
        // Atropella.
        for (u8 k = 0; k < nPl; k++) {
            if (car.hitMask & (1 << k)) continue;
            if (!playerCanBeHit(pls[k])) continue;
            s16 py = getPlayerY(pls[k]);
            if (py < CAR_FEET - CAR_HIT_TOP || py > CAR_FEET + CAR_HIT_BOT) continue;
            s16 hx = getPlayerHurtCX(pls[k]);
            if (hx < car.x + CAR_HIT_X0 || hx > car.x + CAR_HIT_X1) continue;
            playerHitBarsKnockdown(pls[k], (s16)(car.x + CAR_W / 2), CAR_BARS);
            sfxHit();
            car.hitMask |= (u8)(1 << k);
        }
        if (car.x > camX + SCREEN_W + 8) { carRelease(); car.on = 0; }
        else {
            for (u16 k = 0; k < 2; k++)
                SPR_setPosition(car.s[k], (s16)(car.x + k * 96 - camX),
                                (s16)(CAR_FEET - 104 - stageCamY));
        }
    } else if (car.left) {
        // El auto ya se fue: los que quedaban entran caminando por la izquierda.
        if (stageSpawnEnemyAt(carRiderType[car.color], -1, 0, 0)) car.left--;
    }
}

// ---------------------------------------------------------------------------
// MOTOS (fw_moto, 120x64, PAL3). Comparten los tiles: la primera los sube a
// la VRAM y las demas apuntan ahi (sin subir nada). La primera no se suelta
// hasta que terminan todas (si explota, se esconde).
// ---------------------------------------------------------------------------
#define BIKE_MAX        3
#define BIKE_W        120
#define BIKE_H         64
#define BIKE_Q       1024      // 4 px/f
#define BIKE_HIT_DY    10
#define BIKE_HIT_DX    44      // contra el centro de la moto
#define BIKE_BARS       2
#define BIKE_HALF_W    40      // la parte que se le puede pegar
#define BIKE_BODY_H    50
static struct { u8 on; s16 x, y; u8 acc; Sprite* s; u8 hitMask; } bike[BIKE_MAX];
static u8 bikesOn;

static void bikesRelease(void) {
    for (s16 i = BIKE_MAX - 1; i >= 0; i--) {
        if (bike[i].s) { SPR_releaseSprite(bike[i].s); bike[i].s = NULL; }
        bike[i].on = 0;
    }
    bikesOn = 0;
}

static bool bikesStart(u8 n, s16 camX) {
    if (bikesOn) return FALSE;
    if (!sprVramFits(fw_moto.maxNumTile)) return FALSE;
    PAL_setPalette(PAL3, fw_car_l.palette->data, DMA);
    static const s16 laneY[BIKE_MAX] = { 146, 178, 210 };
    static const s16 lag[BIKE_MAX]   = { 0, 56, 24 };
    u16 base = 0;
    for (u16 i = 0; i < n; i++) {
        s16 y = (n == 2) ? laneY[i * 2] : laneY[i];
        if (i == 0) {
            bike[i].s = SPR_addSprite(&fw_moto, -128, -128, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
            if (!bike[i].s) return FALSE;
            base = (u16)(bike[i].s->attribut & TILE_INDEX_MASK);
        } else {
            bike[i].s = SPR_addSpriteEx(&fw_moto, -128, -128,
                                        TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE, base),
                                        SPR_FLAG_AUTO_VISIBILITY);
            if (!bike[i].s) continue;
        }
        SPR_setAutoAnimation(bike[i].s, FALSE);
        bike[i].on = 1; bike[i].y = y; bike[i].acc = 0; bike[i].hitMask = 0;
        bike[i].x = (s16)(camX - BIKE_W - lag[i]);
        SPR_setDepth(bike[i].s, (s16)-y);
    }
    bikesOn = 1;
    return TRUE;
}

static void bikesUpdate(Player** pls, u8 nPl, s16 camX) {
    if (!bikesOn) return;
    // (12/10) Si SPR_defragVRAM mudo la primera moto (VRAM automatica), las
    // otras, que apuntan a sus tiles, la siguen.
    // Si la primera ya esta escondida SGDK no le vuelve a subir los tiles:
    // se suben a mano.
    static u16 bikeIdx0;
    if (bike[0].s) {
        u16 idx = bike[0].s->attribut & TILE_INDEX_MASK;
        if (idx != bikeIdx0 && !bike[0].on && bike[0].s->frame)
            VDP_loadTileSet(bike[0].s->frame->tileset, idx, DMA_QUEUE);
        bikeIdx0 = idx;
        for (u16 i = 1; i < BIKE_MAX; i++)
            if (bike[i].s && (bike[i].s->attribut & TILE_INDEX_MASK) != idx)
                SPR_setVRAMTileIndex(bike[i].s, idx);
    }
    bool any = FALSE;
    for (u16 i = 0; i < BIKE_MAX; i++) {
        if (!bike[i].on) continue;
        u16 t = (u16)(bike[i].acc + BIKE_Q);
        bike[i].acc = (u8)(t & 0xFF);
        bike[i].x = (s16)(bike[i].x + (s16)(t >> 8));
        s16 cx = (s16)(bike[i].x + BIKE_W / 2);
        // Un golpe la hace explotar.
        bool dead = FALSE;
        for (u8 k = 0; k < nPl; k++) {
            if (!playerAttackHitsBox(pls[k], cx, bike[i].y, BIKE_HALF_W, BIKE_BODY_H)) continue;
            sfxHit();
            boomAt(cx, bike[i].y);
            addPlayerScore(pls[k], 1);
            dead = TRUE;
            break;
        }
        if (!dead) {
            for (u8 k = 0; k < nPl; k++) {
                if (bike[i].hitMask & (1 << k)) continue;
                if (!playerCanBeHit(pls[k])) continue;
                if (a16((s16)(getPlayerY(pls[k]) - bike[i].y)) > BIKE_HIT_DY) continue;
                if (a16((s16)(getPlayerHurtCX(pls[k]) - cx)) > BIKE_HIT_DX) continue;
                playerHitBarsKnockdown(pls[k], cx, BIKE_BARS);
                sfxHit();
                bike[i].hitMask |= (u8)(1 << k);
            }
        }
        if (dead || bike[i].x > camX + SCREEN_W + 8) {
            bike[i].on = 0;
            // La primera tiene los tiles de todas: se esconde, no se suelta.
            if (i == 0) SPR_setVisibility(bike[i].s, HIDDEN);
            else if (bike[i].s) { SPR_releaseSprite(bike[i].s); bike[i].s = NULL; }
            continue;
        }
        any = TRUE;
        if (bike[i].s)
            SPR_setPosition(bike[i].s, (s16)(bike[i].x - camX), (s16)(bike[i].y - BIKE_H - stageCamY));
    }
    if (!any) bikesRelease();
}

// ---------------------------------------------------------------------------
// El del MISIL / la GOMA (fw_thrower, celdas de 64x104, PAL2; arcade cuadros
// 2650-2830 y 13530-13620). Entra con la cosa arriba de la cabeza, se alinea,
// la tira y pasa a ser un morado comun (stageSpawnEnemyAt STAGE_IN_PLACE).
// ---------------------------------------------------------------------------
#define THR_MAX          2
#define THR_W           64
#define THR_H          104
#define THR_WALK_Q     256     // 1 px/f
#define THR_LANE_Q     160
#define THR_RANGE      120     // tira con la tortuga a esto o menos ...
#define THR_ALIGN_DY     4     // ... en su carril
#define THR_WALK_MAX   240     // o a los 4 s caminando
#define THR_THROW_T     20
#define THR_RELEASE_T   10
#define THR_HP           2
#define THR_HIT_CD      14
#define THR_HALF_W      16
#define THR_BODY_H      70
#define THR_ANIM_MISSILE 0
#define THR_ANIM_TIRE    2
#define THR_ANIM_EMPTY   4
enum { THR_OFF = 0, THR_WALK, THR_THROW, THR_DONE };
static struct {
    u8 st, kind; s16 x, y; s8 dir; u16 t; u8 accX, accY, hp, cd; Sprite* s;
    s16 aimX;                    // (13/10) x de la tortuga: ahi apunta la bomba
} thr[THR_MAX];

// Proyectiles (13/10, como el arcade): los dos salen de la mano y caen en un
// arco (parabola en z, Q8). La BOMBA cae donde estaba la tortuga y al tocar
// el piso explota (la explosion de Baxter, ver bigBoom): ahi pega. La GOMA
// pega mientras vuela; al tocar el piso parpadea y desaparece, sin rebotar.
#define SHOT_MAX         3
#define SHOT_Z0         48      // altura de la mano
#define SHOT_VZ0       384      // 1,5 px/f hacia arriba (Q8)
#define SHOT_G          40      // gravedad (Q8 por frame)
#define SHOT_FLIGHT     36      // frames hasta el piso con esos valores
#define SHOT_DIST_MIN   40
#define SHOT_DIST_MAX  150
#define TIRE_VX_Q      640      // 2,5 px/f: la goma no apunta
#define TIRE_GROUND_T   32      // parpadea en el piso y se va
#define SHOT_HIT_DX     22
#define SHOT_HIT_DY     10
#define SHOT_BARS        2
#define BOMB_HIT_DX     40      // el radio de la explosion
#define BOMB_HIT_DY     16
#define BOMB_HIT_Z      40
static struct {
    u8 on, kind, ground; s16 x, y; s8 dir; u16 vxq; u8 acc; u16 t;
    s32 zq; s16 vzq; Sprite* s;
} shot[SHOT_MAX];

// --- La explosion de la bomba: la de Baxter (fw_bomb_boom, PAL2) ------------
// Los dos cuartos de la IZQUIERDA (arriba = anim 0, abajo = anim 1) con su
// arte, y los de la derecha espejados apuntando a sus tiles: 128 tiles. Si no
// entran, el de arriba y 3 espejos (64). Si SPR_defragVRAM muda a un dueno,
// sus espejos lo siguen (bigBoomUpdate). Una a la vez; si hay otra en curso o
// no hay VRAM, sale la explosion chica de siempre.
// Cuartos: 0 arriba-izq, 1 arriba-der, 2 abajo-izq, 3 abajo-der.
#define BIGBOOM_FRAMES   8
#define BIGBOOM_TICKS    4      // 8 x 4 = ~0,5 s
#define BIGBOOM_HALF    64
static struct { Sprite* s[4]; u8 owner[4]; s16 cx, cy; u8 frame, tick; } bigBoom;

static void bigBoomRelease(void) {
    for (u16 q = 0; q < 4; q++)
        if (bigBoom.s[q]) { SPR_releaseSprite(bigBoom.s[q]); bigBoom.s[q] = NULL; }
}

static Sprite* bigBoomMaster(u16 anim) {
    Sprite* s = SPR_addSprite(&fw_bomb_boom, -128, -128, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (s) SPR_setAnimAndFrame(s, (s16)anim, 0);
    return s;
}

static Sprite* bigBoomMirror(const Sprite* m, u16 anim, bool hf, bool vf) {
    if (!m) return NULL;
    Sprite* s = SPR_addSpriteEx(&fw_bomb_boom, -128, -128,
                                TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE,
                                               m->attribut & TILE_INDEX_MASK), 0);
    if (!s) return NULL;
    SPR_setAnimAndFrame(s, (s16)anim, 0);
    SPR_setHFlip(s, hf);
    SPR_setVFlip(s, vf);
    return s;
}

static void bigBoomAt(s16 cx, s16 feet) {
    const u16 T = fw_bomb_boom.maxNumTile;
    if (bigBoom.s[0] || !sprVramFits(T)) { boomAt(cx, feet); return; }
    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                   SOUND_PCM_CH3, 15, FALSE, FALSE);
    Sprite** sp = bigBoom.s;
    sp[0] = bigBoomMaster(0);
    if (!sp[0]) return;
    if (SPR_getLargestFreeVRAMBlock() >= T) sp[2] = bigBoomMaster(1);
    if (sp[2]) {                                   // mitad izquierda propia
        sp[1] = bigBoomMirror(sp[0], 0, TRUE, FALSE);
        sp[3] = bigBoomMirror(sp[2], 1, TRUE, FALSE);
        bigBoom.owner[1] = 0; bigBoom.owner[2] = 2; bigBoom.owner[3] = 2;
    } else {                                       // un cuarto y 3 espejos
        sp[1] = bigBoomMirror(sp[0], 0, TRUE,  FALSE);
        sp[2] = bigBoomMirror(sp[0], 0, FALSE, TRUE);
        sp[3] = bigBoomMirror(sp[0], 0, TRUE,  TRUE);
        bigBoom.owner[1] = bigBoom.owner[2] = bigBoom.owner[3] = 0;
    }
    bigBoom.owner[0] = 0;
    for (u16 q = 0; q < 4; q++) if (sp[q]) {
        SPR_setAutoAnimation(sp[q], FALSE);
        SPR_setDepth(sp[q], SPR_MIN_DEPTH + 1);
    }
    bigBoom.cx = cx; bigBoom.cy = (s16)(feet - 40);   // el centro del fuego
    bigBoom.frame = 0; bigBoom.tick = 0;
}

static void bigBoomUpdate(s16 camX) {
    if (!bigBoom.s[0]) return;
    if (++bigBoom.tick >= BIGBOOM_TICKS) {
        bigBoom.tick = 0;
        if (++bigBoom.frame >= BIGBOOM_FRAMES) { bigBoomRelease(); return; }
        for (u16 q = 0; q < 4; q++) if (bigBoom.s[q]) SPR_setFrame(bigBoom.s[q], bigBoom.frame);
    }
    for (u16 q = 1; q < 4; q++) {
        Sprite* m = bigBoom.s[bigBoom.owner[q]];
        if (!bigBoom.s[q] || !m || bigBoom.owner[q] == q) continue;
        u16 idx = m->attribut & TILE_INDEX_MASK;
        if ((bigBoom.s[q]->attribut & TILE_INDEX_MASK) != idx) SPR_setVRAMTileIndex(bigBoom.s[q], idx);
    }
    s16 x0 = (s16)(bigBoom.cx - BIGBOOM_HALF - camX);
    s16 y0 = (s16)(bigBoom.cy - BIGBOOM_HALF - stageCamY);
    for (u16 q = 0; q < 4; q++)
        if (bigBoom.s[q])
            SPR_setPosition(bigBoom.s[q], (s16)(x0 + (q & 1) * BIGBOOM_HALF),
                                          (s16)(y0 + (q >> 1) * BIGBOOM_HALF));
}

static void shotFire(u8 kind, s16 x, s16 y, s8 dir, s16 dist) {
    for (u16 i = 0; i < SHOT_MAX; i++) {
        if (shot[i].on) continue;
        const SpriteDefinition* d = kind ? &fw_tire : &fw_missile;
        if (!sprVramFits(d->maxNumTile)) return;
        shot[i].s = SPR_addSprite(d, -128, -128, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        if (!shot[i].s) return;
        SPR_setHFlip(shot[i].s, dir > 0);         // la punta de la bomba mira a la izquierda
        if (dist < SHOT_DIST_MIN) dist = SHOT_DIST_MIN;
        if (dist > SHOT_DIST_MAX) dist = SHOT_DIST_MAX;
        shot[i].on = 1; shot[i].kind = kind; shot[i].ground = 0;
        shot[i].x = x; shot[i].y = y; shot[i].dir = dir; shot[i].acc = 0; shot[i].t = 0;
        // la bomba llega justo a la tortuga en SHOT_FLIGHT frames
        shot[i].vxq = kind ? TIRE_VX_Q : (u16)((dist * 256) / SHOT_FLIGHT);
        shot[i].zq = (s32)SHOT_Z0 << 8; shot[i].vzq = SHOT_VZ0;
        return;
    }
}

static void shotKill(u16 i) {
    if (shot[i].s) { SPR_releaseSprite(shot[i].s); shot[i].s = NULL; }
    shot[i].on = 0;
}

static void shotUpdate(Player** pls, u8 nPl, s16 camX) {
    for (u16 i = 0; i < SHOT_MAX; i++) {
        if (!shot[i].on) continue;
        shot[i].t++;
        if (shot[i].ground) {               // la goma en el piso: parpadea y se va
            if (shot[i].t >= TIRE_GROUND_T) { shotKill(i); continue; }
            SPR_setVisibility(shot[i].s, (shot[i].t & 2) ? HIDDEN : VISIBLE);
            SPR_setPosition(shot[i].s, (s16)(shot[i].x - 32 - camX), (s16)(shot[i].y - 56 - stageCamY));
            continue;
        }
        u16 t = (u16)(shot[i].acc + shot[i].vxq);
        shot[i].acc = (u8)(t & 0xFF);
        shot[i].x = (s16)(shot[i].x + shot[i].dir * (s16)(t >> 8));
        shot[i].zq += shot[i].vzq;
        shot[i].vzq = (s16)(shot[i].vzq - SHOT_G);
        s16 z = (s16)(shot[i].zq >> 8);
        if (z <= 0) {                       // toco el piso
            if (shot[i].kind == 0) {        // la bomba explota y pega alrededor
                bigBoomAt(shot[i].x, shot[i].y);
                for (u8 k = 0; k < nPl; k++) {
                    if (!playerCanBeHitAir(pls[k])) continue;
                    if (a16((s16)(getPlayerY(pls[k]) - shot[i].y)) > BOMB_HIT_DY) continue;
                    if (a16((s16)(getPlayerHurtCX(pls[k]) - shot[i].x)) > BOMB_HIT_DX) continue;
                    if (getPlayerJumpZ(pls[k]) > BOMB_HIT_Z) continue;
                    playerHitBarsKnockdown(pls[k], shot[i].x, SHOT_BARS);
                    sfxHit();
                }
                shotKill(i);
                continue;
            }
            shot[i].ground = 1; shot[i].t = 0;   // la goma: queda ahi
            SPR_setPosition(shot[i].s, (s16)(shot[i].x - 32 - camX), (s16)(shot[i].y - 56 - stageCamY));
            continue;
        }
        bool gone = (shot[i].x < camX - 80 || shot[i].x > camX + SCREEN_W + 80);
        // La goma pega en el aire (saltando bien alto se la esquiva).
        for (u8 k = 0; k < nPl && !gone && shot[i].kind; k++) {
            if (!playerCanBeHitAir(pls[k])) continue;
            if (a16((s16)(getPlayerY(pls[k]) - shot[i].y)) > SHOT_HIT_DY) continue;
            if (a16((s16)(getPlayerHurtCX(pls[k]) - shot[i].x)) > SHOT_HIT_DX) continue;
            if (getPlayerJumpZ(pls[k]) > z + 20) continue;
            playerHitBarsKnockdown(pls[k], shot[i].x, SHOT_BARS);
            sfxHit();
            shot[i].ground = 1; shot[i].t = 0;   // rebota en la tortuga y cae ahi
        }
        if (gone) { shotKill(i); continue; }
        s16 sy = shot[i].kind ? (s16)(shot[i].y - z - 56) : (s16)(shot[i].y - z - 16);
        SPR_setPosition(shot[i].s, (s16)(shot[i].x - 32 - camX), (s16)(sy - stageCamY));
        SPR_setDepth(shot[i].s, (s16)(-shot[i].y - 1));
    }
}

static bool thrStart(u8 kind, s8 side, s16 camX) {
    for (u16 i = 0; i < THR_MAX; i++) {
        if (thr[i].st != THR_OFF) continue;
        if (!sprVramFits(fw_thrower.maxNumTile)) return FALSE;
        thr[i].s = SPR_addSprite(&fw_thrower, -128, -128, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        if (!thr[i].s) return FALSE;
        SPR_setAutoAnimation(thr[i].s, FALSE);
        thr[i].st = THR_WALK; thr[i].kind = kind; thr[i].t = 0;
        thr[i].dir = (side < 0) ? 1 : -1;
        thr[i].x = (side < 0) ? (s16)(camX - 24) : (s16)(camX + SCREEN_W + 24);
        thr[i].y = (s16)(LVL51_WALK_Y_MIN + 20 +
                         (s16)(random() % (LVL51_WALK_Y_MAX - LVL51_WALK_Y_MIN - 30)));
        thr[i].accX = thr[i].accY = 0; thr[i].hp = THR_HP; thr[i].cd = 0;
        return TRUE;
    }
    return FALSE;
}

static void thrRelease(u16 i) {
    if (thr[i].s) { SPR_releaseSprite(thr[i].s); thr[i].s = NULL; }
    thr[i].st = THR_OFF;
}

static void thrUpdate(Player** pls, u8 nPl, s16 camX) {
    for (u16 i = 0; i < THR_MAX; i++) {
        if (thr[i].st == THR_OFF) continue;
        // La tortuga mas cercana.
        Player* tg = NULL; s16 best = 0x7FFF;
        for (u8 k = 0; k < nPl; k++) {
            if (isPlayerGameOver(pls[k])) continue;
            s16 d = a16((s16)(getPlayerHurtCX(pls[k]) - thr[i].x));
            if (d < best) { best = d; tg = pls[k]; }
        }
        // Golpes de las tortugas: con THR_HP explota.
        if (thr[i].cd) thr[i].cd--;
        else for (u8 k = 0; k < nPl; k++) {
            if (!playerAttackHitsBox(pls[k], thr[i].x, thr[i].y, THR_HALF_W, THR_BODY_H)) continue;
            sfxHit();
            thr[i].cd = THR_HIT_CD;
            if (--thr[i].hp == 0) {
                boomAt(thr[i].x, thr[i].y);
                addPlayerScore(pls[k], 1);
                thrRelease(i);
            }
            break;
        }
        if (thr[i].st == THR_OFF) continue;
        thr[i].t++;
        u8 base = thr[i].kind ? THR_ANIM_TIRE : THR_ANIM_MISSILE;
        if (thr[i].st == THR_WALK) {
            if (tg) {
                s16 tx = getPlayerHurtCX(tg), ty = getPlayerY(tg);
                thr[i].aimX = tx;
                thr[i].dir = (tx < thr[i].x) ? -1 : 1;
                u16 t = (u16)(thr[i].accX + THR_WALK_Q);
                thr[i].accX = (u8)(t & 0xFF);
                // Camina hasta quedar a THR_RANGE (y siempre hasta entrar bien
                // en pantalla, aunque la tortuga este cerca del borde).
                bool outside = (bool)(thr[i].x < camX + 40 || thr[i].x > camX + SCREEN_W - 40);
                if (outside) thr[i].dir = (thr[i].x < camX + SCREEN_W / 2) ? 1 : -1;
                if (outside || a16((s16)(tx - thr[i].x)) > THR_RANGE)
                    thr[i].x = (s16)(thr[i].x + thr[i].dir * (s16)(t >> 8));
                if (!outside) thr[i].dir = (tx < thr[i].x) ? -1 : 1;
                t = (u16)(thr[i].accY + THR_LANE_Q);
                thr[i].accY = (u8)(t & 0xFF);
                s16 vy = (s16)(t >> 8);
                if (thr[i].y < ty) thr[i].y = (s16)((ty - thr[i].y < vy) ? ty : thr[i].y + vy);
                else if (thr[i].y > ty) thr[i].y = (s16)((thr[i].y - ty < vy) ? ty : thr[i].y - vy);
                bool inView = (bool)(thr[i].x >= camX + 40 && thr[i].x <= camX + SCREEN_W - 40);
                bool ready  = (bool)(a16((s16)(tx - thr[i].x)) <= THR_RANGE &&
                                     a16((s16)(ty - thr[i].y)) <= THR_ALIGN_DY);
                if (inView && (ready || thr[i].t >= THR_WALK_MAX)) {
                    thr[i].st = THR_THROW; thr[i].t = 0;
                }
            }
            SPR_setAnimAndFrame(thr[i].s, base, (thr[i].t >> 3) & 1);
        } else if (thr[i].st == THR_THROW) {
            if (thr[i].t < THR_RELEASE_T) SPR_setAnimAndFrame(thr[i].s, (u8)(base + 1), 0);
            else SPR_setAnimAndFrame(thr[i].s, THR_ANIM_EMPTY, 0);
            if (thr[i].t == THR_RELEASE_T)
                shotFire(thr[i].kind, (s16)(thr[i].x + thr[i].dir * 24), thr[i].y, thr[i].dir,
                         a16((s16)(thr[i].aimX - thr[i].x - thr[i].dir * 24)));
            if (thr[i].t >= THR_THROW_T) thr[i].st = THR_DONE;
        } else {   // THR_DONE: pasa a ser un morado comun apenas haya lugar
            SPR_setAnimAndFrame(thr[i].s, THR_ANIM_EMPTY, 0);
            if (stageSpawnEnemyAt(ENEMY_TYPE_FOOT_SOLDIER, STAGE_IN_PLACE, thr[i].x, thr[i].y)) {
                thrRelease(i);
                continue;
            }
        }
        SPR_setHFlip(thr[i].s, thr[i].dir > 0);   // (13/10) el arte mira a la izquierda
        SPR_setPosition(thr[i].s, (s16)(thr[i].x - THR_W / 2 - camX), (s16)(thr[i].y - THR_H - stageCamY));
        SPR_setDepth(thr[i].s, (s16)-thr[i].y);
    }
}

// ---------------------------------------------------------------------------
// PIZZA tirada en la ruta (arcade cuadro 6560: X 1595 del arcade -> 1383).
// El globo "Pizza time!" sale cuando la camara la deja a ~240 px del borde
// izquierdo (arcade 7380). La que la toca recupera toda la vida.
// ---------------------------------------------------------------------------
#define PZ_X          1383     // celda (mundo)
#define PZ_FEET        200
#define PZ_FEET_DY      46
#define PZ_BOX_X0        4
#define PZ_BOX_X1       35
#define PZ_PICK_DX      14
#define PZ_PICK_DY      14
#define PZ_OPEN_T        8
#define PZ_SHOW_T       68
#define PZ_BUB_CAM    (PZ_X - 240)
#define PZ_BUB_DX     (-56)
#define PZ_BUB_DY     (-10)
#define PZ_BUB_T        64
enum { PZ_WAIT = 0, PZ_OPEN, PZ_DONE };
static u8 pzSt; static u16 pzT; static Sprite* pzSpr;
static u8 pzBubSt; static u16 pzBubT; static Sprite* pzBub;

static void pizzaRelease(void) {
    if (pzSpr) SPR_releaseSprite(pzSpr);
    if (pzBub) SPR_releaseSprite(pzBub);
    pzSpr = pzBub = NULL;
}

static void pizzaUpdate(Player** pls, u8 nPl, s16 camX) {
    if (pzSt == PZ_DONE) return;
    s16 sx = (s16)(PZ_X - camX);
    bool near = (bool)(sx > -48 && sx < SCREEN_W + 8);
    if (!near) {
        if (pzSpr) { SPR_releaseSprite(pzSpr); pzSpr = NULL; }
        if (pzSt == PZ_OPEN) { pizzaRelease(); pzSt = PZ_DONE; }
        return;
    }
    if (!pzSpr) {
        if (!sprVramFits(fw_pizza.maxNumTile)) return;
        pzSpr = SPR_addSprite(&fw_pizza, -128, -128, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
        if (!pzSpr) return;
        SPR_setAutoAnimation(pzSpr, FALSE);
    }
    if (pzSt == PZ_WAIT) {
        if (pzBubSt == 0 && camX >= PZ_BUB_CAM && sprVramFits(pizza_time_bubble.maxNumTile)) {
            pzBub = SPR_addSprite(&pizza_time_bubble, -128, -128, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
            if (pzBub) {
                SPR_setDepth(pzBub, SPR_MIN_DEPTH);
                XGM2_playPCMEx(pizza_time_vo, sizeof(pizza_time_vo), SOUND_PCM_CH1, 15, FALSE, FALSE);
                pzBubSt = 1; pzBubT = 0;
            }
        }
        for (u8 k = 0; k < nPl; k++) {
            Player* p = pls[k];
            if (isPlayerGameOver(p) || p->state == STATE_KO) continue;
            s16 fx = (s16)(p->x + PLAYER_SPRITE_W / 2);
            if (fx < PZ_X + PZ_BOX_X0 - PZ_PICK_DX || fx > PZ_X + PZ_BOX_X1 + PZ_PICK_DX) continue;
            if (a16((s16)(p->y - PZ_FEET)) > PZ_PICK_DY) continue;
            p->health = PLAYER_MAX_HEALTH;
            pzSt = PZ_OPEN; pzT = 0;
            break;
        }
    } else if (++pzT >= 2 * PZ_OPEN_T + PZ_SHOW_T) {
        pizzaRelease();
        pzSt = PZ_DONE;
        return;
    }
    if (pzBubSt == 1) {
        if (++pzBubT >= PZ_BUB_T) { SPR_releaseSprite(pzBub); pzBub = NULL; pzBubSt = 2; }
        else SPR_setPosition(pzBub, (s16)(sx + PZ_BUB_DX),
                             (s16)(PZ_FEET - PZ_FEET_DY + PZ_BUB_DY - stageCamY));
    }
    s16 f = (pzSt == PZ_OPEN) ? ((pzT < PZ_OPEN_T) ? 1 : (pzT < 2 * PZ_OPEN_T) ? 2 : 3) : 0;
    SPR_setAnimAndFrame(pzSpr, 0, f);
    SPR_setPosition(pzSpr, sx, (s16)(PZ_FEET - PZ_FEET_DY - stageCamY));
    SPR_setDepth(pzSpr, (s16)-PZ_FEET);
}

// ---------------------------------------------------------------------------
// Ganchos del nivel
// ---------------------------------------------------------------------------
static void lvl51Init(void) {
    car.on = 0; car.left = 0; car.s[0] = car.s[1] = NULL;
    for (u16 i = 0; i < BIKE_MAX; i++) { bike[i].on = 0; bike[i].s = NULL; }
    bikesOn = 0;
    for (u16 i = 0; i < THR_MAX; i++) { thr[i].st = THR_OFF; thr[i].s = NULL; }
    for (u16 i = 0; i < SHOT_MAX; i++) { shot[i].on = 0; shot[i].s = NULL; }
    for (u16 i = 0; i < BOOM_MAX; i++) boom[i].s = NULL;
    for (u16 q = 0; q < 4; q++) bigBoom.s[q] = NULL;
    pzSt = PZ_WAIT; pzT = 0; pzSpr = NULL; pzBubSt = 0; pzBubT = 0; pzBub = NULL;
}

static void lvl51Release(void) {
    carRelease(); car.on = 0; car.left = 0;
    bikesRelease();
    for (u16 i = 0; i < THR_MAX; i++) thrRelease(i);
    for (u16 i = 0; i < SHOT_MAX; i++)
        if (shot[i].s) { SPR_releaseSprite(shot[i].s); shot[i].s = NULL; shot[i].on = 0; }
    for (u16 i = 0; i < BOOM_MAX; i++)
        if (boom[i].s) { SPR_releaseSprite(boom[i].s); boom[i].s = NULL; }
    bigBoomRelease();
    pizzaRelease();
}

static void lvl51Update(Player** pls, u8 nPl, s16 camX) {
    carUpdate(pls, nPl, camX);
    bikesUpdate(pls, nPl, camX);
    thrUpdate(pls, nPl, camX);
    shotUpdate(pls, nPl, camX);
    boomUpdate(camX);
    bigBoomUpdate(camX);
    pizzaUpdate(pls, nPl, camX);
}

static u16 thrActive(void) {
    u16 n = 0;
    for (u16 i = 0; i < THR_MAX; i++) if (thr[i].st != THR_OFF) n++;
    return n;
}

static u16 lvl51SpecialAlive(void) {
    return (u16)((car.on ? 1 : 0) + car.left + (bikesOn ? 1 : 0) + thrActive());
}

static bool lvl51Special(u8 type, s8 side, s16 x, s16 camX) {
    (void)x;
    switch (type) {
    case SP_WAIT:       return (bool)(stageAliveEnemies() == 0 && !car.on && !car.left &&
                                      !bikesOn && thrActive() == 0);
    case SP_CAR_RED:    return carStart(CAR_RED, camX);
    case SP_CAR_BLUE:   return carStart(CAR_BLUE, camX);
    case SP_CAR_YELLOW: return carStart(CAR_YELLOW, camX);
    case SP_BIKES2:     return bikesStart(2, camX);
    case SP_BIKES3:     return bikesStart(3, camX);
    case SP_MISSILE:    return thrStart(0, side, camX);
    case SP_TIRE:       return thrStart(1, side, camX);
    default:            return TRUE;
    }
}

// ===========================================================================
// OLEADAS (12/10, del video del arcade; X del arcade x 0,867)
// ===========================================================================
// lockX = la camara del arcade donde se clavo; trigX = lockX + 180. Los
// blancos en monociclo del arcade (no hay arte) van como blancos de espada.
#define P   ENEMY_TYPE_FOOT_SOLDIER
#define W   ENEMY_TYPE_FOOT_SOLDIER_WHITE
#define H   ENEMY_TYPE_FOOT_SOLDIER_HAMMER
#define S   ENEMY_TYPE_FOOT_SOLDIER_SPEAR
#define WT  SP_WAIT
#define CR  SP_CAR_RED
#define CB  SP_CAR_BLUE
#define CY  SP_CAR_YELLOW
#define B2  SP_BIKES2
#define B3  SP_BIKES3
#define MS  SP_MISSILE
#define TR  SP_TIRE
static const StageWave waves51[] = {
    // A (arcade cam 82-101): los del martillo
    {  120,   88, 3, { H, H, H },                    { +1, -1, +1 }, { 0 } },
    // B (189): lanzas
    {  344,  164, 3, { S, S, S },                    { +1, +1, -1 }, { 0 } },
    // C (293): el auto ROJO (4 morados saltan)
    {  434,  254, 1, { CR },                         { 0 }, { 0 } },
    // D (397-414): dos con misil, blancos y morados
    {  539,  359, 7, { MS, MS, W, P, W, P, W },      { +1, +1, -1, +1, -1, +1, +1 }, { 0 } },
    // E (813): tres motos y despues blancos
    {  885,  705, 5, { WT, B3, WT, W, W },           { 0, 0, 0, +1, -1 }, { 0 } },
    // F (993): dos motos, misiles y morados
    { 1041,  861, 8, { WT, B2, MS, P, P, MS, P, P }, { 0, 0, +1, +1, -1, +1, -1, +1 }, { 0 } },
    // G (1134): el auto AMARILLO con dos motos al lado
    { 1163,  983, 3, { WT, B2, CY },                 { 0 }, { 0 } },
    // H (1176): blancos
    { 1200, 1020, 4, { WT, W, W, W },                { 0, -1, +1, -1 }, { 0 } },
    // I (1303): misil, martillos y morados (la pizza esta aca)
    { 1310, 1130, 6, { MS, H, P, H, P, H },          { +1, +1, -1, +1, +1, -1 }, { 0 } },
    // J (1361): martillos
    { 1360, 1180, 3, { H, H, H },                    { +1, -1, +1 }, { 0 } },
    // K (1448): lanzas
    { 1435, 1255, 4, { S, S, S, S },                 { +1, +1, -1, +1 }, { 0 } },
    // L (1548): blancos, martillos y un morado
    { 1522, 1342, 7, { W, H, P, W, W, H, W },        { +1, +1, -1, -1, +1, +1, -1 }, { 0 } },
    // M (1688): el auto AZUL (3 del martillo)
    { 1643, 1463, 2, { WT, CB },                     { 0 }, { 0 } },
    // N (1968): la barricada, todos blancos
    { 1886, 1706, 8, { W, W, W, W, W, W, W, W },     { -1, -1, +1, -1, +1, -1, +1, -1 }, { 0 } },
    // O (2058): los de las gomas
    { 1964, 1784, 4, { TR, TR, P, P },               { +1, +1, +1, -1 }, { 0 } },
};
#undef P
#undef W
#undef H
#undef S
#undef WT
#undef CR
#undef CB
#undef CY
#undef B2
#undef B3
#undef MS
#undef TR

static const StageLevel level51 = {
    .bg           = NULL,
    .bgRaw        = &road51,
    .fg           = NULL,
    .far          = &bg_freeway_far,
    .farDiv       = 3,
    .farRowShift  = 4,              // el skyline asoma justo arriba del guardarrail
    .farPal0      = TRUE,           // (12/10) PAL3 queda para el auto y las motos
    .pal3         = NULL,           // la carga cada actor al aparecer
    .backdrop     = 4,              // PAL0[4]: el celeste del cielo
    .bgSlots      = 736,            // >= 715, el peor caso medido
    .levelW       = LVL51_W,
    .walkTop      = NULL,
    .walkCols     = 0,
    .walkYMin     = LVL51_WALK_Y_MIN,   // borde de abajo del guardarrail
    .walkYMax     = LVL51_WALK_Y_MAX,
    .waves        = waves51,
    .nWaves       = sizeof(waves51) / sizeof(waves51[0]),
    .bossFeetX    = 2000,
    .music        = music_freeway,
    .musicVol     = 80,
    .levelInit    = lvl51Init,
    .levelUpdate  = lvl51Update,
    .levelRelease = lvl51Release,
    .specialSpawn = lvl51Special,
    .specialAlive = lvl51SpecialAlive,
    .nextScene    = SCENE_6_1_TITLE,
};

SceneId showScene51() {
    return stageLevelRun(&level51);
}
