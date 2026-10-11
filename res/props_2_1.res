// =============================================================================
// props_2_1.res  Objetos fisicos del nivel 2-1 (25/09)
// =============================================================================
// PARQUIMETROS. Los dos PNG los genera tools/gen_parking_meter.py a partir del
// sprite original (res/sprites/packing_meter.png, 3 frames de 56x64). Estan
// reindexados contra la paleta del FONDO del 2-1 (lvl21_pal): se dibujan con
// PAL0 y no llevan paleta propia en uso.
//
// Van partidos en dos a proposito: SGDK reserva por sprite los tiles de su
// frame mas caro, y el parquimetro parado es un palo de 2 columnas mientras
// que volando ocupa casi toda la celda. Asi los quietos cuestan poco.
//
//   parking_meter_stand   16x64 (2x8 tiles), 1 frame: firme en la vereda.
//                         El palo queda en x=8 de la celda; pies = borde de abajo.
//   parking_meter_fly     56x64 (7x8 tiles), 2 frames: [0] recibe el golpe,
//                         [1] arrancado y volando. El palo del frame parado
//                         corresponde a x=26 de esta celda.
// time 0: los frames los elige el codigo (src/meters_2_1.c).
// =============================================================================
SPRITE parking_meter_stand "sprites/parking_meter_stand_gen.png" 2 8 FAST 0
SPRITE parking_meter_fly   "sprites/parking_meter_fly_gen.png"   7 8 FAST 0

// -----------------------------------------------------------------------------
// TV DE LA VIDRIERA "ELECTRONICS" (26/09) -- arte del proyecto del companero
// (tv_april-Sheet.png): 4 frames de 56x48 (7x6 tiles) = la PANTALLA del
// televisor, que calza exacta sobre la pantalla azul oscura del fondo en
// mundo (480, 72). Frames 0-2 April hablando, 3 Shredder interrumpiendo.
// Tiene paleta PROPIA (15 colores): va en PAL3, que en el 2-1 esta libre
// hasta que aparece Bebop (ver level2_1.c). time 30 = medio segundo por frame.
// -----------------------------------------------------------------------------
SPRITE tv_april "sprites/tv_april.png" 7 6 FAST 30

// Globos de dialogo de la TV (27/09). 64x32 (8x4 tiles, 32 tiles cada uno;
// nunca conviven: primero uno y despues el otro). Llegaron con paleta propia
// de 5 colores y se remapearon a la de las TORTUGAS (PAL1), como el globo de
// "who put the light out": relleno 11, borde 9, texto 8, sombra del texto 7.
// El de "Help!" media 64x30: se relleno con 2 filas transparentes ARRIBA
// (la cola apunta abajo, a la tele). Los originales: *_orig.png.
SPRITE tv_help_bubble "sprites/help_baloon.png" 8 4 NONE 0
SPRITE tv_dine_bubble "sprites/tonight_i_dinne_baloon.png" 8 4 NONE 0

// -----------------------------------------------------------------------------
// UTILERIA DE LA CALLE (11/10) -- tools/gen_level2_1_props.py
// -----------------------------------------------------------------------------
// Posiciones medidas sobre gameplayARCADE/tmnt.avi (SCENE 2 desde 3:07): ver
// src/street_2_1.c. Todo PAL0 (paleta del fondo) salvo la chica en skate, que
// trae paleta propia y va en PAL3 (la de la TV, que para entonces ya termino).
//
//   l21_win_front      5x10  ventana del frente rota (solo el vidrio: lo que
//                            es igual al fondo quedo transparente)
//   l21_win_side       3x10  ventana de la pared en diagonal, rota.
//                            anim [0] marco naranja (Robodyne), [1] edificio K
//   l21_hydrant        4x6   hidrante quieto
//   l21_hydrant_spray 10x9   4 frames de rocio (el cuerpo siempre en x=5)
//   l21_hydrant_jet   14x4   la tapa que sale volando con el chorro
//   l21_bill_front    14x8   el cartel "Stunner" colgado en la pared
//   l21_bill_flat     16x4   [0] cayendo, [1] tirado en la vereda
//   l21_skategirl      7x8   [0] parada, [1] agachada
// time 0 en todos: los frames los elige el codigo.
// -----------------------------------------------------------------------------
SPRITE l21_win_front     "sprites/l21_win_front.png"      5 10 FAST 0
SPRITE l21_win_side      "sprites/l21_win_side.png"       3 10 FAST 0
SPRITE l21_hydrant       "sprites/l21_hydrant.png"        4 6  FAST 0
SPRITE l21_hydrant_spray "sprites/l21_hydrant_spray.png" 10 9  FAST 0
SPRITE l21_hydrant_jet   "sprites/l21_hydrant_jet.png"   14 4  FAST 0
SPRITE l21_bill_front    "sprites/l21_bill_front.png"    14 8  FAST 0
SPRITE l21_bill_flat     "sprites/l21_bill_flat.png"     16 4  FAST 0
SPRITE l21_skategirl     "sprites/l21_skategirl.png"      7 8  FAST 0
// (11/10) La pizza de la cornisa: 4 frames de 40x48 (cerrada, abriendo, abierta
// con "ENERGY"). Sale de res/sprites/pizza.png. PAL0.
SPRITE l21_pizza         "sprites/l21_pizza.png"          5 6  FAST 0

// (11/10) Globos de voice over del 2-1, cortados de la hoja de globos del
// arcade (res/images/globos/globos_arcade.png) por tools/gen_globos.py, con el
// remapeo a PAL1 de siempre (relleno 11, borde 9, texto 8, sombra 7).
//   pizza_time_bubble    "Pizza time!" sobre la pizza de la cornisa
//   youre_dead_bubble    Bebop: "You're dead, shell-brain"
//   any_prettier_bubble  tortuga: "They aint getting any prettier!"
//   watch_mouth_bubble   Bebop: "Watch your mouth, slime ball!!"
SPRITE pizza_time_bubble   "sprites/pizza_time_baloon.png"    8 4 NONE 0
SPRITE youre_dead_bubble   "sprites/youre_dead_baloon.png"    8 4 NONE 0
SPRITE any_prettier_bubble "sprites/any_prettier_baloon.png" 12 4 NONE 0
SPRITE watch_mouth_bubble  "sprites/watch_mouth_baloon.png"  12 4 NONE 0
