#ifndef _ENEMY_H_
#define _ENEMY_H_

#include <genesis.h>
#include "enemies.h"
#include "player.h"

// ---------------------------------------------------------------------------
// Tipos de enemigo (determinan spritesheet, paleta y comportamiento de ataque)
// ---------------------------------------------------------------------------
#define ENEMY_TYPE_FOOT_SOLDIER        0   // foot soldier regular (PAL2)
#define ENEMY_TYPE_FOOT_SOLDIER_ORANGE 1   // foot soldier naranja (PAL3, shuriken)

#define MAX_ENEMIES         8
#define ENEMY_SPEED         1
#define ENEMY_AGGRO_RANGE   200
// Des-aggro: MÁS que una pantalla completa (330px) + margen. Los enemigos de
// oleada spawnean off-screen ya persiguiendo; con el histórico AGGRO+60 (260)
// un spawn trasero con el jugador en la otra punta de la pantalla se
// "olvidaba" de perseguir y quedaba patrullando fuera de cámara.
#define ENEMY_DEAGGRO_RANGE 400
#define ENEMY_ATTACK_RANGE  60   // Distancia (centro a centro) para lanzar ataque
// Selección de ataque por distancia: uppercut sólo si está MUY pegado; a media
// distancia elige entre patada (se desplaza) y directo (más alcance).
#define ENEMY_UPPERCUT_RANGE 30  // < esto -> uppercut (corto)
#define ENEMY_UPPERCUT_REACH 24  // hitbox del uppercut (corto, -40%)
#define ENEMY_FRONT_REACH    38  // hitbox del directo (medio, -40%)
// ---------------------------------------------------------------------------
// Dimensiones de frame POR TIPO.
// El morado usa la sheet nueva de 64x80px (8x10 tiles) con los pies pegados al
// borde inferior; el naranja mantiene la grilla vieja de 104x104px (13x13
// tiles, arte en la parte baja del frame). Mantener sincronizado con el
// .res (enemies.res) y con PLAYER_SPRITE_W / PLAYER_FOOT_OFFSET de player.h.
// ---------------------------------------------------------------------------
#define ENEMY_SPRITE_W_PURPLE   64   // Ancho del frame morado (px)
#define ENEMY_SPRITE_H_PURPLE   80   // Alto del frame morado (px)
#define ENEMY_FOOT_OFFSET_PURPLE 80  // Pies en el borde inferior del frame morado
#define ENEMY_SPRITE_W_ORANGE  104   // Ancho del frame naranja (px)
#define ENEMY_SPRITE_H_ORANGE  104   // Alto del frame naranja (px)
#define ENEMY_FOOT_OFFSET_ORANGE 96  // Pies ~96px bajo el borde superior (naranja)
#define ENEMY_HP            4    // Golpes necesarios para eliminar al foot soldier
#define MAX_ACTIVE_ENEMIES  4    // Foot soldiers vivos al mismo tiempo (tope de spawn)
#define ENEMY_INVINCIBLE    20

// ---------------------------------------------------------------------------
// Animaciones del spritesheet del foot soldier (orden de filas en Aseprite).
// El arte mira a la DERECHA → se aplica HFlip cuando dir == -1.
// ---------------------------------------------------------------------------
#define ENEMY_ANIM_IDLE        0   // Quieto
#define ENEMY_ANIM_WALK        1   // Camina a izquierda / derecha / hacia abajo
#define ENEMY_ANIM_KICK        2   // Patada con salto: se desplaza en X
#define ENEMY_ANIM_PUNCH       3   // Uppercut
#define ENEMY_ANIM_WALK_UP     4   // Camina hacia arriba de la pantalla
#define ENEMY_ANIM_EXPLODE     5   // Muerte (6 frames) — desplaza en X con el golpe
#define ENEMY_ANIM_PUNCH_FRONT 6   // Golpe de frente / directo con el puño (2 frames)
#define ENEMY_ANIM_BREAK_DOOR  7   // Rompe la puerta al spawnear (5 frames)
#define ENEMY_ANIM_HIT_1       8   // Golpe recibido — se alternan 8/9/10 en cada golpe
#define ENEMY_ANIM_HIT_2       9
#define ENEMY_ANIM_HIT_3      10
#define ENEMY_ANIM_GIRO       11   // Giro (2f): arranca mirando a la derecha y termina
                                   // mirando a la izquierda (se HFlip con dir)
#define ENEMY_ANIM_GUARD      12   // Guardia (3f): postura defensiva mientras otros atacan
#define ENEMY_ANIM_STANCE     13   // Otra postura de espera (3f), parado sin moverse
#define ENEMY_ANIM_GRAB       14   // Agarre por la espalda (pose visible: el soldier
                                   // la muestra durante todo el GRAB)
#define ENEMY_ANIM_VOLTERETA  15   // Voltereta de entrada (7f), avanza mas en X

// ---------------------------------------------------------------------------
// Animaciones del foot soldier NARANJA (orden de filas en foot_soldier_orange.png).
// El orden es DISTINTO al regular: las constantes no coinciden.
// ---------------------------------------------------------------------------
#define ORANGE_ANIM_IDLE          0   // Quietoa (1 frame)
#define ORANGE_ANIM_WALK          1   // Caminar (4 frames)
#define ORANGE_ANIM_WALK_UP       2   // Caminar hacia arriba (4 frames)
#define ORANGE_ANIM_SHURIKEN      3   // Lanzar shuriken (3 frames) — spawnea proyectil
#define ORANGE_ANIM_PUNCH_FRONT   4   // Puñetazo de frente (2 frames)
#define ORANGE_ANIM_UPPERCUT      5   // Uppercut, menor alcance (3 frames)
#define ORANGE_ANIM_EXPLODE       6   // Muerte (4 frames)
#define ORANGE_ANIM_HIT           7   // Golpe recibido (1 frame, sin alternancia)
#define ORANGE_ANIM_KICK          8   // Patada con salto (4 frames, desplaza en X)

// ---------------------------------------------------------------------------
// Movimiento vertical — lane de profundidad (coordenadas de PIES).
// Mantener en sincronía con BOUND_LANE_TOP/BOTTOM de player.h (ampliada
// 1 tile en cada extremo el 19/07 para dar mas movilidad al jugador,
// sobre todo saltando; los enemigos siguen la misma franja para no dejar
// zonas de la vereda sin cobertura de IA).
#define ENEMY_LANE_TOP      142  // Pies al fondo (1 tile mas alla del muro de edificios)
#define ENEMY_LANE_BOTTOM   200  // Pies al frente (1 tile mas alla del borde de la vereda/cuneta)
// X mínima de mundo: NEGATIVA por tipo para que los spawns "por la espalda" puedan
// nacer fuera de pantalla a la izquierda cuando la cámara está cerca del
// inicio del nivel (con el clamp viejo en 0 aparecían con medio cuerpo visible).
// En el código se usa -(s16)e->w.

// ---------------------------------------------------------------------------
// Pared diagonal al FINAL del nivel (hueco de escalera / fire escape).
// Dibujada en PERSPECTIVA en el fondo, no como pared vertical: el borde
// sólido está más atrás (X menor) en la lane del fondo y más adelante
// (X mayor) en la lane del frente. Mantener en sincronía con
// LEVEL_END_WALL_X_TOP/BOTTOM de player.h (mismos valores, calibrados
// sobre bg01_completa.png).
// ---------------------------------------------------------------------------
#define ENEMY_END_WALL_X_TOP     1308  // borde solido en Y=ENEMY_LANE_TOP (fondo)
#define ENEMY_END_WALL_X_BOTTOM  1352  // borde solido en Y=ENEMY_LANE_BOTTOM (frente)
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
#define ENEMY_ATTACK_PUNCH  0    // valor de Enemy.attackType — uppercut (anim 3)
#define ENEMY_ATTACK_KICK   1    // patada con salto (anim 2)
#define ENEMY_ATTACK_FRONT  2    // golpe de frente / directo (anim 6)
#define ENEMY_ATTACK_SHURIKEN 3  // lanzar shuriken (solo naranja, anim 3)

// --- Shuriken (proyectil del foot soldier naranja) ---
#define MAX_SHURIKENS           4   // proyectiles simultáneos en pantalla
#define ORANGE_SHURIKEN_SPEED   3   // px/frame de desplazamiento en X
#define ORANGE_SHURIKEN_DMG     1   // barras de vida al impactar
#define ORANGE_SHURIKEN_SPAWN_TIMER 16  // timer del ataque al que se spawnea (frame 1 de 3)
// Desplazamiento del spawn respecto del CENTRO del frame: borde del frame
// (w/2 = 52px) quedaba lejos del cuerpo → el shuriken "nacía" pegado a la
// punta del frame. Ahora nace 2 tiles (16px) más cerca del soldier (52-16=36).
#define ORANGE_SHURIKEN_NEAR_OFFSET 16
#define ORANGE_SHURIKEN_RANGE_MIN 30   // rango mínimo para elegir shuriken
#define ORANGE_SHURIKEN_RANGE_MAX 180  // rango máximo para elegir shuriken (kiter a distancia larga)

// ---------------------------------------------------------------------------
// COMPORTAMIENTO POR TIPO (28/07)
// ---------------------------------------------------------------------------
// Morado (ENEMY_TYPE_FOOT_SOLDIER): en vez de encarar de frente, maniobra para
// caer en la ESPALDA del jugador (lado opuesto a su mirada) y pegar desde atrás
// (que dispara la anim HIT_BEHIND del jugador). Si no lo logra en cierto tiempo
// —jugador contra la pared, encimado con otro enemigo— ataca de frente igual.
#define MORADO_BACK_STANDOFF   20   // Punto objetivo: px por detrás del jugador
                                    // (dentro del alcance del uppercut, 24px)
#define MORADO_GOAL_TOL         4   // Tolerancia al llegar a ese punto (≈1 paso)
#define MORADO_FLANK_TIMEOUT   90   // Frames intentando flanquear antes de encarar

// Naranja (ENEMY_TYPE_FOOT_SOLDIER_ORANGE): NO kitea ni se aleja del jugador.
// A distancia lanza shurikens (solo se acerca caminando si el jugador está
// FUERA del rango del shuriken, para no quedar en un punto muerto); cuando el
// jugador se acerca, responde con ataques melee (patada o directo).
#define ORANGE_KICK_RANGE      56   // Jugador dentro de esto → melee (patada/directo)

// Duraciones calzadas con el sheet real (frames de anim x 8 ticks de FAST 8):
// punch = 2 frames x 8 = 16 | kick = 4 frames x 8 = 32 | front = 2 frames x 8 = 16
#define ENEMY_PUNCH_TIME    16   // Duración total del uppercut (y del directo)
#define ENEMY_KICK_TIME     32   // Duración total de la patada con salto
#define ENEMY_KICK_LUNGE    16   // Frames iniciales del kick CON desplazamiento
#define ENEMY_KICK_SPEED     3   // px/frame de avance durante el lunge (16*3 = 48px, se desplaza más)

// Muerte con explosión (anim 5 = 4 frames x 8) y rotura de puerta al spawnear
// (anim 7: se reproduce desde el 2do frame → quedan 4 frames x 8).
// Sin retroceso: el enemigo muere (y se golpea) EN EL LUGAR, sin desplazarse
// en X — se eliminó el knockback de HURT y el empuje de la muerte.
#define ENEMY_EXPLODE_TIME     48   // Muerte: 6 frames x 8 ticks
#define ENEMY_BREAK_DOOR_TIME  32
// Spawn desde ascensor: sólo los 2 últimos frames de BREAK_DOOR (índices 3-4).
#define ENEMY_ELEV_SPAWN_TIME  16

// --- Animaciones nuevas del morado (duraciones en frames x 8 ticks) ---
#define ENEMY_GIRO_TIME        16   // Giro: 2 frames x 8
#define ENEMY_SOMERSAULT_TIME  56   // Voltereta de entrada: 7 frames x 8
#define ENEMY_SOMERSAULT_SPEED  3   // px/frame durante la voltereta (avanza mas que el walk)
#define ENEMY_GRAB_RANGE       44   // Distancia (centro de frame a centro) para agarrar por la espalda
// Agarre por la espalda: distancia centro-a-centro al sostener al jugador
// (el soldier queda justo detrás de la espalda del jugador agarrado).
#define ENEMY_GRAB_BACK_OFFSET 42
// Tope de SEGURIDAD del agarre de pie: si el jugador no mashea ni lo golpean
// (p.ej. quedó solo contra el soldier), se suelta solo a los 4s. El látigo del
// robot no tiene este tope (su drenaje vacía la vida y termina en KO).
#define ENEMY_GRAB_MAX_TIME    240
// Posturas de espera del morado: frames alternando IDLE/STANCE estando quieto
// (STANCE = la nueva "otra postura de espera", fila 13).
#define ENEMY_STANCE_SWITCH    120

// --- Duraciones del foot soldier naranja (frames x 8 ticks) ---
#define ORANGE_SHURIKEN_TIME    24   // 3 frames x 8 = lanzamiento de shuriken
#define ORANGE_UPPERCUT_TIME    24   // 3 frames x 8
#define ORANGE_PUNCH_TIME       16   // 2 frames x 8
#define ORANGE_EXPLODE_TIME     32   // 4 frames x 8
#define ORANGE_KICK_TIME        32   // 4 frames x 8
#define ORANGE_KICK_LUNGE       16   // frames iniciales con desplazamiento

// --- Hitbox de los ataques (contra el jugador) ---
// Ventanas ACTIVAS en frames del timer (que cuenta hacia atrás desde *_TIME):
//   kick : activa durante todo el lunge (timer > KICK_TIME - LUNGE)
//   punch: activa en el tramo medio del uppercut
#define ENEMY_PUNCH_HIT_START  4   // timer mínimo (inclusive) con hitbox activa
#define ENEMY_PUNCH_HIT_END   12   // timer máximo (inclusive) con hitbox activa
#define ENEMY_HIT_RANGE_X     34   // Alcance del golpe hacia adelante (centro a centro, -40%)
#define ENEMY_HIT_BACK_X       8   // Tolerancia hacia atrás (encimados)
#define ENEMY_HIT_TOL_Y       16   // |dy| máximo (pies) para conectar el golpe

typedef enum {
    ENEMY_STATE_INACTIVE,
    ENEMY_STATE_PATROL,
    ENEMY_STATE_CHASE,
    ENEMY_STATE_ATTACK,
    ENEMY_STATE_HURT,
    ENEMY_STATE_DEAD,
    ENEMY_STATE_SPAWNING,   // rompiendo la puerta; al terminar la anim pasa a CHASE
    ENEMY_STATE_TURN,       // giro (cambio de direccion mientras flanquea)
    ENEMY_STATE_GRAB        // agarrando al jugador por la espalda
} EnemyState;

// Entrada de spawn de una OLEADA: cuando el borde derecho de la cámara supera
// triggerX, el enemigo entra por el flanco 'side' en la lane 'y'. La X real
// se calcula al spawnear, fuera de pantalla relativo a la cámara del momento.
// Varias entradas con el mismo triggerX = una oleada.
typedef struct {
    s16 triggerX;   // Disparo: borde derecho de cámara supera este X de mundo
    s8  side;       // +1 = de FRENTE (entra por la derecha) | -1 = por la ESPALDA
    s16 y;          // Lane de spawn (pies), distinta dentro de la oleada
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
    u8          type;         // ENEMY_TYPE_FOOT_SOLDIER o ENEMY_TYPE_FOOT_SOLDIER_ORANGE
    u8          anim;         // Animación actual (evita re-setear la misma anim)
    u8          attackType;   // ENEMY_ATTACK_PUNCH / KICK / FRONT / SHURIKEN
    u8          attackHit;    // 1 = este ataque ya conectó (un golpe por swing)
    u8          hitToggle;    // alterna las 3 anims de golpe recibido (0/1/2)
    u8          attackCooldown; // Frames hasta poder volver a atacar (0 = listo)
    u8          target;       // Jugador asignado: 0 = P1, 1 = P2
    u8          retargetTimer; // Frames hasta la próxima re-evaluación de target
    u8          flankTimer;   // Morado: frames intentando flanquear (fallback por timeout)

    // --- Animaciones nuevas del morado (31/07) ---
    s16         w;            // Ancho del frame (px, según el tipo)
    s16         h;            // Alto del frame (px, según el tipo)
    s16         footOffset;   // Pies dentro del frame (px, según el tipo)
    s8          lastMoveDir;  // Última dirección horizontal de movimiento (+1/-1/0)
    u8          turnTimer;    // Frames restantes del giro (ENEMY_STATE_TURN)
    u8          somersault;   // 1 = spawn entrando con voltereta (anim 15)
    u8          grabTarget;   // Jugador agarrado (0/1) durante ENEMY_STATE_GRAB
    Player*     grabbed;      // Puntero al jugador agarrado (liberado en damageEnemy)
    u8          grabTimer;    // Tope de seguridad del agarre (frames restantes)
    u8          stancePhase;  // Frames en la postura de espera actual (IDLE/STANCE)
    u8          stanceToggle; // 0 = IDLE, 1 = STANCE (alterna cada STANCE_SWITCH)

    // --- Combos del morado (31/07, fiel al arcade ATTACK S0/S1/S2) ---
    // Al atacar, el morado entra a ATTACK con un combo de N golpes; cada paso
    // tiene su propia anim, duración y ventana de hitbox (ver ComboStep en
    // enemy.c). comboLen > 0 activa el camino de combo; el naranja lo deja en
    // 0 y usa el ataque simple de siempre.
    u8          comboStep;    // Índice del golpe actual dentro del combo (0 = primero)
    u8          comboLen;     // Golpes del combo actual (0 = ataque simple, sin combo)
} Enemy;

// --- Shuriken (proyectil del foot soldier naranja) ---
typedef struct {
    Sprite*     sprite;
    s16         x;           // X de mundo
    s16         y;           // Y de mundo (pies)
    s8          dir;         // +1 derecha, -1 izquierda
    s16         cameraOffsetX;
    u8          active;      // 1 = en vuelo, 0 = inactivo
} Shuriken;

// Resetea el estado global de la IA (contador de atacantes simultáneos y
// reparto de targets) e informa cuántos jugadores hay (1 o 2).
// Llamar UNA VEZ al iniciar cada nivel, antes del primer spawn.
void resetEnemyAI(u8 numPlayers);

// Separación de grupo: empuja de a 1px a los pares de enemigos (en PATROL o
// CHASE) que estén encimados, para que no se apilen en el mismo lugar.
// Llamar una vez por frame ANTES de los updateEnemy.
void separateEnemies(Enemy* list, u16 count);

void initEnemySpawn(Enemy* e, s16 spawnX, s16 y, s16 patrolRange, u8 palette, u8 type);

// Spawnea un foot soldier ROMPIENDO una puerta: aparece centrado en el hueco
// (doorCenterX) y en la lane del fondo, reproduciendo ANIM_BREAK_DOOR desde el
// 2do frame. Al terminar la animación pasa a CHASE (enemigo normal).
void initEnemyDoorSpawn(Enemy* e, s16 doorCenterX, u8 palette);

// Igual que initEnemyDoorSpawn pero para los ascensores: la puerta ya se abrió
// con su propia animación, así que el foot soldier sólo reproduce los 2 últimos
// frames de ANIM_BREAK_DOOR (índices 3-4) — "sale" del hueco sin romper nada.
void initEnemyElevatorSpawn(Enemy* e, s16 doorCenterX, u8 palette);

// Spawnea un foot soldier con PATADA (kick) desde fuera de pantalla: aparece
// off-screen y se desplaza hacia la pantalla durante ENEMY_KICK_LUNGE frames.
// dir: +1 entra desde la izquierda, -1 desde la derecha.
void initEnemyKickSpawn(Enemy* e, s16 spawnX, s16 y, s8 dir, u8 palette, u8 type);

// Spawnea un foot soldier morado entrando con VOLTERETA (anim 15) desde fuera
// de pantalla: se desplaza más rápido en X durante ENEMY_SOMERSAULT_TIME frames
// y luego pasa a CHASE. Usada para las oleadas "por la espalda".
void initEnemySomersaultSpawn(Enemy* e, s16 spawnX, s16 y, s8 dir, u8 palette, u8 type);

// Los Player* se usan para el AGARRE por la espalda: el morado pone al jugador
// en STATE_GRABBED (playerFootGrab) y lo suelta al zafarse (mash), ser golpeado
// o si le pegan al soldier (damageEnemy lo saca de GRAB).
void updateEnemy(Enemy* e, Player* player1, Player* player2, bool twoPlayers);
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

// ---------------------------------------------------------------------------
// Sistema de shurikens (proyectiles del foot soldier naranja)
// ---------------------------------------------------------------------------
void shurikenInit(void);
void shurikenSpawn(s16 x, s16 y, s8 dir, u8 palette);
void shurikenUpdate(s16 camX);
void shurikenReleaseAll(void);
// Chequea colisión de todos los shurikens activos contra un jugador en (px, py)
// (centro del frame). Devuelve TRUE si alguno impactó (una sola vez por shuriken).
bool shurikenCheckHitPlayer(s16 px, s16 py, s16* hitX);

// Rompe los shurikens activos alcanzados por la hitbox del ataque del jugador
// (playerAttackHits: misma geometría que contra los enemigos). El proyectil
// desaparece sin dañar al jugador. Devuelve TRUE si rompió alguno. Llamar por
// jugador y ANTES de shurikenCheckHitPlayer.
bool shurikenBreakByPlayerAttack(const Player* p);

#endif
