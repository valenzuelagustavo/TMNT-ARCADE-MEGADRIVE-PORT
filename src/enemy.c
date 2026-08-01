#include "enemy.h"

// ---------------------------------------------------------------------------
// Cambio de animación con guarda: solo re-setea si la anim es distinta a la
// actual (evita reiniciar el ciclo cada frame). 'loop' controla si la anim
// se repite (caminar) o queda clavada en el último frame (ataques).
// ---------------------------------------------------------------------------
static void enemySetAnim(Enemy* e, u8 anim, bool loop) {
    if (e->anim == anim) return;
    e->anim = anim;
    SPR_setAnim(e->sprite, anim);
    SPR_setAnimationLoop(e->sprite, loop);
}

// Reinicia una animación desde el frame 0 AUNQUE sea la misma que ya está
// seteada. Necesario para los ataques: quedan clavados en el último frame
// (sin loop) y SPR_setAnim ignora el cambio si el índice es el mismo — un
// segundo kick consecutivo no se reiniciaría. SPR_setAnimAndFrame sí fuerza
// el reinicio porque el frame actual difiere de 0.
static void enemyRestartAnim(Enemy* e, u8 anim, bool loop) {
    e->anim = anim;
    SPR_setAnimAndFrame(e->sprite, anim, 0);
    SPR_setAnimationLoop(e->sprite, loop);
}

// ---------------------------------------------------------------------------
// MAPEO DE ANIMACIONES — el foot soldier naranja tiene un orden DISTINTO
// de animaciones en su spritesheet. Estos helpers devuelven el índice
// correcto según el tipo de enemigo.
// ---------------------------------------------------------------------------
static u8 enemyAnimIdle(const Enemy* e) {
    return (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) ? ORANGE_ANIM_IDLE
                                                       : ENEMY_ANIM_IDLE;
}
static u8 enemyAnimWalk(const Enemy* e) {
    return (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) ? ORANGE_ANIM_WALK
                                                       : ENEMY_ANIM_WALK;
}
static u8 enemyAnimWalkUp(const Enemy* e) {
    return (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) ? ORANGE_ANIM_WALK_UP
                                                       : ENEMY_ANIM_WALK_UP;
}
static u8 enemyAnimKick(const Enemy* e) {
    return (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) ? ORANGE_ANIM_KICK
                                                       : ENEMY_ANIM_KICK;
}
static u8 enemyAnimPunchFront(const Enemy* e) {
    return (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) ? ORANGE_ANIM_PUNCH_FRONT
                                                       : ENEMY_ANIM_PUNCH_FRONT;
}
static u8 enemyAnimUppercut(const Enemy* e) {
    return (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) ? ORANGE_ANIM_UPPERCUT
                                                       : ENEMY_ANIM_PUNCH;
}
static u8 enemyAnimExplode(const Enemy* e) {
    return (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) ? ORANGE_ANIM_EXPLODE
                                                       : ENEMY_ANIM_EXPLODE;
}
static u8 enemyAnimHit(Enemy* e) {
    if (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) return ORANGE_ANIM_HIT;
    u8 hitAnim = (u8)(ENEMY_ANIM_HIT_1 + e->hitToggle);
    if (++e->hitToggle >= 3) e->hitToggle = 0;
    return hitAnim;
}

// Devuelve la duración del ataque actual según el tipo de enemigo.
static u16 enemyAttackTime(const Enemy* e) {
    if (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) {
        switch (e->attackType) {
            case ENEMY_ATTACK_KICK:    return ORANGE_KICK_TIME;
            case ENEMY_ATTACK_SHURIKEN: return ORANGE_SHURIKEN_TIME;
            case ENEMY_ATTACK_FRONT:   return ORANGE_PUNCH_TIME;
            default:                   return ORANGE_UPPERCUT_TIME;  // PUNCH
        }
    }
    return (e->attackType == ENEMY_ATTACK_KICK) ? ENEMY_KICK_TIME
                                                : ENEMY_PUNCH_TIME;
}

// Devuelve la duración de la explosión de muerte según el tipo.
static u16 enemyExplodeTime(const Enemy* e) {
    return (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) ? ORANGE_EXPLODE_TIME
                                                       : ENEMY_EXPLODE_TIME;
}

// Devuelve el rango del hitbox del ataque actual según el tipo.
static s16 enemyAttackReach(const Enemy* e) {
    if (e->attackType == ENEMY_ATTACK_KICK) return ENEMY_HIT_RANGE_X;
    if (e->attackType == ENEMY_ATTACK_FRONT) return ENEMY_FRONT_REACH;
    if (e->attackType == ENEMY_ATTACK_SHURIKEN) return 0;  // no hitbox melee
    // UPPERCUT
    return ENEMY_UPPERCUT_REACH;
}

// ---------------------------------------------------------------------------
// COMBOS DEL FOOT SOLDIER MORADO (fiel al arcade)
// ---------------------------------------------------------------------------
// En el arcade el foot soldier lanza cadenas de 2-3 golpes (ATTACK S0 = puños,
// S1/S2 = variantes de patada): cada golpe es un PASO con su propia animación,
// duración y ventana de hitbox. El morado entra a ENEMY_STATE_ATTACK con un
// combo de N pasos; arranca en el paso 0 y al expirar su timer avanza al
// siguiente (reseteando attackHit para que cada golpe conecte una vez). El
// timer del estado es el del paso actual; los i-frames del jugador (45) hacen
// que una cadena completa rara vez conecte entera — como el knockback del
// arcade, el golpe 1 suele sacarte del alcance de los siguientes.
typedef struct {
    u8   anim;      // Índice de animación (ENEMY_ANIM_* del morado)
    u16  time;      // Duración total del paso (ticks, calzada a FAST 8 del sheet)
    u16  hitStart;  // Timer mínimo (inclusive) con hitbox activa
    u16  hitEnd;    // Timer máximo (inclusive) con hitbox activa
    s16  reach;     // Alcance del golpe (centro a centro)
    u16  lunge;     // Frames iniciales con desplazamiento en X (patada; 0 = fijo)
} ComboStep;

// Combo de puño (arcade ATTACK S0): doble directo + uppercut final.
static const ComboStep purpleComboPunch[] = {
    { ENEMY_ANIM_PUNCH_FRONT, 16, ENEMY_PUNCH_HIT_START, ENEMY_PUNCH_HIT_END, ENEMY_FRONT_REACH,    0 },
    { ENEMY_ANIM_PUNCH_FRONT, 16, ENEMY_PUNCH_HIT_START, ENEMY_PUNCH_HIT_END, ENEMY_FRONT_REACH,    0 },
    { ENEMY_ANIM_PUNCH,       16, ENEMY_PUNCH_HIT_START, ENEMY_PUNCH_HIT_END, ENEMY_UPPERCUT_REACH, 0 }
};
// Combo de patada (arcade ATTACK S1/S2): directo + patada con salto (lunge).
static const ComboStep purpleComboKick[] = {
    { ENEMY_ANIM_PUNCH_FRONT, 16, ENEMY_PUNCH_HIT_START, ENEMY_PUNCH_HIT_END, ENEMY_FRONT_REACH, 0 },
    { ENEMY_ANIM_KICK, ENEMY_KICK_TIME,
      (u16)(ENEMY_KICK_TIME - ENEMY_KICK_LUNGE + 1), ENEMY_KICK_TIME,
      ENEMY_HIT_RANGE_X, ENEMY_KICK_LUNGE }
};
// Doble directo (variante corta, arcade ATTACK S0 sin uppercut).
static const ComboStep purpleComboFront[] = {
    { ENEMY_ANIM_PUNCH_FRONT, 16, ENEMY_PUNCH_HIT_START, ENEMY_PUNCH_HIT_END, ENEMY_FRONT_REACH, 0 },
    { ENEMY_ANIM_PUNCH_FRONT, 16, ENEMY_PUNCH_HIT_START, ENEMY_PUNCH_HIT_END, ENEMY_FRONT_REACH, 0 }
};

// Devuelve la tabla del combo según el tipo de ataque (solo morado; el naranja
// no usa combos → comboLen queda en 0 y usa el camino simple).
static const ComboStep* comboStepsFor(u8 attackType) {
    switch (attackType) {
        case ENEMY_ATTACK_KICK:  return purpleComboKick;
        case ENEMY_ATTACK_PUNCH: return purpleComboPunch;
        default:                 return purpleComboFront;   // FRONT
    }
}

static u8 comboLengthFor(u8 attackType) {
    switch (attackType) {
        case ENEMY_ATTACK_KICK:  return (u8)(sizeof(purpleComboKick)  / sizeof(ComboStep));
        case ENEMY_ATTACK_PUNCH: return (u8)(sizeof(purpleComboPunch) / sizeof(ComboStep));
        default:                 return (u8)(sizeof(purpleComboFront) / sizeof(ComboStep));
    }
}

// ---------------------------------------------------------------------------
// UTILIDADES
// ---------------------------------------------------------------------------
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

static s16 enemyMaxX(const Enemy* e) {
    s16 laneRange = ENEMY_LANE_BOTTOM - ENEMY_LANE_TOP;
    s32 wallRange = ENEMY_END_WALL_X_BOTTOM - ENEMY_END_WALL_X_TOP;
    s16 wallX     = ENEMY_END_WALL_X_TOP + (s16)(wallRange * (e->y - ENEMY_LANE_TOP) / laneRange);
    s16 levelMax  = 1376 - e->w;
    s16 wallMax   = wallX - e->w;
    return (wallMax < levelMax) ? wallMax : levelMax;
}

// Cota izquierda de movimiento por tipo. El morado conserva la negativa
// (-w) para poder nacer con la voltereta por la espalda desde fuera de
// pantalla; el naranja (kiter) NUNCA retrocede a la izquierda del borde
// visible de la cámara: si se retirara hasta X negativa quedaría fuera
// del nivel, inalcanzable para el jugador (que no pasa del borde de la
// cámara) e imposible de matar.
static s16 enemyMinX(const Enemy* e) {
    if (e->state == ENEMY_STATE_SPAWNING) return -(s16)e->w;
    if (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) return e->cameraOffsetX;
    return -(s16)e->w;
}

// ---------------------------------------------------------------------------
// IA DE GRUPO — atacantes simultáneos
// ---------------------------------------------------------------------------
static u8 enemiesAttacking = 0;

// --- Reparto de targets en 2 jugadores ---
static u8 enemyNumPlayers = 1;
static u8 enemyTargetCount[2] = {0, 0};

void resetEnemyAI(u8 numPlayers) {
    enemiesAttacking = 0;
    enemyNumPlayers = (numPlayers >= 2) ? 2 : 1;
    enemyTargetCount[0] = 0;
    enemyTargetCount[1] = 0;
}

static void releaseTarget(u8 target) {
    if (enemyTargetCount[target] > 0)
        enemyTargetCount[target]--;
}

static void leaveAttackState(Enemy* e) {
    // El shuriken es a distancia: no ocupa cupo de atacante melee, así que
    // tampoco lo libera (nunca lo incrementó — ver el trigger de ataque).
    if (e->state == ENEMY_STATE_ATTACK &&
        e->attackType != ENEMY_ATTACK_SHURIKEN && enemiesAttacking > 0)
        enemiesAttacking--;
}

// ---------------------------------------------------------------------------
// SISTEMA DE SHURIKENS — proyectiles del foot soldier naranja
// ---------------------------------------------------------------------------
static Shuriken shurikens[MAX_SHURIKENS];

void shurikenInit(void) {
    for (u16 i = 0; i < MAX_SHURIKENS; i++) {
        shurikens[i].active = 0;
        shurikens[i].sprite = NULL;
    }
}

void shurikenSpawn(s16 x, s16 y, s8 dir, u8 palette) {
    for (u16 i = 0; i < MAX_SHURIKENS; i++) {
        if (shurikens[i].active) continue;
        shurikens[i].x = x;
        shurikens[i].y = y;
        shurikens[i].dir = dir;
        shurikens[i].cameraOffsetX = 0;
        shurikens[i].active = 1;
        shurikens[i].sprite = SPR_addSprite(&shuriken_sprite,
                                            x, y - ENEMY_FOOT_OFFSET_ORANGE + 40,
                                            TILE_ATTR(palette, FALSE, FALSE, FALSE));
        if (shurikens[i].sprite) {
            SPR_setDepth(shurikens[i].sprite, -(y) - 1);
            SPR_setHFlip(shurikens[i].sprite, (dir < 0));
        }
        return;   // slot encontrado
    }
}

void shurikenUpdate(s16 camX) {
    for (u16 i = 0; i < MAX_SHURIKENS; i++) {
        if (!shurikens[i].active) continue;
        shurikens[i].cameraOffsetX = camX;
        shurikens[i].x += shurikens[i].dir * ORANGE_SHURIKEN_SPEED;

        // Fuera de pantalla (con margen de 32px a cada lado)
        if (shurikens[i].x < camX - 32 || shurikens[i].x > camX + 320 + 32) {
            if (shurikens[i].sprite) SPR_releaseSprite(shurikens[i].sprite);
            shurikens[i].sprite = NULL;
            shurikens[i].active = 0;
            continue;
        }
        if (shurikens[i].sprite)
            SPR_setPosition(shurikens[i].sprite,
                            shurikens[i].x - shurikens[i].cameraOffsetX,
                            shurikens[i].y - ENEMY_FOOT_OFFSET_ORANGE + 40);
    }
}

void shurikenReleaseAll(void) {
    for (u16 i = 0; i < MAX_SHURIKENS; i++) {
        if (shurikens[i].sprite) SPR_releaseSprite(shurikens[i].sprite);
        shurikens[i].sprite = NULL;
        shurikens[i].active = 0;
    }
}

bool shurikenCheckHitPlayer(s16 px, s16 py, s16* hitX) {
    // px = borde izquierdo del frame del jugador (frame de 104px)
    s16 pcx = px + PLAYER_SPRITE_W / 2;   // centro del jugador
    s16 pcy = py;                         // pies del jugador

    for (u16 i = 0; i < MAX_SHURIKENS; i++) {
        if (!shurikens[i].active) continue;

        s16 scx = shurikens[i].x + 8;   // centro del shuriken (16px wide → +8)
        s16 scy = shurikens[i].y;

        if (absS16(pcx - scx) < 16 && absS16(pcy - scy) < 16) {
            // Impacto: destruir el shuriken
            if (hitX) *hitX = scx;
            if (shurikens[i].sprite) SPR_releaseSprite(shurikens[i].sprite);
            shurikens[i].sprite = NULL;
            shurikens[i].active = 0;
            return TRUE;
        }
    }
    return FALSE;
}

bool shurikenBreakByPlayerAttack(const Player* p) {
    // La hitbox del ataque de la tortuga (MISMA geometría que contra los
    // enemigos: playerAttackHits) rompe los shurikens que la cruzan: el
    // proyectil desaparece SIN dañar al jugador. Devuelve TRUE si rompió
    // alguno (para tocar un SFX). Se llama por jugador, ANTES de
    // shurikenCheckHitPlayer: un shuriken roto este frame ya no pega.
    bool broke = FALSE;
    for (u16 i = 0; i < MAX_SHURIKENS; i++) {
        if (!shurikens[i].active) continue;
        s16 scx = shurikens[i].x + 8;   // centro del shuriken (16px wide → +8)
        s16 scy = shurikens[i].y;       // lane (pies) del lanzador = la del shuriken
        if (playerAttackHits(p, scx, scy)) {
            if (shurikens[i].sprite) SPR_releaseSprite(shurikens[i].sprite);
            shurikens[i].sprite = NULL;
            shurikens[i].active = 0;
            broke = TRUE;
        }
    }
    return broke;
}

// ---------------------------------------------------------------------------
// SPAWN DE ENEMIGOS
// ---------------------------------------------------------------------------
void initEnemySpawn(Enemy* e, s16 spawnX, s16 y, s16 patrolRange, u8 palette, u8 type) {
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
    e->palette     = palette;
    e->type        = type;
    e->anim        = 0xFF;   // sentinela: fuerza el primer enemySetAnim
    e->attackType  = ENEMY_ATTACK_PUNCH;
    e->attackHit   = 0;
    e->hitToggle   = 0;
    e->flankTimer  = 0;
    e->attackCooldown = (u8)(random() & 31);
    e->lastMoveDir = 0;
    e->turnTimer   = 0;
    e->somersault  = 0;
    e->grabTarget  = 0;
    e->grabbed     = NULL;
    e->grabTimer   = 0;
    e->stancePhase = 0;
    e->stanceToggle = 0;
    e->comboStep  = 0;
    e->comboLen   = 0;

    // Dimensiones de frame según el tipo (sheet morada 64x80 con los pies en
    // el borde; la naranja mantiene la grilla vieja 104x104).
    if (type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) {
        e->w          = ENEMY_SPRITE_W_ORANGE;
        e->h          = ENEMY_SPRITE_H_ORANGE;
        e->footOffset = ENEMY_FOOT_OFFSET_ORANGE;
    } else {
        e->w          = ENEMY_SPRITE_W_PURPLE;
        e->h          = ENEMY_SPRITE_H_PURPLE;
        e->footOffset = ENEMY_FOOT_OFFSET_PURPLE;
    }

    if (enemyNumPlayers == 2) {
        if      (enemyTargetCount[0] < enemyTargetCount[1]) e->target = 0;
        else if (enemyTargetCount[1] < enemyTargetCount[0]) e->target = 1;
        else                                                e->target = (u8)(random() & 1);
    } else {
        e->target = 0;
    }
    enemyTargetCount[e->target]++;
    e->retargetTimer = ENEMY_RETARGET_INTERVAL;

    // Elegir spritesheet y paleta según el tipo
    const SpriteDefinition* sheetDef = &foot_soldier;
    if (type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) {
        sheetDef = &foot_soldier_orange;
    }
    e->sprite = SPR_addSprite(sheetDef, e->x, e->y,
                              TILE_ATTR(palette, FALSE, FALSE, FALSE));

    // Cargar paleta desde el PNG (que ya tiene blanco en índice 1 para el HUD).
    PAL_setPalette(palette, sheetDef->palette->data, DMA);

    enemySetAnim(e, enemyAnimWalk(e), TRUE);
}

void initEnemyDoorSpawn(Enemy* e, s16 doorCenterX, u8 palette) {
    initEnemySpawn(e, doorCenterX - ENEMY_SPRITE_W_PURPLE / 2, ENEMY_LANE_TOP, 60, palette,
                   ENEMY_TYPE_FOOT_SOLDIER);
    e->state = ENEMY_STATE_SPAWNING;
    e->timer = ENEMY_BREAK_DOOR_TIME;
    e->dir   = 1;

    e->anim = ENEMY_ANIM_BREAK_DOOR;
    SPR_setAnimAndFrame(e->sprite, ENEMY_ANIM_BREAK_DOOR, 1);
    SPR_setAnimationLoop(e->sprite, FALSE);
}

void initEnemyElevatorSpawn(Enemy* e, s16 doorCenterX, u8 palette) {
    initEnemySpawn(e, doorCenterX - ENEMY_SPRITE_W_PURPLE / 2, ENEMY_LANE_TOP, 60, palette,
                   ENEMY_TYPE_FOOT_SOLDIER);
    e->state = ENEMY_STATE_SPAWNING;
    e->timer = ENEMY_ELEV_SPAWN_TIME;
    e->dir   = 1;

    e->anim = ENEMY_ANIM_BREAK_DOOR;
    SPR_setAnimAndFrame(e->sprite, ENEMY_ANIM_BREAK_DOOR, 3);
    SPR_setAnimationLoop(e->sprite, FALSE);
}

void initEnemyKickSpawn(Enemy* e, s16 spawnX, s16 y, s8 dir, u8 palette, u8 type) {
    initEnemySpawn(e, spawnX, y, 0, palette, type);
    e->state = ENEMY_STATE_SPAWNING;
    e->timer = ENEMY_KICK_TIME;
    e->dir   = dir;

    e->anim = (type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE)
              ? ORANGE_ANIM_KICK : ENEMY_ANIM_KICK;
    SPR_setAnimAndFrame(e->sprite, e->anim, 0);
    SPR_setAnimationLoop(e->sprite, FALSE);
}

// Spawn con VOLTERETA (anim 15): el morado entra desde fuera de pantalla
// haciendo la voltereta (7 frames) mientras avanza ENEMY_SOMERSAULT_SPEED
// px/frame (más rápido que el walk), y al terminar pasa a CHASE. Usada para
// las oleadas "por la espalda".
void initEnemySomersaultSpawn(Enemy* e, s16 spawnX, s16 y, s8 dir, u8 palette, u8 type) {
    initEnemySpawn(e, spawnX, y, 0, palette, type);
    e->state     = ENEMY_STATE_SPAWNING;
    e->timer     = ENEMY_SOMERSAULT_TIME;
    e->dir       = dir;
    e->somersault = 1;

    e->anim = ENEMY_ANIM_VOLTERETA;
    SPR_setAnimAndFrame(e->sprite, ENEMY_ANIM_VOLTERETA, 0);
    SPR_setAnimationLoop(e->sprite, FALSE);
}

void setEnemyCamera(Enemy* e, s16 camX) {
    e->cameraOffsetX = camX;
}

// ---------------------------------------------------------------------------
// DAÑO Y MUERTE
// ---------------------------------------------------------------------------
bool damageEnemy(Enemy* e, s16 dmg) {
    if (e->state == ENEMY_STATE_DEAD || e->state == ENEMY_STATE_INACTIVE)
        return FALSE;

    // Si estaba AGARRANDO a un jugador, el golpe lo libera (el otro jugador
    // le pega al soldier para rescatar al agarrado).
    if (e->state == ENEMY_STATE_GRAB && e->grabbed) {
        playerReleaseGrab(e->grabbed);
        e->grabbed = NULL;
        e->state   = ENEMY_STATE_CHASE;   // el golpe lo saca del agarre
    }

    leaveAttackState(e);
    e->attackCooldown = ENEMY_HURT_COOLDOWN;

    e->hp -= dmg;
    if (e->hp <= 0) {
        e->state = ENEMY_STATE_DEAD;
        e->timer = enemyExplodeTime(e);
        enemyRestartAnim(e, enemyAnimExplode(e), FALSE);
        return TRUE;
    }

    // Golpe NO fatal: estado HURT sin retroceso (el enemigo no se mueve en X).
    e->state = ENEMY_STATE_HURT;
    e->timer = 12;
    e->invincible = ENEMY_INVINCIBLE;
    enemyRestartAnim(e, enemyAnimHit(e), FALSE);
    return TRUE;
}

bool enemyCanBeHit(const Enemy* e) {
    if (e->state == ENEMY_STATE_DEAD || e->state == ENEMY_STATE_INACTIVE ||
        e->state == ENEMY_STATE_SPAWNING)
        return FALSE;
    if (e->invincible > 0 || e->state == ENEMY_STATE_HURT)
        return FALSE;
    return TRUE;
}

s16 getEnemyCenterX(const Enemy* e) {
    return e->x + e->w / 2;
}

s16 getEnemyCenterY(const Enemy* e) {
    return e->y;
}

// ---------------------------------------------------------------------------
// UPDATE PRINCIPAL
// ---------------------------------------------------------------------------
void updateEnemy(Enemy* e, Player* player1, Player* player2, bool twoPlayers) {
    if (e->state == ENEMY_STATE_INACTIVE || !e->sprite) return;

    if (e->invincible > 0) e->invincible--;
    if (e->attackCooldown > 0) e->attackCooldown--;

    u16 explodeTime = enemyExplodeTime(e);

    if (e->state == ENEMY_STATE_DEAD) {
        // Sin empuje: muere EN EL LUGAR (igual que HURT, sin retroceso en X).
        if (e->timer > 0) {
            e->timer--;
            if (e->timer == 0) {
                releaseTarget(e->target);
                SPR_releaseSprite(e->sprite);
                e->sprite = NULL;
                e->state = ENEMY_STATE_INACTIVE;
                return;
            }
        }
        SPR_setPosition(e->sprite, e->x - e->cameraOffsetX, e->y - e->footOffset);
        return;
    }

    if (e->state == ENEMY_STATE_SPAWNING) {
        // Kick entry: desplazarse durante el SPAWNING
        if (e->anim == ENEMY_ANIM_KICK || e->anim == ORANGE_ANIM_KICK) {
            u16 kickLunge = (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE)
                            ? ORANGE_KICK_LUNGE : ENEMY_KICK_LUNGE;
            u16 kickTime  = enemyAttackTime(e);
            if (e->timer > (kickTime - kickLunge)) {
                e->x += e->dir * ENEMY_KICK_SPEED;
                e->x = clampS16(e->x, enemyMinX(e), enemyMaxX(e));
            }
        }
        // Voltereta de entrada: avanza en X durante TODO el SPAWNING (más rápido
        // que el walk; el sprite hace la voltereta sola con la anim 15).
        if (e->somersault) {
            e->x += e->dir * ENEMY_SOMERSAULT_SPEED;
            e->x = clampS16(e->x, enemyMinX(e), enemyMaxX(e));
        }
        if (e->timer > 0) e->timer--;
        else              e->state = ENEMY_STATE_CHASE;
        SPR_setHFlip(e->sprite, (e->dir < 0));
        SPR_setPosition(e->sprite, e->x - e->cameraOffsetX, e->y - e->footOffset);
        SPR_setDepth(e->sprite, -(e->y));
        return;
    }

    // --- Target asignado ---
    if (twoPlayers && enemyNumPlayers == 2) {
        if (e->retargetTimer > 0) {
            e->retargetTimer--;
        } else {
            e->retargetTimer = ENEMY_RETARGET_INTERVAL;
            u8  other = e->target ^ 1;
            s16 dCur  = distS16(e->x, getPlayerWorldX((e->target == 0) ? player1 : player2));
            s16 dOth  = distS16(e->x, getPlayerWorldX((other == 0)     ? player1 : player2));
            if (dOth + ENEMY_RETARGET_HYSTERESIS < dCur) {
                releaseTarget(e->target);
                e->target = other;
                enemyTargetCount[other]++;
            }
        }
    } else {
        e->target = 0;
    }

    Player* targetP = (e->target == 1) ? player2 : player1;
    s16 targetX   = getPlayerWorldX(targetP);
    s16 targetY   = getPlayerY(targetP);
    s8  targetDir = getPlayerDir(targetP);

    s16 dx   = targetX - e->x;
    s16 dy   = targetY - e->y;
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
            enemySetAnim(e, enemyAnimWalk(e), TRUE);
            break;
        }

        case ENEMY_STATE_CHASE: {
            if (dist > ENEMY_DEAGGRO_RANGE) {
                newState = ENEMY_STATE_PATROL;
                break;
            }

            // -----------------------------------------------------------------
            // SELECCIÓN DE ATAQUE — según el tipo de enemigo
            //   · Naranja: kiter. Patada si el jugador lo acorrala; si no,
            //     shuriken a distancia (no ocupa cupo de atacante melee).
            //   · Morado: flanqueo. Sólo pega si ya está EN LA ESPALDA del
            //     jugador... salvo que se le haya agotado el tiempo de flanqueo
            //     (MORADO_FLANK_TIMEOUT), en cuyo caso encara de frente.
            // -----------------------------------------------------------------
            bool wantAttack = FALSE;

            // --- Morado: intentar AGARRE por la espalda ---
            // Si ya está detrás del jugador y pegado (y el jugador no está
            // agarrado por nadie), en vez de pegar puede sujetarlo por la
            // espalda: el jugador queda inmovilizado mostrando ANIM_HELD y
            // tiene que zafarse masheando (o que le peguen al soldier). El
            // soldier queda invisible (la anim GRAB es toda transparente: así
            // no tapa el agarre del jugador). Con probabilidad ~1/4 por frame;
            // el resto de las veces pega el uppercut normal.
            if (e->type == ENEMY_TYPE_FOOT_SOLDIER) {
                Player* gp = (e->target == 1) ? player2 : player1;
                bool flanking = (e->flankTimer < MORADO_FLANK_TIMEOUT);
                bool onBack   = (((s32)(e->x - targetX)) * targetDir) < 0;
                if (flanking && onBack && !playerIsGrabbed(gp) &&
                    dist < ENEMY_GRAB_RANGE && absS16(dy) <= ENEMY_ATTACK_TOL_Y &&
                    e->attackCooldown == 0 && enemiesAttacking < ENEMY_MAX_ATTACKERS &&
                    (random() & 3) == 0) {
                    newState = ENEMY_STATE_GRAB;
                    playerFootGrab(gp);
                    e->grabTarget = e->target;
                    e->grabbed    = gp;
                    e->grabTimer  = ENEMY_GRAB_MAX_TIME;
                    e->attackCooldown = ENEMY_ATTACK_COOLDOWN;
                    e->flankTimer = 0;
                    if (dx != 0) e->dir = (dx > 0) ? 1 : -1;
                    enemyRestartAnim(e, ENEMY_ANIM_GRAB, FALSE);
                    break;   // salta el resto del CHASE este frame
                }
            }

            if (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) {
                if (e->attackCooldown == 0 && absS16(dy) <= ENEMY_ATTACK_TOL_Y) {
                    if (dist <= ORANGE_KICK_RANGE && enemiesAttacking < ENEMY_MAX_ATTACKERS) {
                        // Jugador CERCANO → melee. A corta distancia alterna
                        // patada y directo (usa la anim 4, que hoy no se ve);
                        // más lejos (aún dentro del alcance del kick) va la
                        // patada con salto, que se desplaza 48px y alcanza.
                        if (dist <= ENEMY_FRONT_REACH)
                            e->attackType = (random() & 1) ? ENEMY_ATTACK_KICK
                                                           : ENEMY_ATTACK_FRONT;
                        else
                            e->attackType = ENEMY_ATTACK_KICK;
                        wantAttack = TRUE;
                    } else if (dist >= ORANGE_SHURIKEN_RANGE_MIN &&
                               dist <= ORANGE_SHURIKEN_RANGE_MAX) {
                        e->attackType = ENEMY_ATTACK_SHURIKEN;   // a distancia
                        wantAttack = TRUE;
                    }
                }
            } else {
                // Morado (y cualquier otro melee): ataca si está en rango y —
                // mientras dure el flanqueo— sólo desde la espalda del jugador.
                bool flanking = (e->flankTimer < MORADO_FLANK_TIMEOUT);
                bool onBack   = (((s32)(e->x - targetX)) * targetDir) < 0;
                if (dist < ENEMY_ATTACK_RANGE && absS16(dy) <= ENEMY_ATTACK_TOL_Y &&
                    e->attackCooldown == 0 && enemiesAttacking < ENEMY_MAX_ATTACKERS &&
                    (!flanking || onBack)) {
                    if (dist < ENEMY_UPPERCUT_RANGE)
                        e->attackType = ENEMY_ATTACK_PUNCH;      // uppercut pegado
                    else
                        e->attackType = (random() & 1) ? ENEMY_ATTACK_KICK
                                                       : ENEMY_ATTACK_FRONT;
                    wantAttack = TRUE;
                }
            }

            if (wantAttack) {
                newState = ENEMY_STATE_ATTACK;
                // El shuriken es a distancia: no cuenta como atacante melee.
                if (e->attackType != ENEMY_ATTACK_SHURIKEN) enemiesAttacking++;
                if (dx != 0) e->dir = (dx > 0) ? 1 : -1;
                e->attackHit  = 0;
                e->flankTimer = 0;   // reinicia la frustración de flanqueo
                // El morado ataca con COMBOS (cadenas de 2-3 golpes como el
                // arcade ATTACK S0/S1/S2): comboLen > 0 activa la tabla de
                // pasos; el naranja deja comboLen en 0 (ataque simple).
                e->comboLen   = (e->type == ENEMY_TYPE_FOOT_SOLDIER)
                                ? comboLengthFor(e->attackType) : 0;
                e->comboStep  = 0;
                e->timer      = enemyAttackTime(e);
                break;
            }

            // -----------------------------------------------------------------
            // MOVIMIENTO HORIZONTAL — según el tipo
            // -----------------------------------------------------------------
            s16 moveX = 0;
            if (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) {
                // El naranja NO se aleja del jugador (ya no kitea): si el
                // jugador está FUERA del rango del shuriken, se acerca
                // caminando hasta entrar en rango; dentro del rango se queda
                // quieto lanzando shurikens, y si el jugador se acerca ataca
                // melee (selección de ataque de arriba). El clamp de
                // enemyMinX (borde de cámara) queda como red de seguridad.
                if (dist > ORANGE_SHURIKEN_RANGE_MAX)
                    moveX = (dx > 0) ? ENEMY_SPEED : -ENEMY_SPEED;   // acercarse
                // dentro del rango de lanzamiento → se queda en X
            } else {
                // Morado: apunta a un punto DETRÁS del jugador (espalda = lado
                // opuesto a su mirada). Si ya agotó el flanqueo, va directo.
                // Durante el cooldown retrocede para no quedar pegado.
                bool flanking = (e->flankTimer < MORADO_FLANK_TIMEOUT);
                s16  goalX    = flanking ? (targetX - targetDir * MORADO_BACK_STANDOFF)
                                         : targetX;
                s16  gdx      = goalX - e->x;
                if (e->attackCooldown > 0 && dist < ENEMY_HOLD_RANGE) {
                    if (dist < ENEMY_HOLD_RANGE - 8)
                        moveX = (dx > 0) ? -ENEMY_SPEED : ENEMY_SPEED;
                } else if (absS16(gdx) > MORADO_GOAL_TOL) {
                    moveX = (gdx > 0) ? ENEMY_SPEED : -ENEMY_SPEED;
                }
                if (flanking) e->flankTimer++;   // cuenta frames sin poder atacar
            }
            if (moveX != 0) {
                e->x += moveX;
                e->x = clampS16(e->x, enemyMinX(e), enemyMaxX(e));
            }

            // --- Morado: GIRO al invertir el sentido de la maniobra ---
            // La anim 11 (giro, 2 frames) se reproduce cuando el soldier
            // revierte su dirección horizontal mientras flanquea (p.ej. el
            // jugador se da vuelta y la espalda cambia de lado). El HFlip se
            // aplica con 'dir', así que el giro se dibuja para cada lado.
            if (e->type == ENEMY_TYPE_FOOT_SOLDIER) {
                s8 wantDir = 0;
                if (moveX > 0)       wantDir = 1;
                else if (moveX < 0)  wantDir = -1;
                if (wantDir != 0 && e->lastMoveDir != 0 && wantDir != e->lastMoveDir) {
                    e->dir = wantDir;
                    newState = ENEMY_STATE_TURN;
                    e->timer = ENEMY_GIRO_TIME;
                    enemyRestartAnim(e, ENEMY_ANIM_GIRO, FALSE);
                    e->lastMoveDir = 0;
                    break;   // salta el resto del CHASE este frame
                }
                e->lastMoveDir = wantDir;
            }

            // Siempre MIRANDO al jugador (para que el ataque salga hacia él)
            if (dx != 0) e->dir = (dx > 0) ? 1 : -1;

            // --- Movimiento vertical ---
            s16 moveY = 0;
            if (dy > ENEMY_Y_ALIGN)       moveY =  ENEMY_SPEED;
            else if (dy < -ENEMY_Y_ALIGN) moveY = -ENEMY_SPEED;
            if (moveY != 0) {
                e->y = clampS16(e->y + moveY, ENEMY_LANE_TOP, ENEMY_LANE_BOTTOM);
            }

            // --- Animación ---
            if (moveX == 0 && moveY == 0) {
                if (e->type == ENEMY_TYPE_FOOT_SOLDIER) {
                    // Morado: posturas de espera. Si otro soldier está atacando,
                    // se pone en GUARDIA (anim 12); si no, alterna entre el IDLE
                    // clásico (fila 0) y la nueva postura de espera (anim 13,
                    // "otra postura de espera") cada ENEMY_STANCE_SWITCH frames.
                    if (enemiesAttacking > 0) {
                        enemySetAnim(e, ENEMY_ANIM_GUARD, TRUE);
                    } else {
                        if (++e->stancePhase >= ENEMY_STANCE_SWITCH) {
                            e->stancePhase = 0;
                            e->stanceToggle ^= 1;
                        }
                        enemySetAnim(e, e->stanceToggle ? ENEMY_ANIM_STANCE
                                                        : ENEMY_ANIM_IDLE, TRUE);
                    }
                } else {
                    enemySetAnim(e, enemyAnimIdle(e), TRUE);
                }
            } else if (moveY < 0 && (moveX == 0 || absS16(dy) >= dist)) {
                enemySetAnim(e, enemyAnimWalkUp(e), TRUE);
            } else {
                enemySetAnim(e, enemyAnimWalk(e), TRUE);
            }
            break;
        }

        case ENEMY_STATE_ATTACK: {
            if (e->comboLen > 0) {
                // --- Combo del morado: secuencia de golpes ---
                // Cada paso corre su animación con su propio timer y ventana de
                // hitbox; al expirar se avanza al siguiente (attackHit = 0 para
                // que cada golpe conecte una vez) hasta terminar el combo.
                const ComboStep* steps = comboStepsFor(e->attackType);
                const ComboStep* step  = &steps[e->comboStep];
                if (e->timer > 0) {
                    e->timer--;
                    // Patada con salto: desplazamiento en X durante el lunge
                    if (step->lunge > 0 && e->timer > (step->time - step->lunge)) {
                        e->x += e->dir * ENEMY_KICK_SPEED;
                        e->x = clampS16(e->x, enemyMinX(e), enemyMaxX(e));
                    }
                    if (e->timer == 0) {
                        if (e->comboStep + 1 < e->comboLen) {
                            e->comboStep++;
                            e->attackHit = 0;
                            const ComboStep* ns = &steps[e->comboStep];
                            e->timer = ns->time;
                            enemyRestartAnim(e, ns->anim, FALSE);
                        } else {
                            leaveAttackState(e);
                            e->attackCooldown = (u8)(ENEMY_ATTACK_COOLDOWN + (random() & 31));
                            newState = ENEMY_STATE_CHASE;
                        }
                    }
                }
            } else if (e->timer > 0) {
                e->timer--;

                // Kick con salto: desplazamiento en X durante el lunge
                u16 kickLunge = (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE)
                                ? ORANGE_KICK_LUNGE : ENEMY_KICK_LUNGE;
                u16 kickTime  = enemyAttackTime(e);
                if (e->attackType == ENEMY_ATTACK_KICK &&
                    e->timer > (kickTime - kickLunge)) {
                    e->x += e->dir * ENEMY_KICK_SPEED;
                    e->x = clampS16(e->x, enemyMinX(e), enemyMaxX(e));
                }

                // Shuriken: spawnear proyectil en el frame 1 (timer == SPAWN_TIMER).
                // Nace 2 tiles más cerca del soldier que el borde del frame
                // (w/2 - ORANGE_SHURIKEN_NEAR_OFFSET) — antes aparecía pegado
                // a la punta del frame, lejos del cuerpo.
                if (e->attackType == ENEMY_ATTACK_SHURIKEN &&
                    e->timer == ORANGE_SHURIKEN_SPAWN_TIMER) {
                    s16 spawnX = getEnemyCenterX(e) + e->dir * (e->w / 2 - ORANGE_SHURIKEN_NEAR_OFFSET);
                    shurikenSpawn(spawnX, e->y, e->dir, e->palette);
                }
            } else {
                leaveAttackState(e);
                e->attackCooldown = (u8)(ENEMY_ATTACK_COOLDOWN + (random() & 31));
                newState = ENEMY_STATE_CHASE;
            }
            break;
        }

        case ENEMY_STATE_HURT: {
            // Sin retroceso: el golpe NO desplaza al enemigo en X (queda clavado
            // en el lugar mientras muestra la anim de daño).
            if (e->timer > 0) {
                e->timer--;
            } else {
                newState = ENEMY_STATE_CHASE;
            }
            break;
        }

        case ENEMY_STATE_TURN: {
            // Giro (anim 11, 2 frames): el soldier queda quieto dando la vuelta
            // y al terminar retoma el flanqueo. 'dir' ya quedó en la nueva
            // dirección (la setea el detección de reversión en CHASE).
            if (e->timer > 0) {
                e->timer--;
            } else {
                newState = ENEMY_STATE_CHASE;
                enemySetAnim(e, enemyAnimWalk(e), TRUE);
            }
            break;
        }

        case ENEMY_STATE_GRAB: {
            // Agarre por la espalda: el soldier queda CLAVADO a la espalda del
            // jugador (que está inmovilizado en su propio STATE_GRABBED)
            // mostrando la anim [14] (grab). La tortuga muestra la anim [18]
            // (ANIM_HELD): frames 0-2 mientras está agarrada y el frame 3
            // cuando le pegan en pleno agarre (lo maneja damagePlayer). Suelta
            // cuando el jugador zafa (mash), le pegan al soldier (damageEnemy
            // lo libera) o expira el tope de seguridad (grabTimer).
            Player* gp = e->grabbed;
            if (!gp || !playerIsGrabbed(gp)) {
                e->grabbed = NULL;
                newState = ENEMY_STATE_CHASE;
                enemySetAnim(e, enemyAnimWalk(e), TRUE);
                break;
            }
            s8  gdir = getPlayerDir(gp);
            s16 gx   = getPlayerWorldX(gp);
            s16 gy   = getPlayerY(gp);
            // Espalda del jugador: centro del player − gdir * ENEMY_GRAB_BACK_OFFSET,
            // con el soldier centrado en ese punto (su centro = x + w/2).
            e->y  = gy;
            e->x = clampS16(gx + (PLAYER_SPRITE_W / 2) - (s16)gdir * ENEMY_GRAB_BACK_OFFSET
                            - (ENEMY_SPRITE_W_PURPLE / 2),
                            enemyMinX(e), enemyMaxX(e));
            e->dir = (s8)-gdir;
            if (e->grabTimer > 0) {
                e->grabTimer--;
                if (e->grabTimer == 0) {
                    // Tope de seguridad: el jugador no zafó ni lo rescataron.
                    playerReleaseGrab(gp);
                    e->grabbed = NULL;
                    newState = ENEMY_STATE_CHASE;
                    enemySetAnim(e, enemyAnimWalk(e), TRUE);
                    break;
                }
            }
            break;
        }

        default: break;
    }

    if (newState != e->state) {
        e->state = newState;
        if (newState == ENEMY_STATE_ATTACK) {
            if (e->comboLen > 0) {
                // Combo del morado: arranca en el paso 0 (su anim y timer).
                const ComboStep* s = comboStepsFor(e->attackType);
                e->comboStep = 0;
                e->timer     = s->time;
                enemyRestartAnim(e, s->anim, FALSE);
            } else {
                u8 atkAnim;
                if (e->attackType == ENEMY_ATTACK_KICK)
                    atkAnim = enemyAnimKick(e);
                else if (e->attackType == ENEMY_ATTACK_FRONT)
                    atkAnim = enemyAnimPunchFront(e);
                else if (e->attackType == ENEMY_ATTACK_SHURIKEN)
                    atkAnim = ORANGE_ANIM_SHURIKEN;
                else
                    atkAnim = enemyAnimUppercut(e);
                enemyRestartAnim(e, atkAnim, FALSE);
            }
        }
    }

    SPR_setHFlip(e->sprite, (e->dir < 0));
    SPR_setPosition(e->sprite, e->x - e->cameraOffsetX, e->y - e->footOffset);
    SPR_setDepth(e->sprite, -(e->y));
}

// ---------------------------------------------------------------------------
// SEPARACIÓN DE GRUPO
// ---------------------------------------------------------------------------
static bool enemyIsSeparable(const Enemy* e) {
    return (e->state == ENEMY_STATE_PATROL || e->state == ENEMY_STATE_CHASE);
}

void separateEnemies(Enemy* list, u16 count) {
    for (u16 i = 0; i < count; i++) {
        if (!enemyIsSeparable(&list[i])) continue;
        for (u16 j = i + 1; j < count; j++) {
            if (!enemyIsSeparable(&list[j])) continue;

            s16 dx = list[j].x - list[i].x;
            s16 dy = list[j].y - list[i].y;
            if (absS16(dx) >= ENEMY_SEPARATE_X || absS16(dy) >= ENEMY_SEPARATE_Y)
                continue;

            s16 push = (dx > 0 || (dx == 0 && (i & 1))) ? 1 : -1;
            list[i].x = clampS16(list[i].x - push, enemyMinX(&list[i]), enemyMaxX(&list[i]));
            list[j].x = clampS16(list[j].x + push, enemyMinX(&list[j]), enemyMaxX(&list[j]));

            if (dy != 0) {
                s16 pushY = (dy > 0) ? 1 : -1;
                list[i].y = clampS16(list[i].y - pushY, ENEMY_LANE_TOP, ENEMY_LANE_BOTTOM);
                list[j].y = clampS16(list[j].y + pushY, ENEMY_LANE_TOP, ENEMY_LANE_BOTTOM);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// HITBOX DE ATAQUE — enemigo → jugador
// ---------------------------------------------------------------------------
bool enemyTryHitPlayer(Enemy* e, s16 px, s16 py) {
    if (e->state != ENEMY_STATE_ATTACK || e->attackHit || !e->sprite)
        return FALSE;

    // Shuriken no tiene hitbox melee
    if (e->attackType == ENEMY_ATTACK_SHURIKEN)
        return FALSE;

    bool active;
    s16  reach;
    if (e->comboLen > 0) {
        // Combo del morado: la ventana y el alcance los da el paso actual.
        const ComboStep* step = &comboStepsFor(e->attackType)[e->comboStep];
        active = (e->timer >= step->hitStart && e->timer <= step->hitEnd);
        reach  = step->reach;
    } else if (e->attackType == ENEMY_ATTACK_KICK) {
        u16 kickLunge = (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE)
                        ? ORANGE_KICK_LUNGE : ENEMY_KICK_LUNGE;
        u16 kickTime  = enemyAttackTime(e);
        active = (e->timer > kickTime - kickLunge);
        reach  = ENEMY_HIT_RANGE_X;
    } else {
        active = (e->timer >= ENEMY_PUNCH_HIT_START && e->timer <= ENEMY_PUNCH_HIT_END);
        reach  = enemyAttackReach(e);
    }
    if (!active)
        return FALSE;

    s16 ex  = getEnemyCenterX(e);
    s16 pcx = px + PLAYER_SPRITE_W / 2;
    s16 dx  = (e->dir >= 0) ? (pcx - ex) : (ex - pcx);
    if (dx < -ENEMY_HIT_BACK_X || dx > reach)
        return FALSE;

    if (absS16(py - e->y) > ENEMY_HIT_TOL_Y)
        return FALSE;

    e->attackHit = 1;
    return TRUE;
}
