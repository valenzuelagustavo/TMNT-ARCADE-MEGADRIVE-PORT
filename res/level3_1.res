// =============================================================================
// level3_1.res  Scene 3: la cloaca (sewer) -- 26/09, fondo nuevo el 28-29/09
// =============================================================================
// Fuentes: bg_sewer.png y bg_sewer_fg.png (exportadas de Aseprite, 1252x288, las
// dos capas del arcade). tools/gen_level3_1_bg.py las prepara (*_md.png, los
// .bin y la tira de caños de arriba). Ver el encabezado del script.
//
// pal_sewer / bg_sewer_tiles / bg_sewer_map
//             El fondo, filas 0..255 (1248x256 = 156x32 tiles): la camara sube
//             32 px sobre el recorte viejo (camYMin en level3_1.c). 2450 tiles
//             unicos: NO entra en un IMAGE, va en el formato ancho de
//             stage_bg.c y se streamea por columnas. Peor caso en 42
//             columnas: 861. PAL0, con el agua animada (indices 1 y 14).
// bg_sewer_fg Los caños que pasan por DELANTE, mismas 256 filas: entran
//             enteros y se dibujan en BG_A con prioridad alta. PAL0, el
//             indice 0 transparente.
// sewer_fg_top Lo de los caños que puede quedar DEBAJO DEL HUD (filas 0..63;
//             el HUD va en el plano WINDOW): 6 frames de 8x64, uno por
//             columna distinta; stage_level carga sus tiles una sola vez y
//             los 9 sprites (src/level3_1_fgtop.h) los comparten.
// =============================================================================
PALETTE pal_sewer      "images/lvl_3_sewer/bg_sewer_md.png"
BIN     bg_sewer_tiles "images/lvl_3_sewer/bg_sewer_tiles.bin" 2 2 0 NONE
BIN     bg_sewer_map   "images/lvl_3_sewer/bg_sewer_map.bin"   2 2 0 NONE
IMAGE   bg_sewer_fg    "images/lvl_3_sewer/bg_sewer_fg_md.png" NONE ALL
SPRITE  sewer_fg_top   "images/lvl_3_sewer/sewer_fg_top.png"   1 8 NONE 0

// (29/09) Misil que sale del AGUA cuando una tortuga camina por el canal, y
// su explosion. Arte de Traag (traag_missil / traag_explosao) remapeado a la
// paleta de los foot soldiers (PAL2) por tools/gen_sewer_missile.py: en la
// cloaca PAL3 es de Baxter. Misil: 2 frames de 64x32 (mira a la DERECHA).
// Explosion: 7 frames de 32x32 (impacta de derecha a izquierda), a mano.
// (16/10) El MISIL es ahora el de la hoja "Robot Enemies" (tools/
// gen_robot_enemies.py), en la paleta del foot blanco: PAL3 (con Baxter no
// salen misiles). 2 frames de 64x32 con la llama, mira a la DERECHA. La
// explosion sigue siendo la de Traag en PAL2. gen_sewer_missile.py ya no se usa.
SPRITE  sewer_missil   "sprites/sewer_missil.png"   8 4 NONE 4
// (16/10) Salpicon del misil al salir del agua: 5 frames de 64x64, PAL3, a mano.
SPRITE  sewer_mis_splash "sprites/sewer_mis_splash.png" 8 8 NONE 0
SPRITE  sewer_explosao "sprites/sewer_explosao.png" 4 4 NONE 0

// (13/10) Mousers que salen de los huecos de la pared (level3_1.c): la rata
// y la explosion de Baxter pasadas a la paleta del foot blanco (PAL3 durante
// el nivel) por tools/gen_sewer_mouser.py.
// (16/10) sewer_mouser.png es ahora el Mouser AZUL de la hoja "Robot Enemies"
// (tools/gen_robot_enemies.py), ya en la paleta del blanco. Fila 8 nueva: sale
// del hueco caminando (RAT_ANIM_EMERGE). gen_sewer_mouser.py solo hace la
// explosion.
SPRITE  sewer_mouser      "sprites/sewer_mouser.png"      5 5 FAST 5
SPRITE  sewer_mouser_boom "sprites/sewer_mouser_boom.png" 4 4 NONE 0

// (13/10) La pared sana sobre los huecos de los Mousers, y como se raja
// (tools/gen_sewer_holes.py): 3 frames cada uno (sana, rajadura chica,
// grande); despues se suelta y queda el hueco del fondo. PAL0.
SPRITE  sewer_hole_0 "images/lvl_3_sewer/sewer_hole_0.png" 6 5 NONE 0
SPRITE  sewer_hole_1 "images/lvl_3_sewer/sewer_hole_1.png" 6 4 NONE 0
SPRITE  sewer_hole_2 "images/lvl_3_sewer/sewer_hole_2.png" 6 4 NONE 0
SPRITE  sewer_hole_3 "images/lvl_3_sewer/sewer_hole_3.png" 6 4 NONE 0

// (18/10) Ratas DECORATIVAS de la primera parte (level3_1.c, swRat): corren
// por la base de la pared, sin interaccion. tools/gen_sewer_rat.py pasa
// rat.png (RGBA) a la paleta del fondo (PAL0). Celdas de 32x16: [0] quieta,
// [1] corriendo (3 frames). time 0: los frames van a mano.
SPRITE  sewer_rat "images/lvl_3_sewer/sewer_rat.png" 4 2 NONE 0
