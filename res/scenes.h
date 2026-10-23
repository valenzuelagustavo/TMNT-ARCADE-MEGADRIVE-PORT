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
    SCENE_OPTIONS,      // Opciones (10/10): GAME LEVEL, PLAYER REST, CONTINUE, CONTROL PAD, SOUND TEST, EXIT
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
    // (18/10) ORDEN DEL ARCADE + los dos niveles del port de NES. Los IDs de
    // los niveles quedan con su numero INTERNO de siempre (el modulo levelN_1.c
    // que los dibuja); lo que cambio es el recorrido y los titulos:
    //   SCENE 2:  2-1 = SCENE_2_1 (la calle)      2-2 = SCENE_3_1 (la cloaca)
    //   SCENE 3:  3-1 = SCENE_SNOW (NES, falta)   3-2 = SCENE_4_1 (el garage)
    //   SCENE 4:  4-1 = SCENE_5_1 (la autopista)  4-2 = SCENE_6_1 (el skate)
    //   SCENE 5:  5-1 = SCENE_7_1 (la fabrica)    5-2 = SCENE_DOJO (NES, falta)
    //   SCENE 6:  6-1 = SCENE_8_1 (Technodrome)   6-2 = SCENE_9_1 (Krang y Shredder)
    // Entre los dos niveles de una SCENE no hay titulo. Los SCENE_CUT_* son
    // las pantallas intermedias; mientras no llegue su arte son una pantalla
    // provisoria con texto (showPendingScreen, scenes.c).
    SCENE_2_1,          // Nivel 2-1: la calle (recorrido en L)
    SCENE_3_1,          // Nivel 2-2: la cloaca (sewer)
    SCENE_CUT_SHREDDER_APRIL, // Shredder con April raptada y un enemigo al costado
    SCENE_S3_TITLE,     // "SCENE 3 / LET'S MELT SNOW AND REBUILD N.Y. CITY"
    SCENE_SNOW,         // Nivel 3-1: Snow in N.Y. (del port de NES; provisorio)
    SCENE_CUT_CITY,     // La ciudad vuelve a la normalidad
    SCENE_4_1,          // Nivel 3-2: el estacionamiento (garage), jefe Bebop
    SCENE_S4_TITLE,     // "SCENE 4 / LET'S GET TO THAT SECRET FACTORY!!"
    SCENE_5_1,          // Nivel 4-1: la autopista (freeway)
    SCENE_6_1,          // Nivel 4-2: el skate
    SCENE_CUT_VAN,      // Descarrila la camioneta de las tortugas
    SCENE_S5_TITLE,     // "SCENE 5 / C'MON, LET'S BUST THIS JOINT!!"
    SCENE_7_1,          // Nivel 5-1: la fabrica, jefe Granitor
    SCENE_CUT_SPLINTER, // Splinter en primer plano hablando
    SCENE_DOJO,         // Nivel 5-2: el dojo (del port de NES; provisorio)
    SCENE_CUT_TECHNODROME, // El Technodrome y las tortugas saltando hacia el
    SCENE_S6_TITLE,     // "SCENE 6 / WE GOTTA FIND OUT THE TECHNODROME!!"
    SCENE_8_1,          // Nivel 6-1: el Technodrome (ascensor), jefe Traag
    SCENE_9_1,          // Nivel 6-2: la sala del portal, Krang y Shredder
    SCENE_EPILOGUE,     // (18/10) Texto del epilogo (ending.c)
    SCENE_STAFF_CREDITS,// (18/10) Creditos con los cuadros de las tortugas (ending.c)
    SCENE_GAME_OVER,
    SCENE_HISCORE_ENTRY,// (20/10) carga de iniciales (hiscore.c)
    SCENE_HISCORE_BOARD // (20/10) "BEST 10 PLAYERS"
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
SceneId showScene31();
SceneId showScene41();
SceneId showScene51();
SceneId showScene61();
SceneId showScene71();
SceneId showScene81();
SceneId showScene91();
SceneId showSceneSnow();   // (21/10) 3-1 Central Park con nieve (level_snow.c)
// (18/10) Recorrido nuevo: titulos de SCENE 3..6 y pantallas intermedias /
// niveles que todavia no existen (scenes.c), epilogo y creditos (ending.c).
SceneId showSceneS3Title();
SceneId showSceneS4Title();
SceneId showSceneS5Title();
SceneId showSceneS6Title();
SceneId showPendingScene(SceneId id);
SceneId showEpilogue();
SceneId showStaffCredits();
SceneId showGameOver();

// Arranca una pista con el volumen dado (XGM2 permite regular volumen en vivo).
// Vive en scenes.c y la usan tambien los modulos de escena sueltos.
void playMusicVol(const u8* track, u16 vol);

// Función auxiliar para limpiar la pantalla entre escenas.
// keepAudio = TRUE: no detiene la música (para transiciones con música continua).
void clearSceneEx(bool keepAudio);
#define clearScene() clearSceneEx(FALSE)

#endif