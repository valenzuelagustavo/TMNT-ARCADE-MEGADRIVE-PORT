#ifndef _METERS_2_1_H_
#define _METERS_2_1_H_

#include <genesis.h>
#include "props_2_1.h"   // parking_meter_stand, parking_meter_fly (SPRITE)
#include "player.h"

// ===========================================================================
// PARQUIMETROS del nivel 2-1 (25/09)
// ===========================================================================
// Cinco parquimetros sobre la vereda de arriba, en las X que marco Gustavo
// sobre el mapa. Son OBJETOS FISICOS:
//
//   PARADOS  no se los puede atravesar caminando: su base es una caja chica
//            (METER_BLOCK_*) que ni las tortugas ni los foot soldiers pueden
//            pisar. Es fina en profundidad, asi que se los puede rodear y
//            pasar por DETRAS (el sprite se ordena por lane como todo lo
//            demas). Saltando se los pasa por arriba.
//   GOLPEADOS  cualquier golpe de una tortuga los arranca: frame "recibe"
//            un instante y despues salen VOLANDO en X en la direccion del
//            golpe, a la velocidad de la tapa de alcantarilla, hasta salir de
//            camara. En su paso MATAN a los foot soldiers que tocan.
//
// Sprites (ver res/props_2_1.res): parados usan parking_meter_stand (12
// tiles) y solo el que vuela cambia a parking_meter_fly (21 tiles). Se crean
// al acercarse a la camara y se liberan al alejarse, igual que las tapas
// cerradas de las bocas de tormenta. PAL0: comparten la paleta del fondo.
// ===========================================================================

#define METERS_COUNT          5
#define METER_FOOT_OFFSET    64   // pies = borde inferior de la celda
#define METER_STAND_PX        8   // X del palo dentro de parking_meter_stand
#define METER_FLY_W          56   // celda de parking_meter_fly
#define METER_FLY_PX         26   // X del palo dentro de parking_meter_fly

// Caja de la BASE contra la que chocan los pies (centro del parquimetro, lane).
// El palo opaco mide 5 px; la media anchura suma la de las piernas del que
// camina, asi el cuerpo no queda medio metido adentro del palo antes de frenar.
#define METER_BLOCK_HALF_W   12
#define METER_BLOCK_HALF_D    5   // profundidad: fina, se lo rodea por delante o por detras

// Golpe de la tortuga: hurtbox del palo.
#define METER_HIT_HALF_W      6
#define METER_BODY_H         60

#define METER_HIT_TICKS      10   // frames mostrando "recibe el golpe"
#define METER_SPEED           5   // px/frame volando (= LID_SPEED)
#define METER_FLY_HALF_W     20   // caja del que vuela contra los soldiers
#define METER_FLY_TOL_Y      20   // tolerancia de lane (la misma de la tapa)
#define METER_ARM_MARGIN     64   // crear el sprite a esta distancia de pantalla
#define METER_MARGIN         40   // volando: px fuera de pantalla antes de soltarlo

void metersInit(void);
// Sprites cerca de camara, vuelo y dibujo. Llamar una vez por frame.
void metersUpdate(s16 camX, s16 camY);
// TRUE si los pies (fx, fy) caen en la base de un parquimetro PARADO.
bool metersBlock(s16 fx, s16 fy);
// Golpes de las tortugas: el que conecta arranca el parquimetro. Devuelve
// TRUE si arranco alguno este frame (para el SFX).
bool metersPlayerHits(Player** pls, u8 nPl);
// TRUE si un parquimetro VOLANDO alcanza a este enemigo. Cada parquimetro
// golpea a cada enemigo una sola vez. En owner sale el jugador que lo golpeo.
bool metersHitEnemy(u16 enemyIdx, s16 ex, s16 ey, s16 halfW, s8* owner);
void metersReleaseAll(void);

#endif
