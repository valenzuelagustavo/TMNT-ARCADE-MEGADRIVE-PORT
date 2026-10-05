// =============================================================================
// level4_1.res  Scene 4: el estacionamiento (garage) -- 26/09, fondo nuevo 01/10
// =============================================================================
// (01/10) Fondo y props nuevos (bg_garage_new.png + assets, ver
// tools/gen_level4_1_bg.py, que arma todo lo de abajo).
//
// pal_garage / bg_garage_tiles / bg_garage_map / bg_garage_var
//             El fondo, 1288x240 (161x30 tiles; 16 px de piso mas que el de
//             Ray: la camara baja hasta ahi, camYMax en level4_1.c). Con el
//             AUTO estacionado y la PUERTA del ascensor ya pintados. 1968
//             tiles unicos (formato ancho de stage_bg, se streamea por
//             columnas; peor caso en 42 columnas: 656). bg_garage_var son las
//             variantes de dos regiones (el lugar del auto vacio y la persiana
//             del ascensor subiendo, ver src/level4_1_bg.h). PAL0.
// bg_garage   El de Ray (1288x224), queda como referencia: ya no se usa.
//
// Props (todos con la paleta del fondo, PAL0; el borde de abajo de la celda
// es la base del prop; frames a mano):
//   garage_sign20  cartel "20 MPH": [0] parado, [1] torcido (al golpearlo)
//   garage_oneway  cartel "ONE WAY": 8 frames, una vuelta entera al golpearlo
//   garage_cone    cono (vuela al golpearlo)
//   garage_barrel  barril explosivo: [0] quieto, [1..4] la mecha
//   garage_car_l/r el auto que sale del estacionamiento, en dos mitades
//   garage_boom    (01/10) la explosion del barril: la del TNT del foot
//                  soldier (explosion.png, 7 frames de 64x64) en PAL0
//   april_garage   (03/10) April atada adentro del ascensor. april_gen.png
//                  (tools/gen_april_sheet.py): celdas de 32x64, anim 0 parada,
//                  anim 1 atada (la que se usa aca). Paleta COMPARTIDA con
//                  Bebop y Rocksteady (PAL3). time 12: balanceo de 2 frames.
// =============================================================================
PALETTE pal_garage      "images/lvl_4_garage/bg_garage_md.png"
BIN     bg_garage_tiles "images/lvl_4_garage/bg_garage_tiles.bin" 2 2 0 NONE
BIN     bg_garage_map   "images/lvl_4_garage/bg_garage_map.bin"   2 2 0 NONE
BIN     bg_garage_var   "images/lvl_4_garage/bg_garage_var.bin"   2 2 0 NONE

SPRITE  garage_sign20   "images/lvl_4_garage/garage_sign20.png"  6 12 NONE 0
SPRITE  garage_oneway   "images/lvl_4_garage/garage_oneway.png"  7 12 NONE 0
SPRITE  garage_cone     "images/lvl_4_garage/garage_cone.png"    3 4  NONE 0
SPRITE  garage_barrel   "images/lvl_4_garage/garage_barrel.png"  4 8  NONE 0
SPRITE  garage_car_l    "images/lvl_4_garage/garage_car_l.png"   11 13 NONE 0
SPRITE  garage_car_r    "images/lvl_4_garage/garage_car_r.png"   11 13 NONE 0
SPRITE  garage_boom     "images/lvl_4_garage/garage_boom.png"    8 8  NONE 0
SPRITE  april_garage    "sprites/april_gen.png"                  4 8  NONE 12
