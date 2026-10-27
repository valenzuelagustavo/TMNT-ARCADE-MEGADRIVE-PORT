// ===========================================================================
// frosty.h - FROSTY, el muneco de nieve robot de CENTRAL PARK (27/10)
// ===========================================================================
// Arte: tools/gen_frosty.py (desde frosty-sheet.png), res/level_snow_far.res.
// PAL3 = la paleta de Frosty (frostyStart la carga).
//
// Es una oleada propia del nivel (SP_FROSTY): la camara queda clavada hasta
// que mueren todos. Con 1 jugador entran 2, los dos por la derecha; con mas
// de uno entran 3: dos por la derecha y uno por la izquierda.
//
//  1) MUNECO DE NIEVE: no ataca, solo salta (acercandose a las tortugas).
//     Aguanta 2 golpes (cada uno lo tira un poco hacia atras).
//  2) Al segundo golpe se rompe el disfraz (3 cuadros) y queda el ROBOT.
//     Aguanta 2 golpes mas; el segundo lo deja tirado y explota (la explosion
//     de Baxter).
//  3) El robot, cada tanto, lanza su NARIZ como misil: sale en arco y despues
//     persigue a una tortuga girando de a poco (teledirigido) hasta darle o
//     hasta que se le acaba el tiempo. El robot queda sin nariz un rato y la
//     regenera.
// Cada golpe suena como el de Baxter.
// ===========================================================================
#ifndef _FROSTY_H_
#define _FROSTY_H_

#include <genesis.h>
#include "player.h"

void frostyInit(void);
// Arranca la oleada (FALSE = todavia no: con soldiers vivos se reintenta).
// laneMax: tope de profundidad (y) donde pueden caminar.
bool frostyStart(s16 camX, u8 nPlayers, s16 laneMax);
void frostyUpdate(Player** pls, u8 nPl, s16 camX);
// TRUE mientras queda algo: Frosties por entrar o vivos, misiles o explosiones.
bool frostyActive(void);
void frostyRelease(void);

#endif
