// =============================================================================
// level_snow.res  Scene 3-1: Central Park con nieve (port de NES) -- 21/10
// =============================================================================
// (22/10) Fondo POR CAPAS con parallax (lvl_central_park/*.png, las capas
// separadas del artista). tools/gen_level_snow.py lo prepara todo:
//
// pal_snow / bg_snow_tiles / bg_snow_map
//             El FRENTE (alambrado y baranda baja, piso, carteles, canteros y
//             circulos de hielo), 2840x224, en BG_A a la velocidad de la
//             camara. Formato ancho de stage_bg, streameado por columnas con
//             el cache; lo vacio (indice 0) deja ver BG_B. PAL0.
// bg_snow_bands_tiles / bg_snow_band0_map / bg_snow_band1_map
//             Las BANDAS de BG_B (StageBands en stage_level.h): 0 = cielo +
//             arboles + baranda de arriba (filas 0-9, 1/4 de la camara),
//             1 = el rio (filas 10-15, 1/2). Tiles enteros en VRAM.
// snow_oneway (22/10) El cartel ONE WAY (placa one way.png del artista): 8
//             cuadros de una vuelta, celdas de 56x96 con el poste en x 24 y la
//             base abajo (como garage_oneway). Gira al golpearlo. PAL0.
// snow_trator_l / _r (22/10) El TRACTOR (trator.png del artista) en dos
//             mitades de 128x112 (rescomp no admite 32 tiles de ancho): 2
//             cuadros alineados por la rueda de adelante, apoyado abajo. Su PROPIA
//             paleta (indices 6-11 amarillos): va en PAL3. Cruza de derecha a
//             izquierda con la camara clavada (ver level_snow.c). Cortado con
//             TILE MEDIUM (menos tiles, mas sprites de VDP): 365 tiles.
// snow_flakes Los 8 tiles de los copos (sprites con tiles fijos).
// snow_flake_spr  La celda 8x8 del SPRITE de los copos (solo da la forma: los
//             tiles que se ven son los de snow_flakes).
// =============================================================================
PALETTE pal_snow       "images/lvl_central_park/bg_snow_md.png"
BIN     bg_snow_tiles  "images/lvl_central_park/bg_snow_tiles.bin" 2 2 0 NONE
BIN     bg_snow_map    "images/lvl_central_park/bg_snow_map.bin"   2 2 0 NONE
BIN     bg_snow_bands_tiles "images/lvl_central_park/bg_snow_bands_tiles.bin" 2 2 0 NONE
BIN     bg_snow_band0_map   "images/lvl_central_park/bg_snow_band0_map.bin"   2 2 0 NONE
BIN     bg_snow_band1_map   "images/lvl_central_park/bg_snow_band1_map.bin"   2 2 0 NONE
BIN     snow_flakes    "images/lvl_central_park/snow_flakes.bin"   2 2 0 NONE
SPRITE  snow_flake_spr "images/lvl_central_park/snow_flake_spr.png" 1 1 NONE 0
SPRITE  snow_oneway    "images/lvl_central_park/snow_oneway.png"    7 12 NONE 0
SPRITE  snow_trator_l  "images/lvl_central_park/snow_trator_l.png"  16 14 FAST 0 NONE TILE MEDIUM
SPRITE  snow_trator_r  "images/lvl_central_park/snow_trator_r.png"  16 14 FAST 0 NONE TILE MEDIUM
