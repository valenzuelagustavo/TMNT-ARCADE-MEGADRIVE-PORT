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
    ANIM_KO           = 20   // Knockeado — pose dedicada (4 frames)
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
    STATE_GRABBED
} PlayerState;

// ---------------------------------------------------------------------------
// Constantes de movimiento y física
// ---------------------------------------------------------------------------
#define PLAYER_SPEED        2       // Píxeles por frame
#define PLAYER_JUMP_FORCE   13      // Velocidad inicial del salto (apex ~84px)
#define GRAVITY             1       // Aceleración de la gravedad (px/frame²)
#define APEX_HANG           4       // Frames de float en el punto más alto del salto

// --- Animación del salto (control MANUAL de frames, auto-anim apagada) ---
// Subida: frame 0 | Ápice/caída: loop del frame 1 al anteúltimo | Justo
// antes de tocar el suelo: último frame.
#define PLAYER_JUMP_LOOP_TICKS  6   // Frames de juego entre pasos del loop del ápice

// --- Jump kick ---
// Sin dirección: frame 0 de ANIM_JUMP_KICK, vuelo normal.
// Con dirección en X: frame 1, y la tortuga viaja MÁS LEJOS (ímpetu):
// avanza sola a PLAYER_JUMPKICK_SPEED px/frame en la dirección elegida.
#define JUMPKICK_NONE       0
#define JUMPKICK_SOFT       1   // Botón de golpe solo
#define JUMPKICK_STRONG     2   // Golpe + dirección: más ímpetu
#define PLAYER_JUMPKICK_SPEED 4 // px/frame del vuelo con ímpetu (normal: 2)

// --- Especial ---
// El arte del especial es un "saltito" en el lugar: mientras dura la anim,
// el sprite se DIBUJA unos px más arriba. Offset puramente VISUAL: la Y
// lógica (lane, hitbox, profundidad) no cambia.
#define PLAYER_SPECIAL_LIFT 8   // px de elevación visual durante el especial

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
#define PLAYER_ATK_REACH_LEO    56  // Katana: alcance medio
#define PLAYER_ATK_REACH_MIKE   40  // Nunchaku: alcance corto
#define PLAYER_ATK_REACH_DON    60  // Bō: alcance largo
#define PLAYER_ATK_REACH_RAPH   38  // Sai: alcance corto-medio
#define PLAYER_ATK_BACK     12  // Tolerancia hacia atrás (enemigo encimado)
#define PLAYER_ATK_TOL_Y    20  // |dy| máximo en profundidad (pies)
#define PLAYER_JUMPKICK_REACH  28  // Alcance de la patada en salto (medido desde centro del sprite)

// ---------------------------------------------------------------------------
// Daño recibido (golpes de los foot soldiers)
// ---------------------------------------------------------------------------
#define PLAYER_HURT_INVINCIBLE   45  // I-frames tras un golpe normal (~0.75s, SIN parpadeo)
#define PLAYER_HURT_KNOCK_FRAMES 10  // Frames de knockback (deslizamiento)
#define PLAYER_HURT_KNOCK_SPEED   2  // px/frame del knockback (10x2 = 20px)

// Knockout (barra agotada) y respawn
#define PLAYER_KO_FRAMES         70  // Cuánto dura la pose de tortuga knockeada (~1.2s)
#define PLAYER_RESPAWN_INVINCIBLE 90 // I-frames al revivir (~1.5s) — ESTOS sí parpadean
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
#define PLAYER_START_LIVES   3  // Vidas iniciales

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

    // Combo
    u8          comboStep;      // 0 = ataque sin cadena (kick/especial), 1..3 = B-B-B
    u8          comboBuffered;  // B presionado durante el swing (buffer de input)
    u8          comboLinger;    // Frames restantes de la ventana de enlace post-anim

    // Salto
    s16         jumpVel;
    // Altura VISUAL sobre el piso (0 = pisando, crece al saltar). NO es la
    // profundidad: 'y' sigue siendo siempre la lane real, movible en el
    // aire con arriba/abajo igual que caminando. jumpZ solo desplaza el
    // dibujado hacia arriba (ver render en updatePlayer).
    s16         jumpZ;
    u8          isJumpKicking;  // JUMPKICK_NONE / JUMPKICK_SOFT / JUMPKICK_STRONG
    u8          apexHang;       // Contador de frames de float en el ápex
    u8          airFrame;       // Frame actual del loop de ápice (1..n-2)
    u8          airTimer;       // Ticks hasta el próximo paso del loop

    // Ataque especial (botón A o B+C): mata foot soldiers de un golpe.
    // TODO: cuando exista HP, usarlo debe restar vida al jugador.
    u8          attackIsSpecial;
    s16         atkReach;       // Alcance frontal del ataque (varía por arma)

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

// TRUE si el ataque en curso es el ESPECIAL (mata foot soldiers de un
// golpe). Consultar junto con playerAttackHits para decidir el daño.
bool isPlayerSpecialAttack(const Player* p);

// TRUE si la tortuga está ejecutando la patada con salto (en el aire). Se usa
// para reproducir el SFX de impacto sólo cuando conecta la patada aérea.
bool isPlayerJumpKicking(const Player* p);

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

// Aplica un golpe: entra en STATE_HURT con la animación correcta según de
// dónde vino el golpe (HIT_1/HIT_2 alternados de frente, HIT_BEHIND_1 por la
// espalda), knockback alejándose del atacante e i-frames.
// 'attackerX' = centro X del atacante en coordenadas de MUNDO.
void damagePlayer(Player* p, s16 attackerX);

// Golpe que resta VARIAS barras de una (p.ej. el láser del robot = 4).
void playerHitBars(Player* p, s16 attackerX, u8 bars);

// --- Agarre del látigo del robot (lo maneja robot.c) ---

// Pone a la tortuga en STATE_GRABBED (agarrada/electrocutada). No hace nada si
// no es agarrable en ese momento (KO, salto, hurt, i-frames…) o si ya está
// agarrada por algo (foot soldier).
void playerWhipGrab(Player* p);

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

#endif
