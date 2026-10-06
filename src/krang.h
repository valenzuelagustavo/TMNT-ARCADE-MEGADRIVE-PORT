#ifndef _KRANG_H_
#define _KRANG_H_

#include <genesis.h>
#include "final_bosses.h"   // krang_boss, krang_fist, krang_head, krang_bolt, krang_zap
#include "player.h"
#include "boss_flash.h"

// ===========================================================================
// KRANG (el cuerpo androide) — primer jefe de la sala final (9-1) (06/10)
// ===========================================================================
// Hoja krang_boss (tools/gen_final_bosses.py): celdas de 144x144, pies en el
// borde de abajo, mira a la DERECHA. Se dibuja con PAL2.
//
// Pelea (la del remake de PC, grupo "krang" de codigostage6-2):
//   ENTRA      cae del techo y saca pecho (burla).
//   QUIETO 12  mira a la tortuga mas cercana: alineado y cerca -> patada; si
//              no, camina.
//   CAMINA     la persigue (lane exacta, hasta 25 px), sin tope de tiempo.
//              Ataca recien despues de 8 ticks caminando (la ventana para
//              pegarle).
//              Alineado y cerca: 50% patada / 50% rayo. Alineado y lejos con
//              el brazo cargado: PUNO COHETE (y el brazo queda descargado).
//   PATADA 34  pega desde el ultimo frame y derriba.
//   RAYO 36    la antena junta energia y un relampago cae del techo sobre la
//              tortuga (o delante suyo); el que esta ahi cae.
//   PUNO 48    se agacha y dispara el puno a 4,75 px/f por la lane.
//   BURLA 54   saca pecho, intocable.
//   Tras patada o rayo se recarga el brazo. Despues de cada accion: si la
//   tortuga quedo en el piso, burla; si no, vuelve a caminar (o burla al
//   azar si quedo lejos).
//   Solo recibe dano QUIETO o CAMINANDO. Parpadea con poca vida.
//   MUERE 240  grita, se electrocuta y estalla; del pecho sale la CABEZA, que
//              flota hablando unos 4 s y se escapa por arriba.
// ===========================================================================

#define KRANG_ANIM_IDLE     0
#define KRANG_ANIM_WALK     1
#define KRANG_ANIM_KICK     2
#define KRANG_ANIM_LASER    3
#define KRANG_ANIM_ARM      4
#define KRANG_ANIM_TAUNT    5
#define KRANG_ANIM_HURT     6
#define KRANG_ANIM_DEATH    7

#define KRANG_FRAME_W     144
#define KRANG_FOOT_OFFSET 144
#define KRANG_BODY_HALF_W  20
#define KRANG_BODY_H      110

#define KRANG_HP           40
#define KRANG_SPECIAL_DMG   5
#define KRANG_JUMPKICK_DMG  2
#define KRANG_HURT_TICKS   30

// Movimiento (Q8: 256 = 1 px por frame)
#define KRANG_WALK_Q      213     // 0,83 px/f
#define KRANG_WALK_FRAME   12     // ticks por frame de la caminata
#define KRANG_MIN_DIST     25
#define KRANG_TURN_DZ      15     // zona muerta para darse vuelta
#define KRANG_IDLE_T       12
#define KRANG_WALK_REACT    8     // camina al menos esto antes de atacar

// Decisiones (centro a centro)
#define KRANG_ALIGN_DY      3
#define KRANG_NEAR_DX      57
#define KRANG_FAR_DX       87

// Patada
#define KRANG_KICK_T       34
#define KRANG_KICK_HIT_T   20     // pega desde aca (ultimo frame)
#define KRANG_KICK_REACH   70
#define KRANG_HIT_DY        5
#define KRANG_KICK_DMG      2

// Rayo del techo
#define KRANG_LASER_T      36
#define KRANG_BOLT_AT      24     // el relampago sale en el ultimo frame
#define KRANG_BOLT_MIN     24     // donde cae, delante suyo
#define KRANG_BOLT_MAX     96
#define KRANG_BOLT_FALL     8     // ticks que tarda en bajar
#define KRANG_BOLT_LIFE    26
#define KRANG_BOLT_HALF_W  14
#define KRANG_BOLT_DY       6
#define KRANG_BOLT_DMG      2

// Puno cohete
#define KRANG_ARM_T        48
#define KRANG_FIST_AT      36     // sale en el ultimo frame
#define KRANG_FIST_DX      71     // la punta del puno delante del centro
#define KRANG_FIST_DZ      62     // altura sobre los pies
#define KRANG_FIST_Q     1216     // 4,75 px/f
#define KRANG_FIST_HALF_W  14
#define KRANG_FIST_DY       3
#define KRANG_FIST_DMG      2

#define KRANG_TAUNT_T      54

// Entrada y muerte
#define KRANG_DROP_Z      200
#define KRANG_DEATH_T     240
#define KRANG_HEAD_DZ      70     // la cabeza sale a esta altura
#define KRANG_HEAD_FLOAT  240     // flota hablando
#define KRANG_HEAD_UP_Q   512     // 2 px/f al escaparse

typedef enum {
    KRANG_INACTIVE, KRANG_DROP, KRANG_IDLE, KRANG_WALK, KRANG_KICK, KRANG_LASER,
    KRANG_ARM, KRANG_TAUNT, KRANG_HURT, KRANG_DEATH, KRANG_HEAD, KRANG_GONE
} KrangState;

typedef struct {
    Sprite*    sprite;
    Sprite*    head;
    Sprite*    headShadow;
    KrangState state;
    s16        xq, yq;       // centro / pies, en 1/4 px (mundo)
    s16        z;            // altura (la caida de la entrada, la cabeza)
    s16        vz;
    s16        camX, camY;
    s8         dir;
    s16        hp;
    u16        timer;
    u8         flash, invuln;
    u8         accX, accY;
    u8         armReady;
    u8         hitMask;      // jugadores ya alcanzados por la patada
    BossFlash  flashLow;
    s16        arenaL, arenaR, laneT, laneB;
} Krang;

void krangInit(Krang* k);
// Cae en (x, lane media) de mundo. Carga PAL2.
void krangSpawn(Krang* k, s16 x, s16 arenaL, s16 arenaR, s16 laneT, s16 laneB);
void krangUpdate(Krang* k, Player** pls, u8 nPl, s16 camX, s16 camY);
bool krangPlayerHits(Krang* k, Player** pls, u8 nPl, s8* killer);
bool krangIsDying(const Krang* k);   // muriendo o ya cabeza
bool krangIsGone(const Krang* k);    // la cabeza ya se fue
void krangRelease(Krang* k);

// Puno y relampagos (estado de modulo; krangInit los limpia).
void krangShotsUpdate(Player** pls, u8 nPl, s16 camX, s16 camY);
void krangShotsReleaseAll(void);

#endif
