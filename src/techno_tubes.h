// ===========================================================================
// techno_tubes.h - tubos de spray congelante del TECHNODROME (25/10)
// ===========================================================================
// Arte: tools/gen_techno_tube.py (hoja del arcade "Enemies - Robot Enemies"),
// res/level8_1_far.res; PAL3 = paleta del foot soldier blanco (la del nivel).
//
// (26/10) Sin foot soldiers en esta parte del pasillo: PAL2 queda libre para la
// paleta del hielo. Salen en cualquier lugar visible de la zona y miran a
// izquierda o derecha al azar; se destruyen de un golpe.
//
// Sale del piso un tubo (disco, se abre, sube), apunta unos cuadros y tira el
// spray hacia donde estan las tortugas (2 rafagas con una pausa en el medio),
// y se vuelve a hundir. La tortuga que toca el spray pierde 2 barras y queda
// CONGELADA (playerFreezeHit, ANIM_FROZEN). Se destruye de un golpe.
//
// Salen en la zona del piso con los triangulos (TT_X_MIN..TT_X_MAX, entre las
// columnas con laser y el ascensor), de a uno cada tanto, al frente de las
// tortugas; hay tiempo para esquivarlo: cuando sale el disco faltan ~1 s hasta
// el primer chorro, que solo llega a TT_REACH px y TT_DY de lane; saltando
// tambien se esquiva (en el aire no se la puede golpear).
// ===========================================================================
#ifndef _TECHNO_TUBES_H_
#define _TECHNO_TUBES_H_

#include <genesis.h>
#include "player.h"

#define TT_X_MIN      1100    // zona de spawn (mundo): los triangulos del piso
#define TT_X_MAX      1370
#define TT_ZONE_X0    1040    // los tubos salen mientras la tortuga lider este entre estos
#define TT_ZONE_X1    1390

// 'spawn' = TRUE mientras pueden salir tubos nuevos (los que ya salieron
// terminan). leadFeetX: X de pies de la tortuga que va mas adelante.
void ttInit(void);
void ttUpdate(Player** pls, u8 nPl, s16 camX, s16 camY, s16 laneT, s16 laneB,
              s16 leadFeetX, bool spawn);
void ttRelease(void);
// TRUE mientras queda algun tubo en pantalla (el nivel no suelta la oleada
// siguiente hasta que no queda ninguno: comparten la paleta PAL2 con el hielo).
bool ttBusy(void);

#endif
