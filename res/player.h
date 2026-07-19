#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <genesis.h>
#include "chars.h"   // leo_player, mike_player, raph_player, don_player

// ---------------------------------------------------------------------------
// Animaciones del personaje (orden = índice de animación en el spritesheet)
// ---------------------------------------------------------------------------
typedef enum {
    ANIM_IDLE         = 0,
    ANIM_KICK         = 1,
    ANIM_ATTACK_1     = 2,   // Combo paso 1
    ANIM_ATTACK_2     = 3,   // Combo paso 2
    ANIM_ATTACK_3     = 4,   // Combo paso 3 (finalizador)
    ANIM_JUMP         = 5,
    ANIM_JUMP_KICK    = 6,
    ANIM_WALK_FRONT   = 7,
    ANIM_WALK_BACK    = 8,
    ANIM_SPECIAL      = 9,
    ANIM_HIT_1        = 10,
    ANIM_HIT_2        = 11,
    ANIM_HIT_3        = 12,
    ANIM_GET_UP_1     = 13,
    ANIM_HIT_BEHIND_1 = 14,
    ANIM_HIT_BEHIND_2 = 15,
    ANIM_GET_UP_2     = 16,
    ANIM_HELD         = 17
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
    STATE_GRABBED
} PlayerState;

// ---------------------------------------------------------------------------
// Constantes de movimiento y física
// ---------------------------------------------------------------------------
#define PLAYER_SPEED        2       // Píxeles por frame
#define PLAYER_JUMP_FORCE   13      // Velocidad inicial del salto (apex ~84px)
#define GRAVITY             1       // Aceleración de la gravedad (px/frame²)
#define APEX_HANG           4       // Frames de float en el punto más alto del salto

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
#define BOUND_LANE_TOP      150     // Pies al fondo (base del muro de edificios)
#define BOUND_LANE_BOTTOM   192     // Pies al frente (borde de la vereda/cuneta)

// Encadenado de combo (B-B-B): el press de B se BUFFEREA durante todo el
// swing, y al terminar la anim queda además esta ventana de enlace (frames
// congelado en la última pose) durante la cual un press todavía encadena.
// Antes la ventana efectiva era 1 frame (había que apretar B en el frame
// exacto en que terminaba la animación) → combos casi imposibles.
#define COMBO_LINK_WINDOW   20

// ---------------------------------------------------------------------------
// Hitbox de ataque (tortuga → enemigos), medida desde el CENTRO del frame.
// Las tortugas pegan con armas: su alcance (64px) supera el rango de ataque
// del foot soldier (44px) y su distancia de frenado (36px).
// ---------------------------------------------------------------------------
#define PLAYER_ATK_REACH    64  // Alcance hacia adelante (centro a centro)
#define PLAYER_ATK_BACK     12  // Tolerancia hacia atrás (enemigo encimado)
#define PLAYER_ATK_TOL_Y    20  // |dy| máximo en profundidad (pies)

// ---------------------------------------------------------------------------
// Daño recibido (golpes de los foot soldiers)
// ---------------------------------------------------------------------------
#define PLAYER_HURT_INVINCIBLE   45  // I-frames tras recibir un golpe (~0.75s)
#define PLAYER_HURT_KNOCK_FRAMES 10  // Frames de knockback (deslizamiento)
#define PLAYER_HURT_KNOCK_SPEED   2  // px/frame del knockback (10x2 = 20px)

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
    s16         groundY;
    u8          isJumpKicking;
    u8          apexHang;       // Contador de frames de float en el ápex

    // Entrada
    u16         joyId;          // JOY_1 o JOY_2
    u16         prevJoy;        // estado del joystick el frame anterior

    // Dirección de la mirada (para sistema de daño)
    s8          dir;            // -1 izquierda, +1 derecha

    // Daño recibido
    u8          invincible;     // I-frames restantes (0 = puede recibir golpe)
    u8          hurtTimer;      // Frames de knockback restantes
    s8          hurtDir;        // Dirección del empuje (-1/+1, opuesta al atacante)
    u8          hurtToggle;     // Alterna ANIM_HIT_1 / ANIM_HIT_2 en golpes seguidos
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
// tortuga, hacia adelante según su dirección. En el aire (jump kick) usa
// groundY como lane de profundidad.
bool playerAttackHits(const Player* p, s16 targetCX, s16 targetFeetY);

// Devuelve la dirección de la mirada (-1 izquierda, +1 derecha)
s8   getPlayerDir(const Player* p);

// Devuelve la posición Y (pies)
s16  getPlayerY(const Player* p);

// --- Daño recibido (llamadas desde el sistema de colisiones en scenes.c) ---

// TRUE si el jugador puede recibir un golpe en este frame. Saltando NO se
// puede ser golpeado (esquive aéreo estilo arcade), tampoco durante HURT,
// GRABBED o con i-frames activos.
bool playerCanBeHit(const Player* p);

// Aplica un golpe: entra en STATE_HURT con la animación correcta según de
// dónde vino el golpe (HIT_1/HIT_2 alternados de frente, HIT_BEHIND_1 por la
// espalda), knockback alejándose del atacante e i-frames.
// 'attackerX' = centro X del atacante en coordenadas de MUNDO.
void damagePlayer(Player* p, s16 attackerX);

#endif
