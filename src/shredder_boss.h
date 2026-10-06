#ifndef _SHREDDER_BOSS_H_
#define _SHREDDER_BOSS_H_

#include <genesis.h>
#include "final_bosses.h"   // shredder_boss, shredder_beam_a/b, shredder_fx, shredder_helmet
#include "player.h"

// ===========================================================================
// SHREDDER — jefe final (9-1, la sala del portal). Hoja de pelea (06/10)
// ===========================================================================
// Hoja shredder_boss (tools/gen_final_bosses.py): celdas de 160x96, pies en
// el borde de abajo, mira a la DERECHA. Filas 0-12 con casco, 13-22 sin casco.
// Se dibuja con PAL2 (la carga al aparecer, cuando Krang ya se fue).
//
// Pelea (la del remake de PC, grupo "sreder" de codigostage6-2):
//   Son DOS: el verdadero y una COPIA que aparece a su lado. El verdadero
//   busca a la tortuga por la DERECHA y la copia por la IZQUIERDA (se paran a
//   50 px). Se gana al vencer al verdadero.
//   APARECE      se materializa (bolita que crece) con la risa.
//   PARADO 12    mira a la tortuga mas cercana. Cerca: si la tortuga esta en
//                el piso, espera; si no, un ataque al azar (1 a 4). Lejos:
//                camina.
//   CAMINA       se acomoda en la lane y a 50 px de la tortuga. Alineado y
//                cerca: 50% ataque / 50% RAYO (o, si la tortuga esta en el
//                piso, 50% espera / 50% se cubre). Alineado y lejos con el
//                rayo cargado: RAYO.
//                Decide recien despues de 8 ticks caminando (la ventana
//                para pegarle).
//   ATAQUES 1-4  estocada, tajo de arriba (derriba), tajo y barrida (derriba).
//   RAYO         junta chispas en la espada (41), llamarada (15) y dispara el
//                TRIDENTE de rayos (40): a la tortuga de su lane que alcance
//                la convierte en tortuga comun = pierde la vida entera.
//   GOLPEADO 24  al cuarto golpe seguido contraataca con la estocada; despues
//                del golpe tiene el rayo cargado; con 2 golpes seguidos, si
//                lo siguen atacando de cerca, se cubre.
//   SE CUBRE 35  agachado: los golpes no le hacen nada.
//   Solo recibe dano CAMINANDO o en los ataques 1-3, y DE FRENTE (la tortuga
//   mirandolo a el).
//   LA COPIA     tiene media vida. Con poca vida pierde el CASCO (sale
//                rebotando) y sigue sin casco (sin rayo). Al caer, a los 6 s
//                vuelve a aparecer con casco.
//   Un solo RAYO a la vez entre los dos.
//   MUERE        el verdadero grita y se desvanece (la copia con el).
// ===========================================================================

// Filas de la hoja (con casco)
#define SHRED_ANIM_IDLE      0
#define SHRED_ANIM_WALK      1
#define SHRED_ANIM_WALKUP    2
#define SHRED_ANIM_BLOCK     3
#define SHRED_ANIM_ATK1      4     // 4 a 7: ataques 1 a 4
#define SHRED_ANIM_RAY       8
#define SHRED_ANIM_HURT      9
#define SHRED_ANIM_APPEAR   10
#define SHRED_ANIM_FLASH    11
#define SHRED_ANIM_GHOST    12
#define SHRED_ANIM_BARE     13     // + fila con casco (0-7, 9): la misma sin casco
#define SHRED_ANIM_B_HURT   21
#define SHRED_ANIM_B_DEATH  22

#define SHRED_FRAME_W      160
#define SHRED_FOOT_OFFSET   96
#define SHRED_BODY_HALF_W   14
#define SHRED_BODY_H        72

#define SHRED_HP            35
#define SHRED_CLONE_HP      17
#define SHRED_HELMET_HP     10     // la copia pierde el casco por debajo de esto
#define SHRED_SPECIAL_DMG    4
#define SHRED_JUMPKICK_DMG   2
#define SHRED_INVULN        12     // un golpe por swing

// Movimiento (Q8)
#define SHRED_WALK_Q       288     // 1,125 px/f
#define SHRED_WALK_FRAME     6
#define SHRED_SIDE_DX       50     // se para a esta distancia de la tortuga
#define SHRED_TURN_DZ       15
#define SHRED_ALIGN_DY       3
#define SHRED_NEAR_DX       57
#define SHRED_FAR_DX        80

#define SHRED_STANCE_T      12
#define SHRED_WALK_REACT     8     // camina al menos esto antes de atacar
#define SHRED_WAIT_T        60
#define SHRED_BLOCK_T       35
#define SHRED_HURT_T        24
#define SHRED_COUNTER_HITS   4

// Ataques
#define SHRED_HIT_DY         5
#define SHRED_ATK_DMG        2

// Rayo
#define SHRED_RAY_CHARGE    41     // chispas en la espada
#define SHRED_RAY_FLAME     15     // llamarada
#define SHRED_RAY_HOLD      60     // espada al frente (el rayo dura 40)
#define SHRED_BEAM_T        40
#define SHRED_BEAM_DY        3
#define SHRED_TIP_UP_DX      2     // punta de la espada en alto
#define SHRED_TIP_UP_DZ     81
#define SHRED_TIP_DX        32     // punta de la espada al frente
#define SHRED_TIP_DZ        64

// Entradas y muertes
#define SHRED_APPEAR_T      45
#define SHRED_CLONE_DX      67     // la copia aparece a la izquierda
#define SHRED_CLONE_BACK   360     // 6 s hasta que vuelve
#define SHRED_CLONE_DIE_T  120
#define SHRED_DEATH_T      190     // lo que dura el grito

typedef enum {
    SHRED_INACTIVE, SHRED_APPEAR, SHRED_STANCE, SHRED_WAIT, SHRED_WALK, SHRED_ATTACK,
    SHRED_RAY, SHRED_HURT, SHRED_BLOCK, SHRED_DEATH, SHRED_DOWN, SHRED_GONE
} ShredState;

typedef struct {
    Sprite*    sprite;
    Sprite*    beamA;
    Sprite*    beamB;
    Sprite*    fx;
    Sprite*    helmet;
    ShredState state;
    s16        xq, yq;        // centro / pies, en 1/4 px (mundo)
    s16        camX, camY;
    s8         dir;
    s16        hp;
    u16        timer;
    u8         atk;           // 0..3
    u8         invuln, flash;
    u8         accX, accY;
    u8         hits;          // golpes seguidos
    u8         rayReady;
    u8         hitMask;
    u8         beamOn;
    s16        beamX, beamY;  // origen del rayo (mundo, a la altura de la punta)
    s8         beamDir;
    u8         beamT;
    bool       clone;
    bool       bare;          // sin casco
    s16        helmX, helmY, helmZ, helmVz;
    s8         helmDir;
    u16        backTimer;     // la copia: cuenta para volver
    s16        arenaL, arenaR, laneT, laneB;
} ShredderBoss;

void shredderInit(ShredderBoss* s);
// Aparece en (x, y) de mundo. clone = la copia (media vida, se para a la
// izquierda de la tortuga). Carga PAL2.
void shredderSpawn(ShredderBoss* s, s16 x, s16 y, s16 arenaL, s16 arenaR, s16 laneT, s16 laneB,
                   bool clone);
void shredderUpdate(ShredderBoss* s, Player** pls, u8 nPl, s16 camX, s16 camY);
// Golpes de las tortugas. TRUE si lo tumbo (killer = indice); el que gana
// la pelea es el verdadero (shredderIsGone).
bool shredderPlayerHits(ShredderBoss* s, Player** pls, u8 nPl, s8* killer);
bool shredderIsDying(const ShredderBoss* s);
bool shredderIsGone(const ShredderBoss* s);
// TRUE cuando termino de aparecer (para sacar la copia a su lado).
bool shredderIsReady(const ShredderBoss* s);
// El verdadero murio: la copia se desvanece y no vuelve.
void shredderVanish(ShredderBoss* s);
void shredderRelease(ShredderBoss* s);

#endif
