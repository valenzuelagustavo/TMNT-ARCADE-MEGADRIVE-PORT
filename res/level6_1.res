// =============================================================================
// level6_1.res  Scene 6: la segunda autopista (skate) -- 26/09
// =============================================================================
// Arte del proyecto del companero (Ray Project, res/images/lvl_6_scene/).
//
// bg_skate      La ruta (bg_freeway2_near.png de Ray, 6376x240, recortado a
//               las 224 filas de abajo). El indice 0 (magenta) es
//               TRANSPARENTE: arriba del guardarrail se ve la capa lejana.
//               1702 tiles unicos (entra en un IMAGE); se streamea en BG_A,
//               PAL0. Peor caso en 42 columnas: 415 tiles.
// bg_skate_far  Cielo con nubes y skyline (bg_l6s_far.png de Ray, 512x256):
//               entero en VRAM, en BG_B con parallax, PAL3.
// =============================================================================
// (18/10) bg_skate ya no es un IMAGE: formato ancho con DICCIONARIO DE
// COLUMNAS (tools/gen_level6_skate_cols.py, desde el mismo bg_skate_p.png).
// 525 columnas unicas de 797: ~31 KB de mapa en vez de 44,6 KB.
PALETTE pal_skate     "images/lvl_6_skate/bg_skate_p.png"
BIN     skate_tiles   "images/lvl_6_skate/skate_tiles.bin"  2 2 0 NONE
BIN     skate_cols    "images/lvl_6_skate/skate_cols.bin"   2 2 0 NONE
BIN     skate_coloff  "images/lvl_6_skate/skate_coloff.bin" 2 2 0 NONE
IMAGE bg_skate_far "images/lvl_6_skate/bg_skate_far_p0.png" NONE ALL

// (12/10) La version en PATINETA (tools/gen_level6_skate.py):
//   bg_skate_p / bg_skate_far_p0  la ruta con 3 celestes en sus indices libres
//                 (9, 13, 14) y el cielo pasado a esa paleta: los dos en PAL0,
//                 PAL3 queda para el helicoptero.
//   sk_cop_body / sk_cop_rotor  el helicoptero (FootSoldier_helicopter.png):
//                 el cuerpo (1 frame) y el rotor (2 frames), PAL3 (su paleta).
//                 Cortados con opt SPRITE: 9 + 5 sprites de hardware.
//   sk_copter_purple  la misma paleta con los rosas en violeta
//   sk_bullet / sk_flame  la bala y la llamita (sacadas del video), PAL3
//   sk_board_turtle / sk_board_foot  las patinetas (la de las tortugas sacada
//                 del video), 64x24, PAL1
//   sk_shadow     la sombra del helicoptero en la ruta, PAL0
SPRITE  sk_cop_body      "images/lvl_6_skate/sk_cop_body.png"     14 15 NONE 0 NONE SPRITE SLOW
SPRITE  sk_cop_rotor     "images/lvl_6_skate/sk_cop_rotor.png"    21 10 NONE 0 NONE SPRITE SLOW
PALETTE sk_copter_purple "images/lvl_6_skate/sk_copter_purple.png"
SPRITE  sk_bullet        "images/lvl_6_skate/sk_bullet.png"       2 2  NONE 0
SPRITE  sk_flame         "images/lvl_6_skate/sk_flame.png"        2 3  NONE 0
SPRITE  sk_board_turtle  "images/lvl_6_skate/sk_board_turtle.png" 8 3  NONE 0
SPRITE  sk_board_foot    "images/lvl_6_skate/sk_board_foot.png"   8 3  NONE 0
SPRITE  sk_shadow        "images/lvl_6_skate/sk_shadow.png"       6 3  NONE 0
