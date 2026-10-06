// ===========================================================================
// level4_1.c - Scene 4: el ESTACIONAMIENTO (garage) (26/09; fondo y props 01/10)
// ===========================================================================
// El motor es stage_level.c, el mismo de la cloaca: aca van los datos, los
// PROPS del estacionamiento y los dos jefes.
//
// FONDO (01/10): bg_garage_new.png, armado por
// tools/gen_level4_1_bg.py (ver el encabezado del script). Mismo encuadre que
// el de Ray mas 16 px de piso abajo: la camara baja hasta ahi (camYMax). El
// AUTO estacionado y la PUERTA del ascensor estan pintados en el fondo; cuando
// tienen que moverse se cambia esa region del fondo por otra variante
// (sbgSetVariant): asi no ocupan VRAM de sprites mientras estan quietos.
//
// PROPS (ubicados segun bg_garage_new_assets_ubication.png):
//   - Cartel "20 MPH": al golpearlo queda torcido (frame 1). Nada mas.
//   - Cartel "ONE WAY": al golpearlo da una vuelta entera y vuelve al frame 0.
//     Se puede volver a golpear.
//   - Conos: al golpearlos salen volando en X hacia donde los empujo el golpe
//     hasta salir de camara. Les pegan a los foot soldiers que cruzan.
//   - Barriles explosivos: al golpearlos se prende la mecha (frames 1..4, dos
//     vueltas) y explotan (la explosion del misil del sewer). La explosion
//     lastima a las tortugas y mata a los soldiers que agarra.
//   - El AUTO: cuando una tortuga se le acerca, sale del lugar POR DELANTE de
//     todo (el sprite reemplaza al dibujo del fondo) hacia abajo del nivel,
//     hasta salir de camara. Si atropella a una tortuga, la tira y le saca
//     vida.
// Los props son sprites que existen solo cerca de camara (como los
// parquimetros del 2-1), con la paleta del fondo (PAL0).
//
// JEFES: al llegar al fondo se abre la PERSIANA del ascensor y salen BEBOP y
// ROCKSTEADY. Juntos son 210 + 169 tiles de sprites: con una tortuga entran
// los dos a la vez; con dos tortugas no hay VRAM, asi que Rocksteady espera
// en el ascensor y sale cuando hay lugar (en la practica, cuando cae Bebop).
// (03/10) Bebop y Rocksteady comparten PALETA (las hojas nuevas):
// los dos se dibujan en PAL3 (la carga Bebop al salir), con April. El
// parpadeo de vida baja NO toca PAL3: al salir Bebop se carga en PAL2 la
// version quemada de la paleta compartida (en las peleas con jefes nunca hay
// foot soldiers) y el jefe con poca vida pasa SU sprite a PAL2 cada 4 frames
// (flashPal de las arenas, boss_flash.h). Asi parpadea solo ese jefe.
//
// Musica (01/10): "11 - Parking Garage (Scene 2-3)" (music_garage).
// Al ganar: Scene 5 (la autopista).
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level4_1.h"          // pal_garage, bg_garage_*, garage_* (rescomp)
#include "level4_1_bg.h"       // regiones y posiciones (generado)
#include "stage_level.h"
#include "enemy.h"             // ENEMY_TYPE_*, sprVramFits
#include "bebop.h"
#include "rocksteady.h"
#include "audio.h"
#include "boss_vo.h"           // (01/10)

#define SCREEN_W            320
#define LVL41_W            1288
#define LVL41_CAM_MAX_X    (LVL41_W - SCREEN_W)     // 968
#define LVL41_WALK_Y_MIN    128
#define LVL41_WALK_Y_MAX    232      // (01/10) 16 px mas de piso que el de Ray
#define LVL41_CAM_Y_MAX      16

static const SbgRaw garage41 = {
    (const u32*) bg_garage_tiles,
    (const u16*) bg_garage_map,
    LVL41_BG_W, LVL41_BG_ROWS,
    &pal_garage,
};

static const SbgRegion garageRegs[2] = {
    { LVL41_REG_CAR_C0, LVL41_REG_CAR_R0, LVL41_REG_CAR_W, LVL41_REG_CAR_H,
      LVL41_REG_CAR_NVAR, (const u16*) bg_garage_var + LVL41_REG_CAR_OFF },
    { LVL41_REG_DOOR_C0, LVL41_REG_DOOR_R0, LVL41_REG_DOOR_W, LVL41_REG_DOOR_H,
      LVL41_REG_DOOR_NVAR, (const u16*) bg_garage_var + LVL41_REG_DOOR_OFF },
};

// ---------------------------------------------------------------------------
// OLEADAS
// ---------------------------------------------------------------------------
// (06/10) Los de la LANZA de entrada y a mitad, y los del FUSIL al final,
// como en el remaster de PC.
#define P ENEMY_TYPE_FOOT_SOLDIER
#define O ENEMY_TYPE_FOOT_SOLDIER_ORANGE
#define G ENEMY_TYPE_FOOT_SOLDIER_GUN
#define S ENEMY_TYPE_FOOT_SOLDIER_SPEAR
static const StageWave waves41[] = {
    {  160,   0, 3, { S, P, S },    { +1, -1, +1 } },
    {  440, 260, 3, { O, S, P },    { -1, +1, +1 } },
    {  720, 540, 4, { G, O, G, O }, { +1, -1, -1, +1 } },
    {  980, 800, 4, { O, P, G, P }, { -1, +1, +1, -1 } },
};
#undef P
#undef O
#undef G
#undef S

// ===========================================================================
// PROPS
// ===========================================================================
typedef enum { PK_SIGN20, PK_ONEWAY, PK_CONE, PK_BARREL } PropKind;

typedef struct {
    u8  kind;
    s16 x;          // borde izquierdo de la celda del sprite (mundo)
    s16 base;       // borde de abajo de la celda = la base del prop (pies)
} PropDef;

// Posiciones medidas sobre bg_garage_new_assets_ubication.png (esquina del
// arte por coincidencia de pixeles) menos el relleno de la celda.
static const PropDef propDefs[] = {
    { PK_ONEWAY, 269, 227 },
    { PK_CONE,   304, 152 },
    { PK_CONE,   339, 186 },
    { PK_CONE,   378, 227 },
    { PK_BARREL, 460, 152 },
    { PK_SIGN20, 931, 205 },
    { PK_BARREL, 958, 157 },
};
#define N_PROPS (sizeof(propDefs) / sizeof(propDefs[0]))

typedef struct {
    const SpriteDefinition* def;
    s16 w, h;           // celda
    s16 hitDx;          // centro de la hurtbox respecto de x
    s16 halfW, bodyH;   // hurtbox (contra playerAttackHitsBox)
} PropInfo;

static const PropInfo propInfo[4] = {
    { &garage_sign20, 48, 96, 22, 10, 90 },
    { &garage_oneway, 56, 96, 25, 10, 90 },
    { &garage_cone,   24, 32, 12,  9, 24 },
    { &garage_barrel, 32, 64, 16, 13, 56 },
};

typedef enum { PS_IDLE, PS_ANIM, PS_FLY, PS_FUSE, PS_BOOM, PS_DONE, PS_GONE } PropState;

#define PROP_HIT_COOLDOWN   16   // frames entre golpes (un swing pega una vez)
#define ONEWAY_TICKS         3   // ticks por frame de la vuelta del cartel
#define ONEWAY_FRAMES        8
#define CONE_SPEED           6   // px/frame del cono golpeado
#define CONE_ENEMY_DMG       2
#define BARREL_FUSE_TICKS    5   // ticks por frame de la mecha
#define BARREL_FUSE_LOOPS    2
#define BOOM_TICKS           6   // ticks por frame de la explosion (7 frames, como el TNT)
#define BOOM_FRAMES          7
#define BOOM_RADIUS_X       40   // alcance de la explosion (centro a centro)
#define BOOM_RADIUS_Y       22   // en profundidad
#define BOOM_PLAYER_BARS     2
#define PROP_ARM_MARGIN     16   // px que tiene que asomar para existir

typedef struct {
    Sprite* spr;
    Sprite* boom;       // la explosion del barril
    s16     x;          // X actual (el cono se mueve)
    u8      state;
    u8      cool;
    u16     timer;
    s8      dir;
    s8      owner;      // tortuga que lo golpeo (puntaje)
    u8      hitMask;    // soldiers (cono/explosion) o tortugas (explosion) ya golpeados
    u8      plMask;
} Prop;

static Prop props[N_PROPS];

// ---------------------------------------------------------------------------
// El auto
// ---------------------------------------------------------------------------
typedef enum { CAR_PARKED, CAR_WAIT, CAR_DRIVE, CAR_GONE } CarState;
#define CAR_TRIG_DX         20   // pies del lider a esta X del borde del auto
#define CAR_ACCEL_Q         48   // Q8 por frame
#define CAR_VMAX_Q        1536   // 6 px/frame
#define CAR_DEPTH        (-2000) // por delante de todo (menos el HUD)
#define CAR_HIT_TOP         55   // franja de pies que atropella, desde el tope
#define CAR_HIT_BOT        100   // del sprite (la parte de abajo del auto)
#define CAR_HIT_X0           8
#define CAR_HIT_X1         168
#define CAR_BARS             3
#define CAR_DX_NUM          27   // avance en X por cada 32 en Y

static struct {
    u8      state;
    Sprite* spr[2];
    s32     xq, yq;     // Q8, esquina sup. izq.
    s32     vq;
    u8      hitMask;
} car;

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------
static s16 a16(s16 v) { return (v < 0) ? (s16)-v : v; }

static void propRelease(Prop* p) {
    if (p->spr)  { SPR_releaseSprite(p->spr);  p->spr  = NULL; }
    if (p->boom) { SPR_releaseSprite(p->boom); p->boom = NULL; }
}

static void lvl41Init(void) {
    for (u16 i = 0; i < N_PROPS; i++) {
        props[i].spr = props[i].boom = NULL;
        props[i].x = propDefs[i].x;
        props[i].state = PS_IDLE;
        props[i].cool = 0;
        props[i].timer = 0;
        props[i].hitMask = props[i].plMask = 0;
        props[i].owner = -1;
    }
    car.state = CAR_PARKED;
    car.spr[0] = car.spr[1] = NULL;
    car.hitMask = 0;
}

static void lvl41Release(void) {
    for (u16 i = 0; i < N_PROPS; i++) propRelease(&props[i]);
    for (u16 k = 0; k < 2; k++)
        if (car.spr[k]) { SPR_releaseSprite(car.spr[k]); car.spr[k] = NULL; }
}

// Frame que tiene que mostrar el prop segun su estado.
static s16 propFrame(const Prop* p, u8 kind) {
    switch (kind) {
    case PK_SIGN20: return (p->state == PS_DONE) ? 1 : 0;
    case PK_ONEWAY: return (p->state == PS_ANIM)
                           ? (s16)((p->timer / ONEWAY_TICKS) % ONEWAY_FRAMES) : 0;
    case PK_BARREL: return (p->state == PS_FUSE)
                           ? (s16)(1 + (p->timer / BARREL_FUSE_TICKS) % 4) : 0;
    default:        return 0;
    }
}

// Crea o suelta el sprite segun si el prop asoma en camara.
static void propArm(Prop* p, const PropInfo* in, u8 kind, s16 camX, s16 base) {
    if (p->state == PS_GONE || p->state == PS_BOOM) return;
    bool near = (p->x + in->w > camX + PROP_ARM_MARGIN) &&
                (p->x < camX + SCREEN_W - PROP_ARM_MARGIN);
    // Uno que se esta moviendo o animando no se suelta por estar en el borde.
    if (p->state == PS_FLY || p->state == PS_ANIM || p->state == PS_FUSE) near = TRUE;
    if (!near) {
        if (p->spr) { SPR_releaseSprite(p->spr); p->spr = NULL; }
        return;
    }
    if (!p->spr) {
        if (!sprVramFits(in->def->maxNumTile)) return;
        p->spr = SPR_addSprite(in->def, 0, -128, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
        if (!p->spr) return;
        SPR_setAutoAnimation(p->spr, FALSE);
    }
    SPR_setFrame(p->spr, propFrame(p, kind));
    SPR_setPosition(p->spr, (s16)(p->x - camX), (s16)(base - in->h - stageCamY));
    SPR_setDepth(p->spr, -base);
}

// Golpe de alguna tortuga: devuelve su indice (o -1).
static s8 propHitBy(Player** pls, u8 nPl, s16 cx, s16 base, const PropInfo* in) {
    for (u8 k = 0; k < nPl; k++)
        if (playerAttackHitsBox(pls[k], cx, base, in->halfW, in->bodyH)) return (s8)k;
    return -1;
}

static void sfxHit(void) {
    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
}

// La explosion del barril: tortugas y soldiers a tiro, una vez cada uno.
static void barrelBlast(Prop* p, s16 cx, s16 base, Player** pls, u8 nPl) {
    for (u8 k = 0; k < nPl; k++) {
        if (p->plMask & (1 << k)) continue;
        if (!playerCanBeHit(pls[k])) continue;
        s16 pcx = getPlayerHurtCX(pls[k]);   // (02/10) centro de la hurtbox
        if (a16((s16)(pcx - cx)) > BOOM_RADIUS_X + PLAYER_BODY_HALF_W) continue;
        if (a16((s16)(getPlayerY(pls[k]) - base)) > BOOM_RADIUS_Y) continue;
        playerHitBarsKnockdown(pls[k], cx, BOOM_PLAYER_BARS);
        p->plMask |= (u8)(1 << k);
    }
    Enemy* en = stageEnemies();
    if (!en) return;
    for (u16 i = 0; i < MAX_ENEMIES && i < 8; i++) {
        if (p->hitMask & (1 << i)) continue;
        if (!enemyCanBeHit(&en[i])) continue;
        if (a16((s16)(getEnemyCenterX(&en[i]) - cx)) > BOOM_RADIUS_X + enemyBodyHalfW(&en[i])) continue;
        if (a16((s16)(getEnemyCenterY(&en[i]) - base)) > BOOM_RADIUS_Y) continue;
        damageEnemy(&en[i], (s16)(en[i].hp > 0 ? en[i].hp : ENEMY_HP));
        p->hitMask |= (u8)(1 << i);
        if (p->owner >= 0 && p->owner < (s8)nPl) addPlayerScore(pls[(u8)p->owner], 1);
    }
}

static void propsUpdate(Player** pls, u8 nPl, s16 camX) {
    for (u16 i = 0; i < N_PROPS; i++) {
        Prop* p = &props[i];
        const u8 kind = propDefs[i].kind;
        const PropInfo* in = &propInfo[kind];
        const s16 base = propDefs[i].base;
        if (p->state == PS_GONE) continue;
        if (p->cool) p->cool--;

        s16 cx = (s16)(p->x + in->hitDx);

        // --- Golpe de una tortuga ---
        if (p->spr && !p->cool &&
            (p->state == PS_IDLE || (kind == PK_ONEWAY && p->state == PS_ANIM))) {
            s8 k = propHitBy(pls, nPl, cx, base, in);
            if (k >= 0) {
                p->cool  = PROP_HIT_COOLDOWN;
                p->owner = k;
                sfxHit();
                switch (kind) {
                case PK_SIGN20: p->state = PS_DONE; break;
                case PK_ONEWAY: p->state = PS_ANIM; p->timer = 0; break;
                case PK_CONE: {
                    s16 pcx = (s16)(getPlayerWorldX(pls[(u8)k]) + PLAYER_SPRITE_W / 2);
                    p->dir = (cx >= pcx) ? 1 : -1;
                    p->state = PS_FLY;
                    p->hitMask = 0;
                    break;
                }
                case PK_BARREL: p->state = PS_FUSE; p->timer = 0; break;
                }
            }
        }

        // --- Lo que hace cada uno ---
        switch (p->state) {
        case PS_ANIM:          // ONE WAY: una vuelta entera y vuelve al 0
            if (++p->timer >= ONEWAY_TICKS * ONEWAY_FRAMES) { p->state = PS_IDLE; p->timer = 0; }
            break;
        case PS_FLY: {         // cono: derecho en X hasta salir de camara
            p->x = (s16)(p->x + p->dir * CONE_SPEED);
            cx = (s16)(p->x + in->hitDx);
            Enemy* en = stageEnemies();
            for (u16 e = 0; en && e < MAX_ENEMIES && e < 8; e++) {
                if (p->hitMask & (1 << e)) continue;
                if (!enemyCanBeHit(&en[e])) continue;
                if (a16((s16)(getEnemyCenterX(&en[e]) - cx)) > in->halfW + enemyBodyHalfW(&en[e])) continue;
                if (a16((s16)(getEnemyCenterY(&en[e]) - base)) > 14) continue;
                damageEnemy(&en[e], CONE_ENEMY_DMG);
                p->hitMask |= (u8)(1 << e);
                sfxHit();
                if (en[e].state == ENEMY_STATE_DEAD) {
                    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                                   SOUND_PCM_CH3, 15, FALSE, FALSE);
                    if (p->owner >= 0 && p->owner < (s8)nPl) addPlayerScore(pls[(u8)p->owner], 1);
                }
            }
            if (p->x + in->w < camX - 8 || p->x > camX + SCREEN_W + 8) {
                propRelease(p);
                p->state = PS_GONE;
                continue;
            }
            break;
        }
        case PS_FUSE:          // barril: la mecha, dos vueltas, y explota
            if (++p->timer >= BARREL_FUSE_TICKS * 4 * BARREL_FUSE_LOOPS) {
                if (p->spr) { SPR_releaseSprite(p->spr); p->spr = NULL; }
                p->state = PS_BOOM;
                p->timer = 0;
                p->plMask = p->hitMask = 0;
                // (01/10) La explosion del TNT (64x64) en vez de la del misil
                // del Sewer: va mejor con el tamano del barril. En PAL0.
                p->boom = SPR_addSprite(&garage_boom, 0, -64,
                                        TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
                if (p->boom) {
                    SPR_setAutoAnimation(p->boom, FALSE);
                    SPR_setFrame(p->boom, 0);
                    SPR_setDepth(p->boom, (s16)(-base - 1));
                }
                XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                               SOUND_PCM_CH3, 15, FALSE, FALSE);
            }
            break;
        case PS_BOOM: {
            barrelBlast(p, cx, base, pls, nPl);
            s16 f = (s16)(p->timer / BOOM_TICKS);
            if (f >= BOOM_FRAMES) { propRelease(p); p->state = PS_GONE; continue; }
            if (p->boom) {
                SPR_setFrame(p->boom, f);
                // 64x64 centrada en el cuerpo del barril
                SPR_setPosition(p->boom, (s16)(cx - 32 - camX),
                                (s16)(base - 28 - 32 - stageCamY));
            }
            p->timer++;
            continue;
        }
        default: break;
        }

        propArm(p, in, kind, camX, base);
    }
}

// ---------------------------------------------------------------------------
// El auto
// ---------------------------------------------------------------------------
static void carPlace(s16 camX) {
    s16 sx = (s16)((car.xq >> 8) - camX);
    s16 sy = (s16)((car.yq >> 8) - stageCamY);
    for (u16 k = 0; k < 2; k++)
        if (car.spr[k]) SPR_setPosition(car.spr[k], (s16)(sx + k * 88), sy);
}

static void carUpdate(Player** pls, u8 nPl, s16 camX) {
    switch (car.state) {
    case CAR_PARKED: {
        // Una tortuga en juego se le acerco: arranca.
        for (u8 k = 0; k < nPl; k++) {
            if (isPlayerGameOver(pls[k])) continue;
            s16 fx = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
            if (fx >= LVL41_CAR_X + CAR_TRIG_DX && fx <= LVL41_CAR_X + 176) {
                car.state = CAR_WAIT;
                break;
            }
        }
        if (car.state != CAR_WAIT) break;
    }   // fallthrough
    case CAR_WAIT: {
        // Las dos mitades juntas (286 tiles): si no entran, sigue
        // estacionado (en el fondo) y se reintenta el frame que viene.
        u16 need = (u16)(garage_car_l.maxNumTile + garage_car_r.maxNumTile);
        if (SPR_getFreeVRAM() < need || !sprVramFits(garage_car_l.maxNumTile)) break;
        car.spr[0] = SPR_addSprite(&garage_car_l, 0, -128, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
        car.spr[1] = car.spr[0] ? SPR_addSprite(&garage_car_r, 0, -128,
                                                TILE_ATTR(PAL0, FALSE, FALSE, FALSE)) : NULL;
        if (!car.spr[0] || !car.spr[1]) {
            for (u16 k = 0; k < 2; k++)
                if (car.spr[k]) { SPR_releaseSprite(car.spr[k]); car.spr[k] = NULL; }
            break;
        }
        for (u16 k = 0; k < 2; k++) SPR_setDepth(car.spr[k], CAR_DEPTH);
        car.xq = (s32)LVL41_CAR_X << 8;
        car.yq = (s32)LVL41_CAR_Y << 8;
        car.vq = 0;
        car.hitMask = 0;
        carPlace(camX);
        sbgSetVariant(LVL41_REG_CAR, 1);       // el lugar queda vacio
        car.state = CAR_DRIVE;
        break;
    }
    case CAR_DRIVE: {
        // Sale en diagonal hacia abajo a la derecha (hacia la camara),
        // acelerando: el curso marcado sobre la captura va a
        // ~40 grados de la vertical (dx/dy ~ 27/32), que es para donde
        // apunta el auto.
        if (car.vq < CAR_VMAX_Q) car.vq += CAR_ACCEL_Q;
        car.yq += car.vq;
        car.xq += (car.vq * CAR_DX_NUM) >> 5;
        s16 cx = (s16)(car.xq >> 8), cy = (s16)(car.yq >> 8);
        // Atropella: pies dentro de la franja de abajo del auto.
        for (u8 k = 0; k < nPl; k++) {
            if (car.hitMask & (1 << k)) continue;
            if (!playerCanBeHit(pls[k])) continue;
            s16 pcx = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
            s16 py  = getPlayerY(pls[k]);
            if (pcx < cx + CAR_HIT_X0 || pcx > cx + CAR_HIT_X1) continue;
            if (py < cy + CAR_HIT_TOP || py > cy + CAR_HIT_BOT) continue;
            playerHitBarsKnockdown(pls[k], (s16)(cx + 88), CAR_BARS);
            car.hitMask |= (u8)(1 << k);
            sfxHit();
        }
        if (cy - stageCamY > 224 || cx - camX > SCREEN_W) {   // salio de camara
            for (u16 k = 0; k < 2; k++)
                if (car.spr[k]) { SPR_releaseSprite(car.spr[k]); car.spr[k] = NULL; }
            car.state = CAR_GONE;
            break;
        }
        carPlace(camX);
        break;
    }
    default: break;
    }
}

static void lvl41Update(Player** pls, u8 nPl, s16 camX) {
    carUpdate(pls, nPl, camX);
    propsUpdate(pls, nPl, camX);
}

// ===========================================================================
// JEFES: la persiana del ascensor, Bebop y Rocksteady
// ===========================================================================
static s16 lvl41BotAt(s16 x) { (void)x; return LVL41_WALK_Y_MAX; }

static const BebopArena arena41 = {
    LVL41_CAM_MAX_X + 48, LVL41_CAM_MAX_X + SCREEN_W - 48,   // centro del cuerpo
    LVL41_WALK_Y_MIN + 4, LVL41_WALK_Y_MAX,
    stageWalkTopAt, lvl41BotAt,
    1106, 114,          // parado en el piso del ascensor
    1100, 172,          // salta al estacionamiento
    TRUE, 50,
    PAL2                // parpadeo: el sprite pasa a PAL2 (quemada)
};

// Rocksteady sale caminando del ascensor (el borde izquierdo del frame de
// 104 px: centro 1106 - 52) y baja a pelear.
static const RocksteadyArena rockArena41 = {
    LVL41_WALK_Y_MIN + 4, LVL41_WALK_Y_MAX - 4,
    LVL41_CAM_MAX_X - 40, LVL41_CAM_MAX_X + SCREEN_W - 64,
    1054, 118,
    176, 40,
    PAL3,       // (03/10) paleta compartida con Bebop
    0,          // vida: ROCKSTEADY_HP (01/10: igual que Bebop, antes 80 aca)
    PAL2        // (03/10) parpadeo: el sprite pasa a PAL2 (quemada)
};

#define DOOR_STEP_TICKS      4   // ticks por paso de la persiana (8 px)
#define ROCK_AFTER_BEBOP    70   // frames despues de Bebop (ya salto del ascensor)
#define ROCK_VRAM_RESERVE   24   // aire para las balas y el disparo de Bebop

typedef enum { DOOR_CLOSED, DOOR_OPENING, DOOR_OPEN } DoorState;
static Bebop      bebop;
static Rocksteady rock;
static u8   doorState;
static u16  doorTimer;
static u16  doorStep;
static bool bebopOut;
static bool rockOut;
static u16  rockWait;

// (03/10) APRIL atada adentro del ascensor (los 2 frames nuevos de april.png,
// anim 1 de april_gen.png; ver tools/gen_april_sheet.py). Aparece cuando la
// persiana ya subio por encima de su cabeza y se queda hasta el final del
// nivel, al fondo (detras de los jefes, que salen del mismo ascensor, y de
// las tortugas). Paleta compartida con los jefes: PAL3.
#define APRIL41_X          1117  // centro de los pies (mundo): lado derecho del interior
#define APRIL41_Y           116  // pies (mundo): piso del ascensor
#define APRIL41_SHOW_STEP     6  // paso de la persiana que ya le destapa la cabeza
#define APRIL41_FEET_X       15  // ancla de la celda de 32x64 (gen_april_sheet.py)
#define APRIL41_FEET_Y       63
#define APRIL41_DEPTH      -100  // detras de cualquiera con pies en y >= 100
#define APRIL_ANIM_TIED       1
static Sprite* aprilSpr;

static void bossInit41(void) {
    bebopInit(&bebop);
    rocksteadyInit(&rock);
    rocksteadyBulletInit();
    doorState = DOOR_CLOSED;
    doorTimer = doorStep = 0;
    bebopOut = rockOut = FALSE;
    rockWait = 0;
    aprilSpr = NULL;
}

static void aprilUpdate41(s16 camX) {
    if (!aprilSpr) {
        if (doorState == DOOR_CLOSED || doorStep < APRIL41_SHOW_STEP) return;
        // la paleta compartida en PAL3 (Bebop la vuelve a cargar al salir)
        PAL_setPalette(PAL3, april_garage.palette->data, DMA);
        aprilSpr = SPR_addSpriteSafe(&april_garage, 0, 0,
                                     TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        if (!aprilSpr) return;
        SPR_setAnim(aprilSpr, APRIL_ANIM_TIED);
        SPR_setDepth(aprilSpr, APRIL41_DEPTH);
    }
    SPR_setPosition(aprilSpr, (s16)(APRIL41_X - APRIL41_FEET_X - camX),
                    (s16)(APRIL41_Y - APRIL41_FEET_Y - stageCamY));
}

static void bossStart41(s16 camX, s16 levelW) {
    (void)camX; (void)levelW;
    doorState = DOOR_OPENING;              // primero se abre la persiana
    doorTimer = 0;
    doorStep  = 0;
}

static void bebopHits(Player** pls, u8 nPl) {
    if (!bebopCanBeHit(&bebop)) return;
    s16 bcx = bebopGetCenterX(&bebop);
    s16 by  = bebopGetCenterY(&bebop);
    for (u8 k = 0; k < nPl; k++) {
        if (!playerAttackHitsBox(pls[k], bcx, by, BEBOP_BODY_HALF_W, BEBOP_BODY_H))
            continue;
        s16 dmg = isPlayerSpecialAttack(pls[k]) ? BEBOP_SPECIAL_DMG
                : (isPlayerJumpKicking(pls[k]) ? BEBOP_JUMPKICK_DMG : 1);
        sfxHit();
        if (bebopDamage(&bebop, dmg)) addPlayerScore(pls[k], 5);
        break;
    }
}

static void rockHits(Player** pls, u8 nPl) {
    if (rocksteadyCanBeHit(&rock)) {
        s16 bcx = rocksteadyGetCenterX(&rock);
        s16 by  = rocksteadyGetCenterY(&rock);
        for (u8 k = 0; k < nPl; k++) {
            if (!playerAttackHitsBox(pls[k], bcx, by, ROCKSTEADY_BODY_HALF_W,
                                     ROCKSTEADY_BODY_H)) continue;
            rocksteadyDamage(&rock, isPlayerSpecialAttack(pls[k]) ? ROCKSTEADY_SPECIAL_DMG
                                  : (isPlayerJumpKicking(pls[k]) ? ROCKSTEADY_JUMPKICK_DMG : 1));
            XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 15, FALSE, FALSE);
            if (rock.state == ROCKSTEADY_DEAD) addPlayerScore(pls[k], 5);
            break;
        }
    }
    // Sus balas (filtran por altura: con el antiaereo le pegan al que salta).
    s16 hitX = 0;
    for (u8 k = 0; k < nPl; k++) {
        if (!playerCanBeHitAir(pls[k])) continue;
        if (!rocksteadyBulletCheckHitPlayer(getPlayerWorldX(pls[k]), getPlayerY(pls[k]),
                                           getPlayerJumpZ(pls[k]), &hitX)) continue;
        sfxHit();
        playerHitProjectile(pls[k], hitX, ROCKSTEADY_BULLET_DMG);
    }
}

static bool bossUpdate41(Player** pls, u8 nPl, s16 camX) {
    // --- La persiana: sube de a 8 px (variantes 1..N de la region) ---
    if (doorState == DOOR_OPENING) {
        if (++doorTimer >= DOOR_STEP_TICKS) {
            doorTimer = 0;
            doorStep++;
            sbgSetVariant(LVL41_REG_DOOR, doorStep);
            if (doorStep >= LVL41_REG_DOOR_NVAR) doorState = DOOR_OPEN;
        }
    }
    aprilUpdate41(camX);

    // --- Bebop, apenas termina de abrir ---
    if (doorState == DOOR_OPEN && !bebopOut) {
        bebopOut = TRUE;
        // (01/10) Grito completo y con prioridad; el tema del jefe entra
        // cuando termina (boss_vo.h). Hasta ahi sigue el del garage.
        bossVoStart(boss_scream_bebop_vo, sizeof(boss_scream_bebop_vo),
                    music_boss, 80);
        bebopSpawnArena(&bebop, &arena41);
        // PAL2 (ya sin soldiers): la paleta compartida "quemada", para el
        // parpadeo de vida baja de los dos jefes.
        bossFlashLoadLine(PAL2, bebop_boss.palette->data);
    }

    // --- Rocksteady: cuando Bebop ya salto del ascensor Y hay VRAM ---
    if (bebopOut && !rockOut) {
        if (rockWait < ROCK_AFTER_BEBOP) rockWait++;
        else if (sprVramFits((u16)(rocksteady_boss.maxNumTile + ROCK_VRAM_RESERVE)) &&
                 SPR_getFreeVRAM() >= rocksteady_boss.maxNumTile + ROCK_VRAM_RESERVE) {
            rockOut = TRUE;
            // PAL3 ya tiene la paleta compartida (la cargo Bebop)
            rocksteadySpawnArena(&rock, &rockArena41);
        }
    }

    if (bebopOut) {
        bebopUpdate(&bebop, pls, nPl, camX, stageCamY);
        bebopHits(pls, nPl);
    }
    if (rockOut) {
        rocksteadyUpdateN(&rock, camX, pls, nPl);
        rocksteadyBulletUpdate(camX);
        rockHits(pls, nPl);
    }

    // Termina cuando cayeron los dos.
    return bebopOut && bebopIsGone(&bebop) &&
           rockOut && rock.state == ROCKSTEADY_GONE;
}

static bool bossDying41(void) {
    bool bDown = bebopOut && (bebop.state == BEBOP_DEAD || bebop.state == BEBOP_GONE);
    bool rDown = rockOut && (rock.state == ROCKSTEADY_DEAD || rock.state == ROCKSTEADY_GONE);
    return bDown && rDown;
}

static void bossRelease41(void) {
    bebopRelease(&bebop);
    rocksteadyBulletReleaseAll();
    if (rock.sprite) { SPR_releaseSprite(rock.sprite); rock.sprite = NULL; }
    rock.state = ROCKSTEADY_INACTIVE;
    if (aprilSpr) { SPR_releaseSprite(aprilSpr); aprilSpr = NULL; }
}

static const StageLevel level41 = {
    .bgRaw        = &garage41,
    .bgRegions    = garageRegs,
    .nBgRegions   = 2,
    .fg           = NULL,
    .camYMax      = LVL41_CAM_Y_MAX,
    .bgSlots      = LVL41_BG_WORST + 16,
    .levelW       = LVL41_W,
    .walkTop      = NULL,           // franja pareja: walkYMin
    .walkCols     = 0,
    .walkYMin     = LVL41_WALK_Y_MIN,
    .walkYMax     = LVL41_WALK_Y_MAX,
    .waves        = waves41,
    .nWaves       = sizeof(waves41) / sizeof(waves41[0]),
    .bossFeetX    = 1060,
    .music        = music_garage,
    .musicVol     = 80,
    .bossMusic    = music_boss,
    .bossMusicVol = 80,
    .bossMusicByVo = TRUE,      // lo arranca el grito de Bebop (bossVoStart)
    .bossInit     = bossInit41,
    .bossStart    = bossStart41,
    .bossUpdate   = bossUpdate41,
    .bossDying    = bossDying41,
    .bossRelease  = bossRelease41,
    .levelInit    = lvl41Init,
    .levelUpdate  = lvl41Update,
    .levelRelease = lvl41Release,
    .nextScene    = SCENE_5_1_TITLE,
};

SceneId showScene41() {
    return stageLevelRun(&level41);
}
