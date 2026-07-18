#ifndef _ENEMY_H_
#define _ENEMY_H_

#include <genesis.h>
#include "enemies.h"

#define MAX_ENEMIES         8
#define ENEMY_SPEED         1
#define ENEMY_AGGRO_RANGE   200
#define ENEMY_ATTACK_RANGE  44   // Distancia (centro a centro) para lanzar ataque
// El sheet del foot soldier usa la MISMA grilla que las tortugas: frames de
// 104x104px (13x13 tiles) con el arte en la parte baja del frame.
// Mantener en sincronía con PLAYER_SPRITE_W / PLAYER_FOOT_OFFSET de player.h.
#define ENEMY_SPRITE_W      104  // Ancho del frame (px)
#define ENEMY_SPRITE_H      104  // Alto del frame (px)
#define ENEMY_FOOT_OFFSET   96   // Pies ~96px por debajo del borde superior del frame
#define ENEMY_HP            4    // Golpes necesarios para eliminar al foot soldier
#define MAX_ACTIVE_ENEMIES  4    // Foot soldiers vivos al mismo tiempo (tope de spawn)
#define ENEMY_INVINCIBLE    20
#define ENEMY_FLASH_FRAMES  6    // Frames que dura el flash blanco al recibir golpe

// ---------------------------------------------------------------------------
// Animaciones del spritesheet del foot soldier (orden de filas en Aseprite).
// El arte mira a la DERECHA → se aplica HFlip cuando dir == -1.
// ---------------------------------------------------------------------------
#define ENEMY_ANIM_IDLE     0   // Quieto
#define ENEMY_ANIM_WALK     1   // Camina a izquierda / derecha / hacia abajo
#define ENEMY_ANIM_KICK     2   // Patada con salto: se desplaza en X
#define ENEMY_ANIM_PUNCH    3   // Uppercut
#define ENEMY_ANIM_WALK_UP  4   // Camina hacia arriba de la pantalla

// ---------------------------------------------------------------------------
// Movimiento vertical — lane de profundidad (coordenadas de PIES).
// Mantener en sincronía con BOUND_LANE_TOP/BOTTOM de player.h.
// ---------------------------------------------------------------------------
#define ENEMY_LANE_TOP      150  // Pies al fondo (base del muro de edificios)
#define ENEMY_LANE_BOTTOM   192  // Pies al frente (borde de la vereda/cuneta)
#define ENEMY_Y_ALIGN         2  // Tolerancia: dentro de esto no se ajusta más la Y
#define ENEMY_ATTACK_TOL_Y   16  // |dy| máximo con el jugador para lanzar un ataque
#define ENEMY_STOP_RANGE     36  // Distancia X mínima: no seguir empujando al jugador

// ---------------------------------------------------------------------------
// Agresividad (Fase 2) — ritmo de ataque y comportamiento de grupo
// ---------------------------------------------------------------------------
#define ENEMY_MAX_ATTACKERS    2   // Foot soldiers atacando A LA VEZ (el resto rodea)
#define ENEMY_ATTACK_COOLDOWN 60   // Frames mínimos entre ataques del mismo enemigo
                                   // (se le suma random()&31 → 60..91, ~1-1.5s)
#define ENEMY_HURT_COOLDOWN   30   // Cooldown tras recibir un golpe (no contraataca ya)
#define ENEMY_HOLD_RANGE      72   // En cooldown y más cerca que esto → retrocede

// Separación entre enemigos (que no se encimen entre ellos)
#define ENEMY_SEPARATE_X      32   // Si dos enemigos están a menos de esto en X...
#define ENEMY_SEPARATE_Y      12   // ...y menos de esto en Y, se empujan 1px/frame

// ---------------------------------------------------------------------------
// Targeting en 2 jugadores (Fase 3)
// ---------------------------------------------------------------------------
// Cada enemigo tiene UN target asignado (P1 o P2). Al spawnear se asigna al
// jugador con menos enemigos encima (reparto parejo). Cada RETARGET_INTERVAL
// frames re-evalúa: solo cambia de blanco si el otro jugador está
// SIGNIFICATIVAMENTE más cerca (histéresis) — evita el flip-flop de target
// del código anterior, que en la práctica los dejaba pegados a P1.
#define ENEMY_RETARGET_INTERVAL    32  // Frames entre re-evaluaciones de target
#define ENEMY_RETARGET_HYSTERESIS  48  // El otro debe estar 48px MÁS cerca para cambiar

// ---------------------------------------------------------------------------
// Ataques (duraciones en frames — ajustar al largo real de cada animación)
// ---------------------------------------------------------------------------
#define ENEMY_ATTACK_PUNCH  0    // valor de Enemy.attackType
#define ENEMY_ATTACK_KICK   1

// Duraciones calzadas con el sheet real (frames de anim x 8 ticks de FAST 8):
// punch = 2 frames x 8 = 16 | kick = 4 frames x 8 = 32
#define ENEMY_PUNCH_TIME    16   // Duración total del uppercut
#define ENEMY_KICK_TIME     32   // Duración total de la patada con salto
#define ENEMY_KICK_LUNGE    16   // Frames iniciales del kick CON desplazamiento
#define ENEMY_KICK_SPEED     2   // px/frame de avance durante el lunge (16*2 = 32px)

// --- Hitbox de los ataques (contra el jugador) ---
// Ventanas ACTIVAS en frames del timer (que cuenta hacia atrás desde *_TIME):
//   kick : activa durante todo el lunge (timer > KICK_TIME - LUNGE)
//   punch: activa en el tramo medio del uppercut
#define ENEMY_PUNCH_HIT_START  4   // timer mínimo (inclusive) con hitbox activa
#define ENEMY_PUNCH_HIT_END   12   // timer máximo (inclusive) con hitbox activa
#define ENEMY_HIT_RANGE_X     56   // Alcance del golpe hacia adelante (centro a centro)
#define ENEMY_HIT_BACK_X       8   // Tolerancia hacia atrás (encimados)
#define ENEMY_HIT_TOL_Y       16   // |dy| máximo (pies) para conectar el golpe

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
    u8          palette;      // Línea de paleta normal del sprite (PAL0..PAL3)
    u8          flashTimer;   // Frames restantes de flash blanco (0 = sin flash)
    u8          anim;         // Animación actual (evita re-setear la misma anim)
    u8          attackType;   // ENEMY_ATTACK_PUNCH o ENEMY_ATTACK_KICK
    u8          attackHit;    // 1 = este ataque ya conectó (un golpe por swing)
    u8          attackCooldown; // Frames hasta poder volver a atacar (0 = listo)
    u8          target;       // Jugador asignado: 0 = P1, 1 = P2
    u8          retargetTimer; // Frames hasta la próxima re-evaluación de target
} Enemy;

// Carga la paleta "flash" (silueta blanca) en la línea palLine. Llamar UNA VEZ
// al iniciar el nivel, antes del primer spawn. En Level 1: PAL3 (libre).
void initEnemyFlashPalette(u16 palLine);

// Resetea el estado global de la IA (contador de atacantes simultáneos y
// reparto de targets) e informa cuántos jugadores hay (1 o 2).
// Llamar UNA VEZ al iniciar cada nivel, antes del primer spawn.
void resetEnemyAI(u8 numPlayers);

// Separación de grupo: empuja de a 1px a los pares de enemigos (en PATROL o
// CHASE) que estén encimados, para que no se apilen en el mismo lugar.
// Llamar una vez por frame ANTES de los updateEnemy.
void separateEnemies(Enemy* list, u16 count);

void initEnemySpawn(Enemy* e, s16 spawnX, s16 y, s16 patrolRange, u8 palette);
void updateEnemy(Enemy* e, s16 player1X, s16 player1Y, s16 player2X, s16 player2Y, bool twoPlayers);
void setEnemyCamera(Enemy* e, s16 camX);
bool damageEnemy(Enemy* e, s16 dmg);
bool enemyCanBeHit(const Enemy* e);
s16  getEnemyCenterX(const Enemy* e);
s16  getEnemyCenterY(const Enemy* e);

// Intenta conectar el ataque en curso contra un jugador en (px, py) — coords
// de mundo, px = borde izquierdo del frame (misma grilla de 104px), py = pies.
// Devuelve TRUE una sola vez por swing (marca attackHit); el llamador aplica
// damagePlayer. Chequear playerCanBeHit ANTES de llamar, para no "gastar" el
// golpe contra un jugador invulnerable.
bool enemyTryHitPlayer(Enemy* e, s16 px, s16 py);

#endif
