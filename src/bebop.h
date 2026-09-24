#ifndef _BEBOP_H_
#define _BEBOP_H_

#include <genesis.h>
#include "enemies.h"     // bebop_boss, bebop_shot_spr (SPRITE)
#include "player.h"

// ===========================================================================
// BEBOP — jefe del nivel 2-1 (la calle, frente al auto quemado)
// ===========================================================================
// Mismo patron que rocksteady.c: enemigo unico con maquina de estados propia,
// que vive en el tramo final del nivel con la camara ya trabada en su tope
// (LVL21_CAM_X_MAX / LVL21_CAM_Y_MAX). Antes, llegar al borde derecho
// terminaba el nivel; ahora dispara la pelea y el nivel termina cuando el
// jefe cae.
//
// ENTRADA (24/09, sobre la captura que marco Gustavo)
// Cae desde ARRIBA de la pantalla en diagonal hacia la derecha, apoya un pie
// en el TECHO DEL AUTO del fondo, se sostiene ahi un instante y pega el
// segundo salto hasta la calle, delante del auto. Recien ahi empieza la
// pelea. Las dos parabolas son puramente visuales (no hay clamp de area
// caminable durante la entrada).
//
// CONDUCTA
//   IDLE      quieto unos frames; si pasa mucho sin que lo golpeen, VITOREA
//             (anim [1], estira los brazos) -- es el "cada tanto" que pidio
//             Gustavo, y le da al jugador la ventana para acercarse.
//   WALK      se alinea en lane y se acerca al jugador mas cercano.
//   EMBESTIDA lejos: corre en linea recta (anim [3]) y pega por CONTACTO real
//             de los cuerpos, como la de Rocksteady.
//   UPPERCUT  cerca: anim [4]; los frames 0-2 son el arranque y 3-4 el golpe.
//   DISPARO   a media distancia: anim [5]. Frames 0-1 = parado, 2-4 =
//             agachado. Elige AGACHADO contra un jugador en el piso y PARADO
//             contra uno que esta saltando (el tiro sale mas alto).
//   GOLPES    anim [6]: 0-1 al recibir, 2 tirado en el piso, 3-5 levantarse.
//             Cada BEBOP_KD_INTERVAL golpes se va al piso.
//   MUERTE    misma fila: cae, se queda tirado y desaparece -> BEBOP_GONE,
//             que es lo que termina el nivel.
//
// El arte mira a la DERECHA -> SPR_setHFlip cuando dir < 0. La celda es de
// 112x120 con los PIES en el borde inferior (el generador ancla por las
// piernas), asi que b->x es el BORDE IZQUIERDO del frame y el centro visual
// del cuerpo es b->x + BEBOP_FRAME_W/2.
// ===========================================================================

// --- Filas del spritesheet (ver tools/gen_bebop_sheet.py) ---
#define BEBOP_ANIM_IDLE      0   // 3f
#define BEBOP_ANIM_TAUNT     1   // 3f, estira los brazos
#define BEBOP_ANIM_WALK      2   // 6f
#define BEBOP_ANIM_CHARGE    3   // 4f, la embestida
#define BEBOP_ANIM_UPPER     4   // 5f: 0-2 arranque, 3-4 el golpe
#define BEBOP_ANIM_SHOOT     5   // 5f: 0-1 parado, 2-4 agachado
#define BEBOP_ANIM_HURT      6   // 6f: 0-1 golpe, 2 tirado, 3-5 se levanta

#define BEBOP_SHOOT_FR_STAND  0   // primer frame del disparo parado
#define BEBOP_SHOOT_FR_CROUCH 2   // primer frame del disparo agachado
#define BEBOP_HURT_FR_HIT     0
#define BEBOP_HURT_FR_DOWN    2
#define BEBOP_HURT_FR_GETUP   3
#define BEBOP_UPPER_FR_HIT    3   // desde este frame el uppercut pega

// --- Geometria del frame ---
#define BEBOP_FRAME_W       112
#define BEBOP_FRAME_H       120
#define BEBOP_FOOT_OFFSET   120   // los pies son el borde inferior de la celda
#define BEBOP_BODY_HALF_W    22   // hurtbox del cuerpo (como la tortuga)
#define BEBOP_BODY_H         80   // alto del cuerpo sobre los pies

// --- Vida y daño ---
#define BEBOP_HP            110   // golpes normales; el especial saca BEBOP_SPECIAL_DMG
#define BEBOP_SPECIAL_DMG     3
#define BEBOP_CHARGE_DMG      3   // barras que saca la embestida
#define BEBOP_UPPER_DMG       2   // barras del uppercut
#define BEBOP_SHOT_DMG        1   // barras de cada disparo

// --- La arena: el ultimo tramo de la calle, con la camara clavada ----------
// X del CENTRO del cuerpo. El izquierdo lo pone el auto (mas alla se lo comeria
// el dibujo) y el derecho, la pared del fondo.
#define BEBOP_X_MIN        2300
#define BEBOP_X_MAX        2520
// La lane la recorta ademas la tabla del nivel (lvl21WalkTop), esto es el tope
// de seguridad.
#define BEBOP_LANE_TOP      500
#define BEBOP_LANE_BOTTOM   620

// --- Entrada: las dos parabolas de la captura ------------------------------
#define BEBOP_CAR_X        2342   // pie sobre el techo del auto (centro del cuerpo)
#define BEBOP_CAR_Y         483
#define BEBOP_LAND_X       2415   // donde apoya en la calle y arranca la pelea
#define BEBOP_LAND_Y        565
#define BEBOP_FALL_FROM_DX  (-60) // arranca esos px a la izquierda del auto...
#define BEBOP_FALL_FROM_DZ  140   // ...y esa altura por encima (fuera de cuadro)
#define BEBOP_FALL_TICKS     34   // duracion de la caida al auto
#define BEBOP_CAR_HOLD       26   // frames apoyado en el auto
#define BEBOP_JUMP_TICKS     26   // duracion del salto auto -> calle
#define BEBOP_JUMP_APEX      26   // cuanto se eleva en ese salto

// --- Ritmo de la pelea -----------------------------------------------------
#define BEBOP_SPEED           2   // px/frame caminando
#define BEBOP_CHARGE_SPEED    6   // px/frame embistiendo
#define BEBOP_CHARGE_MAX     80   // tope de frames de la embestida
#define BEBOP_CHARGE_OVER    16   // sigue de largo tras conectar
#define BEBOP_CHARGE_DIST   150   // a mas de esto, embiste
#define BEBOP_UPPER_RANGE    62   // alcance hacia adelante del uppercut
// (25/09) El uppercut es el ANTIAEREO: sale cuando el jugador SALTA cerca de
// el (ya no es el golpe de melee comun). Reacciona desde IDLE o caminando.
#define BEBOP_AA_RANGE       80   // distX maxima para reaccionar a un salto
#define BEBOP_AA_MAX_Z      110   // jumpZ maxima que alcanza el puño
#define BEBOP_AA_COOLDOWN    40   // frames entre dos antiaereos
#define BEBOP_UPPER_WIND_TICKS 4  // ticks por frame del arranque (0-2): rapido,
                                  // o el jugador ya aterrizo cuando pega
#define BEBOP_UPPER_HIT_TICKS  8  // ticks por frame del golpe (3-4)
// Rotacion de conductas (25/09): antes el disparo pedia estar alineado en lane
// por casualidad y casi nunca salia. Ahora el jefe ROTA entre tres pasos:
//   0, 1  ARMA       se alinea en lane (sin acercarse) y dispara
//   2     CUERPO     lejos embiste; cerca, disparo agachado a quemarropa
#define BEBOP_STEPS           3
#define BEBOP_CLOSE_RANGE    70   // "cerca" para el paso de cuerpo
#define BEBOP_ALIGN_TICKS    70   // tope de frames alineandose antes de rendirse
#define BEBOP_SHOOT_RANGE   260   // a mas de esto no dispara (no se lo ve)
#define BEBOP_HIT_TOL_Y      26   // |dy| de pies para que conecte un golpe
#define BEBOP_ALIGN_Y         6   // |dy| que considera "alineado" en lane
#define BEBOP_IDLE_MIN       24   // quieto minimo entre acciones
#define BEBOP_COOLDOWN       34   // frames despues de un ataque
#define BEBOP_HURT_FRAMES    14   // flinch
#define BEBOP_KD_INTERVAL     8   // golpes recibidos entre caidas
#define BEBOP_KD_HOLD        70   // frames tirado en el piso
#define BEBOP_GETUP_TICKS     8   // ticks por frame al levantarse
#define BEBOP_TAUNT_IDLE    260   // frames sin recibir golpes -> vitorea
#define BEBOP_TAUNT_TICKS    10   // ticks por frame del vitoreo
#define BEBOP_DEAD_HOLD     120   // tirado antes de desaparecer
#define BEBOP_WALK_TICKS      6   // ticks por frame de la caminata

// --- Disparo ---------------------------------------------------------------
// Boca del arma MEDIDA sobre la grilla generada (celda 112x120, pies abajo):
//   parado   punta en (108, fila 58)  -> dx = 108-56 = 52, z = 120-58 = 62
//   agachado punta en (111, fila 75)  -> dx = 55,          z = 45
#define BEBOP_MUZZLE_STAND_X   52
#define BEBOP_MUZZLE_STAND_Z   62
#define BEBOP_MUZZLE_CROUCH_X  55
#define BEBOP_MUZZLE_CROUCH_Z  45
#define BEBOP_SHOT_TICKS       7   // ticks por frame de la anim de disparo
#define BEBOP_SHOT_SPEED       4   // px/frame del proyectil
#define BEBOP_SHOT_GROW        5   // ticks entre aro y aro del proyectil
#define BEBOP_SHOT_FRAMES      5   // aros del sprite bebop_shot_gen
#define MAX_BEBOP_SHOTS        2
#define BEBOP_SHOT_W          72   // celda del proyectil
#define BEBOP_SHOT_H          40
#define BEBOP_SHOT_TOL_Y      22   // |dy| de lane para conectar
#define BEBOP_SHOT_TOL_Z      36   // |dz| contra el torso del jugador
#define BEBOP_SHOT_TORSO_Z    36   // altura del torso sobre los pies
#define BEBOP_SHOT_MARGIN     40   // px fuera de pantalla antes de liberarlo

// --- Flash por vida baja (igual que Rocksteady) ------------------------------
// Alterna la paleta normal con una "quemada" (cada canal x2): lento por debajo
// de un tercio de la vida, rapido por debajo de un sexto.
#define BEBOP_FLASH_HP        (BEBOP_HP / 3)
#define BEBOP_FLASH_CRIT_HP   (BEBOP_HP / 6)
#define BEBOP_FLASH_TICKS      8
#define BEBOP_FLASH_CRIT_TICKS 3

typedef enum {
    BEBOP_INACTIVE = 0,
    BEBOP_FALL,        // cayendo hacia el auto
    BEBOP_ON_CAR,      // apoyado en el techo del auto
    BEBOP_JUMP_DOWN,   // salto del auto a la calle
    BEBOP_IDLE,
    BEBOP_TAUNT,
    BEBOP_WALK,
    BEBOP_CHARGE,
    BEBOP_UPPER,
    BEBOP_SHOOT,
    BEBOP_HURT,
    BEBOP_DOWN,        // tirado en el piso
    BEBOP_GETUP,
    BEBOP_DEAD,        // cayendo muerto
    BEBOP_GONE         // termino: el nivel puede cerrar
} BebopState;

typedef struct {
    Sprite*    sprite;
    BebopState state;
    s16        x, y;          // x = borde IZQUIERDO del frame; y = pies (lane)
    s16        z;             // altura visual sobre la lane (entrada y salto)
    s8         dir;           // 1 mira a la derecha, -1 a la izquierda
    s16        hp;
    u8         anim;
    u16        timer;
    u16        cooldown;
    u16        calmTimer;     // frames sin recibir golpes (para el vitoreo)
    u8         hitsTaken;
    u8         chargeHit;     // ya conecto esta embestida
    s8         chargeDir;
    u8         upperHit;      // ya conecto este uppercut
    u8         frameTick;     // contador para los frames manejados a mano
    u8         frame;
    u8         crouchShot;    // el disparo en curso es el agachado
    u8         step;          // paso de la rotacion (ver BEBOP_STEPS)
    u8         alignOnly;     // caminata de alinearse para disparar
    u16        aaCooldown;    // frames hasta el proximo antiaereo
    u8         flashTick;
    u8         flashOn;
    s16        fromX, fromY;  // origen de la parabola en curso (entrada)
    s16        cameraOffsetX;
    s16        cameraOffsetY;
} Bebop;

void bebopInit(Bebop* b);
// Arranca la entrada: cae desde arriba hacia el techo del auto.
void bebopSpawn(Bebop* b);
// Un frame de jefe: IA, animacion, proyectiles y golpes CONTRA los jugadores.
void bebopUpdate(Bebop* b, Player** pls, u8 nPl, s16 camX, s16 camY);
// Golpe del jugador. Devuelve TRUE si el golpe lo mato.
bool bebopDamage(Bebop* b, s16 dmg);
bool bebopCanBeHit(const Bebop* b);
bool bebopIsActive(const Bebop* b);
bool bebopIsGone(const Bebop* b);
s16  bebopGetCenterX(const Bebop* b);
s16  bebopGetCenterY(const Bebop* b);
s16  bebopHp(const Bebop* b);
void bebopRelease(Bebop* b);
// Pool de disparos (estado de MODULO, no del jefe): bebopInit ya lo limpia,
// pero la escena puede necesitar soltarlos por su cuenta.
void bebopShotInit(void);
void bebopShotReleaseAll(void);

#endif
