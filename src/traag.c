#include "traag.h"
#include "difficulty.h"  // (20/10) vida segun jugadores
#include "audio.h"    // boss_hit, hit_turtles, foot_soldier_explode
#include "enemy.h"    // sprDefragLock
#include "baxter_res.h"  // baxter_boom (la explosion del misil)

// ===========================================================================
// TRAAG + BOMBAS — ver traag.h
// ===========================================================================

static s16 gabs(s16 v)                 { return (v < 0) ? -v : v; }
static s16 gclamp(s16 v, s16 a, s16 b) { return (v < a) ? a : ((v > b) ? b : v); }

static s16 tStep(u8* acc, u16 q) {
    u16 t = (u16)(*acc + q);
    *acc = (u8)(t & 0xFF);
    return (s16)(t >> 8);
}

static s16 tToward(s16 v, s16 to, s16 step) {
    if (v < to) return (to - v < step) ? to : (s16)(v + step);
    if (v > to) return (v - to < step) ? to : (s16)(v - step);
    return v;
}

// El jugador EN JUEGO mas cercano; si no queda ninguno, el P1.
static Player* nearestPlayer(Player** pls, u8 nPl, s16 x) {
    Player* best = pls[0];
    s16 bestD = 0x7FFF;
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k])) continue;
        s16 d = gabs((s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2 - x));
        if (d < bestD) { bestD = d; best = pls[k]; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// BOMBAS: salen del brazo, suben un poco y caen en arco hacia adelante; al
// tocar el piso revientan y la explosion voltea a la tortuga que este cerca.
// ---------------------------------------------------------------------------
static struct {
    bool    active;
    Sprite* sprite;
    s16     x;             // centro (mundo)
    s16     lane;          // lane de Traag al tirarla
    s8      dir;
    s16     zq, vz;        // altura y velocidad vertical, Q8
    u8      accX;
} bombs[MAX_TRAAG_MISSILES];

// (21/10) La explosion es la de la muerte de Baxter: 128x128 en cuatro
// cuartos de 64x64 (baxter_boom, fila = cuarto). (21/10) Con su PROPIA
// paleta en PAL2: en el Technodrome PAL2 es la de los soldiers, y la pelea
// con Traag arranca recien sin soldiers vivos y no entran mas (traagSpawn la
// carga). Se arma con los dos de la
// izquierda y sus espejos (128 tiles), o si no entra con uno solo y sus tres
// espejos (64): los
// espejos apuntan a la VRAM de su dueno, asi que mientras haya explosiones no
// se desfragmenta (sprDefragLock).
#define BOOM_PARTS 4
static struct {
    bool    active;
    Sprite* part[BOOM_PARTS];
    u8      anim[BOOM_PARTS];
    s16     x, lane;
    s16     dz;            // (22/10) cuanto mas arriba del piso (las de la muerte)
    u8      frame, tick, t;
    u8      hitMask;       // jugadores ya alcanzados
} booms[MAX_TRAAG_EXPLOSIONS];

static void missileInitAll(void) {
    for (u16 i = 0; i < MAX_TRAAG_MISSILES; i++) { bombs[i].active = FALSE; bombs[i].sprite = NULL; }
    for (u16 i = 0; i < MAX_TRAAG_EXPLOSIONS; i++) {
        booms[i].active = FALSE;
        for (u16 q = 0; q < BOOM_PARTS; q++) booms[i].part[q] = NULL;
    }
}

static Sprite* boomMaster(u16 anim) {
    Sprite* s = SPR_addSpriteEx(&baxter_boom, -128, -128,
                                TILE_ATTR(PAL2, FALSE, FALSE, FALSE),
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
    Sprite* s = SPR_addSpriteEx(&baxter_boom, -128, -128,
                                TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, idx),
                                SPR_FLAG_AUTO_VISIBILITY);
    if (s) {
        SPR_setAutoAnimation(s, FALSE);
        SPR_setAnimAndFrame(s, (s16)anim, 0);
        SPR_setHFlip(s, hflip);
        SPR_setVFlip(s, vflip);
    }
    return s;
}

static void boomRelease(u16 i) {
    for (u16 q = 0; q < BOOM_PARTS; q++)
        if (booms[i].part[q]) { SPR_releaseSprite(booms[i].part[q]); booms[i].part[q] = NULL; }
    booms[i].active = FALSE;
    bool any = FALSE;
    for (u16 j = 0; j < MAX_TRAAG_EXPLOSIONS; j++) if (booms[j].active) any = TRUE;
    if (!any) sprDefragLock = 0;
}

static void boomCreate(u16 i) {
    Sprite** sp = booms[i].part;
    u8* an = booms[i].anim;
    sprDefragLock = 1;
    const u16 T = baxter_boom.maxNumTile;
    // Arranca en la mitad (no los 4 cuartos como Baxter): en el Technodrome
    // la cola de DMA ya va cargada con el mapa, y 4 cuartos son 8 KB por
    // cambio de frame (se vio un frame con la explosion a pedazos).
    if (SPR_getFreeVRAM() >= 2 * T) {
        sp[0] = boomMaster(0);
        sp[2] = boomMaster(2);
        if (sp[0] && sp[2]) {
            sp[1] = boomMirror(sp[0], 0, TRUE, FALSE);
            sp[3] = boomMirror(sp[2], 2, TRUE, FALSE);
            an[0] = an[1] = 0;
            an[2] = an[3] = 2;
            return;
        }
        for (u16 q = 0; q < BOOM_PARTS; q++)
            if (sp[q]) { SPR_releaseSprite(sp[q]); sp[q] = NULL; }
    }
    sp[0] = boomMaster(0);
    if (!sp[0]) return;                 // sin VRAM: solo el sonido y el dano
    sp[1] = boomMirror(sp[0], 0, TRUE,  FALSE);
    sp[2] = boomMirror(sp[0], 0, FALSE, TRUE);
    sp[3] = boomMirror(sp[0], 0, TRUE,  TRUE);
    for (u16 q = 0; q < BOOM_PARTS; q++) an[q] = 0;
}

static void boomAtEx(s16 x, s16 lane, s16 dz, bool harmless) {
    u16 i;
    for (i = 0; i < MAX_TRAAG_EXPLOSIONS; i++) if (!booms[i].active) break;
    if (i >= MAX_TRAAG_EXPLOSIONS) {
        // Las dos ocupadas: la mas vieja deja lugar.
        i = (booms[0].t >= booms[1].t) ? 0 : 1;
        boomRelease(i);
    }
    booms[i].active  = TRUE;
    booms[i].x       = x;
    booms[i].lane    = lane;
    booms[i].frame   = 0;
    booms[i].tick    = 0;
    booms[i].t       = 0;
    booms[i].dz      = dz;
    booms[i].hitMask = harmless ? 0xFF : 0;    // 0xFF: no alcanza a nadie
    boomCreate(i);
    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                   SOUND_PCM_CH3, 13, FALSE, FALSE);
}

static void boomAt(s16 x, s16 lane) { boomAtEx(x, lane, 0, FALSE); }

// (22/10) EXPLOSIONES de la muerte de Traag, como las de Granitor: en cadena
// sobre el cuerpo, una cada TRAAG_DBOOM_GAP cuadros, con las explosiones del
// misil (baxter_boom, PAL2) pero sin dano. Con 2 lugares y 40 cuadros cada
// una, el GAP de 20 deja que la siguiente entre justo cuando se va la vieja.
static const s8 tDeathOff[TRAAG_DBOOMS][2] = {
    { 0, 0 }, { -28, -26 }, { 26, 18 }, { 8, -46 }, { -18, 22 }
};
static u8  tDeathN;        // cuantas faltan
static u16 tDeathWait;
static s16 tDeathX, tDeathY;

static void traagDeathBoomsStart(s16 x, s16 y) {
    tDeathN = TRAAG_DBOOMS;
    tDeathWait = 0;
    tDeathX = x;
    tDeathY = y;
}

static void traagDeathBoomsStep(void) {
    if (!tDeathN) return;
    if (tDeathWait) { tDeathWait--; return; }
    u8 k = (u8)(TRAAG_DBOOMS - tDeathN);
    // Centro de la explosion: TRAAG_DBOOM_DZ sobre los pies, mas el corrimiento
    // (y negativo = mas arriba; boomAtEx ya la centra 40 px sobre el piso).
    boomAtEx((s16)(tDeathX + tDeathOff[k][0]), tDeathY,
             (s16)(TRAAG_DBOOM_DZ - 40 - tDeathOff[k][1]), TRUE);
    tDeathN--;
    tDeathWait = TRAAG_DBOOM_GAP;
}

static void bombHide(u16 i) {
    bombs[i].active = FALSE;
    if (bombs[i].sprite) SPR_setVisibility(bombs[i].sprite, HIDDEN);
}

static void bombThrow(s16 x, s16 lane, s8 dir) {
    for (u16 i = 0; i < MAX_TRAAG_MISSILES; i++) {
        if (bombs[i].active) continue;
        if (!bombs[i].sprite)
            bombs[i].sprite = SPR_addSprite(&traag_missil, -80, -80,
                                            TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        bombs[i].active = TRUE;
        bombs[i].x      = x;
        bombs[i].lane   = lane;
        bombs[i].dir    = dir;
        bombs[i].zq     = (s16)(TRAAG_GUN_DY << 8);   // sale a la altura del brazo
        bombs[i].vz     = TRAAG_BOMB_VQ;
        bombs[i].accX   = 0;
        if (bombs[i].sprite) {
            SPR_setAutoAnimation(bombs[i].sprite, TRUE);
            SPR_setAnim(bombs[i].sprite, 0);
            SPR_setAnimationLoop(bombs[i].sprite, TRUE);
            SPR_setHFlip(bombs[i].sprite, dir < 0);   // el arte mira a la derecha
            SPR_setVisibility(bombs[i].sprite, VISIBLE);
        }
        return;
    }
}

void traagMissileUpdate(Player** pls, u8 nPl, s16 camX, s16 camY) {
    for (u16 i = 0; i < MAX_TRAAG_MISSILES; i++) {
        if (!bombs[i].active) continue;
        bombs[i].x  += bombs[i].dir * tStep(&bombs[i].accX, TRAAG_BOMB_XQ);
        bombs[i].zq += bombs[i].vz;
        bombs[i].vz -= TRAAG_BOMB_GRAV_Q;
        if (bombs[i].zq <= 0) {                   // toco el piso: revienta
            boomAt(bombs[i].x, bombs[i].lane);
            bombHide(i);
            continue;
        }
        if (bombs[i].x < camX - 40 || bombs[i].x > camX + 360) { bombHide(i); continue; }
        if (bombs[i].sprite) {
            SPR_setPosition(bombs[i].sprite, bombs[i].x - camX - 32,
                            bombs[i].lane - (bombs[i].zq >> 8) - camY - 16);
            SPR_setDepth(bombs[i].sprite, (s16)(-(bombs[i].lane) - 1));
        }
    }
    for (u16 i = 0; i < MAX_TRAAG_EXPLOSIONS; i++) {
        if (!booms[i].active) continue;
        if (booms[i].t < 255) booms[i].t++;
        if (++booms[i].tick >= TRAAG_BOOM_TICKS) {
            booms[i].tick = 0;
            if (++booms[i].frame >= TRAAG_BOOM_FRAMES) { boomRelease(i); continue; }
            for (u16 q = 0; q < BOOM_PARTS; q++)
                if (booms[i].part[q])
                    SPR_setAnimAndFrame(booms[i].part[q], booms[i].anim[q], booms[i].frame);
        }
        // La explosion alcanza al principio (una vez a cada tortuga).
        if (booms[i].t <= TRAAG_EXPL_HIT_T) {
            for (u8 k = 0; k < nPl; k++) {
                u8 bit = (u8)(1 << k);
                if ((booms[i].hitMask & bit) || !playerCanBeHit(pls[k])) continue;
                s16 px = getPlayerHurtCX(pls[k]);
                if (gabs((s16)(px - booms[i].x)) >= TRAAG_EXPL_HALF_W) continue;
                if (gabs((s16)(getPlayerY(pls[k]) - booms[i].lane)) > TRAAG_EXPL_TOL_Y) continue;
                booms[i].hitMask |= bit;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
                playerHitBarsKnockdown(pls[k], booms[i].x, TRAAG_EXPL_DMG);
            }
        }
        // Centro del lienzo de 128x128 a media altura sobre el piso.
        s16 bx = (s16)(booms[i].x - camX - TRAAG_BOOM_HALF);
        s16 by = (s16)(booms[i].lane - 40 - booms[i].dz - camY - TRAAG_BOOM_HALF);
        for (u16 q = 0; q < BOOM_PARTS; q++) {
            if (!booms[i].part[q]) continue;
            SPR_setPosition(booms[i].part[q], (s16)(bx + (q & 1) * TRAAG_BOOM_HALF),
                            (s16)(by + (q >> 1) * TRAAG_BOOM_HALF));
            SPR_setDepth(booms[i].part[q], (s16)(-(booms[i].lane) - 2));
        }
    }
}

void traagMissileReleaseAll(void) {
    for (u16 i = 0; i < MAX_TRAAG_MISSILES; i++) {
        if (bombs[i].sprite) { SPR_releaseSprite(bombs[i].sprite); bombs[i].sprite = NULL; }
        bombs[i].active = FALSE;
    }
    for (u16 i = 0; i < MAX_TRAAG_EXPLOSIONS; i++) boomRelease(i);
}

// ---------------------------------------------------------------------------
// TRAAG
// ---------------------------------------------------------------------------
static u16 traagPal[16], traagPalBurn[16];

void traagInit(Traag* g) {
    memset(g, 0, sizeof(Traag));
    g->sprite = NULL;
    g->state = TRAAG_INACTIVE;
    g->dir = -1;
    g->hp = diffBossHp(TRAAG_HP); diffBossTrack(&g->hp);
    g->anim = 0xFF;
    g->laneTop = 142; g->laneBot = 196;
    g->topAt = NULL;
    missileInitAll();
    tDeathN = 0;
}

void traagSpawn(Traag* g, s16 arenaLeft, s16 arenaRight, s16 laneTop, s16 laneBot,
                   TraagTopAtFn topAt) {
    for (u16 i = 0; i < 16; i++) traagPal[i] = traag_boss.palette->data[i];
    bossFlashBurn(traagPal, traagPalBurn);
    PAL_setPalette(PAL3, traagPal, DMA);
    // (21/10) PAL2 (la de los soldiers, que ya no quedan): la explosion de
    // Baxter con sus colores.
    PAL_setPalette(PAL2, baxter_boom.palette->data, DMA);
    bossFlashReset(&g->flashLow);
    if (!g->sprite)
        g->sprite = SPR_addSpriteSafe(&traag_boss, -160, -160,
                                      TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    g->state = TRAAG_ENTER;
    g->hp = diffBossHp(TRAAG_HP); diffBossTrack(&g->hp);
    g->xq = (s16)((arenaRight + 56) * 4);
    g->yq = (s16)(((laneTop + laneBot) / 2) * 4);
    g->dir = -1;
    g->anim = 0xFF;
    g->flash = 0;
    g->invuln = 0;
    g->timer = 0;
    g->arenaLeft = arenaLeft;
    g->arenaRight = arenaRight;
    g->laneTop = laneTop;
    g->laneBot = laneBot;
    g->topAt = topAt;
    if (g->sprite) {
        SPR_setVisibility(g->sprite, VISIBLE);
        SPR_setAutoAnimation(g->sprite, TRUE);
        SPR_setAnim(g->sprite, TRAAG_ANIM_WALK);
        SPR_setAnimationLoop(g->sprite, TRUE);
        g->anim = TRAAG_ANIM_WALK;
    }
}

bool traagIsGone(const Traag* g)  { return g->state == TRAAG_GONE; }
bool traagIsDying(const Traag* g) { return g->state == TRAAG_DEATH || g->state == TRAAG_GONE; }

bool traagCanBeHit(const Traag* g) {
    // Solo quieto o caminando.
    return (g->state == TRAAG_IDLE || g->state == TRAAG_WALK) && g->sprite;
}

static void traagSetAnim(Traag* g, u8 a, bool loop) {
    if (g->anim == a) return;
    g->anim = a;
    SPR_setAutoAnimation(g->sprite, TRUE);
    SPR_setAnimAndFrame(g->sprite, a, 0);
    SPR_setAnimationLoop(g->sprite, loop);
}

static void traagManual(Traag* g, u8 a, u8 f) {
    g->anim = a;
    SPR_setAutoAnimation(g->sprite, FALSE);
    SPR_setAnimAndFrame(g->sprite, a, f);
}

// Tope de la lane en esta X: el de la arena y el de la pared del nivel.
static s16 traagLaneTop(const Traag* g, s16 x) {
    s16 t = g->laneTop;
    if (g->topAt) {
        s16 w = g->topAt(x);
        if (w > t) t = w;
    }
    return (t > g->laneBot) ? g->laneBot : t;
}

static void traagEnter(Traag* g, TraagState s) {
    g->state = s;
    g->timer = 0;
}

static void traagToIdle(Traag* g) { traagEnter(g, TRAAG_IDLE); }
static void traagToWalk(Traag* g) { traagEnter(g, TRAAG_WALK); g->accX = g->accY = 0; }

static void traagKill(Traag* g) {
    g->hp = 0;
    traagEnter(g, TRAAG_DEATH);
    g->dPhase = 0;
    g->blinks = 0;
    PAL_setPalette(PAL3, traagPal, DMA);
    traagManual(g, TRAAG_ANIM_DAMAGE, TRAAG_DMG_FLASH);
    XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 15, FALSE, FALSE);
    // (22/10) Explosiones en cadena sobre el cuerpo (como Granitor). Los
    // misiles que quedaban en el aire se van: la cadena usa sus lugares.
    for (u16 i = 0; i < MAX_TRAAG_MISSILES; i++) bombHide(i);
    for (u16 i = 0; i < MAX_TRAAG_EXPLOSIONS; i++) boomRelease(i);
    traagDeathBoomsStart((s16)(g->xq >> 2), (s16)(g->yq >> 2));
}

bool traagPlayerHits(Traag* g, Player** pls, u8 nPl, s8* killer) {
    if (!traagCanBeHit(g) || g->invuln) return FALSE;
    s16 gx = (s16)(g->xq >> 2);
    s16 gy = (s16)(g->yq >> 2);
    for (u8 k = 0; k < nPl; k++) {
        if (!playerAttackHitsBoss(pls[k], gx, gy, TRAAG_BODY_HALF_W, TRAAG_BODY_H,
                                  TRAAG_HIT_DY_UP, TRAAG_HIT_DY_DOWN)) continue;
        s16 dmg = isPlayerSpecialAttack(pls[k]) ? TRAAG_SPECIAL_DMG
                : (isPlayerJumpKicking(pls[k]) ? TRAAG_JUMPKICK_DMG : 1);
        // (22/10) Gustavo: el mismo sonido que los robots del latigo (Roadkill
        // Rodney, robot.c) cuando les pegas. Antes: hit_foot_sfx (rock soldier).
        XGM2_playPCMEx(mouser_hit_sfx, sizeof(mouser_hit_sfx), SOUND_PCM_CH3, 15, FALSE, FALSE);
        g->hp -= dmg;
        g->flash = 8;
        g->invuln = TRAAG_HURT_TICKS;
        if (g->chain < 255) g->chain++;      // (21/10) golpes seguidos
        g->chainT = 0;
        if (g->hp <= 0) {
            traagKill(g);
            if (killer) *killer = (s8)k;
            return TRUE;
        }
        traagEnter(g, TRAAG_HURT);
        traagManual(g, TRAAG_ANIM_DAMAGE, TRAAG_DMG_FLASH);
        XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 12, FALSE, FALSE);
        return FALSE;
    }
    return FALSE;
}

static void traagRender(Traag* g) {
    s16 x = (s16)(g->xq >> 2);
    s16 y = (s16)(g->yq >> 2);
    SPR_setHFlip(g->sprite, (g->dir < 0));
    SPR_setPosition(g->sprite, x - g->camX - TRAAG_FRAME_W / 2, y - g->camY - TRAAG_FOOT_OFFSET);
    SPR_setDepth(g->sprite, (s16)(-y));
    SPR_setVisibility(g->sprite, (g->flash & 1) ? HIDDEN : VISIBLE);
}

// Al terminar el golpe, la recarga o el golpeado.
static void traagAfterAction(Traag* g, bool aligned) {
    if (aligned) traagToWalk(g);
    else if (random() & 1) traagToWalk(g);
    else traagToIdle(g);
}

void traagUpdate(Traag* g, Player** pls, u8 nPl, s16 camX, s16 camY) {
    if (g->state == TRAAG_INACTIVE || g->state == TRAAG_GONE || !g->sprite) return;

    g->camX = camX;
    g->camY = camY;
    s16 x  = (s16)(g->xq >> 2);
    s16 y  = (s16)(g->yq >> 2);
    Player* tgt = nearestPlayer(pls, nPl, x);
    s16 px = (s16)(getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2);
    s16 py = getPlayerY(tgt);
    s16 dX = (s16)(px - x);
    s16 dY = (s16)(py - y);
    bool alignY = (gabs(dY) <= TRAAG_ALIGN_DY);
    bool alNear = alignY && gabs(dX) <= TRAAG_NEAR_DX;
    bool alFar  = alignY && gabs(dX) >  TRAAG_FAR_DX;
    bool gameOn = !isPlayerGameOver(tgt);

    g->timer++;
    if (g->flash)    g->flash--;
    if (g->invuln)   g->invuln--;
    if (g->chainT < 0xFFFF) g->chainT++;
    if (g->chainT > TRAAG_CHAIN_RESET_T) g->chain = 0;

    switch (g->state) {

    case TRAAG_ENTER:
        traagSetAnim(g, TRAAG_ANIM_WALK, TRUE);
        g->dir = -1;
        g->xq -= tStep(&g->accX, TRAAG_WALK_Q) * 4;
        if ((s16)(g->xq >> 2) <= g->arenaRight - 60) {
            g->xq = (s16)((g->arenaRight - 60) * 4);
            traagToIdle(g);
        }
        break;

    case TRAAG_IDLE:
        traagSetAnim(g, TRAAG_ANIM_IDLE1, TRUE);
        g->dir = (dX >= 0) ? 1 : -1;
        if (g->timer < TRAAG_IDLE_T || !gameOn) break;
        if (alNear) {
            traagEnter(g, TRAAG_PUNCH);
            traagManual(g, TRAAG_ANIM_PUNCH, 0);
        } else {
            traagToWalk(g);
        }
        break;

    case TRAAG_WALK: {
        traagSetAnim(g, TRAAG_ANIM_WALK, TRUE);
        g->dir = (dX >= 0) ? 1 : -1;
        s16 sx = tStep(&g->accX, TRAAG_WALK_Q);
        s16 sy = tStep(&g->accY, TRAAG_WALK_Q);
        if (gabs(dX) > TRAAG_MIN_DIST) g->xq += (dX >= 0) ? sx * 4 : -sx * 4;
        g->yq = (s16)(tToward(y, py, sy) * 4);
        if (!gameOn) break;
        if (alNear) {
            traagEnter(g, TRAAG_PUNCH);
            traagManual(g, TRAAG_ANIM_PUNCH, 0);
        } else if (alFar && (random() % 3) != 0) {
            traagEnter(g, TRAAG_FIRE);
            traagManual(g, TRAAG_ANIM_FIRE, 0);
        } else if (g->timer > TRAAG_WALK_MAX) {
            traagToIdle(g);
        }
        break;
    }

    case TRAAG_FIRE:
        // Tira la bomba con el brazo (f0) y despues recarga (f1).
        if (g->timer == 1)
            bombThrow((s16)(x + g->dir * TRAAG_GUN_DX), y, g->dir);
        if (g->timer == TRAAG_THROW_T) SPR_setFrame(g->sprite, 1);
        if (g->timer >= TRAAG_THROW_T + TRAAG_RELOAD_T) traagAfterAction(g, alignY);
        break;

    case TRAAG_PUNCH: {
        // 23 frames repartidos en los 6 del sheet; el impacto es el frame 4.
        static const u8 cut[6] = { 3, 6, 9, 12, 18, 23 };
        u8 f = 0;
        while (f < 5 && g->timer >= cut[f]) f++;
        SPR_setFrame(g->sprite, f);
        if (f == 4 && g->timer < cut[3] + TRAAG_PUNCH_HIT_T) {
            s16 hx = (s16)(x + g->dir * TRAAG_PUNCH_HIT_DX);
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                s16 kx = getPlayerHurtCX(pls[k]);
                if (gabs((s16)(kx - hx)) >= TRAAG_PUNCH_HIT_W) continue;
                s16 ky = (s16)(getPlayerY(pls[k]) - y);
                if (ky < -TRAAG_HIT_DY_UP || ky > TRAAG_HIT_DY_DOWN) continue;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
                playerHitBarsKnockdown(pls[k], x, TRAAG_PUNCH_DMG);
            }
        }
        if (g->timer >= TRAAG_PUNCH_T) traagAfterAction(g, alignY);
        break;
    }

    case TRAAG_HURT:
        if (g->timer < TRAAG_HURT_TICKS) break;
        if (g->chain >= TRAAG_COUNTER_HITS && gameOn) {
            // (21/10) CONTRAATAQUE: se da vuelta hacia el que tiene mas
            // cerca y le tira el culatazo ya levantando el arma (mientras
            // dura el culatazo no se le puede pegar: ver traagCanBeHit).
            g->chain = 0;
            g->dir = (dX >= 0) ? 1 : -1;
            traagEnter(g, TRAAG_PUNCH);
            g->timer = TRAAG_COUNTER_SKIP;
            traagManual(g, TRAAG_ANIM_PUNCH, 0);
            break;
        }
        traagAfterAction(g, alignY);
        break;

    case TRAAG_DEATH:
        g->flash = 0;
        traagDeathBoomsStep();                   // (22/10)
        if (g->dPhase == 0) {
            if (g->timer > 8) {
                g->timer = 0;
                g->blinks++;
                SPR_setFrame(g->sprite, (g->blinks & 1) ? TRAAG_DMG_FLASH : TRAAG_DMG_BREAK);
                if (g->blinks >= TRAAG_DEATH_BLINK * 2) {
                    g->dPhase = 1;
                    SPR_setFrame(g->sprite, TRAAG_DMG_BREAK);
                }
            }
        } else if (g->dPhase == 1) {
            if (g->timer > TRAAG_DEATH_BREAK_T) {
                g->dPhase = 2;
                g->timer = 0;
                SPR_setFrame(g->sprite, TRAAG_DMG_PILE);
                XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                               SOUND_PCM_CH3, 15, FALSE, FALSE);
            }
        } else if (g->timer > TRAAG_DEATH_PILE_T) {
            g->state = TRAAG_GONE;
            SPR_releaseSprite(g->sprite);
            g->sprite = NULL;
            traagMissileReleaseAll();
            return;
        }
        break;

    default:
        break;
    }

    // Parpadeo de vida baja (PAL3 es solo suya y de sus bombas).
    if (g->state != TRAAG_DEATH && bossFlashStep(&g->flashLow, g->hp, diffBossHp(TRAAG_HP)))
        PAL_setPalette(PAL3, g->flashLow.on ? traagPalBurn : traagPal, DMA);

    // Arena y lane (con la pared del nivel).
    x = gclamp((s16)(g->xq >> 2), g->arenaLeft, g->arenaRight);
    if (g->state != TRAAG_ENTER) g->xq = (s16)(x * 4);
    y = gclamp((s16)(g->yq >> 2), traagLaneTop(g, x), g->laneBot);
    g->yq = (s16)(y * 4);

    traagRender(g);
}

void traagRelease(Traag* g) {
    if (g->sprite) { SPR_releaseSprite(g->sprite); g->sprite = NULL; }
    g->state = TRAAG_INACTIVE;
    traagMissileReleaseAll();
}
