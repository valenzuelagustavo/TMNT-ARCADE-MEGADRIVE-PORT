// =============================================================================
// final_bosses.res  Jefes de la sala final (9-1): KRANG y SHREDDER (06/10)
// =============================================================================
// Hojas armadas por tools/gen_final_bosses.py a partir de krang_boss.png y
// shredder_boss.png (frames sueltos -> grilla, anclados por la sombra). Todo
// mira a la DERECHA y se dibuja con PAL2: primero Krang, despues Shredder (se
// cargan al aparecer). Animaciones a mano (los .c fijan cada frame).
//
//   krang_boss      celdas de 144x144, pies en la fila 144 (ver krang.h)
//   krang_fist      64x24: el puno cohete, punta a la derecha (5 frames)
//   krang_head      32x32: fila 0 la cabeza que escapa (5), fila 1 su sombra
//   krang_eyebeam   40x32: un tramo del laser de los ojos, diagonal a 45
//                   grados (2 frames, titila). (13/10, tools/gen_krang_fx.py;
//                   reemplaza a krang_bolt, el rayo que caia del techo)
//   krang_zap       32x16: el chispazo contra el piso (2 frames)
//   krang_boom      64x64: la explosion de Baxter en la paleta de Krang, solo
//                   los cuartos de la izquierda (anim 0 arriba, 1 abajo; 8
//                   frames). (13/10, tools/gen_krang_fx.py)
//   shredder_boss   celdas de 160x96, pies en la fila 96 (ver shredder_boss.h)
//   shredder_beam_a / _b  136x152: las dos mitades del rayo de la espada
//                   (10 frames: crece y queda el tridente). Origen a la
//                   izquierda de la mitad A, en la fila 76.
//   shredder_fx     32x32: fila 0 chispitas, fila 1 llamarada, fila 2 brillo
//   shredder_helmet 16x16: el casco que sale volando
// =============================================================================
SPRITE krang_boss      "sprites/krang_boss_gen.png"      18 18 FAST 0
SPRITE krang_fist      "sprites/krang_fist_gen.png"       8  3 NONE 0
SPRITE krang_head      "sprites/krang_head_gen.png"       4  4 NONE 0
SPRITE krang_eyebeam   "sprites/krang_eyebeam_gen.png"    5  4 NONE 0
SPRITE krang_boom      "sprites/krang_boom_gen.png"       8  8 NONE 0
SPRITE krang_zap       "sprites/krang_zap_gen.png"        4  2 NONE 0
SPRITE shredder_boss   "sprites/shredder_boss_gen.png"   20 12 FAST 0
SPRITE shredder_beam_a "sprites/shredder_beam_a_gen.png" 17 19 FAST 0
SPRITE shredder_beam_b "sprites/shredder_beam_b_gen.png" 17 19 FAST 0
SPRITE shredder_fx     "sprites/shredder_fx_gen.png"      4  4 NONE 0
SPRITE shredder_helmet "sprites/shredder_helmet_gen.png"  2  2 NONE 0
