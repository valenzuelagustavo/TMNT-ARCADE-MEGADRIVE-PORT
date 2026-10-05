#ifndef _BAXTER_H_
#define _BAXTER_H_

#include <genesis.h>
#include "baxter_res.h"   // baxter_boss, baxter_rat, baxter_boom, rat_boom (rescomp)
#include "player.h"

// ===========================================================================
// BAXTER STOCKMAN — jefe de la Scene 3 (sewer) (26/09)
// ===========================================================================
// Portado del proyecto del companero (Ray Project, src/baxter.c) a nuestro
// motor: N jugadores (Player** pls, u8 nPl), los que estan sin vidas no
// cuentan como objetivo, hitbox por frame del jugador (playerAttackHitsFlying
// para la nave, playerAttackHitsBox para las ratas), un golpe por swing
// (invulnerabilidad corta despues de cada impacto) y ratas que respetan la
// pared de la cloaca y el presupuesto de VRAM de sprites.
//
// NAVE 48x72 (colision 48x62). Todo (nave, ratas y explosiones) comparte UNA
// paleta: va en PAL3.
//   anim 0 volando (2f) · 1 portezuela (f0 abriendo, f1 abierta) · 2 dano
// Ciclo: entra volando desde la izquierda -> vuela en diagonal de esquina a
// esquina (cada 3 cruces da una vuelta alrededor de su objetivo) -> al llegar
// a la esquina se para, abre la portezuela y suelta ratas -> repite.
// Con poca vida parpadea en rojo. Al morir explota y con el explotan todas
// las ratas vivas.
//
// RATAS (MOUSERS) 40x40: caen de la nave, persiguen al jugador mas cercano y
// lo atacan SALTANDO (de cerca un saltito, de media distancia uno largo).
// (01/10) Filas del sheet (7 frames cada una):
//   0-1 caminando de FRENTE (las dos filas son UNA caminata: se encadenan)
//       -- reservada para cuando aparezcan dentro del nivel
//   2-3 caminando de COSTADO (idem, dos filas encadenadas)
//   4   caminando hacia ARRIBA (el jugador esta arriba en Y)
//   5   SALTO de ataque (por ahora un golpe; despues ira combinado con el
//       jugador)
//   6   golpeado
//   7   tirado en el piso antes de explotar
// Antes se leian como 0 cae · 1 frente · 2 costado · 3 muerde · ...: la fila 3
// (que es la mitad de la caminata de costado) se usaba como mordida y la 0
// (de frente) como caida.
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
#define BAXTER_HP           48     // la misma energia que Rocksteady
#define BAXTER_SPECIAL_DMG   3
#define BAXTER_LOW_HP       12     // de aca para abajo parpadea en rojo
#define BAXTER_INVULN       12     // frames sin recibir otro golpe (1 por swing)
#define MAX_BAXTER_RATS      8     // pool
#define BAXTER_RATS_ALIVE    6     // tope de vivas (ademas del de VRAM)

#define RAT_FRAME_W         40
#define RAT_HALF_W          14
#define RAT_BODY_H          30
#define RAT_HP               2     // dos golpes comunes; el especial la mata
#define RAT_SPEED            1
#define RAT_BITE_RANGE      20
#define RAT_BITE_Y          12
#define RAT_BITE_DMG         1
#define RAT_JUMP_DMG         2     // mordida fuerte
#define RAT_JUMP_MIN_DX     34
#define RAT_HURT_TICKS      14

typedef enum {
    BAXTER_INACTIVE,
    BAXTER_ENTER,
    BAXTER_DROP,
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
    u16         flightT;
    u8          doorPhase;    // 0 cerrada · 1 abriendo · 2 abierta
    u8          dropsThisCycle;
    s16         tgx, tgy;
    u8          orbit;
    u8          legs;
    u8          corner;       // (30/09) ultima esquina visitada (0..3)
    u8          hurtFlash;
    u8          invuln;
    s16         arenaLeft, arenaRight;
    s16         ratLaneTop, ratLaneBot;
    u8          anim;
    Sprite*     boomSprite[BAXTER_BOOM_PARTS];   // (30/09) los 4 cuartos
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

#endif
