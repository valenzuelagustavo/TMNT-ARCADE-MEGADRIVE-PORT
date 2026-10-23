#ifndef _DIFFICULTY_H_
#define _DIFFICULTY_H_

#include <genesis.h>

// ---------------------------------------------------------------------------
// DIFICULTAD POR CANTIDAD DE JUGADORES (20/10)
// ---------------------------------------------------------------------------
// Con mas tortugas el juego se pone mas dificil, sin pedirle mas a la VRAM
// (el tope de enemigos EN PANTALLA no cambia):
//   1. oleadas con mas foot soldiers (salen de a poco, con el mismo tope);
//   2. mas foot soldiers atacando a la vez (DIFF_ATTACKERS_PER_PLAYER por
//      jugador, en vez de 2 en total);
//   3. menos espera entre un ataque y el siguiente;
//   4. jefes con mas vida.
// El NIVEL sale de los jugadores y del GAME LEVEL de las opciones:
//   nivel = (jugadores - 1) + (ARCADE ? 1 : 0), con tope DIFF_TIER_MAX
// asi que ARCADE con 1 jugador arranca con la dificultad de NORMAL con 2.
//
// difficultySetPlayers() se llama desde resetEnemyAI(), o sea al empezar cada
// nivel y cuando P2 se suma en plena partida: el cambio es inmediato para los
// atacantes, las esperas y la vida del jefe que este peleando (se le suma la
// proporcion de lo que le queda). Las oleadas toman el extra cuando empiezan.
// ---------------------------------------------------------------------------
#define DIFF_TIER_MAX              3
#define DIFF_ATTACKERS_PER_PLAYER  2   // el ENEMY_MAX_ATTACKERS de antes, por tortuga
#define DIFF_ATTACKERS_ARCADE      1   // ARCADE: uno mas
// Por nivel 0..DIFF_TIER_MAX (ver difficulty.c):
//   foot soldiers extra por oleada (%):   0  50  75 100
//   espera entre ataques (%):           100  80  70  60
//   vida de los jefes (%):              100 150 175 200

extern u8 diffTier;
extern u8 diffMaxAttackers;

void difficultySetPlayers(u8 numPlayers);

// Foot soldiers EXTRA para una oleada de 'n' (los decimales se arrastran de
// una oleada a la otra: en el nivel sale el porcentaje justo).
u8   diffWaveExtra(u8 n);
// Espera entre ataques escalada.
u16  diffCooldown(u16 base);
// Vida de un jefe escalada.
s16  diffBossHp(s16 base);
// El jefe se anota al arrancar: si P2 se suma en medio de la pelea, a su vida
// actual se le suma la proporcion que corresponde.
void diffBossTrack(s16* hp);

#endif
