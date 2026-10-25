// =============================================================================
// level8_1_far.res - Scene 8 (Technodrome): el tubo de spray congelante (FAR)
// =============================================================================
// (25/10, rama mapper) Sin NEAR: solo los carga el motor de sprites y van
// comprimidos (FAST). Paleta: la del foot soldier blanco (PAL3, la del nivel).
// Ver techno_tubes.c y tools/gen_techno_tube.py (desde la hoja del arcade
// "Enemies - Robot Enemies").
//   techno_tube   8 cuadros 16x56: 0 disco, 1 disco abierto, 2-4 saliendo,
//                 5 entero, 6 entero disparando, 7 destruido. Cuadros a mano.
//   techno_spray  4 cuadros 64x64: la nube de spray (0/1 plena, 2/3 rala).
// =============================================================================
SPRITE  techno_tube   "images/lvl_8_techno/techno_tube.png"   2 7 FAST 0
SPRITE  techno_spray  "images/lvl_8_techno/techno_spray.png"  8 8 FAST 0
