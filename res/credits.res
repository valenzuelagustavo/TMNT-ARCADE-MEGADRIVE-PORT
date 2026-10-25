// =============================================================================
// credits.res (18/10) -- pantalla de CREDITOS del final (src/ending.c)
// =============================================================================
// Salen de tools/gen_credits.py. Misma paleta (la de la seleccion de
// personaje): todo va en PAL1. El fondo de la ciudad es roof_bg_b
// (roof_april.res).
//   credits_turtle_0..3  160x112: los cuadros de Leo, Mike, Don y Raph que
//                        pasan de derecha a izquierda, uno a la vez
//                        (recortados de credits_turtles.png).
//   credits_konami       72x24: el logo de KONAMI del "PRODUCED BY".
// NONE: el cuadro se redibuja cada vez que sale uno por la izquierda.
// =============================================================================
IMAGE credits_turtle_0 "images/credits/credits_turtle_0.png" NONE
IMAGE credits_turtle_1 "images/credits/credits_turtle_1.png" NONE
IMAGE credits_turtle_2 "images/credits/credits_turtle_2.png" NONE
IMAGE credits_turtle_3 "images/credits/credits_turtle_3.png" NONE
IMAGE credits_konami   "images/credits/credits_konami.png" NONE
// (25/10) Fondo del EPILOGO (src/ending.c, showEpilogue): 320x224, 12 colores,
// va en PAL1 en BG_B con el texto en BG_A. Provisorio (no es el definitivo).
// NONE: se dibuja con VDP_drawImageEx + DMA (FAR_SAFE) y sin descomprimir en RAM.
IMAGE epilogue_bg "images/epilogue/bg_epilogue.png" NONE
