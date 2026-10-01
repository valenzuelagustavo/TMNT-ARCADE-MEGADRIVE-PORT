#ifndef _STAGE_BG_H_
#define _STAGE_BG_H_

#include <genesis.h>

// ===========================================================================
// STAGE_BG — fondo de scroll HORIZONTAL con cache de tiles (26/09)
// ===========================================================================
// Para los niveles cuyo fondo tiene mas tiles unicos que los que entran en
// VRAM (el sewer: 2037 unicos contra ~700 libres). La idea es la del "slot
// cache" del proyecto del companero, reescrita para nuestro motor:
//
//   - El IMAGE de rescomp (NONE: sin comprimir, deduplicado con flips) queda
//     en ROM. En VRAM hay un bloque de 'slots' tiles.
//   - Se mantienen dibujadas SOLO las columnas visibles (41 + 1 de margen).
//     Cuando una columna entra, sus tiles se buscan en el cache: si ya estan
//     (otra columna los usa) se reusa el slot; si no, se sube el tile a un
//     slot libre. Cuando una columna sale, se le resta una referencia a cada
//     tile y los que quedan en cero vuelven a la lista de libres.
//   - El plano es el circular de 64 columnas: la columna de mundo c va en la
//     columna del plano (c & 63).
//
// El peor caso de tiles unicos en 42 columnas lo mide el generador del nivel
// (en el sewer, 689): 'slots' tiene que ser al menos eso. Si igual faltara
// lugar, la celda se dibuja con el tile 0 (negro) en vez de colgarse.
//
// Una sola instancia a la vez (estado de modulo).
// ===========================================================================

#define SBG_WIN_COLS  42      // columnas dibujadas: 40 visibles + scroll fino + 1
#define SBG_WIN_ROWS  29      // (2D) filas dibujadas: 28 visibles + scroll fino

// FORMATO ANCHO (26/09, freeway): el tilemap de un IMAGE de rescomp guarda el
// indice en 11 bits, asi que un fondo con MAS de 2048 tiles unicos no se puede
// describir con IMAGE (la ruta de la freeway tiene 2369). Para esos, el
// generador del nivel arma sus propios BIN: los tiles (4bpp, como SGDK) y un
// mapa u16 con el indice en los bits 0-11 y los flips corridos 2 bits a la
// izquierda de donde los pone SGDK (H en el bit 13, V en el 14).
#define SBG_RAW_INDEX_MASK  0x0FFF
#define SBG_RAW_FLIP_SHIFT  2
typedef struct {
    const u32* tiles;
    const u16* map;
    u16        w, h;          // en tiles
    const Palette* pal;       // 16 colores
} SbgRaw;

// img: IMAGE de rescomp (NONE). plane/pal: donde y con que paleta se dibuja.
// vramBase/slots: bloque de VRAM del cache. camX: camara inicial (dibuja YA
// la ventana, con DMA inmediato: llamar con la pantalla en negro).
void sbgInit(const Image* img, VDPPlane plane, u16 pal, u16 vramBase,
             u16 slots, s16 camX);

// Lo mismo con un fondo en formato ancho.
void sbgInitRaw(const SbgRaw* raw, VDPPlane plane, u16 pal, u16 vramBase,
                u16 slots, s16 camX);

// (27/09) Variante 2D (Technodrome): la ventana sigue a la camara en X e Y
// sobre un plano circular de 64x32. Mapa en formato ancho; el indice 0 es el
// vacio (tile 0, transparente, no ocupa cache). Una instancia por vez: no se
// mezcla con sbgInit/sbgInitRaw.
void sbgInitRaw2D(const SbgRaw* raw, VDPPlane plane, u16 pal, u16 vramBase,
                  u16 slots, s16 camX, s16 camY);
void sbgUpdate2D(s16 camX, s16 camY);

// Trae las columnas que entraron y suelta las que salieron. Una vez por
// frame, ANTES de fijar el scroll. Tambien funciona hacia atras.
void sbgUpdate(s16 camX);

// Cuantos slots estan ocupados ahora mismo (debug / calibracion).
u16  sbgUsed(void);

// (01/10, garage) REGIONES VARIABLES: rectangulos de celdas del fondo (solo
// el modo 1D: sbgInit/sbgInitRaw) con versiones alternas. La variante 0 es la
// del mapa principal; las 1..nVar estan en 'var', una tras otra, cada una de
// w x h celdas en el MISMO formato y con el MISMO tileset que el mapa. Sirve
// para cambiar un pedazo del fondo en pleno nivel sin sprites: el auto
// estacionado que arranca, la persiana del ascensor que sube. Al cambiar de
// variante se sueltan los tiles de la vieja y se piden los de la nueva solo en
// las columnas dibujadas; las demas se resuelven al entrar.
#define SBG_MAX_REGIONS 4
typedef struct {
    u16        c0, r0, w, h;  // en celdas
    u16        nVar;          // variantes alternas (sin contar la 0)
    const u16* var;           // nVar * w * h celdas
} SbgRegion;
// Llamar ANTES de sbgInit/sbgInitRaw (todas arrancan en la variante 0).
void sbgSetRegions(const SbgRegion* regs, u16 n);
void sbgSetVariant(u16 reg, u16 v);

#endif
