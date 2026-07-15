// =============================================================================
// chars.res — Sprites de los personajes jugables
// =============================================================================
// Las 4 tortugas ninja. Cada spritesheet de 13x13 tiles contiene TODAS las
// animaciones del personaje en el orden definido por PlayerAnim en player.h:
//
//   Frame 0  -> ANIM_IDLE
//   Frame 1  -> ANIM_KICK
//   Frame 2  -> ANIM_ATTACK_1  (Combo 3 golpes)
//   Frame 3  -> ANIM_ATTACK_2
//   Frame 4  -> ANIM_ATTACK_3  (finalizador)
//   Frame 5  -> ANIM_JUMP
//   Frame 6  -> ANIM_JUMP_KICK
//   Frame 7  -> ANIM_WALK_FRONT
//   Frame 8  -> ANIM_WALK_BACK
//   Frame 9  -> ANIM_SPECIAL
//   Frame 10 -> ANIM_HIT_1
//   Frame 11 -> ANIM_HIT_2
//   Frame 12 -> ANIM_HIT_3
//   Frame 13 -> ANIM_GET_UP_1
//   Frame 14 -> ANIM_HIT_BEHIND_1
//   Frame 15 -> ANIM_HIT_BEHIND_2
//   Frame 16 -> ANIM_GET_UP_2
//   Frame 17 -> ANIM_HELD
//
// NOTA: Las cuatro tortugas tienen spritesheets completas.
// =============================================================================

SPRITE leo_player  "/sprites/leo_anim_13x13.png"  13 13 FAST 7
SPRITE mike_player "/sprites/mike_anim_13x13.png" 13 13 FAST 7
SPRITE raph_player "/sprites/raph_anim_13x13.png" 13 13 FAST 7
SPRITE don_player  "/sprites/don_anim_13x13.png"  13 13 FAST 7
