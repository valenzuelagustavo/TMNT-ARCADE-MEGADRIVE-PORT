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
    SCENE_PROFILES,     // Modo atracto: perfil de una tortuga al azar (30s sin tocar nada)
    SCENE_OPTIONS,      // Opciones: VIDAS (3/5/7), SOUNDTEST y SALIR
    SCENE_CHAR_SELECT,
    SCENE_CINEMATIC_FIRE,
    // Nomenclatura del ARCADE, no del orden interno: la "Scene 1" del arcade
    // son DOS niveles (1-1 la calle en llamas, 1-2 el pasillo/sala de April),
    // y la "Scene 2" es la calle a la que se sale persiguiendo a Shredder.
    SCENE_1_1_TITLE,    // "SCENE 1 / FIRE! WE GOTTA GET APRIL OUT!!"
    SCENE_1_1,          // Nivel 1-1: la calle en llamas
    SCENE_1_2,          // Nivel 1-2: pasillo en llamas, sala cerrada (Rocksteady)
    SCENE_ENDING,       // Cutscene: Shredder rapta a April y sale por la ventana
    SCENE_2_1_TITLE,    // "SCENE 2 / C'MON, AFTER THAT SHREDDER CREEP!!"
    SCENE_2_1,          // Nivel 2-1: la calle (recorrido en L)
    SCENE_3_1_TITLE,    // (26/09) "SCENE 3"
    SCENE_3_1,          // Nivel 3-1: la cloaca (sewer), arte del proyecto de Ray
    SCENE_4_1_TITLE,    // (26/09) "SCENE 4"
    SCENE_4_1,          // Nivel 4-1: el estacionamiento (garage), jefe Bebop
    SCENE_5_1_TITLE,    // (26/09) "SCENE 5"
    SCENE_5_1,          // Nivel 5-1: la autopista (freeway), skyline con parallax
    SCENE_6_1_TITLE,    // (26/09) "SCENE 6"
    SCENE_6_1,          // Nivel 6-1: la segunda autopista (skate, por ahora a pie)
    SCENE_7_1_TITLE,    // (26/09) "SCENE 7"
    SCENE_7_1,          // Nivel 7-1: la fabrica, jefe Granitor
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
SceneId showProfiles();
SceneId showOptions();
SceneId showCharSelect();
SceneId showFireCinematic();
SceneId showScene11Title();
SceneId showScene11();
SceneId showScene12();
SceneId showEnding();
SceneId showScene21Title();
SceneId showScene21();
SceneId showScene31Title();
SceneId showScene31();
SceneId showScene41Title();
SceneId showScene41();
SceneId showScene51Title();
SceneId showScene51();
SceneId showScene61Title();
SceneId showScene61();
SceneId showScene71Title();
SceneId showScene71();
SceneId showGameOver();

// Arranca una pista con el volumen dado (XGM2 permite regular volumen en vivo).
// Vive en scenes.c y la usan tambien los modulos de escena sueltos.
void playMusicVol(const u8* track, u16 vol);

// Función auxiliar para limpiar la pantalla entre escenas.
// keepAudio = TRUE: no detiene la música (para transiciones con música continua).
void clearSceneEx(bool keepAudio);
#define clearScene() clearSceneEx(FALSE)

#endif