// =============================================================================
// level3_1.res  Scene 3: la cloaca (sewer) -- 26/09
// =============================================================================
// Arte del proyecto del companero (Ray Project, res/images/lvl_4_sewer/).
//
// bg_sewer    1248x224, 16 colores, 2037 tiles unicos: NO entra en VRAM de una.
//             Se queda en ROM sin comprimir (NONE) y stage_bg.c sube a un
//             cache solo los tiles de las columnas visibles. Su paleta va en
//             PAL0 (el indice 0 es un color de verdad: ver level3_1.c).
// bg_sewer_fg Los caños que pasan por DELANTE (74 tiles): entran enteros y se
//             dibujan en BG_A con prioridad alta. Usa los mismos colores e
//             indices que el fondo, asi que va con PAL0.
// =============================================================================
IMAGE bg_sewer    "images/lvl_3_sewer/bg_sewer.png"    NONE ALL
IMAGE bg_sewer_fg "images/lvl_3_sewer/bg_sewer_fg.png" NONE ALL
