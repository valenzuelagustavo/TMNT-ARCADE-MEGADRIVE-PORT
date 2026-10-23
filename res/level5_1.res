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
//             plano): entero en VRAM, en BG_B con parallax. (12/10) Desde
//             bg_freeway_far_p0.png: los mismos tiles con los indices de la
//             paleta de la ruta, en PAL0 (PAL3 queda para los actores).
// =============================================================================
PALETTE pal_freeway       "images/lvl_5_freeway/bg_freeway.png"
BIN     bg_freeway_tiles  "images/lvl_5_freeway/bg_freeway_tiles.bin" 2 2 0 NONE
BIN     bg_freeway_map    "images/lvl_5_freeway/bg_freeway_map.bin"   2 2 0 NONE
IMAGE   bg_freeway_far    "images/lvl_5_freeway/bg_freeway_far_p0.png" NONE ALL

// (12/10) Actores de la autopista (tools/gen_level5_actors.py, desde los rips
// foot_car.png, foot_motocycle.png y foot_with_bomb.png):
//   fw_car_l / fw_car_r  las dos mitades (96x104) del auto lleno de foot
//                        soldiers; anim 0 rojo, 1 azul, 2 amarillo. PAL3
//                        (su paleta es la de la hoja).
//   fw_moto              la moto (120x64), en la paleta del auto (PAL3)
//   fw_thrower           el morado con el misil / la goma, celdas de 64x104:
//                        anim 0 camina con el misil, 1 lo tira, 2 camina con
//                        la goma, 3 la tira, 4 con las manos vacias. PAL2.
//   fw_missile / fw_tire lo que tiran (PAL2)
//   fw_pizza             la caja de pizza del 2-1 en la paleta de la ruta (PAL0)
SPRITE  fw_car_l   "images/lvl_5_freeway/fw_car_l.png"    12 13 NONE 0
SPRITE  fw_car_r   "images/lvl_5_freeway/fw_car_r.png"    12 13 NONE 0
SPRITE  fw_moto    "images/lvl_5_freeway/fw_moto.png"     15 8  NONE 0
SPRITE  fw_thrower "images/lvl_5_freeway/fw_thrower.png"  8 13  NONE 0
SPRITE  fw_missile "images/lvl_5_freeway/fw_missile.png"  8 4   NONE 0
SPRITE  fw_tire    "images/lvl_5_freeway/fw_tire.png"     8 7   NONE 0
// (13/10) la explosion de la bomba: los cuartos de la izquierda de la de
// Baxter (anim 0 arriba, 1 abajo; 8 frames de 64x64, PAL2); el codigo pone
// los de la derecha espejados
SPRITE  fw_bomb_boom "images/lvl_5_freeway/fw_bomb_boom.png" 8 8 NONE 0
SPRITE  fw_pizza   "images/lvl_5_freeway/fw_pizza.png"    5 6   NONE 0
// (23/10) Arte nuevo del artista:
//   fw_moto_alone  la moto SIN el foot soldier (120x64, misma paleta que
//                  fw_moto: PAL3). Cuando le pegan al de la moto, el soldier
//                  muere (la muerte del morado) y la moto sigue sola.
//   fw_valla       la valla con pinchos de la BARRICADA del final (vallas.png
//                  pasada a la paleta de la ruta por tools/gen_freeway_end.py:
//                  PAL0). 96x104.
//   fw_tire_pile   las gomas apiladas detras de las vallas (pile_tire.png,
//                  2 frames de 56x80: 0 tres gomas, 1 dos). Ya viene en el
//                  orden de indices de los foot soldiers: PAL2.
SPRITE  fw_moto_alone "images/lvl_5_freeway/fw_moto_alone-sheet.png" 15 8 NONE 0
SPRITE  fw_valla      "images/lvl_5_freeway/fw_valla_md.png"         12 13 NONE 0
SPRITE  fw_tire_pile  "images/lvl_5_freeway/pile_tire.png"           7 10 NONE 0
