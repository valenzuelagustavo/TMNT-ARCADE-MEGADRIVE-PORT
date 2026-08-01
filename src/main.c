#include <genesis.h>
#include "scenes.h"
#include "resources.h"

int main()
{
    // Inicializar motor de sprites con presupuesto de VRAM ampliado.
    // El default de SPR_init() son 420 tiles: queda corto para el nivel 1
    // (2 tortugas + 4 foot soldiers de 104x104 ≈ 540 tiles en el peor caso).
    // 720 deja margen para los marcos del HUD como sprites (2x 72x32, hasta
    // 72 tiles en 2 jugadores) y la bola de hierro, sin chocar con el área de
    // usuario (fondo ~495 + fuego 64).
    SPR_initEx(720);

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
        case SCENE_LEVEL2:
            currentScene = showLevel2();
            break;
        case SCENE_ENDING:
            currentScene = showEnding();
            break;
        case SCENE_GAME_OVER:
            currentScene = showGameOver();
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
