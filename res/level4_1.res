// =============================================================================
// level4_1.res  Scene 4: el estacionamiento (garage) -- 26/09
// =============================================================================
// Arte del proyecto del companero (Ray Project, res/images/lvl_5_garage/).
//
// bg_garage  1288x224, 16 colores, 1619 tiles unicos: NO entra en VRAM de una.
//            Se queda en ROM sin comprimir (NONE) y stage_bg.c sube a un cache
//            solo los tiles de las columnas visibles (peor caso en 42
//            columnas: 608). Su paleta va en PAL0; el indice 0 es un color de
//            verdad (el verde del camion), por eso el color de fondo del VDP
//            se fija a PAL0[0].
// No tiene primer plano. La colision del companero (col_l5.bin) es una sola
// franja 128..224 en todo el ancho: no hace falta tabla (ver level4_1.c).
// =============================================================================
IMAGE bg_garage "images/lvl_4_garage/bg_garage.png" NONE ALL
