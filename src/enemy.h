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
#define ENEMY_TYPE_FOOT_SOLDIER_WHITE  2   // foot soldier blanco, espada larga (PAL3)
#define ENEMY_TYPE_FOOT_SOLDIER_YELLOW 3   // (29/09) foot soldier amarillo, boomerang (PAL2)
#define ENEMY_TYPE_FOOT_SOLDIER_GUN    4   // (06/10) naranja del FUSIL (PAL2)
#define ENEMY_TYPE_FOOT_SOLDIER_HAMMER 5   // (06/10) morado del MARTILLO (PAL2)
#define ENEMY_TYPE_FOOT_SOLDIER_SPEAR  6   // (06/10) morado de la LANZA (PAL2)

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
// El blanco comparte la grilla del naranja (104x104) pero su arte esta pegado
// mas abajo dentro de la celda: el pixel mas bajo de los frames de caminata
// esta en y=103 (el naranja los tiene en ~97), asi que los pies van 6px mas
// abajo o el sprite queda flotando.
#define ENEMY_SPRITE_W_WHITE   104
#define ENEMY_SPRITE_H_WHITE   104
#define ENEMY_FOOT_OFFSET_WHITE 102
#define ENEMY_HP            4    // Daño que mata a CUALQUIER foot soldier de un
                                 // golpe (especial de la tortuga, bola de hierro)
// ---------------------------------------------------------------------------
// Hurtbox del CUERPO por tipo (media anchura en px desde el centro del frame).
// Patrón colbox del manual SGDK: el golpe conecta contra el cuerpo visible,
// no contra el borde transparente del frame. El morado es esbelto (frame 64px,
// cuerpo ~32px); el naranja es corpulento pero su frame de 104px tiene mucho
// aire (cuerpo ~40px).
// ---------------------------------------------------------------------------
// Alto del CUERPO sobre los pies, medido sobre el arte con
// tools/gen_player_hitbox.py (mediana de todos los frames). Lo usa
// playerAttackHitsBox para validar tambien la ALTURA del golpe: sin esto, una
// patada en salto conecta con la tortuga por encima de la cabeza del soldier.
#define ENEMY_BODY_H_PURPLE        60
#define ENEMY_BODY_H_ORANGE        58
// Blanco: del tope de la capucha (y=40 en los frames de walk) a los pies (102).
// La espada NO cuenta: es arte que sale del cuerpo, no cuerpo.
#define ENEMY_BODY_H_WHITE         62

#define ENEMY_BODY_HALF_W_PURPLE   13   // (02/10) era 16: ancho de la hurtbox nueva (ver abajo)
// (02/10) HURTBOX NUEVA del morado, la marcada en
// Pruebas_TMNT_Control: cabeza y torso, SIN las piernas, un poco corrida hacia
// adelante. Desde los PIES, mirando a la DERECHA (se espeja con e->dir):
// X -11 (atras) .. +14 (adelante), Y -63 .. -28. La usa playerAttackHitsEnemy.
#define ENEMY_HURT_BACK_PURPLE    11
#define ENEMY_HURT_FRONT_PURPLE   14
#define ENEMY_HURT_TOP_PURPLE    -63
#define ENEMY_HURT_BOT_PURPLE    -28
#define ENEMY_BODY_HALF_W_ORANGE   20
// Medido sobre torso+piernas (franja y=60..100, que no toca la espada):
// el cuerpo va de -10 a +30 respecto del centro del frame -> 40px de ancho.
#define ENEMY_BODY_HALF_W_WHITE    20

// Vida maxima POR TIPO (max_health de la tabla de tipos del manual SGDK):
// initEnemySpawn la asigna al spawnear. Hoy ambas mueren con los mismos
// golpes (4), pero cada tipo puede ajustarse sin tocar el resto del codigo.
#define ENEMY_HP_PURPLE    4    // Golpes para eliminar al morado
#define ENEMY_HP_ORANGE    4    // Golpes para eliminar al naranja
#define ENEMY_HP_WHITE     6    // El blanco aguanta mas: aparece recien antes
                                // de Rocksteady y tiene espada larga (13/09)
#define ENEMY_HP_YELLOW    4    // (29/09) El del boomerang: como el morado
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
#define ENEMY_ANIM_TNT        16   // (18/09) Tirar DINAMITA (13f): se asoma, se
                                   // planta, tira el cartucho y se recompone.
                                   // El TNT sale en el frame 11 (ver
                                   // ENEMY_TNT_RELEASE_TIMER).
#define ENEMY_ANIM_MANHOLE    17   // (18/09) Salir por la ALCANTARILLA (6f).
                                   // (19/09) EN USO en el 2-1: el morado sale
                                   // de la boca de tormenta y tira la TAPA.
                                   // f0 = solo la tapa en el piso (el soldier
                                   // todavia esta abajo), f1-f3 la levanta,
                                   // f4 la tira (lineas de movimiento, ya sin
                                   // tapa), f5 se recompone.

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
// Animaciones del foot soldier BLANCO (orden de filas en el sheet de espada).
// Otro orden mas, distinto del morado Y del naranja.
// ---------------------------------------------------------------------------
#define WHITE_ANIM_IDLE         0   // Quieto, espada al frente (1f)
#define WHITE_ANIM_WALK         1   // Caminar (5f)
#define WHITE_ANIM_WALK_UP      2   // Caminar hacia arriba de la pantalla (8f)
#define WHITE_ANIM_SLASH_LONG   3   // Espadazo LARGO: estocada a fondo (3f)
#define WHITE_ANIM_SLASH_MID    4   // Espadazo de alcance medio (3f)
#define WHITE_ANIM_SLASH_MID2   5   // Variante del medio, corte descendente (3f)
#define WHITE_ANIM_JUMP         6   // Salto: despegue + giro tipo bolita (5f)
#define WHITE_ANIM_AIR_SLASH    7   // Espadazo cayendo desde el aire (2f)
#define WHITE_ANIM_HIT          8   // Golpe recibido (2f)
#define WHITE_ANIM_EXPLODE      9   // Muerte: cae al piso y explota (4f)

// ---------------------------------------------------------------------------
// Foot soldier AMARILLO -- el del BOOMERANG (29/09)
// ---------------------------------------------------------------------------
// Sheet Foot_Soldier_Yellow_boomerang.png: frames de 64x80 (8x10 tiles, la
// misma grilla que el morado, pies en el borde de abajo), arte mirando a la
// DERECHA. Usa PAL2 con la paleta del MORADO: el PNG tiene el mismo orden de
// indices (el mismo rip) y cargar la suya cambiaria un poco los colores de los
// morados que esten en pantalla (ver initEnemySpawn).
#define YELLOW_ANIM_IDLE        0   // Quieto (1f)
#define YELLOW_ANIM_THROW       1   // Lanzamiento del boomerang (9f)
#define YELLOW_ANIM_GUARD       2   // Guardia (1f): esperando que vuelva el
                                    // boomerang
#define YELLOW_ANIM_WALK        3   // Caminar de frente (5f)
#define YELLOW_ANIM_WALK_UP     4   // Caminar hacia arriba (4f)
#define YELLOW_ANIM_EXPLODE     5   // Muerte (6f)
#define YELLOW_ANIM_HIT_1       6   // (29/09) Golpe recibido, 1f cada una: se
#define YELLOW_ANIM_HIT_2       7   // alternan 6/7/8 en cada golpe, igual que
#define YELLOW_ANIM_HIT_3       8   // las tres del morado (hitToggle)

#define ENEMY_SPRITE_W_YELLOW   64
#define ENEMY_SPRITE_H_YELLOW   80
#define ENEMY_FOOT_OFFSET_YELLOW 80  // pixel mas bajo en y=79 en todas las filas
// Cuerpo medido sobre el arte (idle y walk): x 18..43 del frame, tope y=17.
#define ENEMY_BODY_HALF_W_YELLOW 14
#define ENEMY_BODY_H_YELLOW      60

// El sheet va a FAST 6 (el lanzamiento es rapido en el arcade; a 8 ticks los
// 9 frames duraban 1,2 s).
#define YELLOW_TICKS             6
#define YELLOW_THROW_TIME       54   // 9 frames x 6
#define YELLOW_EXPLODE_TIME     36   // 6 frames x 6
// El boomerang se ve en la mano en los frames 2..5 y ya no esta en el 6 (el
// brazo estirado hacia adelante): sale al EMPEZAR el frame 6. Con el timer
// contando hacia atras: 54 - 6x6 = 18.
#define YELLOW_RELEASE_TIMER    18
// Atrapada: el frame 4 del lanzamiento (indice 3, boomerang en la mano en
// alto) y de ahi para atras hasta el 0, 6 ticks cada uno. Frames puestos a
// mano (SPR_setAutoAnimation(FALSE)), la anim no corre en reversa sola.
#define YELLOW_CATCH_FRAME0      3
#define YELLOW_CATCH_TIME       ((YELLOW_CATCH_FRAME0 + 1) * YELLOW_TICKS)   // 24

// Distancia (X, centro a centro) a la que tira. Por debajo de MIN se aleja
// para tomar distancia (no tiene golpes cuerpo a cuerpo: su arma es el
// boomerang); por encima de MAX se acerca.
#define YELLOW_RANGE_MIN        56
#define YELLOW_RANGE_MAX       140
#define YELLOW_ALIGN_Y           8   // |dy| maximo para tirar (vuela en X)
// Cada amarillo se planta a una distancia PROPIA (YELLOW_RANGE_MAX menos un
// sorteo de 0..YELLOW_RANGE_JITTER-1, guardado en flankTimer, que el amarillo
// no usa): con la misma distancia los tres quedaban apilados en la misma X.
#define YELLOW_RANGE_JITTER     64
// Espera inicial antes del primer tiro (se suma un sorteo de 0..63): asi los
// que entran juntos no tiran los tres en el mismo frame.
#define YELLOW_FIRST_COOLDOWN   40

// --- El boomerang ---
// Sheet boomerang.png, frames de 32x32 (4x4 tiles): [0] girando (8f),
// [1] pegandole a la tortuga (2f), [2] roto por un golpe de la tortuga (3f).
// Vuela en X a la altura de la mano: sale con BOOM_V0 alejandose y SIEMPRE
// acelera hacia la mano del duenio (BOOM_ACC). O sea que frena, se da vuelta
// a V0^2 / (2 ACC) = 128 px y vuelve solo, aunque el duenio se haya movido.
// Todo en Q8 (256 = 1 px).
#define MAX_BOOMERANGS           3
#define BOOM_MAX_AIR             2   // en el aire a la vez (el tercero espera)
#define BOOM_V0               1536   // 6 px/frame
#define BOOM_ACC                36   // 0,14 px/frame2 -> se da vuelta a 128 px
#define BOOM_LANE_SPEED          1   // px/frame que corrige la lane al volver
#define BOOM_Z                  50   // altura del CENTRO sobre los pies (la mano)
#define BOOM_SPAWN_DX           24   // sale del centro del soldier + dir*24
#define BOOM_HAND_DX            16   // donde lo agarra: centro + dir*16 (frame 3)
#define BOOM_CATCH_X            10   // tolerancia de la atrapada
#define BOOM_CATCH_Y            10
#define BOOM_HIT_X              20   // contra el centro de la tortuga
#define BOOM_HIT_Y              16   // lanes
#define BOOM_LIFE              360   // frames de vuelo como maximo (seguridad)
#define BOOM_IMPACT_TIME        12   // anim [1] (2f x 4) y un poquito quieto
#define BOOM_BROKEN_TIME        14   // anim [2] (3f x 4) y un poquito quieto

// --- Tiempos (el sheet va a FAST 8: 8 ticks por frame) ---
#define WHITE_SLASH_TIME       24   // 3 frames x 8 (vale para las 3 anims 3/4/5)
#define WHITE_AIR_SLASH_TIME   16   // 2 frames x 8
#define WHITE_EXPLODE_TIME     32   // 4 frames x 8
#define WHITE_HIT_TIME         16   // 2 frames x 8

// --- Ventanas de hitbox, en valores del timer que cuenta HACIA ATRAS ---
// Con WHITE_SLASH_TIME 24 y 8 ticks por frame: frame 0 = timer 24..17,
// frame 1 = 16..9, frame 2 = 8..1.
//   [3] estocada larga -> la espada esta extendida en el frame 0
//   [4] y [5]          -> la espada sale en el frame 2 (los dos primeros son
//                         la carga por encima de la cabeza)
#define WHITE_LONG_HIT_START   17
#define WHITE_LONG_HIT_END     24
#define WHITE_MID_HIT_START     1
#define WHITE_MID_HIT_END       8

// --- Alcances, medidos sobre el arte (offset del pixel opaco mas a la
//     derecha respecto del centro del frame de 104px) ---
// [3] la punta de la espada llega a +52 -> es el arma mas larga del juego, y
//     es el sentido de este enemigo: pega desde donde el morado no llega.
// [4] la espada sale horizontal a +41.
// [5] mide bastante menos sobre el arte (+14: es un corte DESCENDENTE, la
//     hoja termina apuntando al piso), pero se lo toma como "igual
//     alcance que el [4]" y en juego barre el mismo frente a la altura del
//     cuerpo, asi que se le deja un valor intermedio en vez del medido.
#define WHITE_SLASH_LONG_REACH 50
#define WHITE_SLASH_MID_REACH  38
#define WHITE_SLASH_MID2_REACH 32

// --- Salto con espadazo (el "kick" del blanco) ---
// (29/09) PUNTO FIJO Q8 (256 = 1 px), igual que el salto de las tortugas
// (ver el bloque "Salto" de player.h): altura en jumpZq y velocidad en
// jumpVel, las dos en Q8; jumpZ queda como la parte entera que se dibuja.
// SUBIDA con la misma velocidad inicial y gravedad que las tortugas (apice de
// 107 px en el frame 15). Al entrar en la zona del apice la gravedad pasa a
// WHITE_FALL_GRAV_Q, la mitad: la CAIDA MAS LENTA del 14/09 (antes era la
// gravedad entera aplicada 1 de cada 2 frames, a los saltos). Simulado: 36
// frames en el aire (antes 36) y ~87 px de avance (antes 86).
#define WHITE_JUMP_V0_Q       3724   // 14,55 px/frame hacia arriba
#define WHITE_JUMP_GRAV_Q      272   // 1,06 px/frame2 subiendo
#define WHITE_JUMP_BAND_Q      408   // por debajo de 1,59 px/frame: apice/caida
#define WHITE_FALL_GRAV_Q      128   // 0,5 px/frame2: apice y caida planeada
#define WHITE_JUMP_SAFETY       88   // frames: tope de seguridad del arco
#define WHITE_JUMP_SPEED         3   // px/frame de avance horizontal en el aire
// Avance horizontal MIENTRAS CAE: mas lento que el de subida para que el
// vuelo total no se estire (la caida ahora dura 21 frames en vez de 14).
// 14*3 + 21*2 = 84 px, practicamente los mismos 84 de antes (28*3).
#define WHITE_FALL_SPEED_X       2
#define WHITE_AIR_SLASH_REACH   30   // hitbox mientras cae con la espada

// Rango en el que el blanco decide atacar: MAS que el generico de 60 porque
// su estocada llega a 50 y si usara el rango comun desperdiciaria el arma.
#define WHITE_ATTACK_RANGE      78
#define WHITE_LONG_RANGE_MIN    40   // por debajo de esto la estocada larga
                                     // pasa de largo: usa los cortes medios
#define WHITE_JUMP_RANGE_MIN    90   // de mas lejos que esto entra saltando
#define WHITE_JUMP_RANGE_MAX   190

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
#define ENEMY_ATTACK_JUMP   4    // salto + espadazo cayendo (solo blanco, anims 6/7)
#define ENEMY_ATTACK_BOOMERANG 5 // tirar el boomerang (solo amarillo, anim 1)
#define ENEMY_ATTACK_CATCH  6    // atrapar el boomerang (amarillo, anim 1 al reves)
#define ENEMY_ATTACK_GUN    7    // (06/10) rafaga del fusil (a distancia)
#define ENEMY_ATTACK_TAUNT  8    // (06/10) burla del fusil (no pega)
#define ENEMY_ATTACK_SPEAR_THROW 9 // (06/10) tira la lanza (a distancia)

// ===========================================================================
// FOOT SOLDIERS CON ARMA (06/10): FUSIL, MARTILLO y LANZA
// ===========================================================================
// Hojas armadas por tools/gen_foot_weapons.py desde las provisorias
// (foot_gun/hammer/spear.png): grilla pareja, arte mirando a la DERECHA, el
// cuerpo centrado en la celda y los pies en el borde de abajo. Las tres usan
// la paleta unica de enemigos (PAL2). Frame time 6 en las tres.
//
// Conducta (la del remaster de PC):
//   FUSIL    se acerca; alineado en la lane y a menos de GUN_SHOOT_RANGE:
//            1/3 rafaga (dos balas que bajan en diagonal hasta el piso),
//            1/3 burla, 1/3 espera. Pegado: culatazo.
//   MARTILLO va derecho a la tortuga y a HAMMER_RANGE o menos, martillazo.
//   LANZA    si tiene la lanza para tirar (la mitad, al azar) y esta en
//            pantalla, la tira; despues sigue como un foot soldier MORADO
//            comun (cambia de hoja). Si no, estocada a SPEAR_RANGE o menos.
//   Ninguno flanquea ni agarra: encaran de frente.
#define FW_TICKS                 6    // frame time de las tres hojas

#define GUN_ANIM_IDLE            0
#define GUN_ANIM_WALK            1    // 7f
#define GUN_ANIM_WALK_UP         2    // 8f
#define GUN_ANIM_SHOOT           3    // 6f: apunta, fogonazos, apunta
#define GUN_ANIM_BUTT            4    // 5f: culatazo (pega en el 4to)
#define GUN_ANIM_HIT             5    // 1f
#define GUN_ANIM_EXPLODE         6    // 3f: golpeado y cae
#define GUN_ANIM_TAUNT           7    // 6f: para el fusil

#define HAMMER_ANIM_IDLE         0
#define HAMMER_ANIM_WALK         1    // 8f
#define HAMMER_ANIM_WALK_UP      2    // 8f
#define HAMMER_ANIM_SWING        3    // 6f: sube, baja, golpea el piso (4to y 5to)
#define HAMMER_ANIM_HIT          4    // 3f
#define HAMMER_ANIM_EXPLODE      5    // 4f: cae

#define SPEAR_ANIM_IDLE          0
#define SPEAR_ANIM_WALK          1    // 8f
#define SPEAR_ANIM_WALK_UP       2    // 8f
#define SPEAR_ANIM_THRUST        3    // 5f: estocada (pega en el 3ro y 4to)
#define SPEAR_ANIM_THROW         4    // 6f: la lanza sale al empezar el 4to
#define SPEAR_ANIM_HIT           5    // 1f
#define SPEAR_ANIM_EXPLODE       6    // 4f: cae (frames de la caida del martillo)

// Celdas (px). Pies en el borde de abajo.
#define ENEMY_SPRITE_W_GUN     120
#define ENEMY_SPRITE_H_GUN      72
#define ENEMY_SPRITE_W_HAMMER  104
#define ENEMY_SPRITE_H_HAMMER   96
#define ENEMY_SPRITE_W_SPEAR   136
#define ENEMY_SPRITE_H_SPEAR   104
// Cuerpo (como el morado: el arma no cuenta como cuerpo).
#define ENEMY_BODY_HALF_W_FW     14
#define ENEMY_BODY_H_GUN         56
#define ENEMY_BODY_H_FW          60

#define ENEMY_HP_GUN             4
#define ENEMY_HP_HAMMER          4
#define ENEMY_HP_SPEAR           4

// Muerte: la caida de la hoja y despues parpadea hasta desaparecer.
#define FW_EXPLODE_TIME         48
#define FW_BLINK_TIME           24

// Distancias de decision (centro a centro, px)
#define GUN_SHOOT_RANGE        100    // rafaga a esto o menos ...
#define GUN_SHOOT_MIN           40    // ... y a mas que esto (pegado: culatazo)
#define GUN_STANDOFF            72    // se planta aca para tirar
#define GUN_ALIGN_Y              6    // |dy| para tirar (la bala va por su lane)
#define HAMMER_RANGE            57
#define HAMMER_STANDOFF         40
#define SPEAR_RANGE             62
#define SPEAR_STANDOFF          48
#define SPEAR_THROW_STANDOFF    90    // el que todavia tiene la lanza para tirar
#define SPEAR_THROW_MIN         50    // tira la lanza a mas que esto ...
#define SPEAR_THROW_SCREEN     100    // ... con el centro a esto o menos del
                                      // centro de la camara
#define SPEAR_ALIGN_Y            6

// Ataques: duracion (frames x FW_TICKS) y ventana del golpe (timer, cuenta
// hacia atras).
#define GUN_SHOOT_TIME          36    // 6f
#define GUN_SHOT1_TIMER         30    // primera bala (2do frame, fogonazo)
#define GUN_SHOT2_TIMER         21    // segunda bala
#define GUN_TAUNT_TIME          36
#define GUN_BUTT_TIME           30    // 5f, pega en el 4to (18..23)
#define GUN_BUTT_HIT_START       7
#define GUN_BUTT_HIT_END        12
#define GUN_BUTT_REACH          44
#define HAMMER_SWING_TIME       36    // 6f, pega en el 4to y 5to (18..29)
#define HAMMER_HIT_START         7
#define HAMMER_HIT_END          18
#define HAMMER_REACH            60
#define SPEAR_THRUST_TIME       30    // 5f, pega en el 3ro y 4to (12..23)
#define SPEAR_HIT_START          7
#define SPEAR_HIT_END           18
#define SPEAR_REACH             64
#define SPEAR_THROW_TIME        36
#define SPEAR_RELEASE_TIMER     18    // sale al empezar el 4to frame

// --- Proyectiles (van en el pool de los shurikens) ---
// La bala del fusil: avanza GUN_BULLET_Q y baja GUN_BULLET_DZQ desde la
// altura del cano hasta el piso, donde salta en chispas (no pega). En vuelo
// le pega a la tortuga de su lane.
#define GUN_MUZZLE_DX           30
#define GUN_MUZZLE_Z            30
#define GUN_BULLET_Q           960    // 3,75 px/f (Q8)
#define GUN_BULLET_DZQ         320    // 1,25 px/f hacia abajo
#define GUN_SPARK_TIME          24    // chispas: 6 frames x 4
// La lanza tirada: recta, a la altura de la mano.
#define SPEAR_PROJ_Q          1600    // 6,25 px/f
#define SPEAR_PROJ_Z            40
#define SPEAR_PROJ_W            96    // celda del sprite
// Lane de la tortuga respecto de la del que tiro: de -5 a +7 (como los jefes).
#define FW_PROJ_DY_UP            5
#define FW_PROJ_DY_DOWN          7

// --- Shuriken (proyectil del foot soldier naranja) ---
#define MAX_SHURIKENS           8   // proyectiles simultáneos en pantalla
                                    // (06/10: 4 -> 8, comparten pool con las
                                    // balas del fusil y la lanza tirada)
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
// (30/09) Gravedad de la patada de entrada saltando de una ventana (2-1):
// 0,25 px/frame2 -> desde 64 px toca el piso en ~23 frames, dentro de los 32
// que dura la patada.
#define ENEMY_WINDOW_FALL_GRAV_Q  64

// Muerte con explosión (anim 5 = 4 frames x 8) y rotura de puerta al spawnear
// (anim 7: se reproduce desde el 2do frame → quedan 4 frames x 8).
// Sin retroceso en HURT: el golpe comun no lo mueve en X.
// (27/09) LA MUERTE SI: termino medio entre morir en el lugar (lo nuestro) y
// el empuje de Ray (4 px/frame durante toda la explosion, ~190 px). El golpe
// que lo mata lo lanza alejandolo de la tortuga: arranca a
// ENEMY_DEATH_PUSH px/frame y baja 1 cada ENEMY_DEATH_PUSH_STEP frames
// (5+4+3+2+1 = 15 x 4 = 60 px, y quieto el resto de la explosion). El
// especial lo lanza un poco mas lejos (ENEMY_DEATH_PUSH_SPECIAL: 84 px).
#define ENEMY_EXPLODE_TIME     48   // Muerte: 6 frames x 8 ticks
#define ENEMY_DEATH_PUSH        5   // px/frame iniciales del empuje de la muerte
#define ENEMY_DEATH_PUSH_SPECIAL 6
#define ENEMY_DEATH_PUSH_STEP   4   // frames entre cada px/frame que pierde
#define ENEMY_BREAK_DOOR_TIME  32
// Spawn desde ascensor: sólo los 2 últimos frames de BREAK_DOOR (índices 3-4).
#define ENEMY_ELEV_SPAWN_TIME  16

// --- Animaciones nuevas del morado (duraciones en frames x 8 ticks) ---
#define ENEMY_GIRO_TIME        16   // Giro: 2 frames x 8
#define ENEMY_SOMERSAULT_TIME  56   // Voltereta de entrada: 7 frames x 8
#define ENEMY_SOMERSAULT_SPEED  3   // px/frame durante la voltereta (avanza mas que el walk)
// (29/09) Entrada CAMINANDO (initEnemyWalkInSpawn): a la velocidad de siempre
// hasta que el centro del cuerpo queda ENEMY_WALKIN_MARGIN px adentro.
#define ENEMY_WALKIN_SPEED      ENEMY_SPEED
#define ENEMY_WALKIN_MARGIN    24

// --- Tirada de DINAMITA (18/09) --------------------------------------------
// La anim 16 son 13 frames x 8 ticks = 104. El cartucho sale de la mano entre
// el frame 10 (brazo arriba, el TNT todavia dibujado en el sprite) y el 11
// (brazo bajando, ya sin TNT): o sea al empezar el frame 11, tick 11x8 = 88.
// El timer cuenta hacia ATRAS desde ENEMY_TNT_TIME, asi que el momento es
// 104 - 88 = 16.
#define ENEMY_TNT_TIME          104   // 13 frames x 8
#define ENEMY_TNT_RELEASE_TIMER  16   // valor del timer en el que sale el TNT

// --- Salida por la ALCANTARILLA (19/09) ------------------------------------
// La anim 17 son 6 frames. A diferencia del resto del sheet NO se reproduce
// sola: el frame lo elige manholeStep() a partir del timer (por eso el spawn
// hace SPR_setAutoAnimation(FALSE)). Hacen falta tres tramos con duraciones
// distintas:
//
//   f0            la tapa quieta en el piso, el soldier todavia abajo
//   f1 SOSTENIDO  el SALTO de salida: sube, cae y aterriza unos px por
//                 delante de la boca (19/09)
//   f2..f5        la tirada propiamente dicha, 8 ticks cada uno
//
// La tapa sale de las manos entre el f3 (brazos arriba, la tapa todavia
// dibujada) y el f4 (agachado, lineas de movimiento, ya sin tapa): o sea al
// empezar el f4. Con el timer contando hacia ATRAS eso cae justo cuando quedan
// dos frames de 8 ticks -> 16.
#define ENEMY_MANHOLE_F0_TIME        8   // f0
#define ENEMY_MANHOLE_HOP_TIME      24   // f1 sostenido (el salto)
#define ENEMY_MANHOLE_REST_TIME     32   // f2..f5, 4 x 8
#define ENEMY_MANHOLE_TIME          (ENEMY_MANHOLE_F0_TIME  + \
                                     ENEMY_MANHOLE_HOP_TIME + \
                                     ENEMY_MANHOLE_REST_TIME)   // 64
#define ENEMY_MANHOLE_RELEASE_TIMER 16   // valor del timer en el que sale la tapa

// El salto de salida. Todo esto vive en jumpZ, que es altura PURAMENTE VISUAL:
// la lane (e->y) no se toca en ningun momento, asi que no hay forma de que el
// soldier quede fuera del area caminable (clampToWalk no teletransporta: si la
// posicion no entra, revierte -- y un enemigo spawneado fuera de la calle se
// queda clavado para siempre).
//
// El truco del aterrizaje "por delante de la boca": se lo spawnea ya en la
// lane FINAL (la del agujero + HOP_DROP) y se arranca con jumpZ = HOP_DROP,
// que lo dibuja exactamente sobre el agujero. Durante el salto ese offset baja
// a 0, asi que termina posado HOP_DROP px mas abajo sin haber movido la lane.
#define ENEMY_MANHOLE_HOP_APEX      24   // px que se eleva en el salto
#define ENEMY_MANHOLE_HOP_DROP      14   // px por debajo del agujero donde cae
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
    u8          type;         // ENEMY_TYPE_FOOT_SOLDIER / _ORANGE / _WHITE

    // Franja de profundidad y tope derecho de ESTE nivel. initEnemySpawn los
    // deja en los del nivel 1 (ENEMY_LANE_*, la pared del hueco de escalera y
    // el ancho 1376); los niveles con otro fondo los pisan con
    // setEnemyBounds(). Igual que laneTop/wallXTop del Player.
    s16         laneTop;
    s16         laneBottom;
    s16         wallXTop;     // 0 = este nivel no tiene pared diagonal
    s16         wallXBottom;
    s16         levelMaxX;    // ancho del nivel en px
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
    u8          tntThrow;     // 1 = spawn guionado de la ESCALERA: reproduce la
                              // anim 16 y suelta el cartucho en el frame 11.
                              // Se apaga al soltarlo: no se repite nunca.
    s16         tntLandX;     // punto FIJO de caida del cartucho (mundo)
    s16         tntLandY;
    u8          lidThrow;     // 1 = spawn guionado de ALCANTARILLA: reproduce la
                              // anim 17 y suelta la tapa en el frame 4. Se apaga
                              // al soltarla: no vuelve a tirar nunca.
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

    // --- Salto del BLANCO (13/09) ---
    // Mismo truco que el jumpZ del Player: 'y' sigue siendo la lane real (la
    // profundidad no cambia en el aire) y jumpZ es un offset puramente VISUAL
    // que se resta al dibujar. Los otros dos tipos lo dejan siempre en 0.
    s16         jumpZ;        // Altura visual sobre el piso (0 = en el suelo)
    s32         jumpVel;      // Velocidad vertical del salto, Q8 (+ sube, - baja)
    s32         jumpZq;       // (29/09) Altura del salto en Q8 (jumpZ = su parte entera)
    u8          gravTick;     // (sin uso desde el 29/09: la caida lenta es WHITE_FALL_GRAV_Q)
    // --- Empuje de la muerte (27/09) ---
    s8          deathDir;     // hacia donde lo lanza el golpe (0 = en el lugar)
    u8          deathSpeed;   // px/frame actuales (van bajando)
    u8          deathTick;
    // --- Boomerang del AMARILLO (29/09) ---
    s8          boomSlot;     // boomerang propio en vuelo (-1 = en la mano)
    // --- Foot soldier de la LANZA (06/10) ---
    u8          spearThrow;   // 1 = todavia tiene la lanza para tirar
} Enemy;

// --- Shuriken (proyectil del foot soldier naranja) ---
typedef struct {
    Sprite*     sprite;
    s16         x;           // X de mundo
    s16         y;           // Y de mundo (pies)
    s8          dir;         // +1 derecha, -1 izquierda
    s16         cameraOffsetX;
    u8          active;      // 1 = en vuelo, 0 = inactivo
    // (06/10) El pool tambien lleva las balas del fusil y la lanza tirada.
    u8          kind;        // SHOT_SHURIKEN / SHOT_BULLET / SHOT_SPEAR
    u8          spark;       // bala: >0 = chispas en el piso (frames que quedan)
    u8          acc;         // resto Q8 del avance
    s16         zq;          // altura sobre la lane, Q8 (bala y lanza)
} Shuriken;

#define SHOT_SHURIKEN 0
#define SHOT_BULLET   1
#define SHOT_SPEAR    2

// Resetea el estado global de la IA (contador de atacantes simultáneos y
// reparto de targets) e informa cuántos jugadores hay (1 o 2).
// Llamar UNA VEZ al iniciar cada nivel, antes del primer spawn.
// Cantidad maxima de jugadores entre los que la IA reparte objetivos.
#define ENEMY_MAX_TARGETS 4

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

// (18/09) Spawn GUIONADO de la escalera del 1-1: el morado se asoma, tira UN
// cartucho de dinamita al punto fijo (landX, landY) y despues pasa a CHASE
// como un enemigo normal. No vuelve a tirar en toda la pelea.
void initEnemyTntSpawn(Enemy* e, s16 spawnX, s16 y, s8 dir, u8 palette,
                       s16 landX, s16 landY);
// Salida por la ALCANTARILLA (19/09, anim 17): el morado sale de la boca de
// tormenta, tira la TAPA en direccion 'dir' y al terminar la animacion pasa a
// CHASE como cualquier otro. 'spawnX' es el borde izquierdo del frame y 'y' la
// lane (pies), o sea el centro del agujero: ver LVL21_MANHOLE_* en level2_1.c.
void initEnemyManholeSpawn(Enemy* e, s16 spawnX, s16 y, s8 dir, u8 palette);
// Entrada saltando del BLANCO (arco de 107px, sin hitbox) -> ver enemy.c
void initEnemyWhiteJumpSpawn(Enemy* e, s16 spawnX, s16 y, s8 dir, u8 palette);
// (29/09) Entrada CAMINANDO desde fuera de pantalla, para cualquier tipo: la
// usan el naranja (no tiene voltereta: con initEnemySomersaultSpawn pedia la
// anim 15, que su sheet no tiene, y aparecia de golpe o con pixeles basura) y
// el blanco. Camina a velocidad normal hasta dejar el centro del cuerpo
// ENEMY_WALKIN_MARGIN px adentro del borde y ahi pasa a CHASE.
void initEnemyWalkInSpawn(Enemy* e, s16 spawnX, s16 y, s8 dir, u8 palette, u8 type);

// Los Player* se usan para el AGARRE por la espalda: el morado pone al jugador
// en STATE_GRABBED (playerFootGrab) y lo suelta al zafarse (mash), ser golpeado
// o si le pegan al soldier (damageEnemy lo saca de GRAB).
void updateEnemy(Enemy* e, Player* player1, Player* player2, bool twoPlayers);

// (14/09) Version de N jugadores (1..ENEMY_MAX_TARGETS). La de arriba es un
// envoltorio de esta. El reparto de objetivos y el retargeteo por cercania
// funcionan igual con 2 que con 4.
void updateEnemyN(Enemy* e, Player** pls, u8 nPl);
void setEnemyCamera(Enemy* e, s16 camX);
// Cambia la franja de profundidad, la pared diagonal (xTop = 0 -> sin pared) y
// el ancho del nivel. Llamar DESPUES de initEnemySpawn().
void setEnemyBounds(Enemy* e, s16 laneTop, s16 laneBottom,
                    s16 wallXTop, s16 wallXBottom, s16 levelW);
bool damageEnemy(Enemy* e, s16 dmg);
// (27/09) Llamar despues del golpe de una tortuga que lo MATO: lo lanza
// alejandolo de 'fromX' (el centro de la tortuga; si coinciden, hacia donde
// ella mira). 'special' = el especial, que lo lanza mas lejos.
void enemyDeathPush(Enemy* e, s16 fromX, s8 facing, bool special);
bool enemyCanBeHit(const Enemy* e);
s16  getEnemyCenterX(const Enemy* e);
s16  getEnemyCenterY(const Enemy* e);

// Media anchura del CUERPO del enemigo (hurtbox) segun su tipo. Pasarla a
// playerAttackHitsBox para que los golpes de la tortuga conecten contra el
// cuerpo y no contra el borde transparente del frame.
s16  enemyBodyHalfW(const Enemy* e);
// Alto del cuerpo sobre los pies, segun el tipo (ENEMY_BODY_H_*).
s16  enemyBodyH(const Enemy* e);

// (02/10) Golpe de la tortuga contra la HURTBOX de este enemigo: la nueva del
// morado (ENEMY_HURT_*_PURPLE, franja de alto y corrida segun hacia donde
// mira) y la de siempre (media anchura + alto desde los pies) para el resto.
bool playerAttackHitsEnemy(const Player* p, const Enemy* e);

// Intenta conectar el ataque en curso contra un jugador en (px, py) — coords
// de mundo, px = borde izquierdo del frame (misma grilla de 104px), py = pies.
// Devuelve TRUE una sola vez por swing (marca attackHit); el llamador aplica
// damagePlayer. Chequear playerCanBeHit ANTES de llamar, para no "gastar" el
// golpe contra un jugador invulnerable.
bool enemyTryHitPlayer(Enemy* e, s16 px, s16 py);

// Igual que enemyTryHitPlayer pero contra la hurtbox del cuerpo del jugador:
// el golpe conecta si el intervalo del ataque se SOLAPA con el cuerpo real
// (media anchura 'targetHalfW' = PLAYER_BODY_HALF_W), no sólo si toca el
// punto central. Con halfW = 0 es idéntica a enemyTryHitPlayer.
bool enemyTryHitPlayerBox(Enemy* e, s16 px, s16 py, s16 targetHalfW);

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

// (06/10) Proyectiles de los foot soldiers con arma (mismo pool): las
// actualizan y chequean shurikenUpdate / shurikenCheckHitPlayer, asi que
// cualquier nivel que maneje shurikens ya los maneja. La bala no se puede
// romper; la lanza tirada si, como el shuriken.
void enemyBulletSpawn(s16 cx, s16 lane, s8 dir);
void enemySpearSpawn(s16 cx, s16 lane, s8 dir);

// (06/10) Hoja y ancho de celda de cada tipo (para los spawns de las escenas
// y el chequeo de VRAM antes de crear el sprite).
const SpriteDefinition* enemySheetDef(u8 type);
s16 enemySpriteW(u8 type);

// ---------------------------------------------------------------------------
// BOOMERANG del foot soldier AMARILLO (29/09)
// ---------------------------------------------------------------------------
// Pool de MAX_BOOMERANGS. Lo lanza updateEnemyN (en el frame 6 del
// lanzamiento) y vuelve a la mano del que lo tiro; al tocarla el soldier lo
// ATRAPA (anim de lanzamiento del frame 3 al 0). Si le pega a una tortuga
// hace la anim [1] y desaparece; si una tortuga lo golpea (cuerpo a cuerpo,
// no con la patada voladora, igual que el shuriken) se rompe con la anim [2].
// Si el duenio muere, sigue de largo y se suelta al salir de camara.
//
// La escena llama boomerangStep una vez por frame, DESPUES de updateEnemyN.
// Dibuja con camX/camY (la Y de camara la pasa el nivel: el 2-1 tiene la
// suya, stage_level pasa stageCamY).
#define BOOM_EV_BROKE   1   // una tortuga rompio uno (SFX de golpe)
#define BOOM_EV_HIT     2   // uno le pego a una tortuga (ya aplico el dano)
void boomerangInit(void);

// ---------------------------------------------------------------------------
// VRAM de sprites (30/09)
// ---------------------------------------------------------------------------
// SPR_getLargestFreeVRAMBlock NO junta bloques libres vecinos: el asignador de
// SGDK (vram.c) los "empaqueta" recien cuando un VRAM_alloc falla en el bloque
// actual. Asi que preguntar el bloque mas grande antes de crear un sprite
// puede dar menos de lo que de verdad hay contiguo, y un spawn que espera esa
// respuesta se queda esperando para siempre (bug del sewer: la camara trabada
// en una oleada hasta que un misil del agua, al crearse, forzaba el empaque).
// sprVramFits responde bien: si el bloque no alcanza pero el total si,
// desfragmenta (SPR_defragVRAM, como SPR_addSpriteSafe) y vuelve a mirar.
// Como mucho una vez cada 30 frames. sprDefragLock != 0 la prohibe (sprites
// con VRAM puesta a mano apuntando a la de otro: la explosion de Baxter).
extern u8 sprDefragLock;
bool sprVramFits(u16 need);
u8   boomerangStep(Player** pls, u8 nPl, s16 camX, s16 camY);
void boomerangReleaseAll(void);

// ---------------------------------------------------------------------------
// DINAMITA del foot soldier MORADO (18/09)
// ---------------------------------------------------------------------------
// Es un ataque GUIONADO, no una habilidad de la IA: lo tira UNA sola vez el
// morado que se asoma por la escalera del 1-1, y siempre cae en el mismo
// punto. Por eso no hay pool: hay UN cartucho y UNA explosión, y el que los
// dispara es la escena (scenes.c), no updateEnemy.
//
// El vuelo copia la convención del salto del jugador: 'y' es la LANE (la
// profundidad, que interpola de la del que tira a la del punto de caída) y 'z'
// es una altura puramente VISUAL que se resta al dibujar. Así el cartucho pasa
// por delante/detrás de los sprites según su lane, como todo lo demás.
//
// La explosión es una sola, se crea al tocar el piso y se libera sola al
// terminar la animación: su VRAM (64 tiles) sólo está tomada esos 42 frames.
void tntInit(void);

// Tira el cartucho desde (x, y) — x = borde izquierdo del frame del que tira,
// y = su lane — hacia el punto FIJO (landX, landY). 'palette' es la línea del
// morado (PAL2): el tnt y la explosión comparten su paleta.
void tntLaunch(s16 x, s16 y, s8 dir, s16 landX, s16 landY, u8 palette);

// Un paso de vuelo/explosión. Devuelve TRUE el frame EXACTO en que toca el
// piso (para que la escena toque el SFX). Llamar una vez por frame.
bool tntUpdate(s16 camX);
// (19/09) Igual, pero con scroll VERTICAL. El 1-1 no lo tiene y usa la de
// arriba (que es esta con camY = 0); el 2-1 baja la esquina en diagonal y sin
// esto el cartucho y la explosion quedan clavados al mundo, no a la pantalla.
bool tntUpdateEx(s16 camX, s16 camY);

// TRUE mientras la explosión está en su ventana de daño. La escena la usa para
// castigar a los jugadores que estén dentro del radio.
bool tntBlastActive(void);
// Centro de la explosión (mundo). Sólo válido con tntBlastActive() == TRUE.
s16  tntBlastX(void);
s16  tntBlastY(void);
// TRUE si el CUERPO del jugador — centro (px), pies (py) y media anchura
// 'halfW' — se solapa con el radio de la explosión. Se mide contra el cuerpo y
// no contra el punto central porque el frame del jugador son 104px: parado al
// borde del estallido, el punto central quedaba fuera por 2px y no lo tocaba.
// Un jugador sólo puede comerse UN golpe por explosión: eso lo lleva la escena
// con su propia marca por jugador.
bool tntBlastHits(s16 px, s16 py, s16 halfW);

void tntReleaseAll(void);

// Geometría y tiempos (todo en píxeles de mundo y frames de 60 Hz).
#define TNT_FLIGHT_FRAMES     42   // duración del vuelo (0,7 s)
#define TNT_ARC_APEX          72   // altura extra del arco sobre la recta
#define TNT_HAND_Z            56   // altura de la mano sobre los pies del que tira
#define TNT_HAND_DX           54   // x de la mano dentro del frame de 64px (mirando a la derecha)
#define TNT_W                 24   // ancho/alto del sprite del cartucho
#define TNT_EXPLOSION_W       64   // ancho/alto del sprite de la explosión
#define TNT_EXPLOSION_FRAMES  42   // 7 frames x 6 ticks
// Ventana de daño: las primeras 3 poses (destello, bola y anillo) + la bola
// blanca. Después el hongo ya es humo y no lastima.
#define TNT_BLAST_DMG_FRAMES  24
#define TNT_BLAST_RADIUS_X    40   // media anchura del radio de daño
#define TNT_BLAST_RADIUS_Y    26   // media profundidad (lanes)
#define TNT_BLAST_DMG          2   // barras de vida que saca

// ---------------------------------------------------------------------------
// TAPA VOLADORA de la alcantarilla (19/09)
// ---------------------------------------------------------------------------
// El proyectil del morado que sale por la boca de tormenta. A diferencia del
// TNT no describe una parabola ni tiene punto de caida: es un TIRO RECTO por
// el eje X a la altura del pecho, que viaja hasta salir de camara y ahi se
// libera. Saca UNA barra al que toque.
//
// Convencion de coordenadas igual que el TNT:
//   x = X de mundo del CENTRO de la tapa
//   y = LANE (profundidad): la del que la tiro, y no cambia en todo el vuelo
//   z = altura VISUAL sobre el piso, constante (no cae)
//
// El pool es de LID_MAX. Con las bocas de tormenta separadas como estan, en la
// practica nunca hay mas de una en el aire; el pool es para no depender de eso
// (y cada tapa cuesta 12 tiles de VRAM de sprites, asi que tampoco conviene
// agrandarlo por las dudas).
void lidInit(void);

// Tira la tapa desde (x, y): x = borde izquierdo del frame del que tira,
// y = su lane. 'palette' es la linea del morado (PAL2): la tapa la comparte.
void lidLaunch(s16 x, s16 y, s8 dir, u8 palette);

// Un paso de vuelo de todas las tapas activas. Libera las que salieron de
// camara. Llamar una vez por frame.
void lidUpdate(s16 camX, s16 camY);

// TRUE si alguna tapa activa se solapa con el CUERPO del jugador -- centro
// (px), pies (py) y media anchura 'halfW'. Si outX no es NULL devuelve ahi la
// X de la tapa que pego, para el retroceso. La tapa NO se consume: sigue de
// largo como en el arcade; de que un jugador no se coma dos golpes seguidos
// se encarga la invencibilidad de playerCanBeHit().
bool lidHits(s16 px, s16 py, s16 halfW, s16* outX);

// (23/09) Golpe CUERPO A CUERPO del jugador sobre una tapa en vuelo: la tapa
// invierte su direccion en X y pasa a ser de las tortugas (deja de pegarles y
// empieza a barrer soldiers). La patada voladora NO la devuelve. playerIdx es
// el numero de jugador, para el puntaje si la tapa termina matando a alguien.
// Devuelve TRUE si devolvio alguna (para el SFX).
bool lidReflectByPlayerAttack(const Player* p, s8 playerIdx);

// TRUE si una tapa DEVUELTA alcanza a este enemigo (centro ex, lane ey, media
// anchura halfW). Cada tapa golpea a cada enemigo una sola vez. El nivel le
// pasa el daño a damageEnemy: LID_ENEMY_DMG.
bool lidHitsEnemy(u16 enemyIdx, s16 ex, s16 ey, s16 halfW, s8* owner);

void lidReleaseAll(void);

#define LID_MAX            2   // tapas simultaneas en el aire
#define LID_W             32   // ancho del sprite (tapa_voladora.png)
#define LID_H             24   // alto
#define LID_SPEED          5   // px de mundo por frame
#define LID_HAND_Z        46   // altura de salida sobre los pies del que tira
#define LID_HAND_DX       46   // x de las manos dentro del frame de 64px
                               // (mirando a la derecha; espejado si dir < 0)
#define LID_HIT_HALF_W    13   // media anchura de la hitbox (el arte son 32px
                               // pero los bordes son la elipse en perspectiva)
#define LID_HIT_RADIUS_Y  20   // tolerancia de lane para conectar
#define LID_MARGIN        24   // px fuera de pantalla antes de liberarla
#define LID_ENEMY_DMG      2   // vida que le saca a un soldier la tapa devuelta

#endif
