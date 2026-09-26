// =============================================================================
// level7_1.res  Scene 7: la fabrica (factory) -- 26/09
// =============================================================================
// Arte del proyecto del companero (Ray Project, res/images/factory/).
//
// bg_factory     2024x224, 16 colores, 1600 tiles unicos: se streamea
//                (stage_bg.c) en BG_B con PAL0. Peor caso en 42 columnas: 358.
//                El indice 0 es un color de verdad (negro): el color de fondo
//                del VDP se fija a PAL0[0].
// bg_factory_fg  Las dos columnas de reticulado que pasan por DELANTE (21
//                tiles): enteras en VRAM, en BG_A con prioridad alta. Misma
//                paleta que el fondo (PAL0).
// La colision (col_factory.bin) la lee tools/gen_level7_1.py y no va a la ROM.
// =============================================================================
IMAGE bg_factory    "images/lvl_7_factory/bg_factory.png"    NONE ALL
IMAGE bg_factory_fg "images/lvl_7_factory/bg_factory_fg.png" NONE ALL
