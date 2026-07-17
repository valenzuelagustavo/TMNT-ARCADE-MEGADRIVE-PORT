#include "enemy.h"

#define ENEMY_ANIM_IDLE  0
#define ENEMY_ANIM_WALK  1

static void enemySetAnim(Enemy* e, u16 anim) {
    SPR_setAnim(e->sprite, anim);
    SPR_setAnimationLoop(e->sprite, TRUE);
}

void initEnemySpawn(Enemy* e, s16 spawnX, s16 y, s16 patrolRange, u8 palette) {
    e->x           = spawnX;
    e->y           = y;
    e->patrolLeft  = spawnX - patrolRange;
    e->patrolRight = spawnX + patrolRange;
    e->cameraOffsetX = 0;
    e->state       = ENEMY_STATE_PATROL;
    e->dir         = -1;
    e->timer       = 0;
    e->hp          = ENEMY_HP;
    e->invincible  = 0;

    e->sprite = SPR_addSprite(&foot_soldier, e->x, e->y, TILE_ATTR(palette, FALSE, FALSE, FALSE));
    PAL_setPalette(palette, foot_soldier.palette->data, DMA);
    enemySetAnim(e, ENEMY_ANIM_WALK);
}

void setEnemyCamera(Enemy* e, s16 camX) {
    e->cameraOffsetX = camX;
}

bool damageEnemy(Enemy* e, s16 dmg) {
    if (e->state == ENEMY_STATE_DEAD || e->state == ENEMY_STATE_INACTIVE)
        return FALSE;

    e->hp -= dmg;
    if (e->hp <= 0) {
        e->state = ENEMY_STATE_DEAD;
        e->timer = 30;
        return TRUE;
    }

    e->state = ENEMY_STATE_HURT;
    e->timer = 12;
    e->invincible = ENEMY_INVINCIBLE;
    return TRUE;
}

bool enemyCanBeHit(const Enemy* e) {
    if (e->state == ENEMY_STATE_DEAD || e->state == ENEMY_STATE_INACTIVE)
        return FALSE;
    if (e->invincible > 0 || e->state == ENEMY_STATE_HURT)
        return FALSE;
    return TRUE;
}

s16 getEnemyCenterX(const Enemy* e) {
    return e->x + ENEMY_SPRITE_W / 2;
}

s16 getEnemyCenterY(const Enemy* e) {
    return e->y;
}

static s16 absS16(s16 v) {
    return (v < 0) ? -v : v;
}

static s16 clampS16(s16 val, s16 minVal, s16 maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

static s16 distS16(s16 a, s16 b) {
    return absS16(a - b);
}

void updateEnemy(Enemy* e, s16 player1X, s16 player1Y, s16 player2X, s16 player2Y, bool twoPlayers) {
    if (e->state == ENEMY_STATE_INACTIVE || !e->sprite) return;

    if (e->invincible > 0) e->invincible--;

    if (e->state == ENEMY_STATE_DEAD) {
        if (e->timer > 0) {
            e->timer--;
            if (e->timer == 0) {
                SPR_releaseSprite(e->sprite);
                e->sprite = NULL;
                e->state = ENEMY_STATE_INACTIVE;
                return;
            }
        }
        SPR_setPosition(e->sprite, e->x - e->cameraOffsetX, e->y - ENEMY_FOOT_OFFSET);
        return;
    }

    s16 targetX = player1X;
    s16 targetY = player1Y;
    if (twoPlayers) {
        s16 d1 = distS16(e->x, player1X);
        s16 d2 = distS16(e->x, player2X);
        if (d2 < d1) {
            targetX = player2X;
            targetY = player2Y;
        }
    }

    s16 dx = targetX - e->x;
    s16 dist = absS16(dx);

    EnemyState newState = e->state;

    switch (e->state) {

        case ENEMY_STATE_PATROL: {
            if (dist < ENEMY_AGGRO_RANGE) {
                newState = ENEMY_STATE_CHASE;
                break;
            }
            e->x += e->dir * ENEMY_SPEED;
            if (e->x <= e->patrolLeft)  { e->x = e->patrolLeft;  e->dir = 1; }
            if (e->x >= e->patrolRight) { e->x = e->patrolRight; e->dir = -1; }
            break;
        }

        case ENEMY_STATE_CHASE: {
            if (dist > ENEMY_AGGRO_RANGE + 60) {
                newState = ENEMY_STATE_PATROL;
                break;
            }
            if (dist < ENEMY_ATTACK_RANGE && absS16(targetY - e->y) < 24) {
                newState = ENEMY_STATE_ATTACK;
                e->timer = 20;
                break;
            }
            if (dx > 0) { e->x += ENEMY_SPEED; e->dir = 1; }
            else        { e->x -= ENEMY_SPEED; e->dir = -1; }
            e->x = clampS16(e->x, 0, 1376 - ENEMY_SPRITE_W);
            break;
        }

        case ENEMY_STATE_ATTACK: {
            if (e->timer > 0) {
                e->timer--;
            } else {
                newState = ENEMY_STATE_CHASE;
            }
            break;
        }

        case ENEMY_STATE_HURT: {
            e->x += (e->dir * -1) * 3;
            if (e->timer > 0) {
                e->timer--;
                SPR_setVisibility(e->sprite, e->timer & 1 ? VISIBLE : HIDDEN);
            } else {
                SPR_setVisibility(e->sprite, VISIBLE);
                newState = ENEMY_STATE_CHASE;
            }
            break;
        }

        default: break;
    }

    if (newState != e->state) {
        e->state = newState;
        if (newState == ENEMY_STATE_PATROL || newState == ENEMY_STATE_CHASE)
            enemySetAnim(e, ENEMY_ANIM_WALK);
    }

    if (dx != 0)
        SPR_setHFlip(e->sprite, (dx < 0));

    SPR_setPosition(e->sprite, e->x - e->cameraOffsetX, e->y - ENEMY_FOOT_OFFSET);
    SPR_setDepth(e->sprite, -(e->y));
}
