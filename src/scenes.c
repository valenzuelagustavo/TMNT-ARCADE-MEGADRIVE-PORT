#include "scenes.h"
#include "sprites.h"
#include "audio.h"

// Función auxiliar de limpieza
void clearScene() {
    // 1. Apagar la pantalla (Brillo a cero)
    PAL_fadeOutAll(20, FALSE); 
    
    // 2. Esperar a que el Fade termine de verdad[cite: 3]
    while(PAL_isDoingFade()) {
        SYS_doVBlankProcess(); 
    }

    // 3. Detener todo proceso de audio y sprites
    XGM_stopPlay();
    SPR_reset();
    
    // 4. Limpieza total de planos
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    
    // 5. Forzar que el fondo sea negro antes de cargar nada nuevo
    VDP_setBackgroundColor(0); 
    
    // 6. Asegurar que los cambios se apliquen al VDP
    SYS_doVBlankProcess();
}

// 1. Aquí pega tu código de la intro de SEGA que ya tenías
SceneId showSegaIntro()
{
    // ... todo el código de Rocksteady chocando el logo ...
    // Usamos NULL para las paletas inicialmente y las cargamos luego
    Sprite *segaLogo = SPR_addSprite(&sega_logo_spr, 104, 92, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
    Sprite *rocksteady = SPR_addSprite(&rocksteady_spr, -90, 80, TILE_ATTR(PAL1, FALSE, FALSE, FALSE));

    PAL_setPalette(PAL0, sega_logo_spr.palette->data, DMA);
    PAL_setPalette(PAL1, rocksteady_spr.palette->data, DMA);

    s16 rockX = -90;
    u16 timerKO = 0;
    u16 estado = 0; // 0: Corriendo, 1: Impacto, 2: KO, 3: Fade

    SPR_setAnim(segaLogo, 0);
    SPR_setAnim(rocksteady, 0);

    // 1. Empezar a reproducir la música en bucle (loop)
    XGM_startPlay(music_sega);

    while (1)
    {
        if (estado == 0)
        {
            rockX += 3;
            if (rockX >= 40)
            {
                estado = 1;
                SPR_setAnim(segaLogo, 1);
                SPR_setAnim(rocksteady, 1);
                XGM_stopPlay();
                XGM_startPlay(golpe);
            }
        }
        else if (estado == 1)
        {
            timerKO++;
            if (timerKO > 20)
            {
                estado = 2;
                SPR_setAnim(rocksteady, 2);
                timerKO = 0;
            }
        }
        else if (estado == 2)
        {
            timerKO++;
            if (timerKO > 60)
            {
                estado = 3;
                PAL_fadeOutAll(30, FALSE);
            }
        }
        else if (estado == 3)
        {
            if (!PAL_isDoingFade())
                break;
        }

        SPR_setPosition(rocksteady, rockX, 80);
        SPR_update();
        SYS_doVBlankProcess(); // Cambiado para evitar el warning
    }
// Al terminar:
    PAL_fadeOutAll(20, FALSE);
    while(PAL_isDoingFade()) {
        SYS_doVBlankProcess();
    }

    // LIBERACIÓN MANUAL: Esto quita los sprites del motor antes de cargar nada más
    if (segaLogo) SPR_releaseSprite(segaLogo);
    if (rocksteady) SPR_releaseSprite(rocksteady);
    
    // Forzamos un update para que el VDP sepa que ya no existen
    SPR_update();
    SYS_doVBlankProcess();

    clearScene(); 
    return SCENE_PLAYER_SELECT;
}

// 2. Las demás deben existir como "cascarones" para que no den error
SceneId showKonamiIntro()
{
    // Vacío por ahora
    return SCENE_SGDK;
}

SceneId showSGDKIntro()
{
    return SCENE_INTRO_ARCADE;
}

SceneId showArcadeIntro()
{
    return SCENE_PLAYER_SELECT;
}

// ¡Esta es una de las que te falta según el error!
SceneId showPlayerSelect()
{
    clearScene();

    // Setear el fondo azul estilo arcade (Color 0 de PAL0)
    VDP_setBackgroundColor(0x0044); // Un azul oscuro (Formato VDP: 0x0BGR)

    // Cargar el logo de fondo (recurso "logo" en tu .res)[cite: 4, 5]
    PAL_setPalette(PAL0, logo.palette->data, DMA);
    VDP_drawImageEx(BG_B, &logo, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX), 3, 0, FALSE, TRUE);

    // Escribir los textos que tenías en 2025
    VDP_drawText("1 TORTUGA", 14, 18);
    VDP_drawText("2 TORTUGAS", 14, 20);
    VDP_drawText("Desarrollado por: Gustavo Valenzuela", 2, 26);

    // Cursor (recurso "selector_turtle")[cite: 4, 5]
    Sprite *cursor = SPR_addSprite(&selector_turtle, 8 * 8, 14 * 8, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    PAL_setPalette(PAL1, selector_turtle.palette->data, DMA);

    u8 selectedOption = 0;

    while (1)
    {
        u16 value = JOY_readJoypad(JOY_1);

        if (value & BUTTON_UP)
            selectedOption = 0;
        if (value & BUTTON_DOWN)
            selectedOption = 1;

        // Actualizar posición del cursor[cite: 4]
        SPR_setPosition(cursor, 8 * 8, (14 + selectedOption * 2) * 8);

        if (value & BUTTON_START)
            break;

        SPR_update();
        SYS_doVBlankProcess();
    }

    return SCENE_CHAR_SELECT; // Ir a selección de personajes
}

SceneId showCharSelect() {
    // 1. Limpieza inicial para asegurar lienzo negro y vacío[cite: 5]
    clearScene();

    // 2. Configuración de Fondos y Paletas
    // Cargamos la imagen en escala de grises en el plano de fondo
    PAL_setPalette(PAL0, characters_greyscale.palette->data, DMA);
    VDP_drawImageEx(BG_B, &characters_greyscale, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX), 0, 0, FALSE, TRUE);

    // 3. Audio[cite: 4, 5]
    XGM_startPlay(select_music);

    // 4. Inicialización de Sprites (Cursor y Caras del HUD)[cite: 4, 5]
    // Usamos variables locales para manejar los sprites dentro de esta escena
    Sprite* cursor = SPR_addSprite(&character_selector, 8, 48, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    Sprite* turtle_face_hud = SPR_addSprite(&faces_hud, 58, 5, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
    
    // Cargamos las paletas específicas de los sprites[cite: 4]
    PAL_setPalette(PAL1, character_selector.palette->data, DMA);
    PAL_setPalette(PAL2, faces_hud.palette->data, DMA);

    // 5. Variables de Control de Selección[cite: 4]
    u8 selectedCharacter = 0;
    bool buttonPressed = FALSE;
    s16 charPosX[] = {8, 88, 168, 248}; // Posiciones X para cada tortuga
    const s16 charPosY = 48;

    // 6. Bucle Principal de la Escena[cite: 3, 4]
    while(1) {
        u16 value = JOY_readJoypad(JOY_1);

        // Lógica de movimiento lateral[cite: 4]
        if (value & (BUTTON_RIGHT | BUTTON_LEFT)) {
            if (!buttonPressed) {
                if ((value & BUTTON_RIGHT) && selectedCharacter < 3) {
                    selectedCharacter++;
                } else if ((value & BUTTON_LEFT) && selectedCharacter > 0) {
                    selectedCharacter--;
                }
                
                // Actualizar animaciones según el índice del personaje (0-3)[cite: 4]
                SPR_setAnim(cursor, selectedCharacter);
                SPR_setAnim(turtle_face_hud, selectedCharacter);
                
                // Actualizar posición del cursor en pantalla[cite: 4]
                SPR_setPosition(cursor, charPosX[selectedCharacter], charPosY);
                
                buttonPressed = TRUE;
            }
        } else {
            buttonPressed = FALSE;
        }

        // Confirmación de selección[cite: 4]
        if (value & BUTTON_START) {
            break; // Salimos del bucle para avanzar de escena
        }

        // Actualizar el motor de sprites y esperar el refresco de pantalla[cite: 3, 4]
        SPR_update();
        SYS_doVBlankProcess();
    }

    // 7. Limpieza Crítica antes de salir (Evita rastro de sprites y música)
    XGM_stopPlay(); // Detener música de selección[cite: 5]
    
    PAL_fadeOutAll(20, FALSE); // Fundido a negro antes de borrar[cite: 5]
    while(PAL_isDoingFade()) {
        SYS_doVBlankProcess();
    }

    // Liberación manual de sprites para no dejar basura en el VDP
    if (cursor) SPR_releaseSprite(cursor);
    if (turtle_face_hud) SPR_releaseSprite(turtle_face_hud);
    
    // Limpieza final de planos[cite: 5]
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    SPR_update();
    SYS_doVBlankProcess();

    return SCENE_LEVEL1_TITLE; // Retorna el ID de la siguiente escena[cite: 4]
}

SceneId showFireCinematic()
{
    return SCENE_LEVEL1_TITLE;
}

SceneId showLevel1Title() {
    clearScene();
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    // Definimos una paleta rápida donde el color 15 es blanco (0xEEE en Mega Drive)
    u16 pal_white[16] = { 0x000, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 
                          0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE };

    VDP_setBackgroundColor(0);
    VDP_drawText("SCENE 1", 16, 10);
    VDP_drawText("FIRE! WE GOTTA GET", 11, 13);
    VDP_drawText("APRIL OUT!!", 14, 15);

    // Usamos nuestra paleta definida arriba
    PAL_fadeIn(0, 15, pal_white, 20, FALSE);

    // 4. Temporizador de espera (aprox. 3 segundos a 60fps)[cite: 4]
    u16 timer = 180;
    while(timer > 0) {
        timer--;
        
        // Opcional: Permitir saltar con START
        u16 joy = JOY_readJoypad(JOY_1);
        if (joy & BUTTON_START) break;

        SYS_doVBlankProcess(); // Mantener el motor vivo[cite: 3, 4]
    }

    // 5. Fade Out antes de entrar al caos del fuego[cite: 5]
    clearScene();

    return SCENE_LEVEL1; // Saltamos al nivel 1
}

SceneId showLevel1()
{
    // El juego en sí
    while (1)
    {
        SYS_doVBlankProcess();
    }
    return SCENE_SEGA;
}