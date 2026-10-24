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
