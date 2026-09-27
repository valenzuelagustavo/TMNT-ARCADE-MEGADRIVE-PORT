#ifndef _TRAAG_H_
#define _TRAAG_H_

#include <genesis.h>
#include "traag_res.h"   // traag_boss, traag_missil, traag_explosao (rescomp)
#include "player.h"

// ===========================================================================
// GENERAL TRAAG — jefe de la Scene 8 (el Technodrome) (27/09)
// ===========================================================================
// Portado del proyecto del companero (Ray Project, src/traag.c) con el mismo
// esquema que Granitor (granitor.c): N jugadores, un golpe por swing,
// culatazo que derriba, anti-trabado. Diferencias con Granitor: dispara
// MISILES rectos (a veces de a dos) que revientan en una explosion que quema
// un ratito, el culatazo tiene 6 frames reales y vive en un nivel con camara
// vertical (camY).
//   QUIETO / CAMINA / DISPARA (alineado) / CULATAZO / MUERTE (parpadea, se
//   quiebra, queda un montoncito).
// ===========================================================================

#define TRAAG_ANIM_IDLE1    0
#define TRAAG_ANIM_IDLE2    1
#define TRAAG_ANIM_WALK     2
#define TRAAG_ANIM_FIRE     3
#define TRAAG_ANIM_PUNCH    4
#define TRAAG_ANIM_DAMAGE   5

#define TRAAG_DMG_FLASH     0
#define TRAAG_DMG_BREAK     1
#define TRAAG_DMG_PILE      2

#define TRAAG_FRAME_W       144
#define TRAAG_FRAME_H       144
#define TRAAG_FOOT_OFFSET   130
#define TRAAG_BODY_HALF_W    24     // hurtbox (como la tortuga, un poco mas ancho)
#define TRAAG_BODY_H         80

#define TRAAG_HP             96     // el doble que Rocksteady
#define TRAAG_SPECIAL_DMG     2
#define TRAAG_INVULN         12     // frames sin recibir otro golpe (1 por swing)

#define TRAAG_WALK_Q          3     // velocidad en 1/4 px
#define TRAAG_WALK_DY        14
#define TRAAG_MIN_DIST       46
#define TRAAG_PUNCH_RANGE    92
#define TRAAG_FIRE_RANGE     96
#define TRAAG_IDLE_DECIDE     8
#define TRAAG_FIRE_CD        70     // misil frecuente
#define TRAAG_PUNCH_CD       28

// disparo (2 frames: el misil nace en el f0, f1 retroceso)
#define TRAAG_FIRE_F0_TICKS 14
#define TRAAG_FIRE_F1_TICKS 26
#define TRAAG_GUN_DX        54     // boca del arma delante del centro
#define TRAAG_GUN_DY        76     // altura del cano sobre los pies
#define MAX_TRAAG_MISSILES   2
#define MAX_TRAAG_EXPLOSIONS 3

#define TRAAG_PUNCH_TICKS     8
#define TRAAG_PUNCH_IMPACT_T 12
#define TRAAG_PUNCH_RECOV_T  18
#define TRAAG_PUNCH_ADV       1
#define TRAAG_PUNCH_HIT_DX   64
#define TRAAG_PUNCH_HIT_W    40
#define TRAAG_PUNCH_TOL_Y    24     // |dy| de pies para que conecte
#define TRAAG_PUNCH_DMG       2

#define TRAAG_HURT_TICKS     12
// (port) Anti-trabado, como Bebop: las tortugas lo encadenaban a golpes y
// nunca podia responder. Aguanta TRAAG_COUNTER_HITS golpes seguidos con
// flinch; el siguiente lo absorbe y contraataca en el acto con el culatazo.
// La racha se corta sola tras TRAAG_COMBO_RESET frames sin recibir golpes.
#define TRAAG_COUNTER_HITS    3
#define TRAAG_COMBO_RESET    50
#define TRAAG_DEATH_BLINK     4
#define TRAAG_DEATH_BREAK_T  30
#define TRAAG_DEATH_PILE_T   70

#define TRAAG_MISSILE_SPEED  4
#define TRAAG_MISSILE_DMG    1
#define TRAAG_MISSILE_TOL_Y 16     // |dy| de lane para que el misil pegue
#define TRAAG_EXPL_TICKS     4     // por frame de la explosion
#define TRAAG_EXPL_HIT_T     8     // la explosion quema solo al principio
#define TRAAG_EXPL_DMG       1

typedef enum {
    TRAAG_INACTIVE, TRAAG_ENTER, TRAAG_IDLE, TRAAG_WALK,
    TRAAG_FIRE, TRAAG_PUNCH, TRAAG_HURT, TRAAG_DEATH, TRAAG_GONE
} TraagState;

// Tope caminable por X de mundo (para no meterse en la pared).
typedef s16 (*TraagTopAtFn)(s16 worldX);

typedef struct {
    Sprite*       sprite;
    TraagState state;
    s16           xq, yq;       // centro / pies, en 1/4 px (mundo)
    s16           camX, camY;
    s8            dir;
    s16           hp;
    u8            anim;
    u16           timer;
    u16           attackCd;
    u8            flash;
    u8            invuln;
    u8            idleToggle;
    u8            pFrame, pLanded;
    u8            fFired, fFrame, dblShot;
    u8            comboHits;    // golpes recibidos seguidos (anti-trabado)
    u8            calm;         // frames sin recibir golpes
    u8            dPhase, blinks;
    s16           arenaLeft, arenaRight;
    s16           laneTop, laneBot;
    TraagTopAtFn topAt;
} Traag;

void traagInit(Traag* g);
// Arena en X de mundo (la camara esta clavada) y franja de pies. Entra
// caminando desde la derecha. Carga PAL3.
void traagSpawn(Traag* g, s16 arenaLeft, s16 arenaRight, s16 laneTop, s16 laneBot,
                   TraagTopAtFn topAt);
void traagUpdate(Traag* g, Player** pls, u8 nPl, s16 camX, s16 camY);
bool traagCanBeHit(const Traag* g);
bool traagIsDying(const Traag* g);
bool traagIsGone(const Traag* g);
// Golpe de las tortugas. Devuelve TRUE si lo mato (killer = indice).
bool traagPlayerHits(Traag* g, Player** pls, u8 nPl, s8* killer);
void traagRelease(Traag* g);

// Misiles y explosiones (estado de modulo; traagInit los limpia).
void traagMissileUpdate(Player** pls, u8 nPl, s16 camX, s16 camY);
void traagMissileReleaseAll(void);

#endif
