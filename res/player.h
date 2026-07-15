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

// Ventana de combo: frames para encadenar el siguiente golpe (~0.58s a 60fps)
#define COMBO_WINDOW        35

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
    u8          comboStep;
    u16         comboTimer;

    // Salto
    s16         jumpVel;
    s16         groundY;
    u8          isJumpKicking;
    u8          apexHang;       // Contador de frames de float en el ápex

    // Entrada
    u16         joyId;          // JOY_1 o JOY_2
    u16         prevJoy;        // estado del joystick el frame anterior
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

#endif
