// =============================================================================
// enemies.res — Sprites de enemigos
// =============================================================================
// Foot Soldier: 7x8 tiles por frame, 2 animaciones:
//   anim 0 → idle (2 frames), anim 1 → caminar (3 frames)
//
// IMPORTANTE: el último parámetro es el TIEMPO DE FRAME en 1/60s. Si se omite,
// rescomp usa 0 = SIN animación automática (el sprite queda clavado en el
// primer frame). Las tortugas usan 7 en chars.res; acá 8 (paso más pesado).
// =============================================================================

SPRITE foot_soldier "sprites/foot_soldier 7 8.png" 7 8 FAST 8
