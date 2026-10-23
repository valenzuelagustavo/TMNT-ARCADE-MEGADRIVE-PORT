#ifndef _HISCORE_H_
#define _HISCORE_H_

#include <genesis.h>
#include "scenes.h"

// (20/10) Carga de iniciales y tabla "BEST 10 PLAYERS" (hiscore.c), guardada
// en la SRAM del cartucho.
void    hiscoreLoad(void);
SceneId showHiscoreEntry(void);   // -> SCENE_HISCORE_BOARD
SceneId showHiscoreBoard(void);   // -> SCENE_SEGA

#endif
