#ifndef _SCENES_H_
#define _SCENES_H_

#include <genesis.h>
#include "resources.h"

// Definición de ID de cada escena para la máquina de estados
typedef enum {
    SCENE_SEGA,
    SCENE_KONAMI,
    SCENE_SGDK,
    SCENE_CREDITS,      // Creditos: reconocimiento al adaptador musical
    SCENE_INTRO_ARCADE,
    SCENE_VRAM_CLEAR,   // Buffer: borrado total de VRAM entre la intro y los menus
    SCENE_PLAYER_SELECT,
    SCENE_OPTIONS,      // Opciones: VIDAS (3/5/7), SOUNDTEST y SALIR
    SCENE_CHAR_SELECT,
    SCENE_CINEMATIC_FIRE,
    SCENE_LEVEL1_TITLE,
    SCENE_LEVEL1,
    SCENE_LEVEL2,       // Nivel 2: pasillo en llamas, 2da parte (sala cerrada)
    SCENE_ENDING,       // Cutscene final: Shredder rapta a April (BG_A + BG_B)
    SCENE_GAME_OVER
} SceneId;

// Prototipos de las funciones de cada escena
SceneId showSegaIntro();
SceneId showKonamiIntro();
SceneId showSGDKIntro();
SceneId showCredits();
SceneId showArcadeIntro();
SceneId showVramClear();
SceneId showPlayerSelect();
SceneId showOptions();
SceneId showCharSelect();
SceneId showFireCinematic();
SceneId showLevel1Title();
SceneId showLevel1();
SceneId showLevel2();
SceneId showEnding();
SceneId showGameOver();

// Función auxiliar para limpiar la pantalla entre escenas.
// keepAudio = TRUE: no detiene la música (para transiciones con música continua).
void clearSceneEx(bool keepAudio);
#define clearScene() clearSceneEx(FALSE)

#endif