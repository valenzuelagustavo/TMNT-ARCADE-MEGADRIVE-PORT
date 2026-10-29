// =============================================================================
// cutscenes.res  Cinematicas de una sola imagen entre niveles (23/10)
// =============================================================================
// cut_accident  Despues del nivel de las patinetas (SCENE_CUT_VAN): la
//               camioneta de las tortugas que choca. 320x224, 15 colores
//               (indices 1-15), 581 tiles unicos (imagen nueva del 27/10). Sin comprimir (REGLA del
//               22/10: un BEST pide al heap un bloque del tamano de todos sus
//               tiles y puede fallar sin avisar). Ver src/cut_scenes.c.
// =============================================================================
IMAGE cut_accident "images/lvl_6_skate/cuscene_accident.png" NONE ALL
// (27/10) cut_leo: Leo, 64x64 en (80,96) sobre el choque; solo se muestra si
//   alguna tortuga en juego es Leo (cut_scenes.c). Paleta propia (PAL1: el fondo
//   usa los 15 colores de PAL0). Lo recorta tools/gen_cut_turtles.py de
//   LEO_TURTLES.png; las otras tortugas van igual cuando lleguen.
SPRITE cut_leo "images/lvl_6_skate/leo_turtles_spr.png" 8 8 FAST 0

// cut_splinter_bg    Despues de la fabrica (SCENE_CUT_SPLINTER): el fondo amarillo
//                    con el globo "THANK YOU MY TURTLES!" y Splinter SIN la cabeza
//                    que se mueve (el cuerpo y todo lo que no cambia en los 13
//                    cuadros). 320x224, 199 tiles unicos. Lo arma
//                    tools/gen_cut_splinter.py desde Cutscene_Factory/1587_*.png.
// cut_splinter_mouth La parte que habla: 64x40 en (168,104), 13 cuadros (los
//                    1587_1..13), la misma paleta que el fondo (PAL0).
IMAGE  cut_splinter_bg    "images/lvl_7_factory/Cutscene_Factory/splinter_bg.png"    FAST ALL
SPRITE cut_splinter_mouth "images/lvl_7_factory/Cutscene_Factory/splinter_mouth.png" 8 5 FAST 0

// (28/10) cut_splinter_bg, cut_sewer_bg y cut_sewer_fg van comprimidas (FAST = LZ4W) para ganar ~10 KB de
//   ROM: en la cutscene el heap esta casi vacio (el pedido es de 5-9 KB). cut_accident (18 KB) sigue NONE.
// (28/10) Despues de The Sewer (SCENE_CUT_SHREDDER_APRIL): Shredder se lleva a April y
// Rocksteady (con Bebop) provoca a las tortugas. Lo arma tools/gen_cut_sewer.py desde
// lvl_3_sewer/1432_1459_*.png y bebop_rocksteady_1..28.png. Tres paletas: fondo PAL0,
// April PAL1, primer plano + boca + globo PAL2.
//   cut_sewer_bg      fondo (BG_B)
//   cut_sewer_fg      Bebop y Rocksteady fijos, boca cerrada y sin globo (cuadro 28; BG_A)
//   cut_sewer_april   April atada, 80x128 en (120,80)
//   cut_sewer_mouth   boca de Rocksteady 64x64 en (222,160): cuadro 0 cerrada, 1 abierta
//   cut_sewer_bubble  el globo "Come and get'em, shell-brain", 96x32 en (192,96)
IMAGE  cut_sewer_bg     "images/lvl_3_sewer/1432_1459_shredder_bg.png"   FAST ALL
IMAGE  cut_sewer_fg     "images/lvl_3_sewer/bebop_rocksteady_28.png"     FAST ALL
SPRITE cut_sewer_april  "images/lvl_3_sewer/cut_april_spr.png"   10 16 FAST 0
SPRITE cut_sewer_mouth  "images/lvl_3_sewer/cut_rock_mouth.png"   8 8 FAST 0
SPRITE cut_sewer_bubble "images/lvl_3_sewer/cut_rock_bubble.png" 12 4 FAST 0
