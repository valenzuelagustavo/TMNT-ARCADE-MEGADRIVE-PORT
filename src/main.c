#include <genesis.h>
#include "scenes.h"
#include "hiscore.h"     // (20/10) iniciales y tabla de puntajes
#include "resources.h"

int main()
{
    // Inicializar motor de sprites con presupuesto de VRAM ampliado.
    // El default de SPR_init() son 420 tiles: queda corto para el nivel 1
    // (2 tortugas + 4 foot soldiers de 104x104 ≈ 540 tiles en el peor caso).
    // 752 deja margen para los marcos del HUD como sprites (2x 72x32, hasta
    // 72 tiles en 2 jugadores), los retratos de tortuga (32x32, 16 tiles) y
    // la bola de hierro, sin chocar con el área de usuario (fondo ~495 +
    // fuego 64).
    SPR_initEx(752);

    // --- Multitap (14/09) ---------------------------------------------------
    // El modo secreto de 4 tortugas necesita JOY_3/JOY_4, que solo existen con
    // un TeamPlayer / Sega Tap conectado. NO hace falta declararlo a mano: el
    // JOY_init de SGDK ya sondea los dos puertos y, si encuentra un teamplayer
    // y ningun pad directo, llama solo a JOY_setSupport(..., TEAMPLAYER) (ver
    // src/joy.c de SGDK v2.11, alrededor de la linea 240).
    // PROBADO: volver a llamarlo desde aca REVIENTA la consola en el arranque
    // (excepcion "LINE 1010 EMULATOR" apenas bootea, reproducible en mednafen
    // con -md.input.multitap tp1). Asi que no se toca.

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
        case SCENE_CREDITS:
            currentScene = showCredits();
            break;
        case SCENE_INTRO_ARCADE:
            currentScene = showArcadeIntro();
            break;
        case SCENE_VRAM_CLEAR:
            currentScene = showVramClear();
            break;
        case SCENE_PLAYER_SELECT:
            currentScene = showPlayerSelect();
            break;
        case SCENE_PROFILES:
            currentScene = showProfiles();
            break;
        case SCENE_OPTIONS:
            currentScene = showOptions();
            break;
        case SCENE_CHAR_SELECT:
            currentScene = showCharSelect(); // Retorna SCENE_CINEMATIC_FIRE
            break;
        case SCENE_CINEMATIC_FIRE:
            currentScene = showFireCinematic();
            break;
        case SCENE_1_1_TITLE:
            currentScene = showScene11Title();
            break;
        case SCENE_1_1:
            currentScene = showScene11();
            break;
        case SCENE_1_2:
            currentScene = showScene12();
            break;
        case SCENE_ENDING:
            currentScene = showEnding();
            break;
        case SCENE_2_1_TITLE:
            currentScene = showScene21Title();
            break;
        case SCENE_2_1:
            currentScene = showScene21();
            break;
        // (18/10) Recorrido del arcade + los niveles del port de NES (ver
        // scenes.h). Entre los dos niveles de una SCENE no hay titulo.
        case SCENE_3_1:                     // 2-2 la cloaca
            currentScene = showScene31();
            break;
        case SCENE_S3_TITLE:
            currentScene = showSceneS3Title();
            break;
        case SCENE_SNOW:                    // 3-1 Central Park (NES; 21/10 prueba del fondo)
            currentScene = showSceneSnow();
            break;
        case SCENE_4_1:                     // 3-2 el garage
            currentScene = showScene41();
            break;
        case SCENE_S4_TITLE:
            currentScene = showSceneS4Title();
            break;
        case SCENE_5_1:                     // 4-1 la autopista
            currentScene = showScene51();
            break;
        case SCENE_6_1:                     // 4-2 el skate
            currentScene = showScene61();
            break;
        case SCENE_S5_TITLE:
            currentScene = showSceneS5Title();
            break;
        case SCENE_7_1:                     // 5-1 la fabrica
            currentScene = showScene71();
            break;
        case SCENE_S6_TITLE:
            currentScene = showSceneS6Title();
            break;
        case SCENE_8_1:                     // 6-1 el Technodrome
            currentScene = showScene81();
            break;
        case SCENE_9_1:                     // 6-2 Krang y Shredder
            currentScene = showScene91();
            break;
        // Pantallas intermedias y niveles que todavia no tienen arte: una
        // pantalla provisoria con texto (scenes.c, showPendingScene). Cuando
        // llegue el arte, cada una pasa a tener su propio case.
        case SCENE_CUT_SHREDDER_APRIL:
        case SCENE_CUT_CITY:
        case SCENE_CUT_SPLINTER:
        case SCENE_DOJO:                    // 5-2 (NES)
        case SCENE_CUT_TECHNODROME:
            currentScene = showPendingScene(currentScene);
            break;
        case SCENE_CUT_VAN:                 // (23/10) ya tiene su imagen
            currentScene = showCutVan();
            break;
        case SCENE_EPILOGUE:
            currentScene = showEpilogue();
            break;
        case SCENE_STAFF_CREDITS:
            currentScene = showStaffCredits();
            break;
        case SCENE_GAME_OVER:
            currentScene = showGameOver();
            break;
        case SCENE_HISCORE_ENTRY:           // (20/10) iniciales y tabla
            currentScene = showHiscoreEntry();
            break;
        case SCENE_HISCORE_BOARD:
            currentScene = showHiscoreBoard();
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
