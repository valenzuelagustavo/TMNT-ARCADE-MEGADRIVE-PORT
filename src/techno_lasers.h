// ===========================================================================
// techno_lasers.h - las tres barras LASER de la puerta del Technodrome (30/10)
// ===========================================================================
// En el arcade, apenas la puerta sale volando aparecen en el hueco tres barras
// amarillas que titilan: si la tortuga las toca se electrocuta (ANIM_ELECTRO,
// playerElectroHit). Arte: tools/gen_techno_laser.py (techno_laser_bar, en
// PAL1 con los colores de las tortugas). Posiciones medidas de las capturas
// (tools/electro_src/arcade_laser_0.png, pantalla del port = arcade + (10,-17),
// mundo = (4352 + sx, 2592 + sy) con la camara al maximo).
//
// Cada barra es un tubo de 4 px de ancho y TL_BODY de alto; la B va TL_STEP a la
// derecha y TL_STEP mas abajo que la A, y la C otro tanto (siguen la
// perspectiva del hueco). La base de cada barra es su LANE: la tortuga se
// electrocuta si esta sobre esa lane (TL_HIT_DY) y a TL_HIT_DX de la barra.
// Solo pega mientras la barra esta ENCENDIDA (titilan: TL_ON_FRAMES prendidas,
// TL_OFF_FRAMES apagadas).
// ===========================================================================
#ifndef _TECHNO_LASERS_H_
#define _TECHNO_LASERS_H_

#include <genesis.h>
#include "player.h"

#define TL_N          3
#define TL_X0         4586     // borde izquierdo de la barra A (mundo)
#define TL_TOP0       2624     // tope de la barra A (mundo)
#define TL_STEP         12     // corrimiento de una barra a la siguiente (x e y)
#define TL_BODY         96     // alto del tubo; la base (la lane) = tope + TL_BODY

#define TL_ON_FRAMES    12     // (estimado, no se ve en las capturas)
#define TL_OFF_FRAMES    6

#define TL_HIT_DX       14     // media tortuga (hurtbox ~12) + media barra (2)
#define TL_HIT_DY        8     // tolerancia de lane
#define TL_DMG           2     // barras de vida por descarga

// Crea los sprites (si falta VRAM, la barra que no entra no existe ni pega).
void tlStart(void);
// Una vez por cuadro: titileo, posicion en pantalla y descarga a las tortugas.
void tlUpdate(Player** pls, u8 nPl, s16 camX, s16 camY);
// Libera los sprites (fin de la escena).
void tlEnd(void);
// TRUE entre tlStart y tlEnd.
bool tlActive(void);

#endif
