#ifndef _GRANITOR_H_
#define _GRANITOR_H_

#include <genesis.h>
#include "granitor_res.h"   // granitor_boss, granitor_flame (rescomp)
#include "player.h"

// ===========================================================================
// TENIENTE GRANITOR — jefe de la Scene 7 (la fabrica) (26/09)
// ===========================================================================
// Portado del proyecto del companero (Ray Project, src/granitor.c) a nuestro
// motor: N jugadores (los que estan sin vidas no cuentan como objetivo),
// golpes de las tortugas con playerAttackHitsBox (un golpe por swing), el
// culatazo derriba (playerHitBarsKnockdown), las llamas pegan como proyectil
// y respetan el tope caminable del nivel.
//
// Tanque de piedra con LANZALLAMAS. Celda 144x144, pies en la fila 130.
//   QUIETO    se da vuelta hacia el objetivo y decide.
//   CAMINA    se acerca y se alinea en lane.
//   DISPARA   de lejos: suelta una llamarada que avanza CRECIENDO y al final
//             queda FIJA ardiendo un rato en el piso.
//   CULATAZO  de cerca: 4 frames de avance, golpe en el 5to y remate.
//   MUERTE    parpadea, se quiebra y queda un montoncito que desaparece.
// ===========================================================================

#define GRAN_ANIM_IDLE1    0
#define GRAN_ANIM_IDLE2    1
#define GRAN_ANIM_WALK     2
#define GRAN_ANIM_FIRE     3
#define GRAN_ANIM_PUNCH    4
#define GRAN_ANIM_DAMAGE   5

#define GRAN_DMG_FLASH     0
#define GRAN_DMG_BREAK     1
#define GRAN_DMG_PILE      2

#define GRAN_FRAME_W       144
#define GRAN_FRAME_H       144
#define GRAN_FOOT_OFFSET   130
#define GRAN_BODY_HALF_W    24     // hurtbox (como la tortuga, un poco mas ancho)
#define GRAN_BODY_H         80

#define GRAN_HP             96     // el doble que Rocksteady
#define GRAN_SPECIAL_DMG     2
#define GRAN_INVULN         12     // frames sin recibir otro golpe (1 por swing)

#define GRAN_WALK_Q          3     // velocidad en 1/4 px
#define GRAN_WALK_DY        14
#define GRAN_MIN_DIST       46
#define GRAN_PUNCH_RANGE    92
#define GRAN_FIRE_RANGE     96
#define GRAN_IDLE_DECIDE     8
#define GRAN_FIRE_CD        80
#define GRAN_PUNCH_CD       28

#define GRAN_FIRE_HOLD      40
#define GRAN_FLAME_DX       54
#define MAX_GRANITOR_FLAMES  2

#define GRAN_PUNCH_TICKS     8
#define GRAN_PUNCH_IMPACT_T 12
#define GRAN_PUNCH_RECOV_T  18
#define GRAN_PUNCH_ADV       1
#define GRAN_PUNCH_HIT_DX   64
#define GRAN_PUNCH_HIT_W    40
#define GRAN_PUNCH_TOL_Y    24     // |dy| de pies para que conecte
#define GRAN_PUNCH_DMG       2

#define GRAN_HURT_TICKS     12
// (port) Anti-trabado, como Bebop: las tortugas lo encadenaban a golpes y
// nunca podia responder. Aguanta GRAN_COUNTER_HITS golpes seguidos con
// flinch; el siguiente lo absorbe y contraataca en el acto con el culatazo.
// La racha se corta sola tras GRAN_COMBO_RESET frames sin recibir golpes.
#define GRAN_COUNTER_HITS    3
#define GRAN_COMBO_RESET    50
#define GRAN_DEATH_BLINK     4
#define GRAN_DEATH_BREAK_T  30
#define GRAN_DEATH_PILE_T   70

#define GRAN_FLAME_SPEED     3
#define GRAN_FLAME_GROW_T    7
#define GRAN_FLAME_TRAVEL_T 70
#define GRAN_FLAME_BURN_T  150
#define GRAN_FLAME_DMG       1
#define GRAN_FLAME_TOL_Y    16     // |dy| de lane para que la llama queme

typedef enum {
    GRAN_INACTIVE, GRAN_ENTER, GRAN_IDLE, GRAN_WALK,
    GRAN_FIRE, GRAN_PUNCH, GRAN_HURT, GRAN_DEATH, GRAN_GONE
} GranitorState;

// Tope caminable por X de mundo (para no meterse en la pared).
typedef s16 (*GranitorTopAtFn)(s16 worldX);

typedef struct {
    Sprite*       sprite;
    GranitorState state;
    s16           xq, yq;       // centro / pies, en 1/4 px (mundo)
    s16           camX;
    s8            dir;
    s16           hp;
    u8            anim;
    u16           timer;
    u16           attackCd;
    u8            flash;
    u8            invuln;
    u8            idleToggle;
    u8            pFrame, pLanded;
    u8            fFired;
    u8            comboHits;    // golpes recibidos seguidos (anti-trabado)
    u8            calm;         // frames sin recibir golpes
    u8            dPhase, blinks;
    s16           arenaLeft, arenaRight;
    s16           laneTop, laneBot;
    GranitorTopAtFn topAt;
} Granitor;

void granitorInit(Granitor* g);
// Arena en X de mundo (la camara esta clavada) y franja de pies. Entra
// caminando desde la derecha. Carga PAL3.
void granitorSpawn(Granitor* g, s16 arenaLeft, s16 arenaRight, s16 laneTop, s16 laneBot,
                   GranitorTopAtFn topAt);
void granitorUpdate(Granitor* g, Player** pls, u8 nPl, s16 camX);
bool granitorCanBeHit(const Granitor* g);
bool granitorIsDying(const Granitor* g);
bool granitorIsGone(const Granitor* g);
// Golpe de las tortugas. Devuelve TRUE si lo mato (killer = indice).
bool granitorPlayerHits(Granitor* g, Player** pls, u8 nPl, s8* killer);
void granitorRelease(Granitor* g);

// Llamas (estado de modulo; granitorInit las limpia).
void granitorFlameUpdate(Player** pls, u8 nPl, s16 camX);
void granitorFlameReleaseAll(void);

#endif
