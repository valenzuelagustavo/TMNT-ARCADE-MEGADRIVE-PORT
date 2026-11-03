// gameover.h (02/11) -- CONTINUE grande (un solo jugador) y cartel de GAME OVER.
#ifndef _GAMEOVER_H_
#define _GAMEOVER_H_

#include <genesis.h>

// --- Cartel GAME OVER ---------------------------------------------------------
// Dentro de un nivel, cuando TODOS los jugadores quedaron fuera (la llama
// continueStepAll, en scenes.c): fundido a negro de la pantalla, jingle
// (music_gameover) y el cartel en el centro con la paleta rotando. Deja la
// pantalla en negro y marca el cartel como mostrado.
void gameOverPlayInLevel(void);
// TRUE una sola vez tras gameOverPlayInLevel: showGameOver lo consume para no
// repetir el cartel.
bool gameOverTakeShown(void);
// El cartel solo, sobre negro (despues de los creditos). Espera la escena
// limpia (clearScene hecho).
void gameOverScreen(void);

// --- CONTINUE grande, en el centro (un solo jugador) --------------------------
// Rotulo "CONTINUE" y el numero en rojo, como el arcade. Son sprites (el plano
// del nivel no se toca). contBigShow devuelve FALSE si no hubo VRAM de sprites:
// el llamador cae al texto del HUD.
bool contBigShow(s8 seconds);
void contBigHide(void);
// SPR_reset/SPR_end ya los liberaron: solo se olvidan los punteros.
void contBigForget(void);

#endif
