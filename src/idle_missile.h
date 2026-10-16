#ifndef _IDLE_MISSILE_H_
#define _IDLE_MISSILE_H_

#include <genesis.h>
#include "player.h"

// ===========================================================================
// MISIL AL QUE SE QUEDA QUIETO (14/10) -- solo en GAME LEVEL = ARCADE
// ===========================================================================
// Si una tortuga se queda QUIETA (parada, sin moverse) y la camara tampoco
// avanza durante IDLE_MISSILE_SECS segundos, cae un misil desde arriba de la
// pantalla justo en su X (el misil de la cloaca, de punta) y al tocar el piso
// explota (la explosion de Baxter): a la que siga ahi le saca la vida entera
// (pierde una vida). Si se movio a tiempo, no le pasa nada.
//
// Uso en cada nivel:
//   idleMissileReset()                      al empezar (antes del bucle)
//   idleMissileUpdate(pls, nPl, camX, camY)  una vez por frame, despues de
//                                            updatePlayer (camY = stageCamY o
//                                            la Y de camara del nivel)
// Los sprites se sueltan solos con el SPR_reset del cambio de escena.
// ===========================================================================
#define IDLE_MISSILE_SECS   7

void idleMissileReset(void);
void idleMissileUpdate(Player** pls, u8 nPl, s16 camX, s16 camY);

#endif
