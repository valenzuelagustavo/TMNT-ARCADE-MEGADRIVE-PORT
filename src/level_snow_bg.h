// GENERADO por tools/gen_level_snow.py -- no editar a mano
#ifndef _LEVEL_SNOW_BG_H_
#define _LEVEL_SNOW_BG_H_

#define LVLSNOW_W          3312  // ancho del nivel (px)
#define LVLSNOW_BG_W       414   // frente: celdas
#define LVLSNOW_BG_ROWS    28
#define LVLSNOW_BG_TILES   1355  // frente: tiles unicos
#define LVLSNOW_BG_WORST   488   // frente: peor caso en 42 columnas
#define LVLSNOW_FLAKES     8     // tiles de copos (snow_flakes)
#define LVLSNOW_WALK_Y_MIN 132   // tope de los pies fuera de los obstaculos

// Bandas de parallax en BG_B (tiles: bg_snow_bands_tiles, mapas u16
// con el indice en los bits 0-12 y los flips en 13/14, como stage_raw).
#define LVLSNOW_BAND_TILES 333
#define LVLSNOW_BAND_WORST 255   // con cache: peor caso en pantalla
#define LVLSNOW_FAR_ROW0   0
#define LVLSNOW_FAR_ROWS   10
#define LVLSNOW_FAR_W      135   // celdas
#define LVLSNOW_FAR_NUM    1
#define LVLSNOW_FAR_DEN    4
#define LVLSNOW_RIV_ROW0   10
#define LVLSNOW_RIV_ROWS   6
#define LVLSNOW_RIV_W      229   // celdas
#define LVLSNOW_RIV_NUM    1
#define LVLSNOW_RIV_DEN    2

// Carteles ONE WAY (sprite snow_oneway): x de la celda, y de la base.
#define LVLSNOW_SIGN_W     56
#define LVLSNOW_SIGN_H     96
#define LVLSNOW_SIGN_POLE  24   // poste (borde izq.) dentro de la celda
#define LVLSNOW_N_SIGNS    3
static const s16 lvlSnowSigns[LVLSNOW_N_SIGNS][2] = {
    { 1200, 164 },
    { 1888, 164 },
    { 2640, 164 },
};

// Tractor (sprite snow_trator, PAL3): celda y rueda de adelante.
#define LVLSNOW_TRAC_W     256
#define LVLSNOW_TRAC_H     112
#define LVLSNOW_TRAC_WHEEL 100

// Tope caminable (pies, y de mundo) por columna de 8 px: canteros y bancos.
static const u8 lvlSnowWalkTop[LVLSNOW_BG_W] = {
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 133, 140, 144,
    146, 141, 148, 144, 143, 143, 147, 146, 148, 144, 144, 145, 141, 143, 145, 148,
    143, 147, 146, 144, 144, 139, 132, 138, 156, 150, 138, 138, 138, 138, 138, 150,
    156, 156, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 134, 137, 143, 139, 136, 145, 141, 139, 144, 143, 145, 145, 141, 142,
    139, 137, 142, 143, 145, 140, 144, 141, 141, 141, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 134, 137, 143, 139, 136, 145, 141, 139, 144, 143, 145,
    145, 141, 142, 139, 137, 142, 143, 145, 140, 144, 141, 141, 141, 132, 132, 156,
    156, 150, 138, 138, 138, 138, 138, 150, 156, 138, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 134, 137, 143, 139, 136, 145, 141, 139,
    144, 143, 145, 145, 141, 142, 139, 137, 142, 143, 145, 140, 144, 141, 141, 141,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
    132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
};

#endif
