#ifndef _ROCKSTEADY_H_
#define _ROCKSTEADY_H_

#include <genesis.h>
#include "level2.h"      // rocksteady_boss, boss_bullet (SPRITE)
#include "player.h"      // el jefe interactúa con el/los jugador(es)
#include "boss_flash.h"  // (03/10) parpadeo de vida baja por linea de sprite

// ===========================================================================
// ROCKSTEADY — jefe del nivel 2 (pasillo en llamas) y del garage (4-1)
// ===========================================================================
// Maquina de estados propia. En el 1-2 sale de la CAPSULA del taladro (sprite
// en scenes.c); en el garage, del ascensor (level4_1.c). Cada nivel le pasa su
// arena (RocksteadyArena).
//
// COMPORTAMIENTO (05/10, rehecho entero)
// Cada estado dura lo que dura su animacion y al terminar elige el siguiente,
// casi siempre con un sorteo:
//
//   QUIETO (108)   mira a la tortuga. Si no esta alineado con ella: 50% la
//                  persigue caminando, 50% embiste. Si esta alineado: camina
//                  derecho hacia adelante.
//   CAMINAR (144)  persiguiendo: se pone en su lane y se acerca hasta
//                  RS_NEAR_DX; alineado -> patada. Hacia adelante: camina
//                  derecho y al terminar embiste (o saca el arma si la tiene).
//   EMBESTIDA      amague (40) y corrida (135) a 5 px/f siguiendo la lane;
//                  pega por contacto y voltea. Al llegar al borde: 50% quieto,
//                  50% persigue.
//   PATADA (19)    pega por contacto desde el frame 1 y voltea.
//   GOLPEADO (20)  suma golpes seguidos y golpes para la caida. Con 4
//                  seguidos contraataca (patada); con 10 cae.
//   CAIDA (60)     desliza hacia atras. Al levantarse CAMBIA de arma: si no la
//                  tenia, la saca (30); si la tenia, la pierde.
//   CON ARMA (103) persigue (y patea con el arma), se alinea para disparar, o
//                  se aleja hacia el borde.
//   DISPARO (90)   dos balas rectas avanzando despacio. Si ya le pegaron 2
//                  balas a la tortuga: 50% camina rapido, 50% frenesi.
//   FRENESI (134)  cuatro tiros: dos rectos y dos en diagonal hacia arriba,
//                  dandose vuelta entre tiro y tiro (tira a los dos lados).
//
// Solo recibe dano quieto, caminando (con o sin arma) y disparando. En la
// patada, el frenesi, la caida y mientras saca el arma es intocable. En la
// EMBESTIDA (amague y corrida) los golpes le sacan vida pero no la cortan.
//
// Muerte: anim [4] hasta el ultimo frame y desaparece (ROCKSTEADY_GONE), lo
// que dispara la victoria del nivel.
//
// El arte mira SIEMPRE a la derecha -> flip con SPR_setHFlip cuando dir < 0.
// El frame es CUADRADO (104x104) y el cuerpo esta centrado.
// ===========================================================================

// --- Indices de animacion de rocksteady_boss (filas del spritesheet) ---
#define ROCKSTEADY_ANIM_IDLE        0   // Quieto
#define ROCKSTEADY_ANIM_WALK        1   // Caminar
#define ROCKSTEADY_ANIM_CHARGE      2   // Embestida (amague y corrida)
#define ROCKSTEADY_ANIM_KICK        3   // Patada
#define ROCKSTEADY_ANIM_HURT        4   // Golpeado [0][1] / cae [2][3] / se levanta [4][5] / muere
#define ROCKSTEADY_ANIM_DRAW        5   // Saca el arma
#define ROCKSTEADY_ANIM_WALK_ARMS   6   // Camina con el arma
#define ROCKSTEADY_ANIM_AIM         7   // Camina apuntando
#define ROCKSTEADY_ANIM_KICK_ARMS   8   // Patada con el arma en la mano
#define ROCKSTEADY_ANIM_SHOOT       9   // Dispara (frames a mano, ver abajo)
#define ROCKSTEADY_ANIM_HURT_ARMS  10   // Golpeado con el arma

// --- Geometria del frame (104x104, igual que las tortugas) ---
// r->x ancla el BORDE IZQUIERDO del frame; el centro del cuerpo es
// r->x + FRAME_W/2 (rocksteadyGetCenterX). r->y son los PIES (lane).
#define ROCKSTEADY_FRAME_W      104
#define ROCKSTEADY_FRAME_H      104
// El arte llega hasta la ultima fila de la celda (los pies en 104, no en 96
// como la tortuga).
#define ROCKSTEADY_FOOT_OFFSET  104

// --- Vida y dano recibido ---
// Golpe comun 1, patada en salto 2, especial ROCKSTEADY_SPECIAL_DMG.
#define ROCKSTEADY_HP           40
#define ROCKSTEADY_SPECIAL_DMG   3
#define ROCKSTEADY_JUMPKICK_DMG  2

// --- Arena del 1-2 ---
// Camara bloqueada en LEVEL2_CAM_MAX_X (120) -> mundo visible 120..440.
#define ROCKSTEADY_LANE_TOP    142
#define ROCKSTEADY_LANE_BOTTOM 196
#define ROCKSTEADY_PATROL_LEFT 150   // r->x minimo
#define ROCKSTEADY_PATROL_RIGHT 330  // r->x maximo
#define ROCKSTEADY_TALADRO_X   340   // X de mundo de la capsula del taladro
#define ROCKSTEADY_SPAWN_X     (ROCKSTEADY_TALADRO_X - 64)
#define ROCKSTEADY_EMERGE_STAND 170  // quieto en la puerta (dura say_your_p)

// Hurtbox del cuerpo (media anchura desde el centro y alto sobre los pies):
// los golpes de la tortuga conectan por solape de cajas (playerAttackHitsBox).
#define ROCKSTEADY_BODY_HALF_W  20
#define ROCKSTEADY_BODY_H       70

// --- Duraciones de cada estado (frames) ---
#define RS_IDLE_T          108
#define RS_WALK_T          144
#define RS_WINDUP_T         40
#define RS_CHARGE_T        135
#define RS_KICK_F0           2    // patada: frame 0 (sin dano)
#define RS_KICK_T           19    // ... y el frame 1 hasta 19 (pega)
#define RS_HURT_F           10    // golpeado: 2 frames de 10
#define RS_HURT_T           20
#define RS_KD_SLIDE_T       12    // caida: frame [2] deslizando
#define RS_KD_T             60    // ... y el [3] en el piso hasta 60
#define RS_GETUP_F           7    // se levanta: [4] 7 frames, [5] 8
#define RS_GETUP_T          15
#define RS_DRAW_T           30
#define RS_ARMS_T          103    // caminar con el arma
#define RS_ARMS_FAST_T     102    // caminata rapida despues de 2 balas que pegaron
#define RS_SHOOT_LOOP_T     45    // disparo: 2 vueltas de 45, una bala por vuelta
#define RS_SHOOT_T          90
#define RS_FRENZY_T        134

// --- Velocidades (Q8: 256 = 1 px por frame) ---
#define RS_WALK_Q          480    // 1,875 px/f caminando / persiguiendo
#define RS_AWAY_Q          427    // 1,67 px/f alejandose con el arma
#define RS_CHARGE_Q       1280    // 5 px/f la embestida
#define RS_SLIDE_Q         853    // 3,33 px/f deslizando al caer
#define RS_SHOOT_Q          96    // 0,375 px/f avanzando mientras dispara

// --- Distancias (centro a centro) ---
#define RS_ALIGN_DY          2    // "alineado": misma lane (+-2 px) ...
#define RS_ALIGN_DX         37    // ... y a esta distancia o menos
#define RS_NEAR_DX          31    // persiguiendo se acerca hasta aca
#define RS_FACE_DEADZONE    12    // quieto: no se da vuelta por menos que esto
#define RS_AIM_DY            1    // apuntando: lane exacta (+-1 px)

// --- Golpes por contacto (patada y embestida) ---
// Lane de la tortuga respecto de la del jefe: de -5 a +7 px.
#define RS_HIT_DY_UP         5
#define RS_HIT_DY_DOWN       7
#define RS_KICK_FWD         44    // alcance de la patada hacia adelante
#define RS_KICK_BACK        16    // ... y hacia atras (desde el centro)
#define RS_CHARGE_DX        32    // la embestida pega a menos de esto
#define ROCKSTEADY_CONTACT_DMG 1  // barras (voltea)

// --- Contadores ---
#define RS_COUNTER_HITS      4    // golpes seguidos -> contraataque
#define RS_KD_HITS          10    // golpes -> caida
#define RS_SHOT_HITS         2    // balas que pegaron -> cambia de plan

// EMBESTIDA CON ARMADURA: durante el amague y la corrida los golpes sacan vida
// pero no la cortan. Despues de cada golpe queda intocable estos frames (uno
// por swing; el especial dura ~38 frames y pega una sola vez).
#define ROCKSTEADY_CHARGE_HIT_CD     18
#define ROCKSTEADY_CHARGE_HIT_CD_SP  40

// --- Balas ---
#define MAX_ROCKSTEADY_BULLETS   6
#define ROCKSTEADY_BULLET_Q    960    // 3,75 px/f la recta
#define ROCKSTEADY_BULLET_DQ   679    // 2,65 px/f en X y en altura (diagonal 45)
#define ROCKSTEADY_BULLET_DMG    1
// Anim [9] (8 frames): [0][1] horizontal con fogonazo, [2][3] arma
// retrocedida, [4][5] arma arriba sin fogonazo, [6][7] arriba con fogonazo.
#define ROCKSTEADY_SHOOT_FR_H    0
#define ROCKSTEADY_SHOOT_FR_REC  2
#define ROCKSTEADY_SHOOT_FR_UP0  4
#define ROCKSTEADY_SHOOT_FR_UP   6
// Boca del canon medida sobre el fogonazo del sheet (celda 104x104, pies en
// la fila 104): X = columna - 52, Z = 104 - fila.
#define ROCKSTEADY_MUZZLE_H_X   40
#define ROCKSTEADY_MUZZLE_H_Z   45
#define ROCKSTEADY_MUZZLE_UP_X  15
#define ROCKSTEADY_MUZZLE_UP_Z  87
// Frames del sprite boss_bullet (sin auto-animacion).
#define ROCKSTEADY_BULLET_FR_H    0
#define ROCKSTEADY_BULLET_FR_UP   1
#define ROCKSTEADY_BULLET_FR_HIT  2
#define ROCKSTEADY_BULLET_HIT_FRAMES 16
// La bala guarda su ALTURA sobre el piso en 'z' y conserva la lane del tiro.
#define ROCKSTEADY_BULLET_MAX_Z    160
// Altura del torso de la tortuga sobre sus pies (mas su jumpZ): contra eso se
// compara la altura de la bala. La recta le pega al que esta parado; la
// diagonal, con una ventana mas ancha, al que salta.
#define ROCKSTEADY_BULLET_TARGET_Z  30
#define ROCKSTEADY_BULLET_HIT_Z     24
#define ROCKSTEADY_BULLET_HIT_Z_UP  48

typedef enum {
    ROCKSTEADY_INACTIVE,    // Todavia no aparecio
    ROCKSTEADY_EMERGE,      // Quieto en la puerta y baja a la lane de pelea
    ROCKSTEADY_IDLE,        // Quieto (decide)
    ROCKSTEADY_WALK,        // Camina sin arma (persigue o va derecho)
    ROCKSTEADY_WINDUP,      // Amague de la embestida
    ROCKSTEADY_CHARGE,      // Embestida
    ROCKSTEADY_KICK,        // Patada (con o sin arma)
    ROCKSTEADY_HURT,        // Golpeado
    ROCKSTEADY_KNOCKDOWN,   // Cae y queda en el piso
    ROCKSTEADY_GETUP,       // Se levanta
    ROCKSTEADY_DRAW,        // Saca el arma
    ROCKSTEADY_WALK_ARMS,   // Camina con el arma (persigue / apunta / se aleja)
    ROCKSTEADY_SHOOT,       // Dos tiros rectos
    ROCKSTEADY_FRENZY,      // Frenesi: cuatro tiros a los dos lados
    ROCKSTEADY_DEAD,        // Cayendo (anim [4] hasta el final)
    ROCKSTEADY_GONE         // Muerto y removido
} RocksteadyState;

// Modos de caminar.
enum { RS_MODE_CHASE, RS_MODE_AHEAD, RS_MODE_AIM, RS_MODE_AWAY };

typedef struct {
    Sprite*     sprite;
    RocksteadyState state;
    u8          armed;       // tiene el arma (cambia al levantarse de una caida)
    s16         x;           // X de MUNDO del BORDE IZQUIERDO del frame
    s16         y;           // PIES (lane)
    s16         cameraOffsetX;
    s8          dir;         // -1 mira a la izquierda / +1 a la derecha
    s16         hp;
    u8          anim;        // anim actual (evita re-setear)
    u16         timer;       // frames desde que arranco el estado
    u16         dur;         // duracion del estado (los que duran fijo)
    u8          mode;        // RS_MODE_* al caminar
    u8          fast;        // caminata rapida con el arma
    u8          combo;       // golpes seguidos (contraataque)
    u8          kdHits;      // golpes para la caida
    u8          fr, ft;      // frame y ticks de la anim manejada a mano
    BossFlash   flash;       // parpadeo de vida baja (arena.flashPal)
    u8          accX, accY;  // restos Q8 del movimiento
    s8          slideDir;    // hacia donde desliza al caer
    u8          chargeHitCD; // intocable tras un golpe en la embestida
} Rocksteady;

// (01/10) ARENA: lo que depende del escenario. rocksteadySpawn usa la del
// pasillo en llamas del 1-2 (las macros ROCKSTEADY_LANE_* / PATROL_* /
// SPAWN_X de arriba); otro nivel arma la suya y llama a rocksteadySpawnArena.
typedef struct {
    s16 laneTop, laneBot;   // franja de pies en la que pelea
    s16 xMin, xMax;         // r->x (BORDE IZQUIERDO del frame) permitido
    s16 spawnX, spawnY;     // donde aparece (r->x, pies)
    s16 emergeY;            // lane a la que baja caminando antes de pelear
    u16 emergeStand;        // frames quieto al aparecer (taunt)
    u8  pal;                // linea de paleta (sprite y balas); la carga el nivel
    s16 hp;                 // vida (0 = ROCKSTEADY_HP)
    // (03/10) Parpadeo de vida baja hecho aca: 0 = no (en el 1-2 lo hace
    // scenes.c cambiando los colores de PAL3). Si no, la LINEA a la que pasa
    // el sprite en la fase "quemada" (el nivel carga ahi la paleta quemada;
    // garage: PAL2, porque PAL3 la comparte con Bebop y April).
    u8  flashPal;
} RocksteadyArena;

// --- API pública ---
void rocksteadyInit(Rocksteady* r);
void rocksteadySpawn(Rocksteady* r);   // Aparece en la cápsula del taladro (PAL3 ya cargada)
void rocksteadySpawnArena(Rocksteady* r, const RocksteadyArena* a);   // (01/10)
void rocksteadyUpdate(Rocksteady* r, s16 cameraX, Player* p1, Player* p2, bool twoPlayers);

// (14/09) Version de N jugadores (1..4). La de arriba es un envoltorio.
void rocksteadyUpdateN(Rocksteady* r, s16 cameraX, Player** pls, u8 nPl);
bool rocksteadyIsActive(const Rocksteady* r);
bool rocksteadyCanBeHit(const Rocksteady* r);
s16  rocksteadyGetCenterX(const Rocksteady* r);
s16  rocksteadyGetCenterY(const Rocksteady* r);
void rocksteadyDamage(Rocksteady* r, s16 dmg);
// special = el especial de la tortuga: suma un golpe extra a los contadores
// de contraataque y de caida. rocksteadyDamage lo deduce del dano
// (>= ROCKSTEADY_SPECIAL_DMG).
void rocksteadyDamageEx(Rocksteady* r, s16 dmg, bool special);

// --- Balas del disparo (misma estructura que el sistema de shurikens) ---
void rocksteadyBulletInit(void);
void rocksteadyBulletUpdate(s16 camX);
void rocksteadyBulletReleaseAll(void);
// Chequea colisión de todas las balas activas contra un jugador en (px, py)
// (centro del frame). Devuelve TRUE si alguna impactó (una vez por bala).
bool rocksteadyBulletCheckHitPlayer(s16 px, s16 py, s16 pz, s16* hitX);

#endif
