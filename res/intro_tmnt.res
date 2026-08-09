// =============================================================================
// intro_tmnt.res — Intro arcade (secuencia de titulo estilo TMNT arcade)
// =============================================================================
// Cinco fases en scenes.c (showArcadeIntro):
//   P1  fondo_1 estatico + nubes (sprites)
//   P2  fondo_a barre sobre fondo_1 (wipe vertical, BG_A sobre BG_B)
//   P3  rayas fondo_b (scroll vertical rapido con wrap y aceleracion)
//   P4  fondo_a baja hasta llenar la pantalla
//   P5  fondo_2 panea reemplazando fondo_a -> SCENE_PLAYER_SELECT
//
// Presupuesto de VRAM (plano 64x64 = 512x512 px, SPR_initEx(420)):
//   P1/P2: fondo_1 (271) + fondo_a (372) = 643 tiles
//   P3:    fondo_a (372) + fondo_b (32)  = 404 tiles
//   P4/P5: fondo_a (372) + fondo_2 (439) = 811 tiles <= 813 disponibles
// Las nubes (44 + 130 tiles) viven en la region de sprites (420 tiles).
//
// Compresion BEST: son imagenes de un solo uso (no se streamean ni se indexan
// desde ROM), como bg_a_final/bg_b_final en level1.res.
// map_opt ALL (default): dedup de tiles -> fondos de pocos cientos de tiles.
// =============================================================================

IMAGE fondo_1 "images/intro_tmnt/fondo_1.png" BEST ALL
IMAGE fondo_a "images/intro_tmnt/fondo_a.png" BEST ALL
IMAGE fondo_b "images/intro_tmnt/fondo_b.png" BEST ALL
IMAGE fondo_2 "images/intro_tmnt/fondo_2.png" BEST ALL

// --- Nubes de la fase 1 (sprites de pantalla, comparten PAL0 con fondo_1) ---
SPRITE nube_chica  "images/intro_tmnt/nube_chica.png" 11 4 NONE 0
SPRITE nube_grande "images/intro_tmnt/nube_grande.png" 26 5 NONE 0
