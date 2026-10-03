#ifndef _BEBOP_H_
#define _BEBOP_H_

#include <genesis.h>
#include "boss_flash.h"
#include "enemies.h"     // bebop_boss, bebop_shot_spr (SPRITE)
#include "player.h"

// ===========================================================================
// BEBOP — jefe del nivel 2-1 (la calle, frente al auto quemado) y, desde el
// 26/09, tambien de la Scene 4 (el garage; ver BebopArena)
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
#define BEBOP_HP             55   // golpes normales; el especial saca BEBOP_SPECIAL_DMG (01/10: 110 -> 55)
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
// (03/10) Velocidades MEDIDAS en el video del arcade (Q8: 256 = 1 px/frame).
#define BEBOP_WALK_X_Q      256   // 1 px/f caminando (antes 2)
#define BEBOP_WALK_Y_Q      192   // 0,75 px/f en profundidad
#define BEBOP_CHARGE_Q      896   // 3,5 px/f embistiendo (antes 6)
#define BEBOP_CHARGE_LANE_Q 256   // corrige la lane 1 px/f mientras embiste
#define BEBOP_CHARGE_WINDUP  32   // amaga en el lugar antes de correr (video: ~32)
#define BEBOP_CHARGE_TICKS    4   // ticks por frame de la anim (orden 0,1,3,2)
#define BEBOP_CHARGE_MAX     56   // tope de frames corriendo (~196 px)
#define BEBOP_CHARGE_OVER    16   // sigue de largo tras conectar
#define BEBOP_CHARGE_DIST   150   // a mas de esto, embiste
#define BEBOP_UPPER_RANGE    62   // alcance hacia adelante del uppercut
// (25/09) El uppercut es el ANTIAEREO: sale cuando el jugador SALTA cerca de
// el (ya no es el golpe de melee comun). Reacciona desde IDLE o caminando.
#define BEBOP_AA_RANGE       80   // distX maxima para reaccionar a un salto
#define BEBOP_AA_MAX_Z      110   // jumpZ maxima que alcanza el puño
#define BEBOP_AA_COOLDOWN    40   // frames entre dos antiaereos
#define BEBOP_UPPER_WIND_TICKS 2  // ticks por frame del arranque (0-2): rapido,
                                  // o el jugador ya aterrizo cuando pega
#define BEBOP_UPPER_HIT_TICKS  6  // ticks por frame del golpe (3-4)
                                  // (03/10: el arcade tarda ~18 frames en todo)
// Rotacion de conductas (25/09): antes el disparo pedia estar alineado en lane
// por casualidad y casi nunca salia. Ahora el jefe ROTA entre tres pasos:
//   0, 1  ARMA       se alinea en lane (sin acercarse) y dispara
//   2     CUERPO     lejos embiste; cerca, disparo agachado a quemarropa
#define BEBOP_STEPS           2   // (03/10) arcade: dispara, embiste, dispara...
#define BEBOP_CLOSE_RANGE    70   // "cerca" para el paso de cuerpo
#define BEBOP_ALIGN_TICKS    70   // tope de frames alineandose antes de rendirse
#define BEBOP_SHOOT_RANGE   260   // a mas de esto no dispara (no se lo ve)
#define BEBOP_HIT_TOL_Y      26   // |dy| de pies para que conecte un golpe
#define BEBOP_ALIGN_Y         6   // |dy| que considera "alineado" en lane
#define BEBOP_IDLE_MIN       24   // quieto minimo entre acciones
#define BEBOP_COOLDOWN       34   // frames despues de un ataque
#define BEBOP_HURT_FRAMES    18   // flinch (03/10, video: 16-20)
// (03/10) EMBESTIDA CON ARMADURA: en el amague y la corrida los golpes le
// sacan vida (y lo pueden matar) pero NO la cortan: ni flinch, ni caida, ni
// contraataque. Sin flinch no hay i-frames, asi que tras cada golpe queda
// intocable (armorTimer) estos frames: uno por swing; el especial, uno solo.
#define BEBOP_CHARGE_HIT_CD     BEBOP_HURT_FRAMES
#define BEBOP_CHARGE_HIT_CD_SP  40
#define BEBOP_KD_INTERVAL     8   // golpes recibidos entre caidas
// (03/10) Como en el arcade: el 4to golpe SEGUIDO (sin BEBOP_COMBO_RESET
// frames de calma entre golpes) y el especial lo DERRIBAN.
#define BEBOP_KD_COMBO        4
// Caida del arcade: frame 3 (sentado) deslizando hacia atras, frame 4
// (arrodillado) y frame 5 (se levanta): 12 + 36 + 12 frames.
#define BEBOP_KD_SLIDE_F     12
#define BEBOP_KD_SLIDE_Q   1024   // 4 px/f mientras desliza (~48 px)
#define BEBOP_KD_KNEEL_F     36
#define BEBOP_KD_RISE_F      12
// Al levantarse, SIEMPRE suelta el uppercut (en el video, ~12 frames despues).
#define BEBOP_GETUP_WAIT     12
// Despues de disparar VITOREA (3 de cada 4 veces en el video): 0/1 cada 8.
#define BEBOP_TAUNT_SHOT_PCT 75
#define BEBOP_TAUNT_SHOT_F   64
#define BEBOP_TAUNT_LOOP_TICKS 8
// --- Anti-trabado (26/09) ---------------------------------------------------
// Gustavo lo trababa a golpes: cada golpe lo mandaba al flinch, al salir del
// flinch quedaba golpeable de nuevo con cooldown 0, y la tortuga encadenaba
// combos hasta tirarlo; se levantaba y vuelta a empezar, sin que el jefe
// pudiera responder nunca. Tres frenos:
//  1. RACHA: aguanta BEBOP_COUNTER_HITS golpes seguidos con flinch; el
//     siguiente lo ABSORBE (le baja la vida igual) y contraataca en el acto
//     con un uppercut CON ARMADURA (no se lo puede interrumpir) que derriba.
//     La racha se corta sola tras BEBOP_COMBO_RESET frames sin recibir golpes.
//  2. AL LEVANTARSE: BEBOP_GETUP_ARMOR frames invulnerable, y si hay una
//     tortuga encima (BEBOP_WAKE_RANGE) se levanta directo con el uppercut
//     con armadura.
//  3. La caida cada BEBOP_KD_INTERVAL golpes se mantiene, pero la racha se
//     reinicia al caer.
#define BEBOP_COUNTER_HITS   99   // (03/10) sin contra por racha: el arcade
                                  // aguanta el combo y cae (BEBOP_KD_COMBO)
#define BEBOP_COMBO_RESET    50
#define BEBOP_GETUP_ARMOR    40
#define BEBOP_WAKE_RANGE    999   // (03/10) siempre se levanta pegando
#define BEBOP_COUNTER_DMG     2   // barras del uppercut de contraataque
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
#define BEBOP_SHOT_GROW        7   // (03/10) frames entre tamanos del aro
#define BEBOP_SHOT_FRAMES      5   // tamanos del aro (bebop_shot_gen)
#define MAX_BEBOP_SHOTS        6   // (03/10) 3 aros por disparo
#define BEBOP_SHOT_W          16   // celda del aro (un aro solo, centrado)
#define BEBOP_SHOT_H          40
// (03/10) Disparo agachado del arcade (video): se para y apunta (frames 0 y 1,
// 6 + 6), se agacha y tira TRES aros, uno cada 12 frames. 40 frames en total.
#define BEBOP_CSHOT_F         40
#define BEBOP_CSHOT_RING1     14
#define BEBOP_CSHOT_RING_GAP  12
#define BEBOP_SHOT_TOL_Y      22   // |dy| de lane para conectar
#define BEBOP_SHOT_TOL_Z      36   // |dz| contra el torso del jugador
#define BEBOP_SHOT_TORSO_Z    36   // altura del torso sobre los pies
#define BEBOP_SHOT_MARGIN     40   // px fuera de pantalla antes de liberarlo

// --- Flash por vida baja (igual que Rocksteady) ------------------------------
// Alterna la paleta normal con una "quemada" (cada canal x2): lento por debajo
// de un tercio de la vida, rapido por debajo de un sexto.
// (03/10) Ritmo del arcade: 4/4 constante desde el ultimo tercio (boss_flash.h).
#define BEBOP_FLASH_HP        (BEBOP_HP / BOSS_FLASH_DIV)
#define BEBOP_FLASH_TICKS     BOSS_FLASH_TICKS

// --- ARENA (26/09) -----------------------------------------------------------
// Con la Scene 4 Bebop dejo de ser solo del 2-1: todo lo que depende del
// escenario (limites, franja caminable y la entrada) vive en una BebopArena.
// bebopSpawn() usa la del 2-1 (las macros de arriba); el garage pasa la suya
// con bebopSpawnArena().
typedef s16 (*BebopLaneFn)(s16 worldX);
typedef struct {
    s16 xMin, xMax;          // CENTRO del cuerpo (mundo)
    s16 laneTop, laneBot;    // tope de seguridad de la franja (pies)
    BebopLaneFn topAt;       // franja real por X (NULL = laneTop/laneBot)
    BebopLaneFn botAt;
    s16 carX, carY;          // primer apoyo de la entrada (centro, pies)
    s16 landX, landY;        // donde apoya y arranca la pelea
    u8  startOnCar;          // TRUE: aparece YA parado en el apoyo (sin caida)
    u16 carHold;             // frames en el apoyo (0 = BEBOP_CAR_HOLD)
    // (03/10) Parpadeo de vida baja: 0 = cambia los colores de PAL3 (el 2-1,
    // donde PAL3 es solo suya). Si no, la LINEA a la que pasa el sprite en la
    // fase "quemada" (el nivel carga ahi la paleta quemada, boss_flash.h):
    // en el garage PAL3 la comparte con Rocksteady y April.
    u8  flashPal;
} BebopArena;

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
    u8         comboHits;     // golpes recibidos SEGUIDOS (ver anti-trabado)
    u8         armored;       // uppercut de contraataque: no se lo puede golpear
    u16        armorTimer;    // invulnerable al levantarse / tras un golpe en la embestida
    s16        fromX, fromY;  // origen de la parabola en curso (entrada)
    s16        cameraOffsetX;
    s16        cameraOffsetY;
    const BebopArena* arena;  // (26/09) escenario de la pelea
    // (03/10) movimiento en Q8 (restos fraccionarios) y timers del arcade
    u8         accX, accY;
    u8         chargeWind;    // frames de amague que le quedan a la embestida
    u16        actT;          // frames dentro del disparo agachado / vitoreo
    s8         slideDir;      // hacia donde desliza al caer
} Bebop;

void bebopInit(Bebop* b);
// Arranca la entrada: cae desde arriba hacia el techo del auto (arena del 2-1).
void bebopSpawn(Bebop* b);
// Lo mismo en otra arena (la estructura tiene que vivir mientras dure la pelea).
void bebopSpawnArena(Bebop* b, const BebopArena* arena);
// Un frame de jefe: IA, animacion, proyectiles y golpes CONTRA los jugadores.
void bebopUpdate(Bebop* b, Player** pls, u8 nPl, s16 camX, s16 camY);
// Golpe del jugador. Devuelve TRUE si el golpe lo mato.
bool bebopDamage(Bebop* b, s16 dmg);
// (03/10) special = el especial de la tortuga: lo DERRIBA (como en el
// arcade). bebopDamage lo deduce del dano (>= BEBOP_SPECIAL_DMG).
bool bebopDamageEx(Bebop* b, s16 dmg, bool special);
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
