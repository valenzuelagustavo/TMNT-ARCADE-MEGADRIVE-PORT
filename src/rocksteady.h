#ifndef _ROCKSTEADY_H_
#define _ROCKSTEADY_H_

#include <genesis.h>
#include "level2.h"      // rocksteady_boss, boss_bullet (SPRITE)
#include "player.h"      // el jefe interactúa con el/los jugador(es)

// ===========================================================================
// ROCKSTEADY — jefe final del nivel 2 (pasillo en llamas)
// ===========================================================================
// Enemigo único con máquina de estados propia, patrón de robot.c. Aparece
// saliendo de la CÁPSULA del taladro que emerge del piso del fondo de la sala
// (sprite en scenes.c); patrulla el arena (cámara bloqueada en
// LEVEL2_CAM_MAX_X) y ataca según la fase:
//
//   FASE 1 (sin arma):  estampida [2] contra el jugador y patada [3] en melee.
//                       Cada ROCKSTEADY_KD_INTERVAL golpes CAE (knock-down, anim
//                       [4]) y se levanta. Pasa a la FASE 2 cuando pierde la
//                       mitad del HP (24) o al 4º knock-down, lo que pase
//                       primero — aunque nunca se haya caído.
//   FASE 2 (con arma):  saca la ametralladora (anim [5]), se acerca apuntando
//                       [7] y dispara ráfagas de boss_bullet [9]; si el jugador
//                       se arrima, patada con el arma [8].
//
// Muerte: anim [4] hasta el último frame y desaparece (ROCKSTEADY_GONE), lo
// que dispara la victoria del nivel (scenes.c). El golpe del jugador NO es
// letal al 100% con el especial: normal −1, especial −ROCKSTEADY_SPECIAL_DMG.
//
// Se dibuja en PAL3 (su paleta se carga al aparecer; PAL3[1] se fuerza blanco
// para el texto del HUD). El arte mira SIEMPRE a la derecha → flip con
// SPR_setHFlip cuando dir < 0. El frame es CUADRADO (104x104) y el cuerpo
// centrado → no hace falta compensar la X al espejar (a diferencia del robot).
// ===========================================================================

// --- Índices de animación de rocksteady_boss (filas del spritesheet) ---
#define ROCKSTEADY_ANIM_IDLE        0   // Quieto
#define ROCKSTEADY_ANIM_WALK        1   // Caminar (entrada / reposicionarse)
#define ROCKSTEADY_ANIM_CHARGE      2   // Estampida (carga contra el jugador)
#define ROCKSTEADY_ANIM_KICK        3   // Patada (fase 1)
#define ROCKSTEADY_ANIM_HURT        4   // Recibe golpes / cae / muere (fase 1 y muerte)
#define ROCKSTEADY_ANIM_DRAW        5   // Saca el arma (transición → fase 2)
#define ROCKSTEADY_ANIM_WALK_ARMS   6   // Camina con el arma (idle de fase 2)
#define ROCKSTEADY_ANIM_AIM         7   // Camina apuntando (acercarse para disparar)
#define ROCKSTEADY_ANIM_KICK_ARMS   8   // Patada con el arma en la mano (fase 2)
#define ROCKSTEADY_ANIM_SHOOT       9   // Dispara (ráfaga de balas)
#define ROCKSTEADY_ANIM_HURT_ARMS  10   // Recibe golpes con el arma (fase 2)

// --- Geometría del frame (104x104, igual que las tortugas) ---
// r->x es el CENTRO del cuerpo (mundo); r->y son los PIES (lane).
#define ROCKSTEADY_FRAME_W      104
#define ROCKSTEADY_FRAME_H      104
#define ROCKSTEADY_FOOT_OFFSET   96   // Pies ~96px por debajo del tope del frame

// --- Vida y daño ---
#define ROCKSTEADY_HP           48   // Barras totales (media = 24 → fase 2)
#define ROCKSTEADY_SPECIAL_DMG   3   // Daño del ataque especial (botón A / B+C)
#define ROCKSTEADY_PHASE2_HP    (ROCKSTEADY_HP / 2)   // 24: pierde la mitad → fase 2
#define ROCKSTEADY_KD_MAX        4   // 4 knock-downs → fase 2 (aunque no pierda la mitad)

// --- Movimiento / patrulla ---
// Arena: cámara bloqueada en LEVEL2_CAM_MAX_X (120) → mundo visible 120..440.
#define ROCKSTEADY_SPEED         2   // px/frame al caminar/alinear lane
#define ROCKSTEADY_CHARGE_SPEED  6   // px/frame de la estampida
#define ROCKSTEADY_LANE_TOP    142
#define ROCKSTEADY_LANE_BOTTOM 196
#define ROCKSTEADY_PATROL_LEFT 150   // centro del cuerpo, extremo izquierdo
#define ROCKSTEADY_PATROL_RIGHT 330  // centro del cuerpo, extremo derecho
#define ROCKSTEADY_TALADRO_X   340   // X de mundo de la cápsula del taladro (por donde emerge)
#define ROCKSTEADY_SPAWN_X     (ROCKSTEADY_TALADRO_X - 64)   // Aparece 8 tiles (64px) a la izquierda de la cápsula
#define ROCKSTEADY_HIT_INSET    10   // Punto de impacto hundido en el cuerpo (hitbox más chica)

// --- Introducción del jefe (cápsula del taladro) ---
#define ROCKSTEADY_EMERGE_STAND 170  // Frames quieto en la puerta (≈ duración de say_your_p) antes de bajar al arena

// --- Ataques (decisión por distancia centro↔centro) ---
#define ROCKSTEADY_CHARGE_MIN   90   // distX para elegir estampida (fase 1)
#define ROCKSTEADY_MELEE_RANGE  32
#define ROCKSTEADY_SHOOT_RANGE 130   // distX para abrir el disparo (fase 2)
#define ROCKSTEADY_HIT_TOL_Y    25   // |dy| máx (pies) para conectar ataques
#define ROCKSTEADY_ATTACK_COOLDOWN 40
#define ROCKSTEADY_HURT_FRAMES  14   // Flinch tras un golpe normal
#define ROCKSTEADY_KD_INTERVAL  10   // Golpes recibidos entre knock-downs (fase 1)
#define ROCKSTEADY_KD_HOLD      60   // Frames que queda tirado en el knock-down
#define ROCKSTEADY_IDLE_MIN     30   // Quieto mínimo antes de atacar
#define ROCKSTEADY_CHARGE_MAX   80   // Tope de frames de la estampida
#define ROCKSTEADY_CHARGE_OVER  18   // Frames que sigue la estampida tras impactar (overshoot)

// --- Balas del disparo (fase 2) ---
#define MAX_ROCKSTEADY_BULLETS   6   // Proyectiles simultáneos
#define ROCKSTEADY_BULLET_SPEED  3   // px/frame
#define ROCKSTEADY_BULLET_DMG    1   // Barras de vida al impactar
#define ROCKSTEADY_SHOT_COUNT    3   // Balas por ráfaga
#define ROCKSTEADY_SHOT_TICKS    5   // Ticks entre frames de la anim de disparo
// Frames de la anim [9] en los que sale cada bala (dispara/retrocede 3 veces)
#define ROCKSTEADY_SHOT_FRAME_A  3
#define ROCKSTEADY_SHOT_FRAME_B  5
#define ROCKSTEADY_SHOT_FRAME_C  7

typedef enum {
    ROCKSTEADY_INACTIVE,    // Todavía no apareció
    ROCKSTEADY_EMERGE,      // Quieto en la puerta de la cápsula (IDLE), luego BAJA al arena caminando (WALK [1])
    ROCKSTEADY_IDLE,        // Quieto, decide el próximo ataque
    ROCKSTEADY_APPROACH,    // Camina hacia el jugador (fase 1, sin arma)
    ROCKSTEADY_CHARGE,      // Estampida: carga contra el jugador (fase 1)
    ROCKSTEADY_KICK,        // Patada melee (fase 1)
    ROCKSTEADY_HURT,        // Flinch al recibir golpe (fase 1)
    ROCKSTEADY_KNOCKDOWN,   // Cayó (anim [4] hasta el suelo), se levanta
    ROCKSTEADY_ARMS_INTRO,  // Saca el arma (anim [5]) → pasa a fase 2
    ROCKSTEADY_AIM_WALK,    // Se acerca apuntando (fase 2)
    ROCKSTEADY_SHOOT,       // Ráfaga de balas (fase 2)
    ROCKSTEADY_KICK_ARMS,   // Patada con el arma (fase 2)
    ROCKSTEADY_HURT_ARMS,   // Flinch al recibir golpe (fase 2)
    ROCKSTEADY_DEAD,        // Cayendo (anim [4] hasta el final)
    ROCKSTEADY_GONE         // Muerto y removido
} RocksteadyState;

typedef struct {
    Sprite*     sprite;
    RocksteadyState state;
    u8          phase;       // 1 = sin arma · 2 = con arma
    s16         x;           // X de MUNDO del CENTRO del cuerpo
    s16         y;           // Y = PIES (lane)
    s16         cameraOffsetX;
    s8          dir;         // -1 mira izquierda / +1 mira derecha
    s16         hp;
    u8          anim;        // Anim actual (evita re-setear)
    u16         timer;       // Timer genérico del estado (golpes/KD/idle)
    u8          attackCooldown;
    u8          hitsTaken;   // Golpes recibidos desde el último knock-down
    u8          knockdowns;  // Total de caídas en fase 1
    u8          moveToggle;  // Alterna estampida / acercarse caminando cuando está lejos (fase 1)
    u8          chargeHit;   // 1 = la estampida ya impactó en esta carga (overshoot sin re-dañar)
    // Ráfaga de disparos (control manual de frames de la anim [9])
    u8          shotFrame;   // Frame actual de SHOOT
    u8          shotsFired;  // Balas disparadas en esta ráfaga
    u8          shotTimer;   // Ticks hasta el próximo paso de frame
} Rocksteady;

// --- API pública ---
void rocksteadyInit(Rocksteady* r);
void rocksteadySpawn(Rocksteady* r);   // Aparece en la cápsula del taladro (PAL3 ya cargada)
void rocksteadyUpdate(Rocksteady* r, s16 cameraX, Player* p1, Player* p2, bool twoPlayers);
bool rocksteadyIsActive(const Rocksteady* r);
bool rocksteadyCanBeHit(const Rocksteady* r);
s16  rocksteadyGetCenterX(const Rocksteady* r);
s16  rocksteadyGetCenterY(const Rocksteady* r);
void rocksteadyDamage(Rocksteady* r, s16 dmg);

// --- Balas del disparo (misma estructura que el sistema de shurikens) ---
void rocksteadyBulletInit(void);
void rocksteadyBulletUpdate(s16 camX);
void rocksteadyBulletReleaseAll(void);
// Chequea colisión de todas las balas activas contra un jugador en (px, py)
// (centro del frame). Devuelve TRUE si alguna impactó (una vez por bala).
bool rocksteadyBulletCheckHitPlayer(s16 px, s16 py, s16* hitX);
// Rompe las balas alcanzadas por la hitbox del ataque del jugador (se llama
// ANTES de rocksteadyBulletCheckHitPlayer).
bool rocksteadyBulletBreakByPlayerAttack(const Player* p);

#endif
