#ifndef _SHREDDER_BOSS_H_
#define _SHREDDER_BOSS_H_

#include <genesis.h>
#include "level2.h"      // shredder_lvl1 (el sprite de la cutscene del 1-2)
#include "player.h"

// ===========================================================================
// SHREDDER — jefe final (Scene 9, la sala del portal) (27/09)
// ===========================================================================
// Todavia no hay spritesheet de pelea de Shredder: se usa el de la cutscene
// (shredder_lvl1, celdas de 72x80, pies en la fila 79, mira a la derecha):
//   anim 0 quieto (1 frame) · anim 1 camina con la espada (6 frames)
// Con eso solo, la pelea se arma asi:
//   APARECE   se materializa parpadeando delante del portal.
//   CAMINA    se acerca y se alinea en lane.
//   ESPADAZO  de cerca: levanta la espada (frame 2 de la caminata, aviso) y
//             la tira al frente (frame 5, la espada extendida): 2 barras.
//   EMBESTIDA de lejos: se planta un instante (quieto, titilando) y cruza la
//             sala corriendo; el que queda en el camino cae (2 barras).
//   TELETRANSPORTE  anti-trabado: al tercer golpe seguido desaparece y
//             reaparece del otro lado del objetivo (el Shredder del arcade
//             tambien se teletransporta).
//   MUERTE    parpadea cada vez mas rapido y se desvanece.
// La paleta va en PAL2 (en esta sala no hay foot soldiers); PAL3 es del
// portal, que cicla colores.
// ===========================================================================

#define SHRED_FRAME_W       72
#define SHRED_FOOT_OFFSET   80
#define SHRED_BODY_HALF_W   16
#define SHRED_BODY_H        72
#define SHRED_ANIM_IDLE      0
#define SHRED_ANIM_WALK      1

#define SHRED_HP            80
#define SHRED_SPECIAL_DMG    2
#define SHRED_INVULN        12     // un golpe por swing
#define SHRED_COUNTER_HITS   3     // golpes seguidos -> se teletransporta
#define SHRED_COMBO_RESET   50

#define SHRED_WALK_SPEED     1
#define SHRED_ALIGN_Y        6
#define SHRED_SLASH_RANGE   40     // |dx| del espadazo
#define SHRED_SLASH_TOL_Y   12
#define SHRED_SLASH_CD      60
#define SHRED_SLASH_DMG      2
#define SHRED_SLASH_WIND    14     // espada en alto (aviso)
#define SHRED_SLASH_HOLD    14     // espada al frente
#define SHRED_POSE_RAISED    2     // frames de la caminata usados como poses
#define SHRED_POSE_THRUST    5
#define SHRED_CHARGE_DIST  130     // mas lejos que esto: embiste
#define SHRED_CHARGE_WIND   24     // quieto antes de salir
#define SHRED_CHARGE_SPEED   5
#define SHRED_CHARGE_HIT_W  26
#define SHRED_CHARGE_TOL_Y  14
#define SHRED_CHARGE_DMG     2
#define SHRED_CHARGE_CD     90
#define SHRED_HURT_TICKS    10
#define SHRED_TP_OUT        20     // parpadeo al irse
#define SHRED_TP_GONE       30     // invisible
#define SHRED_TP_IN         20     // parpadeo al volver
#define SHRED_ENTER_TICKS   50
#define SHRED_DEATH_TICKS  100

typedef enum {
    SHRED_INACTIVE, SHRED_ENTER, SHRED_IDLE, SHRED_WALK, SHRED_SLASH, SHRED_WIND, SHRED_CHARGE,
    SHRED_HURT, SHRED_TP_OUT_ST, SHRED_TP_GONE_ST, SHRED_TP_IN_ST, SHRED_DEATH, SHRED_GONE
} ShredState;

typedef struct {
    Sprite*    sprite;
    ShredState state;
    s16        x, y;          // centro / pies (mundo)
    s16        camX, camY;
    s8         dir;
    s16        hp;
    u16        timer;
    u16        slashCd, chargeCd;
    u8         invuln, flash;
    u8         comboHits, calm;
    u8         hitMask;       // jugadores ya golpeados en esta embestida
    s16        arenaL, arenaR, laneT, laneB;
    u8         anim;
} ShredderBoss;

void shredderInit(ShredderBoss* s);
// Aparece en (x, y) de mundo. Carga PAL2.
void shredderSpawn(ShredderBoss* s, s16 x, s16 y, s16 arenaL, s16 arenaR, s16 laneT, s16 laneB);
void shredderUpdate(ShredderBoss* s, Player** pls, u8 nPl, s16 camX, s16 camY);
// Golpes de las tortugas. TRUE si lo mato (killer = indice).
bool shredderPlayerHits(ShredderBoss* s, Player** pls, u8 nPl, s8* killer);
bool shredderIsDying(const ShredderBoss* s);
bool shredderIsGone(const ShredderBoss* s);
void shredderRelease(ShredderBoss* s);

#endif
