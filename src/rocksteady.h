#ifndef _ROCKSTEADY_H_
#define _ROCKSTEADY_H_

#include <genesis.h>
#include "level2.h"      // rocksteady_boss, boss_bullet (SPRITE)
#include "player.h"      // el jefe interactúa con el/los jugador(es)
#include "boss_flash.h"  // (03/10) parpadeo de vida baja por linea de sprite

// ===========================================================================
// ROCKSTEADY — jefe final del nivel 2 (pasillo en llamas)
// ===========================================================================
// Enemigo único con máquina de estados propia, patrón de robot.c. Aparece
// saliendo de la CÁPSULA del taladro que emerge del piso del fondo de la sala
// (sprite en scenes.c) y pelea en el arena (cámara bloqueada en
// LEVEL2_CAM_MAX_X).
//
// COMPORTAMIENTO (rehecho el 13/09 a pedido de Gustavo)
// Ya NO hay una progresión de dos fases (primero sin arma, después con arma
// para siempre): ahora ALTERNA los dos modos cada ROCKSTEADY_ATTACKS_PER_SWAP
// ataques completados, y guarda/saca el arma con la anim [5].
//
//   SIEMPRE          busca ponerse en el EJE Y del jugador (con 2 jugadores
//                    elige el más cercano en X y lo sigue).
//   CON ARMA         si el jugador está alineado en Y y en rango, dispara el
//                    tiro HORIZONTAL (bala frame [0]); si el jugador está
//                    SALTANDO, dispara el tiro HACIA ARRIBA (bala frame [1]).
//                    En melee, patada con el arma [8] como contraataque.
//   SIN ARMA         si está LEJOS, embestida [2], que golpea por CONTACTO
//                    real de los cuerpos. De cerca se planta y contraataca
//                    con la patada [3].
//
// Cada ROCKSTEADY_KD_INTERVAL golpes recibidos SIN el arma, cae (knock-down,
// anim [4]) y se levanta. Con el arma no cae: la anim de caída lo dibuja
// desarmado y quedaría fuera de lugar.
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
// OJO: r->x ancla el BORDE IZQUIERDO del frame en pantalla (SPR_setPosition
// esquina superior izquierda, sin la compensación de robot.c); el CENTRO
// visual del cuerpo es r->x + FRAME_W/2 (lo devuelve rocksteadyGetCenterX).
// r->y son los PIES (lane).
#define ROCKSTEADY_FRAME_W      104
#define ROCKSTEADY_FRAME_H      104
// (13/09) 104, no 96. El arte del jefe llega HASTA ABAJO DE TODO de la celda:
// medido frame por frame, la ultima fila con pixeles es la 104 en idle, walk,
// aim y shoot. Con 96 se lo dibujaba 8px mas abajo que a la tortuga estando los
// dos en la MISMA lane -- que es justo lo que se ve en la captura que mando
// Gustavo. La tortuga si tiene los pies en 96 (PLAYER_FOOT_OFFSET).
#define ROCKSTEADY_FOOT_OFFSET  104   // Pies en el borde inferior del frame

// --- Vida y daño ---
#define ROCKSTEADY_HP           55   // Barras totales. (01/10) Igual que Bebop (BEBOP_HP), a
                                     // pedido de Gustavo. Historial: 48 -> 52 -> 57 -> 62 (30/08)
                                     // -> 124 (13/09) -> 55. Un golpe normal saca 1 barra y el
                                     // especial ROCKSTEADY_SPECIAL_DMG.
#define ROCKSTEADY_SPECIAL_DMG   3   // Daño del ataque especial (botón A / B+C)
// ALTERNANCIA DEL ARMA (13/09): ya no hay progresion de fases. El jefe cuenta
// los ataques que COMPLETA (una rafaga, una embestida o una patada) y cada
// ROCKSTEADY_ATTACKS_PER_SWAP guarda o saca el arma. Se cuentan ataques y no
// tiempo a propósito: si el jugador se esconde, el jefe no cambia de modo solo
// -- el ritmo lo marca la pelea, no el reloj.
#define ROCKSTEADY_ATTACKS_PER_SWAP 3
// (01/10, pedido de Gustavo) La tanda SIN arma es mas corta: pasaba mucho
// tiempo de la pelea desarmado (la rotacion sin arma tiene un paso de
// "esperar" que no cuenta como ataque, mas el acercamiento de la patada).
// Ahora con UN ataque completado sin arma (la embestida o la patada) ya saca
// el arma; CON arma sigue haciendo ROCKSTEADY_ATTACKS_PER_SWAP rafagas.
#define ROCKSTEADY_UNARMED_ATTACKS  1

// --- Movimiento / patrulla ---
// Arena: cámara bloqueada en LEVEL2_CAM_MAX_X (120) → mundo visible 120..440.
#define ROCKSTEADY_SPEED         2   // px/frame al caminar/alinear lane
#define ROCKSTEADY_CHARGE_SPEED  5   // px/frame de la estampida (01/10: 6 -> 5)
// (01/10) Antes de correr se queda en el lugar con la anim de la embestida
// estos frames (amaga, mirando al jugador): le da al jugador tiempo de leerla
// y salir de la lane. La direccion se latchea al terminar la preparacion.
#define ROCKSTEADY_CHARGE_WINDUP 30
#define ROCKSTEADY_LANE_TOP    142
#define ROCKSTEADY_LANE_BOTTOM 196
#define ROCKSTEADY_PATROL_LEFT 150   // centro del cuerpo, extremo izquierdo
#define ROCKSTEADY_PATROL_RIGHT 330  // centro del cuerpo, extremo derecho
#define ROCKSTEADY_TALADRO_X   340   // X de mundo de la cápsula del taladro (por donde emerge)
#define ROCKSTEADY_SPAWN_X     (ROCKSTEADY_TALADRO_X - 64)   // Aparece 8 tiles (64px) a la izquierda de la cápsula
// Hurtbox del CUERPO del jefe (media anchura desde el centro): los golpes de
// la tortuga conectan por SOLAPE de cajas (playerAttackHitsBox), igual que
// contra los foot soldiers, SIN depender del facing del jefe — de frente hay
// que llegar al contacto real de los sprites y por la espalda se pega igual.
// 20px ≈ la hurtbox del foot soldier naranja: puño/nunchaku/sai piden contacto,
// katana/bō llegan ~20px antes (el largo del arma).
#define ROCKSTEADY_BODY_HALF_W  20
// Alto del cuerpo sobre los pies, medido sobre el arte con la linea de pies
// corregida (104): idle 74, walk 65-71. Lo usa playerAttackHitsBox para validar
// la altura del golpe.
#define ROCKSTEADY_BODY_H       70

// --- Introducción del jefe (cápsula del taladro) ---
#define ROCKSTEADY_EMERGE_STAND 170  // Frames quieto en la puerta (≈ duración de say_your_p) antes de bajar al arena

// --- Ataques (distancias medidas desde el CENTRO VISUAL del cuerpo,
//     r->x + FRAME_W/2) ---
// Rango de CONTACTO centro-a-centro: hasta acá camina y se planta el jefe, y
// es el umbral "en alcance" del timer anti-camping. La distancia real de
// combos es 55-80px (alcance del arma + hurtbox del jefe).
#define ROCKSTEADY_KICK_RANGE       64
// (13/09) La embestida ya NO usa un radio fijo: golpea por SOLAPE REAL de los
// cuerpos (ROCKSTEADY_BODY_HALF_W + PLAYER_BODY_HALF_W = 42), que es lo que
// pidio Gustavo -- "la embestida golpea al player si lo toca". Los 56px de
// antes pegaban bastante antes del contacto visual.
// Ciclo de acercamiento tipo arcade (ver informe de reverse engineering,
// seccion 2): en vez de depender solo del timer anti-camping para embestir,
// el jefe elige EMBESTIR directo (en vez de caminar) apenas la distancia es
// realmente grande; la caminata (ROCKSTEADY_APPROACH) queda para cerrar el
// resto del hueco corto. Tambien se usa para garantizar el primer golpe del
// combate al bajar de la capsula (ver ROCKSTEADY_EMERGE en rocksteady.c).
#define ROCKSTEADY_CHARGE_TRIGGER_DIST 110
// La PATADA ya NO es una decisión espontánea: SOLO sale como contraataque
// cuando recibe ROCKSTEADY_COUNTER_HITS golpes seguidos.
#define ROCKSTEADY_COUNTER_HITS      3   // (01/10: 2 -> 3)
// (01/10, pedido de Gustavo) La patada se spammeaba: con 2 golpes seguidos ya
// contraatacaba, o sea que cortaba CADA combo. Ahora, despues de cualquier
// patada (contra o de la rotacion), no vuelve a patear durante estos frames:
// el contraataque que caiga en ese lapso se descarta (solo flinchea) y el paso
// "patear" de la rotacion sin arma cede el turno.
#define ROCKSTEADY_KICK_COOLDOWN   180
// Anti-camping: jugador fuera de alcance durante estos frames (~2 s) seguidos
// mientras el jefe está neutral → EMBESTIDA.
#define ROCKSTEADY_FAR_FRAMES      120
#define ROCKSTEADY_SHOOT_RANGE    130   // distX para abrir el disparo (fase 2)
#define ROCKSTEADY_SHOOT_ALIGN_Y    6   // |dy| máx con el jugador para disparar
#define ROCKSTEADY_HIT_TOL_Y    25   // |dy| máx (pies) para conectar ataques
#define ROCKSTEADY_ATTACK_COOLDOWN 40
#define ROCKSTEADY_HURT_FRAMES  14   // Flinch tras un golpe normal
// (13/09) SE QUITO el flash BLANCO al recibir daño: reescribia los indices
// 2..15 de PAL3 durante 8 frames en cada golpe y Gustavo lo pidio sacar. Con
// ROCKSTEADY_HP en 124 son ~124 destellos por pelea. OJO: el parpadeo por HP
// BAJO sigue -- ese es OTRO efecto, vive en scenes.c (bossPal/flashPal) y es
// un aviso de vida critica, no una reaccion al golpe.
#define ROCKSTEADY_KD_INTERVAL  10   // Golpes recibidos entre knock-downs (sin arma)
#define ROCKSTEADY_KD_HOLD      60   // Frames que queda tirado en el knock-down
#define ROCKSTEADY_IDLE_MIN     30   // Quieto mínimo antes de atacar

// --- Rotacion del jefe SIN ARMA (14/09, pedido de Gustavo) -----------------
// Antes, sin arma solo tenia dos salidas y ninguna cubria el caso "el jugador
// esta encima": lejos -> embestida, medio -> caminar hasta contacto, cerca ->
// NADA. Con el jugador pegado se quedaba plantado para siempre, porque la
// patada era unicamente el contraataque de ROCKSTEADY_COUNTER_HITS.
// Ahora rota entre TRES conductas, una por decision:
//   0 ESPERAR    se queda en el lugar unos frames (le da aire al jugador y
//                deja que el contraataque siga existiendo)
//   1 EMBESTIDA  si hay pista suficiente (ver ROCKSTEADY_CHARGE_MIN_DIST)
//   2 PATADA     camina hasta el rango y patea de verdad, no como contra
// Se rota en vez de sortear para garantizar variedad: sorteando, tres
// "esperar" seguidos se ven igual que el bug que se esta arreglando.
#define ROCKSTEADY_UNARMED_STEPS   3
// Espera del paso 0: base + hasta 31 frames de azar, para que no quede
// metronomico.
#define ROCKSTEADY_WAIT_FRAMES    45
// Pista minima para que la embestida se lea como embestida. Por debajo de esto
// el paso 1 cede el turno al paso 2 (patada), que es lo que corresponde de
// cerca.
#define ROCKSTEADY_CHARGE_MIN_DIST 80
#define ROCKSTEADY_CHARGE_MAX   80   // Tope de frames de la estampida
#define ROCKSTEADY_CHARGE_OVER  18   // Frames que sigue la estampida tras impactar (overshoot)

// --- Balas del disparo ---
#define MAX_ROCKSTEADY_BULLETS   6   // Proyectiles simultáneos en vuelo
#define ROCKSTEADY_BULLET_SPEED  3   // px/frame horizontal
#define ROCKSTEADY_BULLET_DMG    1   // Barras de vida al impactar
#define ROCKSTEADY_CHARGE_DMG    4   // Barras de vida al conectar la embestida (30/08, a pedido de Gustavo)
#define ROCKSTEADY_SHOT_COUNT    3   // Balas por ráfaga
#define ROCKSTEADY_SHOT_TICKS    5   // Ticks entre frames de la anim de disparo
// --- Sub-rangos de la anim [9] (8 frames) segun hacia donde dispara --------
// La fila NO es una secuencia sola: son DOS poses de dos frames cada una, con
// sus frames de retroceso en el medio. Medido sobre el arte:
//   [0][1] horizontal CON fogonazo   [2][3] horizontal, arma retrocedida
//   [4][5] arma arriba, sin fogonazo [6][7] arma arriba CON fogonazo
// Asi que el disparo horizontal usa 0-1 y el de arriba 6-7 (13/09, Gustavo).
// NO hizo falta partir la fila en dos animaciones del sheet: el estado SHOOT
// ya maneja los frames a mano con SPR_setFrame, asi que alcanza con arrancar
// en la base que corresponda.
#define ROCKSTEADY_SHOOT_FR_H    0   // primer frame del par horizontal
#define ROCKSTEADY_SHOOT_FR_UP   6   // primer frame del par hacia arriba
// Disparo hacia arriba: sale cuando el jugador esta SALTANDO. Sube 3px por
// frame de ALTURA (no de lane), asi que en ~19 frames pasa por la altura del
// apex del salto de la tortuga (107 + torso).
#define ROCKSTEADY_UPSHOT_DZ  3

// --- BOCA DEL CAÑON (13/09) -------------------------------------------------
// Las balas salian a una altura fija (40px bajo el tope del frame), que caia a
// la altura de la BOCA de Rocksteady y no del arma. Estos offsets estan
// MEDIDOS sobre el fogonazo del sheet, buscando la mancha de color 4/11 mas
// alejada del cuerpo en cada pose:
//   anim [9] frame 0 (horizontal): fogonazo centrado en la celda (92, 59)
//   anim [9] frame 6 (hacia arriba): centrado en la celda (67, 17)
// La celda es de 104x104 y los pies del jefe estan a ROCKSTEADY_FOOT_OFFSET
// (104, o sea el borde inferior) del tope, asi que:
//   X = columna - 52 (centro del frame), con el signo de r->dir
//   Z = 104 - fila   (altura sobre los PIES; 104 = ROCKSTEADY_FOOT_OFFSET)
#define ROCKSTEADY_MUZZLE_H_X   40   // 92 - 52
#define ROCKSTEADY_MUZZLE_H_Z   45   // 104 - 59
#define ROCKSTEADY_MUZZLE_UP_X  15   // 67 - 52
#define ROCKSTEADY_MUZZLE_UP_Z  87   // 104 - 17
// --- Frames del sprite boss_bullet (una fila de 3, sin auto-animacion) ---
#define ROCKSTEADY_BULLET_FR_H    0   // tiro horizontal
#define ROCKSTEADY_BULLET_FR_UP   1   // tiro hacia arriba (diagonal)
#define ROCKSTEADY_BULLET_FR_HIT  2   // impacto contra el jugador
// Al pegar, la bala NO se borra en el acto: se queda quieta mostrando el frame
// de impacto estos frames y recien ahi se libera el sprite.
#define ROCKSTEADY_BULLET_HIT_FRAMES 16
// La bala guarda su ALTURA SOBRE EL PISO en 'z' (misma idea que el jumpZ del
// jugador) y NO en la lane: la lane es PROFUNDIDAD, y moverla haria que la bala
// se fuera al fondo de la sala en vez de subir por pantalla.
// Arranca en la altura de la boca del cañon (ver ROCKSTEADY_MUZZLE_*_Z) y sube
// dz por frame; se apaga al salirse de la pantalla por arriba.
#define ROCKSTEADY_BULLET_MAX_Z    160
// Altura del TORSO del jugador sobre sus pies: es contra esto (mas su jumpZ) que
// se compara la altura de la bala. Con esto, el tiro recto (z 37) le pega al que
// esta parado y NO al que salta, y el de arriba al reves -- que es justo la
// gracia de tener dos tiros.
#define ROCKSTEADY_BULLET_TARGET_Z  30
#define ROCKSTEADY_BULLET_HIT_Z     24
// Tolerancia SEPARADA para el tiro hacia arriba (14/09). El antiaereo sube 3px
// por frame y avanza otros 3, asi que cuando llega al jugador ya va por
// z~120-130: con los 24 del tiro recto solo conecta si la tortuga esta muy
// cerca del apex en ese frame exacto. MEDIDO en emulador, ~30s saltando sin
// parar: con 24 conecto 1 de 3 impactos; con 48, 2 de 4. Se deja 48 porque la
// bala llega alta y el antiaereo tiene que castigar la mitad de ARRIBA del
// salto, no solo el apex. El tiro recto se queda en 24: si se le ampliara,
// le pegaria tambien al que salta y los dos tiros harian lo mismo.
#define ROCKSTEADY_BULLET_HIT_Z_UP  48

typedef enum {
    ROCKSTEADY_INACTIVE,    // Todavía no apareció
    ROCKSTEADY_EMERGE,      // Quieto en la puerta de la cápsula (IDLE), luego BAJA al arena caminando (WALK [1])
    ROCKSTEADY_IDLE,        // Quieto (alineando lane), decide el próximo ataque
    ROCKSTEADY_APPROACH,    // Camina hacia el jugador (sin arma)
    ROCKSTEADY_CHARGE,      // Embestida contra el jugador (sin arma)
    ROCKSTEADY_KICK,        // Patada melee (sin arma)
    ROCKSTEADY_HURT,        // Flinch al recibir golpe (sin arma)
    ROCKSTEADY_KNOCKDOWN,   // Cayó (anim [4] hasta el suelo), se levanta
    ROCKSTEADY_ARMS_INTRO,  // Saca O guarda el arma (anim [5]) → cambia de modo
    ROCKSTEADY_AIM_WALK,    // Se acerca apuntando y alineando lane (con arma)
    ROCKSTEADY_SHOOT,       // Ráfaga de balas (con arma)
    ROCKSTEADY_KICK_ARMS,   // Patada con el arma
    ROCKSTEADY_HURT_ARMS,   // Flinch al recibir golpe (con arma)
    ROCKSTEADY_DEAD,        // Cayendo (anim [4] hasta el final)
    ROCKSTEADY_GONE         // Muerto y removido
} RocksteadyState;

typedef struct {
    Sprite*     sprite;
    RocksteadyState state;
    u8          armed;       // 0 = sin arma (embestida/patada) · 1 = con arma
                             // (dispara). Alterna cada ROCKSTEADY_ATTACKS_PER_SWAP
                             // ataques completados; ya NO es una progresión.
    s16         x;           // X de MUNDO del BORDE IZQUIERDO del frame
                              // (centro visual = x + FRAME_W/2, ver GetCenterX)
    s16         y;           // Y = PIES (lane)
    s16         cameraOffsetX;
    s8          dir;         // -1 mira izquierda / +1 mira derecha
    s16         hp;
    u8          anim;        // Anim actual (evita re-setear)
    u16         timer;       // Timer genérico del estado (golpes/KD/idle)
    u8          attackCooldown;
    u8          hitsTaken;   // Golpes recibidos desde el último knock-down
    u8          knockdowns;  // Total de caídas en fase 1
    u8          comboHits;     // Golpes seguidos sin poder responder (contraataque)
    u8          counterPending; // 1 = al próximo flinch suelta la patada counter
    u16         farTimer;      // Frames seguidos con el jugador fuera de alcance (anti-camping)
    u8          attacksDone;   // Ataques COMPLETADOS desde el último cambio de
                               // arma (ver ROCKSTEADY_ATTACKS_PER_SWAP)
    u8          chargeHit;   // 1 = la estampida ya impactó en esta carga (overshoot sin re-dañar)
    u8          chargeWind;  // Frames de preparacion que le quedan a la embestida
                             // (ROCKSTEADY_CHARGE_WINDUP): quieto, sin dañar.
    u8          unarmedStep; // Paso de la rotacion SIN ARMA (ver
                             // ROCKSTEADY_UNARMED_STEPS): 0 esperar,
                             // 1 embestida, 2 acercarse y patear.
    u8          kickOnArrive;// 1 = el APPROACH en curso termina en patada.
    u8          kickCooldown;// Frames hasta poder volver a patear (ROCKSTEADY_KICK_COOLDOWN)
    s8          chargeDir;   // Dirección LATCHEADA de la embestida (14/09). Se
                             // fija al arrancar y no se re-apunta: una vez
                             // lanzada la corrida, se esquiva.
    // Ráfaga de disparos (control manual de frames de la anim [9]).
    u8          shotFrame;   // Paso dentro de la ráfaga (2 por bala: fogonazo
                             // + segundo frame del par)
    u8          shotsFired;  // Balas disparadas en esta ráfaga
    u8          shotTimer;   // Ticks hasta el próximo paso de frame
    u8          shotUp;      // 1 = TODA esta ráfaga es el tiro hacia arriba.
                             // Se decide UNA vez al abrir fuego, no por bala:
                             // si no, la pose y la bala podian contradecirse.
    BossFlash   flash;       // (03/10) parpadeo de vida baja (arena.flashPal)
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

// --- Balas del disparo (misma estructura que el sistema de shurikens) ---
void rocksteadyBulletInit(void);
void rocksteadyBulletUpdate(s16 camX);
void rocksteadyBulletReleaseAll(void);
// Chequea colisión de todas las balas activas contra un jugador en (px, py)
// (centro del frame). Devuelve TRUE si alguna impactó (una vez por bala).
bool rocksteadyBulletCheckHitPlayer(s16 px, s16 py, s16 pz, s16* hitX);

#endif
