// ===========================================================================
// factory_fx.c - lasers de la pared, capsula del taladro y abejas (18/10)
// ===========================================================================
// Ver factory_fx.h. Medidas del video del arcade (seg_05, 2 fps + tramos a
// 59 fps):
//   - laser: arranca en la boca del emisor (y 116) y llega abajo de todo;
//     titila cada frame entre gruesa y finita ~1,5 s y se recoge de arriba
//     para abajo en unos frames. Los emisores de la pared estan en x 560,
//     656 y 752 del fondo del port.
//   - capsula: sale del piso (frames del taladro), abre la puerta y salen 4
//     abejas que rodean a la tortuga tirando rayitos.
// ===========================================================================

#include "factory_fx.h"
#include "level7_1.h"      // factory_capsule, factory_bee, factory_bee_shot, factory_laser
#include "level3_1.h"      // sewer_mouser_boom (la explosion, misma paleta PAL3)
#include "level8_1.h"      // techno_pillar_sh (la sombra gris, misma paleta PAL3)
#include "stage_level.h"   // stageShake
#include "enemy.h"         // sprVramFits
#include "audio.h"

#define SCREEN_W   320

static s16 fabs16(s16 v) { return (v < 0) ? -v : v; }

// ---------------------------------------------------------------------------
// LASERS de la pared
// ---------------------------------------------------------------------------
#define LZ_N          3
// (18/10, captura de Gustavo) Sale de la BOCA del emisor (y 116) y es una
// linea blanca de 6 px (centrada en lzX + 0,5).
#define LZ_TOP      116            // boca del emisor (y de pantalla = mundo)
#define LZ_SEGS       4            // 4 tramos de 32: pasa el borde de abajo (224)
#define LZ_ON        90
#define LZ_OFF_MIN   80            // + random 0..79
#define LZ_RETRACT_T  3            // frames por tramo al recogerse
#define LZ_HIT_DX     5
#define LZ_DMG        2

static const s16 lzX[LZ_N] = { 560, 656, 752 };
static struct {
    u8      st;                    // 0 apagado, 1 prendido, 2 recogiendose
    u16     t;
    Sprite* s[LZ_SEGS];
} lz[LZ_N];

static void lzFree(u16 i) {
    for (u16 k = 0; k < LZ_SEGS; k++)
        if (lz[i].s[k]) { SPR_releaseSprite(lz[i].s[k]); lz[i].s[k] = NULL; }
}

void ffxLasersUpdate(Player** pls, u8 nPl, s16 camX) {
    for (u16 i = 0; i < LZ_N; i++) {
        s16 sx = (s16)(lzX[i] - camX);
        bool vis = (sx > -8) && (sx < SCREEN_W);
        if (lz[i].t) lz[i].t--;
        switch (lz[i].st) {
        case 0:
            if (!lz[i].t && vis) { lz[i].st = 1; lz[i].t = LZ_ON; }
            break;
        case 1:
            if (!lz[i].t) { lz[i].st = 2; lz[i].t = LZ_SEGS * LZ_RETRACT_T; }
            break;
        default:
            if (!lz[i].t) { lz[i].st = 0; lz[i].t = (u16)(LZ_OFF_MIN + (random() % 80)); }
            break;
        }
        // Cuantos tramos se ven: todos prendido; recogiendose, los de abajo.
        u16 from = 0;
        if (lz[i].st == 2) from = (u16)(LZ_SEGS - (lz[i].t + LZ_RETRACT_T - 1) / LZ_RETRACT_T);
        if (lz[i].st == 0 || !vis) { lzFree(i); continue; }
        for (u16 k = 0; k < LZ_SEGS; k++) {
            if (!lz[i].s[k]) {
                lz[i].s[k] = SPR_addSprite(&factory_laser, -16, -64, TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
                if (!lz[i].s[k]) continue;
                SPR_setAutoAnimation(lz[i].s[k], FALSE);
                SPR_setDepth(lz[i].s[k], SPR_MAX_DEPTH - 2);
            }
            Sprite* s = lz[i].s[k];
            SPR_setVisibility(s, (k >= from) ? VISIBLE : HIDDEN);
            SPR_setFrame(s, (s16)(vtimer & 1));               // titila: 6 px / 4 px
            SPR_setPosition(s, (s16)(sx - 3), (s16)(LZ_TOP + 32 * k));
        }
        if (lz[i].st != 1) continue;
        for (u8 k = 0; k < nPl; k++) {
            Player* p = pls[k];
            if (!playerCanBeHit(p)) continue;
            // (18/10) El rayo baja por delante de la plataforma: arriba de
            // ella (pies por encima de la boca) no alcanza.
            if (getPlayerY(p) < LZ_TOP) continue;
            if (fabs16((s16)(getPlayerHurtCX(p) - lzX[i])) > LZ_HIT_DX) continue;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
            XGM2_playPCMEx(shock_pain_vo, sizeof(shock_pain_vo), SOUND_PCM_CH2, 12, FALSE, FALSE);
            playerHitBarsKnockdown(p, lzX[i], LZ_DMG);
        }
    }
}

// ---------------------------------------------------------------------------
// CAPSULA
// ---------------------------------------------------------------------------
#define CAP_W            96
#define CAP_H           120
#define CAP_EMERGE_FR     9        // anim 0: el taladro saliendo + capsula cerrada
#define CAP_EMERGE_T     10
#define CAP_DOOR_T       28        // anim 1: frame 0 cerrada, 1 abierta
#define CAP_BEE_GAP      26        // frames entre abeja y abeja
#define CAP_DOOR_DY      44        // altura de la puerta (centro) sobre la base

enum { CAP_OFF, CAP_EMERGE, CAP_DOOR, CAP_RELEASE, CAP_DONE };
static struct {
    u8      st;
    s16     x, baseY;
    u16     t;
    u8      frame, bees;
    u8      anim, fr;              // frame cargado a mano (el que se ve)
    u8      wantAnim, wantFr;      // (19/10) frame pedido
    bool    pending;               // (19/10) sus tiles no entraron en la cola
    u16     vram;
    Sprite* s;
} cap;

// ---------------------------------------------------------------------------
// ABEJAS
// ---------------------------------------------------------------------------
// (18/10, Gustavo) Hasta 8 a la vez. Cada una tiene su LUGAR alrededor de
// la tortuga (beeSlot: 4 distancias por lado, alturas alternadas) para que no
// se apilen en las mismas lineas de pantalla (tope de 20 sprites / 320 px por
// linea del VDP: apiladas titilaban).
#define BEE_MAX          8
#define BEE_HP           2
#define BEE_Z            44        // altura de vuelo (pies de la celda)
#define BEE_SPEED_X_Q  320         // 1,25 px/f (Q8)
#define BEE_SPEED_Y_Q  192         // 0,75 px/f
#define BEE_FLASH       12
#define BEE_HIT_CD      14
#define BEE_FIRE_MIN   100         // + random 0..127 (son el doble que antes)
#define BEE_ANIM_T       3
#define BEE_OUT_T       30         // saliendo de la capsula

enum { BEE_OFF, BEE_OUT, BEE_FLY, BEE_BOOM };
typedef struct {
    u8      st;
    s16     x, y, z;               // centro X, lane (sombra) y altura
    u8      accX, accY;
    u8      hp, flash, hitCd, frame, tick, face;
    u16     t, fireT;
    s8      side;                  // de que lado de la tortuga se queda
    Sprite* s;
    Sprite* sh;
} Bee;
static Bee bees[BEE_MAX];

// Lugar de cada abeja: distancia a la tortuga, altura y corrimiento de lane.
static const struct { u8 dx, z; s8 dy; } beeSlot[BEE_MAX / 2] = {
    {  48, 40, -6 }, {  78, 62,  6 }, { 108, 30, -2 }, { 138, 54,  2 },
};

#define SHOT_MAX         6
#define SHOT_T          14         // frames de vuelo hasta el piso
#define SHOT_DMG         1
static struct {
    bool    on;
    s16     x0, y0, z0, x1, y1;
    u8      t;
    Sprite* s;
} shots[SHOT_MAX];

void ffxInit(void) {
    for (u16 i = 0; i < LZ_N; i++) {
        lz[i].st = 0;
        lz[i].t = (u16)(20 + i * 45);
        for (u16 k = 0; k < LZ_SEGS; k++) lz[i].s[k] = NULL;
    }
    cap.st = CAP_OFF; cap.s = NULL;
    for (u16 i = 0; i < BEE_MAX; i++) { bees[i].st = BEE_OFF; bees[i].s = bees[i].sh = NULL; }
    for (u16 i = 0; i < SHOT_MAX; i++) { shots[i].on = FALSE; shots[i].s = NULL; }
}

// La capsula es grande (12x15 tiles por frame): por la cola de DMA, junto con
// el fondo que se streamea, el frame no entraba en la capacidad del cuadro y
// SGDK descartaba parte de la carga (la capsula salia a medias). Sus tiles se
// cargan A MANO con DMA directo cada vez que cambia de frame.
// Si sprVramFits desfragmenta la VRAM (SPR_defragVRAM) y muda la capsula,
// SGDK no le vuelve a subir los tiles (no tiene carga automatica): se
// recargan cuando cambia su indice.
// (19/10) Los tiles del frame van por la cola de DMA (llegan en el mismo
// vblank que la tabla de sprites: antes, con DMA inmediato en plena imagen,
// el cuadro del cambio salia mezclado) y el frame cambia SOLO si la cola los
// acepto; si no, se reintenta el cuadro siguiente (ver la capsula del 1-2).
static bool capQueue(u16 anim, u16 frame) {
    const TileSet* ts = factory_capsule.animations[anim]->frames[frame]->tileset;
    u16 vram = cap.s->attribut & TILE_INDEX_MASK;
    if (!DMA_queueDma(DMA_VRAM, (void*) ts->tiles, (u16)(vram * 32), (u16)(ts->numTile * 16), 2))
        return FALSE;
    SPR_setAnimAndFrame(cap.s, (s16)anim, (s16)frame);
    cap.anim = (u8)anim; cap.fr = (u8)frame; cap.vram = vram;
    return TRUE;
}
static void capSetFrame(u16 anim, u16 frame) {
    cap.wantAnim = (u8)anim; cap.wantFr = (u8)frame;
    cap.pending = !capQueue(anim, frame);
}

bool ffxCapsuleStart(s16 x, s16 baseY, u8 n) {
    if (cap.st != CAP_OFF) return FALSE;
    if (!sprVramFits(factory_capsule.maxNumTile)) return FALSE;
    cap.s = SPR_addSpriteEx(&factory_capsule, -128, -128, TILE_ATTR(PAL1, FALSE, FALSE, FALSE),
                            SPR_FLAG_AUTO_VRAM_ALLOC | SPR_FLAG_AUTO_VISIBILITY);
    if (!cap.s) return FALSE;
    SPR_setAutoAnimation(cap.s, FALSE);
    capSetFrame(0, 0);
    SPR_setDepth(cap.s, (s16)-baseY);
    cap.st = CAP_EMERGE;
    cap.x = x; cap.baseY = baseY;
    cap.t = 0; cap.frame = 0;
    cap.bees = n;
    XGM2_playPCMEx(drill_sfx, sizeof(drill_sfx), SOUND_PCM_CH3, 12, FALSE, FALSE);
    return TRUE;
}

static Player* nearestP(Player** pls, u8 nPl, s16 x) {
    Player* best = NULL;
    s16 bd = 0x7FFF;
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k]) || !pls[k]->sprite) continue;
        s16 d = fabs16((s16)(getPlayerHurtCX(pls[k]) - x));
        if (d < bd) { bd = d; best = pls[k]; }
    }
    return best;
}

static bool beeSpawn(s16 x, s16 y, s16 z) {
    for (u16 i = 0; i < BEE_MAX; i++) {
        Bee* b = &bees[i];
        if (b->st != BEE_OFF) continue;
        if (!sprVramFits(factory_bee.maxNumTile)) return FALSE;
        b->s = SPR_addSprite(&factory_bee, -64, -64, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        if (!b->s) return FALSE;
        SPR_setAutoAnimation(b->s, FALSE);
        b->sh = SPR_addSprite(&techno_pillar_sh, -64, -64, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        b->st = BEE_OUT;
        b->x = x; b->y = y; b->z = z;
        b->accX = b->accY = 0;
        b->hp = BEE_HP; b->flash = 0; b->hitCd = 0;
        b->frame = 0; b->tick = 0; b->face = 1;
        b->t = 0;
        b->fireT = (u16)(BEE_FIRE_MIN + (random() & 127));
        b->side = (i & 1) ? 1 : -1;
        return TRUE;
    }
    return FALSE;
}

static void beeFree(Bee* b) {
    if (b->s)  { SPR_releaseSprite(b->s);  b->s  = NULL; }
    if (b->sh) { SPR_releaseSprite(b->sh); b->sh = NULL; }
    b->st = BEE_OFF;
}

static void shotFire(const Bee* b, const Player* t) {
    for (u16 i = 0; i < SHOT_MAX; i++) {
        if (shots[i].on) continue;
        shots[i].s = SPR_addSprite(&factory_bee_shot, -32, -32, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        if (!shots[i].s) return;
        SPR_setAutoAnimation(shots[i].s, FALSE);
        shots[i].on = TRUE;
        shots[i].x0 = b->x; shots[i].y0 = b->y; shots[i].z0 = (s16)(b->z - 4);
        shots[i].x1 = getPlayerHurtCX(t); shots[i].y1 = getPlayerY(t);
        shots[i].t = 0;
        s16 dx = (s16)(shots[i].x1 - shots[i].x0);
        SPR_setFrame(shots[i].s, (dx < -12) ? 0 : ((dx > 12) ? 2 : 1));
        XGM2_playPCMEx(rodney_shoot_sfx, sizeof(rodney_shoot_sfx), SOUND_PCM_CH3, 6, FALSE, FALSE);
        return;
    }
}

static void shotsUpdate(Player** pls, u8 nPl, s16 camX) {
    for (u16 i = 0; i < SHOT_MAX; i++) {
        if (!shots[i].on) continue;
        u8 t = ++shots[i].t;
        s16 x = (s16)(shots[i].x0 + (shots[i].x1 - shots[i].x0) * t / SHOT_T);
        s16 y = (s16)(shots[i].y0 + (shots[i].y1 - shots[i].y0) * t / SHOT_T);
        s16 z = (s16)(shots[i].z0 - shots[i].z0 * t / SHOT_T);
        if (t >= SHOT_T) {
            for (u8 k = 0; k < nPl; k++) {
                Player* p = pls[k];
                if (!playerCanBeHit(p) || playerDrawZ(p) > 16) continue;
                if (fabs16((s16)(getPlayerHurtCX(p) - x)) > 14) continue;
                if (fabs16((s16)(getPlayerY(p) - y)) > 8) continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
                playerHitProjectile(p, x, SHOT_DMG);
            }
            SPR_releaseSprite(shots[i].s); shots[i].s = NULL;
            shots[i].on = FALSE;
            continue;
        }
        SPR_setPosition(shots[i].s, (s16)(x - 8 - camX), (s16)(y - z - 24));
        SPR_setDepth(shots[i].s, (s16)(-y - 1));
    }
}

static void beesUpdate(Player** pls, u8 nPl, s16 camX, s16 laneTop, s16 laneBot) {
    for (u16 i = 0; i < BEE_MAX; i++) {
        Bee* b = &bees[i];
        if (b->st == BEE_OFF) continue;
        b->t++;

        if (b->st == BEE_BOOM) {
            if (++b->tick >= 4) {
                b->tick = 0;
                if (++b->frame >= 7) { beeFree(b); continue; }
                SPR_setFrame(b->s, b->frame);
            }
            SPR_setPosition(b->s, (s16)(b->x - 16 - camX), (s16)(b->y - b->z - 24));
            continue;
        }

        Player* t = nearestP(pls, nPl, b->x);
        if (b->st == BEE_OUT) {
            // Sale de la puerta hacia adelante (abajo) subiendo un poco.
            b->y += 1;
            if (b->z < BEE_Z && (b->t & 1)) b->z++;
            if (b->t >= BEE_OUT_T) b->st = BEE_FLY;
        } else if (t) {
            // Se queda a un costado de la tortuga, en su lane, con vaiven.
            u16 sl = (u16)(i >> 1);
            s16 tx = (s16)(getPlayerHurtCX(t) + b->side * beeSlot[sl].dx);
            s16 ty = (s16)(getPlayerY(t) + beeSlot[sl].dy);
            u16 sx = (u16)(b->accX + BEE_SPEED_X_Q); b->accX = (u8)(sx & 0xFF);
            u16 sy = (u16)(b->accY + BEE_SPEED_Y_Q); b->accY = (u8)(sy & 0xFF);
            s16 stx = (s16)(sx >> 8), sty = (s16)(sy >> 8);
            if (b->x < tx - 2) b->x += stx; else if (b->x > tx + 2) b->x -= stx;
            if (b->y < ty) b->y += sty; else if (b->y > ty) b->y -= sty;
            if (b->y < laneTop) b->y = laneTop;
            if (b->y > laneBot) b->y = laneBot;
            // Hacia la altura de su lugar, con vaiven de +-2.
            s16 wz = (s16)(beeSlot[sl].z + (((b->t >> 3) & 3) == 1 ? 2 : (((b->t >> 3) & 3) == 3 ? -2 : 0)));
            if (b->z < wz) b->z++; else if (b->z > wz) b->z--;
            // Si quedo del otro lado (la tortuga paso de largo), cambia.
            if ((random() & 255) == 0) b->side = (s8)-b->side;
            if (b->fireT) b->fireT--;
            s16 dx = (s16)(getPlayerHurtCX(t) - b->x);
            if (!b->fireT && fabs16(dx) < 150 && fabs16((s16)(getPlayerY(t) - b->y)) < 24) {
                shotFire(b, t);
                b->fireT = (u16)(BEE_FIRE_MIN + (random() & 127));
            }
            b->face = (dx < -12) ? 0 : ((dx > 12) ? 2 : 1);
        }

        // Golpes de las tortugas (la abeja vuela: banda a su altura).
        if (b->hitCd) b->hitCd--;
        if (b->flash) b->flash--;
        if (!b->hitCd) {
            for (u8 k = 0; k < nPl; k++) {
                if (!playerAttackHitsBand(pls[k], b->x, b->y, 14,
                                          (s16)-(b->z + 28), (s16)-(b->z - 6))) continue;
                b->hitCd = BEE_HIT_CD;
                b->flash = BEE_FLASH;
                b->x += (getPlayerHurtCX(pls[k]) < b->x) ? 10 : -10;
                XGM2_playPCMEx(mouser_hit_sfx, sizeof(mouser_hit_sfx), SOUND_PCM_CH3, 15, FALSE, FALSE);
                if (--b->hp == 0) {
                    addPlayerScore(pls[k], 1);
                    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                                   SOUND_PCM_CH3, 15, FALSE, FALSE);
                    SPR_releaseSprite(b->s);
                    b->s = SPR_addSprite(&sewer_mouser_boom, -64, -64, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
                    if (b->sh) { SPR_releaseSprite(b->sh); b->sh = NULL; }
                    if (!b->s) { b->st = BEE_OFF; break; }
                    SPR_setAutoAnimation(b->s, FALSE);
                    SPR_setDepth(b->s, (s16)(-b->y - 1));
                    b->st = BEE_BOOM; b->frame = 0; b->tick = 0;
                }
                break;
            }
            if (b->st != BEE_OUT && b->st != BEE_FLY) continue;
        }

        // Dibujo: helice girando, naranja con el golpe.
        if (++b->tick >= BEE_ANIM_T) { b->tick = 0; if (++b->frame >= 3) b->frame = 0; }
        u8 anim = (u8)(b->face + (((b->flash >> 1) & 1) ? 3 : 0));
        SPR_setAnimAndFrame(b->s, anim, b->frame);
        SPR_setPosition(b->s, (s16)(b->x - 16 - camX), (s16)(b->y - b->z - 30));
        SPR_setDepth(b->s, (s16)-b->y);
        if (b->sh) {
            SPR_setPosition(b->sh, (s16)(b->x - 16 - camX), (s16)(b->y - 6));
            SPR_setDepth(b->sh, SPR_MAX_DEPTH);
        }
    }
}

static void capsuleUpdate(s16 camX) {
    if (cap.st == CAP_OFF) return;
    s16 sx = (s16)(cap.x - CAP_W / 2 - camX);
    if (cap.st == CAP_DONE && (sx < -CAP_W - 16 || sx > SCREEN_W + 16)) {
        if (cap.s) { SPR_releaseSprite(cap.s); cap.s = NULL; }
        return;
    }
    if (cap.s) {
        if (cap.pending) cap.pending = !capQueue(cap.wantAnim, cap.wantFr);
        else if ((cap.s->attribut & TILE_INDEX_MASK) != cap.vram) capQueue(cap.anim, cap.fr);
    }
    cap.t++;
    switch (cap.st) {
    case CAP_EMERGE:
        stageShake(2);
        if (cap.t >= CAP_EMERGE_T) {
            cap.t = 0;
            if (++cap.frame >= CAP_EMERGE_FR) {
                cap.st = CAP_DOOR; cap.frame = 0;
                capSetFrame(1, 0);
            } else capSetFrame(0, cap.frame);
        }
        break;
    case CAP_DOOR:
        if (cap.t >= CAP_DOOR_T) {
            cap.st = CAP_RELEASE; cap.t = CAP_BEE_GAP;
            capSetFrame(1, 1);
            XGM2_playPCMEx(capsule_door_sfx, sizeof(capsule_door_sfx), SOUND_PCM_CH3, 12, FALSE, FALSE);
        }
        break;
    case CAP_RELEASE:
        if (cap.t >= CAP_BEE_GAP) {
            if (beeSpawn(cap.x, (s16)(cap.baseY + 2), (s16)(CAP_DOOR_DY - 10))) {
                cap.t = 0;
                if (--cap.bees == 0) cap.st = CAP_DONE;
            }
        }
        break;
    default:
        break;
    }
    if (cap.s) SPR_setPosition(cap.s, sx, (s16)(cap.baseY - CAP_H));
}

void ffxUpdate(Player** pls, u8 nPl, s16 camX, s16 laneTop, s16 laneBot) {
    capsuleUpdate(camX);
    beesUpdate(pls, nPl, camX, laneTop, laneBot);
    shotsUpdate(pls, nPl, camX);
}

u16 ffxAlive(void) {
    u16 n = 0;
    if (cap.st == CAP_EMERGE || cap.st == CAP_DOOR || cap.st == CAP_RELEASE) n++;
    for (u16 i = 0; i < BEE_MAX; i++) if (bees[i].st != BEE_OFF) n++;
    return n;
}

void ffxRelease(void) {
    for (u16 i = 0; i < LZ_N; i++) lzFree(i);
    if (cap.s) { SPR_releaseSprite(cap.s); cap.s = NULL; }
    cap.st = CAP_OFF;
    for (u16 i = 0; i < BEE_MAX; i++) beeFree(&bees[i]);
    for (u16 i = 0; i < SHOT_MAX; i++)
        if (shots[i].s) { SPR_releaseSprite(shots[i].s); shots[i].s = NULL; shots[i].on = FALSE; }
}
