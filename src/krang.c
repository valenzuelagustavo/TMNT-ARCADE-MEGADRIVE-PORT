#include "krang.h"
#include "audio.h"    // hit_turtles, boss_hit, electric_shock_sfx, ...

// ===========================================================================
// KRANG — ver krang.h
// ===========================================================================

static s16 kabs(s16 v)                 { return (v < 0) ? -v : v; }
static s16 kclamp(s16 v, s16 a, s16 b) { return (v < a) ? a : ((v > b) ? b : v); }

static s16 kStep(u8* acc, u16 q) {
    u16 t = (u16)(*acc + q);
    *acc = (u8)(t & 0xFF);
    return (s16)(t >> 8);
}

static s16 kToward(s16 v, s16 to, s16 step) {
    if (v < to) return (to - v < step) ? to : (s16)(v + step);
    if (v > to) return (v - to < step) ? to : (s16)(v - step);
    return v;
}

static Player* nearestPlayer(Player** pls, u8 nPl, s16 x) {
    Player* best = pls[0];
    s16 bestD = 0x7FFF;
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k])) continue;
        s16 d = kabs((s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2 - x));
        if (d < bestD) { bestD = d; best = pls[k]; }
    }
    return best;
}

// Tortuga golpeada, derribada o con i-frames: "en el piso" para la IA.
static bool isDown(Player* p) { return !playerCanBeHitAir(p) && !isPlayerGameOver(p); }

// ---------------------------------------------------------------------------
// PUNO COHETE y RELAMPAGOS
// ---------------------------------------------------------------------------
#define MAX_KRANG_FISTS 2
#define MAX_KRANG_BOLTS 2

static struct {
    bool    active;
    Sprite* sprite;
    s16     x, lane;      // punta del puno (mundo) y lane
    s8      dir;
    u8      accX, t;
    u8      hitMask;
} fists[MAX_KRANG_FISTS];

static struct {
    bool    active;
    Sprite* sprite;       // el relampago
    Sprite* zap;          // el chispazo en el piso
    s16     x, lane;
    u8      t;
    u8      hitMask;
} bolts[MAX_KRANG_BOLTS];

static void shotsInit(void) {
    for (u16 i = 0; i < MAX_KRANG_FISTS; i++) { fists[i].active = FALSE; fists[i].sprite = NULL; }
    for (u16 i = 0; i < MAX_KRANG_BOLTS; i++) {
        bolts[i].active = FALSE; bolts[i].sprite = NULL; bolts[i].zap = NULL;
    }
}

static void fistFire(s16 x, s16 lane, s8 dir) {
    for (u16 i = 0; i < MAX_KRANG_FISTS; i++) {
        if (fists[i].active) continue;
        if (!fists[i].sprite)
            fists[i].sprite = SPR_addSprite(&krang_fist, -80, -80, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        fists[i].active = TRUE;
        fists[i].x = x;
        fists[i].lane = lane;
        fists[i].dir = dir;
        fists[i].accX = 0;
        fists[i].t = 0;
        fists[i].hitMask = 0;
        if (fists[i].sprite) {
            SPR_setAutoAnimation(fists[i].sprite, FALSE);
            SPR_setAnimAndFrame(fists[i].sprite, 0, 1);
            SPR_setHFlip(fists[i].sprite, dir < 0);
            SPR_setVisibility(fists[i].sprite, VISIBLE);
        }
        XGM2_playPCMEx(rocksteady_charge_sfx, sizeof(rocksteady_charge_sfx), SOUND_PCM_CH3, 12, FALSE, FALSE);
        return;
    }
}

static void boltDrop(s16 x, s16 lane) {
    for (u16 i = 0; i < MAX_KRANG_BOLTS; i++) {
        if (bolts[i].active) continue;
        if (!bolts[i].sprite)
            bolts[i].sprite = SPR_addSprite(&krang_bolt, -80, -80, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        if (!bolts[i].zap)
            bolts[i].zap = SPR_addSprite(&krang_zap, -80, -80, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        bolts[i].active = TRUE;
        bolts[i].x = x;
        bolts[i].lane = lane;
        bolts[i].t = 0;
        bolts[i].hitMask = 0;
        if (bolts[i].sprite) {
            SPR_setAutoAnimation(bolts[i].sprite, FALSE);
            SPR_setVisibility(bolts[i].sprite, VISIBLE);
        }
        if (bolts[i].zap) {
            SPR_setAutoAnimation(bolts[i].zap, FALSE);
            SPR_setVisibility(bolts[i].zap, HIDDEN);
        }
        XGM2_playPCMEx(electric_shock_sfx, sizeof(electric_shock_sfx), SOUND_PCM_CH3, 13, FALSE, FALSE);
        return;
    }
}

static void fistHide(u16 i) {
    fists[i].active = FALSE;
    if (fists[i].sprite) SPR_setVisibility(fists[i].sprite, HIDDEN);
}

static void boltHide(u16 i) {
    bolts[i].active = FALSE;
    if (bolts[i].sprite) SPR_setVisibility(bolts[i].sprite, HIDDEN);
    if (bolts[i].zap)    SPR_setVisibility(bolts[i].zap, HIDDEN);
}

void krangShotsUpdate(Player** pls, u8 nPl, s16 camX, s16 camY) {
    for (u16 i = 0; i < MAX_KRANG_FISTS; i++) {
        if (!fists[i].active) continue;
        fists[i].t++;
        fists[i].x += fists[i].dir * kStep(&fists[i].accX, KRANG_FIST_Q);
        if (fists[i].x < camX - 72 || fists[i].x > camX + 392) { fistHide(i); continue; }
        // La punta del puno alcanza a la tortuga de su lane (una vez a cada una).
        for (u8 k = 0; k < nPl; k++) {
            u8 bit = (u8)(1 << k);
            if ((fists[i].hitMask & bit) || !playerCanBeHit(pls[k])) continue;
            s16 px = getPlayerHurtCX(pls[k]);
            if (kabs((s16)(px - (fists[i].x - fists[i].dir * 8))) >= KRANG_FIST_HALF_W) continue;
            if (kabs((s16)(getPlayerY(pls[k]) - fists[i].lane)) > KRANG_FIST_DY) continue;
            fists[i].hitMask |= bit;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
            playerHitBars(pls[k], fists[i].x, KRANG_FIST_DMG);
        }
        if (fists[i].sprite) {
            // Despega con chispas (frames 1-2) y sigue con la llama (3-4).
            u8 f = (fists[i].t < 6) ? 1 : ((fists[i].t < 10) ? 2 : (u8)(3 + ((fists[i].t >> 2) & 1)));
            SPR_setFrame(fists[i].sprite, f);
            s16 left = (fists[i].dir > 0) ? (s16)(fists[i].x - 64) : fists[i].x;
            SPR_setPosition(fists[i].sprite, left - camX,
                            fists[i].lane - KRANG_FIST_DZ - 12 - camY);
            SPR_setDepth(fists[i].sprite, (s16)(-(fists[i].lane) - 1));
        }
    }
    for (u16 i = 0; i < MAX_KRANG_BOLTS; i++) {
        if (!bolts[i].active) continue;
        bolts[i].t++;
        if (bolts[i].t > KRANG_BOLT_LIFE) { boltHide(i); continue; }
        // Baja desde el techo en unos frames; cuando toca el piso, pega.
        s16 reach = (s16)(bolts[i].t * (128 / KRANG_BOLT_FALL));
        if (reach > 128) reach = 128;
        bool grounded = bolts[i].t >= KRANG_BOLT_FALL;
        if (grounded) {
            for (u8 k = 0; k < nPl; k++) {
                u8 bit = (u8)(1 << k);
                if ((bolts[i].hitMask & bit) || !playerCanBeHit(pls[k])) continue;
                s16 px = getPlayerHurtCX(pls[k]);
                if (kabs((s16)(px - bolts[i].x)) >= KRANG_BOLT_HALF_W) continue;
                if (kabs((s16)(getPlayerY(pls[k]) - bolts[i].lane)) > KRANG_BOLT_DY) continue;
                bolts[i].hitMask |= bit;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                playerHitBarsKnockdown(pls[k], bolts[i].x, KRANG_BOLT_DMG);
            }
        }
        if (bolts[i].sprite) {
            SPR_setFrame(bolts[i].sprite, (bolts[i].t >> 1) & 1);
            SPR_setPosition(bolts[i].sprite, bolts[i].x - 8 - camX, bolts[i].lane - reach - camY);
            SPR_setDepth(bolts[i].sprite, (s16)(-(bolts[i].lane) - 2));
            // titila al final
            SPR_setVisibility(bolts[i].sprite,
                              (bolts[i].t > KRANG_BOLT_LIFE - 8 && (bolts[i].t & 1)) ? HIDDEN : VISIBLE);
        }
        if (bolts[i].zap) {
            SPR_setVisibility(bolts[i].zap, grounded ? VISIBLE : HIDDEN);
            SPR_setFrame(bolts[i].zap, (bolts[i].t >> 2) & 1);
            SPR_setPosition(bolts[i].zap, bolts[i].x - 16 - camX, bolts[i].lane - 12 - camY);
            SPR_setDepth(bolts[i].zap, (s16)(-(bolts[i].lane) - 3));
        }
    }
}

void krangShotsReleaseAll(void) {
    for (u16 i = 0; i < MAX_KRANG_FISTS; i++) {
        if (fists[i].sprite) { SPR_releaseSprite(fists[i].sprite); fists[i].sprite = NULL; }
        fists[i].active = FALSE;
    }
    for (u16 i = 0; i < MAX_KRANG_BOLTS; i++) {
        if (bolts[i].sprite) { SPR_releaseSprite(bolts[i].sprite); bolts[i].sprite = NULL; }
        if (bolts[i].zap)    { SPR_releaseSprite(bolts[i].zap);    bolts[i].zap = NULL; }
        bolts[i].active = FALSE;
    }
}

// ---------------------------------------------------------------------------
// KRANG
// ---------------------------------------------------------------------------
static u16 krangPal[16], krangPalBurn[16];

void krangInit(Krang* k) {
    memset(k, 0, sizeof(Krang));
    k->state = KRANG_INACTIVE;
    k->dir = -1;
    k->hp = KRANG_HP;
    shotsInit();
}

static void setFrame(Krang* k, u8 anim, u8 frame) {
    SPR_setAnimAndFrame(k->sprite, anim, frame);
}

static void enter(Krang* k, KrangState s) {
    k->state = s;
    k->timer = 0;
    k->hitMask = 0;
}

void krangSpawn(Krang* k, s16 x, s16 arenaL, s16 arenaR, s16 laneT, s16 laneB) {
    for (u16 i = 0; i < 16; i++) krangPal[i] = krang_boss.palette->data[i];
    bossFlashBurn(krangPal, krangPalBurn);
    PAL_setPalette(PAL2, krangPal, DMA);
    bossFlashReset(&k->flashLow);
    if (!k->sprite)
        k->sprite = SPR_addSpriteSafe(&krang_boss, -160, -160, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    k->arenaL = arenaL; k->arenaR = arenaR;
    k->laneT = laneT;   k->laneB = laneB;
    k->xq = (s16)(x * 4);
    k->yq = (s16)(((laneT + laneB) / 2) * 4);
    k->z = KRANG_DROP_Z;
    k->vz = 0;
    k->dir = -1;
    k->hp = KRANG_HP;
    k->armReady = 1;
    k->flash = k->invuln = 0;
    enter(k, KRANG_DROP);
    if (k->sprite) {
        SPR_setAutoAnimation(k->sprite, FALSE);
        setFrame(k, KRANG_ANIM_IDLE, 0);
        SPR_setVisibility(k->sprite, VISIBLE);
    }
}

bool krangIsDying(const Krang* k) {
    return k->state == KRANG_DEATH || k->state == KRANG_HEAD || k->state == KRANG_GONE;
}
bool krangIsGone(const Krang* k) { return k->state == KRANG_GONE; }

static bool canBeHit(const Krang* k) {
    return k->sprite && !k->invuln && (k->state == KRANG_IDLE || k->state == KRANG_WALK);
}

bool krangPlayerHits(Krang* k, Player** pls, u8 nPl, s8* killer) {
    if (!canBeHit(k)) return FALSE;
    s16 x = (s16)(k->xq >> 2);
    s16 y = (s16)(k->yq >> 2);
    for (u8 i = 0; i < nPl; i++) {
        if (!playerAttackHitsBox(pls[i], x, y, KRANG_BODY_HALF_W, KRANG_BODY_H)) continue;
        s16 dmg = isPlayerSpecialAttack(pls[i]) ? KRANG_SPECIAL_DMG
                : (isPlayerJumpKicking(pls[i]) ? KRANG_JUMPKICK_DMG : 1);
        XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
        k->hp -= dmg;
        k->flash = 8;
        k->invuln = 12;
        if (k->hp <= 0) {
            k->hp = 0;
            enter(k, KRANG_DEATH);
            PAL_setPalette(PAL2, krangPal, DMA);
            XGM2_stop();               // el tema se corta con el ultimo golpe
            XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 15, FALSE, FALSE);
            if (killer) *killer = (s8)i;
            return TRUE;
        }
        enter(k, KRANG_HURT);
        XGM2_playPCMEx(electric_shock_sfx, sizeof(electric_shock_sfx), SOUND_PCM_CH3, 12, FALSE, FALSE);
        return FALSE;
    }
    return FALSE;
}

static void toWalk(Krang* k) { enter(k, KRANG_WALK); k->accX = k->accY = 0; }

// Despues de patada / rayo / golpeado: burla si la tortuga quedo en el piso.
static void afterAction(Krang* k, bool near, bool tgtDown, u8 tauntOdds) {
    if (near) {
        if (tgtDown) enter(k, KRANG_TAUNT);
        else toWalk(k);
    } else if ((random() % tauntOdds) == 0) {
        enter(k, KRANG_TAUNT);
    } else {
        toWalk(k);
    }
}

static void faceTarget(Krang* k, s16 x, s16 px) {
    if (x < px - KRANG_TURN_DZ) k->dir = 1;
    else if (x > px + KRANG_TURN_DZ) k->dir = -1;
}

// Ataque cuerpo a cuerpo: decide entre patada y rayo.
static void meleeOrLaser(Krang* k) {
    if (random() & 1) enter(k, KRANG_KICK);
    else enter(k, KRANG_LASER);
}

static void headSpawn(Krang* k) {
    if (!k->head)
        k->head = SPR_addSprite(&krang_head, -40, -40, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (!k->headShadow)
        k->headShadow = SPR_addSprite(&krang_head, -40, -40, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (k->head) {
        SPR_setAutoAnimation(k->head, FALSE);
        SPR_setAnimAndFrame(k->head, 0, 0);
    }
    if (k->headShadow) {
        SPR_setAutoAnimation(k->headShadow, FALSE);
        SPR_setAnimAndFrame(k->headShadow, 1, 0);
    }
    k->z = KRANG_HEAD_DZ;
    XGM2_playPCMEx(krang_speech_vo, sizeof(krang_speech_vo), SOUND_PCM_CH1, 15, FALSE, FALSE);
}

static void render(Krang* k, bool visible) {
    s16 x = (s16)(k->xq >> 2);
    s16 y = (s16)(k->yq >> 2);
    SPR_setHFlip(k->sprite, k->dir < 0);
    SPR_setPosition(k->sprite, x - k->camX - KRANG_FRAME_W / 2,
                    y - k->camY - KRANG_FOOT_OFFSET - (k->state == KRANG_DROP ? k->z : 0));
    SPR_setDepth(k->sprite, (s16)(-y));
    SPR_setVisibility(k->sprite, (visible && !(k->flash & 1)) ? VISIBLE : HIDDEN);
}

static const s8 bob[16] = { 0, 1, 2, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -2, -1 };

void krangUpdate(Krang* k, Player** pls, u8 nPl, s16 camX, s16 camY) {
    if (k->state == KRANG_INACTIVE || k->state == KRANG_GONE) return;
    k->camX = camX;
    k->camY = camY;
    s16 x = (s16)(k->xq >> 2);
    s16 y = (s16)(k->yq >> 2);

    // La cabeza: flota hablando y se escapa por arriba.
    if (k->state == KRANG_HEAD) {
        k->timer++;
        if (k->timer > KRANG_HEAD_FLOAT) k->z += 2;
        s16 hz = (s16)(k->z + ((k->timer <= KRANG_HEAD_FLOAT) ? bob[(k->timer >> 2) & 15] : 0));
        if (k->head) {
            SPR_setFrame(k->head, (u8)((k->timer / 10) % 5));
            SPR_setPosition(k->head, x - 16 - camX, y - hz - 32 - camY);
            SPR_setDepth(k->head, (s16)(-y - 1));
        }
        if (k->headShadow) {
            SPR_setPosition(k->headShadow, x - 16 - camX, y - 16 - camY);
            SPR_setDepth(k->headShadow, (s16)(-y + 1));
            SPR_setVisibility(k->headShadow, (k->timer <= KRANG_HEAD_FLOAT) ? VISIBLE : HIDDEN);
        }
        if (y - hz - 32 - camY < -40) {
            if (k->head)       { SPR_releaseSprite(k->head);       k->head = NULL; }
            if (k->headShadow) { SPR_releaseSprite(k->headShadow); k->headShadow = NULL; }
            k->state = KRANG_GONE;
        }
        return;
    }
    if (!k->sprite) return;

    Player* tgt = nearestPlayer(pls, nPl, x);
    s16 px = (s16)(getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2);
    s16 py = getPlayerY(tgt);
    s16 dX = (s16)(px - x);
    s16 dY = (s16)(py - y);
    bool alignY  = kabs(dY) <= KRANG_ALIGN_DY;
    bool near    = alignY && kabs(dX) <= KRANG_NEAR_DX;
    bool far     = alignY && kabs(dX) >  KRANG_FAR_DX;
    bool gameOn  = !isPlayerGameOver(tgt);
    bool tgtDown = isDown(tgt);
    bool visible = TRUE;

    k->timer++;
    if (k->flash)  k->flash--;
    if (k->invuln) k->invuln--;

    switch (k->state) {

    case KRANG_DROP:
        // Cae del techo con la pose de quieto; al tocar el piso, burla.
        k->vz += 1;
        k->z -= k->vz;
        k->dir = (dX >= 0) ? 1 : -1;
        if (k->z <= 0) {
            k->z = 0;
            XGM2_playPCMEx(iron_ball_sfx, sizeof(iron_ball_sfx), SOUND_PCM_CH3, 14, FALSE, FALSE);
            enter(k, KRANG_TAUNT);
        }
        break;

    case KRANG_IDLE:
        setFrame(k, KRANG_ANIM_IDLE, 0);
        faceTarget(k, x, px);
        if (k->timer < KRANG_IDLE_T || !gameOn) break;
        if (near) enter(k, KRANG_KICK);
        else toWalk(k);
        break;

    case KRANG_WALK: {
        setFrame(k, KRANG_ANIM_WALK, (u8)((k->timer / KRANG_WALK_FRAME) % 7));
        faceTarget(k, x, px);
        s16 sx = kStep(&k->accX, KRANG_WALK_Q);
        s16 sy = kStep(&k->accY, KRANG_WALK_Q);
        if (dX > KRANG_MIN_DIST)       k->xq += sx * 4;
        else if (dX < -KRANG_MIN_DIST) k->xq -= sx * 4;
        k->yq = (s16)(kToward(y, py, sy) * 4);
        if (!gameOn || k->timer < KRANG_WALK_REACT) break;
        if (near) meleeOrLaser(k);
        else if (far && k->armReady) { enter(k, KRANG_ARM); k->armReady = 0; }
        break;
    }

    case KRANG_KICK: {
        u8 f = (k->timer < 9) ? 0 : ((k->timer < KRANG_KICK_HIT_T) ? 1 : 2);
        setFrame(k, KRANG_ANIM_KICK, f);
        if (f == 2) {
            for (u8 i = 0; i < nPl; i++) {
                u8 bit = (u8)(1 << i);
                if ((k->hitMask & bit) || !playerCanBeHit(pls[i])) continue;
                s16 fwd = (s16)((getPlayerHurtCX(pls[i]) - x) * k->dir);
                if (fwd < 0 || fwd > KRANG_KICK_REACH) continue;
                if (kabs((s16)(getPlayerY(pls[i]) - y)) > KRANG_HIT_DY) continue;
                k->hitMask |= bit;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                playerHitBarsKnockdown(pls[i], x, KRANG_KICK_DMG);
            }
        }
        if (k->timer >= KRANG_KICK_T) { k->armReady = 1; afterAction(k, near, tgtDown, 2); }
        break;
    }

    case KRANG_LASER:
        setFrame(k, KRANG_ANIM_LASER, (u8)((k->timer < 12) ? 0 : ((k->timer < KRANG_BOLT_AT) ? 1 : 2)));
        if (k->timer == KRANG_BOLT_AT) {
            // Cae sobre la tortuga si esta delante y a tiro; si no, delante.
            s16 fwd = (s16)(dX * k->dir);
            fwd = kclamp(fwd, KRANG_BOLT_MIN, KRANG_BOLT_MAX);
            boltDrop((s16)(x + k->dir * fwd), py);
        }
        if (k->timer >= KRANG_LASER_T) { k->armReady = 1; afterAction(k, near, tgtDown, 2); }
        break;

    case KRANG_ARM: {
        u8 f = (u8)(k->timer / 12);
        if (f > 3) f = 3;
        setFrame(k, KRANG_ANIM_ARM, f);
        if (k->timer == KRANG_FIST_AT) fistFire((s16)(x + k->dir * KRANG_FIST_DX), y, k->dir);
        if (k->timer >= KRANG_ARM_T) {
            if (random() & 1) toWalk(k);
            else enter(k, KRANG_TAUNT);
        }
        break;
    }

    case KRANG_TAUNT:
        // 2 frames (12 y 15 ticks) dos veces.
        setFrame(k, KRANG_ANIM_TAUNT, (u8)(((k->timer % 27) < 12) ? 0 : 1));
        if (k->timer >= KRANG_TAUNT_T) toWalk(k);
        break;

    case KRANG_HURT:
        setFrame(k, KRANG_ANIM_HURT, (u8)((k->timer >> 3) & 3));
        if (k->timer >= KRANG_HURT_TICKS) afterAction(k, near, tgtDown, 3);
        break;

    case KRANG_DEATH: {
        // Grita (60), se electrocuta estallando (150) y destello amarillo (30).
        u16 t = k->timer;
        if (t < 60) {
            setFrame(k, KRANG_ANIM_DEATH, (u8)((t / 15) & 1));
        } else if (t < 210) {
            setFrame(k, KRANG_ANIM_DEATH, (u8)(2 + ((t / 6) % 5)));
            if ((t % 30) == 0)
                XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                               SOUND_PCM_CH3, 14, FALSE, FALSE);
            visible = ((t >> 1) & 3) != 0;
        } else {
            setFrame(k, KRANG_ANIM_DEATH, 7);
            visible = (t & 2) != 0;
        }
        if (t >= KRANG_DEATH_T) {
            SPR_releaseSprite(k->sprite);
            k->sprite = NULL;
            krangShotsReleaseAll();
            enter(k, KRANG_HEAD);
            headSpawn(k);
            return;
        }
        break;
    }

    default:
        break;
    }

    // Parpadeo de vida baja (PAL2 es solo suya mientras pelea).
    if (k->state != KRANG_DEATH && bossFlashStep(&k->flashLow, k->hp, KRANG_HP))
        PAL_setPalette(PAL2, k->flashLow.on ? krangPalBurn : krangPal, DMA);

    x = kclamp((s16)(k->xq >> 2), k->arenaL, k->arenaR);
    k->xq = (s16)(x * 4);
    y = kclamp((s16)(k->yq >> 2), k->laneT, k->laneB);
    k->yq = (s16)(y * 4);
    render(k, visible);
}

void krangRelease(Krang* k) {
    if (k->sprite)     { SPR_releaseSprite(k->sprite);     k->sprite = NULL; }
    if (k->head)       { SPR_releaseSprite(k->head);       k->head = NULL; }
    if (k->headShadow) { SPR_releaseSprite(k->headShadow); k->headShadow = NULL; }
    k->state = KRANG_INACTIVE;
    krangShotsReleaseAll();
}
