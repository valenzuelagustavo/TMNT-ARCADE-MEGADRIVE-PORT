// ===========================================================================
// techno_hazards.h - peligros del TECHNODROME (Scene 6-1) (18/10)
// ===========================================================================
// Arte: tools/gen_techno_hazards.py, desde la plantilla de Gustavo
// ("...Enemies_abelhas.png"). Todo en PAL3 = la paleta del foot soldier
// BLANCO (la plantilla viene en esa misma paleta): el nivel la carga en PAL3
// al arrancar y Traag la pisa recien al final, cuando ya no queda nada de
// esto en pantalla.
//
// COLUMNAS (arriba): 4, juntas. Cuelgan del techo (llegan hasta arriba de la
// pantalla) y la cabeza (con el canon) sube y baja sin parar, cada una a su
// velocidad, disparando rafagas de laser siempre hacia la IZQUIERDA (no se
// dan vuelta). Disparando son plateadas lisas; cada golpe de una tortuga le
// saca rayitos y la hace parpadear naranja; al 4to golpe queda quieta donde
// estaba, naranja y sin disparar.
// El laser vuela a la altura del canon sobre la lane de la columna (con su
// sombra en el piso): si la cabeza esta alta pasa por arriba de la tortuga.
//
// BOLAS (viaje en el ascensor): caen desde arriba sobre la base del
// ascensor, pican ahi (con sombra), ruedan pozo abajo mas rapido que el
// ascensor y al salirse por el borde caen al pozo; hay que esquivarlas o
// saltarlas. Golpe con volteo.
//
// VRAM: las columnas, el laser y las sombras comparten tiles cargados a mano
// (TH_VRAM_TILES, fuera del pool de sprites: el nivel se los reserva). Las
// bolas usan el pool normal.
// ===========================================================================
#ifndef _TECHNO_HAZARDS_H_
#define _TECHNO_HAZARDS_H_

#include <genesis.h>
#include "player.h"

#define TH_VRAM_TILES  108

// (18/10) Una columna: x/lane (pies) de mundo, altura de la base de la cabeza
// sobre el piso (zMin..zMax, recortada por zTopK de thInit), velocidad en Q4
// (8 = 0,5 px/f) y frames hasta el primer disparo.
typedef struct {
    s16 x, lane;
    u8  zMin, zMax;
    u8  speedQ;
    u8  delay;
} ThPillarDef;
#define TH_TECHNO_PILLARS 4
extern const ThPillarDef thTechnoPillars[TH_TECHNO_PILLARS];

// Carga los tiles compartidos en 'vram' (TH_VRAM_TILES) y deja todo apagado.
// defs/n: las columnas del nivel (hasta 4). zTopK: la cabeza no pasa de
// z = lane - zTopK (que su borde de arriba no se meta en la franja del HUD;
// Technodrome 110 con camY 32, fabrica 78 sin camara vertical).
void thInit(u16 vram, const ThPillarDef* defs, u8 n, s16 zTopK);
// Columnas + lasers (fase de arriba). camY: la Y de la camara del nivel.
void thPillarsUpdate(Player** pls, u8 nPl, s16 camX, s16 camY);
// Bolas (viaje). elevX/elevY: esquina del ascensor (mundo); spawn = TRUE
// mientras pueden salir bolas nuevas (las que ya estan terminan su camino).
void thBallsUpdate(Player** pls, u8 nPl, s16 camX, s16 camY,
                   s16 elevX, s16 elevY, bool spawn);
// Suelta todos los sprites (columnas, lasers y bolas).
void thRelease(void);

#endif
