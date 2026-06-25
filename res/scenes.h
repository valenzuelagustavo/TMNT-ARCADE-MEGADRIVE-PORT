#ifndef _SCENES_H_
#define _SCENES_H_

#include <genesis.h>
#include "resources.h"

// Definición de ID de cada escena para la máquina de estados
typedef enum {
    SCENE_SEGA,
    SCENE_KONAMI,
    SCENE_SGDK,
    SCENE_INTRO_ARCADE,
    SCENE_PLAYER_SELECT,
    SCENE_CHAR_SELECT,
    SCENE_CINEMATIC_FIRE,
    SCENE_LEVEL1_TITLE,
    SCENE_LEVEL1
} SceneId;

// Prototipos de las funciones de cada escena
SceneId showSegaIntro();
SceneId showKonamiIntro();
SceneId showSGDKIntro();
SceneId showArcadeIntro();
SceneId showPlayerSelect();
SceneId showCharSelect();
SceneId showFireCinematic();
SceneId showLevel1Title();
SceneId showLevel1();

// Función auxiliar para limpiar la pantalla entre escenas
void clearScene();

#endif