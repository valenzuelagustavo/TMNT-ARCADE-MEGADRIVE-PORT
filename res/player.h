#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <genesis.h>
#include "chars.h"   // leo_player, mike_player, raph_player, don_player

// ---------------------------------------------------------------------------
// Animaciones del personaje (orden = índice de animación en el spritesheet).
// OJO: el 31/07 se REDISEÑARON las sheets de Leo y Mike a 21 filas y se metió
// una animación de espera nueva en el índice 1. TODO el enum se corrió 1.
// Raph y Don mantienen las sheets VIEJAS (19 y 20 filas respectivamente):
// hasta que se actualicen quedan "corridas" — se aceptó así a propósito.
// ---------------------------------------------------------------------------
typedef enum {
    ANIM_IDLE         = 0,
    ANIM_IDLE2        = 1,   // Nueva pose de espera: se reproduce una vez tras
                             // ~5s quieto (PLAYER_IDLE_ANIM_DELAY) y vuelve a IDLE
    ANIM_KICK         = 2,
    ANIM_ATTACK_1     = 3,   // Combo paso 1
    ANIM_ATTACK_2     = 4,   // Combo paso 2
    ANIM_ATTACK_3     = 5,   // Combo paso 3 (finalizador)
    ANIM_JUMP         = 6,
    ANIM_JUMP_KICK    = 7,
    ANIM_WALK_FRONT   = 8,
    ANIM_WALK_BACK    = 9,
    ANIM_SPECIAL      = 10,
    ANIM_HIT_1        = 11,
    ANIM_HIT_2        = 12,
    ANIM_HIT_3        = 13,
    ANIM_GET_UP_1     = 14,
    ANIM_HIT_BEHIND_1 = 15,
    ANIM_HIT_BEHIND_2 = 16,
    ANIM_GET_UP_2     = 17,
    ANIM_HELD         = 18,  // Agarre (4 frames): 0-2 agarrado (loop manual en
                             // el grab de foot soldier), frame 3 = golpe mientras
                             // lo tienen agarrado (solo se muestra al recibir un
                             // golpe en pleno agarre)
    ANIM_WHIP_SHOCK   = 19,  // Atrapado por el látigo (frame 0) + electrocución (frames 1-2)
    ANIM_KO           = 20,  // Knockeado — pose dedicada (4 frames)
    ANIM_MANHOLE      = 21,  // (20/09) Se cae por una boca de tormenta destapada
                             // del 2-1. No se reproduce sola. Cantidad de
                             // frames distinta por sheet (Leo 9, Mike/Raph 8,
                             // Don 7), siempre con la forma:
                             //   caida  se hunde en el agujero
                             //   VACIO  se sostiene mientras suena el voice
                             //          over y esta el globo en pantalla
                             //   salida sale del agujero
                             // El vacio se detecta en runtime (numSprite == 0).
                             // El frame lo maneja playerManholeStep() a mano
                             // (ver la seccion de abajo).
    // (09/10) Solo en las hojas NUEVAS (Leo y Don, tools/gen_players_new.py):
    ANIM_ATTACK_4     = 22,  // Combo paso 4 (finalizador)
    ANIM_JUMP_KICK_DIR = 23, // Golpe en el aire con direccion (el otro es
                             // ANIM_JUMP_KICK, sin direccion)
    ANIM_THROW        = 24,  // (09/10) La llave: agarra al foot soldier y lo
                             // tira por encima del hombro. Se dibuja ESPEJADA
                             // (la tortuga le da la espalda al que agarra).
    ANIM_MOUSER_BITE  = 25   // (18/10) Un Mouser le muerde el brazo (3 frames:
                             // sacude el brazo de arriba a abajo). Los frames
                             // van A MANO: cada boton que toca el jugador
                             // avanza uno (ver GRAB_TYPE_MOUSER). Hoy solo la
                             // hoja de Leo la trae; las demas caen a ANIM_HELD.
} PlayerAnim;

// Hoja NUEVA (09/10): trae ANIM_ATTACK_4 y ANIM_JUMP_KICK_DIR. Las hojas viejas
// (Mike y Raph, que solo salen en el modo de 4) llegan hasta ANIM_MANHOLE.
#define PLAYER_NEW_SHEET_ANIMS  24
#define playerHasNewSheet(p)    ((p)->numAnims >= PLAYER_NEW_SHEET_ANIMS)
#define playerHasBiteAnim(p)    ((p)->numAnims > ANIM_MOUSER_BITE)

// ---------------------------------------------------------------------------
// Estados lógicos del personaje
// ---------------------------------------------------------------------------
typedef enum {
    STATE_IDLE,
    STATE_WALKING,
    STATE_ATTACKING,
    STATE_JUMPING,
    STATE_HURT,
    STATE_KO,        // Sin vida: tortuga knockeada (último frame de HIT_BEHIND_2)
    STATE_KNOCKED_DOWN,  // Derribada por un golpe fuerte (patada del jefe):
                         // cae de espaldas, queda tirada y se levanta
    STATE_GRABBED
} PlayerState;

// ---------------------------------------------------------------------------
// Constantes de movimiento y física
// ---------------------------------------------------------------------------
// (14/09) Tope de jugadores simultaneos. 2 es el modo normal; 4 es el modo
// secreto que se habilita con el codigo de la pantalla de cantidad de players
// (necesita multitap). Vive aca y no en hud.h porque el estado persistente
// entre niveles (vidas/puntaje/barra) tambien se dimensiona con esto.
#define MAX_PLAYERS         4

#define PLAYER_SPEED        2       // Píxeles por frame
// --- Salto (28/09: PUNTO FIJO) ---------------------------------------------
// Altura y velocidad vertical en Q8 (256 = 1 px). Antes eran enteras: la
// gravedad frenaba 1 px/frame por frame, el apice solo podia ser 1+2+...+N
// (91, 105, 120 px...) y habia que emparcharlo con un empujon suelto de 2 px;
// ademas el flote del apice era un congelado de 4 frames y la caida saltaba
// de golpe a 6 px/frame. Ahora:
//   SUBIDA  arranca a 14,55 px/frame y la gravedad (1,06 px/frame2) la frena.
//   APICE   mientras |vel| < 1,59 px/frame la gravedad va a la MITAD: la
//           tortuga "cuelga" 3 frames en lo alto de forma continua, sin
//           congelarse (y sin depender de si se aprieta una direccion).
//   CAIDA   sin patada acelera a 1,5 px/frame2 hasta PLAYER_FALL_SPEED (6) y
//           sigue pareja ahi (la caida constante del 13/09,
//           ahora sin el escalon de velocidad al salir del apice).
//   PATADA  gravedad 1 px/frame2 con tope PLAYER_KICK_FALL_MAX (7).
// Simulado antes de pasarlo al codigo: apice 107,2 px en el frame 15
// (el mismo alto de antes), 37 frames en el aire (antes 37), con patada 34-35.
// jumpZ (entero) sigue existiendo: es la altura en px que lee todo el resto
// del juego; la verdad es jumpZq.
#define PLAYER_JUMP_Q         8
// (01/10: un poco mas alto) 3724 -> 4000: apice 123 px en
// el frame 16 (antes 107,2 en el 15), 41 frames en el aire sin patada (antes
// 37). Simulado con el mismo integrador de updatePlayer.
#define PLAYER_JUMP_V0_Q   4000     // 15,6 px/frame hacia arriba
#define PLAYER_GRAVITY_Q    272     // 1,0625 px/frame2
#define PLAYER_APEX_BAND_Q  408     // |vel| < 1,59 px/frame = zona del apice
#define PLAYER_APEX_GRAV_Q  136     // gravedad a la mitad en el apice
// (09/10) 384 -> 192: la caida pasaba de 1 a 6 px/frame en 4 frames
// (1,1,1,2,3,5,6) y se notaba el escalon; ahora rampa 1,1,1,2,3,3,4,5,6. El
// apice (123 px) no cambia y el salto dura 1 frame mas (41).
#define PLAYER_FALL_ACCEL_Q 192     // 0,75 px/frame2 al empezar a caer
#define PLAYER_KICK_GRAV_Q  256     // 1 px/frame2 cayendo con patada
// (29/09) Dejarse caer de un borde (playerFallTo: escalon del sewer, cornisa
// del 2-1): arranca con este saltito hacia arriba (2 px/frame) en vez de caer
// a plomo. Simulado: 30 px de bajada en 13 frames (antes 7), 78 px en 21
// (antes 15); el saltito sube ~3 px.
#define PLAYER_DROP_HOP_Q   512

// Velocidad de CAIDA cuando NO se pateo en el aire: pareja (13/09).
// Desde el 28/09 se llega a ella acelerando (PLAYER_FALL_ACCEL_Q)
// en vez de saltar de golpe. Si la tortuga patea, la caida vuelve a ser
// acelerada -- patear "pesa" y te tira al piso mas rapido.
#define PLAYER_FALL_SPEED    6      // px/frame de la caida sin patada (tope)
// VELOCIDAD TERMINAL de la caida CON patada (14/09). Antes
// la gravedad aceleraba sin tope y la tortuga llegaba al piso a 14 px/frame:
// se sentia una plomada. Ahora acelera igual al principio (patear sigue
// "pesando") pero la velocidad se corta en este valor, asi que el ultimo tramo
// de la caida no se dispara. MEDIDO en emulador (frames de caida desde el apex
// de 107px, con un readout en pantalla; la patada se dispara en plena subida):
//   sin tope (como estaba) -> 13 frames, toca el piso a 14 px/frame
//   tope 9                 -> 14 frames, a  9 px/frame
//   tope 8                 -> 15 frames, a  8 px/frame
//   tope 7 (el que se usa) -> 16 frames, a  7 px/frame   <--
//   sin patada             -> 18 frames, a  6 px/frame (caida pareja)
// O sea: +23% de tiempo en el aire y LA MITAD de velocidad de impacto, pero
// patear sigue llegando al piso antes que no patear. Bajar mas el tope invierte
// eso (con 6 seria igual que no patear) y la patada perderia su peso.
// Vale para las DOS patadas aereas (la debil de golpe solo y la fuerte de
// golpe + direccion), que comparten este camino de caida.
#define PLAYER_KICK_FALL_MAX 7      // px/frame maximos cayendo con patada

// --- Animación del salto (control MANUAL de frames, auto-anim apagada) ---
// Subida: frame 0 | Ápice/caída: loop del frame 1 al anteúltimo | Justo
// antes de tocar el suelo: último frame.
#define PLAYER_JUMP_LOOP_TICKS  6   // Frames de juego entre pasos del loop del ápice

// --- Jump kick ---
// Sin dirección: anteultimo frame de ANIM_JUMP_KICK, vuelo normal.
// Con dirección en X: ultimo frame, y la tortuga viaja MÁS LEJOS (ímpetu):
// avanza sola a PLAYER_JUMPKICK_SPEED px/frame en la dirección elegida.
// Sheets con 3 frames de patada (Don, Raph): frame 0 = arranque, se ve estos
// ticks antes de pasar al frame de la patada elegida.
#define PLAYER_JUMPKICK_TUCK_TICKS  5
#define JUMPKICK_NONE       0
#define JUMPKICK_SOFT       1   // Botón de golpe solo
#define JUMPKICK_STRONG     2   // Golpe + dirección: más ímpetu
#define JUMPKICK_SPIN       3   // (09/10) Golpe sin direccion en lo alto del
                                // salto: el giro del especial, con dano normal
#define PLAYER_JUMPKICK_SPEED 4 // px/frame del vuelo con ímpetu (normal: 2)
// Extra de la patada FUERTE: 16px (~2 tiles) más de desplazamiento en el eje X
// sobre el vuelo completo, repartido en fracciones por frame (Q16, 16 = 1px).
// Con el salto completo (~28 frames) 9/16 ≈ 0.56 px/frame ≈ los 16px buscados.
#define PLAYER_JUMPKICK_EXTRA_Q 9

// --- Especial (26/09: rehecho como en el arcade) ---
// Swing amplio del arma con un SALTITO: el frame 0 es la preparacion en el
// piso, desde el frame PLAYER_SPECIAL_HOP_FROM la tortuga despega, sube en
// arco hasta PLAYER_SPECIAL_HOP px y vuelve a apoyar justo al terminar el
// ultimo frame. Los frames los maneja el codigo a mano (auto-animacion
// apagada, como el salto) con la duracion de cada uno en specialFrameTicks
// (player.c): asi la animacion se ve COMPLETA y el arco queda sincronizado con
// ella. La altura es solo VISUAL (se suma a jumpZ al dibujar, ver
// playerDrawZ): la lane, la profundidad y la tolerancia de golpe no cambian.
#define PLAYER_SPECIAL_HOP       32   // px de alto del saltito (pico del arco; 01/10: 20 -> 32)
#define PLAYER_SPECIAL_HOP_FROM   1   // frame en el que despega
// El especial del arcade es ATAQUE + SALTO a la vez. Apretar B y C en el mismo
// frame exacto es dificil: si llega uno solo, arranca el golpe (B) o el salto
// (C) de siempre, y si el otro llega dentro de esta ventana se convierte en el
// especial.
#define PLAYER_SPECIAL_BC_WINDOW  4

// ---------------------------------------------------------------------------
// CONTROL ARCADE (09/10) -- el "modo Arcade" de Pruebas_TMNT_Control, medido
// en el video del arcade (doc analisis-gameplay-arcade-02-10). El SALTO NO
// cambia: sigue la fisica Q8 de arriba (~123 px de alto), que es la que se
// eligio para el port; del arcade se toma solo el avance de la patada.
// ---------------------------------------------------------------------------
// 1 = control arcade; 0 = el control anterior del port (2 px/f, golpe liviano
// de 1 barra con empujon, KO de 70 frames).
#define PLAYER_ARCADE_CONTROL    1
// Caminar: el scroll del arcade va 1-2-2-2 px por frame = 1,75 px/f. Q8 con
// acumulador (p->walkAccX/Y) para no perder los decimales.
#define PLAYER_WALK_X_Q        448   // 1,75 px/f en X
#define PLAYER_WALK_Y_Q        384   // 1,5 px/f en Y
#define PLAYER_DIAG_X_Q        352   // 1,375 en diagonal (estimado)
#define PLAYER_DIAG_Y_Q        336   // 1,31 en diagonal (video 2: 1,25-1,35)
#define PLAYER_WALK_TICKS        4   // frames por pose de la caminata (a mano)
// SALTO DEL ARCADE (09/10, medido cuadro a cuadro en gameplayARCADE/tmnt.avi,
// el salto en el lugar de los frames 2044..2107; doc salto-arcade-medido-09-10):
//   - el primer frame ya despega PLAYER_AJ_POP px;
//   - sube RECTO a 5 px/f; a los PLAYER_AJ_RISE_F frames empieza la gravedad
//     (0,3125 px/f2), frena, pasa el apice (~118 px, frame 29) y cae
//     acelerando hasta 5 px/f, que mantiene hasta tocar el piso;
//   - 62 frames en el aire (el del port era de 41).
// Con el golpe en el aire, si todavia subia se frena en seco (velocidad 0) y
// cae con la misma gravedad hasta 7 px/f.
#define PLAYER_AJ_POP           11
#define PLAYER_AJ_V_Q         1280   // 5 px/f
#define PLAYER_AJ_RISE_F        14
#define PLAYER_AJ_GRAV_Q        80   // 0,3125 px/f2
#define PLAYER_AJ_FALL_MAX       5
#define PLAYER_AJ_KICK_FALL_MAX  7
// Animacion del salto (hoja nueva, 10 frames), igual que el arcade: frame 0
// los primeros PLAYER_AJ_F0_F frames (casi toda la subida), despues cada pose
// de la vuelta PLAYER_AJ_POSE_F frames y el ultimo (aterrizaje) queda hasta
// tocar el piso. Cada pose ademas sube/corre lo que tiene en el arcade
// (playerJumpDYTab / playerFrameDXTab, tools/gen_players_new.py).
#define PLAYER_AJ_F0_F          19
#define PLAYER_AJ_POSE_F         4
// Caidas que no empiezan con un salto (bajar de una cornisa, entrar desde
// arriba): el reloj del salto arranca aca, con la gravedad ya andando.
#define PLAYER_AJ_LATE_T        39
// Golpe en el aire: sale disparado hacia donde mira (sin control en X;
// arriba/abajo siguen moviendo la lane). Medido: sin direccion 2 px/f, con
// direccion 4 px/f.
#define PLAYER_AIR_ATK_VX        2
// (09/10) Golpe SIN direccion en lo alto del salto (a PLAYER_AJ_SPIN_MIN_Z px
// o mas: del frame 19 al 41 del salto, ~0,4 s alrededor del apice de 118): en
// vez de la patada hace el GIRO del especial (ANIM_SPECIAL, mismos tiempos que
// en el piso), con el dano de un golpe normal. Mas abajo, la patada.
#define PLAYER_AJ_SPIN_MIN_Z   100
#define PLAYER_AIR_ATK_VX_DIR    4
// Combo: 4 golpes con la hoja nueva (ATTACK_1..4), 3 con las viejas.
#define PLAYER_COMBO_LEN_NEW     4
// COMBO DEL ARCADE (09/10, segundo video de gameplayARCADE/tmnt.avi; doc
// combo-agarre-arcade-09-10). Hojas nuevas con el control arcade:
//   golpe 1  ATTACK_1 o ATTACK_2 (al azar)
//   golpe 2  PATADA o ATTACK_3 (al azar) ... o la LLAVE si el que recibio el
//            golpe 1 esta pegado (PLAYER_THROW_RANGE)
//   golpe 3  ATTACK_4 (o la llave, si sigue pegado)
// Se encadena SOLO si el golpe anterior conecto: el press durante el golpe
// queda guardado y el siguiente arranca apenas termina. Si no conecto, el
// golpe termina y vuelve a quieto (el proximo press arranca otra vez en 1).
// Los tiempos de cada pose estan en las tablas AtkSeq de player.c.
#define PLAYER_ARC_COMBO_LEN     3
#define PLAYER_ARC_LINK_F        6   // frames en la ultima pose esperando el press
// La llave: distancia maxima entre los centros (X) y entre las lanes (Y). En
// el video la llave sale con el soldier a ~25-35 px y la patada a ~40 px; los
// foot soldiers se frenan a ENEMY_STOP_RANGE (36), asi que hay que acercarse.
#define PLAYER_THROW_RANGE      34
#define PLAYER_THROW_TOL_Y       8
// Golpe de foot soldier: 2 barras y VOLTEO. La tortuga se da vuelta hacia
// quien le pego y sale despedida hacia atras en una parabola de PLAYER_KD_Z
// px a PLAYER_KD_VX px/f, cae, rueda PLAYER_KD_ROLL_F px y se levanta. Los
// frames y su duracion salen de una tabla por hoja (KdStep, player.c).
// Invulnerable todo el volteo + PLAYER_KD_INV_EXTRA.
#define PLAYER_ARCADE_HIT_BARS   2
#define PLAYER_KD_Z             35
#define PLAYER_KD_VX             3
#define PLAYER_KD_ROLL_F         6
#define PLAYER_KD_INV_EXTRA     10
// Muerte: sale volando (el volteo hasta tocar el piso), tirada PLAYER_DEAD_LIE_F
// con la pose de knockeada, se esfuma parpadeando 2/2 PLAYER_DEAD_FADE_F,
// invisible PLAYER_DEAD_GONE_F y reaparece parpadeando (invulnerable)
// PLAYER_ARCADE_RESPAWN_INV.
#define PLAYER_DEAD_LIE_F       83
#define PLAYER_DEAD_FADE_F      58
#define PLAYER_DEAD_GONE_F      37
#define PLAYER_ARCADE_RESPAWN_INV 128

// --- Movilidad en el aire ---
// Como en el arcade original: saltando se puede seguir reposicionando en X
// Y TAMBIÉN en Y (la lane de profundidad), no solo en X. Por eso la altura
// del salto YA NO vive en 'y' (que ahora es siempre la lane real, se puede
// mover con arriba/abajo en el aire igual que caminando): se simula aparte
// en el campo 'jumpZ' del struct Player, un offset puramente VISUAL que se
// resta al dibujar (ver render en updatePlayer). Así el salto no pisotea
// la posición de profundidad real, y de paso el Y-sorting (SPR_setDepth)
// queda siempre correcto también en el aire.

// Sprite del personaje mide ~104px (~13 tiles x 8px), dejamos margen derecho
#define PLAYER_SPRITE_W     104

// El frame del sprite es 104x104px pero el arte ocupa solo la parte baja:
// los pies del personaje están ~96px por debajo del borde superior del frame.
// playerY representa la posición de los PIES (suelo); al renderizar restamos
// este offset para colocar el frame en la pantalla.
#define PLAYER_FOOT_OFFSET  96

// Lane de profundidad: franja vertical (coordenadas de PIES) donde camina el
// jugador. Calibrado sobre bg01.png: la vereda gris empieza en la base del
// muro de edificios y llega hasta el borde de la cuneta oscura.
// Ampliada 1 tile (8px) en cada extremo respecto del calibrado original,
// para dar más margen de reposicionamiento (sobre todo saltando).
#define BOUND_LANE_TOP      142     // Pies al fondo (1 tile más allá del muro de edificios)
#define BOUND_LANE_BOTTOM   200     // Pies al frente (1 tile más allá del borde de la vereda/cuneta)

// ---------------------------------------------------------------------------
// Pared diagonal al FINAL del nivel (hueco de escalera / fire escape).
// El arte de fondo la dibuja en PERSPECTIVA, no como una pared vertical:
// el borde sólido está más ATRÁS (X menor) en la lane del fondo y más
// ADELANTE (X mayor) en la lane del frente. Un límite recto (vertical)
// dejaba al personaje "parado sobre" la pared en las lanes de atrás.
// Calibrado midiendo el borde real sobre bg01_completa.png (ver el PNG,
// columna ~1290-1360). Se interpola linealmente entre estos dos extremos
// según la 'y' del jugador (ver levelEndWallX en player.c).
// Mantener en sincronía con ENEMY_END_WALL_X_TOP/BOTTOM de enemy.h.
// ---------------------------------------------------------------------------
#define LEVEL_END_WALL_X_TOP     1308  // borde sólido en Y=BOUND_LANE_TOP (fondo)
#define LEVEL_END_WALL_X_BOTTOM  1352  // borde sólido en Y=BOUND_LANE_BOTTOM (frente)

// Encadenado de combo (B-B-B): el press de B se BUFFEREA durante todo el
// swing, y al terminar la anim queda además esta ventana de enlace (frames
// congelado en la última pose) durante la cual un press todavía encadena.
// Antes la ventana efectiva era 1 frame (había que apretar B en el frame
// exacto en que terminaba la animación) → combos casi imposibles.
#define COMBO_LINK_WINDOW   10

// ---------------------------------------------------------------------------
// Hitbox de ataque (tortuga → enemigos), medida desde el CENTRO del frame.
// El alcance varía por personaje según el largo de su arma:
//   Donatello (bō)     → 72px
//   Leonardo  (katana)  → 60px
//   Raphael   (sai)     → 48px
//   Michelangelo (nunchaku) → 44px
// ---------------------------------------------------------------------------
// ALCANCE DEL GOLPE: sale del ARTE, no de una constante (13/09).
// Antes había un alcance fijo por personaje (56 Leo, 40 Mike, 60 Don, 38 Raph)
// que valía durante TODO el swing. Pero el arma o el pie solo está extendido en
// uno o dos frames de cada animación: en el frame 0 de ATTACK_1 de Leo el arte
// llega a 6px del centro y el hitbox ya valía 56, así que el golpe conectaba
// ~50px antes de que los sprites se tocaran.
//
// Ahora el alcance de CADA FRAME se mide sobre los píxeles opacos de la sheet
// (tools/gen_player_hitbox.py -> src/player_hitbox.c) y el golpe solo conecta
// en los frames en los que el arma realmente llega. Ver attackReachNow() en
// player.c.
//
// PLAYER_ATK_SLACK es la única perilla que queda: px de gracia que se suman al
// borde del arte. 0 = contacto estricto pixel a pixel, que es como se pidió. Si
// al jugarlo se siente demasiado exigente, subirlo de a 2.
#define PLAYER_ATK_SLACK     0  // px de gracia sobre el borde opaco del arte

// Alto por defecto del cuerpo del objetivo, en px sobre sus pies. Solo lo usan
// las llamadas que no pasan una altura propia; los enemigos y el jefe pasan la
// suya (ENEMY_BODY_H_*, ROCKSTEADY_BODY_H). Con 0 se desactiva la validacion
// por altura y el golpe usa el maximo del frame (objetivos puntuales: shuriken,
// balas).
#define PLAYER_TARGET_BODY_H  60
#define PLAYER_ATK_BACK     12  // Tolerancia hacia atrás (enemigo encimado)
#define PLAYER_ATK_TOL_Y    20  // |dy| máximo en profundidad (pies)

// ---------------------------------------------------------------------------
// Hurtbox del CUERPO de la tortuga (media anchura, en px desde el centro).
// El frame mide 104px pero el cuerpo visible ocupa ~44px: los golpes de los
// enemigos conectan contra esta caja, no contra el borde transparente del
// frame (patrón colbox del manual SGDK). Así un puño que pega en el borde
// del sprite pero no toca el cuerpo ya no conecta.
//
// (02/10) HURTBOX NUEVA, la marcada en Pruebas_TMNT_Control: cabeza
// y torso, SIN las piernas. En px desde los PIES mirando a la DERECHA (al
// mirar a la izquierda se espeja): X -16..+7, Y -58..-23. Antes era 22 a cada
// lado de los pies. Como la caja esta corrida hacia atras (hacia el
// caparazon), su CENTRO no es el de los pies: usar getPlayerHurtCX() (o
// getPlayerHurtX() donde se pasa la X del frame) con PLAYER_BODY_HALF_W.
// La Y no se usa todavia: los golpes de los enemigos se validan por lane.
// ---------------------------------------------------------------------------
#define PLAYER_HURT_X0     -16
#define PLAYER_HURT_X1       7
#define PLAYER_HURT_Y0     -58
#define PLAYER_HURT_Y1     -23
#define PLAYER_BODY_HALF_W  ((PLAYER_HURT_X1 - PLAYER_HURT_X0 + 1) / 2)   // 12
#define PLAYER_HURT_DX      ((PLAYER_HURT_X0 + PLAYER_HURT_X1) / 2)       // -4 (mirando a la derecha)

// ---------------------------------------------------------------------------
// Daño recibido (golpes de los foot soldiers)
// ---------------------------------------------------------------------------
#define PLAYER_HURT_INVINCIBLE   45  // I-frames tras un golpe normal (~0.75s, SIN parpadeo)
#define PLAYER_HURT_KNOCK_FRAMES 10  // Frames de knockback (deslizamiento)
#define PLAYER_HURT_KNOCK_SPEED   2  // px/frame del knockback (10x2 = 20px)

// Knockout (barra agotada) y respawn
#define PLAYER_KO_FRAMES         70  // Cuánto dura la pose de knockeada (~1.2s)
#define PLAYER_RESPAWN_INVINCIBLE 90 // I-frames al revivir (~1.5s) — ESTOS sí parpadean

// Derribo (patada de Rocksteady): la tortuga cae de espaldas (HIT_BEHIND_1 →
// HIT_BEHIND_2), queda TIRADA en el piso un momento y se levanta (GET_UP_2).
#define PLAYER_KD_HOLD_FRAMES   35   // Frames tirada en el piso antes de levantarse
#define PLAYER_KD_INVINCIBLE   110   // I-frames de TODA la secuencia (sin parpadeo)
// ARRASTRE del derribo (13/09). Las anims 13 (la tumban de
// FRENTE: sale despedida hacia atras dando una vuelta) y 16 (la tumban de
// ESPALDAS: trastabilla hacia adelante y rueda) son golpes potentes que dan
// por sentado que el personaje VIAJA -- pero el arte no lleva ese avance
// adentro: midiendo el centro del cuerpo frame a frame en las 4 sheets, el
// dibujo oscila +-20px y no progresa. O sea que el desplazamiento lo tiene que
// poner el motor, o la tortuga se cae "en el lugar".
// Arrastre que DECAE en vez de constante: arranca en SLIDE_SPEED y baja 1
// px/frame cada SLIDE_DECAY frames hasta frenar -- 5*(4+3+2+1) = 50px en 20
// frames, que es lo que dura la caida (anim 16 son 12 frames a 5 ticks = 60).
// Frenar de a poco se lee como inercia; un arrastre constante que corta de
// golpe se nota como un tiron.
#define PLAYER_KD_SLIDE_SPEED    4   // px/frame al empezar el derribo
#define PLAYER_KD_SLIDE_DECAY    5   // frames entre cada -1 px/frame
// Frame EXACTO de ANIM_HIT_BEHIND_2 con la tortuga tirada de espaldas (la
// "12a" de la fila, índice 11). Se salta directo a este frame y se congela:
// no queremos ver la caída (los frames anteriores), sólo la pose knockeada.
#define PLAYER_KO_FRAME          11

// Agarre del látigo del robot: "metro de forcejeo" que arranca al ser agarrado
// y baja masheando A/B/C; al llegar a 0 la tortuga zafa. No hay liberación por
// tiempo (si no zafás, la electrocución te vacía la vida).
#define PLAYER_GRAB_ESCAPE           90
// Cuánto baja el metro por cada press de A/B/C al mashear.
#define PLAYER_GRAB_MASH_STEP        18

// Agarre por la ESPALDA del foot soldier morado (a diferencia del látigo del
// robot, el agarre de pie usa la anim ANIM_HELD a MANO — el frame 4 es el golpe
// mientras lo sostienen — y tiene un tope de seguridad por tiempo).
#define GRAB_TYPE_WHIP   0   // robot: anim WHIP_SHOCK con auto-animación
#define GRAB_TYPE_FOOT   1   // foot soldier: anim HELD manual (frames 0-2)
#define GRAB_TYPE_MOUSER 2   // (18/10) Mouser prendido del brazo: ANIM_MOUSER_BITE,
                             // un frame por cada boton (sin la hoja: ANIM_HELD)
#define PLAYER_HELD_LOOP_TICKS 12  // Frames entre pasos del loop de HELD 0→1→2
#define PLAYER_HELD_HIT_FRAMES 8   // Frames mostrando el frame 3 (golpe en el agarre)

// Pose de espera nueva: tras este tiempo quieto sin atacar/saltar/mover, la
// tortuga reproduce ANIM_IDLE2 una vez y vuelve a ANIM_IDLE (y repite).
#define PLAYER_IDLE_ANIM_DELAY 300  // ~5s quieto

// ---------------------------------------------------------------------------
// Vida / vidas / puntaje (HUD)
// ---------------------------------------------------------------------------
// La barra de vida (hp_bar) tiene 11 estados: frame[0] = 10 barras (llena),
// frame[10] = 0 barras (vacia). Cada golpe de un foot soldier resta 1 barra.
// Al agotarse la barra se pierde una vida y la barra se recarga; al llegar a
// 0 vidas -> game over (lo maneja scenes.c volviendo a la pantalla inicial).
#define PLAYER_MAX_HEALTH   10  // Barras de vida al maximo (frame 0 del sprite)
#define PLAYER_START_LIVES   3  // Vidas iniciales por defecto

// Vidas iniciales CONFIGURABLES (pantalla OPCIONES: 3 / 5 / 7). Arranca en
// PLAYER_START_LIVES. La usan playerPersistReset() (partida nueva) y el
// revive por continue (scenes.c). La modifica showOptions().
extern u8 vidasIniciales;

// (10/10) CONTROL PAD de opciones: que hace cada boton. El codigo del jugador
// sigue pensado en el TYPE A (A = especial, B = golpe, C = salto);
// playerRemapJoy() traduce el mando fisico a ese esquema segun controlTipo.
//   TYPE A: A.SPECIAL B.ATTACK C.JUMP   (el de siempre)
//   TYPE B: A.ATTACK  B.JUMP   C.SPECIAL
//   TYPE C: A.JUMP    B.ATTACK C.SPECIAL
// La combinacion golpe+salto sigue dando el especial en los tres.
#define CONTROL_TIPO_A   0
#define CONTROL_TIPO_B   1
#define CONTROL_TIPO_C   2
#define CONTROL_TIPOS    3
extern u8 controlTipo;
u16 playerRemapJoy(u16 raw);

// (10/10) GAME LEVEL de opciones. ARCADE es el dano de siempre; NORMAL hace
// que las tortugas reciban el NORMAL_DMG_NUM/NORMAL_DMG_DEN del dano (3/4).
// El resto de la cuenta se arrastra entre golpes (Player.dmgCarry), asi un
// golpe de 2 barras saca 1, 2, 1, 2... = 1,5 de promedio. Los golpes que
// matan de una (>= PLAYER_MAX_HEALTH) siguen matando.
#define DIFICULTAD_NORMAL  0
#define DIFICULTAD_ARCADE  1
#define NORMAL_DMG_NUM     3
#define NORMAL_DMG_DEN     4
extern u8 dificultad;

// (11/10) Vida extra cada 100 puntos (solo DIFICULTAD_NORMAL), con tope de 9
// (el HUD muestra las vidas con un solo digito). Ver addPlayerScore.
#define PLAYER_EXTRA_LIFE_PTS  100
#define PLAYER_MAX_LIVES         9

// ---------------------------------------------------------------------------
// INSTANCIA DE JUGADOR
// ---------------------------------------------------------------------------
// El módulo es multi-instancia: cada jugador (P1, P2, ...) tiene su propio
// Player. Las funciones reciben un Player* para operar sobre esa instancia.
// ---------------------------------------------------------------------------
typedef struct {
    Sprite*     sprite;
    s16         x;              // Posición X en coordenadas de MUNDO (borde del frame)
    s16         y;              // Posición Y = PIES (suelo)
    PlayerState state;

    // Límites dinámicos de movimiento (actualizados por la cámara)
    s16         boundLeft;      // Borde izquierdo actual (= cameraX)
    s16         boundRight;     // Borde derecho actual (= fin de nivel - ancho sprite)
    s16         cameraOffsetX;  // Offset de cámara para render mundo→pantalla

    // Lane de profundidad de ESTE nivel, en coordenadas de PIES. initPlayer la
    // deja en BOUND_LANE_TOP/BOUND_LANE_BOTTOM (el nivel 1, que es donde se
    // calibraron); los niveles con otro fondo la pisan con setPlayerLane().
    // El nivel 2-1 (la calle) la abre de par en par y hace el recorte fino
    // contra el polígono de la calle desde la propia escena.
    s16         laneTop;
    s16         laneBottom;

    // Pared diagonal del final del nivel 1 (hueco de escalera), interpolada
    // entre las dos lanes. wallXTop == 0 → este nivel no tiene pared.
    s16         wallXTop;
    s16         wallXBottom;

    // Combo
    u8          comboStep;      // 0 = ataque sin cadena (kick/especial), 1..3 = B-B-B
    u8          comboBuffered;  // B presionado durante el swing (buffer de input)
    u8          comboLinger;    // Frames restantes de la ventana de enlace post-anim

    // Salto (28/09: en punto fijo Q8, ver PLAYER_JUMP_Q)
    s32         jumpVq;         // velocidad vertical (+ = hacia abajo), Q8
    s32         jumpZq;         // altura, Q8 (la verdad; jumpZ es su parte entera)
    // Altura VISUAL sobre el piso (0 = pisando, crece al saltar). NO es la
    // profundidad: 'y' sigue siendo siempre la lane real, movible en el
    // aire con arriba/abajo igual que caminando. jumpZ solo desplaza el
    // dibujado hacia arriba (ver render en updatePlayer).
    s16         jumpZ;
    u8          isJumpKicking;  // JUMPKICK_NONE / JUMPKICK_SOFT / JUMPKICK_STRONG
    u8          airFrame;       // Frame actual del loop de ápice (1..n-2)
    u8          airTimer;       // Ticks hasta el próximo paso del loop
    u16         kickCarry;      // Fracción de px extra de la patada fuerte (Q16, 16=1px)

    // Ataque especial (botón A o B+C): mata foot soldiers de un golpe.
    // TODO: cuando exista HP, usarlo debe restar vida al jugador.
    u8          attackIsSpecial;
    u8          specialTick;    // ticks desde que arranco el especial
    u8          bcWindow;       // frames que quedan para completar B+C
    u8          charIndex;      // 0=Leo 1=Mike 2=Don 3=Raph (indexa playerAtkReach)

    // Entrada
    u16         joyId;          // JOY_1 o JOY_2
    u16         prevJoy;        // estado del joystick el frame anterior

    // Dirección de la mirada (para sistema de daño)
    s8          dir;            // -1 izquierda, +1 derecha

    // Daño recibido
    u8          invincible;     // I-frames restantes (0 = puede recibir golpe). Invulnerabilidad "lógica", SIN efecto visual
    u8          hurtTimer;      // Frames de knockback restantes
    s8          hurtDir;        // Dirección del empuje (-1/+1, opuesta al atacante)
    u8          hurtToggle;     // Alterna ANIM_HIT_1 / ANIM_HIT_2 en golpes seguidos

    // Knockout / respawn
    u8          koTimer;        // Frames restantes de la pose de knockeado (0 = no está KO)
    // Derribo (STATE_KNOCKED_DOWN): secuencia manual caída → piso → levantarse
    u8          kdPhase;        // 0=retroceso (sólo de espaldas) 1=cayendo
                                // 2=tirada en el piso 3=levantándose 4=listo
    u8          kdTimer;        // Frames restantes tirada en el piso (fase 2)
    u8          kdFront;        // 1 = la tumbaron DE FRENTE (cadena HIT_3 →
                                // GET_UP_1) · 0 = por la ESPALDA (HIT_BEHIND_1
                                // → HIT_BEHIND_2 → GET_UP_2)
    s8          kdSlide;        // px/frame que le queda al arrastre del derribo
                                // (decae hasta 0; ver PLAYER_KD_SLIDE_*)
    u8          kdSlideTick;    // Frames hasta el próximo -1 de kdSlide
    u8          blinkTimer;     // Frames restantes de PARPADEO (solo al revivir, no al ser golpeado)
    bool        gameOver;       // TRUE cuando cae sin vidas restantes (lo lee scenes.c)
    bool        outCarried;     // (01/10) Arranco el nivel FUERA de juego: habia
                                // quedado afuera en el nivel anterior (sin vidas
                                // y sin continuar). Ver playerPersistSave.

    // HUD: vida, vidas y puntaje (por jugador, estilo arcade)
    s16         health;         // Barras de vida restantes (0..PLAYER_MAX_HEALTH)
    u8          dmgCarry;       // (10/10) resto del dano reducido de NORMAL (0..NORMAL_DMG_DEN-1)
    u8          lives;          // Vidas restantes
    u16         score;          // Puntaje acumulado

    // Cantidad de animaciones que tiene la sheet de ESTE personaje. Se usa para
    // habilitar las animaciones nuevas (ANIM_WHIP_SHOCK, ANIM_KO) sólo si la
    // sheet realmente las contiene — hoy únicamente Leo. Con las sheets viejas
    // (18 anims) se cae automáticamente al comportamiento anterior.
    u8          numAnims;
    // Timer del preview de agarre del látigo (TEMPORAL, hasta que exista el robot).
    u8          grabTimer;

    // --- Rediseño 31/07 ---
    u16         idleTimer;   // Frames quieto sin atacar/saltar/mover (0 = en movimiento)
    u8          idleTwice;   // 1 = ya reproduciendo ANIM_IDLE2 (esperando que termine)
    u8          grabType;    // GRAB_TYPE_WHIP (látigo del robot) o GRAB_TYPE_FOOT
    u8          heldFrame;   // Frame actual del loop manual de ANIM_HELD (0-2)
    u8          heldTimer;   // Ticks hasta el próximo paso del loop de HELD
    u8          heldHit;     // Frames restantes del frame 3 de HELD (golpe en el agarre)

    // --- Caida por la boca de tormenta (20/09, solo 2-1) ---
    u8          mhPhase;     // 0 = no esta en el pozo; 1 = cayendo (f0-2),
                             // 2 = abajo (f3 + globo + VO), 3 = saliendo (f4-8)
    u16         mhTimer;     // Ticks restantes de la fase actual
    s16         mhOutX;      // Donde reaparece al terminar (mundo, borde del frame)
    s16         mhOutY;      // ...y su lane
                             // (11/10) 4 = SALIDA del 2-1: se hunde y no vuelve
    // --- Salto guionado de la salida del 2-1 (11/10) ---
    s16         exX0, exY0;  // de donde salta (borde del frame, lane)
    s16         exTX, exTY;  // a donde cae
    u16         exT, exN;    // frame actual / duracion

    // --- Control arcade (09/10) ---
    s16         walkAccX;    // acumuladores Q8 de la caminata
    s16         walkAccY;
    u8          walkTick;    // frames en la caminata (pose = walkTick / PLAYER_WALK_TICKS)
    u8          kdArcade;    // 1 = el derribo en curso es el VOLTEO (tabla KdStep)
    u8          kdIdx;       // paso del volteo
    u8          kdT;         // frames en ese paso
    u8          kdAirT;      // frames volando
    u8          kdAirTotal;  // frames que vuela en total (parabola)
    u8          koPhase;     // muerte: 0 volando, 1 tirada, 2 esfumandose,
                             // 3 invisible
    u8          jumpT;       // frames desde el despegue (salto del arcade)
    // --- Combo del arcade (09/10, ver PLAYER_ARC_*) ---
    u8          arcAtk;      // 1 = golpe del combo arcade en curso (frames a mano)
    u8          arcMove;     // ARC_* de player.c (cual de los golpes)
    u8          arcStep;     // 1..PLAYER_ARC_COMBO_LEN
    u8          arcIdx;      // pose actual de la tabla
    u8          arcT;        // frames en esa pose
    u8          arcHit;      // 1 = este golpe conecto (habilita el siguiente)
    u8          arcBuf;      // 1 = se apreto el golpe durante este golpe
    u8          arcLink;     // frames de espera en la ultima pose
    void*       arcTarget;   // el Enemy que recibio el ultimo golpe (la llave)
    // --- Cartel "toca los botones" (18/10) ---
    u8          palIdx;      // linea de paleta del sprite (la de initPlayer)
    Sprite*     hitBtnSpr;   // hit_buttons arriba de la cabeza (solo agarrada
                             // por la espalda por un foot soldier), o NULL
    u8          hitBtnT;     // ticks para alternar sus dos frames
} Player;

// ---------------------------------------------------------------------------
// API pública del módulo (multi-instancia)
// ---------------------------------------------------------------------------

// Inicializa una instancia: crea el sprite del personaje (0-3), lo asocia al
// joystick joyId (JOY_1/JOY_2) usando la paleta 'palette', en (startX, startY).
void initPlayer(Player* p, u8 selectedCharacter, u16 joyId, u8 palette, s16 startX, s16 startY);

// Lógica de input + física + render de la instancia, llamar una vez por frame
void updatePlayer(Player* p);

// --- Interfaz de cámara (llamadas desde scenes.c cada frame) ---

// Devuelve la posición X del jugador en coordenadas de MUNDO
s16  getPlayerWorldX(const Player* p);

// Notifica el desplazamiento de cámara para renderizar en pantalla
void setPlayerCamera(Player* p, s16 camX);

// Actualiza el límite izquierdo de movimiento (borde izq. de la cámara)
// Cambia la franja de profundidad por la que camina el jugador (coordenadas de
// PIES). Por defecto es la del nivel 1.
void setPlayerLane(Player* p, s16 top, s16 bottom);

// Pared diagonal al final del nivel, interpolada entre las dos lanes.
// xTop == 0 desactiva la pared (niveles que no la tienen).
void setPlayerEndWall(Player* p, s16 xTop, s16 xBottom);

void setPlayerLeftBound(Player* p, s16 leftBound);

// Actualiza el límite derecho de movimiento (fin del nivel)
void setPlayerRightBound(Player* p, s16 rightBound);

// --- Accesores para el sistema de colisiones ---

// TRUE si hay un ataque con hitbox ACTIVA en este frame: swing en curso
// (no cuenta la pose congelada de la ventana de enlace) o patada en salto.
bool isPlayerAttackActive(const Player* p);

// (09/10) Combo del arcade. playerAttackHitsEnemy (enemy.c) avisa con esto que
// el golpe conecto contra 'target' (un Enemy*): habilita el golpe siguiente y
// lo deja como candidato a la llave.
void playerAttackLanded(Player* p, void* target);
// TRUE mientras hace la llave (el soldier agarrado la sigue, ver enemy.c).
bool playerIsThrowing(const Player* p);
// (enemy.c) Arranca la llave contra 'target' (Enemy*) si esta pegado a 'p' y
// se puede agarrar: el soldier queda en manos de la tortuga. FALSE = no.
bool enemyThrowStart(void* target, Player* p);

// TRUE si el ataque activo alcanza un objetivo con centro X 'targetCX' y
// pies en 'targetFeetY' (coordenadas de MUNDO). Mide desde el CENTRO de la
// tortuga, hacia adelante según su dirección, contra la lane real ('y'),
// incluso en el aire (ahora la profundidad es siempre 'y': ver jumpZ).
bool playerAttackHits(const Player* p, s16 targetCX, s16 targetFeetY);

// Igual que playerAttackHits pero contra un objetivo con CUERPO de media
// anchura 'targetHalfW' (hurtbox): conecta cuando el intervalo del ataque
// [-ATK_BACK, +reach] se SOLAPA con la caja del cuerpo [dx-halfW, dx+halfW],
// no sólo cuando el centro entra en alcance. Con halfW = 0 es idéntica a
// playerAttackHits (objetos puntuales: shurikens, balas del jefe).
// targetBodyH = alto del cuerpo del objetivo sobre sus pies. Con ese dato el
// golpe valida tambien la ALTURA: solo conecta si el arte del jugador llega
// lejos EN LA FRANJA donde esta el cuerpo del enemigo (ver player_hitbox.h).
// Pasar 0 desactiva esa validacion (objetivos puntuales: shurikens, balas).
bool playerAttackHitsBox(const Player* p, s16 targetCX, s16 targetFeetY,
                         s16 targetHalfW, s16 targetBodyH);

// (02/10) Igual que playerAttackHitsBox pero con la hurtbox del objetivo como
// FRANJA de alto: de (pies + yTop) a (pies + yBot), offsets negativos = arriba.
// playerAttackHitsBox(..., bodyH) == playerAttackHitsBand(..., -bodyH, 0).
bool playerAttackHitsBand(const Player* p, s16 targetCX, s16 targetFeetY,
                          s16 targetHalfW, s16 yTop, s16 yBot);

// (02/10) Centro X (mundo) de la HURTBOX de la tortuga (PLAYER_HURT_*), y la
// misma posicion expresada como "X del frame" (para las funciones que le suman
// PLAYER_SPRITE_W / 2 adentro, como enemyTryHitPlayerBox).
s16 getPlayerHurtCX(const Player* p);
s16 getPlayerHurtX(const Player* p);

// TRUE si el ataque en curso es el ESPECIAL (mata foot soldiers de un
// golpe). Consultar junto con playerAttackHits para decidir el daño.
bool isPlayerSpecialAttack(const Player* p);
// Golpe contra un objetivo EN EL AIRE, sin lane (ver player.c).
bool playerAttackHitsFlying(const Player* p, s16 targetCX, s16 top, s16 bot,
                            s16 targetHalfW);
// Altura VISUAL total sobre el piso: el salto (jumpZ) mas el saltito del
// especial. Es lo que hay que restar a 'y' para dibujar la tortuga; los
// niveles que reposicionan el sprite a mano (2-1, por la camara vertical)
// tienen que usar esto y no jumpZ solo.
s16  playerDrawZ(const Player* p);
// (09/10) Ubica el sprite de la tortuga en pantalla ((sx, sy) = esquina del
// frame) sumando el desplazamiento en X del frame actual (playerFrameDXTab).
// Usarla en vez de SPR_setPosition para los sprites de las tortugas.
void playerSetSpritePos(Player* p, s16 sx, s16 sy);

// TRUE si la tortuga está ejecutando la patada con salto (en el aire). Se usa
// para reproducir el SFX de impacto sólo cuando conecta la patada aérea.
bool isPlayerJumpKicking(const Player* p);

// TRUE si el jugador está en el aire saltando (con o sin patada voladora).
bool isPlayerJumping(const Player* p);
// Altura VISUAL sobre el piso (0 = en el suelo). La usa quien necesite saber
// a que altura de PANTALLA esta la tortuga sin mirarle la lane: la lane 'y'
// es profundidad y NO cambia en el aire (ver jumpZ arriba).
s16  getPlayerJumpZ(const Player* p);
// (12/10) Patineta (Scene 6): != 0 la tortuga se dibuja asi de mas arriba
// (parada en la tabla) y al moverse no camina. La pone y la saca el nivel.
extern s16 playerSkateLift;

// Dejarse caer hasta una lane mas adelante (newFeetY > y). Mueve la
// profundidad de golpe y compensa con jumpZ, asi el sprite queda donde estaba
// y baja con la gravedad del salto. Si newFeetY <= y solo asigna la lane.
// La usa el nivel 2-1 para bajar de la cornisa de los portones.
void playerFallTo(Player* p, s16 newFeetY);

// (29/09) Camara VERTICAL de los niveles de stage_level (la cloaca): todo lo
// que se dibuja en coordenadas de mundo resta esto en Y (pantalla = y - esto).
// 0 en todos los demas niveles. Ver camYMin en stage_level.h.
extern s16 stageCamY;

// Devuelve la dirección de la mirada (-1 izquierda, +1 derecha)
s8   getPlayerDir(const Player* p);

// Devuelve la posición Y (pies)
s16  getPlayerY(const Player* p);

// --- Daño recibido (llamadas desde el sistema de colisiones en scenes.c) ---

// TRUE si el jugador puede recibir un golpe en este frame. Saltando NO se
// puede ser golpeado (esquive aéreo estilo arcade), tampoco durante HURT o KO
// o con i-frames activos. AGARRADO SÍ se puede (los otros foot soldiers le
// pegan a la tortuga que tienen inmovilizada — la anim HELD frame 3 lo muestra);
// el doble agarre lo bloquean explícitamente playerWhipGrab / enemy.c con
// playerIsGrabbed.
bool playerCanBeHit(const Player* p);

// Igual que playerCanBeHit pero acepta a la tortuga EN EL AIRE. Para los
// proyectiles que ya discriminan por altura (ver playerHitProjectile).
bool playerCanBeHitAir(const Player* p);

// Aplica un golpe: entra en STATE_HURT con la animación correcta según de
// dónde vino el golpe (HIT_1/HIT_2 alternados de frente, HIT_BEHIND_1 por la
// espalda), knockback alejándose del atacante e i-frames.
// 'attackerX' = centro X del atacante en coordenadas de MUNDO.
void damagePlayer(Player* p, s16 attackerX);
void damagePlayerAir(Player* p, s16 attackerX);   // (15/10) tambien en el aire

// Golpe que resta VARIAS barras de una (p.ej. el láser del robot = 4).
void playerHitBars(Player* p, s16 attackerX, u8 bars);

// Golpe de PROYECTIL (14/09): igual que damagePlayer/playerHitBars pero usa
// playerCanBeHitAir, o sea que TAMBIEN conecta con la tortuga en el aire. Lo
// usan las balas de Rocksteady: el tiro hacia arriba es antiaereo y con la
// regla general de "saltando no te pegan" no podia conectar nunca. Quien llama
// ya filtro la altura (la bala compara su z contra el torso del jugador).
void playerHitProjectile(Player* p, s16 attackerX, u8 bars);

// Golpe FUERTE que DERRIBA: la tortuga cae de espaldas, queda tirada un
// momento y se levanta (la usa la patada de Rocksteady). Misma regla de vida
// que playerHitBars: si la barra llega a 0 → knockout normal. Si está
// agarrada, degrada a un golpe normal (no rompe el agarre).
void playerHitBarsKnockdown(Player* p, s16 attackerX, u8 bars);

// --- Agarre del látigo del robot (lo maneja robot.c) ---

// Pone a la tortuga en STATE_GRABBED (agarrada/electrocutada). No hace nada si
// no es agarrable en ese momento (KO, salto, hurt, i-frames…) o si ya está
// agarrada por algo (foot soldier).
void playerWhipGrab(Player* p);

// Igual que playerWhipGrab pero además COLOCA a la tortuga donde termina el
// cable: 'worldX' es la X de mundo del sprite (esquina, no el centro) y 'lane'
// la profundidad del robot. Es el tirón del enganche — sin él el cable queda
// desfasado, porque solo hay 4 largos dibujados (ver ROBOT_WHIP_GRAB_* en
// robot.h). Ambos valores se clampean a los límites vigentes del jugador.
void playerWhipGrabAt(Player* p, s16 worldX, s16 lane);

// TRUE mientras la tortuga está agarrada por el látigo o por un foot soldier.
bool playerIsGrabbed(const Player* p);

// Drena 1 barra de vida (el robot lo llama ~1 vez por segundo mientras agarra).
// Si la deja en 0, dispara el knockout (que además termina el agarre).
void playerElectroDrain(Player* p);

// --- Agarre por la espalda del foot soldier morado (lo maneja enemy.c) ---

// Igual que playerWhipGrab pero con la anim ANIM_HELD a MANO (frames 0-2 con
// loop manual; el frame 3 solo al recibir un golpe en pleno agarre). La tortuga
// se zafa masheando A/B/C (grabTimer) o si alguien le pega al soldier.
void playerFootGrab(Player* p);

// (18/10) Mordida de un Mouser (baxter.c): mismo agarre con mash, pero con
// ANIM_MOUSER_BITE y el frame avanzando con cada boton que toca el jugador
// (cuanto mas rapido toca, mas rapido sacude el brazo).
void playerMouserGrab(Player* p);

// (18/10) TRUE mientras un foot soldier la tiene agarrada POR LA ESPALDA
// (GRAB_TYPE_FOOT): lo usa el cartel de "toca los botones" (hit_buttons).
bool playerIsFootGrabbed(const Player* p);

// Suelta a la tortuga de CUALQUIER agarre: vuelve a STATE_IDLE con i-frames
// para que no la vuelvan a agarrar en el acto. La llaman enemy.c (le pegaron
// al soldier que la tenía) y el mash del STATE_GRABBED.
void playerReleaseGrab(Player* p);

// --- Vida / vidas / puntaje (lectura desde el HUD en scenes.c) ---

// Barras de vida restantes (0..PLAYER_MAX_HEALTH). El HUD la traduce a frame
// de la barra: frame = PLAYER_MAX_HEALTH - health.
s16  getPlayerHealth(const Player* p);

// Vidas restantes.
u8   getPlayerLives(const Player* p);

// Puntaje acumulado.
u16  getPlayerScore(const Player* p);

// Suma 'points' al puntaje (p.ej. al matar un foot soldier).
void addPlayerScore(Player* p, u16 points);

// --- Persistencia entre niveles (vidas + puntaje) ---
// Cada nivel crea un Player nuevo con initPlayer (que resetea todo el struct).
// Para que vidas y puntaje NO se reinicien al cambiar de nivel, el módulo
// guarda un estado "meta" por joystick (JOY_1 -> P1, JOY_2 -> P2) del que
// initPlayer arranca. Desde el 13/09 la BARRA DE VIDA tambien persiste (antes
// se recargaba al maximo en cada nivel y regalaba la barra entera al pasar del
// 1-1 al apartamento de April). Lo unico que la rellena es revivir tras perder
// una vida, o empezar partida nueva (playerPersistReset).

// Guarda las vidas/puntaje de la instancia en el estado persistente. Llamar
// al GANAR un nivel (escena de victoria), antes del cambio de escena.
void playerPersistSave(const Player* p);

// Vuelve el estado persistente a los valores iniciales (partida nueva).
// Llamar al iniciar una partida (selección de personajes).
void playerPersistReset(void);

// (01/10) Olvida que el jugador habia quedado FUERA de juego (lo llama el
// continue antes de reinicializarlo). Ver initPlayer / playerPersistSave.
void playerPersistClearOut(const Player* p);

// (01/10) Lectura del estado persistente del jugador k (indice 0..3), para el
// HUD de las pantallas de titulo, que no tienen Player. Y la recarga de la
// barra que hacen esas pantallas.
void playerPersistPeek(u8 k, u8* lives, u16* score, s16* health, bool* out);
void playerPersistSetHealth(u8 k, s16 health);

// TRUE cuando el jugador agoto vidas y vida (game over). scenes.c lo consulta
// para cortar el nivel.
bool isPlayerGameOver(const Player* p);

// --- Movimiento SCRIPTEADO para cutscenes (fin del nivel) ---
// Se llaman EN LUGAR de updatePlayer: mueven/animan sin leer input.

// Deja la tortuga quieta (idle) y sólo re-renderiza en su posición.
void playerCutsceneStand(Player* p);

// Camina la tortuga hacia (targetX, targetY) de MUNDO a velocidad normal, con
// la anim de caminata. Devuelve TRUE cuando ya llegó al destino.
bool playerCutsceneWalkTo(Player* p, s16 targetX, s16 targetY);

// Congela la tortuga en un frame de "caminar hacia arriba" (ANIM_WALK_BACK, de
// espaldas a la cámara) y deja de leer input: da la apariencia de que observa la
// cutscene de victoria del nivel 2 (Shredder raptando a April).
void playerCutsceneWatch(Player* p);

// ---------------------------------------------------------------------------
// CAIDA POR LA BOCA DE TORMENTA (20/09) — nivel 2-1
// ---------------------------------------------------------------------------
// Secuencia guionada de tres fases, con la anim 21 manejada A MANO (la
// auto-animación no sirve: el frame 3 tiene que SOSTENERSE lo que dure el
// voice over, y los otros dos tramos van a otra cadencia).
//
//   fase 1  f0..f2   se hunde
//   fase 2  f3       vacío: la tortuga está abajo. Acá la escena muestra el
//                    globo "who put the light out" y suena el VO.
//   fase 3  f4..f8   sale del pozo
//
// Mientras dura, la tortuga NO lee input y NO se la puede golpear
// (playerCanBeHit devuelve FALSE). Al terminar aparece en (mhOutX, mhOutY),
// que la escena fija unos píxeles POR DEBAJO del agujero: si reapareciera
// encima volvería a caer en el acto.
#define PLAYER_MANHOLE_DMG        3   // barras que cuesta cada caída
#define PLAYER_MH_FALL_TICKS      7   // ticks por frame del hundimiento (f0..f2)
#define PLAYER_MH_OUT_TICKS       7   // ticks por frame de la salida (f4..f8)
// El VO "who put the light out" dura 1,301 s = 78 frames NTSC. 96 deja el
// globo un poco después de que termina de hablar, que es como se leen los
// otros globos del juego (ver BUBBLE_SOLID_SECS en scenes.c).
#define PLAYER_MH_HOLD_TICKS     96

// Arranca la secuencia. Cobra PLAYER_MANHOLE_DMG barras; si con eso la barra
// llega a 0 dispara el KNOCKOUT normal y NO hay caída (devuelve FALSE: la
// tortuga se está muriendo, no se cae por el pozo).
// (outX, outY) = dónde reaparece al salir, en coordenadas de MUNDO.
// Devuelve FALSE también si la tortuga no está en condiciones (ya en el pozo,
// KO, agarrada, en el aire…).
bool playerManholeFall(Player* p, s16 outX, s16 outY);

// TRUE mientras dura cualquier fase de la secuencia. La escena la usa para
// saltearse updatePlayer y para no dejar que la cámara la siga.
bool playerInManhole(const Player* p);

// (25/09) ENTRADA CAYENDO: deja a la tortuga en el aire a 'height' px sobre su
// lane, ya en la fase de CAIDA del salto (velocidad PLAYER_FALL_SPEED, loop
// de frames del apex y ultimo frame antes de tocar el piso). No es un estado
// nuevo: es un salto que arranca por la mitad, asi que aterriza, se puede
// patear en el aire y los enemigos la ignoran igual que a cualquier salto.
void playerDropIn(Player* p, s16 height);

// TRUE solo durante la fase 2 (el frame vacío): es la ventana del globo y del
// voice over. El flanco de subida lo detecta la escena comparándolo con el
// valor del frame anterior.
bool playerManholeSpeaking(const Player* p);

// Un frame de la secuencia. Se llama EN LUGAR de updatePlayer. Devuelve TRUE
// el frame en que termina (la tortuga ya está fuera y vuelve a tener control).
bool playerManholeStep(Player* p);

// ---------------------------------------------------------------------------
// SALIDA DEL NIVEL 2-1 (11/10): saltar adentro de la alcantarilla
// ---------------------------------------------------------------------------
// Con Bebop derrotado, cada tortuga salta a la boca de tormenta del final y se
// hunde (como en el arcade: asi termina la scene). Todo guionado, sin input.
//
// playerExitJumpStart  arranca un salto en arco desde donde esta hasta
//                      (tx = X del frame, ty = lane), de 'frames' de duracion
//                      y 'peak' px de alto, con las poses de ANIM_JUMP.
// playerExitJumpStep   un frame del salto. TRUE el frame en que aterriza.
// playerManholeEnter   la primera parte de la caida por el pozo (anim 21,
//                      f0..f2) SIN dano y sin la salida: queda en el frame
//                      vacio (mhPhase 4). Avanza con playerManholeStep.
// playerManholeGone    TRUE cuando ya se hundio del todo.
#define PLAYER_EXIT_JUMP_PEAK   56
void playerExitJumpStart(Player* p, s16 tx, s16 ty, u16 frames, s16 peak);
bool playerExitJumpStep(Player* p);
void playerManholeEnter(Player* p);
bool playerManholeGone(const Player* p);

#endif
