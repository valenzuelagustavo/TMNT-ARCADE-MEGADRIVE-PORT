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
//             variantes de dos regiones (el lugar del auto vacio y la puerta
//             del ascensor abriendose por la mitad, ver src/level4_1_bg.h). PAL0.
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
//   garage_pizza   (12/10) la caja de "Pizza time!" (la del 2-1 en PAL0)
//   april_garage   (03/10) April atada adentro del ascensor. april_gen.png
//                  (tools/gen_april_sheet.py): celdas de 32x80 (12/10; antes
//                  32x64), anim 0 parada, 1 atada, 2 se le caen las sogas,
//                  3 saludando. Paleta COMPARTIDA con Bebop y Rocksteady
//                  (PAL3). time 12: balanceo de 2 frames.
// =============================================================================
// (23/10, rama mapper) NEAR: todo lo de este archivo va en los primeros
// 3 MB de la ROM (sin bank switch). Lo lee el codigo directo de la ROM
// (fondos streameados, tiles por cuadro, mapas) o lo toca el Z80 (audio).
// Ver tools/mapper/README.md.
NEAR

PALETTE pal_garage      "images/lvl_4_garage/bg_garage_md.png"
BIN     bg_garage_tiles "images/lvl_4_garage/bg_garage_tiles.bin" 2 2 0 NONE
BIN     bg_garage_map   "images/lvl_4_garage/bg_garage_map.bin"   2 2 0 NONE
BIN     bg_garage_var   "images/lvl_4_garage/bg_garage_var.bin"   2 2 0 NONE

SPRITE  garage_sign20   "images/lvl_4_garage/garage_sign20.png"  6 12 FAST 0
// (14/10) El cartel "20 MPH" arrancado: [0] recibe el golpe, [1] volando
// (tools/gen_garage_sign_fly.py, el cartel parado rotado sobre el pie).
SPRITE  garage_sign20_fly "images/lvl_4_garage/garage_sign20_fly.png" 12 12 FAST 0
SPRITE  garage_oneway   "images/lvl_4_garage/garage_oneway.png"  7 12 FAST 0
SPRITE  garage_cone     "images/lvl_4_garage/garage_cone.png"    3 4  FAST 0
SPRITE  garage_barrel   "images/lvl_4_garage/garage_barrel.png"  4 8  FAST 0
SPRITE  garage_car_l    "images/lvl_4_garage/garage_car_l.png"   11 13 FAST 0
SPRITE  garage_car_r    "images/lvl_4_garage/garage_car_r.png"   11 13 FAST 0
SPRITE  garage_boom     "images/lvl_4_garage/garage_boom.png"    8 8  FAST 0
SPRITE  garage_pizza    "images/lvl_4_garage/garage_pizza.png"   5 6  FAST 0
SPRITE  april_garage    "sprites/april_gen.png"                  4 10 FAST 12

// (30/10) La camioneta de las tortugas del final (tools/gen_garage_van.py,
// src/garage_van.c): dibujo de BG_A de 30x18 tiles que se desliza con el scroll.
//   van_tiles  468 tiles 4bpp unicos (con flips; el 0 es vacio)
//   van_map    4 estados de la puerta (0 cerrada .. 3 abierta) x 18x30 u16 (flips | indice)
//   pal_van    los 15 colores de TURTLE_VAN.png; va en PAL2 (los jefes ya no estan)
BIN     van_tiles "images/lvl_4_garage/van_tiles.bin" 2 2 0 NONE
BIN     van_map   "images/lvl_4_garage/van_map.bin"   2 2 0 NONE
PALETTE pal_van   "images/lvl_4_garage/van_pal_gen.png"
