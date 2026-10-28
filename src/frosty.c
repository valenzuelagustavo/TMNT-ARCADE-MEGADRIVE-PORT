// ===========================================================================
// frosty.c - FROSTY, el muneco de nieve robot de CENTRAL PARK (27/10)
// ===========================================================================
// Ver frosty.h.
#include <genesis.h>
#include "frosty.h"
#include "frosty_art.h"       // medidas de las hojas (generado)
#include "level_snow_far.h"   // frosty_* (rescomp)
#include "audio.h"            // baxter_hit_sfx, foot_soldier_explode
#include "enemy.h"            // sprVramFits, sprDefragLock
#include "stage_level.h"      // stageWalkTopAt, stageAliveEnemies

#define SCREEN_W        320

#define FR_MAX            3
#define FR_HP_SNOW        2          // golpes del muneco
#define FR_HP_BOT         2          // golpes del robot
#define FR_HIT_CD        14          // un golpe por swing
#define FR_SCORE          1          // puntos al destruirlo

// Salto (cuadros a 60 Hz)
#define FR_HOP_T         22
#define FR_HOP_DX        20          // tope de lo que avanza por salto
#define FR_HOP_DL          6          // y de profundidad
#define FR_HOP_ARC         3          // px extra de arco (el arte ya trae el vuelo)
#define FR_HOVER         13          // altura de los cuadros "saltando" (golpes)
#define FR_WAIT_MIN      12
#define FR_WAIT_RND      22
#define FR_STAND_MIN     21          // a que distancia de la tortuga se detiene (X)
#define FR_STAND_RND     30

// Golpes y rotura
#define FR_HURT_T        14
#define FR_KB_V           3          // retroceso (px por cuadro, baja de a 1)
#define FR_BREAK_STEP    10          // cuadros por cuadro de la rotura
#define FR_DEAD_T        26          // tirado, antes de explotar
#define FR_DEAD_BLINK     8

// Hitbox (cuerpo)
#define FR_HALF_W_SNOW   12
#define FR_HALF_W_BOT     8
#define FR_BODY_H        39

// Nariz
#define FR_NOSE_CD_MIN  100          // espera entre misiles (cuadros)
#define FR_NOSE_CD_RND   90
#define FR_NOSE_FIRST    80          // la primera vez que es robot
#define FR_NOSE_GONE     70          // sin nariz
#define FR_NOSE_REGEN    26          // regenerandola
#define FR_NOSE_Z        32          // altura de la nariz (sale de ahi)
#define FR_NOSE_DX       11          // y que tan adelante

// Misil: SIEMPRE avanza hacia la tortuga (velocidad fija) y va dibujando arcos
// en el aire (saltos parabolicos de FR_MIS_PH_SPD en FR_MIS_PH_SPD en la altura);
// los arcos se achican a medida que se acerca, hasta quedar a la altura del cuerpo.
#define FR_MIS_MAX        3
#define FR_MIS_SPD_Q4    40          // 2,5 px por cuadro
#define FR_MIS_PH_SPD    16          // fase del arco por cuadro (512 = un arco): ~32 cuadros, ~80 px
#define FR_MIS_PH0      160          // fase al salir (ya arriba, a la altura de la nariz)
#define FR_MIS_ZMIN        5          // piso de los arcos
#define FR_MIS_AMP_MIN   11
#define FR_MIS_AMP_MAX   33
#define FR_MIS_AMP_DIV    3          // amplitud = distancia / esto
#define FR_MIS_LIFE     420
#define FR_MIS_HIT_DX     8
#define FR_MIS_HIT_DL     8
#define FR_MIS_HIT_Z     30
#define FR_MIS_DMG        2          // barras

// Explosion (la de Baxter, con la paleta de Frosty)
#define FR_BOOM_MAX       2
#define FR_BOOM_FRAMES    8
#define FR_BOOM_TICKS     5

// Entrada
#define FR_ENTER_GAP     45          // cuadros entre uno y el siguiente
#define FR_STARVE_MAX   240          // sin VRAM y sin nadie: se saltea

#define FR_SHADOW_W      FA_SH_W
#define FR_SHADOW_H      FA_SH_H

enum { D_SNOW, D_BREAK, D_BOT, D_NONE = 0xFF };
// cuadros
enum { SF_GROUND = 0, SF_JUMP = 1 };
enum { BF_GROUND = 0, BF_JUMP_A = 1, BF_JUMP_B = 2, BF_HIT = 3, BF_DEAD = 4,
       BF_NONOSE = 5, BF_REGEN = 6 };

typedef enum { FS_OFF, FS_IDLE, FS_HOP, FS_HURT, FS_BREAK, FS_NOSE, FS_DEAD } FState;

typedef struct {
    u8      st;
    u8      robot;        // 0 muneco, 1 robot
    u8      def;          // D_*
    s8      face;         // +1 derecha, -1 izquierda
    u8      hp;
    u8      hitCd;
    u8      entered;      // ya estuvo dentro de la pantalla
    u8      killer;
    s16     x, lane;      // mundo (centro, lane = centro de la sombra)
    s16     x0, l0, dx, dl;   // salto
    u16     t;
    u16     wait;
    u16     noseCd;
    s8      kb;
    s8      kbSign;
    Sprite* s;
    Sprite* sh;
    u8      vis;
} Frosty;

typedef struct {
    u8      on;
    s32     xq, zq, lq;   // Q4
    u16     ph;           // fase del arco (0..511 = un arco)
    u16     t;
    s8      face;
    u8      target;       // indice de tortuga
    Sprite* s;
    Sprite* sh;
    u8      vis;
} Missile;

typedef struct {
    u8      on;
    Sprite* part[4];
    u8      anim[4];
    s16     x, y;         // centro (mundo)
    s16     lane;
    u8      f, tk;
} Boom;

static Frosty  fr[FR_MAX];
static Missile mis[FR_MIS_MAX];
static Boom    booms[FR_BOOM_MAX];

static s16  frCamX;
static s16  frLaneMax;
static u8   pendN;                 // por entrar
static u8   pendIdx;               // cual sigue (0..2)
static u16  pendWait;
static u16  starveT;
static bool waveOn;

static const s8 pendSide[3] = { +1, +1, -1 };

// ---------------------------------------------------------------------------
static s16 iabs(s16 v) { return (s16)(v < 0 ? -v : v); }
static s16 iclamp(s16 v, s16 lo, s16 hi) { return (s16)(v < lo ? lo : (v > hi ? hi : v)); }

static const SpriteDefinition* defOf(u8 d) {
    return d == D_SNOW ? &frosty_snow : (d == D_BREAK ? &frosty_break : &frosty_bot);
}
static s16 defW(u8 d)    { return d == D_SNOW ? FA_SNOW_W : (d == D_BREAK ? FA_BREAK_W : FA_BOT_W); }
static s16 defFeet(u8 d) { return d == D_SNOW ? FA_SNOW_FEET : (d == D_BREAK ? FA_BREAK_FEET : FA_BOT_FEET); }

static bool playerAlive(const Player* p) { return !isPlayerGameOver(p); }

// Tortuga viva mas cercana en X (NULL si no queda ninguna).
static s8 nearestPlayer(Player** pls, u8 nPl, s16 x) {
    s8 best = -1; s16 bd = 32000;
    for (u8 k = 0; k < nPl; k++) {
        if (!playerAlive(pls[k])) continue;
        s16 d = iabs((s16)(getPlayerHurtCX(pls[k]) - x));
        if (d < bd) { bd = d; best = (s8)k; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Explosion de Baxter (baxter_boom) con la paleta de Frosty: 4 cuartos de
// 64x64; si no entra la VRAM, dos duenos + espejos, o uno + 3 espejos (los
// espejos apuntan a la VRAM de su dueno: mientras haya explosiones no se
// desfragmenta). Ver traag.c / baxter.c.
// ---------------------------------------------------------------------------
static Sprite* boomMaster(u16 anim) {
    Sprite* s = SPR_addSpriteEx(&frosty_boom, -128, -128,
                                TILE_ATTR(PAL3, FALSE, FALSE, FALSE),
                                SPR_FLAG_AUTO_VISIBILITY | SPR_FLAG_AUTO_VRAM_ALLOC |
                                SPR_FLAG_AUTO_TILE_UPLOAD |
                                SPR_FLAG_DISABLE_DELAYED_FRAME_UPDATE);
    if (s) {
        SPR_setAutoAnimation(s, FALSE);
        SPR_setAnimAndFrame(s, (s16)anim, 0);
    }
    return s;
}

static Sprite* boomMirror(const Sprite* m, u16 anim, bool hflip, bool vflip) {
    if (!m) return NULL;
    u16 idx = m->attribut & TILE_INDEX_MASK;
    Sprite* s = SPR_addSpriteEx(&frosty_boom, -128, -128,
                                TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE, idx),
                                SPR_FLAG_AUTO_VISIBILITY);
    if (s) {
        SPR_setAutoAnimation(s, FALSE);
        SPR_setAnimAndFrame(s, (s16)anim, 0);
        SPR_setHFlip(s, hflip);
        SPR_setVFlip(s, vflip);
    }
    return s;
}

static void boomsLock(void) {
    bool any = FALSE;
    for (u8 i = 0; i < FR_BOOM_MAX; i++) if (booms[i].on) any = TRUE;
    sprDefragLock = any ? 1 : 0;
}

static void boomFree(Boom* b) {
    for (u8 q = 0; q < 4; q++)
        if (b->part[q]) { SPR_releaseSprite(b->part[q]); b->part[q] = NULL; }
    b->on = FALSE;
    boomsLock();
}

static void boomCreate(Boom* b) {
    Sprite** sp = b->part;
    sprDefragLock = 1;
    const u16 T = frosty_boom.maxNumTile;
    if (SPR_getFreeVRAM() >= 4 * T) {
        for (u8 q = 0; q < 4; q++) { b->anim[q] = q; sp[q] = boomMaster(q); }
        if (sp[0] && sp[1] && sp[2] && sp[3]) return;
        for (u8 q = 0; q < 4; q++) if (sp[q]) { SPR_releaseSprite(sp[q]); sp[q] = NULL; }
    }
    if (SPR_getFreeVRAM() >= 2 * T) {
        sp[0] = boomMaster(0);
        sp[2] = boomMaster(2);
        if (sp[0] && sp[2]) {
            sp[1] = boomMirror(sp[0], 0, TRUE, FALSE);
            sp[3] = boomMirror(sp[2], 2, TRUE, FALSE);
            b->anim[0] = b->anim[1] = 0;
            b->anim[2] = b->anim[3] = 2;
            return;
        }
        for (u8 q = 0; q < 4; q++) if (sp[q]) { SPR_releaseSprite(sp[q]); sp[q] = NULL; }
    }
    sp[0] = boomMaster(0);
    if (!sp[0]) return;                       // sin VRAM: solo el sonido
    sp[1] = boomMirror(sp[0], 0, TRUE,  FALSE);
    sp[2] = boomMirror(sp[0], 0, FALSE, TRUE);
    sp[3] = boomMirror(sp[0], 0, TRUE,  TRUE);
    for (u8 q = 0; q < 4; q++) b->anim[q] = 0;
}

// (x, y): centro de la explosion en el mundo; lane: para el orden de dibujo.
static void boomAt(s16 x, s16 y, s16 lane) {
    u8 i;
    for (i = 0; i < FR_BOOM_MAX; i++) if (!booms[i].on) break;
    if (i >= FR_BOOM_MAX) {                   // las dos ocupadas: la mas vieja se va
        i = (booms[0].f >= booms[1].f) ? 0 : 1;
        boomFree(&booms[i]);
    }
    Boom* b = &booms[i];
    b->on = TRUE; b->x = x; b->y = y; b->lane = lane; b->f = 0; b->tk = 0;
    boomCreate(b);
    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                   SOUND_PCM_CH3, 15, FALSE, FALSE);
}

static void boomsUpdate(void) {
    for (u8 i = 0; i < FR_BOOM_MAX; i++) {
        Boom* b = &booms[i];
        if (!b->on) continue;
        if (++b->tk >= FR_BOOM_TICKS) {
            b->tk = 0;
            if (++b->f >= FR_BOOM_FRAMES) { boomFree(b); continue; }
            for (u8 q = 0; q < 4; q++)
                if (b->part[q]) SPR_setAnimAndFrame(b->part[q], b->anim[q], b->f);
        }
        s16 bx = (s16)(b->x - frCamX - 64), by = (s16)(b->y - stageCamY - 64);
        for (u8 q = 0; q < 4; q++) {
            if (!b->part[q]) continue;
            SPR_setPosition(b->part[q], (s16)(bx + (q & 1) * 64), (s16)(by + (q >> 1) * 64));
            SPR_setDepth(b->part[q], (s16)(-b->lane - 30));
        }
    }
}

// ---------------------------------------------------------------------------
// Sprite de cada Frosty
// ---------------------------------------------------------------------------
static void frFreeSprites(Frosty* f) {
    if (f->s)  { SPR_releaseSprite(f->s);  f->s  = NULL; }
    if (f->sh) { SPR_releaseSprite(f->sh); f->sh = NULL; }
}

static Sprite* newMain(u8 d) {
    Sprite* s = SPR_addSprite(defOf(d), -128, -128, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    if (s) {
        SPR_setAutoAnimation(s, FALSE);
        SPR_setFrame(s, 0);
        SPR_setVisibility(s, HIDDEN);
    }
    return s;
}

// Cambia la hoja del Frosty (suelta la anterior antes: hay que ahorrar VRAM).
static bool frSetDef(Frosty* f, u8 d) {
    u8 old = f->def;
    if (f->s) { SPR_releaseSprite(f->s); f->s = NULL; }
    f->s = newMain(d);
    f->def = d;
    f->vis = FALSE;
    if (!f->s && old != D_NONE) {            // no entro: vuelve a la anterior
        f->s = newMain(old);
        f->def = old;
    }
    return f->s != NULL && f->def == d;
}

static void frKill(Frosty* f) {
    frFreeSprites(f);
    f->st = FS_OFF;
}

static void frSnd(void) {
    XGM2_playPCMEx(baxter_hit_sfx, sizeof(baxter_hit_sfx), SOUND_PCM_CH3, 15, FALSE, FALSE);
}

// ---------------------------------------------------------------------------
// Misil
// ---------------------------------------------------------------------------
static void misFree(Missile* m) {
    if (m->s)  { SPR_releaseSprite(m->s);  m->s  = NULL; }
    if (m->sh) { SPR_releaseSprite(m->sh); m->sh = NULL; }
    m->on = FALSE;
}

static void misLaunch(Frosty* f, u8 target) {
    u8 i;
    for (i = 0; i < FR_MIS_MAX; i++) if (!mis[i].on) break;
    if (i >= FR_MIS_MAX) return;
    if (!sprVramFits(frosty_missile.maxNumTile)) return;
    Missile* m = &mis[i];
    m->s  = SPR_addSprite(&frosty_missile, -80, -80, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    m->sh = NULL;                      // (27/10) el misil no lleva sombra
    if (!m->s) { misFree(m); return; }
    SPR_setAutoAnimation(m->s, FALSE);
    SPR_setVisibility(m->s, HIDDEN);
    m->on = TRUE;
    m->vis = FALSE;
    m->face = f->face;
    m->xq = (s32)(f->x + f->face * FR_NOSE_DX) << 4;
    m->zq = (s32)FR_NOSE_Z << 4;
    m->lq = (s32)f->lane << 4;
    m->ph = FR_MIS_PH0;
    m->t = 0;
    m->target = target;
}

static void misUpdate(Player** pls, u8 nPl) {
    for (u8 i = 0; i < FR_MIS_MAX; i++) {
        Missile* m = &mis[i];
        if (!m->on) continue;
        s16 x = (s16)(m->xq >> 4), lane = (s16)(m->lq >> 4);
        m->t++;
        // el blanco: la tortuga elegida (si cayo, la mas cercana)
        if (m->target >= nPl || !playerAlive(pls[m->target])) {
            s8 n = nearestPlayer(pls, nPl, x);
            if (n < 0) { misFree(m); continue; }
            m->target = (u8)n;
        }
        Player* tp = pls[m->target];
        s16 tx = getPlayerHurtCX(tp), tl = getPlayerY(tp);
        // avanza siempre hacia ella
        s16 dx = (s16)(tx - x);
        if (dx > 4) m->face = 1; else if (dx < -4) m->face = -1;
        m->xq += m->face * FR_MIS_SPD_Q4;
        // altura: arcos (|seno|) que se achican con la distancia
        s16 amp = (s16)(iabs(dx) / FR_MIS_AMP_DIV);
        if (amp < FR_MIS_AMP_MIN) amp = FR_MIS_AMP_MIN;
        if (amp > FR_MIS_AMP_MAX) amp = FR_MIS_AMP_MAX;
        m->ph = (u16)((m->ph + FR_MIS_PH_SPD) & 511);
        s16 z = (s16)(FR_MIS_ZMIN + (((s32)amp * sinFix16((s16)m->ph)) >> 10));
        m->zq = (s32)z << 4;
        // profundidad: se acomoda de a poco a la de la tortuga
        s32 dl = ((s32)tl << 4) - m->lq;
        if (dl > 20) dl = 20; else if (dl < -20) dl = -20;
        m->lq += dl;
        x = (s16)(m->xq >> 4); lane = (s16)(m->lq >> 4);

        // le da a alguna tortuga?
        bool done = FALSE;
        for (u8 k = 0; k < nPl && !done; k++) {
            if (!playerCanBeHit(pls[k])) continue;
            if (iabs((s16)(getPlayerHurtCX(pls[k]) - x)) > FR_MIS_HIT_DX) continue;
            if (iabs((s16)(getPlayerY(pls[k]) - lane)) > FR_MIS_HIT_DL) continue;
            if (z > FR_MIS_HIT_Z) continue;
            playerHitBarsKnockdown(pls[k], x, FR_MIS_DMG);
            done = TRUE;
        }
        if (!done && m->t >= FR_MIS_LIFE) done = TRUE;
        if (done) {
            boomAt(x, (s16)(lane - z), lane);
            misFree(m);
            continue;
        }

        // dibujo
        s16 sx = (s16)(x - (m->face > 0 ? FA_MIS_CX : FA_MIS_W - FA_MIS_CX) - frCamX);
        s16 sy = (s16)(lane - z - FA_MIS_H / 2 - stageCamY);
        bool v = (sx > -FA_MIS_W && sx < SCREEN_W);
        if (v != (bool)m->vis) {
            m->vis = v;
            SPR_setVisibility(m->s,  v ? VISIBLE : HIDDEN);
        }
        SPR_setHFlip(m->s, m->face < 0);
        SPR_setFrame(m->s, (s16)((m->t / 3) & 1));
        if (v) {
            SPR_setPosition(m->s, sx, sy);
            SPR_setDepth(m->s, (s16)(-lane - 4));
        }
    }
}

// ---------------------------------------------------------------------------
// Frosty
// ---------------------------------------------------------------------------
static bool spawnOne(void) {
    u8 i;
    for (i = 0; i < FR_MAX; i++) if (fr[i].st == FS_OFF) break;
    if (i >= FR_MAX) return FALSE;
    if (!sprVramFits((u16)(frosty_snow.maxNumTile + frosty_shadow.maxNumTile))) return FALSE;
    Frosty* f = &fr[i];
    f->s = NULL; f->sh = NULL; f->def = D_NONE;
    if (!frSetDef(f, D_SNOW)) { frFreeSprites(f); return FALSE; }
    f->sh = SPR_addSprite(&frosty_shadow, -80, -80, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    if (!f->sh) { frFreeSprites(f); return FALSE; }
    SPR_setAutoAnimation(f->sh, FALSE);
    SPR_setVisibility(f->sh, HIDDEN);
    s8 side = pendSide[pendIdx];
    s16 cx = (s16)(frCamX + SCREEN_W / 2);
    s16 top = stageWalkTopAt(cx);
    s16 span = (s16)(frLaneMax - 6 - (top + 8));
    if (span < 8) span = 8;
    f->x     = (s16)(side > 0 ? frCamX + SCREEN_W + 40 : frCamX - 40);
    f->lane  = (s16)(top + 8 + (s16)(random() % (u16)span));
    f->face  = (s8)-side;
    f->st    = FS_IDLE;
    f->robot = 0;
    f->hp    = FR_HP_SNOW;
    f->hitCd = 0;
    f->entered = FALSE;
    f->killer = 0;
    f->t     = 0;
    f->wait  = 4;
    f->kb    = 0;
    f->noseCd = 0;
    f->vis   = FALSE;
    pendIdx++;
    pendN--;
    return TRUE;
}

static void frStartHop(Frosty* f, Player** pls, u8 nPl) {
    s8 n = nearestPlayer(pls, nPl, f->x);
    s16 destX = f->x, destL = f->lane;
    if (n >= 0) {
        s16 px = getPlayerHurtCX(pls[n]);
        s16 stand = (s16)(FR_STAND_MIN + (s16)(random() % FR_STAND_RND));
        // se queda a 'stand' px de la tortuga, del lado en que esta
        destX = (s16)(px + (f->x >= px ? stand : -stand));
        destL = (s16)(getPlayerY(pls[n]) + (s16)(random() % 21) - 10);
        f->face = (s8)(px >= f->x ? 1 : -1);
        // si ya esta cerca, un saltito al azar
        if (iabs((s16)(destX - f->x)) < 8) destX = (s16)(f->x + (s16)(random() % 41) - 20);
    } else {
        destX = (s16)(f->x + (s16)(random() % 61) - 30);
    }
    destX = iclamp(destX, (s16)(frCamX + 30), (s16)(frCamX + SCREEN_W - 30));
    s16 top = stageWalkTopAt(destX);
    destL = iclamp(destL, (s16)(top + 4), (s16)(frLaneMax - 2));
    f->x0 = f->x; f->l0 = f->lane;
    f->dx = iclamp((s16)(destX - f->x), -FR_HOP_DX, FR_HOP_DX);
    f->dl = iclamp((s16)(destL - f->lane), -FR_HOP_DL, FR_HOP_DL);
    f->st = FS_HOP;
    f->t = 0;
}

// Le dieron: baja la vida, retrocede, suena como Baxter.
static void frHit(Frosty* f, Player** pls, u8 k) {
    frSnd();
    f->hp--;
    f->hitCd = FR_HIT_CD;
    f->killer = k;
    s16 pcx = (s16)(pls[k]->x + PLAYER_SPRITE_W / 2);
    f->kbSign = (s8)(f->x >= pcx ? 1 : -1);
    f->kb = FR_KB_V;
    f->st = FS_HURT;
    f->t = 0;
}

static u16 randNoseCd(void) { return (u16)(FR_NOSE_CD_MIN + random() % FR_NOSE_CD_RND); }

static void frUpdate(Frosty* f, Player** pls, u8 nPl) {
    if (f->st == FS_OFF) return;
    if (f->hitCd) f->hitCd--;
    f->t++;
    s16 arc = 0;
    u8  frame = 0;
    bool shadow = TRUE;
    bool blink = FALSE;

    switch (f->st) {
    case FS_IDLE:
        frame = f->robot ? BF_GROUND : SF_GROUND;
        if (f->robot && f->noseCd) f->noseCd--;
        if (f->t >= f->wait) {
            s8 n = nearestPlayer(pls, nPl, f->x);
            if (f->robot && !f->noseCd && n >= 0 && f->entered) {
                // lanza la nariz
                f->face = (s8)(getPlayerHurtCX(pls[n]) >= f->x ? 1 : -1);
                misLaunch(f, (u8)n);
                f->st = FS_NOSE; f->t = 0;
            } else {
                frStartHop(f, pls, nPl);
            }
        }
        break;

    case FS_HOP: {
        u16 dur = FR_HOP_T;
        f->x    = (s16)(f->x0 + (s16)(((s32)f->dx * f->t) / dur));
        f->lane = (s16)(f->l0 + (s16)(((s32)f->dl * f->t) / dur));
        arc = (s16)((4 * FR_HOP_ARC * (s32)f->t * (dur - f->t)) / ((s32)dur * dur));
        if (f->t <= 3 || f->t + 3 >= dur) frame = f->robot ? BF_GROUND : SF_GROUND;
        else frame = f->robot ? (f->t < dur / 2 ? BF_JUMP_A : BF_JUMP_B) : SF_JUMP;
        if (f->t >= dur) {
            f->st = FS_IDLE; f->t = 0;
            f->wait = (u16)(FR_WAIT_MIN + random() % FR_WAIT_RND);
        }
        break;
    }

    case FS_HURT:
        frame = f->robot ? BF_HIT : SF_GROUND;
        blink = !f->robot && (f->t & 2);
        f->x = (s16)(f->x + f->kb * f->kbSign);
        if ((f->t % 3) == 0 && f->kb > 0) f->kb--;
        if (f->t >= FR_HURT_T) {
            f->t = 0;
            if (f->hp == 0) {
                if (f->robot) { f->st = FS_DEAD; }
                else {
                    // se rompe el disfraz: hoja de la rotura
                    if (frSetDef(f, D_BREAK)) { f->st = FS_BREAK; }
                    else if (frSetDef(f, D_BOT)) { f->robot = 1; f->hp = FR_HP_BOT; f->st = FS_IDLE; f->wait = 10; f->noseCd = FR_NOSE_FIRST; }
                }
            } else {
                f->st = FS_IDLE; f->wait = 8;
            }
        }
        break;

    case FS_BREAK: {
        u8 b = (u8)(f->t / FR_BREAK_STEP);
        if (b > 2) b = 2;
        frame = b;
        if (f->t >= 3 * FR_BREAK_STEP) {
            if (frSetDef(f, D_BOT)) {
                f->robot = 1; f->hp = FR_HP_BOT; f->st = FS_IDLE;
                f->t = 0; f->wait = 10; f->noseCd = FR_NOSE_FIRST;
            } else {
                // sin VRAM para el robot: queda en la rotura hasta que haya
                f->t = (u16)(3 * FR_BREAK_STEP - 1);
            }
        }
        break;
    }

    case FS_NOSE:
        frame = (f->t < FR_NOSE_GONE) ? BF_NONOSE : BF_REGEN;
        if (f->t >= FR_NOSE_GONE + FR_NOSE_REGEN) {
            f->st = FS_IDLE; f->t = 0; f->wait = 14;
            f->noseCd = randNoseCd();
        }
        break;

    case FS_DEAD:
        frame = BF_DEAD;
        shadow = FALSE;                      // el arte ya trae la suya
        blink = (f->t > FR_DEAD_T - FR_DEAD_BLINK) && (f->t & 2);
        if (f->t >= FR_DEAD_T) {
            boomAt(f->x, (s16)(f->lane - 34), f->lane);
            if (f->killer < nPl) addPlayerScore(pls[f->killer], FR_SCORE);
            frKill(f);
            return;
        }
        break;

    default: break;
    }

    // pantalla: una vez adentro, no se sale por los costados
    if (!f->entered) {
        if (f->x > frCamX + 24 && f->x < frCamX + SCREEN_W - 24) f->entered = TRUE;
    } else if (f->st != FS_HOP) {
        f->x = iclamp(f->x, (s16)(frCamX + 18), (s16)(frCamX + SCREEN_W - 18));
    }

    // --- golpes de las tortugas ---------------------------------------------
    bool hittable = f->entered && !f->hitCd &&
                    (f->st == FS_IDLE || f->st == FS_HOP || f->st == FS_NOSE);
    if (hittable) {
        s16 hw  = f->robot ? FR_HALF_W_BOT : FR_HALF_W_SNOW;
        s16 air = (f->st == FS_HOP && frame != SF_GROUND && frame != BF_GROUND) ? (s16)(FR_HOVER + arc) : 0;
        for (u8 k = 0; k < nPl; k++) {
            if (!playerAlive(pls[k])) continue;
            if (!playerAttackHitsBand(pls[k], f->x, f->lane, hw,
                                      (s16)(-(air + FR_BODY_H)), (s16)(-air))) continue;
            frHit(f, pls, k);
            frame = f->robot ? BF_HIT : SF_GROUND;
            break;
        }
    }

    // --- dibujo ---------------------------------------------------------------
    u8 d = f->def;
    s16 w = defW(d);
    s16 sx = (s16)(f->x - w / 2 - frCamX);
    s16 sy = (s16)(f->lane - defFeet(d) - arc - stageCamY);
    bool v = (sx > -w && sx < SCREEN_W) && !blink;
    if (f->s) {
        if (v != (bool)f->vis) {
            f->vis = v;
            SPR_setVisibility(f->s, v ? VISIBLE : HIDDEN);
        }
        SPR_setHFlip(f->s, f->face < 0);
        SPR_setFrame(f->s, frame);
        if (v) {
            SPR_setPosition(f->s, sx, sy);
            SPR_setDepth(f->s, (s16)-f->lane);
        }
    }
    if (f->sh) {
        bool sv = shadow && (sx > -w && sx < SCREEN_W);
        SPR_setVisibility(f->sh, sv ? VISIBLE : HIDDEN);
        if (sv) {
            SPR_setPosition(f->sh, (s16)(f->x - FR_SHADOW_W / 2 - frCamX),
                            (s16)(f->lane - FR_SHADOW_H / 2 - stageCamY));
            SPR_setDepth(f->sh, (s16)(-f->lane + 1));
        }
    }
}

// ---------------------------------------------------------------------------
void frostyInit(void) {
    for (u8 i = 0; i < FR_MAX; i++) { fr[i].s = fr[i].sh = NULL; fr[i].st = FS_OFF; fr[i].def = D_NONE; }
    for (u8 i = 0; i < FR_MIS_MAX; i++) { mis[i].s = mis[i].sh = NULL; mis[i].on = FALSE; }
    for (u8 i = 0; i < FR_BOOM_MAX; i++) {
        booms[i].on = FALSE;
        for (u8 q = 0; q < 4; q++) booms[i].part[q] = NULL;
    }
    pendN = 0; pendIdx = 0; pendWait = 0; starveT = 0;
    waveOn = FALSE;
}

bool frostyStart(s16 camX, u8 nPlayers, s16 laneMax) {
    if (waveOn) return TRUE;
    if (stageAliveEnemies() > 0) return FALSE;       // pantalla limpia (la paleta PAL3 es nuestra)
    PAL_setPalette(PAL3, frosty_snow.palette->data, DMA);
    frCamX = camX;
    frLaneMax = laneMax;
    pendN = (u8)(nPlayers > 1 ? 3 : 2);
    pendIdx = 0;
    pendWait = 20;
    starveT = 0;
    waveOn = TRUE;
    return TRUE;
}

bool frostyActive(void) {
    if (!waveOn) return FALSE;
    if (pendN) return TRUE;
    for (u8 i = 0; i < FR_MAX; i++) if (fr[i].st != FS_OFF) return TRUE;
    for (u8 i = 0; i < FR_MIS_MAX; i++) if (mis[i].on) return TRUE;
    for (u8 i = 0; i < FR_BOOM_MAX; i++) if (booms[i].on) return TRUE;
    return FALSE;
}

void frostyUpdate(Player** pls, u8 nPl, s16 camX) {
    if (!waveOn) return;
    frCamX = camX;
    // entran de a uno
    if (pendN) {
        if (pendWait) pendWait--;
        else if (spawnOne()) { pendWait = FR_ENTER_GAP; starveT = 0; }
        else {
            bool any = FALSE;
            for (u8 i = 0; i < FR_MAX; i++) if (fr[i].st != FS_OFF) any = TRUE;
            if (!any && ++starveT >= FR_STARVE_MAX) pendN = 0;   // nunca entra: se saltea
        }
    }
    for (u8 i = 0; i < FR_MAX; i++) frUpdate(&fr[i], pls, nPl);
    misUpdate(pls, nPl);
    boomsUpdate();
    if (!frostyActive()) waveOn = FALSE;
}

void frostyRelease(void) {
    for (u8 i = 0; i < FR_MAX; i++) { frFreeSprites(&fr[i]); fr[i].st = FS_OFF; fr[i].def = D_NONE; }
    for (u8 i = 0; i < FR_MIS_MAX; i++) if (mis[i].on || mis[i].s || mis[i].sh) misFree(&mis[i]);
    for (u8 i = 0; i < FR_BOOM_MAX; i++) if (booms[i].on) boomFree(&booms[i]);
    sprDefragLock = 0;
    pendN = 0;
    waveOn = FALSE;
}
