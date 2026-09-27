// =============================================================================
// level8_1.res  Scene 8: el Technodrome -- 27/09
// =============================================================================
// Arte del proyecto del companero (Ray Project, res/images/lvl_7_techno/).
//
// techno_tiles/map  El mapa: una L de 584x352 tiles (4672x2816 px), 2316 tiles
//                   unicos, en el formato ancho de stage_bg.c (indice u16 por
//                   celda; 0 = vacio). Lo arma tools/gen_level8_1.py a partir
//                   del mapa de Ray (techno_src_*.bin, que no van a la ROM),
//                   sacandole el ascensor que venia dibujado. Se streamea en
//                   2D en BG_B con PAL0.
// pal_techno        La paleta del mapa (el indice 0, magenta, es transparente).
// techno_elevator   El ascensor (320x128), misma paleta: se dibuja en BG_A.
// =============================================================================
PALETTE pal_techno      "images/lvl_8_techno/techno_pal.png"
BIN     techno_tiles    "images/lvl_8_techno/techno_tiles.bin" 2 2 0 NONE
BIN     techno_map      "images/lvl_8_techno/techno_map.bin"   2 2 0 NONE
IMAGE   techno_elevator "images/lvl_8_techno/techno_elevator.png" NONE ALL
