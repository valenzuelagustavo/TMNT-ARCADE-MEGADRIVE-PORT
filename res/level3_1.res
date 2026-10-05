// =============================================================================
// level3_1.res  Scene 3: la cloaca (sewer) -- 26/09, fondo nuevo el 28-29/09
// =============================================================================
// Fuentes: bg_sewer.png y bg_sewer_fg.png (exportadas de Aseprite, 1252x288, las
// dos capas del arcade). tools/gen_level3_1_bg.py las prepara (*_md.png, los
// .bin y la tira de caños de arriba). Ver el encabezado del script.
//
// pal_sewer / bg_sewer_tiles / bg_sewer_map
//             El fondo, filas 0..255 (1248x256 = 156x32 tiles): la camara sube
//             32 px sobre el recorte viejo (camYMin en level3_1.c). 2450 tiles
//             unicos: NO entra en un IMAGE, va en el formato ancho de
//             stage_bg.c y se streamea por columnas. Peor caso en 42
//             columnas: 861. PAL0, con el agua animada (indices 1 y 14).
// bg_sewer_fg Los caños que pasan por DELANTE, mismas 256 filas: entran
//             enteros y se dibujan en BG_A con prioridad alta. PAL0, el
//             indice 0 transparente.
// sewer_fg_top Lo de los caños que puede quedar DEBAJO DEL HUD (filas 0..63;
//             el HUD va en el plano WINDOW): 6 frames de 8x64, uno por
//             columna distinta; stage_level carga sus tiles una sola vez y
//             los 9 sprites (src/level3_1_fgtop.h) los comparten.
// =============================================================================
PALETTE pal_sewer      "images/lvl_3_sewer/bg_sewer_md.png"
BIN     bg_sewer_tiles "images/lvl_3_sewer/bg_sewer_tiles.bin" 2 2 0 NONE
BIN     bg_sewer_map   "images/lvl_3_sewer/bg_sewer_map.bin"   2 2 0 NONE
IMAGE   bg_sewer_fg    "images/lvl_3_sewer/bg_sewer_fg_md.png" NONE ALL
SPRITE  sewer_fg_top   "images/lvl_3_sewer/sewer_fg_top.png"   1 8 NONE 0

// (29/09) Misil que sale del AGUA cuando una tortuga camina por el canal, y
// su explosion. Arte de Traag (traag_missil / traag_explosao) remapeado a la
// paleta de los foot soldiers (PAL2) por tools/gen_sewer_missile.py: en la
// cloaca PAL3 es de Baxter. Misil: 2 frames de 64x32 (mira a la DERECHA).
// Explosion: 7 frames de 32x32 (impacta de derecha a izquierda), a mano.
SPRITE  sewer_missil   "sprites/sewer_missil.png"   8 4 NONE 4
SPRITE  sewer_explosao "sprites/sewer_explosao.png" 4 4 NONE 0
