#ifndef _ENEMY_H_
#define _ENEMY_H_

#include <genesis.h>
#include "enemies.h"

#define MAX_ENEMIES         8
#define ENEMY_SPEED         1
#define ENEMY_AGGRO_RANGE   200
#define ENEMY_ATTACK_RANGE  28
#define ENEMY_SPRITE_W      56
#define ENEMY_FOOT_OFFSET   56
#define ENEMY_SPRITE_H      64
#define ENEMY_HP            3
#define ENEMY_INVINCIBLE    20

typedef enum {
    ENEMY_STATE_INACTIVE,
    ENEMY_STATE_PATROL,
    ENEMY_STATE_CHASE,
    ENEMY_STATE_ATTACK,
    ENEMY_STATE_HURT,
    ENEMY_STATE_DEAD
} EnemyState;

typedef struct {
    s16 triggerX;
    s16 spawnX;
    s16 y;
    s16 patrolRange;
} EnemySpawnDef;

typedef struct {
    Sprite*     sprite;
    s16         x;
    s16         y;
    s16         patrolLeft;
    s16         patrolRight;
    s16         cameraOffsetX;
    EnemyState  state;
    s8          dir;
    u16         timer;
    s16         hp;
    u8          invincible;
} Enemy;

void initEnemySpawn(Enemy* e, s16 spawnX, s16 y, s16 patrolRange, u8 palette);
void updateEnemy(Enemy* e, s16 player1X, s16 player1Y, s16 player2X, s16 player2Y, bool twoPlayers);
void setEnemyCamera(Enemy* e, s16 camX);
bool damageEnemy(Enemy* e, s16 dmg);
bool enemyCanBeHit(const Enemy* e);
s16  getEnemyCenterX(const Enemy* e);
s16  getEnemyCenterY(const Enemy* e);

#endif
