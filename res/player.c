#include "player.h"

// 1. VARIABLES PRIVADAS DEL MÓDULO (El "estado" del jugador)
// Al no ponerlas en el .h, estas variables solo pueden ser modificadas desde este archivo
Sprite* playerSprite;
s16 playerX;
s16 playerY;
PlayerState currentState;
u8 currentComboStep;

// 2. FUNCIÓN DE INICIALIZACIÓN
void initPlayer(u8 selectedCharacter) {
    const SpriteDefinition* spriteDef = &don_player; // Por defecto

    // Elegimos la tortuga correcta según lo que pasó el menú de selección
    switch(selectedCharacter) {
        case 0: spriteDef = &leo_player; break; // Asumiendo que 0 es Leo
        // case 1: spriteDef = &raph_player; break;
        // case 2: spriteDef = &don_player; break;
        // case 3: spriteDef = &mike_player; break;
    }

    // Valores iniciales al empezar el nivel
    playerX = 100;
    playerY = 120;
    currentState = STATE_IDLE;
    currentComboStep = 0;

    // Creamos el sprite y le asignamos la paleta
    playerSprite = SPR_addSprite(spriteDef, playerX, playerY, TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
    PAL_setPalette(PAL1, spriteDef->palette->data, DMA);
    
    // Forzamos la animación inicial
    SPR_setAnim(playerSprite, ANIM_IDLE);
}

// 3. LA LÓGICA PRINCIPAL (Se llama en cada frame desde el while(1) del nivel)
void updatePlayerInput() {
    u16 joy = JOY_readJoypad(JOY_1);

    // Comportamiento según el estado actual
    switch (currentState) {
        
        case STATE_IDLE:
        case STATE_WALKING: {
            s16 moveX = 0;
            s16 moveY = 0;

            // Leer direccionales
            if (joy & BUTTON_RIGHT) {
                moveX += 2;
                SPR_setHFlip(playerSprite, FALSE); // Mira a la derecha
            }
            if (joy & BUTTON_LEFT) {
                moveX -= 2;
                SPR_setHFlip(playerSprite, TRUE);  // Mira a la izquierda
            }
            if (joy & BUTTON_UP) { moveY -= 2; }
            if (joy & BUTTON_DOWN) { moveY += 2; }

            // Si hay movimiento, actualizamos coordenadas y cambiamos a estado caminando
            if (moveX != 0 || moveY != 0) {
                playerX += moveX;
                playerY += moveY;
                currentState = STATE_WALKING;

                // Lógica simple para Walk Front vs Walk Back
                if (moveY < 0) { // Si va hacia "arriba" (al fondo del escenario)
                    SPR_setAnim(playerSprite, ANIM_WALK_BACK);
                } else {
                    SPR_setAnim(playerSprite, ANIM_WALK_FRONT);
                }
            } else {
                // Si no toca nada, vuelve a Idle
                currentState = STATE_IDLE;
                SPR_setAnim(playerSprite, ANIM_IDLE);
            }

            // Transición a estado de ataque
            if (joy & BUTTON_B) {
                currentState = STATE_ATTACKING;
                currentComboStep = 1;
                SPR_setAnim(playerSprite, ANIM_ATTACK_1);
            }
            break;
        }

        case STATE_ATTACKING: {
            // CAMBIA SPR_isAnimDone por SPR_isAnimationDone
            if (SPR_isAnimationDone(playerSprite)) {
                // Si la animación terminó, volvemos a Idle
                currentState = STATE_IDLE;
                currentComboStep = 0;
                SPR_setAnim(playerSprite, ANIM_IDLE);
            }
            default:
                break;
        }

        // Aquí irás agregando case STATE_JUMPING:, case STATE_HURT:, etc.
    }

    // Finalmente, actualizamos la posición real del sprite en la consola
    SPR_setPosition(playerSprite, playerX, playerY);
}