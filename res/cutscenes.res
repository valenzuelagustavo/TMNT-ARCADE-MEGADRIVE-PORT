// =============================================================================
// cutscenes.res  Cinematicas de una sola imagen entre niveles (23/10)
// =============================================================================
// cut_accident  Despues del nivel de las patinetas (SCENE_CUT_VAN): la
//               camioneta de las tortugas que choca. 320x224, 15 colores
//               (indices 1-15), 570 tiles unicos. Sin comprimir (REGLA del
//               22/10: un BEST pide al heap un bloque del tamano de todos sus
//               tiles y puede fallar sin avisar). Ver src/cut_scenes.c.
// =============================================================================
IMAGE cut_accident "images/lvl_6_skate/cuscene_accident.png" NONE ALL

// cut_splinter_bg    Despues de la fabrica (SCENE_CUT_SPLINTER): el fondo amarillo
//                    con el globo "THANK YOU MY TURTLES!" y Splinter SIN la cabeza
//                    que se mueve (el cuerpo y todo lo que no cambia en los 13
//                    cuadros). 320x224, 199 tiles unicos. Lo arma
//                    tools/gen_cut_splinter.py desde Cutscene_Factory/1587_*.png.
// cut_splinter_mouth La parte que habla: 64x40 en (168,104), 13 cuadros (los
//                    1587_1..13), la misma paleta que el fondo (PAL0).
IMAGE  cut_splinter_bg    "images/lvl_7_factory/Cutscene_Factory/splinter_bg.png"    NONE ALL
SPRITE cut_splinter_mouth "images/lvl_7_factory/Cutscene_Factory/splinter_mouth.png" 8 5 FAST 0
