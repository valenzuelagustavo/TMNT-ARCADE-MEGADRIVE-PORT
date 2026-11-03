// =============================================================================
// gameover_res.res (02/11) -- CONTINUE grande y cartel de GAME OVER
// =============================================================================
// Los genera tools/gen_gameover.py (fuente del cartel: sprites/gameover.png).
// (02/11) NEAR: todo lo de este archivo va en los primeros 3 MB de la ROM (sin
// bank switch), igual que el resto de los sprites de los niveles. Ver
// tools/mapper/README.md.
NEAR

// --- Cartel GAME OVER: UN cuadro de 112x32 (14x4 tiles) ---
// Las 5 variantes del arte original tienen la misma forma y cambian solo los
// colores, asi que el efecto es una rotacion de PALETA (src/gameover_pal.h,
// src/gameover.c) y el sprite queda 5 veces mas chico. NO se carga su paleta:
// el codigo copia las 15 de cada cuadro en PAL3 (indice 0 = transparente).
SPRITE gameover_sign "images/gameover/gameover_sign.png" 14 4 FAST 0

// --- CONTINUE grande (un solo jugador): rotulo + digitos 0-9 ---
// Van en PAL1 (la paleta de las tortugas, que ya tiene el rojo -indice 14- y el
// blanco): los PNG estan indexados sobre ella y NO se carga su paleta, asi no
// se pisa ninguna linea del nivel. continue_digits: 10 cuadros de 40x56 (5x7
// tiles) en una fila; el digito N es el cuadro N (SPR_setFrame).
SPRITE continue_digits "images/gameover/continue_digits.png" 5 7 FAST 0
SPRITE continue_label  "images/gameover/continue_label.png"  8 1 FAST 0
