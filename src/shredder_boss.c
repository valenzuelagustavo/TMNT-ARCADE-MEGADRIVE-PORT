#include "shredder_boss.h"
#include "audio.h"    // boss_hit, hit_turtles, shredder_laugh_sfx

// ===========================================================================
// SHREDDER (jefe final) — ver shredder_boss.h
// ===========================================================================

static s16 sabs(s16 v)                 { return (v < 0) ? -v : v; }
static s16 sclamp(s16 v, s16 a, s16 b) { return (v < a) ? a : ((v > b) ? b : v); }

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

static void setAnim(ShredderBoss* s, u8 a) {
    if (s->anim == a) return;
    s->anim = a;
    SPR_setAutoAnimation(s->sprite, TRUE);
    SPR_setAnim(s->sprite, a);
}

// Frame fijo de la caminata (el espadazo usa dos poses de la espada).
static void setPose(ShredderBoss* s, u8 f) {
    s->anim = 0xFE;                      // "a mano": el proximo setAnim la repone
    SPR_setAutoAnimation(s->sprite, FALSE);
    SPR_setAnimAndFrame(s->sprite, SHRED_ANIM_WALK, f);
}

void shredderInit(ShredderBoss* s) {
    s->sprite = NULL;
    s->state  = SHRED_INACTIVE;
    s->anim   = 0xFF;
}

void shredderSpawn(ShredderBoss* s, s16 x, s16 y, s16 arenaL, s16 arenaR, s16 laneT, s16 laneB) {
    PAL_setPalette(PAL2, shredder_lvl1.palette->data, DMA);
    if (!s->sprite)
        s->sprite = SPR_addSpriteSafe(&shredder_lvl1, -80, -80,
                                      TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    s->state = SHRED_ENTER;
    s->x = x; s->y = y;
    s->dir = -1;
    s->hp = SHRED_HP;
    s->timer = 0;
    s->slashCd = 30;
    s->chargeCd = 60;
    s->invuln = 0; s->flash = 0;
    s->comboHits = 0; s->calm = 0;
    s->hitMask = 0;
    s->arenaL = arenaL; s->arenaR = arenaR;
    s->laneT = laneT; s->laneB = laneB;
    s->anim = 0xFF;
    if (s->sprite) {
        SPR_setAutoAnimation(s->sprite, TRUE);
        setAnim(s, SHRED_ANIM_IDLE);
    }
    XGM2_playPCMEx(shredder_laugh_sfx, sizeof(shredder_laugh_sfx), SOUND_PCM_CH2, 15, FALSE, FALSE);
}

bool shredderIsDying(const ShredderBoss* s) { return s->state == SHRED_DEATH || s->state == SHRED_GONE; }
bool shredderIsGone(const ShredderBoss* s)  { return s->state == SHRED_GONE; }

static bool canBeHit(const ShredderBoss* s) {
    return s->sprite && !s->invuln &&
           (s->state == SHRED_IDLE || s->state == SHRED_WALK || s->state == SHRED_WIND ||
            s->state == SHRED_CHARGE || s->state == SHRED_HURT || s->state == SHRED_SLASH);
}

static void teleportOut(ShredderBoss* s) {
    s->state = SHRED_TP_OUT_ST;
    s->timer = 0;
    s->comboHits = 0;
    setAnim(s, SHRED_ANIM_IDLE);
}

bool shredderPlayerHits(ShredderBoss* s, Player** pls, u8 nPl, s8* killer) {
    if (!canBeHit(s)) return FALSE;
    for (u8 k = 0; k < nPl; k++) {
        if (!playerAttackHitsBox(pls[k], s->x, s->y, SHRED_BODY_HALF_W, SHRED_BODY_H)) continue;
        s16 dmg = isPlayerSpecialAttack(pls[k]) ? SHRED_SPECIAL_DMG : 1;
        XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
        s->hp -= dmg;
        s->invuln = SHRED_INVULN;
        s->flash = 8;
        s->calm = 0;
        if (s->hp <= 0) {
            s->hp = 0;
            s->state = SHRED_DEATH;
            s->timer = 0;
            setAnim(s, SHRED_ANIM_IDLE);
            XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 15, FALSE, FALSE);
            if (killer) *killer = (s8)k;
            return TRUE;
        }
        XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 12, FALSE, FALSE);
        if (++s->comboHits > SHRED_COUNTER_HITS) {
            teleportOut(s);
        } else if (s->state != SHRED_CHARGE && s->state != SHRED_SLASH) {
            s->state = SHRED_HURT;
            s->timer = 0;
            setAnim(s, SHRED_ANIM_IDLE);
        }
        return FALSE;
    }
    return FALSE;
}

static void render(ShredderBoss* s, bool visible) {
    SPR_setHFlip(s->sprite, s->dir < 0);
    SPR_setPosition(s->sprite, s->x - s->camX - SHRED_FRAME_W / 2,
                    s->y - s->camY - SHRED_FOOT_OFFSET);
    SPR_setDepth(s->sprite, (s16)(-s->y));
    SPR_setVisibility(s->sprite, visible ? VISIBLE : HIDDEN);
}

void shredderUpdate(ShredderBoss* s, Player** pls, u8 nPl, s16 camX, s16 camY) {
    if (s->state == SHRED_INACTIVE || s->state == SHRED_GONE || !s->sprite) return;
    s->camX = camX;
    s->camY = camY;

    Player* tgt = nearestPlayer(pls, nPl, s->x);
    s16 px = (s16)(getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2);
    s16 py = getPlayerY(tgt);
    s16 dX = (s16)(px - s->x);
    s16 dY = (s16)(py - s->y);

    s->timer++;
    if (s->calm < 255) s->calm++;
    if (s->calm > SHRED_COMBO_RESET) s->comboHits = 0;
    if (s->slashCd)  s->slashCd--;
    if (s->chargeCd) s->chargeCd--;
    if (s->invuln)   s->invuln--;
    if (s->flash)    s->flash--;

    bool visible = !(s->flash & 1);

    switch (s->state) {
    case SHRED_ENTER:
        visible = (s->timer > SHRED_ENTER_TICKS * 2 / 3) || ((s->timer >> 2) & 1);
        s->dir = (dX >= 0) ? 1 : -1;
        if (s->timer >= SHRED_ENTER_TICKS) { s->state = SHRED_IDLE; s->timer = 0; }
        break;

    case SHRED_IDLE:
        setAnim(s, SHRED_ANIM_IDLE);
        s->dir = (dX >= 0) ? 1 : -1;
        if (s->timer > 12 && !isPlayerGameOver(tgt)) {
            s->timer = 0;
            if (!s->chargeCd && sabs(dX) >= SHRED_CHARGE_DIST && sabs(dY) <= SHRED_CHARGE_TOL_Y) {
                s->state = SHRED_WIND;
            } else {
                s->state = SHRED_WALK;
            }
        }
        break;

    case SHRED_WALK:
        setAnim(s, SHRED_ANIM_WALK);
        s->dir = (dX >= 0) ? 1 : -1;
        if (sabs(dX) > SHRED_SLASH_RANGE - 8) s->x += s->dir * SHRED_WALK_SPEED;
        if (sabs(dY) > SHRED_ALIGN_Y)         s->y += (dY > 0) ? 1 : -1;
        // De cerca y alineado: espadazo.
        if (!s->slashCd && sabs(dX) <= SHRED_SLASH_RANGE + 8 && sabs(dY) <= SHRED_SLASH_TOL_Y) {
            s->state = SHRED_SLASH;
            s->timer = 0;
            setPose(s, SHRED_POSE_RAISED);
            break;
        }
        if (s->timer > 70 || (!s->chargeCd && sabs(dX) >= SHRED_CHARGE_DIST &&
                               sabs(dY) <= SHRED_CHARGE_TOL_Y)) {
            s->state = SHRED_IDLE;
            s->timer = 0;
        }
        break;

    case SHRED_SLASH:
        // Levanta la espada (aviso) y la baja al frente: pega en ese instante
        // a todo el que este delante, al alcance y alineado.
        if (s->timer == SHRED_SLASH_WIND) {
            setPose(s, SHRED_POSE_THRUST);
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                s16 kx = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
                s16 fwd = (s16)((kx - s->x) * s->dir);
                if (fwd < -8 || fwd > SHRED_SLASH_RANGE + 12) continue;
                if (sabs((s16)(getPlayerY(pls[k]) - s->y)) > SHRED_SLASH_TOL_Y) continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                playerHitBars(pls[k], s->x, SHRED_SLASH_DMG);
            }
        }
        if (s->timer >= SHRED_SLASH_WIND + SHRED_SLASH_HOLD) {
            s->state = SHRED_IDLE;
            s->timer = 0;
            s->slashCd = SHRED_SLASH_CD;
        }
        break;

    case SHRED_WIND:
        setAnim(s, SHRED_ANIM_IDLE);
        s->dir = (dX >= 0) ? 1 : -1;
        visible = visible && ((s->timer >> 1) & 1);        // titila: aviso
        if (s->timer >= SHRED_CHARGE_WIND) {
            s->state = SHRED_CHARGE;
            s->timer = 0;
            s->hitMask = 0;
        }
        break;

    case SHRED_CHARGE:
        setAnim(s, SHRED_ANIM_WALK);
        s->x += s->dir * SHRED_CHARGE_SPEED;
        for (u8 k = 0; k < nPl; k++) {
            u8 bit = (u8)(1 << k);
            if ((s->hitMask & bit) || !playerCanBeHit(pls[k])) continue;
            s16 kx = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
            if (sabs((s16)(kx - s->x)) > SHRED_CHARGE_HIT_W) continue;
            if (sabs((s16)(getPlayerY(pls[k]) - s->y)) > SHRED_CHARGE_TOL_Y) continue;
            s->hitMask |= bit;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
            playerHitBarsKnockdown(pls[k], s->x, SHRED_CHARGE_DMG);
        }
        if (s->x <= s->arenaL || s->x >= s->arenaR || s->timer > 90) {
            s->state = SHRED_IDLE;
            s->timer = 0;
            s->chargeCd = SHRED_CHARGE_CD;
        }
        break;

    case SHRED_HURT:
        if (s->timer >= SHRED_HURT_TICKS) { s->state = SHRED_IDLE; s->timer = 0; }
        break;

    case SHRED_TP_OUT_ST:
        visible = (s->timer >> 1) & 1;
        if (s->timer >= SHRED_TP_OUT) { s->state = SHRED_TP_GONE_ST; s->timer = 0; }
        break;

    case SHRED_TP_GONE_ST:
        visible = FALSE;
        if (s->timer >= SHRED_TP_GONE) {
            // Reaparece del otro lado del objetivo (o del lado con lugar).
            s16 nx = (s16)(px - s->dir * 90);
            if (nx < s->arenaL + 16 || nx > s->arenaR - 16) nx = (s16)(px + s->dir * 90);
            s->x = nx;
            s->y = py;
            s->dir = (px >= s->x) ? 1 : -1;
            s->state = SHRED_TP_IN_ST;
            s->timer = 0;
            XGM2_playPCMEx(shredder_laugh_sfx, sizeof(shredder_laugh_sfx), SOUND_PCM_CH2, 12, FALSE, FALSE);
        }
        break;

    case SHRED_TP_IN_ST:
        visible = (s->timer >> 1) & 1;
        if (s->timer >= SHRED_TP_IN) {
            s->state = SHRED_IDLE;
            s->timer = 0;
            s->slashCd = 0;
        }
        break;

    case SHRED_DEATH: {
        // Parpadeo que se acelera y se desvanece.
        u16 t = s->timer;
        u16 sh = (t < 40) ? 3 : (t < 70) ? 2 : 1;
        visible = (t >> sh) & 1;
        if (t >= SHRED_DEATH_TICKS) {
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

    s->x = sclamp(s->x, s->arenaL, s->arenaR);
    s->y = sclamp(s->y, s->laneT, s->laneB);
    render(s, visible);
}

void shredderRelease(ShredderBoss* s) {
    if (s->sprite) { SPR_releaseSprite(s->sprite); s->sprite = NULL; }
    s->state = SHRED_INACTIVE;
}
