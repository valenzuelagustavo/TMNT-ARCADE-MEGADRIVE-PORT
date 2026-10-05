#ifndef _TRAAG_H_
#define _TRAAG_H_

#include <genesis.h>
#include "traag_res.h"   // traag_boss, traag_missil, traag_explosao (rescomp)
#include "player.h"
#include "boss_flash.h"

// ===========================================================================
// GENERAL TRAAG — jefe de la Scene 8 (el Technodrome) (27/09)
// ===========================================================================
// Base portada del proyecto del companero (Ray Project, src/traag.c).
// Mismo esqueleto que Granitor (granitor.c): N jugadores, un golpe por swing,
// culatazo que derriba. Vive en un nivel con camara vertical (camY).
// Pelea (05/10):
//   QUIETO (40)   alineado con la tortuga y cerca -> culatazo; si no, camina.
//   CAMINA        la persigue (lane exacta, hasta TRAAG_MIN_DIST). Alineado
//                 y cerca -> culatazo; alineado y lejos -> 2/3 BOMBA.
//   CULATAZO (23) pega en el frame del impacto y derriba.
//   BOMBA         la tira con el brazo en arco hacia adelante; revienta al
//                 tocar el piso y voltea al que este cerca. Despues RECARGA.
//   Solo recibe dano quieto o caminando. Sin caida ni contraataque.
//   MUERTE (parpadea, se quiebra, queda un montoncito).
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

#define TRAAG_HP             40
#define TRAAG_SPECIAL_DMG     5
#define TRAAG_JUMPKICK_DMG    2

// Movimiento (Q8: 256 = 1 px por frame)
#define TRAAG_WALK_Q        149     // 0,58 px/f
#define TRAAG_MIN_DIST       25     // persiguiendo se acerca hasta aca
#define TRAAG_WALK_MAX      240     // tope de una caminata sin decidir
#define TRAAG_IDLE_T         40

// Decisiones (centro a centro)
#define TRAAG_ALIGN_DY        2     // "alineado": lane a +-2 px
#define TRAAG_NEAR_DX        57     // culatazo a esta distancia o menos
#define TRAAG_FAR_DX         87     // bomba mas lejos que esto

// Culatazo
#define TRAAG_PUNCH_T        23
#define TRAAG_PUNCH_HIT_T     4     // frames que pega desde el impacto
#define TRAAG_PUNCH_HIT_DX   64
#define TRAAG_PUNCH_HIT_W    40
#define TRAAG_HIT_DY_UP       5     // lane de la tortuga respecto de la suya
#define TRAAG_HIT_DY_DOWN     7
#define TRAAG_PUNCH_DMG       1

// Bomba
#define TRAAG_GUN_DX         54     // el brazo delante del centro
#define TRAAG_GUN_DY         76     // altura del brazo sobre los pies
#define TRAAG_THROW_T        20     // tira (frame 0) ...
#define TRAAG_RELOAD_T       26     // ... y recarga (frame 1)
#define TRAAG_BOMB_XQ       352     // 1,375 px/f hacia adelante
#define TRAAG_BOMB_VQ       373     // 1,46 px/f para arriba al salir
#define TRAAG_BOMB_GRAV_Q    27     // 0,104 px/f2
#define MAX_TRAAG_MISSILES    2
#define MAX_TRAAG_EXPLOSIONS  2
#define TRAAG_EXPL_TICKS      4     // por frame de la explosion
#define TRAAG_EXPL_HIT_T      8     // alcanza solo al principio
#define TRAAG_EXPL_HALF_W    24
#define TRAAG_EXPL_TOL_Y     16
#define TRAAG_EXPL_DMG        1     // voltea

#define TRAAG_HURT_TICKS     12
#define TRAAG_DEATH_BLINK     4
#define TRAAG_DEATH_BREAK_T  30
#define TRAAG_DEATH_PILE_T   70

typedef enum {
    TRAAG_INACTIVE, TRAAG_ENTER, TRAAG_IDLE, TRAAG_WALK,
    TRAAG_FIRE, TRAAG_PUNCH, TRAAG_HURT, TRAAG_DEATH, TRAAG_GONE
} TraagState;

// Tope caminable por X de mundo (para no meterse en la pared).
typedef s16 (*TraagTopAtFn)(s16 worldX);

typedef struct {
    Sprite*       sprite;
    TraagState    state;
    s16           xq, yq;       // centro / pies, en 1/4 px (mundo)
    s16           camX, camY;
    s8            dir;
    s16           hp;
    u8            anim;
    u16           timer;
    u8            flash;        // golpe: parpadeo corto
    u8            invuln;
    u8            accX, accY;   // restos Q8 de la caminata
    u8            dPhase, blinks;
    BossFlash     flashLow;     // parpadeo de vida baja
    s16           arenaLeft, arenaRight;
    s16           laneTop, laneBot;
    TraagTopAtFn  topAt;
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

// Bombas y explosiones (estado de modulo; traagInit las limpia).
void traagMissileUpdate(Player** pls, u8 nPl, s16 camX, s16 camY);
void traagMissileReleaseAll(void);

#endif
