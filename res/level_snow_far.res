// =============================================================================
// level_snow_far.res - Scene 3-1 (Central Park): el jefe y su utileria (FAR)
// =============================================================================
// (23/10, rama mapper) Separado de level_snow.res (NEAR): estos sprites solo
// los carga el motor de sprites (pasa por FAR_SAFE) y van comprimidos (FAST),
// asi pueden quedar mas alla de los 3 MB. Las paletas siempre quedan NEAR
// (rescomp las pone en .rodata_bin): tora.c las lee directo sin problema.
// Ver tools/mapper/README.md.
// =============================================================================
// El jefe TORA y su utileria (tools/gen_snow_boss.py, desde
// Tora_Boss.png, ice_ball.png y DISCO.png). Comparten UNA paleta: PAL3 (la
// cargan la lluvia de esferas, el disco y el jefe; el tractor, que tambien usa
// PAL3, nunca esta al mismo tiempo).
//   snow_tora       104x104, pies en el borde de abajo, mira a la derecha:
//                   0 camina (4) 1 espera la esfera / la lanza (2)
//                   2 golpe (2) 3 recibe un golpe (1) 4 muere (3)
//   snow_iceball    80x72: 0 la esfera, 1-2 se rompe
//   snow_iceshadow  56x24: la sombra que avisa donde cae
//   snow_disco      64x64: la esfera que flota al final (3 cuadros)
//   snow_disco_sh   56x16: su sombra
SPRITE  snow_tora      "images/lvl_central_park/snow_tora.png"      13 13 FAST 0
SPRITE  snow_iceball   "images/lvl_central_park/snow_iceball.png"   10 9  FAST 0
SPRITE  snow_iceshadow "images/lvl_central_park/snow_iceshadow.png" 7 3   FAST 0
SPRITE  snow_disco     "images/lvl_central_park/snow_disco.png"     8 8   FAST 0
SPRITE  snow_disco_sh  "images/lvl_central_park/snow_disco_sh.png"  7 2   FAST 0
// (24/10) El globo de Tora al entrar: "Time to put you on ice!" (96x32, PAL1
// como todos los globos). Armado por tools/gen_globos.py (COMPUESTOS).
SPRITE  tora_ice_bubble "sprites/tora_ice_baloon.png" 12 4 FAST 0
// (27/10) FROSTY, el muneco de nieve robot (tools/gen_frosty.py, desde
// frosty-sheet.png). Todo comparte UNA paleta (PAL3, la carga frosty.c). Hojas
// de una fila; el codigo elige el cuadro (medidas en src/frosty_art.h):
//   frosty_snow    64x112: 0 en el piso, 1 saltando
//   frosty_break   104x104: 0-2 se rompe el disfraz
//   frosty_bot     80x120: 0 piso, 1-2 salto, 3 golpeado, 4 muerto, 5 sin nariz, 6 regenera
//   frosty_missile 64x24: la nariz, 0 sin fuego / 1 con fuego (mira a la derecha)
//   frosty_shadow  40x24
//   frosty_boom    la explosion de Baxter con la paleta de Frosty (como baxter_boom)
SPRITE  frosty_snow    "images/lvl_central_park/frosty_snow.png"    8 14 FAST 0
SPRITE  frosty_break   "images/lvl_central_park/frosty_break.png"   13 13 FAST 0
SPRITE  frosty_bot     "images/lvl_central_park/frosty_bot.png"     10 15 FAST 0
SPRITE  frosty_missile "images/lvl_central_park/frosty_missile.png" 8 3 FAST 0
SPRITE  frosty_shadow  "images/lvl_central_park/frosty_shadow.png"  5 3 FAST 0
SPRITE  frosty_boom    "images/lvl_central_park/frosty_boom.png"    8 8 FAST 0
