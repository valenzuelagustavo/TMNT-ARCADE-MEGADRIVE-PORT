// =============================================================================
// level3_1.res  Scene 3: la cloaca (sewer) -- 26/09, fondo nuevo el 28/09
// =============================================================================
// Arte: "Arcade - Teenage Mutant Ninja Turtles - Backgrounds - Stage 3.png"
// (la hoja que trajo Gustavo). tools/gen_level3_1_bg.py saca de ahi:
//
// pal_sewer / bg_sewer_tiles / bg_sewer_map
//             El fondo, 1248x224, 15 colores. Tiene 2253 tiles unicos (el agua
//             nueva es mucho mas rica que la de Ray): NO entra en el indice de
//             11 bits de un IMAGE, va en el formato ancho de stage_bg.c y se
//             streamea por columnas. Peor caso en 42 columnas: 796 tiles.
//             PAL0. El indice 0 (sin usar en el fondo) quedo en negro.
// bg_sewer_fg Los caños que pasan por DELANTE (74 tiles): entran enteros y se
//             dibujan en BG_A con prioridad alta. Misma paleta (PAL0), el
//             indice 0 transparente. Alineados con el fondo como en la hoja.
// =============================================================================
PALETTE pal_sewer      "images/lvl_3_sewer/bg_sewer.png"
BIN     bg_sewer_tiles "images/lvl_3_sewer/bg_sewer_tiles.bin" 2 2 0 NONE
BIN     bg_sewer_map   "images/lvl_3_sewer/bg_sewer_map.bin"   2 2 0 NONE
IMAGE   bg_sewer_fg    "images/lvl_3_sewer/bg_sewer_fg.png"    NONE ALL

// (28/09) Lo del primer plano que cae en la franja del HUD (filas 0-3, que en
// BG_A no scrollean): 9 frames de 8x32, uno por columna, que stage_level
// muestra como sprites (fgTop). Sus X de mundo: src/level3_1_fgtop.h.
SPRITE  sewer_fg_top   "images/lvl_3_sewer/sewer_fg_top.png"   1 4 NONE 0

// (29/09) Misil que sale del AGUA cuando una tortuga camina por el canal, y
// su explosion. Arte de Traag (traag_missil / traag_explosao) remapeado a la
// paleta de los foot soldiers (PAL2) por tools/gen_sewer_missile.py: en la
// cloaca PAL3 es de Baxter. Misil: 2 frames de 64x32 (mira a la DERECHA).
// Explosion: 7 frames de 32x32 (impacta de derecha a izquierda), a mano.
SPRITE  sewer_missil   "sprites/sewer_missil.png"   8 4 NONE 4
SPRITE  sewer_explosao "sprites/sewer_explosao.png" 4 4 NONE 0
