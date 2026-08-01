// =============================================================================
// enemies.res  Sprites de enemigos
// =============================================================================
// Foot Soldier (morado): sheet de 448x1280 = grilla 7x16 de frames de 64x80px
// (8x10 tiles). El arte mira a la DERECHA (enemy.c aplica HFlip cuando dir == -1).
// Animaciones (filas, en orden de Aseprite):
//
//   [0]  idle quieto (1f)
//   [1]  walk (5f)
//   [2]  kick patada con salto (4f)
//   [3]  uppercut (2f)
//   [4]  walk up (4f)
//   [5]  explode muerte (6f)
//   [6]  punch front directo (2f)
//   [7]  break door (4f)
//   [8]  hit 1 (1f)
//   [9]  hit 2 (1f)
//   [10] hit 3 (1f)
//   [11] giro (2f: arranca mirando a la derecha, termina mirando a la izquierda)
//   [12] guard espera (3f: el frame 2 se mantiene mas tiempo; f1/f3 = entrada/salida)
//   [13] stance, otra postura de espera (3f)
//   [14] grab agarre por la espalda (ATENCION: si esta fila queda 100%
//        transparente, rescomp la elimina y todos los indices siguientes
//        se corren UNO: 14 pasa a ser la voltereta y 15 queda fuera de rango.
//        Hoy tiene un punto placeholder de 4px hasta dibujar la pose).
//   [15] voltereta (7f, avanza mas en X que el walk)
//
// IMPORTANTE: el ultimo parametro es el TIEMPO DE FRAME en 1/60s. Si se omite,
// rescomp usa 0 = SIN animacion automatica (el sprite queda clavado en el
// primer frame). Con 8: kick = 4x8 = 32 y punch = 2x8 = 16  deben coincidir
// con ENEMY_KICK_TIME / ENEMY_PUNCH_TIME de enemy.h.
// =============================================================================

SPRITE foot_soldier "sprites/foot_soldier_16colors.png" 8 10 FAST 8

// Foot Soldier Naranja: sheet de 416x936 = grilla 4x9 de frames de 104x104px.
// Misma grilla que las tortugas/regular. Usa PAL3 (reemplaza al flash eliminado).
// Animaciones (filas):
//   [0] Idle (1f) | [1] Walk (4f) | [2] Walk up (4f) | [3] Shuriken throw (3f)
//   [4] Punch front (2f) | [5] Uppercut (3f) | [6] Explode (4f)
//   [7] Hit received (1f) | [8] Jump kick (4f)
// El shuriken se spawnea en el frame 1 de la anim [3] (timer == 16).
SPRITE foot_soldier_orange "sprites/foot_soldier_orange.png" 13 13 FAST 8

// Shuriken: proyectil del foot soldier naranja. 16x16px = 2x2 tiles.
// Misma paleta que el naranja (PAL3). Se crea/destruye en runtime.
SPRITE shuriken_sprite "sprites/shuriken.png" 2 2 FAST 0
