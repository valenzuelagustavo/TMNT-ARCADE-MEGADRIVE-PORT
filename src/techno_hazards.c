// ===========================================================================
// techno_hazards.c - columnas con laser y bolas del TECHNODROME (18/10)
// ===========================================================================
// Ver techno_hazards.h. Medido en el video del arcade (seg_06, ~1:05-1:15):
//   - (18/10, Gustavo) son 4, juntas; la cabeza siempre mira a la IZQUIERDA
//     (no se dan vuelta) y disparando son plateadas lisas. Los rayitos y el
//     parpadeo naranja salen cuando una tortuga las GOLPEA;
//   - el laser son 4 eslabones que vuelan a 8 px por frame;
//   - cada columna dispara en rafagas, un tiro cada ~12 frames mientras la
//     cabeza sigue moviendose (los tiros quedan "en escalera");
//   - la sombra del laser es una raya negra en el piso, debajo del tiro.
// ===========================================================================

#include "techno_hazards.h"
#include "level8_1.h"      // techno_pillar_head/seg, techno_laser(_sh), techno_pillar_sh, techno_ball
#include "audio.h"
#include "enemy.h"         // sprVramFits

#define SCREEN_W        320

// --- VRAM compartida (offsets desde thVram) ----------------------------------
#define TV_HEAD_PLAIN    0          // cabeza plateada lisa (16 tiles)
#define TV_SEG_PLAIN    16          // tramo plateado liso (8)
#define TV_HEAD_ELEC    24          // cabeza plateada con rayos (el frame de turno)
#define TV_SEG_ELEC     40
#define TV_HEAD_ORANGE  48          // cabeza naranja (golpeada / rota)
#define TV_SEG_ORANGE   64
#define TV_HEAD_OELEC   72          // cabeza naranja con rayos (el frame de turno)
#define TV_SEG_OELEC    88
#define TV_LASER        96          // 4
#define TV_LASER_SH    100          // 4
#define TV_PILLAR_SH   104          // 4
#define TH_FR_ORANGE     4          // frame naranja liso de las hojas

// --- Columnas ------------------------------------------------------------------
#define PIL_MAX          4
#define PIL_SEGS         7          // tramos de 32 px arriba de la cabeza (llegan al techo)
#define PIL_HALF_W      16
#define PIL_HITS         4          // golpes para dejarla quieta
#define PIL_FLASH       24          // frames de rayos + parpadeo naranja tras un golpe
#define PIL_BLINK_T      3          // frames de cada color del parpadeo
#define PIL_HIT_CD      16          // un golpe por swing
#define PIL_FIRE_T      12          // frames entre tiros de una rafaga
#define PIL_BURST        4          // tiros por rafaga
#define PIL_PAUSE_MIN   50          // pausa entre rafagas
#define PIL_MUZZLE_Z    17          // altura del canon sobre la base de la cabeza
#define PIL_ACT_MARGIN  24          // se activa a esto del borde de pantalla
#define TH_ELEC_T        4          // frames por cuadro de los rayitos
// Tope de la cabeza: z <= lane - zTopK (thInit). Technodrome 110: con camY 32,
// 32 + 46 + 32 de cabeza; su borde de arriba queda en y >= 46 de pantalla.

// (18/10) Las 4 del arcade, juntas (~36 px entre una y otra), en distintas
// lanes y con distintas velocidades. Estan en la zona de la 2da oleada
// (camara trabada en 360: se ven las 4 a la vez).
const ThPillarDef thTechnoPillars[TH_TECHNO_PILLARS] = {
    {  560, 214,  4, 96, 16, 50 },
    {  596, 150,  6, 80, 10, 20 },
    {  632, 236,  4, 90, 20, 40 },
    {  668, 178, 10, 72, 12, 10 },
};

typedef struct {
    u8   zMax;                      // zMax de la tabla, recortado por la lane
    s16  zq;                        // altura en Q4
    s8   dirZ;                      // +1 sube, -1 baja
    u8   hits, flash, hitCd;
    u8   fireT, burst, pause;
    bool dead;
    Sprite* head;
    Sprite* seg[PIL_SEGS];
    Sprite* sh;
} Pillar;

static Pillar pils[PIL_MAX];
// (18/10) La tabla la pasa el nivel (Technodrome / fabrica).
static const ThPillarDef* pilDefs;
static u8  pilN;
static s16 pilZTopK;

// --- Lasers --------------------------------------------------------------------
#define LAS_MAX          6
#define LAS_SPEED        8
#define LAS_HALF_W      16
#define LAS_DMG          2

typedef struct {
    bool on;
    s16  x, lane, z;
    s8   dir;
    Sprite* s;
    Sprite* sh;
} Laser;

static Laser lasers[LAS_MAX];

// --- Bolas ---------------------------------------------------------------------
// (18/10, dibujo de Gustavo) Entran por la IZQUIERDA de la pantalla con un
// arco largo, pican en la base del ascensor (los piques siguen la diagonal de
// la base: mas al fondo, mas a la derecha; con sombra mientras estan encima)
// y siguen a la DERECHA con otro arco de ~40 px, que las saca de la pantalla
// o las hace caer al pozo si pican fuera de la base. La profundidad (ly) no
// cambia en todo el camino. z y vz en Q6.
#define BALL_MAX         3
#define BALL_VX          3          // px/f hacia la derecha, sobre el ascensor
#define BALL_DMG         2
#define BALL_FIRST      50
#define BALL_EVERY      70          // + random 0..47
#define BALL_HALF_W     18
#define BALL_DY         10
#define BALL_G           4          // gravedad, Q6 (0,0625 px/f^2)
#define BALL_BOUNCE_N    5          // pique: conserva 5/8 de la velocidad
#define BALL_BOUNCE_D    8
#define BALL_ELEV_SX    60          // elevX - camX en el viaje (camara en L8_TOP_CAM_MAX_X)
#define BALL_START_LX  (-24 - BALL_ELEV_SX)   // nace a la izquierda de la pantalla
#define BALL_LAND_K    180          // primer pique en lx = BALL_LAND_K - ly (+-12)
// El piso del ascensor (ver level8_1.c, L8_FLOOR_*): en la fila local y va de
// x = 126 - y a x = 317 - y, filas 4..120.
#define BALL_FLOOR_L0  126
#define BALL_FLOOR_R0  317
#define BALL_FLOOR_T     4
#define BALL_FLOOR_B   120

typedef struct {
    bool on, off;                   // off: se salio de la base y cae al pozo
    s16  lx, ly;                    // relativo a la esquina del ascensor
    s16  zq, vzq;                   // Q6
    u8   anim, hitMask;
    Sprite* s;
    Sprite* sh;
} Ball;

static Ball balls[BALL_MAX];
static u16  ballWait;

static u16 thVram;
static u8  elecFrame, elecTick;

// ===========================================================================
static void loadFrame(const SpriteDefinition* def, u16 frame, u16 vram, TransferMethod tm) {
    VDP_loadTileSet(def->animations[0]->frames[frame]->tileset, vram, tm);
}

static Sprite* addShared(const SpriteDefinition* def, u16 vram) {
    Sprite* s = SPR_addSpriteEx(def, -64, -64,
                                TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE, vram),
                                SPR_FLAG_AUTO_VISIBILITY);
    if (s) {
        SPR_setAutoAnimation(s, FALSE);
        SPR_setVisibility(s, HIDDEN);
    }
    return s;
}

void thInit(u16 vram, const ThPillarDef* defs, u8 n, s16 zTopK) {
    thVram = vram;
    pilDefs = defs;
    pilN = (n > PIL_MAX) ? PIL_MAX : n;
    pilZTopK = zTopK;
    loadFrame(&techno_pillar_head, 0, (u16)(vram + TV_HEAD_PLAIN), DMA);
    loadFrame(&techno_pillar_seg,  0, (u16)(vram + TV_SEG_PLAIN), DMA);
    loadFrame(&techno_pillar_head, 1, (u16)(vram + TV_HEAD_ELEC), DMA);
    loadFrame(&techno_pillar_seg,  1, (u16)(vram + TV_SEG_ELEC), DMA);
    loadFrame(&techno_pillar_head, TH_FR_ORANGE, (u16)(vram + TV_HEAD_ORANGE), DMA);
    loadFrame(&techno_pillar_seg,  TH_FR_ORANGE, (u16)(vram + TV_SEG_ORANGE), DMA);
    loadFrame(&techno_pillar_head, TH_FR_ORANGE + 1, (u16)(vram + TV_HEAD_OELEC), DMA);
    loadFrame(&techno_pillar_seg,  TH_FR_ORANGE + 1, (u16)(vram + TV_SEG_OELEC), DMA);
    loadFrame(&techno_laser,       0, (u16)(vram + TV_LASER), DMA);
    loadFrame(&techno_laser_sh,    0, (u16)(vram + TV_LASER_SH), DMA);
    loadFrame(&techno_pillar_sh,   0, (u16)(vram + TV_PILLAR_SH), DMA);
    elecFrame = 1; elecTick = 0;

    for (u16 i = 0; i < PIL_MAX; i++) {
        Pillar* p = &pils[i];
        memset(p, 0, sizeof(Pillar));
        // La cabeza nunca sube hasta la franja del HUD (su borde de arriba
        // queda a 46 px o mas de la parte de arriba de la pantalla).
        if (i >= pilN) continue;
        s16 zTop = (s16)(pilDefs[i].lane - pilZTopK);
        p->zMax = (pilDefs[i].zMax < zTop) ? pilDefs[i].zMax : (u8)zTop;
        p->zq   = (s16)(((pilDefs[i].zMin + p->zMax) / 2) << 4);
        p->dirZ = (i & 1) ? 1 : -1;
        p->fireT = pilDefs[i].delay;
    }
    for (u16 i = 0; i < LAS_MAX; i++) { lasers[i].on = FALSE; lasers[i].s = lasers[i].sh = NULL; }
    for (u16 i = 0; i < BALL_MAX; i++) { balls[i].on = FALSE; balls[i].s = balls[i].sh = NULL; }
    ballWait = BALL_FIRST;
}

// ---------------------------------------------------------------------------
// Columnas
// ---------------------------------------------------------------------------
static void pillarFree(Pillar* p) {
    if (p->head) { SPR_releaseSprite(p->head); p->head = NULL; }
    for (u16 k = 0; k < PIL_SEGS; k++)
        if (p->seg[k]) { SPR_releaseSprite(p->seg[k]); p->seg[k] = NULL; }
    if (p->sh) { SPR_releaseSprite(p->sh); p->sh = NULL; }
}

static bool pillarAlloc(Pillar* p) {
    p->head = addShared(&techno_pillar_head, (u16)(thVram + TV_HEAD_PLAIN));
    if (!p->head) return FALSE;
    for (u16 k = 0; k < PIL_SEGS; k++) {
        p->seg[k] = addShared(&techno_pillar_seg, (u16)(thVram + TV_SEG_PLAIN));
        if (!p->seg[k]) { pillarFree(p); return FALSE; }
    }
    p->sh = addShared(&techno_pillar_sh, (u16)(thVram + TV_PILLAR_SH));
    if (!p->sh) { pillarFree(p); return FALSE; }
    return TRUE;
}

static void laserFire(s16 x, s16 lane, s16 z, s8 dir) {
    for (u16 i = 0; i < LAS_MAX; i++) {
        Laser* l = &lasers[i];
        if (l->on) continue;
        l->s  = addShared(&techno_laser, (u16)(thVram + TV_LASER));
        l->sh = addShared(&techno_laser_sh, (u16)(thVram + TV_LASER_SH));
        if (!l->s || !l->sh) {
            if (l->s)  { SPR_releaseSprite(l->s);  l->s  = NULL; }
            if (l->sh) { SPR_releaseSprite(l->sh); l->sh = NULL; }
            return;
        }
        if (dir > 0) SPR_setHFlip(l->s, TRUE);
        l->on = TRUE; l->x = x; l->lane = lane; l->z = z; l->dir = dir;
        return;
    }
}

static void laserFree(Laser* l) {
    if (l->s)  { SPR_releaseSprite(l->s);  l->s  = NULL; }
    if (l->sh) { SPR_releaseSprite(l->sh); l->sh = NULL; }
    l->on = FALSE;
}

// La tortuga en juego mas cercana en X (NULL si no hay ninguna).
static Player* nearestPlayer(Player** pls, u8 nPl, s16 x) {
    Player* best = NULL;
    s16 bd = 0x7FFF;
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k]) || !pls[k]->sprite) continue;
        s16 d = (s16)(getPlayerHurtCX(pls[k]) - x);
        if (d < 0) d = -d;
        if (d < bd) { bd = d; best = pls[k]; }
    }
    return best;
}

static void pillarSetLook(Pillar* p, u16 head, u16 seg) {
    SPR_setVRAMTileIndex(p->head, (s16)(thVram + head));
    for (u16 k = 0; k < PIL_SEGS; k++) SPR_setVRAMTileIndex(p->seg[k], (s16)(thVram + seg));
}

void thPillarsUpdate(Player** pls, u8 nPl, s16 camX, s16 camY) {
    // Rayitos: un solo juego de tiles para todas las columnas que disparan.
    if (++elecTick >= TH_ELEC_T) {
        elecTick = 0;
        if (++elecFrame > 3) elecFrame = 1;
        loadFrame(&techno_pillar_head, elecFrame, (u16)(thVram + TV_HEAD_ELEC), DMA_QUEUE);
        loadFrame(&techno_pillar_seg,  elecFrame, (u16)(thVram + TV_SEG_ELEC), DMA_QUEUE);
        loadFrame(&techno_pillar_head, (u16)(TH_FR_ORANGE + elecFrame), (u16)(thVram + TV_HEAD_OELEC), DMA_QUEUE);
        loadFrame(&techno_pillar_seg,  (u16)(TH_FR_ORANGE + elecFrame), (u16)(thVram + TV_SEG_OELEC), DMA_QUEUE);
    }

    bool firedSfx = FALSE;
    for (u16 i = 0; i < PIL_MAX; i++) {
        Pillar* p = &pils[i];
        if (i >= pilN) break;
        const ThPillarDef* d = &pilDefs[i];
        s16 sx = (s16)(d->x - camX);
        if (sx < -PIL_ACT_MARGIN || sx > SCREEN_W + PIL_ACT_MARGIN) {
            pillarFree(p);
            continue;
        }
        if (!p->head && !pillarAlloc(p)) continue;

        // --- Movimiento: la cabeza sube y baja entre zMin y zMax ---------------
        if (!p->dead) {
            p->zq = (s16)(p->zq + p->dirZ * d->speedQ);
            if (p->zq >= (s16)(p->zMax << 4)) { p->zq = (s16)(p->zMax << 4); p->dirZ = -1; }
            if (p->zq <= (s16)(d->zMin << 4)) { p->zq = (s16)(d->zMin << 4); p->dirZ = 1; }
        }
        s16 z = (s16)(p->zq >> 4);

        // --- Golpes de las tortugas -------------------------------------------
        if (p->hitCd) p->hitCd--;
        if (p->flash) p->flash--;
        if (!p->dead && !p->hitCd) {
            for (u8 k = 0; k < nPl; k++) {
                if (!playerAttackHitsBand(pls[k], d->x, d->lane, PIL_HALF_W - 4,
                                          (s16)-(z + 32), (s16)-z)) continue;
                p->hitCd = PIL_HIT_CD;
                p->flash = PIL_FLASH;
                XGM2_playPCMEx(mouser_hit_sfx, sizeof(mouser_hit_sfx),
                               SOUND_PCM_CH3, 15, FALSE, FALSE);
                if (++p->hits >= PIL_HITS) {
                    p->dead = TRUE;
                    addPlayerScore(pls[k], 1);
                }
                break;
            }
        }

        // --- Disparos ---------------------------------------------------------
        // Siempre hacia la IZQUIERDA (como en el arcade: no se dan vuelta).
        if (!p->dead && sx > -8 && sx < SCREEN_W + 8) {
            Player* t = nearestPlayer(pls, nPl, d->x);
            if (p->pause) p->pause--;
            else if (t) {
                if (p->fireT) p->fireT--;
                if (!p->fireT) {
                    s16 mx = (s16)(d->x - PIL_HALF_W);
                    laserFire(mx, d->lane, (s16)(z + PIL_MUZZLE_Z), -1);
                    if (!firedSfx && p->burst == 0) {
                        XGM2_playPCMEx(rodney_shoot_sfx, sizeof(rodney_shoot_sfx),
                                       SOUND_PCM_CH3, 6, FALSE, FALSE);
                        firedSfx = TRUE;
                    }
                    p->fireT = PIL_FIRE_T;
                    if (++p->burst >= PIL_BURST) {
                        p->burst = 0;
                        p->pause = (u8)(PIL_PAUSE_MIN + (random() & 31));
                    }
                }
            }
        }

        // --- Dibujo -----------------------------------------------------------
        // Golpeada: rayos y parpadeo plateada/naranja. Rota: naranja lisa.
        if (p->flash) {
            if ((p->flash / PIL_BLINK_T) & 1) pillarSetLook(p, TV_HEAD_OELEC, TV_SEG_OELEC);
            else                              pillarSetLook(p, TV_HEAD_ELEC, TV_SEG_ELEC);
        }
        else if (p->dead) pillarSetLook(p, TV_HEAD_ORANGE, TV_SEG_ORANGE);
        else              pillarSetLook(p, TV_HEAD_PLAIN, TV_SEG_PLAIN);
        s16 hy = (s16)(d->lane - z - 32 - camY);
        SPR_setPosition(p->head, (s16)(sx - PIL_HALF_W), hy);
        SPR_setVisibility(p->head, VISIBLE);
        SPR_setDepth(p->head, (s16)-(d->lane));
        for (u16 k = 0; k < PIL_SEGS; k++) {
            s16 y = (s16)(hy - 32 * (k + 1));
            Sprite* s = p->seg[k];
            // (18/10) Hasta arriba de todo, tambien en la franja del HUD
            // (prioridad baja: los marcos del HUD quedan delante).
            if (y <= -32) { SPR_setVisibility(s, HIDDEN); continue; }
            SPR_setPosition(s, (s16)(sx - 8), y);
            SPR_setVisibility(s, VISIBLE);
            SPR_setDepth(s, (s16)-(d->lane));
        }
        SPR_setPosition(p->sh, (s16)(sx - PIL_HALF_W), (s16)(d->lane - 6 - camY));
        SPR_setVisibility(p->sh, VISIBLE);
        SPR_setDepth(p->sh, SPR_MAX_DEPTH);
    }

    // --- Lasers ------------------------------------------------------------------
    for (u16 i = 0; i < LAS_MAX; i++) {
        Laser* l = &lasers[i];
        if (!l->on) continue;
        l->x = (s16)(l->x + l->dir * LAS_SPEED);
        s16 sx = (s16)(l->x - camX);
        if (sx < -40 || sx > SCREEN_W + 40) { laserFree(l); continue; }
        bool hit = FALSE;
        for (u8 k = 0; k < nPl && !hit; k++) {
            Player* pl = pls[k];
            if (!playerCanBeHitAir(pl)) continue;
            s16 dy = (s16)(pl->y - l->lane);
            if (dy < -10 || dy > 10) continue;
            s16 dx = (s16)(getPlayerHurtCX(pl) - (l->x + l->dir * LAS_HALF_W));
            if (dx < -LAS_HALF_W || dx > LAS_HALF_W) continue;
            s16 pz = playerDrawZ(pl);
            if (l->z < pz + 2 || l->z > pz + 56) continue;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
            playerHitProjectile(pl, l->x, LAS_DMG);
            hit = TRUE;
        }
        if (hit) { laserFree(l); continue; }
        // la X del laser es su punta: el sprite queda detras
        s16 lx = (l->dir < 0) ? sx : (s16)(sx - 32);
        SPR_setPosition(l->s,  lx, (s16)(l->lane - l->z - 4 - camY));
        SPR_setPosition(l->sh, lx, (s16)(l->lane - 7 - camY));
        SPR_setVisibility(l->s, VISIBLE);
        SPR_setVisibility(l->sh, VISIBLE);
        SPR_setDepth(l->s, (s16)(-(l->lane) - 1));
        SPR_setDepth(l->sh, SPR_MAX_DEPTH);
    }
}

// ---------------------------------------------------------------------------
// Bolas
// ---------------------------------------------------------------------------
static void ballFree(Ball* b) {
    if (b->s)  { SPR_releaseSprite(b->s);  b->s  = NULL; }
    if (b->sh) { SPR_releaseSprite(b->sh); b->sh = NULL; }
    b->on = FALSE;
}

static bool ballOnFloor(const Ball* b) {
    return b->ly >= BALL_FLOOR_T && b->ly <= BALL_FLOOR_B &&
           b->lx >= BALL_FLOOR_L0 - b->ly && b->lx <= BALL_FLOOR_R0 - b->ly;
}

void thBallsUpdate(Player** pls, u8 nPl, s16 camX, s16 camY,
                   s16 elevX, s16 elevY, bool spawn) {
    if (spawn) {
        if (ballWait) ballWait--;
        if (!ballWait) {
            for (u16 i = 0; i < BALL_MAX; i++) {
                Ball* b = &balls[i];
                if (b->on) continue;
                if (!sprVramFits(techno_ball.maxNumTile)) break;
                b->s = SPR_addSprite(&techno_ball, -64, -64, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
                if (!b->s) break;
                SPR_setAutoAnimation(b->s, FALSE);
                b->sh = addShared(&techno_pillar_sh, (u16)(thVram + TV_PILLAR_SH));
                // Primer pique: en la base, sobre su diagonal. Nace a la
                // izquierda de la pantalla, a una altura que la deja en
                // pantalla (y ~45..90), con la vz que la hace caer justo ahi
                // en T frames: z0 + vz*T - g*T^2/2 = 0.
                s16 ly = (s16)(10 + (random() % 90));
                s16 lx = (s16)(BALL_LAND_K - ly - 12 + (random() % 25));
                s16 T  = (s16)((lx - BALL_START_LX) / BALL_VX);
                s16 z0 = (s16)(96 + ly - (40 + (ly >> 1)));    // pies en pantalla - y de arranque
                b->on = TRUE;
                b->off = FALSE;
                b->lx = (s16)(lx - T * BALL_VX);
                b->ly = ly;
                b->zq = (s16)(z0 << 6);
                b->vzq = (s16)((((s32)BALL_G * T * T) / 2 - ((s32)z0 << 6)) / T);
                b->anim = 0;
                b->hitMask = 0;
                break;
            }
            ballWait = (u16)(BALL_EVERY + (random() & 47));
        }
    }

    for (u16 i = 0; i < BALL_MAX; i++) {
        Ball* b = &balls[i];
        if (!b->on) continue;
        b->lx += BALL_VX;
        b->vzq -= BALL_G;
        b->zq += b->vzq;
        if (!b->off && b->zq <= 0) {
            if (ballOnFloor(b)) {                       // pica y sigue
                b->zq = 0;
                b->vzq = (s16)((-b->vzq * BALL_BOUNCE_N) / BALL_BOUNCE_D);
                XGM2_playPCMEx(iron_ball_sfx, sizeof(iron_ball_sfx), SOUND_PCM_CH3, 8, FALSE, FALSE);
            } else {
                b->off = TRUE;                          // fuera de la base: al pozo
            }
        }
        if (b->zq < -(160 << 6) || b->lx > 300) { ballFree(b); continue; }
        s16 wx = (s16)(elevX + b->lx), wy = (s16)(elevY + b->ly);
        s16 z = (s16)(b->zq >> 6);

        if (!b->off && z < 12) {
            for (u8 k = 0; k < nPl; k++) {
                Player* pl = pls[k];
                u8 bit = (u8)(1 << k);
                if (b->hitMask & bit) continue;
                if (playerDrawZ(pl) > 20) continue;          // la salto
                s16 dy = (s16)(pl->y - wy);
                if (dy < -BALL_DY || dy > BALL_DY) continue;
                s16 dx = (s16)(getPlayerHurtCX(pl) - wx);
                if (dx < -BALL_HALF_W || dx > BALL_HALF_W) continue;
                if (!playerCanBeHit(pl)) continue;
                b->hitMask |= bit;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
                playerHitBarsKnockdown(pl, wx, BALL_DMG);
            }
        }

        if (++b->anim >= 8) b->anim = 0;
        SPR_setFrame(b->s, (s16)(b->anim >> 2));
        SPR_setPosition(b->s, (s16)(wx - 16 - camX), (s16)(wy - 30 - z - camY));
        SPR_setDepth(b->s, b->off ? SPR_MAX_DEPTH - 1 : (s16)-wy);
        // Sombra en la base mientras esta arriba de ella.
        if (b->sh) {
            bool shOn = !b->off && ballOnFloor(b);
            SPR_setVisibility(b->sh, shOn ? VISIBLE : HIDDEN);
            if (shOn) {
                SPR_setPosition(b->sh, (s16)(wx - 16 - camX), (s16)(wy - 6 - camY));
                SPR_setDepth(b->sh, SPR_MAX_DEPTH);
            }
        }
    }
}

void thRelease(void) {
    for (u16 i = 0; i < PIL_MAX; i++) pillarFree(&pils[i]);
    for (u16 i = 0; i < LAS_MAX; i++) laserFree(&lasers[i]);
    for (u16 i = 0; i < BALL_MAX; i++) ballFree(&balls[i]);
}
