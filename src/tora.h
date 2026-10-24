#ifndef _TORA_H_
#define _TORA_H_

#include <genesis.h>
#include "level_snow.h"    // snow_tora, snow_iceball, snow_iceshadow (rescomp)
#include "player.h"
#include "boss_flash.h"

// ===========================================================================
// TORA — jefe de la Scene 3-1 (Central Park con nieve) (23/10)
// ===========================================================================
// Arte: tools/gen_snow_boss.py (Tora_Boss.png). Celda 104x104, pies en el
// borde de abajo, centro en x 52, mira a la DERECHA. PAL3 (la comparte con
// las esferas de hielo y el disco del final).
// Pelea (pedido de Gustavo):
//   PERSIGUE  va hacia la tortuga mas cercana y, alineado y cerca, le pega
//             cuerpo a cuerpo (TORA_PUNCH_DMG, derriba).
//   CORRIDA   cada TORA_DASH_EVERY(+RND) frames corre rapido al extremo de la
//             arena mas lejos de la tortuga,
//   ATRAPA    levanta los brazos: cae del cielo una esfera de hielo y queda
//             sobre sus manos,
//   LANZA     la tira en arco (~media pantalla, hacia la lane de la tortuga;
//             la sombra avisa donde cae). Si pega: TORA_BALL_DMG barras.
//   Recibe dano siempre (menos entrando y muriendo); solo se frena (HURT)
//   persiguiendo o pegando: corriendo, atrapando y lanzando no (super armor).
//   ANTI-TRABADO: pasada la racha de golpes, el siguiente lo absorbe y se va
//   corriendo a lanzar.
//   MUERTE    las 3 poses de la fila 4, parpadea y desaparece.
//
// ESFERAS DE HIELO (modulo toraIce*): las del jefe y las que llueven del cielo
// en dos tramos del nivel (level_snow.c). Las que llueven primero muestran la
// SOMBRA (parpadea) y despues caen; si aplastan a una tortuga le sacan
// TORA_RAIN_DMG barras (TORA_RAIN_DMG_ARCADE en ARCADE). Al tocar el piso se
// rompen (cuadros 1-2).
// ===========================================================================

#define TORA_FRAME_W       104
#define TORA_FRAME_H       104
#define TORA_CX             52

#define TORA_ANIM_WALK       0
#define TORA_ANIM_THROW      1     // 0 manos arriba (espera la esfera), 1 la lanza
#define TORA_ANIM_PUNCH      2
#define TORA_ANIM_HURT       3
#define TORA_ANIM_DEATH      4

// Hurtbox (sobre los pies)
#define TORA_BODY_HALF_W    20
#define TORA_BODY_TOP       84
#define TORA_BODY_BOT        8
#define TORA_BODY_TOP_AIR  100
#define TORA_HIT_DY_UP      10
#define TORA_HIT_DY_DOWN    10

#define TORA_HP             34
#define TORA_SPECIAL_DMG     5
#define TORA_JUMPKICK_DMG    2

// Movimiento (Q8)
#define TORA_WALK_Q        230     // 0,9 px/f
#define TORA_LANE_Q        170
#define TORA_DASH_PX         4     // px/f corriendo
#define TORA_MIN_DIST       30
#define TORA_ENTER_DX       70
#define TORA_WALK_TICKS      9     // frames por cuadro caminando
#define TORA_DASH_TICKS      4     // corriendo     // entra desde el borde derecho hasta aca adentro

// Golpe cuerpo a cuerpo
#define TORA_NEAR_DX        52
#define TORA_ALIGN_DY        6
#define TORA_PUNCH_WIND     12     // cuadro 0
#define TORA_PUNCH_T        28     // total
#define TORA_PUNCH_HIT_T     4
#define TORA_PUNCH_HIT_DX   38
#define TORA_PUNCH_HIT_W    26
#define TORA_PUNCH_DMG       1
#define TORA_PUNCH_CD       36

// Corrida + esfera
#define TORA_DASH_EVERY    200
#define TORA_DASH_RND       90
#define TORA_EDGE_IN        28     // se para a esto del borde de la arena
#define TORA_HOLD_Z        122     // centro de la esfera sobre los pies (en las manos)
#define TORA_HOLD_DX         3
#define TORA_HOLD_T         20     // con la esfera arriba antes de lanzarla
#define TORA_THROW_T        24     // pose de lanzar
#define TORA_THROW_MIN     110     // alcance del arco
#define TORA_THROW_MAX     180
#define TORA_THROW_AIR      44     // frames de vuelo
#define TORA_BALL_DMG        2

#define TORA_HURT_TICKS     12
#define TORA_STREAK_MAX      4
#define TORA_STREAK_RESET   50
#define TORA_DEATH_STEP     12     // frames por pose de la muerte
#define TORA_DEATH_BLINK     6

// Esferas
#define TORA_ICE_MAX         3
#define TORA_ICE_R          32     // radio: apoyada en el piso, centro a esto
#define TORA_ICE_SKY_Z     210
#define TORA_ICE_WARN       44     // la sombra sola, avisando
#define TORA_ICE_HIT_DX     24
#define TORA_ICE_HIT_DY     10
#define TORA_ICE_HIT_Z      52     // borde de abajo por debajo de esto: pega
#define TORA_RAIN_DMG        2
#define TORA_RAIN_DMG_ARCADE 3

typedef enum {
    TORA_INACTIVE, TORA_ENTER, TORA_CHASE, TORA_PUNCH, TORA_DASH,
    TORA_CATCH, TORA_THROW, TORA_HURT, TORA_DEATH, TORA_GONE
} ToraState;

typedef s16 (*ToraTopAtFn)(s16 worldX);

typedef struct {
    Sprite*     sprite;
    ToraState   state;
    s16         xq, yq;       // pies (mundo), 1/4 px
    s16         camX;
    s8          dir;
    s16         hp;
    u8          anim;
    u16         walkT;        // (23/10) caminata a mano
    u16         timer;
    u16         dashT;        // frames hasta la proxima corrida
    s16         dashTo;       // x de destino de la corrida
    u8          flash, invuln;
    u8          accX, accY;
    u8          streak, streakT;
    u8          punchCd;
    u8          blinks;
    s8          ball;         // esfera que tiene en las manos (-1 ninguna)
    BossFlash   flashLow;
    s16         arenaLeft, arenaRight, laneTop, laneBot;
    ToraTopAtFn topAt;
} Tora;

void toraInit(Tora* t);
// Arena en X de mundo (camara clavada) y franja de pies. Carga PAL3.
void toraSpawn(Tora* t, s16 arenaLeft, s16 arenaRight, s16 laneTop, s16 laneBot,
               ToraTopAtFn topAt);
void toraUpdate(Tora* t, Player** pls, u8 nPl, s16 camX);
bool toraIsDying(const Tora* t);
bool toraIsGone(const Tora* t);
bool toraPlayerHits(Tora* t, Player** pls, u8 nPl, s8* killer);
void toraRelease(Tora* t);

// Esferas de hielo (estado de modulo).
void toraIceInit(void);
// Carga en PAL3 la paleta del jefe (esferas y disco la usan).
void toraLoadPal(void);
// Lluvia: la sombra aparece en (x, lane) y a TORA_ICE_WARN cae la esfera.
bool toraIceRain(s16 x, s16 lane);
void toraIceUpdate(Player** pls, u8 nPl, s16 camX);
u16  toraIceAlive(void);
void toraIceReleaseAll(void);

#endif
