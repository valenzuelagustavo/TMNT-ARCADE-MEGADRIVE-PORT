#include "baxter.h"
#include "audio.h"    // hit_turtles, foot_soldier_explode
#include "enemy.h"    // sprVramFits / sprDefragLock (30/09)

// ===========================================================================
// BAXTER + RATAS — ver baxter.h
// ===========================================================================
// La nave vuela en una Lissajous sobre la arena y suelta un Mouser cada
// BAXTER_DROP_T; no ataca. Los Mousers persiguen (los dos primeros) o
// deambulan, y saltan a morder: se PRENDEN de la tortuga hasta que zafa.
// ===========================================================================

static s16 xabs(s16 v)                 { return (v < 0) ? -v : v; }
static s16 xclamp(s16 v, s16 a, s16 b) { return (v < a) ? a : ((v > b) ? b : v); }

// Seno de tabla: fase 0..255 = una vuelta, resultado -256..256.
static const u8 bxQuarter[65] = {
    0, 6, 13, 19, 25, 31, 38, 44, 50, 56, 62, 68, 74, 80, 86, 92, 98, 104,
    109, 115, 121, 126, 132, 137, 142, 147, 152, 157, 162, 167, 172, 177, 181,
    185, 190, 194, 198, 202, 206, 209, 213, 216, 220, 223, 226, 229, 231, 234,
    237, 239, 241, 243, 245, 247, 248, 250, 251, 252, 253, 254, 255, 255, 255,
    255, 255
};
static s16 bxSin(u8 ph) {
    u8 q = (u8)(ph & 63);
    switch (ph >> 6) {
        case 0:  return (s16)bxQuarter[q];
        case 1:  return (s16)bxQuarter[64 - q];
        case 2:  return (s16)-bxQuarter[q];
        default: return (s16)-bxQuarter[64 - q];
    }
}

static s16 bxStep(u8* acc, u16 q) {
    u16 t = (u16)(*acc + q);
    *acc = (u8)(t & 0xFF);
    return (s16)(t >> 8);
}

// El jugador EN JUEGO mas cercano en X; si no queda ninguno, el P1.
static Player* nearestPlayer(Player** pls, u8 nPl, s16 x) {
    Player* best = pls[0];
    s16 bestD = 0x7FFF;
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k])) continue;
        s16 d = xabs((s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2 - x));
        if (d < bestD) { bestD = d; best = pls[k]; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// RATAS (MOUSERS)
// ---------------------------------------------------------------------------
typedef enum {
    RAT_INACTIVE, RAT_FALL, RAT_WALK, RAT_PAUSE, RAT_JUMP, RAT_LATCH,
    RAT_HURT, RAT_DEAD, RAT_BOOM
} RatState;

typedef struct {
    Sprite*  sprite;
    RatState state;
    s16      x;            // centro (mundo)
    s16      y;            // pies (lane)
    s16      z;            // altura (caida y salto), solo visual
    s8       dir;
    u16      timer;
    u8       accX, accY, accZ;
    s16      landY;        // lane donde aterriza al caer de la nave
    s16      goX, goY;     // destino al deambular
    u8       chaser;       // persigue a la tortuga (las dos primeras)
    u8       anim;
    Player*  victim;       // a quien esta mordiendo
    u16      drain;        // frames hasta la proxima barra que saca
    Sprite*  boomSprite;
    u8       boomFrame;
    u8       boomTick;
} BaxterRat;

static BaxterRat rats[MAX_BAXTER_RATS];
static s16 ratLaneTop, ratLaneBot;
static s16 ratArenaL, ratArenaR;

// ALTURAS DE VUELO: todo es la Y del BORDE DE ARRIBA del frame (72 px), en
// mundo. El piso de la arena (laneBot de baxterSpawn) fija el tope, asi la
// nave baja por encima del agua del canal.
#define BAXTER_Y_MIN        44      // lo mas alto
static s16 bxYMax  = 128;           // tope: el frame termina 8 px bajo laneBot

void baxterRatInitAll(void) {
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++) {
        rats[i].state      = RAT_INACTIVE;
        rats[i].sprite     = NULL;
        rats[i].boomSprite = NULL;
        rats[i].victim     = NULL;
    }
}

// Ratas que todavia tienen algo en pantalla (vivas O explotando). La usa el
// fin de la pelea: sin esto el nivel terminaba con la explosion de una rata a
// medio hacer y quedaba congelada durante el jingle.
u16 baxterRatBusyCount(void) {
    u16 n = 0;
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++)
        if (rats[i].state != RAT_INACTIVE) n++;
    return n;
}

u16 baxterRatAliveCount(void) {
    u16 n = 0;
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++)
        if (rats[i].state != RAT_INACTIVE && rats[i].state != RAT_BOOM &&
            rats[i].state != RAT_DEAD) n++;
    return n;
}

static u16 ratChaserCount(void) {
    u16 n = 0;
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++)
        if (rats[i].chaser && rats[i].state != RAT_INACTIVE &&
            rats[i].state != RAT_BOOM && rats[i].state != RAT_DEAD &&
            rats[i].state != RAT_HURT) n++;
    return n;
}

static void ratSetAnim(BaxterRat* r, u8 a, bool loop) {
    if (r->anim == a || !r->sprite) return;
    r->anim = a;
    SPR_setAutoAnimation(r->sprite, TRUE);
    SPR_setAnim(r->sprite, a);
    SPR_setAnimationLoop(r->sprite, loop);
}

// Caminatas de DOS filas (frente 0-1, costado 2-3): cada fila corre una vez y
// se pasa a la otra. 'base' es la primera fila (FRONT_A / SIDE_A).
static void ratSetWalk2(BaxterRat* r, u8 base) {
    if (r->anim == base || r->anim == (u8)(base + 1)) return;
    ratSetAnim(r, base, FALSE);
}

// Un frame de las caminatas de dos filas: al terminar una mitad, la otra.
static void ratWalk2Step(BaxterRat* r) {
    if (!r->sprite) return;
    u8 a = r->anim;
    if (a != RAT_ANIM_FRONT_A && a != RAT_ANIM_FRONT_B &&
        a != RAT_ANIM_SIDE_A  && a != RAT_ANIM_SIDE_B) return;
    if (!SPR_isAnimationDone(r->sprite)) return;
    ratSetAnim(r, (u8)(a ^ 1), FALSE);     // A <-> B (filas pares/impares)
}

static void ratRender(BaxterRat* r, s16 camX) {
    if (!r->sprite) return;
    SPR_setHFlip(r->sprite, (r->dir < 0));
    SPR_setPosition(r->sprite, r->x - camX - RAT_FRAME_W / 2, r->y - 36 - r->z - stageCamY);
    SPR_setDepth(r->sprite, (r->state == RAT_LATCH) ? (s16)(-(r->y) - 2) : (s16)-(r->y));
}

static void ratStartBoom(BaxterRat* r, s16 camX) {
    r->state     = RAT_BOOM;
    r->boomFrame = 0;
    r->boomTick  = 0;
    r->timer     = 0;
    if (r->sprite) { SPR_releaseSprite(r->sprite); r->sprite = NULL; }
    if (!r->boomSprite)
        r->boomSprite = SPR_addSprite(&rat_boom, -40, -40,
                                      TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    if (r->boomSprite) {
        SPR_setAutoAnimation(r->boomSprite, FALSE);
        SPR_setAnimAndFrame(r->boomSprite, 0, 0);
        SPR_setPosition(r->boomSprite, r->x - camX - 16, r->y - 36 - stageCamY);
        SPR_setDepth(r->boomSprite, -(r->y));
    }
}

// Suelta a la tortuga si la estaba mordiendo.
static void ratLetGo(BaxterRat* r) {
    if (r->victim && playerIsGrabbed(r->victim)) playerReleaseGrab(r->victim);
    r->victim = NULL;
}

static void ratKill(BaxterRat* r) {
    ratLetGo(r);
    r->state = RAT_DEAD;
    r->timer = 0;
    r->z     = 0;
    ratSetAnim(r, RAT_ANIM_DEAD, FALSE);   // tirada en el piso hasta explotar
    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                   SOUND_PCM_CH3, 12, FALSE, FALSE);
}

// Golpeada: sale despedida y despues muere (un golpe la mata).
static void ratFling(BaxterRat* r, s8 dir) {
    ratLetGo(r);
    r->state = RAT_HURT;
    r->timer = 0;
    r->dir   = (s8)-dir;                 // mira hacia quien le pego
    r->accX  = 0;
    ratSetAnim(r, RAT_ANIM_HURT, FALSE);
}

static bool ratCanBeHit(const BaxterRat* r) {
    return (r->state == RAT_WALK || r->state == RAT_PAUSE || r->state == RAT_JUMP);
}

static void ratPickSpot(BaxterRat* r) {
    s16 w = (s16)(ratArenaR - ratArenaL - 40);
    s16 h = (s16)(ratLaneBot - ratLaneTop);
    r->goX = (s16)(ratArenaL + 20 + ((w > 0) ? (s16)(random() % (u16)w) : 0));
    r->goY = (s16)(ratLaneTop + ((h > 0) ? (s16)(random() % (u16)h) : 0));
}

static void ratToWalk(BaxterRat* r) {
    r->state = RAT_WALK;
    r->timer = 0;
    r->z = 0;
    if (!r->chaser) ratPickSpot(r);
    ratSetWalk2(r, RAT_ANIM_SIDE_A);
}

// Sale de la panza de la nave y baja derecho.
static void ratSpawnFromShip(s16 x, s16 shipTop) {
    // Tambien la VRAM de sprites: sin lugar para la rata (y despues su
    // explosion), no sale.
    if (!sprVramFits(baxter_rat.maxNumTile)) return;
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++) {
        BaxterRat* r = &rats[i];
        if (r->state != RAT_INACTIVE) continue;
        r->sprite = SPR_addSprite(&baxter_rat, -60, -60,
                                  TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        if (!r->sprite) return;
        r->state = RAT_FALL;
        r->x     = x;
        // Cae en una lane al azar de la franja, no siempre abajo de todo.
        s16 span = (s16)(ratLaneBot - ratLaneTop - 16);
        r->landY = (s16)(ratLaneTop + 8 + ((span > 0) ? (s16)(random() % (u16)span) : 0));
        r->y     = r->landY;
        r->z     = (s16)(r->landY - (shipTop + BAXTER_FRAME_H - 12));   // sale de la panza
        if (r->z < 0) r->z = 0;
        r->dir   = -1;
        r->timer = 0;
        r->accX = r->accY = r->accZ = 0;
        r->anim  = 0xFF;
        r->victim = NULL;
        r->chaser = (ratChaserCount() < RAT_CHASERS) ? 1 : 0;
        ratSetAnim(r, RAT_ANIM_JUMP, FALSE);   // baja en la pose del salto
        if (r->sprite) { SPR_setAutoAnimation(r->sprite, FALSE); SPR_setFrame(r->sprite, 0); }
        return;
    }
}

static void ratStartJump(BaxterRat* r, s8 dir) {
    r->state = RAT_JUMP;
    r->dir   = dir;
    r->timer = 0;
    r->accX  = 0;
    r->anim  = 0xFF;
    ratSetAnim(r, RAT_ANIM_JUMP, FALSE);
    if (r->sprite) { SPR_setAutoAnimation(r->sprite, FALSE); SPR_setFrame(r->sprite, 0); }
}

void baxterRatUpdateAll(Player** pls, u8 nPl, s16 camX, BaxterTopAtFn topAt) {
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++) {
        BaxterRat* r = &rats[i];
        if (r->state == RAT_INACTIVE) continue;
        r->timer++;

        switch (r->state) {

        case RAT_FALL:
            r->z -= bxStep(&r->accZ, RAT_FALL_Q);
            if (r->z <= 0) ratToWalk(r);
            break;

        case RAT_WALK:
        case RAT_PAUSE: {
            Player* tgt = nearestPlayer(pls, nPl, r->x);
            s16 dX = (s16)(getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2 - r->x);
            s16 dY = (s16)(getPlayerY(tgt) - r->y);
            // Ataque: alineada con la tortuga y cerca -> salto con mordida.
            if (xabs(dY) <= RAT_ATK_DY && xabs(dX) <= RAT_ATK_DX &&
                !playerIsGrabbed(tgt) && playerCanBeHit(tgt)) {
                ratStartJump(r, (dX >= 0) ? 1 : -1);
                break;
            }
            if (r->state == RAT_PAUSE) {
                if (r->timer >= RAT_PAUSE_T) ratToWalk(r);
                break;
            }
            s16 gx, gy;
            if (r->chaser) {
                // Se pone en su lane y se acerca hasta RAT_ATK_DX.
                gx = (xabs(dX) > RAT_ATK_DX) ? (s16)(r->x + dX) : r->x;
                gy = (s16)(r->y + dY);
            } else {
                gx = r->goX; gy = r->goY;
            }
            s16 sx = bxStep(&r->accX, RAT_WALK_Q);
            s16 sy = bxStep(&r->accY, RAT_WALK_Q);
            if (gx > r->x)      { r->x += (gx - r->x < sx) ? gx - r->x : sx; r->dir = 1; }
            else if (gx < r->x) { r->x -= (r->x - gx < sx) ? r->x - gx : sx; r->dir = -1; }
            if (gy > r->y)      r->y += (gy - r->y < sy) ? gy - r->y : sy;
            else if (gy < r->y) r->y -= (r->y - gy < sy) ? r->y - gy : sy;
            if (r->chaser) r->dir = (dX >= 0) ? 1 : -1;
            if (!r->chaser && r->x == gx && r->y == gy) {
                r->state = RAT_PAUSE;           // llego: se queda quieta un rato
                r->timer = 0;
                ratSetAnim(r, RAT_ANIM_SIDE_A, FALSE);
                if (r->sprite) { SPR_setAutoAnimation(r->sprite, FALSE); SPR_setFrame(r->sprite, 0); }
                r->anim = 0xFF;
                break;
            }
            // Hacia arriba (fila 4) SOLO si va hacia arriba; si no, la
            // caminata de costado (filas 2-3).
            if (gy < r->y - 8 && xabs(gx - r->x) < 8) ratSetAnim(r, RAT_ANIM_WALK_UP, TRUE);
            else                                      ratSetWalk2(r, RAT_ANIM_SIDE_A);
            ratWalk2Step(r);
            break;
        }

        case RAT_JUMP: {
            // Salta hacia adelante: la primera mitad sube y la segunda baja y
            // muerde. La fila del salto (7 frames) repartida en lo que dura.
            r->x += r->dir * bxStep(&r->accX, RAT_JUMP_Q);
            u16 t = r->timer;
            r->z = (s16)((4 * RAT_JUMP_H * t * (RAT_JUMP_T - t)) / (RAT_JUMP_T * RAT_JUMP_T));
            if (r->z < 0) r->z = 0;
            if (r->sprite) {
                u16 f = (u16)((t * RAT_JUMP_FRAMES) / (RAT_JUMP_T + 1));
                if (f >= RAT_JUMP_FRAMES) f = RAT_JUMP_FRAMES - 1;
                SPR_setFrame(r->sprite, (s16)f);
            }
            if (t >= RAT_JUMP_T / 2) {
                for (u8 k = 0; k < nPl; k++) {
                    Player* p = pls[k];
                    if (!playerCanBeHit(p) || playerIsGrabbed(p)) continue;
                    if (xabs(getPlayerHurtCX(p) - r->x) > RAT_BITE_DX) continue;
                    if (xabs(getPlayerY(p) - r->y) > RAT_BITE_DY) continue;
                    playerFootGrab(p);
                    if (!playerIsGrabbed(p)) continue;
                    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                                   SOUND_PCM_CH2, 15, FALSE, FALSE);
                    r->state  = RAT_LATCH;
                    r->victim = p;
                    r->drain  = RAT_DRAIN_T;
                    r->timer  = 0;
                    break;
                }
                if (r->state == RAT_LATCH) break;
            }
            if (t >= RAT_JUMP_T) ratToWalk(r);
            break;
        }

        case RAT_LATCH: {
            // Prendida de la tortuga: le saca una barra cada RAT_DRAIN_T. Se
            // suelta cuando la tortuga zafa (mash) o cae.
            Player* p = r->victim;
            if (!p || !playerIsGrabbed(p)) {
                r->victim = NULL;
                ratFling(r, r->dir);
                break;
            }
            r->x = (s16)(getPlayerHurtCX(p) - r->dir * RAT_LATCH_DX);
            r->y = getPlayerY(p);
            r->z = RAT_LATCH_Z;
            if (r->sprite) SPR_setFrame(r->sprite, RAT_JUMP_FRAMES - 1);
            if (--r->drain == 0) { r->drain = RAT_DRAIN_T; playerElectroDrain(p); }
            break;
        }

        case RAT_HURT:
            // Despedida hacia atras y despues muere.
            r->x -= r->dir * bxStep(&r->accX, RAT_FLING_Q);
            r->z = 0;
            if (r->timer >= RAT_HURT_T) ratKill(r);
            break;

        case RAT_DEAD:
            if (r->timer > RAT_DEAD_T) ratStartBoom(r, camX);
            break;

        case RAT_BOOM:
            if (++r->boomTick >= 4) {
                r->boomTick = 0;
                if (r->boomFrame < 6) {
                    r->boomFrame++;
                    if (r->boomSprite) SPR_setFrame(r->boomSprite, r->boomFrame);
                }
            }
            if (r->boomSprite)
                SPR_setPosition(r->boomSprite, r->x - camX - 16, r->y - 36 - stageCamY);
            if (r->timer > 44) {
                if (r->boomSprite) { SPR_releaseSprite(r->boomSprite); r->boomSprite = NULL; }
                r->state = RAT_INACTIVE;
            }
            continue;

        default:
            break;
        }

        // Dentro de la arena y fuera de la pared de la cloaca.
        r->x = xclamp(r->x, (s16)(camX + 20), (s16)(camX + 300));
        s16 top = ratLaneTop;
        if (topAt) { s16 t = topAt(r->x); if (t > top) top = t; }
        if (r->state != RAT_FALL && r->state != RAT_LATCH)
            r->y = xclamp(r->y, top, ratLaneBot);
        ratRender(r, camX);
    }
}

void baxterRatPlayerHits(Player** pls, u8 nPl) {
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++) {
        BaxterRat* r = &rats[i];
        if (!ratCanBeHit(r)) continue;
        for (u8 k = 0; k < nPl; k++) {
            if (!playerAttackHitsBox(pls[k], r->x, r->y, RAT_HALF_W, RAT_BODY_H))
                continue;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                           SOUND_PCM_CH2, 15, FALSE, FALSE);
            s16 px = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
            ratFling(r, (px <= r->x) ? 1 : -1);
            addPlayerScore(pls[k], 1);
            break;
        }
    }
}

void baxterRatReleaseAll(void) {
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++) {
        ratLetGo(&rats[i]);
        if (rats[i].sprite)     { SPR_releaseSprite(rats[i].sprite);     rats[i].sprite = NULL; }
        if (rats[i].boomSprite) { SPR_releaseSprite(rats[i].boomSprite); rats[i].boomSprite = NULL; }
        rats[i].state = RAT_INACTIVE;
    }
}

// ---------------------------------------------------------------------------
// LA NAVE
// ---------------------------------------------------------------------------
void baxterInit(Baxter* b) {
    memset(b, 0, sizeof(Baxter));
    b->sprite     = NULL;
    for (u16 q = 0; q < BAXTER_BOOM_PARTS; q++) b->boomSprite[q] = NULL;
    b->state      = BAXTER_INACTIVE;
    b->hp         = BAXTER_HP;
    b->anim       = 0xFF;
}

void baxterSpawn(Baxter* b, s16 arenaLeft, s16 arenaRight, s16 laneTop, s16 laneBot) {
    if (!b->sprite)
        b->sprite = SPR_addSprite(&baxter_boss, -80, -80,
                                  TILE_ATTR(PAL3, TRUE, FALSE, FALSE));
    // La paleta de la nave, las ratas y las explosiones (es una sola).
    PAL_setPalette(PAL3, baxter_boss.palette->data, DMA);
    b->arenaLeft  = arenaLeft;
    b->arenaRight = arenaRight;
    ratLaneTop = laneTop;
    ratLaneBot = laneBot;
    ratArenaL  = arenaLeft;
    ratArenaR  = arenaRight;
    bxYMax = (s16)(laneBot - BAXTER_FRAME_H + 8);
    if (bxYMax < 128) bxYMax = 128;
    // Centro del vuelo: el medio de la arena, a media altura.
    b->cx = (s16)((arenaLeft + arenaRight) / 2);
    b->cy = (s16)((BAXTER_Y_MIN + bxYMax) / 2);
    b->ampY = (s16)((bxYMax - BAXTER_Y_MIN) / 2);
    if (b->ampY > BAXTER_AMP_Y) b->ampY = BAXTER_AMP_Y;
    // Entra en diagonal desde abajo a la derecha.
    b->state = BAXTER_ENTER;
    b->hp    = BAXTER_HP;
    b->x     = (s16)(arenaRight + 40);
    b->y     = (s16)(b->cy + BAXTER_ENTER_DY);
    b->accX = b->accY = 0;
    b->timer = 0;
    b->flightT = 0;
    b->dropT   = 0;
    b->hurtFlash = 0;
    b->invuln = 0;
    b->anim = 0xFF;
    if (b->sprite) {
        SPR_setAutoAnimation(b->sprite, TRUE);
        SPR_setAnim(b->sprite, BAXTER_ANIM_FLY);
        SPR_setAnimationLoop(b->sprite, TRUE);
        SPR_setHFlip(b->sprite, TRUE);        // el arte mira a la izquierda
    }
}

bool baxterIsGone(const Baxter* b)   { return b->state == BAXTER_GONE; }
bool baxterCanBeHit(const Baxter* b) {
    return (b->state == BAXTER_FLY);
}

// Junta los 4 cuartos de la explosion alrededor del centro de la nave.
static void baxterBoomPlace(Baxter* b, s16 camX) {
    s16 cx = (s16)(b->x - camX - BAXTER_BOOM_HALF);
    s16 cy = (s16)(b->y + BAXTER_FRAME_H / 2 - stageCamY - BAXTER_BOOM_HALF);
    for (u16 q = 0; q < BAXTER_BOOM_PARTS; q++)
        if (b->boomSprite[q])
            SPR_setPosition(b->boomSprite[q],
                            (s16)(cx + (q & 1) * BAXTER_BOOM_HALF),
                            (s16)(cy + (q >> 1) * BAXTER_BOOM_HALF));
}

static void baxterBoomRelease(Baxter* b) {
    for (u16 q = 0; q < BAXTER_BOOM_PARTS; q++)
        if (b->boomSprite[q]) { SPR_releaseSprite(b->boomSprite[q]); b->boomSprite[q] = NULL; }
}

// (30/09) La explosion va en CUATRO sprites de 64x64 (cuartos del lienzo de
// 128x128, ver BAXTER_BOOM_PARTS y tools/gen_baxter_boom.py). Antes era uno
// solo declarado de 48x72 sobre una tira que no es una grilla: se veian
// tajadas sueltas de la explosion.
//
// VRAM: los 4 cuartos distintos son 4 x 64 = 256 tiles, y en la cloaca (con la
// camara vertical quedan 382 para sprites) al morir Baxter hay ~210 libres con
// una tortuga y ~70 con dos. Asi que la explosion se arma con lo que haya,
// COMPARTIENDO tiles entre cuartos (los que comparten apuntan al mismo bloque
// de VRAM, sin subir tiles propios, y se dibujan espejados):
//   FULL    256 tiles  los 4 cuartos con su propio arte
//   HALF    128        los 2 de la izquierda con su arte; los de la derecha
//                      son su espejo horizontal
//   QUARTER  64        el de arriba a la izquierda y sus 3 espejos
// Se prueba en ese orden; si ni el QUARTER entra, no hay explosion (el sonido
// y el final de la pelea siguen igual).
static Sprite* boomMaster(u16 anim) {
    Sprite* s = SPR_addSpriteEx(&baxter_boom, -128, -128,
                                TILE_ATTR(PAL3, TRUE, FALSE, FALSE),
                                SPR_FLAG_AUTO_VISIBILITY | SPR_FLAG_AUTO_VRAM_ALLOC |
                                SPR_FLAG_AUTO_TILE_UPLOAD |
                                SPR_FLAG_DISABLE_DELAYED_FRAME_UPDATE);
    if (s) {
        SPR_setAutoAnimation(s, FALSE);
        SPR_setAnimAndFrame(s, (s16)anim, 0);
        SPR_setDepth(s, SPR_MIN_DEPTH);
    }
    return s;
}

// Un cuarto que reusa los tiles de 'm' (mismo anim y frame), espejado.
static Sprite* boomMirror(const Sprite* m, u16 anim, bool hflip, bool vflip) {
    if (!m) return NULL;
    u16 idx = m->attribut & TILE_INDEX_MASK;
    Sprite* s = SPR_addSpriteEx(&baxter_boom, -128, -128,
                                TILE_ATTR_FULL(PAL3, TRUE, FALSE, FALSE, idx),
                                SPR_FLAG_AUTO_VISIBILITY);
    if (s) {
        SPR_setAutoAnimation(s, FALSE);
        SPR_setAnimAndFrame(s, (s16)anim, 0);
        SPR_setHFlip(s, hflip);
        SPR_setVFlip(s, vflip);
        SPR_setDepth(s, SPR_MIN_DEPTH);
    }
    return s;
}

static void baxterBoomRelease(Baxter* b);

static void baxterBoomCreate(Baxter* b) {
    Sprite** sp = b->boomSprite;
    // Los espejos apuntan a la VRAM de su dueno: mientras dure la explosion
    // nadie puede desfragmentar (moveria al dueno y no a los espejos).
    sprDefragLock = 1;
    const u16 T = baxter_boom.maxNumTile;
    // FULL
    if (SPR_getFreeVRAM() >= 4 * T) {
        for (u16 q = 0; q < BAXTER_BOOM_PARTS; q++) {
            b->boomAnim[q] = (u8)q;
            sp[q] = boomMaster(q);
        }
        if (sp[0] && sp[1] && sp[2] && sp[3]) return;
        baxterBoomRelease(b);
    }
    // HALF: izquierda con arte propio, derecha espejada
    if (SPR_getFreeVRAM() >= 2 * T) {
        sp[0] = boomMaster(0);
        sp[2] = boomMaster(2);
        if (sp[0] && sp[2]) {
            sp[1] = boomMirror(sp[0], 0, TRUE, FALSE);
            sp[3] = boomMirror(sp[2], 2, TRUE, FALSE);
            b->boomAnim[0] = b->boomAnim[1] = 0;
            b->boomAnim[2] = b->boomAnim[3] = 2;
            return;
        }
        baxterBoomRelease(b);
    }
    // QUARTER: arriba a la izquierda y sus tres espejos
    sp[0] = boomMaster(0);
    if (!sp[0]) return;
    sp[1] = boomMirror(sp[0], 0, TRUE,  FALSE);
    sp[2] = boomMirror(sp[0], 0, FALSE, TRUE);
    sp[3] = boomMirror(sp[0], 0, TRUE,  TRUE);
    for (u16 q = 0; q < BAXTER_BOOM_PARTS; q++) b->boomAnim[q] = 0;
}

static void baxterKill(Baxter* b) {
    b->hp    = 0;
    b->state = BAXTER_DEAD;
    b->boomFrame = 0;
    b->boomTick  = 0;
    if (b->sprite) { SPR_releaseSprite(b->sprite); b->sprite = NULL; }
    baxterBoomCreate(b);
    baxterBoomPlace(b, b->camX);
    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                   SOUND_PCM_CH3, 15, FALSE, FALSE);
    // Con la nave explotan todas las ratas (tambien la que muerde).
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++)
        if (rats[i].state != RAT_INACTIVE && rats[i].state != RAT_BOOM &&
            rats[i].state != RAT_DEAD)
            ratKill(&rats[i]);
}

bool baxterPlayerHits(Baxter* b, Player** pls, u8 nPl, s8* killer) {
    if (!baxterCanBeHit(b) || b->invuln) return FALSE;
    for (u8 k = 0; k < nPl; k++) {
        if (!playerAttackHitsFlying(pls[k], b->x, b->y, (s16)(b->y + BAXTER_BODY_H),
                                    BAXTER_HALF_W))
            continue;
        // Cualquier golpe saca uno.
        XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
        b->hp -= 1;
        b->hurtFlash = BAXTER_INVULN;
        b->invuln    = BAXTER_INVULN;
        if (b->hp <= 0) {
            baxterKill(b);
            if (killer) *killer = (s8)k;
            return TRUE;
        }
        return FALSE;
    }
    return FALSE;
}

void baxterUpdate(Baxter* b, Player** pls, u8 nPl, s16 camX) {
    (void)pls; (void)nPl;
    if (b->state == BAXTER_INACTIVE || b->state == BAXTER_GONE) return;
    b->camX = camX;
    b->timer++;
    if (b->hurtFlash) b->hurtFlash--;
    if (b->invuln)    b->invuln--;

    switch (b->state) {

    case BAXTER_ENTER:
        // En diagonal hacia arriba a la izquierda hasta el centro del vuelo.
        b->x -= bxStep(&b->accX, BAXTER_ENTER_XQ);
        b->y -= bxStep(&b->accY, BAXTER_ENTER_YQ);
        if (b->x <= b->cx || b->y <= b->cy) {
            b->state = BAXTER_FLY;
            b->timer = 0;
            // Arranca la Lissajous donde esta, sin saltos.
            b->cx = b->x;
            b->cy = b->y;
            b->flightT = 0;
            b->dropT = 0;
        }
        break;

    case BAXTER_FLY: {
        // Lissajous: de lado a lado (BAXTER_PER_X) y arriba-abajo
        // (BAXTER_PER_Y), cada una con su periodo.
        b->flightT++;
        u8 phX = (u8)(((u32)b->flightT * 256) / BAXTER_PER_X);
        u8 phY = (u8)(((u32)b->flightT * 256) / BAXTER_PER_Y);
        if (b->flightT >= (u16)(BAXTER_PER_X * BAXTER_PER_Y / 30)) b->flightT = 0;
        s16 tx = (s16)(b->cx + ((bxSin(phX) * BAXTER_AMP_X) >> 8));
        s16 ty = (s16)(b->cy + ((bxSin(phY) * b->ampY) >> 8));
        // De a poco hacia el centro de la arena (al entrar quedo corrido).
        s16 mid = (s16)((b->arenaLeft + b->arenaRight) / 2);
        if ((b->timer & 3) == 0) b->cx = (b->cx < mid) ? b->cx + 1 : ((b->cx > mid) ? b->cx - 1 : b->cx);
        s16 midY = (s16)((BAXTER_Y_MIN + bxYMax) / 2);
        if ((b->timer & 3) == 0) b->cy = (b->cy < midY) ? b->cy + 1 : ((b->cy > midY) ? b->cy - 1 : b->cy);
        b->x = xclamp(tx, (s16)(b->arenaLeft + 8), (s16)(b->arenaRight - 8));
        b->y = xclamp(ty, BAXTER_Y_MIN, bxYMax);
        // Suelta un Mouser cada BAXTER_DROP_T (abre la portezuela un rato).
        if (++b->dropT >= BAXTER_DROP_T) {
            b->dropT = 0;
            if (baxterRatAliveCount() < BAXTER_RATS_ALIVE)
                ratSpawnFromShip(b->x, b->y);
        }
        break;
    }

    case BAXTER_DEAD:
        if (++b->boomTick >= BAXTER_BOOM_TICKS) {
            b->boomTick = 0;
            if (++b->boomFrame >= BAXTER_BOOM_FRAMES) {
                baxterBoomRelease(b);
                sprDefragLock = 0;
                b->state = BAXTER_GONE;
                return;
            }
            for (u16 q = 0; q < BAXTER_BOOM_PARTS; q++)
                if (b->boomSprite[q])
                    SPR_setAnimAndFrame(b->boomSprite[q], b->boomAnim[q], b->boomFrame);
        }
        baxterBoomPlace(b, camX);
        return;

    default:
        break;
    }

    if (!b->sprite) return;

    // Golpe: flash rojo. Poca vida: parpadea alternando con el vuelo.
    bool lowHp = (b->hp > 0 && b->hp <= BAXTER_LOW_HP);
    // La portezuela se abre BAXTER_DOOR_T frames antes de cada Mouser
    // (primero entreabierta, despues abierta).
    s16 toDrop = (s16)(BAXTER_DROP_T - b->dropT);
    bool door  = (b->state == BAXTER_FLY && toDrop <= BAXTER_DOOR_T);
    u8 doorFr  = (toDrop <= BAXTER_DOOR_T / 2) ? 1 : 0;
    u8 want;
    if (b->hurtFlash || (lowHp && ((b->timer >> 3) & 1))) want = BAXTER_ANIM_HURT;
    else if (door) want = BAXTER_ANIM_DOOR;
    else want = BAXTER_ANIM_FLY;

    if (want == BAXTER_ANIM_FLY) {
        if (b->anim != BAXTER_ANIM_FLY) {
            SPR_setAutoAnimation(b->sprite, TRUE);
            SPR_setAnimationLoop(b->sprite, TRUE);
            SPR_setAnim(b->sprite, BAXTER_ANIM_FLY);
        }
    } else {
        SPR_setAutoAnimation(b->sprite, FALSE);
        SPR_setAnimAndFrame(b->sprite, want, (want == BAXTER_ANIM_DOOR) ? doorFr : 0);
    }
    b->anim = want;
    SPR_setHFlip(b->sprite, TRUE);
    SPR_setPosition(b->sprite, b->x - camX - BAXTER_FRAME_W / 2, b->y - stageCamY);
    SPR_setDepth(b->sprite, SPR_MIN_DEPTH);      // por encima de ratas y tortugas
}

void baxterRelease(Baxter* b) {
    if (b->sprite)     { SPR_releaseSprite(b->sprite);     b->sprite = NULL; }
    baxterBoomRelease(b);
    sprDefragLock = 0;
    b->state = BAXTER_INACTIVE;
    baxterRatReleaseAll();
}
