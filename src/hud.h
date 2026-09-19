#ifndef _HUD_H_
#define _HUD_H_

#include <genesis.h>
#include "player.h"

// ===========================================================================
// HUD del nivel — interfaz compartida entre escenas
// ===========================================================================
// La implementacion vive en scenes.c (junto al nivel 1, que es donde nacio).
// Este header existe para que los niveles que estan en su propio modulo
// (level2_1.c) puedan usar EXACTAMENTE el mismo HUD en vez de duplicarlo.
//
// Recordatorio de como esta armado (el detalle largo esta en scenes.c):
//   - Los MARCOS (72x32) y los retratos son SPRITES de alto nivel, asi
//     quedan siempre por encima de los planos. Se liberan solos en
//     clearScene() via SPR_reset().
//   - El CONTENIDO (vidas, puntaje, barra de vida) son tiles de BG_A con
//     prioridad alta, en PAL1 (la paleta de las tortugas).
//   - La barra de vida necesita HPBAR_FRAME_TILES tiles de VRAM de fondo POR
//     JUGADOR; el llamador decide donde y los pasa en 'barVram'.
// ---------------------------------------------------------------------------

// MAX_PLAYERS vive en player.h (lo usa tambien el estado persistente).

#define HUD_TILE_W         9                       // Ancho del marco en tiles (72px)
#define HUD_P1_X           40                      // P1 corrido hacia adentro (libera 40px del borde izq)
#define HUD_P2_X           (320 - (HUD_TILE_W * 8) - 40)  // 208: P2 corrido hacia adentro
#define HUD_P1_BASECOL     (HUD_P1_X / 8)          // 5: columna de tile donde arranca el marco P1
#define HUD_P2_BASECOL     (HUD_P2_X / 8)          // 26: idem P2

// (17/09) EL MARCO BAJA 4 PIXELES, y es lo que hace que entre todo.
// Midiendo el arte (hud_1p.png, 72x32): el borde de arriba de la caja esta en
// y 0..2 para x >= 29 y en y 9..11 debajo de la pestana del "1UP"; el de abajo,
// en y 28..31. O sea que el interior util es y 4..27 a la derecha (24px = 3
// filas de tiles) pero NO arranca en multiplo de 8. Corriendo el marco a y=4,
// esas 24 filas caen justo en las filas de tile 1, 2 y 3:
//   fila 1 (y  8..15) -> PUNTAJE          (interior a partir de la col 4)
//   filas 2-3 (16..31) -> BARRA 32x16 + digito de VIDAS, que ocupan el
//                         interior completo, como en el arcade
// Sin este offset la barra alta no entra sin comerse la fila del puntaje.
#define HUD_FRAME_Y        4

// Retratos: ya NO se dibujan durante la partida (17/09). Vuelven a aparecer
// SOLO mientras un jugador elige tortuga -- al continuar, o cuando el P2 se
// suma a una partida de 1 jugador -- como ayuda visual de a quien esta
// eligiendo. Estas son las posiciones donde se crean en ese momento.
#define PORTRAIT_P1_X      0                       // borde izquierdo libre
#define PORTRAIT_P2_X      (320 - 32)              // 288: borde derecho libre
#define PORTRAIT_Y         HUD_FRAME_Y

#define HPBAR_FRAME_TILES_W  4                                            // 32px
#define HPBAR_FRAME_TILES_H  2                                            // 16px (17/09: era 1)
#define HPBAR_FRAME_TILES    (HPBAR_FRAME_TILES_W * HPBAR_FRAME_TILES_H)  // 8

// Vidas: un digito de 8x16 (1x2 tiles) en verde, pegado a la barra.
#define HUDLIVES_TILES_W     1
#define HUDLIVES_TILES_H     2
#define HUDLIVES_TILES       (HUDLIVES_TILES_W * HUDLIVES_TILES_H)        // 2

// Estado del HUD de un jugador: cachea lo ultimo dibujado para redibujar solo
// cuando cambia (evita reescribir VRAM cada frame).
typedef struct {
    Player* pl;
    u16     baseCol;    // columna de tile donde arranca el marco (0 = P1)
    u16     barVram;    // primer tile de VRAM del bloque de la barra
    u16     livesVram;  // primer tile de VRAM del digito de vidas
    s16     lastHealth;
    s16     lastLives;
    s32     lastScore;
} HudPlayer;

// Crea los marcos del HUD. Llamar con el motor de sprites ya inicializado.
// (17/09) YA NO crea los retratos: durante la partida no van.
void hudInit(void);

// Solo para la pantalla de SELECCION DE PERSONAJE, que si los quiere fijos.
void hudInitPortraits(void);

// Prepara el HUD de un jugador. Llamar DESPUES de initPlayer (PAL1 cargada) y
// de fijar paleta/plano de texto. 'barVram' apunta a un bloque de
// HPBAR_FRAME_TILES + HUDLIVES_TILES tiles: barra primero, digito despues.
void hudPlayerInit(HudPlayer* h, Player* pl, u16 baseCol, u16 barVram);

// Tiles de VRAM de fondo que necesita el HUD de UN jugador (barra + vidas).
#define HUD_VRAM_PER_PLAYER  (HPBAR_FRAME_TILES + HUDLIVES_TILES)   // 10

// Invitacion "PULSE / START" dentro del marco vacio del P2 (solo en 1 jugador)
// y entrada del P2 en plena partida. Devuelve el personaje elegido (0..3) el
// frame en que confirma, o 0xFF si todavia no. Hay que llamarla una vez por
// frame mientras numJugadores() == 1.
u8   p2JoinPoll(u16 baseCol);
void p2JoinReset(void);

// Redibuja SOLO lo que cambio. Una vez por frame.
void hudPlayerUpdate(HudPlayer* h);

// --- Modo 4 jugadores (14/09, reescrito el 16/09) --------------------------
// Primera version: cuatro bloques pelados de 10 columnas, sin marco, para
// ahorrar sprites. Ahora van los CUATRO MARCOS, como el arcade.
//
// Medido sobre la captura del arcade (320x224): los bordes inferiores de los
// cuatro marcos caen en x = 17..86 / 89..159 / 161..231 / 233..303, o sea
// paso de 72px EXACTOS (= el ancho de nuestro marco, 9 tiles) y el conjunto
// centrado en pantalla. Redondeando a columna de tile (el CONTENIDO del HUD
// son tiles de BG_A y tiene que caer alineado) queda x = 16/88/160/232, o
// sea columnas 2/11/20/29: los cuatro bloques se tocan sin solaparse y
// sobran 2 columnas de cada lado. Y = 0, como en el arcade.
//
// Sin retratos: en el arcade tampoco los hay con 4 jugadores y no queda hueco.
#define HUD4_X0            16                      // x del marco del jugador 0
#define HUD4_BASECOL0      (HUD4_X0 / 8)           // 2
#define HUD4_BLOCK_COLS    HUD_TILE_W              // 9 columnas: marcos pegados
#define HUD4_LABEL_COL     1                       // (legacy) etiqueta dentro del bloque

// Columna de tile donde arranca el HUD del jugador k (0..3), segun el modo.
u16 hudPlayerCol(u8 k);

// Joystick / personaje / cantidad de jugadores (implementados en scenes.c).
u16 playerJoy(u8 k);
u8  playerChar(u8 k);
u8  numJugadores(void);

#endif // _HUD_H_
