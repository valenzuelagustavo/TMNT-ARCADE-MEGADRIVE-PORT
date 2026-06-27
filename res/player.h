#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <genesis.h>
#include "resources.h"

//1. Las animaciones del personaje
typedef enum{
    ANIM_IDLE = 0,
    ANIM_KICK = 1,
    ANIM_ATTACK_1 = 2,
    ANIM_ATTACK_2 = 3,
    ANIM_ATTACK_3 = 4,
    ANIM_ATTACK_4 = 5,
    ANIM_JUMP = 6,
    ANIM_JUMP_KICK = 7,
    ANIM_WALK_FRONT = 8,
    ANIM_WALK_BACK = 9,
    ANIM_SPECIAL = 10,
    ANIM_HIT_1 = 11,
    ANIM_HIT_2 = 12,
    ANIM_HIT_3 = 13,
    ANIM_GET_UP_1 = 14,
    ANIM_HIT_BEHIND_1 = 15,
    ANIM_HIT_BEHIND_2 = 16,
    ANIM_GET_UP_2 = 17,
    ANIM_HELD = 18
} PlayerAnim;

// 2. Los estados (lo que el personaje está haciendo lógicamente)
typedef enum {
    STATE_IDLE,
    STATE_WALKING,
    STATE_ATTACKING,
    STATE_JUMPING,
    STATE_HURT,
    STATE_GRABBED
} PlayerState;

// 3. Declaración de las funciones para que 'scenes.c' las pueda invocar
void initPlayer(u8 selectedCharacter);
void updatePlayerInput();

#endif 