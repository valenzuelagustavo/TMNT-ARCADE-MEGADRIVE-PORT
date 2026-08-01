// =============================================================================
// chars.res — Sprites de los personajes jugables
// =============================================================================
// Las 4 tortugas ninja. Cada spritesheet contiene TODAS las animaciones del
// personaje en el orden definido por PlayerAnim en player.h (cada FILA del PNG
// es una animación; el frame es de 104x104px = 13x13 tiles):
//
//   Leo/Mike (sheets NUEVAS, 21 filas):           Raph/Don (sheets VIEJAS, 19-20
//   filas — quedaron "corridas" a propósito hasta que se rediseñen):
//
//   Fila 0  -> ANIM_IDLE
//   Fila 1  -> ANIM_IDLE2 (pose de espera, tras ~5s quieto)
//   Fila 2  -> ANIM_KICK
//   Fila 3  -> ANIM_ATTACK_1 (Combo 3 golpes)
//   Fila 4  -> ANIM_ATTACK_2
//   Fila 5  -> ANIM_ATTACK_3 (finalizador)
//   Fila 6  -> ANIM_JUMP
//   Fila 7  -> ANIM_JUMP_KICK
//   Fila 8  -> ANIM_WALK_FRONT
//   Fila 9  -> ANIM_WALK_BACK
//   Fila 10 -> ANIM_SPECIAL
//   Fila 11 -> ANIM_HIT_1
//   Fila 12 -> ANIM_HIT_2
//   Fila 13 -> ANIM_HIT_3
//   Fila 14 -> ANIM_GET_UP_1
//   Fila 15 -> ANIM_HIT_BEHIND_1
//   Fila 16 -> ANIM_HIT_BEHIND_2
//   Fila 17 -> ANIM_GET_UP_2
//   Fila 18 -> ANIM_HELD (4 frames: 0-2 agarrado, 3 = golpe en pleno agarre)
//   Fila 19 -> ANIM_WHIP_SHOCK (látigo del robot)
//   Fila 20 -> ANIM_KO (4 frames)
// =============================================================================

SPRITE leo_player  "/sprites/leo_anim_13x13.png"  13 13 FAST 5
SPRITE mike_player "/sprites/mike_anim_13x13.png" 13 13 FAST 5
SPRITE raph_player "/sprites/raph_anim_13x13.png" 13 13 FAST 5
SPRITE don_player  "/sprites/don_anim_13x13.png"  13 13 FAST 5
