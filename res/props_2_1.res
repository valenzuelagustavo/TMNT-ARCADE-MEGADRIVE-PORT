// =============================================================================
// props_2_1.res  Objetos fisicos del nivel 2-1 (25/09)
// =============================================================================
// PARQUIMETROS. Los dos PNG los genera tools/gen_parking_meter.py a partir del
// sprite de Gustavo (res/sprites/packing_meter.png, 3 frames de 56x64). Estan
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
