#ifndef _GRANITOR_H_
#define _GRANITOR_H_

#include <genesis.h>
#include "granitor_res.h"   // granitor_boss, granitor_flame (rescomp)
#include "player.h"
#include "boss_flash.h"

// ===========================================================================
// TENIENTE GRANITOR — jefe de la Scene 7 (la fabrica) (26/09)
// ===========================================================================
// Base portada del proyecto del companero (Ray Project, src/granitor.c).
// Golpes de las tortugas con playerAttackHitsBox (un golpe por swing), el
// culatazo derriba (playerHitBarsKnockdown), las llamas pegan como proyectil
// y respetan el tope caminable del nivel.
//
// Tanque de piedra con LANZALLAMAS. Celda 144x144, pies en la fila 130.
// Pelea (05/10):
//   QUIETO (40)   alineado con la tortuga y cerca -> culatazo. Si no: 50%
//                 camina, 50% SALTA.
//   CAMINA        la persigue (lane exacta, hasta GRAN_MIN_DIST). Alineado y
//                 cerca -> culatazo; alineado y lejos -> 2/3 lanzallamas.
//   CULATAZO (21) pega en el frame del impacto y derriba.
//   LANZALLAMAS (36) un chorro: una llama cada GRAN_FLAME_EVERY frames.
//   SALTA         en el lugar; al caer tiembla el piso y le saca una barra a
//                 toda tortuga que este parada.
//   Solo recibe dano quieto o caminando.
//   (18/10, pedido de Gustavo):
//   ENTRADA   cae desde arriba cerca de la puerta del ascensor; al caer
//             (y al caer de cada salto) tiembla la pantalla (stageShake).
//   ANTI-TRABADO  aguanta GRAN_STREAK_MAX golpes seguidos; el siguiente lo
//             absorbe y contraataca en el acto con el culatazo hacia el que le
//             pega. Ademas da el culatazo a cualquier tortuga pegada a el (lane
//             +-GRAN_CLOSE_DY), no solo a la perfectamente alineada: antes se
//             lo trababa pegandole desde una lane de al lado.
//   MUERTE    parpadea, se quiebra y queda un montoncito que desaparece,
//             con EXPLOSIONES en cadena sobre el cuerpo (como Krang).
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
// (18/10) 130 -> 140: la bota de atras llega a la fila 141 de la celda. Con
// 130 los pies LOGICOS quedaban 11 px arriba de los que se ven: parado a la
// misma altura que el jefe, la tortuga estaba "en otra lane" (y las patadas
// voladoras no entraban).
#define GRAN_FOOT_OFFSET   140
// (18/10, captura de Gustavo) Hurtbox: el TORSO. Centro un poco hacia
// adelante (los pies estan en x+4 de la celda y el pecho mira al arma),
// 40 px de ancho, de 30 a 84 px sobre los pies (antes +-24 y 0..80).
#define GRAN_BODY_DX         8
#define GRAN_BODY_HALF_W    20
#define GRAN_BODY_TOP       84
#define GRAN_BODY_BOT       30
// (18/10) En el aire (patada voladora) tambien cuenta de los hombros a la
// cabeza: el salto llega a 123 px y la patada pasaba por arriba del torso.
#define GRAN_BODY_TOP_AIR  124

#define GRAN_HP             40
#define GRAN_SPECIAL_DMG     5
#define GRAN_JUMPKICK_DMG    2

// Movimiento (Q8: 256 = 1 px por frame)
#define GRAN_WALK_Q        149     // 0,58 px/f
#define GRAN_MIN_DIST       25     // persiguiendo se acerca hasta aca
#define GRAN_WALK_MAX      240     // tope de una caminata sin decidir
#define GRAN_IDLE_T         40

// Decisiones (centro a centro)
#define GRAN_ALIGN_DY        2     // "alineado": lane a +-2 px
#define GRAN_NEAR_DX        57     // culatazo a esta distancia o menos
#define GRAN_FAR_DX         87     // lanzallamas mas lejos que esto

// Culatazo
#define GRAN_PUNCH_T        21
#define GRAN_PUNCH_HIT_T     4     // frames que pega desde el impacto
#define GRAN_PUNCH_HIT_DX   64
#define GRAN_PUNCH_HIT_W    40
#define GRAN_HIT_DY_UP      12     // (18/10) lane de la tortuga respecto de la suya (antes 5/7)
#define GRAN_HIT_DY_DOWN    12
#define GRAN_PUNCH_DMG       1

// Lanzallamas
#define GRAN_FIRE_T         36     // (18/10) era 60: el chorro duraba demasiado
#define GRAN_FLAME_EVERY     5
#define GRAN_FLAME_DX       78     // (18/10) la llama nace en la punta del arma (x +71 de la celda)
#define GRAN_FLAME_Q      1067     // 4,17 px/f
#define GRAN_FLAME_LIFE     23
#define GRAN_FLAME_HALF_W   12
#define GRAN_FLAME_DY_UP     3     // lane de la tortuga respecto de la llama
#define GRAN_FLAME_DY_DOWN   5
#define GRAN_FLAME_DMG       1
#define MAX_GRANITOR_FLAMES  5

// Salto
#define GRAN_JUMP_VQ       507     // 1,98 px/f para arriba
#define GRAN_GRAV_Q         27     // 0,104 px/f2
#define GRAN_LAND_T         30     // medio segundo despues de caer
#define GRAN_QUAKE_DMG       1

#define GRAN_HURT_TICKS     12
// (18/10) Anti-trabado y cercania (ver arriba).
#define GRAN_STREAK_MAX      3
#define GRAN_STREAK_RESET   50     // frames sin recibir golpes que cortan la racha
#define GRAN_CLOSE_DY       12
#define GRAN_CLOSE_CD       60     // despues de un culatazo "de cercania", tanto sin repetirlo
// (18/10) Entrada: cae desde arriba en arenaLeft + GRAN_DROP_DX (la puerta).
#define GRAN_DROP_DX       176
#define GRAN_DROP_Z        200
#define GRAN_SHAKE_LAND     20     // frames de temblor al caer
// (18/10) Explosiones de la muerte.
#define GRAN_BOOMS           5
#define GRAN_BOOM_GAP       14
#define GRAN_BOOM_FRAMES     8
#define GRAN_BOOM_TICKS      4
#define GRAN_BOOM_HALF      64
#define GRAN_BOOM_DZ        74     // centro de la primera sobre los pies (+10: GRAN_FOOT_OFFSET 140)
#define GRAN_DEATH_BLINK     4
#define GRAN_DEATH_BREAK_T  30
#define GRAN_DEATH_PILE_T   70

typedef enum {
    GRAN_INACTIVE, GRAN_ENTER, GRAN_IDLE, GRAN_WALK,
    GRAN_FIRE, GRAN_PUNCH, GRAN_JUMP, GRAN_HURT, GRAN_DEATH, GRAN_GONE
} GranitorState;

// Tope caminable por X de mundo (para no meterse en la pared).
typedef s16 (*GranitorTopAtFn)(s16 worldX);

typedef struct {
    Sprite*       sprite;
    GranitorState state;
    s16           xq, yq;       // centro / pies, en 1/4 px (mundo)
    s16           z;            // altura del salto (px)
    s16           zq, vz;       // salto en Q8
    u8            landed;
    s16           camX;
    s8            dir;
    s16           hp;
    u8            anim;
    u16           timer;
    u8            flash;        // golpe: parpadeo corto
    u8            invuln;
    u8            accX, accY;   // restos Q8 de la caminata
    u8            dPhase, blinks;
    BossFlash     flashLow;     // parpadeo de vida baja
    u8            streak, streakT;   // (18/10) golpes seguidos / frames sin golpes
    u8            closeCd;           // (18/10) espera para el proximo culatazo de cercania
    s16           arenaLeft, arenaRight;
    s16           laneTop, laneBot;
    GranitorTopAtFn topAt;
} Granitor;

void granitorInit(Granitor* g);
// Arena en X de mundo (la camara esta clavada) y franja de pies. (18/10) Cae
// desde arriba en arenaLeft + GRAN_DROP_DX. Carga PAL3.
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
