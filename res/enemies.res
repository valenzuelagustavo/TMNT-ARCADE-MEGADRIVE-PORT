// =============================================================================
// enemies.res  Sprites de enemigos
// =============================================================================
// Foot Soldier: sheet de 520x520 = grilla 5x5 de frames de 104x104px
// (13x13 tiles, la MISMA grilla que las tortugas). El arte mira a la DERECHA
// (enemy.c aplica HFlip cuando dir == -1). Animaciones (filas):
//
//   anim 0 -> ENEMY_ANIM_IDLE     (1 frame)
//   anim 1 -> ENEMY_ANIM_WALK     (5 frames)  izq / der / abajo
//   anim 2 -> ENEMY_ANIM_KICK     (4 frames)  patada con salto, avanza en X
//   anim 3 -> ENEMY_ANIM_PUNCH    (2 frames)  uppercut
//   anim 4 -> ENEMY_ANIM_WALK_UP  (4 frames)  caminar hacia arriba
//
// IMPORTANTE: el ultimo parametro es el TIEMPO DE FRAME en 1/60s. Si se omite,
// rescomp usa 0 = SIN animacion automatica (el sprite queda clavado en el
// primer frame). Las tortugas usan 7 en chars.res; aca 8 (paso mas pesado).
// Con 8: kick = 4x8 = 32 frames y punch = 2x8 = 16 frames  deben coincidir
// con ENEMY_KICK_TIME / ENEMY_PUNCH_TIME de enemy.h.
// =============================================================================

SPRITE foot_soldier "sprites/foot_soldier_16colors.png" 13 13 FAST 8
