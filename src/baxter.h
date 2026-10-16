#ifndef _BAXTER_H_
#define _BAXTER_H_

#include <genesis.h>
#include "baxter_res.h"   // baxter_boss, baxter_rat, baxter_boom, rat_boom (rescomp)
#include "player.h"

// ===========================================================================
// BAXTER STOCKMAN — jefe de la Scene 3 (sewer) (26/09)
// ===========================================================================
// Base portada del proyecto del companero (Ray Project, src/baxter.c) a
// nuestro motor: N jugadores (Player** pls, u8 nPl), los que estan sin vidas no
// cuentan como objetivo, hitbox por frame del jugador (playerAttackHitsFlying
// para la nave, playerAttackHitsBox para las ratas), un golpe por swing
// (invulnerabilidad corta despues de cada impacto) y ratas que respetan la
// pared de la cloaca y el presupuesto de VRAM de sprites.
//
// NAVE 48x72 (colision 48x62). Todo (nave, ratas y explosiones) comparte UNA
// paleta: va en PAL3.
//   anim 0 volando (2f) · 1 portezuela (f0 abriendo, f1 abierta) · 2 dano
// Pelea (05/10):
//   ENTRA en diagonal desde abajo a la derecha.
//   (12/10) VUELA COMO EN EL ARCADE (medido en la partida grabada,
//   gameplayARCADE/tmnt.avi 8:20-9:30): va de punto en punto de una grilla de
//   3 columnas (cada ~80 px desde el borde izquierdo de la camara) x 3 alturas
//   (arriba, medio, abajo sobre el canal). Casi siempre en diagonal a una
//   columna vecina y cambiando de altura (~1,5 px/f en X, ~3 px/f en Y); por
//   arriba tambien cruza en horizontal. A veces se queda flotando en el punto
//   (sube y baja 20 px cada segundo, 2 a 4 veces). La nave NO ataca: cada
//   BAXTER_DROP_T frames abre la portezuela y suelta un Mouser (en el arcade
//   la cloaca se llena: 6 a 8 a la vez). BAXTER_HP golpes (en la partida
//   grabada fueron 16).
//   (La Lissajous de antes queda en el historial de git.)
// Con poca vida parpadea en rojo. Al morir explota y con el explotan todas
// las ratas vivas.
//
// RATAS (MOUSERS) 40x40: bajan de la nave y caminan despacio. Las dos
// primeras persiguen a la tortuga; el resto deambula con pausas. Alineadas y
// cerca, saltan a morder: se PRENDEN de la tortuga (agarre con mash) y le
// sacan una barra cada RAT_DRAIN_T. Un golpe las despide y mueren.
// Filas del sheet (7 frames cada una):
//   0-1 caminando de FRENTE (las dos filas son UNA caminata: se encadenan)
//       -- reservada para cuando aparezcan dentro del nivel
//   2-3 caminando de COSTADO (idem, dos filas encadenadas)
//   4   caminando hacia ARRIBA (el jugador esta arriba en Y)
//   5   SALTO de ataque (tambien la bajada de la nave y la mordida)
//   6   golpeado
//   7   tirado en el piso antes de explotar
// ===========================================================================

#define BAXTER_ANIM_FLY    0
#define BAXTER_ANIM_DOOR   1
#define BAXTER_ANIM_HURT   2

#define RAT_ANIM_FRONT_A     0   // caminata de frente, 1a mitad
#define RAT_ANIM_FRONT_B     1   //   2a mitad
#define RAT_ANIM_SIDE_A      2   // caminata de costado, 1a mitad
#define RAT_ANIM_SIDE_B      3   //   2a mitad
#define RAT_ANIM_WALK_UP     4
#define RAT_ANIM_JUMP        5   // salto de ataque (tambien la caida de la nave)
#define RAT_ANIM_HURT        6
#define RAT_ANIM_DEAD        7
#define RAT_ANIM_EMERGE      8   // (16/10) sale del hueco de la pared (3 frames, a mano)
#define RAT_EMERGE_TICKS    10   // frames de juego por pose al salir del hueco
#define RAT_JUMP_FRAMES      7   // frames de la fila del salto (se reparten a mano)

#define BAXTER_FRAME_W      48
#define BAXTER_FRAME_H      72
// (30/09) Explosion de la nave: 8 frames de hasta 128x128, partidos en 4
// cuartos de 64x64 (tools/gen_baxter_boom.py). Un Sprite por cuarto, todos
// con la definicion baxter_boom: el ARTE del cuarto es la anim (0 arriba-izq,
// 1 arriba-der, 2 abajo-izq, 3 abajo-der) y el frame es el de la explosion.
// Segun la VRAM libre, algunos cuartos reusan los tiles de otro espejados
// (ver baxterBoomCreate en baxter.c).
#define BAXTER_BOOM_PARTS    4
#define BAXTER_BOOM_HALF    64     // medio lienzo = lado de cada cuarto
#define BAXTER_BOOM_FRAMES   8
#define BAXTER_BOOM_TICKS    7     // ticks por frame (8 x 7 = ~1 s)
#define BAXTER_BODY_H       62     // la parte que colisiona (sin el escape)
#define BAXTER_HALF_W       22
#define BAXTER_HP           32     // golpes (12/10: 16, los del arcade; 14/10: el doble, pedido de Gustavo)
#define BAXTER_LOW_HP        4     // de aca para abajo parpadea en rojo (14/10: 4, por el doble de vida)
#define BAXTER_INVULN       12     // golpeado: sin recibir otro golpe
#define BAXTER_ENTER_XQ    640     // entrada: 2,5 px/f a la izquierda ...
#define BAXTER_ENTER_YQ    384     // ... y 1,5 px/f para arriba
#define BAXTER_ENTER_DY     60     // arranca esto por debajo del centro del vuelo
#define BAXTER_AMP_X        75     // Lissajous: de lado a lado ...
#define BAXTER_PER_X       150     // ... cada 2,5 s
#define BAXTER_AMP_Y        50     // arriba-abajo (o lo que de la arena) ...
#define BAXTER_PER_Y       120     // ... cada 2 s
#define BAXTER_DROP_T       75     // (12/10) un Mouser cada 1,25 s (era 2,5)
// (12/10) Vuelo por grilla (ver arriba).
#define BAXTER_COL_DX       80     // columnas: arenaLeft + 0 / 80 / 160
#define BAXTER_FLY_XQ      384     // 1,5 px/f en X
#define BAXTER_FLY_YQ      768     // 3 px/f en Y
#define BAXTER_HOVER_PCT    35     // % de las llegadas en que se queda flotando
#define BAXTER_BOB_DY       20     // flotando: sube y baja esto ...
#define BAXTER_BOB_MOVE     19     // ... en estos frames ...
#define BAXTER_BOB_HOLD     11     // ... y se queda quieto estos
#define BAXTER_DOOR_T       24     // la portezuela se abre antes de soltarlo
#define BAXTER_DOOR_AFTER   12     // (14/10) ... y sigue abierta esto despues
#define MAX_BAXTER_RATS      8     // pool
#define BAXTER_RATS_ALIVE    6     // tope de vivas (ademas del de VRAM)

#define RAT_FRAME_W         40
#define RAT_HALF_W          14
#define RAT_BODY_H          30
#define RAT_CHASERS          2     // las que persiguen; el resto deambula
#define RAT_FALL_Q         747     // baja de la nave a 2,92 px/f
#define RAT_WALK_Q         107     // camina a 0,42 px/f
#define RAT_PAUSE_T         80     // pausa al llegar a su punto (las que deambulan)
#define RAT_ATK_DY           2     // salta si esta en la lane de la tortuga ...
#define RAT_ATK_DX          37     // ... y a esta distancia o menos
#define RAT_JUMP_T          20     // el salto dura ...
#define RAT_JUMP_Q         533     // ... avanza a 2,08 px/f ...
#define RAT_JUMP_H          14     // ... y sube esto
#define RAT_BITE_DX         16     // muerde (en la bajada) a esta distancia ...
#define RAT_BITE_DY          6     // ... y en esta lane
#define RAT_LATCH_DX         6     // prendida: corrida del centro de la tortuga ...
#define RAT_LATCH_Z         20     // ... y a esta altura
#define RAT_DRAIN_T        120     // una barra cada 2 s
#define RAT_HURT_T          30     // golpeada: despedida ...
#define RAT_FLING_Q        853     // ... a 3,33 px/f
#define RAT_DEAD_T          22     // tirada antes de explotar

typedef enum {
    BAXTER_INACTIVE,
    BAXTER_ENTER,
    BAXTER_FLY,
    BAXTER_DEAD,
    BAXTER_GONE
} BaxterState;

typedef struct {
    Sprite*     sprite;
    BaxterState state;
    s16         x;            // centro de la nave (mundo)
    s16         y;            // borde de arriba de la nave (mundo)
    s16         camX;
    s16         hp;
    u16         timer;
    u16         flightT;      // fase de la Lissajous
    u16         dropT;        // frames desde el ultimo Mouser
    s16         cx, cy;       // centro de la Lissajous
    s16         ampY;         // amplitud vertical (la que entra en la arena)
    // (12/10) vuelo por grilla
    s16         tx, ty;       // punto al que va
    u8          col, row;     // punto de la grilla (0..2)
    u8          hoverLeft;    // medias vueltas de flotar que le quedan
    u8          hoverPh;      // 0 sube, 1 quieto, 2 baja, 3 quieto
    u8          hoverT;
    s16         hoverY0;
    u8          accX, accY;   // restos Q8 de la entrada
    u8          hurtFlash;
    u8          invuln;
    s16         arenaLeft, arenaRight;
    u8          anim;
    Sprite*     boomSprite[BAXTER_BOOM_PARTS];   // los 4 cuartos de la explosion
    u8          boomAnim[BAXTER_BOOM_PARTS];     // anim (arte) de cada cuarto
    u8          boomFrame;
    u8          boomTick;
} Baxter;

// Tope caminable por X de mundo (las ratas no se meten en la pared).
typedef s16 (*BaxterTopAtFn)(s16 worldX);

void baxterInit(Baxter* b);
// Arena en X de mundo (la camara esta clavada) y franja de las ratas.
void baxterSpawn(Baxter* b, s16 arenaLeft, s16 arenaRight, s16 laneTop, s16 laneBot);
void baxterUpdate(Baxter* b, Player** pls, u8 nPl, s16 camX);
bool baxterIsGone(const Baxter* b);
bool baxterCanBeHit(const Baxter* b);
// Golpe de tortugas a la nave. Devuelve TRUE si la destruyo (para el puntaje).
bool baxterPlayerHits(Baxter* b, Player** pls, u8 nPl, s8* killer);
void baxterRelease(Baxter* b);

void baxterRatInitAll(void);
void baxterRatUpdateAll(Player** pls, u8 nPl, s16 camX, BaxterTopAtFn topAt);
// Golpes de tortugas a las ratas; suma el punto al que la mata.
void baxterRatPlayerHits(Player** pls, u8 nPl);
u16  baxterRatAliveCount(void);
u16  baxterRatBusyCount(void);    // vivas o explotando (30/09)
void baxterRatReleaseAll(void);
// (13/10) Mousers fuera de la pelea con Baxter (los huecos de la cloaca).
bool baxterRatSpawnAt(const SpriteDefinition* def, const SpriteDefinition* boomDef,
                      s16 x, s16 landY, s16 fallZ);
void baxterRatSetArena(s16 arenaLeft, s16 arenaRight, s16 laneTop, s16 laneBot);

#endif
