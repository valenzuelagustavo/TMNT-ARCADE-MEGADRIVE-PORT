// ===========================================================================
// factory_fx.h - actores y peligros de THE FACTORY (Scene 5-1) (18/10)
// ===========================================================================
// Segun el video del arcade (seg_05):
//   - LASERS de los emisores "NFP-1" de la pared: una linea vertical rosa
//     que titila (cada frame gruesa / finita) desde el emisor hasta abajo de
//     la pantalla; se prenden un rato, se recogen y se apagan, cada emisor a
//     su ritmo. Pegan a la tortuga que este en esa X (a cualquier
//     profundidad: el laser cruza todo el piso).
//   - La CAPSULA DEL TALADRO (la del 1-2, en la paleta del fondo): sale del
//     piso temblando la pantalla, abre la puerta y suelta ABEJAS (robots con
//     helice, plantilla "...Enemies_abelhas.png", PAL3 = paleta del blanco).
//     Las abejas rodean a la tortuga a media altura y le tiran rayitos
//     rosas; 2 golpes las rompen (naranja al golpe, explosion del Mouser).
// ===========================================================================
#ifndef _FACTORY_FX_H_
#define _FACTORY_FX_H_

#include <genesis.h>
#include "player.h"

void ffxInit(void);
// Lasers de la pared (siempre: cada emisor trabaja mientras esta en pantalla).
void ffxLasersUpdate(Player** pls, u8 nPl, s16 camX);
// Arranca la capsula con su centro en x y el borde de abajo en baseY (mundo),
// con 'bees' abejas adentro. FALSE si ya hay una o no hay VRAM (se reintenta).
bool ffxCapsuleStart(s16 x, s16 baseY, u8 bees);
// Capsula + abejas + sus tiros.
void ffxUpdate(Player** pls, u8 nPl, s16 camX, s16 laneTop, s16 laneBot);
// Cuantos actores cuentan como vivos (capsula todavia soltando + abejas).
u16  ffxAlive(void);
void ffxRelease(void);

#endif
