// =============================================================================
// intro.res — Gráficos de las escenas de introducción
// =============================================================================
// Contiene los sprites usados antes de llegar al menú principal:
//   - Intro SEGA (Rocksteady choca el logo)
//   - Intro KONAMI (src/konami_logo.c)
//   - Intro Arcade (agregar aquí cuando se implemente)
// =============================================================================

// --- Intro SEGA ---
SPRITE sega_logo_spr "sprites/logo_sega_14_5.png" 14 5 FAST 4
SPRITE rocksteady_spr "sprites/rocksteady_sprite.png" 11 11 FAST 10

// --- Intro KONAMI (14/10) ---
// Los arma tools/gen_konami_logo.py desde images/konami_logo/konami_logo_frames.png.
// Las dos imagenes son de 64 colores: el indice / 16 es la linea de paleta de
// cada tile (A: la pieza de arriba en PAL1; B: la de abajo en PAL2 y KONAMI en
// PAL0). Las paletas las pone el codigo (src/konami_logo_pal.h).
IMAGE  konami_logo_a "images/konami_logo/konami_logo_a.png" NONE ALL
IMAGE  konami_logo_b "images/konami_logo/konami_logo_b.png" NONE ALL
// La estrella (4 frames, a mano), la linea fina y la linea gruesa (PAL3).
SPRITE konami_star  "images/konami_logo/konami_star.png"  2 2 NONE 0
SPRITE konami_trail "images/konami_logo/konami_trail.png" 4 1 NONE 0
SPRITE konami_band  "images/konami_logo/konami_band.png"  4 2 NONE 0
