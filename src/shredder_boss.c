#include "shredder_boss.h"
#include "audio.h"    // boss_hit, hit_turtles, shredder_laugh_sfx, shredder_death_vo, ...

// ===========================================================================
// SHREDDER (jefe final) — ver shredder_boss.h
// ===========================================================================

static s16 sabs(s16 v)                 { return (v < 0) ? -v : v; }
static s16 sclamp(s16 v, s16 a, s16 b) { return (v < a) ? a : ((v > b) ? b : v); }

static s16 sStep(u8* acc, u16 q) {
    u16 t = (u16)(*acc + q);
    *acc = (u8)(t & 0xFF);
    return (s16)(t >> 8);
}

static s16 sToward(s16 v, s16 to, s16 step) {
    if (v < to) return (to - v < step) ? to : (s16)(v + step);
    if (v > to) return (v - to < step) ? to : (s16)(v - step);
    return v;
}

static Player* nearestPlayer(Player** pls, u8 nPl, s16 x) {
    Player* best = pls[0];
    s16 bestD = 0x7FFF;
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k])) continue;
        s16 d = sabs((s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2 - x));
        if (d < bestD) { bestD = d; best = pls[k]; }
    }
    return best;
}

static bool isDown(Player* p) { return !playerCanBeHitAir(p) && !isPlayerGameOver(p); }

// Ataques 1-4: ticks de cada frame, frame desde el que pega, alcance y si
// derriba.
static const u8 atkTicks[4][4] = { { 5, 5, 5, 9 }, { 4, 4, 2, 8 }, { 4, 4, 3, 10 }, { 4, 4, 3, 8 } };
static const u8 atkHitFrame[4] = { 2, 2, 2, 2 };
static const u8 atkReach[4]    = { 64, 46, 46, 50 };
static const u8 atkKnock[4]    = { 0, 1, 0, 1 };

// El rayo: ticks de cada frame (40 en total) y hasta donde llega.
static const u8 beamTicks[10]  = { 3, 2, 3, 2, 5, 5, 5, 5, 5, 5 };
static const u16 beamReach[10] = { 40, 71, 104, 139, 171, 200, 234, 265, 269, 268 };

#define SHRED_NO_RETURN 0xFFFF

// Un solo rayo a la vez entre el verdadero y la copia.
static ShredderBoss* rayOwner = NULL;

static u8 animOf(const ShredderBoss* s, u8 a) {
    if (!s->bare) return a;
    if (a <= SHRED_ANIM_ATK1 + 3) return (u8)(SHRED_ANIM_BARE + a);
    if (a == SHRED_ANIM_HURT) return SHRED_ANIM_B_HURT;
    return a;
}

static void setFrame(ShredderBoss* s, u8 anim, u8 frame) {
    SPR_setAnimAndFrame(s->sprite, animOf(s, anim), frame);
}

static void enter(ShredderBoss* s, ShredState st) {
    s->state = st;
    s->timer = 0;
    s->hitMask = 0;
}

void shredderInit(ShredderBoss* s) {
    memset(s, 0, sizeof(ShredderBoss));
    s->state = SHRED_INACTIVE;
    rayOwner = NULL;
    s->dir = -1;
}

static void hideExtras(ShredderBoss* s) {
    s->beamOn = 0;
    if (rayOwner == s) rayOwner = NULL;
    if (s->beamA) SPR_setVisibility(s->beamA, HIDDEN);
    if (s->beamB) SPR_setVisibility(s->beamB, HIDDEN);
    if (s->fx)    SPR_setVisibility(s->fx, HIDDEN);
}

static void appear(ShredderBoss* s, s16 x, s16 y) {
    s->xq = (s16)(x * 4);
    s->yq = (s16)(y * 4);
    s->hp = s->clone ? SHRED_CLONE_HP : SHRED_HP;
    s->bare = FALSE;
    s->hits = 0;
    s->rayReady = 0;
    s->invuln = s->flash = 0;
    hideExtras(s);
    enter(s, SHRED_APPEAR);
    if (!s->sprite)
        s->sprite = SPR_addSpriteSafe(&shredder_boss, -160, -160, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (s->sprite) {
        SPR_setAutoAnimation(s->sprite, FALSE);
        setFrame(s, SHRED_ANIM_APPEAR, 0);
        SPR_setVisibility(s->sprite, VISIBLE);
    }
}

void shredderSpawn(ShredderBoss* s, s16 x, s16 y, s16 arenaL, s16 arenaR, s16 laneT, s16 laneB,
                   bool clone) {
    PAL_setPalette(PAL2, shredder_boss.palette->data, DMA);
    s->clone = clone;
    s->arenaL = arenaL; s->arenaR = arenaR;
    s->laneT = laneT;   s->laneB = laneB;
    s->dir = -1;
    s->backTimer = 0;
    appear(s, sclamp(x, arenaL, arenaR), sclamp(y, laneT, laneB));
    // La risa del verdadero la toca level9_1 (bossVoStart, antes del tema).
    if (clone)
        XGM2_playPCMEx(shredder_laugh_sfx, sizeof(shredder_laugh_sfx), SOUND_PCM_CH2, 10, FALSE, FALSE);
}

bool shredderIsDying(const ShredderBoss* s) { return s->state == SHRED_DEATH || s->state == SHRED_GONE; }
bool shredderIsGone(const ShredderBoss* s)  { return s->state == SHRED_GONE; }
bool shredderIsReady(const ShredderBoss* s) {
    return s->state != SHRED_INACTIVE && s->state != SHRED_APPEAR;
}

static bool canBeHit(const ShredderBoss* s) {
    if (!s->sprite || s->invuln) return FALSE;
    return s->state == SHRED_WALK || (s->state == SHRED_ATTACK && s->atk < 3);
}

static void helmetOff(ShredderBoss* s, s16 x, s16 y) {
    s->bare = TRUE;
    if (!s->helmet)
        s->helmet = SPR_addSprite(&shredder_helmet, -40, -40, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    s->helmX = x;
    s->helmY = y;
    s->helmZ = 70 << 4;
    s->helmVz = 48;
    s->helmDir = (s8)-s->dir;
    if (s->helmet) SPR_setVisibility(s->helmet, VISIBLE);
}

static void startDeath(ShredderBoss* s) {
    hideExtras(s);
    enter(s, SHRED_DEATH);
    if (!s->clone) {
        XGM2_stop();
        XGM2_playPCMEx(shredder_death_vo, sizeof(shredder_death_vo), SOUND_PCM_CH1, 15, FALSE, FALSE);
    }
}

bool shredderPlayerHits(ShredderBoss* s, Player** pls, u8 nPl, s8* killer) {
    if (!canBeHit(s)) return FALSE;
    s16 x = (s16)(s->xq >> 2);
    s16 y = (s16)(s->yq >> 2);
    for (u8 k = 0; k < nPl; k++) {
        // Solo de frente: la tortuga lo tiene que estar mirando.
        if (getPlayerDir(pls[k]) != -s->dir) continue;
        if (!playerAttackHitsBox(pls[k], x, y, SHRED_BODY_HALF_W, SHRED_BODY_H)) continue;
        s16 dmg = isPlayerSpecialAttack(pls[k]) ? SHRED_SPECIAL_DMG
                : (isPlayerJumpKicking(pls[k]) ? SHRED_JUMPKICK_DMG : 1);
        XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
        s->hp -= dmg;
        s->invuln = SHRED_INVULN;
        s->flash = 6;
        u8 hurtAnim = SHRED_ANIM_HURT;
        if (s->clone && !s->bare && s->hp < SHRED_HELMET_HP) {
            helmetOff(s, x, y);
            hurtAnim = 0xFF;                 // este golpe: el de casco
        }
        if (s->hp <= 0) {
            s->hp = 0;
            XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 15, FALSE, FALSE);
            if (s->clone) {
                hideExtras(s);
                enter(s, SHRED_DOWN);
            } else {
                startDeath(s);
            }
            if (killer) *killer = (s8)k;
            return TRUE;
        }
        XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 12, FALSE, FALSE);
        hideExtras(s);
        if (++s->hits >= SHRED_COUNTER_HITS) {
            // Cuarto golpe seguido: contraataca con la estocada.
            s->hits = 0;
            enter(s, SHRED_ATTACK);
            s->atk = 0;
        } else {
            enter(s, SHRED_HURT);
            s->atk = hurtAnim;               // que frame de golpeado usar
        }
        return FALSE;
    }
    return FALSE;
}

static void faceTarget(ShredderBoss* s, s16 x, s16 px) {
    if (x < px - SHRED_TURN_DZ) s->dir = 1;
    else if (x > px + SHRED_TURN_DZ) s->dir = -1;
}

static void startAttack(ShredderBoss* s, u8 n) {
    enter(s, SHRED_ATTACK);
    s->atk = n;
}

static bool canRay(const ShredderBoss* s) {
    return !s->bare && (rayOwner == NULL || rayOwner == s);
}

static void startRay(ShredderBoss* s) {
    rayOwner = s;
    enter(s, SHRED_RAY);
    if (!s->fx)
        s->fx = SPR_addSprite(&shredder_fx, -40, -40, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (s->fx) {
        SPR_setAutoAnimation(s->fx, FALSE);
        SPR_setAnimAndFrame(s->fx, 0, 0);
        SPR_setVisibility(s->fx, VISIBLE);
    }
}

// Despues de un ataque o del rayo.
static void afterAttack(ShredderBoss* s, bool near, bool tgtDown) {
    if (!near) {
        if (random() & 1) enter(s, SHRED_WALK);
        else enter(s, SHRED_STANCE);
    } else if (tgtDown) {
        enter(s, SHRED_STANCE);
    } else {
        enter(s, SHRED_WALK);
    }
}

static void beamFire(ShredderBoss* s, s16 x, s16 y) {
    if (!s->beamA)
        s->beamA = SPR_addSprite(&shredder_beam_a, -200, -200, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (!s->beamB)
        s->beamB = SPR_addSprite(&shredder_beam_b, -200, -200, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    s->beamOn = 1;
    s->beamT = 0;
    s->beamDir = s->dir;
    s->beamX = (s16)(x + s->dir * SHRED_TIP_DX);
    s->beamY = y;
    s->hitMask = 0;
    Sprite* parts[2] = { s->beamA, s->beamB };
    for (u8 i = 0; i < 2; i++) {
        if (!parts[i]) continue;
        SPR_setAutoAnimation(parts[i], FALSE);
        SPR_setAnimAndFrame(parts[i], 0, 0);
        SPR_setHFlip(parts[i], s->dir < 0);
        // Atras de todo: si en una linea se pasan los sprites, se corta el
        // rayo y no las tortugas.
        SPR_setDepth(parts[i], 0x7000);
        SPR_setVisibility(parts[i], VISIBLE);
    }
    XGM2_playPCMEx(electric_shock_sfx, sizeof(electric_shock_sfx), SOUND_PCM_CH3, 14, FALSE, FALSE);
}

static void beamUpdate(ShredderBoss* s, Player** pls, u8 nPl) {
    if (!s->beamOn) return;
    s->beamT++;
    u8 f = 0;
    u16 acc = 0;
    while (f < 9 && s->beamT >= acc + beamTicks[f]) { acc += beamTicks[f]; f++; }
    if (s->beamT >= SHRED_BEAM_T) { hideExtras(s); return; }
    // A la tortuga de su lane que alcance: pierde la vida entera.
    for (u8 k = 0; k < nPl; k++) {
        u8 bit = (u8)(1 << k);
        if ((s->hitMask & bit) || !playerCanBeHit(pls[k])) continue;
        s16 fwd = (s16)((getPlayerHurtCX(pls[k]) - s->beamX) * s->beamDir);
        if (fwd < -8 || fwd > (s16)beamReach[f]) continue;
        if (sabs((s16)(getPlayerY(pls[k]) - s->beamY)) > SHRED_BEAM_DY) continue;
        s->hitMask |= bit;
        XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
        playerHitBarsKnockdown(pls[k], s->beamX, PLAYER_MAX_HEALTH);
    }
    s16 sx = (s16)(s->beamX - s->camX);
    s16 sy = (s16)(s->beamY - SHRED_TIP_DZ - 76 - s->camY);
    if (s->beamA) {
        SPR_setFrame(s->beamA, f);
        SPR_setPosition(s->beamA, (s->beamDir > 0) ? sx : (s16)(sx - 136), sy);
    }
    if (s->beamB) {
        SPR_setFrame(s->beamB, f);
        SPR_setPosition(s->beamB, (s->beamDir > 0) ? (s16)(sx + 136) : (s16)(sx - 272), sy);
    }
}

static void helmetUpdate(ShredderBoss* s) {
    if (!s->helmet) return;
    s->helmX += s->helmDir * 2;
    s->helmZ += s->helmVz;
    s->helmVz -= 4;
    if (s->helmZ <= 0) { s->helmZ = 0; s->helmVz = 40; }   // rebota
    s16 sx = (s16)(s->helmX - s->camX);
    if (sx < -24 || sx > 344) {
        SPR_releaseSprite(s->helmet);
        s->helmet = NULL;
        return;
    }
    SPR_setPosition(s->helmet, sx - 8, s->helmY - (s->helmZ >> 4) - 12 - s->camY);
    SPR_setDepth(s->helmet, (s16)(-s->helmY - 1));
}

static void render(ShredderBoss* s, bool visible) {
    s16 x = (s16)(s->xq >> 2);
    s16 y = (s16)(s->yq >> 2);
    SPR_setHFlip(s->sprite, s->dir < 0);
    SPR_setPosition(s->sprite, x - s->camX - SHRED_FRAME_W / 2, y - s->camY - SHRED_FOOT_OFFSET);
    SPR_setDepth(s->sprite, (s16)(-y));
    SPR_setVisibility(s->sprite, (visible && !(s->flash & 1)) ? VISIBLE : HIDDEN);
}

static void fxAt(ShredderBoss* s, u8 anim, u8 frame, s16 wx, s16 wy) {
    if (!s->fx) return;
    SPR_setAnimAndFrame(s->fx, anim, frame);
    SPR_setHFlip(s->fx, s->dir < 0);
    SPR_setPosition(s->fx, wx - 16 - s->camX, wy - 16 - s->camY);
    SPR_setDepth(s->fx, (s16)(-(s->yq >> 2) - 2));
    SPR_setVisibility(s->fx, VISIBLE);
}

void shredderVanish(ShredderBoss* s) {
    s->backTimer = SHRED_NO_RETURN;
    if (s->state == SHRED_INACTIVE || s->state == SHRED_GONE || s->state == SHRED_DEATH) return;
    startDeath(s);
}

void shredderUpdate(ShredderBoss* s, Player** pls, u8 nPl, s16 camX, s16 camY) {
    if (s->state == SHRED_INACTIVE) return;
    s->camX = camX;
    s->camY = camY;
    helmetUpdate(s);

    if (s->state == SHRED_GONE) {
        // La copia vuelve a los 6 s (con casco y media vida), mas abajo.
        if (s->clone && s->backTimer != SHRED_NO_RETURN && s->backTimer && --s->backTimer == 0) {
            appear(s, (s16)(s->xq >> 2), (s16)((s->yq >> 2) + 25));
            s->yq = (s16)(sclamp((s16)(s->yq >> 2), s->laneT, s->laneB) * 4);
            XGM2_playPCMEx(shredder_laugh_sfx, sizeof(shredder_laugh_sfx), SOUND_PCM_CH2, 10, FALSE, FALSE);
        }
        return;
    }
    if (!s->sprite) return;

    s16 x = (s16)(s->xq >> 2);
    s16 y = (s16)(s->yq >> 2);
    Player* tgt = nearestPlayer(pls, nPl, x);
    s16 px = (s16)(getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2);
    s16 py = getPlayerY(tgt);
    s16 dX = (s16)(px - x);
    s16 dY = (s16)(py - y);
    bool alignY  = sabs(dY) <= SHRED_ALIGN_DY;
    bool near    = alignY && sabs(dX) <= SHRED_NEAR_DX;
    bool far     = alignY && sabs(dX) >  SHRED_FAR_DX;
    bool gameOn  = !isPlayerGameOver(tgt);
    bool tgtDown = isDown(tgt);
    bool visible = TRUE;

    s->timer++;
    if (s->invuln) s->invuln--;
    if (s->flash)  s->flash--;
    beamUpdate(s, pls, nPl);

    switch (s->state) {

    case SHRED_APPEAR: {
        u8 f = (u8)((s->timer * 7) / SHRED_APPEAR_T);
        if (f > 6) f = 6;
        setFrame(s, SHRED_ANIM_APPEAR, f);
        s->dir = (dX >= 0) ? 1 : -1;
        if (s->timer >= SHRED_APPEAR_T) enter(s, SHRED_STANCE);
        break;
    }

    case SHRED_STANCE:
        setFrame(s, SHRED_ANIM_IDLE, 0);
        faceTarget(s, x, px);
        if (s->timer < SHRED_STANCE_T || !gameOn) break;
        if (!near) enter(s, SHRED_WALK);
        else if (tgtDown) enter(s, SHRED_WAIT);
        else startAttack(s, (u8)(random() & 3));
        break;

    case SHRED_WAIT:
        // Espera a que la tortuga se levante.
        setFrame(s, SHRED_ANIM_IDLE, 0);
        faceTarget(s, x, px);
        if (!tgtDown || s->timer >= SHRED_WAIT_T) enter(s, SHRED_STANCE);
        break;

    case SHRED_WALK: {
        faceTarget(s, x, px);
        s16 spot = (s16)(px + (s->clone ? -SHRED_SIDE_DX : SHRED_SIDE_DX));
        s16 sx = sStep(&s->accX, SHRED_WALK_Q);
        s16 sy = sStep(&s->accY, SHRED_WALK_Q);
        if (x < spot - 2)      s->xq += sx * 4;
        else if (x > spot + 2) s->xq -= sx * 4;
        s->yq = (s16)(sToward(y, py, sy) * 4);
        setFrame(s, (dY < -1) ? SHRED_ANIM_WALKUP : SHRED_ANIM_WALK,
                 (u8)((s->timer / SHRED_WALK_FRAME) % 6));
        if (!gameOn || s->timer < SHRED_WALK_REACT) break;
        if (near) {
            if (tgtDown) {
                if (random() & 1) enter(s, SHRED_WAIT);
                else enter(s, SHRED_BLOCK);
            } else if (!canRay(s) || (random() & 1)) {
                startAttack(s, (u8)(random() & 3));
            } else {
                startRay(s);
            }
        } else if (far && !tgtDown && s->rayReady && canRay(s)) {
            startRay(s);
        }
        break;
    }

    case SHRED_ATTACK: {
        const u8* tk = atkTicks[s->atk];
        u8 f = 0;
        u16 acc = tk[0];
        while (f < 3 && s->timer >= acc) { f++; acc += tk[f]; }
        setFrame(s, (u8)(SHRED_ANIM_ATK1 + s->atk), f);
        if (f >= atkHitFrame[s->atk]) {
            for (u8 k = 0; k < nPl; k++) {
                u8 bit = (u8)(1 << k);
                if ((s->hitMask & bit) || !playerCanBeHit(pls[k])) continue;
                s16 fwd = (s16)((getPlayerHurtCX(pls[k]) - x) * s->dir);
                if (fwd < -4 || fwd > atkReach[s->atk]) continue;
                if (sabs((s16)(getPlayerY(pls[k]) - y)) > SHRED_HIT_DY) continue;
                s->hitMask |= bit;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                if (atkKnock[s->atk]) playerHitBarsKnockdown(pls[k], x, SHRED_ATK_DMG);
                else playerHitBars(pls[k], x, SHRED_ATK_DMG);
            }
        }
        if (s->timer >= tk[0] + tk[1] + tk[2] + tk[3]) afterAttack(s, near, tgtDown);
        break;
    }

    case SHRED_RAY: {
        u16 t = s->timer;
        if (t < SHRED_RAY_CHARGE) {
            setFrame(s, SHRED_ANIM_RAY, 0);
            u8 f = (u8)(t / 7);
            fxAt(s, 0, (f > 5) ? 5 : f, (s16)(x + s->dir * SHRED_TIP_UP_DX), (s16)(y - SHRED_TIP_UP_DZ));
        } else if (t < SHRED_RAY_CHARGE + SHRED_RAY_FLAME) {
            setFrame(s, SHRED_ANIM_RAY, 1);
            fxAt(s, 1, (u8)((t / 5) % 3), (s16)(x + s->dir * SHRED_TIP_UP_DX), (s16)(y - SHRED_TIP_UP_DZ + 2));
        } else {
            setFrame(s, SHRED_ANIM_RAY, 2);
            if (t == SHRED_RAY_CHARGE + SHRED_RAY_FLAME) beamFire(s, x, y);
            if (s->beamOn)
                fxAt(s, 2, 0, (s16)(x + s->dir * SHRED_TIP_DX), (s16)(y - SHRED_TIP_DZ));
            else if (s->fx)
                SPR_setVisibility(s->fx, HIDDEN);
        }
        if (t >= SHRED_RAY_CHARGE + SHRED_RAY_FLAME + SHRED_RAY_HOLD) {
            s->rayReady = 0;
            hideExtras(s);
            afterAttack(s, near, tgtDown);
        }
        break;
    }

    case SHRED_HURT:
        if (s->atk == 0xFF) SPR_setAnimAndFrame(s->sprite, SHRED_ANIM_HURT, (u8)(s->timer >= 12));
        else setFrame(s, SHRED_ANIM_HURT, (u8)(s->timer >= 12));
        if (s->timer >= SHRED_HURT_T) {
            s->rayReady = 1;
            if (!near) {
                if ((random() % 3) == 0) enter(s, SHRED_STANCE);
                else enter(s, SHRED_WALK);
            } else if (tgtDown) {
                enter(s, SHRED_WAIT);
            } else if (s->hits >= 2) {
                enter(s, SHRED_BLOCK);
            } else {
                enter(s, SHRED_WALK);
            }
        }
        break;

    case SHRED_BLOCK:
        // Agachado, cubriendose: los golpes no entran (no es golpeable).
        setFrame(s, SHRED_ANIM_BLOCK, 0);
        if (s->timer >= SHRED_BLOCK_T) { s->hits = 0; enter(s, SHRED_STANCE); }
        break;

    case SHRED_DOWN: {
        // La copia cae (sin casco), queda tirada y se desvanece.
        u16 t = s->timer;
        SPR_setAnimAndFrame(s->sprite, SHRED_ANIM_B_DEATH, (u8)((t < 30) ? 0 : ((t < 45) ? 1 : 2)));
        if (t > SHRED_CLONE_DIE_T - 40) visible = (t >> 1) & 1;
        if (t >= SHRED_CLONE_DIE_T) {
            enter(s, SHRED_GONE);
            SPR_setVisibility(s->sprite, HIDDEN);
            if (s->backTimer != SHRED_NO_RETURN) s->backTimer = SHRED_CLONE_BACK;
            return;
        }
        break;
    }

    case SHRED_DEATH: {
        // Golpeado y despues el fantasma palido que se desvanece.
        u16 t = s->timer;
        if (t < 24) {
            SPR_setAnimAndFrame(s->sprite, s->bare ? SHRED_ANIM_B_HURT : SHRED_ANIM_HURT, 0);
        } else {
            u16 g = (u16)((t - 24) / 30);
            SPR_setAnimAndFrame(s->sprite, SHRED_ANIM_GHOST, (u8)((g > 2) ? 2 : g));
            u16 sh = (t < 110) ? 3 : ((t < 150) ? 2 : 1);
            visible = (t >> sh) & 1;
        }
        if (t >= SHRED_DEATH_T) {
            s->state = SHRED_GONE;
            SPR_releaseSprite(s->sprite);
            s->sprite = NULL;
            return;
        }
        break;
    }

    default:
        break;
    }

    x = sclamp((s16)(s->xq >> 2), s->arenaL, s->arenaR);
    s->xq = (s16)(x * 4);
    y = sclamp((s16)(s->yq >> 2), s->laneT, s->laneB);
    s->yq = (s16)(y * 4);
    render(s, visible);
}

void shredderRelease(ShredderBoss* s) {
    if (s->sprite) { SPR_releaseSprite(s->sprite); s->sprite = NULL; }
    if (s->beamA)  { SPR_releaseSprite(s->beamA);  s->beamA = NULL; }
    if (s->beamB)  { SPR_releaseSprite(s->beamB);  s->beamB = NULL; }
    if (s->fx)     { SPR_releaseSprite(s->fx);     s->fx = NULL; }
    if (s->helmet) { SPR_releaseSprite(s->helmet); s->helmet = NULL; }
    if (rayOwner == s) rayOwner = NULL;
    s->state = SHRED_INACTIVE;
}
