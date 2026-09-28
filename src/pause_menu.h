#ifndef _PAUSE_MENU_H_
#define _PAUSE_MENU_H_

#include <genesis.h>
#include "scenes.h"
#include "player.h"

// ===========================================================================
// PAUSA + SELECTOR DE NIVELES (26/09)
// ===========================================================================
// START del CONTROL 1 (el mando del jugador 1, playerJoy(0)) pausa el nivel.
// Todo queda congelado tal cual (no se llama a SPR_update ni a la logica) y la
// musica se pausa. En el medio de la pantalla aparece un recuadro:
//
//   con el codigo Konami                   sin el codigo
//   ------------------------------        ------------------------------
//             PAUSA                                   PAUSA
//     SEGUIR
//     SCENE 1   1-1   1-2                        START: SEGUIR
//     SCENE 2   2-1
//      START: IR   B: SEGUIR
//
//   Arriba/abajo eligen la fila, izquierda/derecha el subnivel. START sobre
//   SEGUIR (o B/C en cualquier fila) vuelve al juego; START sobre un subnivel
//   corta el nivel actual y salta a ese (empezandolo de cero, con vidas y
//   barra llenas y el puntaje que se traia).
//
// (27/09) El SELECTOR solo aparece si en la pantalla de cantidad de jugadores
// se ingreso el codigo Konami (arriba, arriba, abajo, abajo, izquierda,
// derecha, izquierda, derecha, B, A; suena "Cowabunga!"). Sin el codigo la
// pausa es la simple. Queda activo hasta apagar o resetear la consola.
//
// NO pisa el continue: si el jugador 1 esta en game over (mostrando
// CONTINUE?), su START es del continue y la pausa no se abre. Los mandos 2-4
// no pausan.
//
// Para sumar un nivel nuevo al menu alcanza con agregarlo a la tabla de
// pause_menu.c; si hay mas filas que las que entran, la lista scrollea.
//
// Cada nivel lo engancha asi:
//     pauseReset();                        // antes del bucle principal
//     while (...) {
//         SceneId jump = pausePoll(pls, nPl);
//         if (jump != PAUSE_NO_JUMP) break;     // y al salir: return jump
//         ...
// ===========================================================================

#define PAUSE_NO_JUMP      ((SceneId)0xFF)

// Recuadro en BG_A, en tiles de PANTALLA. Filas 12..19: entre el humo del
// techo del 1-2 (filas 4..11) y la banda de fuego del 1-1/1-2 (20..27), que
// son las unicas zonas de BG_A que scrollean. Todo lo que hubiera debajo del
// recuadro se guarda al abrir y se repone al cerrar.
#define PAUSE_BOX_COL      8
#define PAUSE_BOX_ROW      12
#define PAUSE_BOX_W        24
#define PAUSE_BOX_H        8

// Activa / consulta el selector de niveles (lo prende el codigo Konami en
// showPlayerSelect).
void pauseSetLevelSelect(bool on);
bool pauseLevelSelect(void);

// Olvida el START que venga apretado de la escena anterior. Llamar una vez,
// justo antes del bucle principal del nivel.
void pauseReset(void);
// (29/09) Scroll vertical de BG_A en px (0 = ninguno), para que el recuadro de
// la pausa caiga en su lugar de pantalla. Lo actualiza stage_level.
void pauseSetVScroll(s16 px);

// Llamar al PRINCIPIO de cada frame del nivel (antes de los continues). Si
// no se pulso START, vuelve enseguida con PAUSE_NO_JUMP. Si se pulso, se
// queda adentro hasta que el jugador sigue (PAUSE_NO_JUMP) o elige un nivel
// (devuelve su SceneId; el nivel tiene que cortar su bucle, limpiar como en
// el game over y devolver esa escena).
SceneId pausePoll(Player** pls, u8 nPl);

#endif
