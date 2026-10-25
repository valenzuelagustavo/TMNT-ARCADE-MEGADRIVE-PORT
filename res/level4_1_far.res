// =============================================================================
// level4_1_far.res - Scene 4 (garage): la salida con la capsula del taladro (FAR)
// =============================================================================
// (25/10, rama mapper) Sin NEAR: solo los carga el motor de sprites (pasan por
// FAR_SAFE) y van comprimidos (FAST). Las paletas quedan NEAR (rescomp). Ver
// tools/mapper/README.md.
// =============================================================================
// garage_capsule  tools/gen_drill_capsule.py, desde 17 capturas del arcade
//                 (tools/drill_capsule_src). 104x184, 17 cuadros en una sola
//                 animacion: la capsula sale del piso girando (cuadro 0: solo
//                 la punta del taladro; 16: ya entera). El piso es el borde de
//                 abajo del cuadro y el centro de la capsula, x 52. 8 grises,
//                 su PROPIA paleta (PAL2: los jefes ya no estan). Cuadros a mano.
// cork_bubble     el globo de Leo ("Oh,no.it's that overgrown cork-screw again",
//                 128x32, el de la hoja del arcade: tools/gen_globos.py), PAL1.
SPRITE  garage_capsule "images/lvl_4_garage/garage_capsule.png" 13 23 FAST 0
SPRITE  cork_bubble    "sprites/cork_screw_baloon.png"          16 4  FAST 0
