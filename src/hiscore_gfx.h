// GENERADO POR tools/gen_highscore.py -- NO EDITAR A MANO
#ifndef _HISCORE_GFX_H_
#define _HISCORE_GFX_H_

#include <genesis.h>

#define HS_GLYPHS     43
#define HS_GLYPH_STR  " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.!?-*&"
// Fuente alta: [variante][glifo][arriba, abajo]
enum { HS_WHITE, HS_CYAN, HS_YELLOW, HS_MAGENTA, HS_LAV, HS_COLORS };
#define HS_FONT_TILES (HS_COLORS * HS_GLYPHS * 2)
extern const u32 hsFontTiles[];
#define HS_FRAME_TILES 8
extern const u32 hsFrameTiles[];
extern const u32 hsGradEntryTiles[];   // 28 tiles, uno por fila de pantalla
extern const u32 hsGradBoardTiles[];
extern const u16 hsPalEntry[16];       // PAL0: degrade + texto
extern const u16 hsPalBoard[16];
extern const u16 hsPalTint[4][16];     // retratos de la tabla
extern const u16 hsPalBlack[16];       // siluetas

#endif
