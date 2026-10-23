// =============================================================================
// roof_april_far.res - Cinematica del rescate de April: fondos y sprites (FAR)
// =============================================================================
// (23/10, rama mapper) Separado de roof_april.res para que vaya en la parte FAR
// de la ROM (puede quedar mas alla de los 3 MB, con bank switch): todo esto lo
// cargan las funciones de SGDK (VDP_loadTileSet, VDP_setTileMapEx con DMA
// inmediato, motor de sprites), que pasan por FAR_SAFE. El humo (TILESET que se
// lee crudo cuadro por cuadro) sigue en roof_april.res, NEAR.
// Ver roof_april.res para el detalle de las escenas, paletas y compresion.
// =============================================================================
IMAGE  roof_bg_a     "images/roof_april_scene/genesis/roof_bg_a.png"     NONE
IMAGE  roof_bg_b     "images/roof_april_scene/genesis/roof_bg_b.png"     NONE
IMAGE  roof_bg_c     "images/roof_april_scene/genesis/roof_bg_c.png"     NONE
SPRITE roof_climb     "images/roof_april_scene/genesis/roof_climb.png"     7 8 FAST 0
SPRITE roof_climb_far "images/roof_april_scene/genesis/roof_climb_far.png" 5 6 FAST 0
SPRITE roof_tiny     "images/roof_april_scene/genesis/roof_tiny.png"     6  8 FAST 0
SPRITE roof_far      "images/roof_april_scene/genesis/roof_far.png"      9 11 FAST 0
SPRITE roof_near     "images/roof_april_scene/genesis/roof_near.png"    13 15 FAST 0
SPRITE roof_splinter "images/roof_april_scene/genesis/roof_splinter.png" 6  8 FAST 0
SPRITE roof_baloon   "images/roof_april_scene/genesis/roof_baloon.png"   8  4 FAST 0
