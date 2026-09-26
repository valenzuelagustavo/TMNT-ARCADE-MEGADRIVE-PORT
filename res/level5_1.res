// =============================================================================
// level5_1.res  Scene 5: la autopista (freeway) -- 26/09
// =============================================================================
// Arte del proyecto del companero (Ray Project, res/images/lvl_6_scene/).
//
// bg_freeway  La ruta con los edificios (bg_freeway_near.png de Ray, 2104x256,
//             recortado a las 224 filas de abajo). 15 colores; el indice 0
//             (negro) es TRANSPARENTE: por los huecos se ve la capa lejana.
//             Tiene 2369 tiles unicos: NO entra en un IMAGE (indice de 11
//             bits). tools/gen_level5_1.py arma tiles + mapa en el formato
//             ancho de stage_bg.c; se streamea en BG_A con PAL0. Peor caso en
//             42 columnas: 716 tiles.
// bg_freeway_far  El skyline (bg_l6_far.png de Ray, 512x256 = el ancho del
//             plano): entero en VRAM, en BG_B con parallax, PAL3.
// =============================================================================
PALETTE pal_freeway       "images/lvl_5_freeway/bg_freeway.png"
BIN     bg_freeway_tiles  "images/lvl_5_freeway/bg_freeway_tiles.bin" 2 2 0 NONE
BIN     bg_freeway_map    "images/lvl_5_freeway/bg_freeway_map.bin"   2 2 0 NONE
IMAGE   bg_freeway_far    "images/lvl_5_freeway/bg_freeway_far.png"   NONE ALL
