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
// (23/10, rama mapper) NEAR: todo lo de este archivo va en los primeros
// 3 MB de la ROM (sin bank switch). Lo lee el codigo directo de la ROM
// (fondos streameados, tiles por cuadro, mapas) o lo toca el Z80 (audio).
// Ver tools/mapper/README.md.
NEAR

PALETTE pal_techno      "images/lvl_8_techno/techno_pal.png"
// (26/10) Paleta del hielo de la tortuga congelada (se carga en PAL2, ver player.c)
PALETTE pal_frozen      "sprites/turtle_frosen.png"
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
SPRITE techno_pillar_head "images/lvl_8_techno/techno_pillar_head.png" 4 4 FAST 0 NONE NONE
SPRITE techno_pillar_seg  "images/lvl_8_techno/techno_pillar_seg.png"  2 4 FAST 0 NONE NONE
SPRITE techno_laser       "images/lvl_8_techno/techno_laser.png"       4 1 FAST 0 NONE NONE
SPRITE techno_laser_sh    "images/lvl_8_techno/techno_laser_sh.png"    4 1 FAST 0 NONE NONE
SPRITE techno_pillar_sh   "images/lvl_8_techno/techno_pillar_sh.png"   4 1 FAST 0 NONE NONE
SPRITE techno_ball        "images/lvl_8_techno/techno_ball.png"        4 4 FAST 0

// (29/10) La PUERTA del fondo de la sala de Traag (tools/gen_techno_door.py,
// desde las capturas del arcade en tools/techno_door_src). La cerrada (48x144,
// ~70 tiles) se dibuja sobre el hueco que el mapa trae ya abierto; al entrar
// Traag se cambia por la que sale volando (64x96, ~78 tiles, una sola imagen
// que cruza la pantalla). Usan la paleta del mapa (PAL0, la de pal_techno).
SPRITE techno_door_closed "images/lvl_8_techno/techno_door_closed.png" 6 18 FAST 0
SPRITE techno_door_fly    "images/lvl_8_techno/techno_door_fly.png"    8 12 FAST 0

// (30/10) La barra LASER de la puerta (tools/gen_techno_laser.py): 2 cuadros de
// 8x104 (1x13 tiles) con los colores de las tortugas, asi que se dibuja en PAL1.
// Se usa 3 veces (techno_lasers.c).
SPRITE techno_laser_bar   "images/lvl_8_techno/techno_laser_bar.png"   1 13 FAST 0
