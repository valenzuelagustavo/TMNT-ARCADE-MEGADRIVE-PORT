// =============================================================================
// intro_tmnt.res — Intro arcade (5 chunks verticales con overlap)
// =============================================================================
// Imagen original: intro.png (304x1560)
// Chunks cortados con overlap de 224 px para transiciones imperceptibles:
//   - intro_a.png: y=0..511
//   - intro_b.png: y=288..799
//   - intro_c.png: y=576..1087
//   - intro_d.png: y=864..1375
//   - intro_e.png: y=1152..1559
// =============================================================================

IMAGE intro_a "images/intro_tmnt/intro_a.png" BEST ALL
IMAGE intro_b "images/intro_tmnt/intro_b.png" BEST ALL
IMAGE intro_c "images/intro_tmnt/intro_c.png" BEST ALL
IMAGE intro_d "images/intro_tmnt/intro_d.png" BEST ALL
IMAGE intro_e "images/intro_tmnt/intro_e.png" BEST ALL

// --- Nubes de la intro (sprites de pantalla, comparten PAL0 con los chunks) ---
SPRITE nube_chica  "images/intro_tmnt/nube_chica.png" 11 4 NONE 0
SPRITE nube_grande "images/intro_tmnt/nube_grande.png" 26 5 NONE 0
