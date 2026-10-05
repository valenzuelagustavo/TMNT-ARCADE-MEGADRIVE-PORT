// GENERADO POR tools/gen_level2_1_limits.py -- NO EDITAR A MANO
// Sale de res/images/lvl_2_scene/"Stage 2-_LIMITES.png" (el magenta pintado
// = limite de pared) + el arte v2 (el ultimo pixel dibujado de cada
// columna = borde de la vereda).
#ifndef _LEVEL2_1_LIMITS_H_
#define _LEVEL2_1_LIMITS_H_

#include <genesis.h>

#define LVL21_LIM_STEP   8     // px de X que cubre cada entrada
#define LVL21_LIM_COLS   320
#define LVL21_WORLD_W    2560
#define LVL21_WORLD_H    640

// Para cada bloque de LVL21_LIM_STEP px en X: primer y ultimo Y de PIES
// caminable. Un bloque sin calle tiene top > bot (no hay Y que lo cumpla).
extern const u16 lvl21WalkTop[LVL21_LIM_COLS];
extern const u16 lvl21WalkBot[LVL21_LIM_COLS];

// La cornisa sobre los portones: se sube saltando y se baja caminando hacia
// abajo (te dejas caer a la calle). Fuera de este rango de X no existe.
#define LVL21_PLAT_X0    1055
#define LVL21_PLAT_X1    1592
#define LVL21_PLAT_Y0    96
#define LVL21_PLAT_Y1    103

// Extremos de TODA el area util (plataforma incluida). Son los que se le
// pasan a setPlayerLane/setEnemyBounds; el recorte fino lo hace la tabla.
#define LVL21_WALK_Y_MIN 96
#define LVL21_WALK_Y_MAX 634
#define LVL21_WALK_X_MIN 32
#define LVL21_WALK_X_MAX 2559

#endif
