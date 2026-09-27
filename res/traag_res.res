// =============================================================================
// traag_res.res  Jefe de la Scene 8: el GENERAL TRAAG (27/09)
// =============================================================================
// Arte del proyecto del companero (Ray Project). Los tres PNG comparten UNA
// paleta y se dibujan con PAL3 (traagSpawn la carga).
//   traag_boss      celdas de 144x144, pies en la fila 130:
//                   0 quieto A · 1 quieto B · 2 camina (loop) · 3 dispara
//                   (f0 sale el misil, f1 retroceso) · 4 culatazo (6 frames,
//                   a mano) · 5 dano/muerte (0 flash, 1 se quiebra,
//                   2 montoncito; a mano)
//   traag_missil    64x32: el misil (2 frames, loop). Mira a la derecha.
//   traag_explosao  32x32: la explosion (7 frames, a mano)
// Se llama traag_res (y no traag) para que su .h no choque con src/traag.h.
// =============================================================================
SPRITE traag_boss     "sprites/traag_boss.png"     18 18 NONE 8
SPRITE traag_missil   "sprites/traag_missil.png"    8  4 NONE 4
SPRITE traag_explosao "sprites/traag_explosao.png"  4  4 NONE 0
