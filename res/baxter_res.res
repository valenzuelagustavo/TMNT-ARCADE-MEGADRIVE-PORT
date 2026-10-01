// =============================================================================
// baxter_res.res  Jefe de la Scene 3: BAXTER STOCKMAN (26/09)
// =============================================================================
// Arte del proyecto del companero (Ray Project). Los cuatro PNG comparten UNA
// paleta y se dibujan con PAL3 (baxterSpawn la carga).
//   baxter_boss 48x72: fila 0 volando · 1 portezuela (f0 abriendo, f1 abierta)
//               · 2 dano. time 6: el vuelo se mueve solo; portezuela y dano
//               los pone el codigo a mano.
//   baxter_rat  40x40 (Mouser), 7 frames por fila. (01/10, segun Gustavo)
//               0-1 caminando de frente (una caminata en dos filas; para
//               cuando aparezcan en el nivel) · 2-3 caminando de costado
//               (idem) · 4 hacia arriba · 5 salto de ataque · 6 golpeado
//               · 7 tirado antes de explotar
//   baxter_boom 64x64: explosion de la nave. (30/09) Sale de
//               baxter_boom_gen.png (tools/gen_baxter_boom.py, a partir de la
//               tira baxter_boom.png): 8 frames de 128x128 partidos en 4
//               cuartos; fila = cuarto (anim 0..3), columna = frame. Antes se
//               declaraba 48x72 sobre la tira, que no es una grilla, y la
//               explosion salia en tajadas. Frames a mano (baxter.c).
//   rat_boom    32x32: explosion de la rata (7 frames, a mano)
// Se llama baxter_res (y no baxter) para que su .h no choque con src/baxter.h.
// =============================================================================
SPRITE baxter_boss "sprites/baxter_boss.png" 6 9 NONE 6
SPRITE baxter_rat  "sprites/baxter_rat.png"  5 5 FAST 5
SPRITE baxter_boom "sprites/baxter_boom_gen.png" 8 8 NONE 0
SPRITE rat_boom    "sprites/rat_boom.png"    4 4 NONE 0
