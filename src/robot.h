#ifndef _ROBOT_H_
#define _ROBOT_H_

#include <genesis.h>
#include "level1.h"      // robot_whip, whip_waves (SPRITE, comparten PAL2)
#include "player.h"      // el robot interactúa con el/los jugador(es)

// ===========================================================================
// ROBOT DEL LÁTIGO — mini-jefe del final del nivel 1
// ===========================================================================
// Enemigo único, con máquina de estados propia (NO usa el pool de foot
// soldiers). Aparece saliendo del suelo unos tiles antes de la pared final,
// patrulla el ancho del arena, y en cada extremo gira (alineándose en Y al
// jugador) y ataca: LÁSER si el jugador está lejos, LÁTIGO si está en rango.
// El látigo puede atrapar a la tortuga y electrocutarla (1 barra/seg hasta que
// zafe masheando). Comparte la paleta de los foot soldiers (PAL2).
// Tanto el látigo como el láser usan un SUB-SPRITE aparte (whip_waves).
// ===========================================================================

// --- Índices de animación de robot_whip (filas del spritesheet) ---
#define ROBOT_ANIM_APPEAR     0   // Sale del suelo con el taladro (inmune)
#define ROBOT_ANIM_IDLE       1   // Quieto, mirando a la IZQUIERDA
#define ROBOT_ANIM_TURN       2   // Giro de dirección (termina mirando a la derecha)
#define ROBOT_ANIM_WALK       3   // Desplazamiento (mira a la derecha)
#define ROBOT_ANIM_WHIP       4   // Lanza el látigo
#define ROBOT_ANIM_WHIP_HOLD  5   // Quieto con el látigo desplegado
#define ROBOT_ANIM_CAUGHT     6   // El látigo atrapó a la tortuga
#define ROBOT_ANIM_ELECTRO    7   // Electrocutando a la tortuga atrapada
#define ROBOT_ANIM_LASER      8   // Saca el láser y dispara
#define ROBOT_ANIM_HURT       9   // Golpeado por el jugador
#define ROBOT_ANIM_DESTROY    10  // Destruido: explota y desaparece

// --- Índices de animación del sub-sprite whip_waves (látigo + láser) ---
#define WHIP_ANIM_SEARCH      0   // Látigo buscando (frame = distancia)
#define WHIP_ANIM_CONTACT     1   // Látigo hizo contacto
#define WHIP_ANIM_ELECTRO_A   2   // Electrocución (se intercala con B)
#define WHIP_ANIM_ELECTRO_B   3   // Electrocución (se intercala con A)
#define WHIP_ANIM_LASER       4   // Rayo láser

// ---------------------------------------------------------------------------
// Geometría (frames de 80x80 para el robot; 96x16 para el látigo/láser)
// ---------------------------------------------------------------------------
#define ROBOT_SPRITE_W        80
#define ROBOT_SPRITE_H        80
#define ROBOT_FOOT_OFFSET     72   // Pies ~72px por debajo del borde superior del frame
#define WHIP_SPRITE_W         96   // Ancho del sub-sprite del látigo/láser
#define WHIP_SPRITE_H         16
#define WHIP_HAND_Y_OFFSET    30   // Altura (desde el tope del frame) por donde sale el látigo/láser

// ---------------------------------------------------------------------------
// Vida y daño
// ---------------------------------------------------------------------------
#define ROBOT_HP               7   // Golpes normales para destruirlo
#define ROBOT_SPECIAL_DMG      3   // Daño del especial (7/3 -> 3 especiales)
#define ROBOT_FLASH_FRAMES     6   // Flash blanco al recibir un golpe
#define ROBOT_HURT_FRAMES     14   // Duración del estado "golpeado"

// ---------------------------------------------------------------------------
// Movimiento / patrulla (constantes de AJUSTE FINO)
// ---------------------------------------------------------------------------
#define ROBOT_SPEED            1   // px/frame caminando
#define ROBOT_LANE_TOP       142   // misma lane de profundidad que el resto
#define ROBOT_LANE_BOTTOM    200
#define ROBOT_Y_ALIGN          2   // tolerancia de alineación en Y con el jugador
// Extremos de patrulla (X = borde IZQUIERDO del frame de 80px). La cámara queda
// fija cerca del final (world ~1056), así que el arena visible es ~1056..1376:
// el robot patrulla DENTRO de esa franja para no salirse de pantalla.
#define ROBOT_PATROL_LEFT   1060
#define ROBOT_PATROL_RIGHT  1220   // antes de la pared del final (~1308)
#define ROBOT_ARRIVE_MARGIN    4   // "llegó al extremo" si |x - target| <= esto
// Aparición:
#define ROBOT_SPAWN_CENTER  1250   // centro de mundo del hueco de aparición
#define ROBOT_SPAWN_TRIGGER 1120   // el jugador supera este worldX -> aparece
#define ROBOT_SPAWN_Y        188   // lane de pies al aparecer

// ---------------------------------------------------------------------------
// Ataques
// ---------------------------------------------------------------------------
// El robot decide por distancia (centro a centro en X): si el jugador está a
// más de ROBOT_WHIP_REACH usa láser; si está en rango, látigo.
#define ROBOT_WHIP_REACH      96   // 3 frames x 32px de alcance del látigo
#define ROBOT_WHIP_STEP       32   // px de alcance por frame del látigo
#define ROBOT_WHIP_TOL_Y      20   // |dy| máx para poder atrapar
#define ROBOT_WHIP_HOLD       50   // frames que el látigo queda afuera buscando
#define ROBOT_ATTACK_COOLDOWN 45   // frames entre ataques (tras volver a caminar)
#define ROBOT_TURN_MAX        40   // tope de frames del giro (por si la anim es corta)

// Láser: horizontal a la altura del robot, atraviesa el nivel.
#define ROBOT_LASER_SPEED      6   // px/frame (ajustable)
#define ROBOT_LASER_DMG        4   // barras de vida al impactar
#define ROBOT_LASER_TOL_Y     20   // |dy| máx para impactar al jugador
#define ROBOT_LASER_FIRE_DELAY 10  // frames de la anim [8] antes de soltar el rayo

// Electrocución del agarre: 1 barra por segundo (NTSC 60 / PAL 50 lo maneja
// scenes.c pasando el fps; acá la constante es en frames a 60).
#define ROBOT_ELECTRO_INTERVAL 60  // 1 barra cada ~1s

typedef enum {
    ROBOT_INACTIVE,   // todavía no apareció
    ROBOT_APPEAR,     // saliendo del suelo (inmune)
    ROBOT_WALK,       // caminando hacia un extremo
    ROBOT_TURN,       // girando + alineándose en Y
    ROBOT_LASER,      // disparando el láser
    ROBOT_WHIP,       // látigo desplegado (buscando)
    ROBOT_GRAB,       // atrapó a la tortuga (electrocución)
    ROBOT_HURT,       // golpeado
    ROBOT_DEAD,       // explotando
    ROBOT_GONE        // destruido y removido
} RobotState;

typedef struct {
    Sprite*     sprite;
    RobotState  state;
    s16         x;             // X de MUNDO (borde izq. del frame)
    s16         y;             // Y = pies
    s16         cameraOffsetX;
    s8          dir;           // -1 mira izquierda / +1 mira derecha
    s16         hp;
    u8          anim;          // anim actual (evita re-setear)
    u8          flashTimer;    // frames de flash blanco restantes
    u16         timer;         // timer genérico del estado actual
    s16         patrolTarget;  // X objetivo (extremo) al caminar
    u8          attackCooldown;
    u16         drainTimer;    // acumulador para el drenaje de electrocución

    // Sub-sprite del LÁTIGO (se crea al desplegar, se libera al terminar)
    Sprite*     whipSpr;
    u8          whipElectroTgl; // alterna WHIP_ANIM_ELECTRO_A / _B

    // Proyectil LÁSER (vida independiente: viaja hasta salir o impactar)
    Sprite*     laserSpr;
    bool        laserActive;
    s16         laserX;        // X de mundo del proyectil
    s16         laserY;        // Y (altura del robot al disparar)
    s8          laserDir;
} Robot;

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

// Deja el robot en INACTIVE (sin sprite). Llamar al iniciar el nivel.
void robotInit(Robot* r);

// Hace aparecer el robot saliendo del suelo, centrado en 'centerX' (mundo).
void robotSpawn(Robot* r, s16 centerX);

// Lógica + render de un frame. Recibe la cámara y el/los jugador(es); el robot
// aplica por su cuenta el daño de láser/agarre sobre el jugador. 'fps' se usa
// para el ritmo de la electrocución (1 barra/seg).
void robotUpdate(Robot* r, s16 cameraX, Player* p1, Player* p2, bool twoPlayers, u16 fps);

// ¿El robot está en juego (apareció y no terminó de explotar)?
bool robotIsActive(const Robot* r);

// ¿Puede recibir daño del jugador ahora? (no durante aparición / muerte)
bool robotCanBeHit(const Robot* r);

// Centro del robot en coordenadas de mundo (para el sistema de colisiones).
s16  robotGetCenterX(const Robot* r);
s16  robotGetCenterY(const Robot* r);

// Aplica 'dmg' de daño (1 normal / ROBOT_SPECIAL_DMG especial).
void robotDamage(Robot* r, s16 dmg);

#endif
