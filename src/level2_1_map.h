// GENERADO POR tools/gen_level2_1_bg.py -- NO EDITAR A MANO
#ifndef _LEVEL2_1_MAP_H_
#define _LEVEL2_1_MAP_H_

#include <genesis.h>

#define LVL21_MAP_W      320
#define LVL21_MAP_H      80
#define LVL21_SECTIONS   13
#define LVL21_RING_TILES 768
#define LVL21_PRELOAD_PX 16
#define LVL21_LOAD_TILES_PER_FRAME 128

// Corredor de camara: para cada camY (de a 8px, desde LVL21_CAM_Y_MIN) el
// minimo y el maximo camX que no dejan asomar zona sin arte.
//
// El TECHO es una escalera -- son los "topes" de camara marcados sobre el
// mapa -- y el PISO de cada peldano es el techo del peldano ANTERIOR. Con el
// freno de camY del runtime (no puede adelantarse a camX) eso da el recorrido
// del arcade: la camara avanza hasta el tope del escalon, se clava, y solo se
// libera cuando los jugadores bajan al escalon siguiente.
#define LVL21_CAM_Y_MIN  32
#define LVL21_CAM_Y_MAX  416
#define LVL21_CAM_X_MAX  2240
#define LVL21_CAM_ROWS   49

// Cada celda: bit15 = tiene arte, bits 9..14 = seccion, bits 0..8 = tile.
// Celda 0 = sin arte -> se dibuja con el tile 0 del VDP (negro).
extern const u16 lvl21Map[LVL21_MAP_W * LVL21_MAP_H];
extern const u16 lvl21SecTiles[LVL21_SECTIONS];
extern const u16 lvl21SecStart[LVL21_SECTIONS];
extern const u16 lvl21CamFloor[LVL21_CAM_ROWS];
extern const u16 lvl21CamCeil[LVL21_CAM_ROWS];

#endif
