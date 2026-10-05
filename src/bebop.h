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
// ENTRADA (24/09, sobre la captura marcada)
// Cae desde ARRIBA de la pantalla en diagonal hacia la derecha, apoya un pie
// en el TECHO DEL AUTO del fondo, se sostiene ahi un instante y pega el
// segundo salto hasta la calle, delante del auto. Recien ahi empieza la
// pelea. Las dos parabolas son puramente visuales (no hay clamp de area
// caminable durante la entrada).
//
// CONDUCTA (05/10, rehecha entera)
// Cada estado dura lo que dura su animacion y al terminar elige el siguiente:
//   QUIETO (30)    si no esta alineado con la tortuga: 50% la persigue, 50%
//                  embiste. Alineado y cerca: la persigue. Alineado y lejos:
//                  1/3 dispara, 1/3 camina, 1/3 sigue quieto.
//   CAMINAR (72)   la persigue (lane exacta, hasta BEBOP_NEAR_DX). Alineado y
//                  cerca -> golpe; alineado y lejos -> dispara o se queda.
//   EMBESTIDA      amague (40) y corrida (120) a 4,17 px/f siguiendo la lane;
//                  pega una vez por contacto y voltea.
//   GOLPE (21)     5 frames; desde el frame 3 pega por contacto y voltea.
//   DISPARO (42)   se para, apunta, se agacha y tira TRES aros (uno cada 6).
//   GOLPEADO (20)  con 4 golpes seguidos contraataca (golpe); con 10, cae.
//   CAIDA (36)     desliza hacia atras, se levanta (8) y dispara.
// Solo recibe dano quieto, caminando y disparando. En la EMBESTIDA (amague y
// corrida) los golpes le sacan vida pero no la cortan.
//   MUERTE         cae, se queda tirado y desaparece -> BEBOP_GONE, que es lo
//                  que termina el nivel.
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

// --- Vida y dano ---
// Golpe comun 1, patada en salto 2, especial BEBOP_SPECIAL_DMG.
#define BEBOP_HP             40
#define BEBOP_SPECIAL_DMG     5
#define BEBOP_JUMPKICK_DMG    2
#define BEBOP_CONTACT_DMG     1   // embestida y golpe (voltean)
#define BEBOP_SHOT_DMG        1   // cada aro

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

// --- Duraciones (frames) ---
#define BEBOP_IDLE_T         30
#define BEBOP_WALK_T         72
#define BEBOP_WINDUP_T       40
#define BEBOP_CHARGE_T      120
#define BEBOP_UPPER_T        21   // 5 frames a 14 fps
#define BEBOP_SHOOT_T        42   // frames de 7, 7, 15 y 13
#define BEBOP_HURT_T         20   // 2 frames de 10
#define BEBOP_KD_SLIDE_F     12   // caida: sentado deslizando ...
#define BEBOP_KD_T           36   // ... y arrodillado hasta 36
#define BEBOP_GETUP_T         8
#define BEBOP_DEAD_HOLD     120   // tirado antes de desaparecer
#define BEBOP_WALK_TICKS      6   // ticks por frame de la caminata
#define BEBOP_CHARGE_TICKS    4   // ticks por frame de la embestida (orden 0,1,3,2)

// --- Velocidades (Q8: 256 = 1 px por frame) ---
#define BEBOP_WALK_Q        427   // 1,67 px/f
#define BEBOP_CHARGE_Q     1067   // 4,17 px/f
#define BEBOP_SLIDE_Q       853   // 3,33 px/f al caer

// --- Distancias (centro a centro) ---
#define BEBOP_ALIGN_Y         5   // "alineado": lane a +-5 px
#define BEBOP_ALIGN_X        37   // "cerca": a esta distancia o menos
#define BEBOP_NEAR_DX        31   // persiguiendo se acerca hasta aca
#define BEBOP_HIT_DY_UP       5   // golpes por contacto: lane de la tortuga
#define BEBOP_HIT_DY_DOWN     7   // de -5 a +7 respecto de la suya
#define BEBOP_UPPER_FWD      48   // alcance del golpe hacia adelante
#define BEBOP_UPPER_BACK     16   // ... y hacia atras
#define BEBOP_UPPER_FR_HIT    3   // desde este frame pega
#define BEBOP_CHARGE_DX      34   // la embestida pega a menos de esto
#define BEBOP_EDGE_NEAR      24   // "cerca del borde izquierdo" al terminar de embestir

// --- Contadores ---
#define BEBOP_COUNTER_HITS    4   // golpes seguidos -> contraataque
#define BEBOP_KD_HITS        10   // golpes -> caida (se reinicia al caer)

// EMBESTIDA CON ARMADURA: en el amague y la corrida los golpes le sacan vida
// pero NO la cortan. Tras cada golpe queda intocable estos frames: uno por
// swing; el especial, uno solo.
#define BEBOP_CHARGE_HIT_CD      18
#define BEBOP_CHARGE_HIT_CD_SP   40

// --- Disparo ---------------------------------------------------------------
// Boca del arma MEDIDA sobre la grilla generada (celda 112x120, pies abajo):
//   parado   punta en (108, fila 58)  -> dx = 108-56 = 52, z = 120-58 = 62
//   agachado punta en (111, fila 75)  -> dx = 55,          z = 45
#define BEBOP_MUZZLE_STAND_X   52
#define BEBOP_MUZZLE_STAND_Z   62
#define BEBOP_MUZZLE_CROUCH_X  55
#define BEBOP_MUZZLE_CROUCH_Z  45
#define BEBOP_SHOT_Q          960  // 3,75 px/f cada aro
#define BEBOP_SHOT_GROW        7   // frames entre tamanos del aro
#define BEBOP_SHOT_FRAMES      5   // tamanos del aro (bebop_shot_gen)
#define MAX_BEBOP_SHOTS        6   // 3 aros por disparo
#define BEBOP_SHOT_W          16   // celda del aro (un aro solo, centrado)
#define BEBOP_SHOT_H          40
#define BEBOP_SHOT_RING1      14   // primer aro (al agacharse) ...
#define BEBOP_SHOT_RING_GAP    6   // ... y uno cada 6 frames
#define BEBOP_SHOT_RINGS       3
#define BEBOP_SHOT_TOL_Y       3   // |dy| de lane para conectar
#define BEBOP_SHOT_TOL_Z      36   // |dz| contra el torso del jugador
#define BEBOP_SHOT_TORSO_Z    36   // altura del torso sobre los pies
#define BEBOP_SHOT_MARGIN     40   // px fuera de pantalla antes de liberarlo

// --- Flash por vida baja (igual que Rocksteady) ------------------------------
// Alterna la paleta normal con una "quemada" (cada canal x2), 4/4 constante
// desde el ultimo cuarto de la vida (boss_flash.h).
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
    BEBOP_WALK,
    BEBOP_WINDUP,      // amague de la embestida
    BEBOP_CHARGE,
    BEBOP_UPPER,       // golpe (tambien el contraataque)
    BEBOP_SHOOT,
    BEBOP_HURT,
    BEBOP_DOWN,        // cae y queda arrodillado
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
    u16        timer;         // entrada: cuenta hacia abajo; pelea: frames en el estado
    u8         frameTick;     // contador para los frames manejados a mano
    u8         frame;
    u8         chargeHit;     // ya conecto esta embestida
    u8         combo;         // golpes seguidos (contraataque)
    u8         kdHits;        // golpes para la caida
    u8         flashTick;
    u8         flashOn;
    u16        armorTimer;    // intocable tras un golpe en la embestida
    s16        fromX, fromY;  // origen de la parabola en curso (entrada)
    s16        cameraOffsetX;
    s16        cameraOffsetY;
    const BebopArena* arena;  // escenario de la pelea
    u8         accX, accY;    // restos Q8 del movimiento
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
// special = el especial de la tortuga: suma un golpe extra a los contadores
// de contraataque y de caida. bebopDamage lo deduce del dano
// (>= BEBOP_SPECIAL_DMG).
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
