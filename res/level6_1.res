// =============================================================================
// level6_1.res  Scene 6: la segunda autopista (skate) -- 26/09
// =============================================================================
// Arte del proyecto del companero (Ray Project, res/images/lvl_6_scene/).
//
// bg_skate      La ruta (bg_freeway2_near.png de Ray, 6376x240, recortado a
//               las 224 filas de abajo). El indice 0 (magenta) es
//               TRANSPARENTE: arriba del guardarrail se ve la capa lejana.
//               1702 tiles unicos (entra en un IMAGE); se streamea en BG_A,
//               PAL0. Peor caso en 42 columnas: 415 tiles.
// bg_skate_far  Cielo con nubes y skyline (bg_l6s_far.png de Ray, 512x256):
//               entero en VRAM, en BG_B con parallax, PAL3.
// =============================================================================
IMAGE bg_skate     "images/lvl_6_skate/bg_skate.png"     NONE ALL
IMAGE bg_skate_far "images/lvl_6_skate/bg_skate_far.png" NONE ALL
