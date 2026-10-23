// =============================================================================
// level7_1.res  Scene 7: la fabrica (factory) -- 26/09
// =============================================================================
// Arte del proyecto del companero (Ray Project, res/images/factory/).
//
// bg_factory     2024x224, 16 colores, 1600 tiles unicos: se streamea
//                (stage_bg.c) en BG_B con PAL0. Peor caso en 42 columnas: 358.
//                El indice 0 es un color de verdad (negro): el color de fondo
//                del VDP se fija a PAL0[0].
// bg_factory_fg  Las dos columnas de reticulado que pasan por DELANTE (21
//                tiles): enteras en VRAM, en BG_A con prioridad alta. Misma
//                paleta que el fondo (PAL0).
// La colision (col_factory.bin) la lee tools/gen_level7_1.py y no va a la ROM.
// =============================================================================
IMAGE bg_factory    "images/lvl_7_factory/bg_factory.png"    NONE ALL
IMAGE bg_factory_fg "images/lvl_7_factory/bg_factory_fg.png" NONE ALL
// (18/10) Lo de las columnas que cae en la franja del HUD (filas 0..3 de BG_A,
// que no scrollean): frames de 8x32 que stage_level muestra como sprites
// (fgTop). Sale de tools/gen_level7_1_fgtop.py (con src/level7_1_fgtop.h).
SPRITE factory_fg_top "images/lvl_7_factory/factory_fg_top.png" 1 4 NONE 0
// (18/10) INTRO: los Mousers llevandose a Splinter cruzan la pantalla hacia la
// derecha antes de que entren las tortugas. 3 frames de 72x64 (9x8 tiles),
// paleta propia (va en PAL3, que en la fabrica esta libre hasta Granitor).
// Sale de tools/gen_factory_intro.py (desde mousers_splinter.png).
SPRITE mousers_splinter "images/lvl_7_factory/mousers_splinter_gen.png" 9 8 NONE 0

// (18/10) SPLINTER ATADO en la pared de la sala de Granitor (0 quieto, 1-3
// forcejea), de tools/gen_factory_splinter.py (desde splinter_tide.png), con
// los colores ya en la paleta de las tortugas (PAL1):
//   splinter_tide_ts  el cuerpo, 4 frames de 5x10 tiles uno abajo del otro,
//                     SIN optimizar (los 50 tiles de cada frame seguidos):
//                     va en BG_A como overlay de stage_level
//   splinter_rope     la soga en la franja del HUD, sprite 1x4 tiles, 4 frames
TILESET splinter_tide_ts "images/lvl_7_factory/splinter_tide_ts.png" NONE NONE
SPRITE splinter_rope "images/lvl_7_factory/splinter_rope.png" 1 4 NONE 0

// (18/10) Actores de la fabrica (tools/gen_factory_fx.py, ver factory_fx.c):
//   factory_capsule   la capsula del taladro del 1-2 en la paleta de las
//                     tortugas (PAL1, blanca): anim 0 el taladro sale (9), anim 1 la puerta (2)
//   factory_bee       abejas 32x32 (PAL3, la del blanco): anims 0-2 azul a la
//                     izquierda / de frente / a la derecha, 3-5 naranja (golpe);
//                     3 frames (la helice)
//   factory_bee_shot  sus tiros 16x24: "/" "|" "\"
//   factory_laser     el laser de la pared, tramo 8x32 (PAL1): blanco 6 px / 4 px
SPRITE factory_capsule  "images/lvl_7_factory/factory_capsule_gen.png" 12 15 NONE 0
SPRITE factory_bee      "images/lvl_7_factory/factory_bee_gen.png"      4 4 NONE 0
SPRITE factory_bee_shot "images/lvl_7_factory/factory_bee_shot_gen.png" 2 3 NONE 0
SPRITE factory_laser    "images/lvl_7_factory/factory_laser_gen.png"    1 4 NONE 0
