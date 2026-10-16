// ===========================================================================
// level6_1.c - Scene 6: la autopista en PATINETA (26/09; arcade 12/10)
// ===========================================================================
// Arte del fondo del proyecto del companero (Ray Project): la autopista larga
// (6376 px) con el guardarrail y, arriba, el cielo con el skyline en parallax.
//
// (12/10) COMO EN EL ARCADE (gameplayARCADE/tmnt.avi, 1060-1170 s; ver
// claude/patinetas-nivel-arcade-12-10.md):
//   - Las tortugas y los foot soldiers andan en PATINETA y la ruta CORRE SOLA
//     (en el arcade a 6 px/f, en el port a 5). Motor: stage_level con
//     autoScrollQ (la camara del juego queda fija en una pantalla, lo que
//     avanza es el fondo, que se repite) y skateLift (se dibujan parados
//     sobre la tabla y no caminan).
//   - La patineta de cada tortuga y de cada soldier es un sprite aparte, en
//     el piso: cuando la tortuga salta, la tabla sigue abajo y vuelve a caer
//     arriba. Todas las tablas del mismo tipo comparten los tiles.
//   - HELICOPTEROS (FootSoldier_helicopter.png): entran por la derecha,
//     revolotean adelante de la tortuga con su sombra en la ruta y disparan.
//     Los rosas tiran balas que bajan en diagonal y dejan una llamita en el
//     piso; los violetas (la misma hoja con la paleta cambiada) sueltan
//     bombas que explotan. Solo se les pega SALTANDO.
//   - Oleadas por tiempo (avance del fondo): helicopteros rosas, blancos en
//     patineta, helicopteros rosas, los del martillo, violetas + martillos...
//
// PALETAS: PAL0 ruta + cielo (tools/gen_level6_skate.py le metio 3 celestes),
// PAL1 tortugas y TODAS las patinetas, PAL2 foot soldiers y explosiones, PAL3
// el helicoptero, sus balas y llamas O los blancos (nunca a la vez: SP_WAIT).
//
// Sin jefe: el nivel se gana con las oleadas limpias al llegar al final, y
// sigue la Scene 7 (la fabrica).
// Musica: music_skate, "14 - Highway Chaser (Scene 3-2)".
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level6_1.h"          // bg_skate*, sk_* (rescomp)
#include "stage_level.h"
#include "enemy.h"             // ENEMY_TYPE_*, sprVramFits, enemySkateMode
#include "enemies.h"           // explosion_sprite
#include "audio.h"

#define SCREEN_W            320
#define LVL61_WALK_Y_MIN    134
#define LVL61_WALK_Y_MAX    206
#define LVL61_SKATE_LIFT      5     // la tortuga parada sobre la tabla (13/10: 10 flotaba)
#define LVL61_SCROLL_Q    1280      // 5 px/f de AVANCE (oleadas y final; el arcade va a 6)
#define LVL61_SCROLL_VIS_Q 1792     // (14/10) 7 px/f lo que SE VE correr la ruta (mas sensacion de velocidad)

static s16 a16(s16 v) { return (v < 0) ? (s16)-v : v; }
static u16 fr61;                    // frames del nivel (animaciones)

static void sfxHit(void) {
    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
}

// ===========================================================================
// EXPLOSIONES (la del TNT, PAL2)
// ===========================================================================
#define BOOM_MAX     3
#define BOOM_T      42
static struct { Sprite* s; s16 x, y; u16 t; } boom[BOOM_MAX];

static void boomAt(s16 cx, s16 y) {
    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                   SOUND_PCM_CH3, 15, FALSE, FALSE);
    for (u16 i = 0; i < BOOM_MAX; i++) {
        if (boom[i].s) continue;
        if (!sprVramFits(explosion_sprite.maxNumTile)) return;
        boom[i].s = SPR_addSprite(&explosion_sprite, -64, -64, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        if (!boom[i].s) return;
        boom[i].x = (s16)(cx - 32); boom[i].y = (s16)(y - 56); boom[i].t = 0;
        SPR_setDepth(boom[i].s, SPR_MIN_DEPTH + 1);
        return;
    }
}

static void boomUpdate(void) {
    for (u16 i = 0; i < BOOM_MAX; i++) {
        if (!boom[i].s) continue;
        if (++boom[i].t >= BOOM_T) { SPR_releaseSprite(boom[i].s); boom[i].s = NULL; continue; }
        SPR_setPosition(boom[i].s, boom[i].x, (s16)(boom[i].y - stageCamY));
    }
}

// ===========================================================================
// PATINETAS (PAL1). Una tabla por tortuga y por soldier, en el piso (no
// sigue el salto). Las de cada tipo comparten los tiles: una "base" fuera
// de pantalla los sube a la VRAM y las demas apuntan ahi.
// ===========================================================================
#define BOARD_W        64
#define BOARD_H        24
#define BOARD_PL_BACK   6      // (13/10) la de las tortugas, corrida hacia atras
#define BOARD_DECK     11      // fila de la tabla donde apoyan los pies (el medio de la cubierta)
static Sprite* boardBaseT;     // la base de las de las tortugas
static Sprite* boardBaseF;     // y la de los foot soldiers
static Sprite* boardPl[MAX_PLAYERS];
static Sprite* boardEn[MAX_ENEMIES];

static Sprite* boardBase(const SpriteDefinition* d) {
    if (!sprVramFits(d->maxNumTile)) return NULL;
    Sprite* s = SPR_addSpriteEx(d, -96, 0, TILE_ATTR(PAL1, FALSE, FALSE, FALSE),
                                SPR_FLAG_AUTO_VRAM_ALLOC | SPR_FLAG_AUTO_TILE_UPLOAD);
    if (s) SPR_setVisibility(s, VISIBLE);   // siempre "visible" (fuera de pantalla): asi sube los tiles
    return s;
}

static Sprite* boardCopy(Sprite* base, const SpriteDefinition* d) {
    if (!base) return NULL;
    Sprite* s = SPR_addSpriteEx(d, -96, 0,
                                TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE,
                                               base->attribut & TILE_INDEX_MASK), 0);
    if (s) SPR_setVisibility(s, HIDDEN);
    return s;
}

static void boardPlace(Sprite* s, s16 cx, s16 y, bool show) {
    if (!s) return;
    if (!show) { SPR_setVisibility(s, HIDDEN); return; }
    SPR_setVisibility(s, VISIBLE);
    SPR_setPosition(s, (s16)(cx - BOARD_W / 2), (s16)(y - LVL61_SKATE_LIFT - BOARD_DECK - stageCamY));
    SPR_setDepth(s, (s16)(-y + 2));          // detras de los pies
}

// SPR_defragVRAM (lo llama sprVramFits cuando la VRAM esta fragmentada) muda
// los sprites con VRAM automatica: las bases. Las copias apuntan a un indice
// fijo y hay que seguirlas (como hudSharedSync en el HUD).
static void boardFollow(Sprite* copy, Sprite* base) {
    if (!copy || !base) return;
    u16 idx = base->attribut & TILE_INDEX_MASK;
    if ((copy->attribut & TILE_INDEX_MASK) != idx) SPR_setVRAMTileIndex(copy, idx);
}

static void boardsUpdate(Player** pls, u8 nPl) {
    for (u8 k = 0; k < nPl && k < MAX_PLAYERS; k++) {
        if (!boardPl[k]) boardPl[k] = boardCopy(boardBaseT, &sk_board_turtle);
        boardFollow(boardPl[k], boardBaseT);
        Player* p = pls[k];
        bool show = (bool)(p->sprite && !isPlayerGameOver(p));
        // (13/10) la tabla corrida hacia atras: el cuerpo de la tortuga queda
        // centrado sobre la tabla (antes se la veia corrida hacia la cola)
        s16 bcx = (s16)(p->x + PLAYER_SPRITE_W / 2 - ((p->dir >= 0) ? BOARD_PL_BACK : -BOARD_PL_BACK));
        boardPlace(boardPl[k], bcx, getPlayerY(p), show);
    }
    Enemy* en = stageEnemies();
    for (u16 i = 0; i < MAX_ENEMIES; i++) {
        if (!boardEn[i]) boardEn[i] = boardCopy(boardBaseF, &sk_board_foot);
        boardFollow(boardEn[i], boardBaseF);
        bool show = (bool)(en[i].state != ENEMY_STATE_INACTIVE && en[i].sprite);
        boardPlace(boardEn[i], getEnemyCenterX(&en[i]), en[i].y, show);
    }
}

// ===========================================================================
// HELICOPTEROS (PAL3). Dos sprites por helicoptero dentro del cuadro de
// 168x168 del arte: el CUERPO (sk_cop_body, 112x120, 9 sprites de hardware) y
// el ROTOR (sk_cop_rotor, 168x80, 2 posiciones, 5 sprites). Los tiles se
// suben UNA vez a la VRAM que reservan 3 sprites base ocultos (cuerpo, rotor
// A, rotor B) y los que vuelan los comparten: el rotor gira cambiando el
// frame y el indice de tiles, sin DMA. (Antes eran 3 franjas de 56x168 con 3
// frames: ~10 KB de DMA cada 3 frames y 20 sprites de hardware cada uno.)
// Posicion: x = centro, lane = y de la sombra en la ruta, alt = altura de la
// punta del arma sobre la sombra.
// ===========================================================================
#define COP_MAX          2      // 3 no entran en los 80 sprites del VDP
#define COP_FRAME_W    168      // el cuadro del arte
#define COP_BODY_X       8      // donde va cada parte dentro del cuadro
#define COP_BODY_Y      48      // (COP_BODY / COP_ROTOR del generador)
#define COP_BODY_W     112
#define COP_ROTOR_X    -16      // (13/10) el arco del rotor B sale del cuadro
#define COP_ROTOR_Y      0
#define COP_ROTOR_W    168
#define COP_ANCHOR      64      // x del "centro" en el cuadro
#define COP_MUZZLE_X    47      // la punta del arma, desde el centro
#define COP_MUZZLE_Y   161      // fila de la punta del arma en el cuadro
#define COP_ALT         30      // la punta del arma, sobre la sombra (arcade ~25-35)
#define COP_ALT_AMP      8
#define COP_Q          384      // 1,5 px/f
#define COP_LANE_Q     256
#define COP_SPAWN_GAP   45      // frames entre uno que entra y el siguiente
#define COP_HP_PINK      4
#define COP_HP_PURPLE    5
#define COP_HIT_CD      20
#define COP_HIT_DX      44
#define COP_HIT_DY      16
#define COP_HIT_JZ      24      // hay que estar saltando al menos esto
#define COP_FIRE_MIN    90
#define COP_FIRE_RND    60
#define COP_ROTOR_T      2      // frames por posicion del rotor
#define COP_CYCLE      240      // cada 4 s...
#define COP_DIVE        90      // ...1,5 s encima de la tortuga
enum { COP_OFF = 0, COP_FLY, COP_DEAD };
enum { CS_BODY = 0, CS_ROTOR, CS_N };
static struct {
    u8 st; s16 x, lane, alt; s8 dir; u8 hp, cd, slot; u16 fire, t;
    u8 accX, accY; u8 shots; Sprite* s[CS_N]; Sprite* sh; bool want[CS_N];
} cop[COP_MAX];
static Sprite* copBase[3];      // cuerpo, rotor A, rotor B: reservan la VRAM (ocultos)
static u16 copTile[3];          // el indice de tiles de cada uno
static u8  copPurple;           // los que vuelan son violetas
static u16 copLastSpawn;

static u16 copActive(void) {
    u16 n = 0;
    for (u16 i = 0; i < COP_MAX; i++) if (cop[i].st != COP_OFF) n++;
    return n;
}

static void copBaseRelease(void) {
    for (u16 k = 0; k < 3; k++) if (copBase[k]) { SPR_releaseSprite(copBase[k]); copBase[k] = NULL; }
}

static const TileSet* copTileSet(u16 k) {
    return (k == 0) ? sk_cop_body.animations[0]->frames[0]->tileset
                    : sk_cop_rotor.animations[0]->frames[k - 1]->tileset;
}

// Si SPR_defragVRAM mudo las bases (estan ocultas: SGDK no les vuelve a subir
// los tiles), se suben de nuevo a mano y los cuerpos se apuntan ahi (el rotor
// toma copTile en cada frame).
static void copFollow(void) {
    for (u16 k = 0; k < 3; k++) {
        if (!copBase[k]) return;
        u16 idx = copBase[k]->attribut & TILE_INDEX_MASK;
        if (idx == copTile[k]) continue;
        copTile[k] = idx;
        const TileSet* ts = copTileSet(k);
        if (ts->numTile) VDP_loadTileSet(ts, idx, DMA_QUEUE);
        if (k == 0)
            for (u16 i = 0; i < COP_MAX; i++)
                if (cop[i].s[CS_BODY]) SPR_setVRAMTileIndex(cop[i].s[CS_BODY], idx);
    }
}

static bool copBaseInit(bool purple) {
    static const SpriteDefinition* const def[3] = { &sk_cop_body, &sk_cop_rotor, &sk_cop_rotor };
    u16 need = (u16)(sk_cop_body.maxNumTile + 2 * sk_cop_rotor.maxNumTile);
    if (!sprVramFits((u16)(need + sk_shadow.maxNumTile))) return FALSE;
    for (u16 k = 0; k < 3; k++) {
        copBase[k] = SPR_addSpriteEx(def[k], -200, 0, TILE_ATTR(PAL3, FALSE, FALSE, FALSE),
                                     SPR_FLAG_AUTO_VRAM_ALLOC);
        if (!copBase[k]) { copBaseRelease(); return FALSE; }
        SPR_setAutoAnimation(copBase[k], FALSE);
        SPR_setVisibility(copBase[k], HIDDEN);    // oculto: no gasta SAT
        copTile[k] = copBase[k]->attribut & TILE_INDEX_MASK;
        // un sprite oculto no sube tiles: se suben a mano, una sola vez
        const TileSet* ts = copTileSet(k);
        if (ts->numTile) VDP_loadTileSet(ts, copTile[k], DMA_QUEUE);
    }
    PAL_setPalette(PAL3, purple ? sk_copter_purple.data : sk_cop_body.palette->data, DMA);
    copPurple = purple ? 1 : 0;
    return TRUE;
}

static void copRelease(u16 i) {
    for (u16 k = 0; k < CS_N; k++) if (cop[i].s[k]) { SPR_releaseSprite(cop[i].s[k]); cop[i].s[k] = NULL; }
    if (cop[i].sh) { SPR_releaseSprite(cop[i].sh); cop[i].sh = NULL; }
    cop[i].st = COP_OFF;
}

static bool copStart(bool purple) {
    if (copActive() && (bool)copPurple != purple) return FALSE;   // un color a la vez (PAL3)
    if (copActive() && (u16)(fr61 - copLastSpawn) < COP_SPAWN_GAP) return FALSE;
    u16 i;
    for (i = 0; i < COP_MAX; i++) if (cop[i].st == COP_OFF) break;
    if (i >= COP_MAX) return FALSE;
    if (!copBase[0] && !copBaseInit(purple)) return FALSE;
    if (!sprVramFits(sk_shadow.maxNumTile)) return FALSE;
    for (u16 k = 0; k < CS_N; k++) {
        cop[i].s[k] = SPR_addSpriteEx(k == CS_BODY ? &sk_cop_body : &sk_cop_rotor, -200, 0,
                                      TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE, copTile[k]), 0);
        if (!cop[i].s[k]) { copRelease(i); return FALSE; }
        SPR_setAutoAnimation(cop[i].s[k], FALSE);
        SPR_setVisibility(cop[i].s[k], HIDDEN);
        cop[i].want[k] = FALSE;
    }
    cop[i].sh = SPR_addSprite(&sk_shadow, -64, 0, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
    cop[i].st = COP_FLY; cop[i].x = SCREEN_W + 100; cop[i].alt = COP_ALT + 24;
    cop[i].lane = (s16)(LVL61_WALK_Y_MIN + 10 + (s16)(random() % 40));
    cop[i].dir = -1; cop[i].hp = purple ? COP_HP_PURPLE : COP_HP_PINK; cop[i].cd = 0;
    cop[i].slot = (u8)i; cop[i].fire = (u16)(COP_FIRE_MIN + (random() % COP_FIRE_RND));
    cop[i].t = 0; cop[i].accX = cop[i].accY = 0; cop[i].shots = 0;
    copLastSpawn = fr61;
    return TRUE;
}

// --- Balas, llamas y bombas -------------------------------------------------
#define SHOT_MAX         6
#define SHOT_VX_Q      512      // 2 px/f hacia la tortuga
#define SHOT_VZ          2      // y baja 2 px/f
#define SHOT_HIT_DX     10
#define SHOT_HIT_DY      8
#define BOMB_VZ          3
#define BOMB_R_X        30
#define BOMB_R_Y        12
#define FLAME_MAX        6
#define FLAME_T         48
#define FLAME_HIT_DX     8
#define FLAME_HIT_DY     6
static struct { u8 on, bomb; s16 x, lane, z; s8 dir; u8 acc; Sprite* s; } shot[SHOT_MAX];
static struct { u8 on; s16 x, lane; u16 t; u8 hitMask; Sprite* s; } flame[FLAME_MAX];

static void flameAt(s16 x, s16 lane) {
    for (u16 i = 0; i < FLAME_MAX; i++) {
        if (flame[i].on) continue;
        if (!sprVramFits(sk_flame.maxNumTile)) return;
        flame[i].s = SPR_addSprite(&sk_flame, -32, 0, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        if (!flame[i].s) return;
        SPR_setAutoAnimation(flame[i].s, FALSE);
        flame[i].on = 1; flame[i].x = x; flame[i].lane = lane; flame[i].t = 0; flame[i].hitMask = 0;
        return;
    }
}

static void shotFire(s16 x, s16 lane, s16 z, s8 dir, bool bomb) {
    for (u16 i = 0; i < SHOT_MAX; i++) {
        if (shot[i].on) continue;
        if (!sprVramFits(sk_bullet.maxNumTile)) return;
        shot[i].s = SPR_addSprite(&sk_bullet, -32, 0, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        if (!shot[i].s) return;
        SPR_setHFlip(shot[i].s, dir > 0);
        shot[i].on = 1; shot[i].bomb = bomb ? 1 : 0; shot[i].x = x; shot[i].lane = lane;
        shot[i].z = z; shot[i].dir = dir; shot[i].acc = 0;
        return;
    }
}

static void shotsUpdate(Player** pls, u8 nPl) {
    for (u16 i = 0; i < SHOT_MAX; i++) {
        if (!shot[i].on) continue;
        bool gone = FALSE;
        if (shot[i].bomb) {
            shot[i].z = (s16)(shot[i].z - BOMB_VZ);
            if (shot[i].z <= 0) {
                boomAt(shot[i].x, shot[i].lane);
                for (u8 k = 0; k < nPl; k++) {
                    if (!playerCanBeHit(pls[k])) continue;
                    if (a16((s16)(getPlayerHurtCX(pls[k]) - shot[i].x)) > BOMB_R_X) continue;
                    if (a16((s16)(getPlayerY(pls[k]) - shot[i].lane)) > BOMB_R_Y) continue;
                    playerHitBarsKnockdown(pls[k], shot[i].x, 2);
                    sfxHit();
                }
                gone = TRUE;
            }
        } else {
            u16 t = (u16)(shot[i].acc + SHOT_VX_Q);
            shot[i].acc = (u8)(t & 0xFF);
            shot[i].x = (s16)(shot[i].x + shot[i].dir * (s16)(t >> 8));
            shot[i].z = (s16)(shot[i].z - SHOT_VZ);
            // Pega al que esta a su altura en el carril.
            for (u8 k = 0; k < nPl && !gone; k++) {
                if (!playerCanBeHitAir(pls[k])) continue;
                if (a16((s16)(getPlayerY(pls[k]) - shot[i].lane)) > SHOT_HIT_DY) continue;
                if (a16((s16)(getPlayerHurtCX(pls[k]) - shot[i].x)) > SHOT_HIT_DX) continue;
                s16 pz = getPlayerJumpZ(pls[k]);
                if (shot[i].z > pz + 48 || shot[i].z < pz - 8) continue;
                playerHitProjectile(pls[k], shot[i].x, 1);
                sfxHit();
                gone = TRUE;
            }
            if (!gone && shot[i].z <= 0) { flameAt(shot[i].x, shot[i].lane); gone = TRUE; }
        }
        if (gone || shot[i].x < -16 || shot[i].x > SCREEN_W + 16) {
            SPR_releaseSprite(shot[i].s); shot[i].s = NULL; shot[i].on = 0;
            continue;
        }
        SPR_setPosition(shot[i].s, (s16)(shot[i].x - 8), (s16)(shot[i].lane - shot[i].z - 8 - stageCamY));
        SPR_setDepth(shot[i].s, (s16)(-shot[i].lane - 1));
    }
}

static void flamesUpdate(Player** pls, u8 nPl) {
    for (u16 i = 0; i < FLAME_MAX; i++) {
        if (!flame[i].on) continue;
        if (++flame[i].t >= FLAME_T) {
            SPR_releaseSprite(flame[i].s); flame[i].s = NULL; flame[i].on = 0;
            continue;
        }
        for (u8 k = 0; k < nPl; k++) {
            if (flame[i].hitMask & (1 << k)) continue;
            if (!playerCanBeHit(pls[k])) continue;
            if (a16((s16)(getPlayerY(pls[k]) - flame[i].lane)) > FLAME_HIT_DY) continue;
            if (a16((s16)(getPlayerHurtCX(pls[k]) - flame[i].x)) > FLAME_HIT_DX) continue;
            playerHitBars(pls[k], flame[i].x, 1);
            sfxHit();
            flame[i].hitMask |= (u8)(1 << k);
        }
        SPR_setFrame(flame[i].s, (flame[i].t >> 2) & 1);
        SPR_setPosition(flame[i].s, (s16)(flame[i].x - 8), (s16)(flame[i].lane - 24 - stageCamY));
        SPR_setDepth(flame[i].s, (s16)-flame[i].lane);
    }
}

// Presupuesto del SAT. SPR_update de SGDK solo controla el limite de 80
// sprites del VDP antes de cada Sprite, no antes de cada sprite de hardware:
// si un Sprite de muchos sprites de hardware arranca cerca del final escribe
// fuera de vdpSpriteCache y pisa la RAM (cuelgue). Aca se cuenta lo que piden
// los demas Sprites visibles (por su maximo, el cuadro puede cambiar en este
// mismo frame) y los helicopteros usan lo que sobra; si no entran todos se
// turnan cuadro a cuadro (parpadeo, como el arcade cuando se llena).
#define SAT_BUDGET      78
static u16 copSatOthers(void) {
    u16 n = 0;
    for (Sprite* sp = firstSprite; sp; sp = sp->next) {
        if (!sp->visibility) continue;
        bool isCop = FALSE;
        for (u16 i = 0; i < COP_MAX && !isCop; i++)
            for (u16 k = 0; k < CS_N; k++) if (cop[i].s[k] == sp) { isCop = TRUE; break; }
        if (!isCop) n = (u16)(n + sp->definition->maxNumSprite);
    }
    return n;
}

static void copBudget(u8 rotor) {
    u16 used = copSatOthers();
    s16 left = (s16)(SAT_BUDGET - (s16)used);
    u16 first = (u16)(fr61 & 1);                  // a quien le toca primero
    for (u16 j = 0; j < COP_MAX; j++) {
        u16 i = (u16)((j + first) % COP_MAX);
        if (cop[i].st != COP_FLY) continue;
        u16 need = 0;
        for (u16 k = 0; k < CS_N; k++) if (cop[i].want[k]) {
            const SpriteDefinition* d = (k == CS_BODY) ? &sk_cop_body : &sk_cop_rotor;
            s8 ns = d->animations[0]->frames[k == CS_BODY ? 0 : rotor]->numSprite;
            need = (u16)(need + (ns < 0 ? 1 : ns));
        }
        bool show = (bool)((s16)need <= left);
        if (show) left = (s16)(left - (s16)need);
        for (u16 k = 0; k < CS_N; k++)
            SPR_setVisibility(cop[i].s[k], (show && cop[i].want[k]) ? VISIBLE : HIDDEN);
    }
}

static void copUpdate(Player** pls, u8 nPl) {
    u16 n = copActive();
    if (!n) { copBaseRelease(); return; }
    u8 rotor = (u8)((fr61 / COP_ROTOR_T) & 1);
    copFollow();
    for (u16 i = 0; i < COP_MAX; i++) {
        if (cop[i].st == COP_OFF) continue;
        cop[i].t++;
        // La tortuga mas cercana.
        Player* tg = NULL; s16 best = 0x7FFF;
        for (u8 k = 0; k < nPl; k++) {
            if (isPlayerGameOver(pls[k])) continue;
            s16 d = a16((s16)(getPlayerHurtCX(pls[k]) - cop[i].x));
            if (d < best) { best = d; tg = pls[k]; }
        }
        if (cop[i].st == COP_DEAD) { copRelease(i); continue; }
        // Golpes (solo saltando).
        if (cop[i].cd) cop[i].cd--;
        else for (u8 k = 0; k < nPl; k++) {
            Player* p = pls[k];
            if (getPlayerJumpZ(p) < COP_HIT_JZ) continue;
            if (a16((s16)(getPlayerY(p) - cop[i].lane)) > COP_HIT_DY) continue;
            if (!isPlayerJumpKicking(p) && !playerAttackHitsBox(p, cop[i].x, cop[i].lane, COP_HIT_DX, 0))
                continue;
            if (a16((s16)(getPlayerHurtCX(p) - cop[i].x)) > COP_HIT_DX + 20) continue;
            XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 15, FALSE, FALSE);
            cop[i].cd = COP_HIT_CD;
            if (--cop[i].hp == 0) {
                boomAt(cop[i].x, (s16)(cop[i].lane - cop[i].alt + 40));
                addPlayerScore(p, 1);
                cop[i].st = COP_DEAD;
            }
            break;
        }
        if (cop[i].st == COP_DEAD) continue;
        // Revolotea adelante de la tortuga, cada uno en su lugar.
        if (tg) {
            s16 px = getPlayerHurtCX(tg), py = getPlayerY(tg);
            // Uno de cada lado (como el arcade) y cada tanto pasan rasantes
            // sobre la tortuga: ahi es cuando se los puede patear.
            s16 side = (px < SCREEN_W / 2) ? 1 : -1;
            if (cop[i].slot & 1) side = (s16)-side;
            bool dive = (bool)(((cop[i].t + cop[i].slot * 120) % COP_CYCLE) >= COP_CYCLE - COP_DIVE);
            s16 tx = (s16)(px + side * (dive ? 28 : 100));
            if (tx < 40) tx = 40;
            if (tx > SCREEN_W - 30) tx = SCREEN_W - 30;
            s16 ty = (s16)(py + 4 + cop[i].slot * 10);
            if (ty < LVL61_WALK_Y_MIN) ty = LVL61_WALK_Y_MIN;
            if (ty > LVL61_WALK_Y_MAX) ty = LVL61_WALK_Y_MAX;
            u16 t = (u16)(cop[i].accX + COP_Q);
            cop[i].accX = (u8)(t & 0xFF);
            s16 v = (s16)(t >> 8);
            if (cop[i].x < tx) cop[i].x = (s16)((tx - cop[i].x < v) ? tx : cop[i].x + v);
            else if (cop[i].x > tx) cop[i].x = (s16)((cop[i].x - tx < v) ? tx : cop[i].x - v);
            t = (u16)(cop[i].accY + COP_LANE_Q);
            cop[i].accY = (u8)(t & 0xFF);
            v = (s16)(t >> 8);
            if (cop[i].lane < ty) cop[i].lane = (s16)(cop[i].lane + v);
            else if (cop[i].lane > ty) cop[i].lane = (s16)(cop[i].lane - v);
            cop[i].dir = (px < cop[i].x) ? -1 : 1;
            // Dispara (los rosas: rafaga de 3 balas; los violetas: una bomba).
            if (cop[i].fire) cop[i].fire--;
            else if (a16((s16)(px - cop[i].x)) < 220 && cop[i].x < SCREEN_W - 8) {
                if (copPurple) {
                    shotFire(cop[i].x, cop[i].lane, (s16)(cop[i].alt + 20), cop[i].dir, TRUE);
                    cop[i].fire = (u16)(COP_FIRE_MIN + 30 + (random() % COP_FIRE_RND));
                } else {
                    shotFire((s16)(cop[i].x + cop[i].dir * COP_MUZZLE_X), cop[i].lane,
                             cop[i].alt, cop[i].dir, FALSE);
                    if (++cop[i].shots >= 3) {
                        cop[i].shots = 0;
                        cop[i].fire = (u16)(COP_FIRE_MIN + (random() % COP_FIRE_RND));
                    } else cop[i].fire = 8;
                }
            }
        }
        // Sube y baja.
        s16 ph = (s16)(cop[i].t & 63);
        s16 tri = (s16)((ph < 32) ? ph : 64 - ph) - 16;
        s16 altT = (s16)(COP_ALT + (tri * COP_ALT_AMP) / 16);
        if (cop[i].alt > altT) cop[i].alt--; else if (cop[i].alt < altT) cop[i].alt++;
        // Dibujo: el arte mira a la derecha (el arma apunta abajo a la
        // derecha); mirando a la izquierda se espeja el cuadro entero.
        bool flip = (bool)(cop[i].dir < 0);
        s16 left = flip ? (s16)(cop[i].x - (COP_FRAME_W - 1 - COP_ANCHOR)) : (s16)(cop[i].x - COP_ANCHOR);
        s16 top  = (s16)(cop[i].lane - cop[i].alt - COP_MUZZLE_Y - stageCamY);
        bool blink = (bool)(cop[i].cd && (cop[i].cd & 2));
        for (u16 k = 0; k < CS_N; k++) {
            Sprite* s = cop[i].s[k];
            s16 ox = (k == CS_BODY) ? COP_BODY_X : COP_ROTOR_X;
            s16 ow = (k == CS_BODY) ? COP_BODY_W : COP_ROTOR_W;
            s16 sx = (s16)(left + (flip ? COP_FRAME_W - ox - ow : ox));
            s16 sy = (s16)(top + ((k == CS_BODY) ? COP_BODY_Y : COP_ROTOR_Y));
            SPR_setHFlip(s, flip);
            if (k == CS_ROTOR) {                    // gira: otro frame y sus tiles
                SPR_setFrame(s, rotor);
                SPR_setVRAMTileIndex(s, copTile[1 + rotor]);
            }
            SPR_setPosition(s, sx, sy);
            // fuera de pantalla no gasta entradas del SAT
            bool off = (bool)(sx >= SCREEN_W || sx + ow <= 0 || sy >= 224);
            cop[i].want[k] = (bool)!(blink || off);     // copBudget decide
            SPR_setDepth(s, (s16)(-cop[i].lane - 40));
        }
        if (cop[i].sh) {
            SPR_setPosition(cop[i].sh, (s16)(cop[i].x - 24), (s16)(cop[i].lane - 12 - stageCamY));
            SPR_setHFlip(cop[i].sh, flip);
            SPR_setDepth(cop[i].sh, SPR_MAX_DEPTH);
        }
    }
}

// ===========================================================================
// Ganchos del nivel
// ===========================================================================
#define SP_WAIT          (STAGE_SPECIAL + 0)
#define SP_COP_PINK      (STAGE_SPECIAL + 1)
#define SP_COP_PURPLE    (STAGE_SPECIAL + 2)

static u16 shotsActive(void) {
    u16 n = 0;
    for (u16 i = 0; i < SHOT_MAX; i++) if (shot[i].on) n++;
    for (u16 i = 0; i < FLAME_MAX; i++) if (flame[i].on) n++;
    return n;
}

static bool lvl61Special(u8 type, s8 side, s16 x, s16 camX) {
    (void)side; (void)x; (void)camX;
    switch (type) {
    case SP_WAIT:       return (bool)(stageAliveEnemies() == 0 && copActive() == 0 && shotsActive() == 0);
    case SP_COP_PINK:   return copStart(FALSE);
    case SP_COP_PURPLE: return copStart(TRUE);
    default:            return TRUE;
    }
}

static u16 lvl61SpecialAlive(void) { return copActive(); }

static void lvl61Init(void) {
    fr61 = 0;
    for (u16 i = 0; i < COP_MAX; i++) { cop[i].st = COP_OFF; for (u16 k = 0; k < CS_N; k++) cop[i].s[k] = NULL; cop[i].sh = NULL; }
    for (u16 k = 0; k < 3; k++) copBase[k] = NULL;
    copPurple = 0; copLastSpawn = 0;
    for (u16 i = 0; i < SHOT_MAX; i++) { shot[i].on = 0; shot[i].s = NULL; }
    for (u16 i = 0; i < FLAME_MAX; i++) { flame[i].on = 0; flame[i].s = NULL; }
    for (u16 i = 0; i < BOOM_MAX; i++) boom[i].s = NULL;
    for (u16 k = 0; k < MAX_PLAYERS; k++) boardPl[k] = NULL;
    for (u16 i = 0; i < MAX_ENEMIES; i++) boardEn[i] = NULL;
    // Las bases de las tablas: viven todo el nivel.
    boardBaseT = boardBase(&sk_board_turtle);
    boardBaseF = boardBase(&sk_board_foot);
}

static void lvl61Release(void) {
    for (u16 i = 0; i < COP_MAX; i++) if (cop[i].st != COP_OFF || cop[i].s[0]) copRelease(i);
    copBaseRelease();
    for (u16 i = 0; i < SHOT_MAX; i++) if (shot[i].s) { SPR_releaseSprite(shot[i].s); shot[i].s = NULL; shot[i].on = 0; }
    for (u16 i = 0; i < FLAME_MAX; i++) if (flame[i].s) { SPR_releaseSprite(flame[i].s); flame[i].s = NULL; flame[i].on = 0; }
    for (u16 i = 0; i < BOOM_MAX; i++) if (boom[i].s) { SPR_releaseSprite(boom[i].s); boom[i].s = NULL; }
    for (u16 k = 0; k < MAX_PLAYERS; k++) if (boardPl[k]) { SPR_releaseSprite(boardPl[k]); boardPl[k] = NULL; }
    for (u16 i = 0; i < MAX_ENEMIES; i++) if (boardEn[i]) { SPR_releaseSprite(boardEn[i]); boardEn[i] = NULL; }
    if (boardBaseT) { SPR_releaseSprite(boardBaseT); boardBaseT = NULL; }
    if (boardBaseF) { SPR_releaseSprite(boardBaseF); boardBaseF = NULL; }
}

static void lvl61Update(Player** pls, u8 nPl, s16 camX) {
    (void)camX;
    fr61++;
    copUpdate(pls, nPl);
    shotsUpdate(pls, nPl);
    flamesUpdate(pls, nPl);
    boomUpdate();
    boardsUpdate(pls, nPl);
    if (copActive()) copBudget((u8)((fr61 / COP_ROTOR_T) & 1));
}

// ===========================================================================
// OLEADAS (12/10): por AVANCE del fondo (5 px/f). Cuadros del tramo del video
// (desde que arranca, cuadro 600) x 5:
//   650  helicopteros rosas        1600 blancos en patineta
//   2400 helicopteros rosas        3000 los del martillo (con su "arma de
//   3560 violetas + martillos           aros" en el arcade)
//   4500 martillos                 5450 violetas + martillos   6280 fin
// ===========================================================================
#define W   ENEMY_TYPE_FOOT_SOLDIER_WHITE
#define H   ENEMY_TYPE_FOOT_SOLDIER_HAMMER
#define WT  SP_WAIT
#define CP  SP_COP_PINK
#define CV  SP_COP_PURPLE
static const StageWave waves61[] = {
    {   250, 0, 3, { CP, CP, CP },                 { 0 }, { 0 } },
    {  5000, 0, 6, { WT, W, W, W, W, W },          { 0, +1, -1, +1, +1, -1 }, { 0 } },
    {  9000, 0, 4, { WT, CP, CP, CP },             { 0 }, { 0 } },
    { 12000, 0, 6, { WT, H, H, H, H, H },          { 0, +1, -1, +1, -1, +1 }, { 0 } },
    { 14800, 0, 6, { WT, CV, CV, CV, H, H },       { 0, 0, 0, 0, +1, -1 }, { 0 } },
    { 19500, 0, 7, { WT, H, H, H, H, H, H },       { 0, +1, +1, -1, +1, -1, +1 }, { 0 } },
    { 24250, 0, 6, { WT, CV, CV, CV, H, H },       { 0, 0, 0, 0, +1, -1 }, { 0 } },
};
#undef W
#undef H
#undef WT
#undef CP
#undef CV

static const StageLevel level61 = {
    .bg           = &bg_skate,
    .fg           = NULL,
    .far          = &bg_skate_far,
    .farDiv       = 3,
    .farRowShift  = 2,              // el skyline asoma justo arriba del guardarrail
    .farPal0      = TRUE,           // (12/10) PAL3 queda para el helicoptero
    .pal3         = NULL,
    .backdrop     = 9,              // PAL0[9]: el celeste del cielo (gen_level6_skate.py)
    .bgSlots      = 448,            // >= 415, el peor caso medido
    .levelW       = SCREEN_W,       // (12/10) la camara no se mueve: corre el fondo
    .walkTop      = NULL,
    .walkCols     = 0,
    .walkYMin     = LVL61_WALK_Y_MIN,
    .walkYMax     = LVL61_WALK_Y_MAX,
    .waves        = waves61,
    .nWaves       = sizeof(waves61) / sizeof(waves61[0]),
    .bossFeetX    = 28400,          // avance al que termina (cuadro 6280 x 5)
    .music        = music_skate,
    .musicVol     = 80,
    .autoScrollQ  = LVL61_SCROLL_Q,
    .autoScrollVisQ = LVL61_SCROLL_VIS_Q,
    .skateLift    = LVL61_SKATE_LIFT,
    .levelInit    = lvl61Init,
    .levelUpdate  = lvl61Update,
    .levelRelease = lvl61Release,
    .specialSpawn = lvl61Special,
    .specialAlive = lvl61SpecialAlive,
    .nextScene    = SCENE_7_1_TITLE,
};

SceneId showScene61() {
    return stageLevelRun(&level61);
}
