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
// Ancho de pantalla, para cortar el laser cuando SALE DE CAMARA (ver abajo).
#define ROBOT_SCREEN_W        320

// ---------------------------------------------------------------------------
// Vida y daño
// ---------------------------------------------------------------------------
#define ROBOT_HP               7
#define ROBOT_SPECIAL_DMG      3
#define ROBOT_FLASH_FRAMES     6
#define ROBOT_HURT_FRAMES     16   // duracion total del HURT
#define ROBOT_HURT_KNOCK_FRAMES 16  // frames de empuje al ser golpeado (dentro de HURT)
#define ROBOT_HURT_KNOCK_SPEED   4  // px/frame del empuje (16x4 = 64px = 8 tiles; MUY notorio
                                    // a proposito -- 30/08, para poder ajustarlo a ojo. Bajar
                                    // este numero para reducir la distancia del empuje.

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
// Limites SEPARADOS para el empuje del HURT (mas anchos que el corral de
// patrulla de arriba). Bug encontrado 30/08: el empuje clampeaba contra
// ROBOT_PATROL_LEFT/RIGHT, un corral de apenas 150px -- si el robot ya
// estaba cerca de un extremo (algo muy comun, el jugador tiende a
// arrinconarlo contra una punta a fuerza de golpes), el clamp devoraba
// el desplazamiento entero y el empuje se sentia como si no hiciera nada.
// El limite derecho deja margen (250px) sin llegar a la pared real del
// nivel (ver ENEMY_END_WALL_X_TOP/BOTTOM en enemy.h: 1308..1352 segun la
// lane). El limite IZQUIERDO (30/08) deja a
// PROPOSITO que el empuje saque al robot afuera de la camara fija de la
// zona del robot (ZONE5_ROBOT_LOCK=1056 en scenes.c, pantalla visible
// 1056..1376) -- se ve mas natural que un golpe fuerte lo mande fuera de
// cuadro un instante. No hace falta logica extra para el regreso: al
// terminar el HURT, robotStartWalk() elige como destino el extremo de
// patrulla MAS LEJANO de donde quedo (ver esa funcion), asi que si el
// empuje lo dejo a la izquierda de la camara, camina solo de vuelta hacia
// la derecha a ROBOT_SPEED px/frame, sin teletransportarse.
// --- RETIRADA post-golpe (13/09) ---------------------------------------------
// Antes, al terminar el HURT el robot volvia directo a la patrulla: caminaba al
// extremo mas lejano DE SI MISMO y se realineaba con la lane del jugador en el
// TURN. O sea que se dejaba acorralar y alcanzaba con machacar en el lugar.
// Ahora primero RETROCEDE: va al extremo mas lejano DEL JUGADOR y de paso se
// cruza a la lane opuesta, asi hay que perseguirlo en las dos direcciones.
#define ROBOT_RETREAT_MAX_FRAMES 120  // tope de seguridad de la retirada
#define ROBOT_RETREAT_ARRIVE      6   // margen para dar por llegada la lane
#define ROBOT_HURT_KNOCK_MIN_X  970
#define ROBOT_HURT_KNOCK_MAX_X 1300
#define ROBOT_ARRIVE_MARGIN    4
#define ROBOT_WALK_START_TICKS 24  // cuánto dura la anim de arranque [3] antes de pasar a [12]
#define ROBOT_SPAWN_CENTER  1256   // centro de mundo donde emerge
#define ROBOT_SPAWN_TRIGGER 1200   // el jugador supera este worldX -> aparece
#define ROBOT_SPAWN_Y        150   // lane de pies al aparecer (1 jugador / robot #1)
#define ROBOT_SPAWN_Y2       195   // lane de pies del 2do robot (modo 2 jugadores),
                                   // mismo eje X que el #1 pero distinto Y
// (16/09) Modo 4 jugadores: CUATRO robots, mismo eje X, repartidos en las
// cuatro lanes utiles (BOUND_LANE_TOP 142 .. BOUND_LANE_BOTTOM 200).
#define LEVEL1_MAX_ROBOTS      4
#define ROBOT_SPAWN_Y_4P_0   142
#define ROBOT_SPAWN_Y_4P_1   161
#define ROBOT_SPAWN_Y_4P_2   180
#define ROBOT_SPAWN_Y_4P_3   199

// ---------------------------------------------------------------------------
// Ataques
// ---------------------------------------------------------------------------
// Decisión por distancia (centro a centro): más lejos que el alcance del látigo
// -> láser; en rango -> látigo.
// Valores MEDIDOS sobre robot_whip.png (celda 184x80, cuerpo a la izquierda,
// centro en ROBOT_BODY_CX=28). Alcance = X de la punta del látigo − 28.
//   fila [5] THROW, 11 frames: punta en 79,87,95…159 → alcance 51,59,67…131
#define ROBOT_WHIP_REACH_MIN  51   // alcance del látigo en el primer frame del throw
#define ROBOT_WHIP_STEP        8   // px de alcance que suma cada frame del throw
#define ROBOT_WHIP_REACH_MAX 131   // alcance máximo (umbral látigo vs láser)
#define ROBOT_WHIP_TOL_Y      20   // |dy| máx para poder atrapar

// --- Enganche: que el cable TERMINE en la tortuga (14/09) ----------------------
// Las filas [6] CAUGHT y [7]/[8] ELECTRO_A/B NO son animaciones temporales: son
// 4 VARIANTES DE LARGO del mismo cable tenso. Medidas (punta − 28):
//   f0 = 37 px | f1 = 69 px | f2 = 101 px | f3 = 133 px
// Antes el frame se elegía escalando linealmente el frame del throw contra el
// numFrame de la anim, y el error llegaba a 38px: el cable sobresalía por
// detrás de la tortuga o le quedaba corto. Y encima CAUGHT se reproducía como
// animación, así que el cable se estiraba de 37 a 133 delante del jugador.
// Ahora se elige la variante cuyo largo está MÁS CERCA de la distancia real, se
// congela ese frame (nada de reproducir la fila), y se pega un tirón al jugador
// para que la punta caiga justo sobre su cuerpo. El error residual es como
// mucho medio escalón (16px) y se lo come el tirón.
#define ROBOT_WHIP_GRAB_R0    37
#define ROBOT_WHIP_GRAB_R1    69
#define ROBOT_WHIP_GRAB_R2   101
#define ROBOT_WHIP_GRAB_R3   133
#define ROBOT_WHIP_GRAB_N      4   // variantes de largo de CAUGHT/ELECTRO
// px que la punta entra en el cuerpo de la tortuga (0 = justo en su centro).
// Con 8 el cable muere apenas pasado el borde del torso y se lee "enganchado".
#define ROBOT_WHIP_GRAB_INSET  8
// Frames que se muestra la pose CAUGHT congelada antes de pasar a la
// electrocución (antes se esperaba a que terminara la anim, que ya no corre).
#define ROBOT_CAUGHT_FRAMES   12
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
    ROBOT_RETREAT,    // tras el golpe: se aleja del jugador Y cambia de lane
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
    s8          hurtDir;       // -1/+1: sentido del empuje al ser golpeado (lejos del atacante)
    s16         hp;
    u8          anim;          // anim actual (evita re-setear)
    u8          flashTimer;
    u16         timer;         // timer genérico del estado
    s16         patrolTarget;  // X objetivo (centro) al caminar
    s16         retreatY;      // lane objetivo de la retirada post-golpe
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
