#ifndef _ROBOT_H_
#define _ROBOT_H_

#include <genesis.h>
#include "level1.h"      // robot_whip, whip_waves (SPRITE, comparten PAL2)
#include "player.h"      // el robot interactúa con el/los jugador(es)

// ===========================================================================
// ROBOT DEL LÁTIGO — mini-jefe del final del nivel 1
// ===========================================================================
// Enemigo único con máquina de estados propia. Aparece saliendo del suelo unos
// tiles antes de la pared final, patrulla el ancho del arena, y en cada extremo
// gira (alineándose en Y al jugador) y ataca: LÁSER si el jugador está lejos,
// LÁTIGO si está en rango. El látigo AHORA está integrado en el sprite del robot
// (la animación de lanzamiento lo estira); ya NO se usa un sub-sprite para él.
// El LÁSER sí es un sub-sprite (whip_waves) que atraviesa el escenario.
// Comparte la paleta de los foot soldiers (PAL2).
//
// El frame del robot es GRANDE y NO cuadrado: 184x80 (23x10 tiles). El cuerpo
// vive en la parte IZQUIERDA del frame (centro ~x28) y el látigo se extiende
// hacia la derecha; al mirar a la izquierda se espeja y hay que compensar la X.
// ===========================================================================

// --- Índices de animación de robot_whip (filas del spritesheet) ---
#define ROBOT_ANIM_APPEAR      0   // Sale del suelo con el taladro (inmune)
#define ROBOT_ANIM_IDLE        1   // Quieto
#define ROBOT_ANIM_TURN        2   // Cambio de dirección (3f, termina mirando a la derecha)
#define ROBOT_ANIM_WALK        3   // Arranque del desplazamiento (humito de fricción)
#define ROBOT_ANIM_WHIP_WINDUP 4   // Previa al látigo (9f, SIEMPRE antes de lanzar)
#define ROBOT_ANIM_WHIP_THROW  5   // Lanzamiento (se estira 30->110; se reproduce al revés para recoger)
#define ROBOT_ANIM_CAUGHT      6   // Atrapó a la tortuga (3f)
#define ROBOT_ANIM_ELECTRO_A   7   // Látigo electrificado
#define ROBOT_ANIM_ELECTRO_B   8   // Variación del látigo electrificado (se alterna con 6/7)
#define ROBOT_ANIM_LASER       9   // Dispara el láser (spawnea el proyectil)
#define ROBOT_ANIM_HURT       10   // Golpeado por el jugador
#define ROBOT_ANIM_DESTROY    11   // Destruido: explota y desaparece
#define ROBOT_ANIM_WALK_LONG  12   // Desplazamiento largo (continúa tras ROBOT_ANIM_WALK)

// --- Sub-sprite del LÁSER (whip_waves): sólo se usa su animación de láser ---
#define WHIP_ANIM_LASER        4

// ---------------------------------------------------------------------------
// Geometría del frame (184x80). El cuerpo está a la izquierda; centro ~x28.
// ---------------------------------------------------------------------------
#define ROBOT_FRAME_W         184
#define ROBOT_FRAME_H          80
#define ROBOT_BODY_CX          28   // X (frame-local) del centro del cuerpo (sin espejar)
#define ROBOT_FOOT_OFFSET      72   // Pies ~72px por debajo del tope del frame
#define WHIP_SPRITE_W          96   // Ancho del sub-sprite del láser (whip_waves, 96x16)

// ---------------------------------------------------------------------------
// Vida y daño
// ---------------------------------------------------------------------------
#define ROBOT_HP               7
#define ROBOT_SPECIAL_DMG      3
#define ROBOT_FLASH_FRAMES     6
#define ROBOT_HURT_FRAMES     14

// ---------------------------------------------------------------------------
// Movimiento / patrulla (AJUSTE FINO). r->x es el CENTRO del cuerpo (mundo).
// La cámara queda fija cerca del final (~1056), arena visible ~1056..1376.
// ---------------------------------------------------------------------------
#define ROBOT_SPEED            3   // px/frame de patrulla + alineado en Y (antes 2)
#define ROBOT_LANE_TOP       142
#define ROBOT_LANE_BOTTOM    200
#define ROBOT_Y_ALIGN          2
#define ROBOT_PATROL_LEFT   1100   // centro del cuerpo, extremo izquierdo
#define ROBOT_PATROL_RIGHT  1250   // centro del cuerpo, extremo derecho (antes de la pared)
#define ROBOT_ARRIVE_MARGIN    4
#define ROBOT_WALK_START_TICKS 24  // cuánto dura la anim de arranque [3] antes de pasar a [12]
#define ROBOT_SPAWN_CENTER  1256   // centro de mundo donde emerge
#define ROBOT_SPAWN_TRIGGER 1200   // el jugador supera este worldX -> aparece
#define ROBOT_SPAWN_Y        150   // lane de pies al aparecer

// ---------------------------------------------------------------------------
// Ataques
// ---------------------------------------------------------------------------
// Decisión por distancia (centro a centro): más lejos que el alcance del látigo
// -> láser; en rango -> látigo.
#define ROBOT_WHIP_REACH_MIN  50   // alcance del látigo en el primer frame del throw
#define ROBOT_WHIP_STEP        8   // px de alcance que suma cada frame del throw
#define ROBOT_WHIP_REACH_MAX 120   // alcance máximo (umbral látigo vs láser)
#define ROBOT_WHIP_TOL_Y      20   // |dy| máx para poder atrapar
#define ROBOT_THROW_TICKS      3   // ticks por frame del lanzamiento/recogida (antes 5, más rápido)
#define ROBOT_ATTACK_COOLDOWN 45   // frames entre ataques
#define ROBOT_TURN_MAX        48   // tope de frames del giro (por si la anim es corta)

// Láser (sub-sprite, horizontal a la altura del robot)
#define ROBOT_LASER_SPEED      6   // px/frame (ajustable)
#define ROBOT_LASER_DMG        4   // barras de vida al impactar
#define ROBOT_LASER_TOL_Y     20
#define ROBOT_LASER_FIRE_DELAY 8   // frames de la anim [9] antes de soltar el rayo (antes 12)

// Electrocución del agarre: 1 barra por segundo.
#define ROBOT_ELECTRO_INTERVAL 60

typedef enum {
    ROBOT_INACTIVE,   // todavía no apareció
    ROBOT_APPEAR,     // saliendo del suelo (inmune)
    ROBOT_WALK,       // caminando hacia un extremo
    ROBOT_TURN,       // girando + alineándose en Y
    ROBOT_WINDUP,     // preparando el látigo (antes de lanzar)
    ROBOT_THROW,      // lanzando el látigo (se estira)
    ROBOT_RETRACT,    // recogiendo el látigo (throw al revés, no enganchó)
    ROBOT_GRAB,       // atrapó a la tortuga (electrocución)
    ROBOT_LASER,      // disparando el láser
    ROBOT_HURT,       // golpeado
    ROBOT_DEAD,       // explotando
    ROBOT_GONE        // destruido y removido
} RobotState;

typedef struct {
    Sprite*     sprite;
    RobotState  state;
    s16         x;             // X de MUNDO del CENTRO del cuerpo
    s16         y;             // Y = pies
    s16         cameraOffsetX;
    s8          dir;           // -1 mira izquierda / +1 mira derecha
    s16         hp;
    u8          anim;          // anim actual (evita re-setear)
    u8          flashTimer;
    u16         timer;         // timer genérico del estado
    s16         patrolTarget;  // X objetivo (centro) al caminar
    u16         walkTimer;     // para pasar de WALK [3] a WALK_LONG [12]
    u8          attackCooldown;
    u16         drainTimer;    // acumulador del drenaje de electrocución
    u8          electroTgl;    // alterna anims de electrocución
    u8          grabFrame;     // frame congelado de la electro (según distancia)

    // Lanzamiento/recogida del látigo (control manual de frames)
    u8          throwFrame;    // frame actual del throw
    u8          throwFrames;   // cantidad de frames del throw (de la sheet)
    u8          throwTick;     // contador de ticks por frame

    // Proyectil LÁSER (vida independiente)
    Sprite*     laserSpr;
    bool        laserActive;
    s16         laserX, laserY;
    s8          laserDir;
} Robot;

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------
void robotInit(Robot* r);
void robotSpawn(Robot* r, s16 centerX);
void robotUpdate(Robot* r, s16 cameraX, Player* p1, Player* p2, bool twoPlayers, u16 fps);
bool robotIsActive(const Robot* r);
bool robotCanBeHit(const Robot* r);
s16  robotGetCenterX(const Robot* r);
s16  robotGetCenterY(const Robot* r);
void robotDamage(Robot* r, s16 dmg);

#endif
