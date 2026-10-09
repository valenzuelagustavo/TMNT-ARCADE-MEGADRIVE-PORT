// GENERADO POR tools/gen_players_new.py -- NO EDITAR A MANO
#ifndef _PLAYER_OFFSETS_H_
#define _PLAYER_OFFSETS_H_

#include <genesis.h>

// Desplazamiento en X de cada frame de las tortugas (px, mirando a la
// DERECHA; espejado se resta). Deja el cuerpo quieto aunque el rip haya
// corrido el dibujo para que entre el arma. Personaje: 0=Leo 1=Mike 2=Don
// 3=Raph (Mike y Raph, hojas viejas: todo 0).
#define PDX_CHARS   4
#define PDX_ANIMS   24
#define PDX_FRAMES  12

extern const s8 playerFrameDXTab[PDX_CHARS][PDX_ANIMS][PDX_FRAMES];
// Cuanto SUBIR cada frame de ANIM_JUMP (px) para que la vuelta en el aire quede
// a la altura que tiene en el arcade (el rip apoya todos en el piso).
extern const s8 playerJumpDYTab[PDX_CHARS][PDX_FRAMES];

#endif
