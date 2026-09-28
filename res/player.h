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
    ANIM_MANHOLE      = 21   // (20/09) Se cae por una boca de tormenta destapada
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
} PlayerAnim;

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
//           sigue pareja ahi (la caida constante que pidio Gustavo el 13/09,
//           ahora sin el escalon de velocidad al salir del apice).
//   PATADA  gravedad 1 px/frame2 con tope PLAYER_KICK_FALL_MAX (7).
// Simulado antes de pasarlo al codigo: apice 107,2 px en el frame 15
// (el mismo alto de antes), 37 frames en el aire (antes 37), con patada 34-35.
// jumpZ (entero) sigue existiendo: es la altura en px que lee todo el resto
// del juego; la verdad es jumpZq.
#define PLAYER_JUMP_Q         8
#define PLAYER_JUMP_V0_Q   3724     // 14,55 px/frame hacia arriba
#define PLAYER_GRAVITY_Q    272     // 1,0625 px/frame2
#define PLAYER_APEX_BAND_Q  408     // |vel| < 1,59 px/frame = zona del apice
#define PLAYER_APEX_GRAV_Q  136     // gravedad a la mitad en el apice
#define PLAYER_FALL_ACCEL_Q 384     // 1,5 px/frame2 al empezar a caer
#define PLAYER_KICK_GRAV_Q  256     // 1 px/frame2 cayendo con patada

// Velocidad de CAIDA cuando NO se pateo en el aire: pareja (13/09, a pedido
// de Gustavo). Desde el 28/09 se llega a ella acelerando (PLAYER_FALL_ACCEL_Q)
// en vez de saltar de golpe. Si la tortuga patea, la caida vuelve a ser
// acelerada -- patear "pesa" y te tira al piso mas rapido.
#define PLAYER_FALL_SPEED    6      // px/frame de la caida sin patada (tope)
// VELOCIDAD TERMINAL de la caida CON patada (14/09, pedido de Gustavo). Antes
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
#define PLAYER_SPECIAL_HOP       20   // px de alto del saltito (pico del arco)
#define PLAYER_SPECIAL_HOP_FROM   1   // frame en el que despega
// El especial del arcade es ATAQUE + SALTO a la vez. Apretar B y C en el mismo
// frame exacto es dificil: si llega uno solo, arranca el golpe (B) o el salto
// (C) de siempre, y si el otro llega dentro de esta ventana se convierte en el
// especial.
#define PLAYER_SPECIAL_BC_WINDOW  4

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
// ---------------------------------------------------------------------------
#define PLAYER_BODY_HALF_W  22

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
// ARRASTRE del derribo (13/09, pedido de Gustavo). Las anims 13 (la tumban de
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

    // HUD: vida, vidas y puntaje (por jugador, estilo arcade)
    s16         health;         // Barras de vida restantes (0..PLAYER_MAX_HEALTH)
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

// TRUE si la tortuga está ejecutando la patada con salto (en el aire). Se usa
// para reproducir el SFX de impacto sólo cuando conecta la patada aérea.
bool isPlayerJumpKicking(const Player* p);

// TRUE si el jugador está en el aire saltando (con o sin patada voladora).
bool isPlayerJumping(const Player* p);
// Altura VISUAL sobre el piso (0 = en el suelo). La usa quien necesite saber
// a que altura de PANTALLA esta la tortuga sin mirarle la lane: la lane 'y'
// es profundidad y NO cambia en el aire (ver jumpZ arriba).
s16  getPlayerJumpZ(const Player* p);

// Dejarse caer hasta una lane mas adelante (newFeetY > y). Mueve la
// profundidad de golpe y compensa con jumpZ, asi el sprite queda donde estaba
// y baja con la gravedad del salto. Si newFeetY <= y solo asigna la lane.
// La usa el nivel 2-1 para bajar de la cornisa de los portones.
void playerFallTo(Player* p, s16 newFeetY);

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

#endif
