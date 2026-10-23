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
// (18/10) techno_map.bin (411 KB) ya NO va a la ROM: pasaba los 4 MB. Va por
// bloques de 8x8 celdas (tools/pack_techno_map.py): bloques unicos + indice.
BIN     techno_blocks   "images/lvl_8_techno/techno_blocks.bin" 2 2 0 NONE
BIN     techno_bidx     "images/lvl_8_techno/techno_bidx.bin"   2 2 0 NONE
IMAGE   techno_elevator "images/lvl_8_techno/techno_elevator.png" NONE ALL

// (18/10) Peligros (tools/gen_techno_hazards.py, desde la plantilla
// "...Enemies_abelhas.png"; paleta del foot soldier BLANCO, van en PAL3).
// Las columnas, el laser y las sombras se dibujan con tiles cargados a mano
// (techno_hazards.c): opt NONE para que cada frame sea UN sprite de hardware
// con sus tiles completos y en orden.
//   techno_pillar_head  cabeza con el canon 32x32: 0 lisa, 1-3 con rayos,
//                       4 naranja lisa, 5-7 naranja con rayos
//   techno_pillar_seg   tramo del cuerpo 16x32, mismos frames
//   techno_laser        el tiro (4 eslabones) 32x8; techno_laser_sh su sombra
//   techno_pillar_sh    la sombra de la cabeza en el piso
//   techno_ball         la bola del pozo, 2 frames 32x32
SPRITE techno_pillar_head "images/lvl_8_techno/techno_pillar_head.png" 4 4 NONE 0 NONE NONE
SPRITE techno_pillar_seg  "images/lvl_8_techno/techno_pillar_seg.png"  2 4 NONE 0 NONE NONE
SPRITE techno_laser       "images/lvl_8_techno/techno_laser.png"       4 1 NONE 0 NONE NONE
SPRITE techno_laser_sh    "images/lvl_8_techno/techno_laser_sh.png"    4 1 NONE 0 NONE NONE
SPRITE techno_pillar_sh   "images/lvl_8_techno/techno_pillar_sh.png"   4 1 NONE 0 NONE NONE
SPRITE techno_ball        "images/lvl_8_techno/techno_ball.png"        4 4 NONE 0
