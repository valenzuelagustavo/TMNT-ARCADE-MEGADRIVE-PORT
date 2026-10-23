#ifndef _HISCORE_H_
#define _HISCORE_H_

#include <genesis.h>
#include "scenes.h"

// (20/10) Carga de iniciales y tabla "BEST 10 PLAYERS" (hiscore.c), guardada
// en la SRAM del cartucho.
void    hiscoreLoad(void);
SceneId showHiscoreEntry(void);   // -> SCENE_HISCORE_BOARD
SceneId showHiscoreBoard(void);   // -> SCENE_SEGA (o SCENE_PLAYER_SELECT en atracto)
// (22/10) Modo ATRACTO: los perfiles de las tortugas (scene_profiles.c), si
// terminan solos, lo ponen en TRUE antes de ir a la tabla. Entonces la tabla
// vuelve a la seleccion de jugadores (no a SEGA) y la corta cualquier boton
// de los dos joysticks. La tabla lo vuelve a FALSE al salir.
extern bool hiscoreBoardAttract;

#endif
