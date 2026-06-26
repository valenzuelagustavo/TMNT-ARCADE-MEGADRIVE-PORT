#include <genesis.h>
#include "scenes.h"
#include "resources.h"

int main()
{
    // Inicializar motor de sprites
    SPR_init();

    SceneId currentScene = SCENE_SEGA; // Empezamos por Sega

    while (1)
    {
        switch (currentScene)
        {
        case SCENE_SEGA:
            currentScene = showSegaIntro();
            break;
        case SCENE_KONAMI:
            currentScene = showKonamiIntro();
            break;
        case SCENE_SGDK:
            currentScene = showSGDKIntro();
            break;
        case SCENE_INTRO_ARCADE:
            currentScene = showArcadeIntro();
            break;
        case SCENE_PLAYER_SELECT:
            currentScene = showPlayerSelect();
            break;
        case SCENE_CHAR_SELECT:
            currentScene = showCharSelect(); // Esta debe retornar SCENE_LEVEL1_TITLE
            break;
        case SCENE_LEVEL1_TITLE:
            currentScene = showLevel1Title();
            break;
        case SCENE_LEVEL1:
            currentScene = showLevel1();
            break;
        // ... agregar el resto de casos ...
        default:
            currentScene = SCENE_SEGA; // Por seguridad
            break;
        }

        // El bucle principal siempre debe llamar a esto
        SYS_doVBlankProcess();
    }
    return 0;
}