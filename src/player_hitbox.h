// GENERADO POR tools/gen_player_hitbox.py -- NO EDITAR A MANO
#ifndef _PLAYER_HITBOX_H_
#define _PLAYER_HITBOX_H_

#include <genesis.h>

// ---------------------------------------------------------------------------
// Hitbox de los ataques, medido sobre los pixeles OPACOS del arte
// ---------------------------------------------------------------------------
// Para cada personaje, animacion de ataque, frame y FRANJA HORIZONTAL de
// PHB_BAND px: hasta donde llega el pixel opaco mas a la derecha, en offset
// desde el CENTRO de la celda de 104 x 104. PHB_NONE = esa franja esta vacia.
//
// El arte de las 4 sheets mira a la derecha y el motor espeja el sprite al
// mirar a la izquierda, asi que el mismo numero vale para los dos lados.
//
// La franja 0 es la de ARRIBA del frame. Para saber a que altura de pantalla
// cae hay que partir del tope del frame dibujado, que es
// (p->y - PLAYER_FOOT_OFFSET - p->jumpZ): por eso saltar sube las franjas y
// una patada en el aire deja de tocar al enemigo que quedo abajo.
//
// Personaje: mismo orden que initPlayer() -- 0=Leo 1=Mike 2=Don 3=Raph.
// ---------------------------------------------------------------------------
#define PHB_CHARS   4
#define PHB_SLOTS   6      // animaciones que pueden golpear
#define PHB_FRAMES  12
#define PHB_BANDS   13
#define PHB_BAND    8      // px de alto de cada franja
#define PHB_ANIMS   21     // filas de PlayerAnim
#define PHB_NONE    (-128)  // franja sin un solo pixel opaco

extern const s8 phbSlotOfAnim[PHB_ANIMS];
extern const s8 playerAtkReach[PHB_CHARS][PHB_SLOTS][PHB_FRAMES][PHB_BANDS];
extern const s8 playerAtkReachMax[PHB_CHARS][PHB_SLOTS][PHB_FRAMES];

#endif
