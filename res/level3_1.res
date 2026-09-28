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
//             indice 0 transparente. Misma posicion que tenian los de Ray.
// =============================================================================
PALETTE pal_sewer      "images/lvl_3_sewer/bg_sewer.png"
BIN     bg_sewer_tiles "images/lvl_3_sewer/bg_sewer_tiles.bin" 2 2 0 NONE
BIN     bg_sewer_map   "images/lvl_3_sewer/bg_sewer_map.bin"   2 2 0 NONE
IMAGE   bg_sewer_fg    "images/lvl_3_sewer/bg_sewer_fg.png"    NONE ALL
