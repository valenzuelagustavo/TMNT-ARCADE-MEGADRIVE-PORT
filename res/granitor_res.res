// =============================================================================
// granitor_res.res  Jefe de la Scene 7: el TENIENTE GRANITOR (26/09)
// =============================================================================
// Arte del proyecto del companero (Ray Project). Los dos PNG comparten UNA
// paleta y se dibujan con PAL3 (granitorSpawn la carga).
//   granitor_boss   celdas de 144x144, pies en la fila 130:
//                   0 quieto A · 1 quieto B · 2 camina (loop) · 3 dispara
//                   · 4 culatazo (0-3, a mano) · 5 dano/muerte (0 flash,
//                   1 se quiebra, 2 montoncito; a mano)
//   granitor_flame  32x32: 0 la llamarada crece (7 frames, a mano)
//                   · 1 la llamarada grande quemando (loop)
// Se llama granitor_res (y no granitor) para que su .h no choque con
// src/granitor.h.
// =============================================================================
SPRITE granitor_boss  "sprites/granitor_boss.png"  18 18 NONE 8
SPRITE granitor_flame "sprites/granitor_flame.png"  4  4 NONE 6
