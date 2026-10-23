// =============================================================================
// level9_1.res  Scene 9: la sala del portal (pelea final) -- 27/09
// =============================================================================
// Arte del proyecto del companero (Ray Project, lvl_8_destroyer/bga.png y
// bgb.png). Los dos son de 512x256; la camara es fija y se ve la franja
// x 96..416, y 32..256.
//   final_room    la sala (300 tiles), BG_A, PAL0. Indice 0 = huecos del portal.
//   final_portal  el portal a la Dimension X (37 tiles), BG_B, PAL3: cicla
//                 colores (ver portalCycle en level9_1.c).
// =============================================================================
// (23/10, rama mapper) NEAR: todo lo de este archivo va en los primeros
// 3 MB de la ROM (sin bank switch). Lo lee el codigo directo de la ROM
// (fondos streameados, tiles por cuadro, mapas) o lo toca el Z80 (audio).
// Ver tools/mapper/README.md.
NEAR

IMAGE final_room   "images/lvl_9_final/final_room.png"   NONE ALL
IMAGE final_portal "images/lvl_9_final/final_portal.png" NONE ALL
