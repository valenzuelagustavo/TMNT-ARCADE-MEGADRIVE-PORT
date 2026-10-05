#ifndef _ROBOT_H_
#define _ROBOT_H_

#include <genesis.h>
#include "level1.h"      // robot_whip, whip_waves (SPRITE, comparten PAL2)
#include "player.h"      // el robot interactúa con el/los jugador(es)

// ===========================================================================
// ROBOT DEL LÁTIGO — mini-jefe del final del nivel 1
// ===========================================================================
// Enemigo único con máquina de estados propia. Aparece saliendo del suelo unos
// tiles antes de la pared final y pelea asi (05/10):
//   CORRE (120)   a 2 px/f de un lado al otro, cruzando a la tortuga: se da
//                 vuelta al salir de camara o al quedar ROBOT_FLEE_BEHIND px
//                 detras de ella. Va un poco por encima de su lane. Es el
//                 UNICO momento en que se le puede pegar.
//   FRENA (48)    mira a la tortuga y se alinea en su lane.
//   ATACA         50% LÁTIGO (si engancha, electrocuta hasta que la tortuga
//                 zafa) y 50% LÁSER. Despues vuelve a correr.
//   GOLPEADO (24) retrocede y vuelve a correr.
// El látigo está integrado en el sprite del robot (la animación de
// lanzamiento lo estira); ya NO se usa un sub-sprite para él.
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
// Ancho de pantalla, para cortar el laser cuando SALE DE CAMARA (ver abajo).
#define ROBOT_SCREEN_W        320

// ---------------------------------------------------------------------------
// Vida y daño
// ---------------------------------------------------------------------------
// Cinco golpes: cualquier golpe de la tortuga (comun, patada o especial) le
// saca uno.
#define ROBOT_HP               5
#define ROBOT_SPECIAL_DMG      1

// ---------------------------------------------------------------------------
// Movimiento. r->x es el CENTRO del cuerpo (mundo).
// La cámara queda fija cerca del final (~1056), arena visible ~1056..1376.
// ---------------------------------------------------------------------------
#define ROBOT_LANE_TOP       142
#define ROBOT_LANE_BOTTOM    200
#define ROBOT_MIN_X         1000   // tope de seguridad (el empuje lo puede sacar de cuadro)
#define ROBOT_MAX_X         1300
#define ROBOT_FLEE_T         120   // corre ...
#define ROBOT_FLEE_Q         512   // ... a 2 px/f
#define ROBOT_FLEE_LANE_Q     32   // corrige la lane a 0,125 px/f ...
#define ROBOT_FLEE_LANE_DY    10   // ... quedando estos px por ENCIMA de la tortuga
#define ROBOT_FLEE_BEHIND     67   // se da vuelta al quedar esto detras de la tortuga
#define ROBOT_BRAKE_T         48   // frena mirando a la tortuga ...
#define ROBOT_BRAKE_Q        256   // ... alineandose a 1 px/f
#define ROBOT_WALK_START_TICKS 24  // anim de arranque [3] antes de pasar a [12]
#define ROBOT_HURT_T          24   // golpeado: retrocede ...
#define ROBOT_HURT_Q         512   // ... a 2 px/f
#define ROBOT_SPAWN_CENTER  1256   // centro de mundo donde emerge
#define ROBOT_SPAWN_TRIGGER 1200   // el jugador supera este worldX -> aparece
#define ROBOT_SPAWN_Y        150   // lane de pies al aparecer (1 jugador / robot #1)
#define ROBOT_SPAWN_Y2       195   // lane de pies del 2do robot (modo 2 jugadores),
                                   // mismo eje X que el #1 pero distinto Y
// Modo 4 jugadores: CUATRO robots, mismo eje X, repartidos en las cuatro
// lanes utiles (BOUND_LANE_TOP 142 .. BOUND_LANE_BOTTOM 200).
#define LEVEL1_MAX_ROBOTS      4
#define ROBOT_SPAWN_Y_4P_0   142
#define ROBOT_SPAWN_Y_4P_1   161
#define ROBOT_SPAWN_Y_4P_2   180
#define ROBOT_SPAWN_Y_4P_3   199

// ---------------------------------------------------------------------------
// Ataques
// ---------------------------------------------------------------------------
// Al terminar de frenar: 50% latigo, 50% laser.
// Alcance del látigo MEDIDO sobre robot_whip.png (celda 184x80, cuerpo a la
// izquierda, centro en ROBOT_BODY_CX=28). Alcance = X de la punta − 28.
//   fila [5] THROW, 11 frames: punta en 79,87,95…159 → alcance 51,59,67…131
#define ROBOT_WHIP_REACH_MIN  51   // alcance del látigo en el primer frame del throw
#define ROBOT_WHIP_STEP        8   // px de alcance que suma cada frame del throw
#define ROBOT_WHIP_REACH_MAX 131   // alcance máximo
#define ROBOT_WHIP_TOL_Y       7   // |dy| máx para poder atrapar

// --- Enganche: que el cable TERMINE en la tortuga ------------------------------
// Las filas [6] CAUGHT y [7]/[8] ELECTRO_A/B son 4 VARIANTES DE LARGO del
// mismo cable tenso (punta − 28): 37, 69, 101 y 133 px. Se elige la variante
// mas cercana a la distancia real, se congela ese frame y se le pega un tiron
// a la tortuga para que la punta caiga justo sobre su cuerpo.
#define ROBOT_WHIP_GRAB_R0    37
#define ROBOT_WHIP_GRAB_R1    69
#define ROBOT_WHIP_GRAB_R2   101
#define ROBOT_WHIP_GRAB_R3   133
#define ROBOT_WHIP_GRAB_N      4   // variantes de largo de CAUGHT/ELECTRO
// px que la punta entra en el cuerpo de la tortuga (0 = justo en su centro).
#define ROBOT_WHIP_GRAB_INSET  8
// Frames que se muestra la pose CAUGHT congelada antes de la electrocución.
#define ROBOT_CAUGHT_FRAMES   12
#define ROBOT_THROW_TICKS      3   // ticks por frame del lanzamiento/recogida

// Láser (sub-sprite, horizontal a la altura del robot)
#define ROBOT_LASER_T         66   // dura la pose del disparo
#define ROBOT_LASER_Q        896   // 3,5 px/f
#define ROBOT_LASER_DMG        1   // barras de vida al impactar
#define ROBOT_LASER_TOL_Y      5
#define ROBOT_LASER_FIRE_DELAY 8   // frames de la anim [9] antes de soltar el rayo

// Electrocución del agarre: 1 barra cada 2 segundos.
#define ROBOT_ELECTRO_SECONDS  2

typedef enum {
    ROBOT_INACTIVE,   // todavía no apareció
    ROBOT_APPEAR,     // saliendo del suelo (inmune)
    ROBOT_FLEE,       // corre de un lado al otro, cruzando a la tortuga
    ROBOT_BRAKE,      // frena, mira a la tortuga y se alinea
    ROBOT_WINDUP,     // preparando el látigo (antes de lanzar)
    ROBOT_THROW,      // lanzando el látigo (se estira)
    ROBOT_RETRACT,    // recogiendo el látigo (throw al revés, no enganchó)
    ROBOT_GRAB,       // atrapó a la tortuga (electrocución)
    ROBOT_LASER,      // disparando el láser
    ROBOT_HURT,       // golpeado (retrocede)
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
    s8          hurtDir;       // -1/+1: sentido del retroceso al ser golpeado
    s16         hp;
    u8          anim;          // anim actual (evita re-setear)
    u16         timer;         // frames desde que arranco el estado
    u8          accX, accY;    // restos Q8 del movimiento
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
    u8          laserAcc;
} Robot;

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------
void robotInit(Robot* r);
void robotSpawn(Robot* r, s16 centerX, s16 spawnY);
void robotUpdate(Robot* r, s16 cameraX, Player* p1, Player* p2, bool twoPlayers, u16 fps);

// (14/09) Version de N jugadores (1..4). La de arriba es un envoltorio.
void robotUpdateN(Robot* r, s16 cameraX, Player** pls, u8 nPl, u16 fps);
bool robotIsActive(const Robot* r);
bool robotCanBeHit(const Robot* r);
s16  robotGetCenterX(const Robot* r);
s16  robotGetCenterY(const Robot* r);
void robotDamage(Robot* r, s16 dmg, s16 attackerX);

#endif
