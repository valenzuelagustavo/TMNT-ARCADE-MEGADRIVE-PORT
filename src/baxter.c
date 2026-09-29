#include "baxter.h"
#include "audio.h"    // hit_turtles, foot_soldier_explode
#include "enemy.h"    // sprVramFits / sprDefragLock (30/09)

// ===========================================================================
// BAXTER + RATAS — ver baxter.h
// ===========================================================================
// La logica de vuelo, de la portezuela y de las ratas es la del companero
// (Ray Project); lo que cambio al portarla esta marcado con (port).
// ===========================================================================

static s16 xabs(s16 v)                 { return (v < 0) ? -v : v; }
static s16 xclamp(s16 v, s16 a, s16 b) { return (v < a) ? a : ((v > b) ? b : v); }

static s16 rsin(u16 t) {   // seno de tabla ~[-256..256]
    static const s16 tab[16] = { 0, 100, 191, 256, 256, 191, 100, 0,
                                 -100, -191, -256, -256, -191, -100, 0, 0 };
    return tab[(t >> 2) & 15];
}

// (port) El jugador EN JUEGO mas cercano en X; si no queda ninguno, el P1.
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
// RATAS
// ---------------------------------------------------------------------------
typedef enum {
    RAT_INACTIVE, RAT_FALL, RAT_WALK, RAT_BITE, RAT_JUMP, RAT_HURT, RAT_DEAD, RAT_BOOM
} RatState;

typedef struct {
    Sprite*  sprite;
    RatState state;
    s16      x;            // centro (mundo)
    s16      y;            // pies (lane)
    s16      z;            // (port) altura del salto/caida, solo visual
    s8       dir;
    s16      hp;
    u16      timer;
    u8       biteCooldown;
    s16      vxq, vzq;     // velocidades en 1/4 px
    s16      landY;        // lane donde aterriza al caer de la nave
    u8       anim;
    Sprite*  boomSprite;
    u8       boomFrame;
    u8       boomTick;
} BaxterRat;

static BaxterRat rats[MAX_BAXTER_RATS];
static s16 ratLaneTop, ratLaneBot;

// (29/09) ALTURAS DE VUELO. Los numeros de Ray (esquina de abajo en y=118,
// tope 128) estaban hechos para la vereda sola: con el canal del sewer nuevo
// (pies hasta 216) la nave nunca bajaba mas alla del escalon y parecia que se
// trababa en el. Es una maquina VOLADORA: ahora la esquina de abajo, el centro
// de la vuelta alrededor del jugador y el tope salen del piso de la arena
// (laneBot de baxterSpawn), asi cruza por encima del agua como en el arcade.
// Todo es la Y del BORDE DE ARRIBA del frame (72 px), en mundo.
#define BAXTER_Y_MIN        44      // lo mas alto (sin cambios)
#define BAXTER_Y_TOP        56      // esquina de arriba (sin cambios)
#define BAXTER_Y_LOW_GAP     8      // la esquina de abajo, 8 px sobre el tope
static s16 bxYMax  = 128;           // tope: el frame termina 8 px bajo laneBot
static s16 bxYLow  = 118;           // esquina de abajo
static s16 bxYOrb  = 88;            // centro de la vuelta alrededor del jugador

void baxterRatInitAll(void) {
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++) {
        rats[i].state      = RAT_INACTIVE;
        rats[i].sprite     = NULL;
        rats[i].boomSprite = NULL;
    }
}

// (30/09) Ratas que todavia tienen algo en pantalla (vivas O explotando). La
// usa el fin de la pelea: sin esto el nivel terminaba con la explosion de una
// rata a medio hacer y quedaba congelada durante el jingle.
u16 baxterRatBusyCount(void) {
    u16 n = 0;
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++)
        if (rats[i].state != RAT_INACTIVE) n++;
    return n;
}

u16 baxterRatAliveCount(void) {
    u16 n = 0;
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++)
        if (rats[i].state != RAT_INACTIVE && rats[i].state != RAT_BOOM) n++;
    return n;
}

static void ratSetAnim(BaxterRat* r, u8 a, bool loop) {
    if (r->anim == a || !r->sprite) return;
    r->anim = a;
    SPR_setAutoAnimation(r->sprite, TRUE);
    SPR_setAnim(r->sprite, a);
    SPR_setAnimationLoop(r->sprite, loop);
}

static void ratRender(BaxterRat* r, s16 camX) {
    if (!r->sprite) return;
    SPR_setHFlip(r->sprite, (r->dir < 0));
    SPR_setPosition(r->sprite, r->x - camX - RAT_FRAME_W / 2, r->y - 36 - r->z - stageCamY);
    SPR_setDepth(r->sprite, -(r->y));
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

static void ratKill(BaxterRat* r) {
    r->state = RAT_DEAD;
    r->timer = 0;
    r->vzq   = 6 * 4;       // salta un poco para arriba antes de caer
    ratSetAnim(r, RAT_ANIM_DEAD, FALSE);
    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                   SOUND_PCM_CH3, 12, FALSE, FALSE);
}

static bool ratCanBeHit(const BaxterRat* r) {
    // (port) HURT no: asi un swing pega UNA vez y no una por frame.
    return (r->state == RAT_WALK || r->state == RAT_BITE || r->state == RAT_JUMP);
}

static void ratDamage(BaxterRat* r, s16 dmg) {
    r->hp -= dmg;
    if (r->hp <= 0) { ratKill(r); return; }
    r->state = RAT_HURT;
    r->timer = RAT_HURT_TICKS;
    r->z     = 0;
    ratSetAnim(r, RAT_ANIM_HURT, FALSE);
}

static void ratSpawnFromShip(s16 x, s16 shipTop) {
    // (port) Tambien la VRAM de sprites: sin lugar para la rata (y despues su
    // explosion), no sale. La nave vuelve a intentarlo en la proxima parada.
    if (!sprVramFits(baxter_rat.maxNumTile)) return;
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++) {
        BaxterRat* r = &rats[i];
        if (r->state != RAT_INACTIVE) continue;
        r->sprite = SPR_addSprite(&baxter_rat, -60, -60,
                                  TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        if (!r->sprite) return;
        r->state = RAT_FALL;
        r->x     = x;
        // (port) Cae en una lane al azar de la franja, no siempre abajo de todo.
        s16 span = (s16)(ratLaneBot - ratLaneTop - 16);
        r->landY = (s16)(ratLaneTop + 8 + ((span > 0) ? (s16)(random() % (u16)span) : 0));
        r->y     = r->landY;
        r->z     = (s16)(r->landY - (shipTop + BAXTER_FRAME_H - 12));   // sale de la panza
        if (r->z < 0) r->z = 0;
        r->dir   = -1;
        r->hp    = RAT_HP;
        r->timer = 0;
        r->biteCooldown = 0;
        r->vxq = 0;
        r->vzq = 0;
        r->anim = 0xFF;
        ratSetAnim(r, RAT_ANIM_FALL, TRUE);
        return;
    }
}

void baxterRatUpdateAll(Player** pls, u8 nPl, s16 camX, BaxterTopAtFn topAt) {
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++) {
        BaxterRat* r = &rats[i];
        if (r->state == RAT_INACTIVE) continue;

        switch (r->state) {

        case RAT_FALL:
            r->vzq -= 4;                       // gravedad
            r->z   += r->vzq >> 2;
            if (r->z <= 0) {
                r->z = 0;
                r->state = RAT_WALK;
                r->timer = 0;
                ratSetAnim(r, RAT_ANIM_WALK_SIDE, TRUE);
            }
            break;

        case RAT_WALK: {
            Player* tgt = nearestPlayer(pls, nPl, r->x);
            s16 dX = (s16)(getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2 - r->x);
            s16 dY = (s16)(getPlayerY(tgt) - r->y);
            if (r->biteCooldown) r->biteCooldown--;

            if (xabs(dX) <= RAT_BITE_RANGE && xabs(dY) <= RAT_BITE_Y && !r->biteCooldown) {
                r->state = RAT_BITE;
                r->timer = 0;
                r->dir = (dX >= 0) ? 1 : -1;
                ratSetAnim(r, RAT_ANIM_BITE, FALSE);
                break;
            }
            if (xabs(dX) >= RAT_JUMP_MIN_DX && xabs(dX) <= 110 && xabs(dY) <= 16 &&
                !(r->timer & 63) && !r->biteCooldown) {
                r->state = RAT_JUMP;
                r->dir = (dX >= 0) ? 1 : -1;
                r->vxq = xclamp((s16)(dX / 4), -40, 40);
                r->vzq = 8 * 4;
                ratSetAnim(r, RAT_ANIM_JUMP, TRUE);
                break;
            }
            r->timer++;
            bool step = (r->timer & 1);
            if (xabs(dX) > 4) {
                if (step) r->x += (dX >= 0) ? RAT_SPEED : -RAT_SPEED;
                r->dir = (dX >= 0) ? 1 : -1;
            }
            if (xabs(dY) > 4) {
                if (step) r->y += (dY >= 0) ? 1 : -1;
                ratSetAnim(r, (xabs(dY) >= 10) ? RAT_ANIM_WALK_UP : RAT_ANIM_WALK_SIDE, TRUE);
            } else {
                ratSetAnim(r, (xabs(dX) > 8) ? RAT_ANIM_WALK_SIDE : RAT_ANIM_WALK_FRONT, TRUE);
            }
            break;
        }

        case RAT_BITE:
            r->timer++;
            if (r->timer == 8) {
                for (u8 k = 0; k < nPl; k++) {
                    Player* p = pls[k];
                    if (!playerCanBeHit(p)) continue;
                    s16 px = (s16)(getPlayerWorldX(p) + PLAYER_SPRITE_W / 2);
                    if (xabs(px - r->x) > RAT_BITE_RANGE + 6) continue;
                    if (xabs(getPlayerY(p) - r->y) > RAT_BITE_Y + 10) continue;
                    playerHitBars(p, r->x, RAT_BITE_DMG);
                    XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                                   SOUND_PCM_CH2, 15, FALSE, FALSE);
                    break;
                }
            } else if (r->timer > 20) {
                r->state = RAT_WALK;
                r->biteCooldown = 50;
                r->timer = 0;
                ratSetAnim(r, RAT_ANIM_WALK_SIDE, TRUE);
            }
            break;

        case RAT_JUMP: {
            // (port) El salto es ALTURA (z), no un cambio de lane.
            r->x   += r->vxq >> 2;
            r->vzq -= 4;
            r->z   += r->vzq >> 2;
            for (u8 k = 0; k < nPl; k++) {
                Player* p = pls[k];
                if (!playerCanBeHit(p)) continue;
                s16 px = (s16)(getPlayerWorldX(p) + PLAYER_SPRITE_W / 2);
                if (xabs(px - r->x) >= 26 || xabs(getPlayerY(p) - r->y) >= 16) continue;
                playerHitBars(p, r->x, RAT_JUMP_DMG);
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                               SOUND_PCM_CH2, 15, FALSE, FALSE);
                if (r->vzq > 0) r->vzq = 0;        // corta el salto
                break;
            }
            if (r->z <= 0) {
                r->z = 0;
                r->state = RAT_WALK;
                r->biteCooldown = 60;
                r->timer = 0;
                ratSetAnim(r, RAT_ANIM_WALK_SIDE, TRUE);
            }
            break;
        }

        case RAT_HURT:
            if (r->timer) r->timer--;
            else {
                r->state = RAT_WALK;
                r->timer = 0;
                ratSetAnim(r, RAT_ANIM_WALK_SIDE, TRUE);
            }
            break;

        case RAT_DEAD:
            r->vzq -= 4;
            r->z   += r->vzq >> 2;
            if (r->z < 0) r->z = 0;
            if (++r->timer > 22) ratStartBoom(r, camX);
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
            if (++r->timer > 44) {
                if (r->boomSprite) { SPR_releaseSprite(r->boomSprite); r->boomSprite = NULL; }
                r->state = RAT_INACTIVE;
            }
            continue;

        default:
            break;
        }

        // (port) Dentro de la arena y fuera de la pared de la cloaca.
        r->x = xclamp(r->x, (s16)(camX + 20), (s16)(camX + 300));
        s16 top = ratLaneTop;
        if (topAt) { s16 t = topAt(r->x); if (t > top) top = t; }
        if (r->state != RAT_FALL) r->y = xclamp(r->y, top, ratLaneBot);
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
            s16 dmg = isPlayerSpecialAttack(pls[k]) ? RAT_HP : 1;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                           SOUND_PCM_CH2, 15, FALSE, FALSE);
            ratDamage(r, dmg);
            if (r->state == RAT_DEAD) addPlayerScore(pls[k], 1);
            break;
        }
    }
}

void baxterRatReleaseAll(void) {
    for (u16 i = 0; i < MAX_BAXTER_RATS; i++) {
        if (rats[i].sprite)     { SPR_releaseSprite(rats[i].sprite);     rats[i].sprite = NULL; }
        if (rats[i].boomSprite) { SPR_releaseSprite(rats[i].boomSprite); rats[i].boomSprite = NULL; }
        rats[i].state = RAT_INACTIVE;
    }
}

// ---------------------------------------------------------------------------
// LA NAVE
// ---------------------------------------------------------------------------
void baxterInit(Baxter* b) {
    b->sprite     = NULL;
    for (u16 q = 0; q < BAXTER_BOOM_PARTS; q++) b->boomSprite[q] = NULL;
    b->state      = BAXTER_INACTIVE;
    b->hp         = BAXTER_HP;
    b->anim       = 0xFF;
    b->invuln     = 0;
    b->hurtFlash  = 0;
}

void baxterSpawn(Baxter* b, s16 arenaLeft, s16 arenaRight, s16 laneTop, s16 laneBot) {
    if (!b->sprite)
        b->sprite = SPR_addSprite(&baxter_boss, -80, -80,
                                  TILE_ATTR(PAL3, TRUE, FALSE, FALSE));
    // La paleta de la nave, las ratas y las explosiones (es una sola).
    PAL_setPalette(PAL3, baxter_boss.palette->data, DMA);
    b->state = BAXTER_ENTER;
    b->hp    = BAXTER_HP;
    b->x     = (s16)(arenaLeft - 40);          // entra por la izquierda
    b->y     = 48;
    b->timer = 0;
    b->flightT = 0;
    b->doorPhase = 0;
    b->dropsThisCycle = 0;
    b->hurtFlash = 0;
    b->invuln = 0;
    b->orbit = 0;
    b->legs  = 2;
    b->corner = 3;       // (30/09) la primera esquina despues de la vuelta: 0
    b->arenaLeft  = arenaLeft;
    b->arenaRight = arenaRight;
    ratLaneTop = laneTop;
    ratLaneBot = laneBot;
    // Alturas de vuelo a partir del piso de la arena (ver BAXTER_Y_*). Nunca
    // menos que las de Ray, por si otra arena tuviera el piso mas arriba.
    bxYMax = (s16)(laneBot - BAXTER_FRAME_H + 8);
    if (bxYMax < 128) bxYMax = 128;
    bxYLow = (s16)(bxYMax - BAXTER_Y_LOW_GAP);
    bxYOrb = (s16)((BAXTER_Y_TOP + bxYLow) / 2);
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
    return (b->state == BAXTER_ENTER || b->state == BAXTER_DROP || b->state == BAXTER_FLY);
}

// Siguiente destino: diagonales de esquina a esquina; cada 3 cruces, una
// vuelta alrededor del jugador.
// (30/09) Las esquinas van en ORDEN FIJO: arriba-izq, abajo-der, arriba-der,
// abajo-izq (dos diagonales y dos verticales). Antes el lado salia de donde
// estaba la nave y el alto de la paridad de 'legs', y con la vuelta alrededor
// de la tortuga en el medio la combinacion dejaba afuera siempre la misma
// esquina: nunca iba arriba a la izquierda.
static const s8 bxCornerRight[4] = { 0, 1, 1, 0 };
static const s8 bxCornerTop[4]   = { 1, 0, 1, 0 };

static void baxterPickTarget(Baxter* b, s16 playerX) {
    b->legs++;
    if (b->legs >= 3) {
        b->legs  = 0;
        b->orbit = 1;
        b->tgx   = xclamp(playerX, (s16)(b->arenaLeft + 60), (s16)(b->arenaRight - 60));
        b->tgy   = bxYOrb;
        return;
    }
    b->orbit = 0;
    b->corner = (u8)((b->corner + 1) & 3);
    bool right = bxCornerRight[b->corner];
    bool top   = bxCornerTop[b->corner];
    b->tgx = right ? (s16)(b->arenaRight - 34) : (s16)(b->arenaLeft + 34);
    b->tgy = top ? BAXTER_Y_TOP : bxYLow;
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
        s16 dmg = isPlayerSpecialAttack(pls[k]) ? BAXTER_SPECIAL_DMG : 1;
        XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
        b->hp -= dmg;
        b->hurtFlash = 8;
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
    if (b->state == BAXTER_INACTIVE || b->state == BAXTER_GONE) return;
    b->camX = camX;
    s16 pxWorld = (s16)(getPlayerWorldX(nearestPlayer(pls, nPl, b->x)) + PLAYER_SPRITE_W / 2);
    b->timer++;
    if (b->hurtFlash) b->hurtFlash--;
    if (b->invuln)    b->invuln--;

    switch (b->state) {

    case BAXTER_ENTER:
        b->x += 2;
        b->y = (s16)(80 + ((rsin(b->timer) * 3) >> 8));   // (30/09) flote suave
        if (b->x >= b->arenaLeft + 50) {
            b->x = (s16)(b->arenaLeft + 50);
            baxterPickTarget(b, (s16)(b->arenaLeft + 60));
            b->state = BAXTER_FLY;
            b->timer = 0;
        }
        break;

    case BAXTER_DROP: {
        // (30/09) Flote SUAVE: +-3 px con periodo de 64 frames y a 1 px por
        // frame como mucho. Antes era +-4 px cada 16 frames (la tabla del seno
        // se recorria entera en 16) con un acercamiento de 1/4 que redondeaba
        // distinto para arriba que para abajo: la nave temblaba parada, y con
        // la esquina de abajo a la altura del escalon parecia que chocaba.
        s16 hoverY = (s16)(b->tgy + ((rsin(b->timer) * 3) >> 8));
        if (b->y < hoverY) b->y++;
        else if (b->y > hoverY) b->y--;
        if (b->doorPhase == 0) {
            if (b->timer > 22) { b->doorPhase = 1; b->timer = 0; }
        } else if (b->doorPhase == 1) {
            if (b->timer > 16) {
                b->doorPhase = 2;
                b->timer = 0;
                if (baxterRatAliveCount() < BAXTER_RATS_ALIVE)
                    ratSpawnFromShip(b->x, b->y);
            }
        } else if (b->timer > 24) {
            b->dropsThisCycle++;
            bool full = (baxterRatAliveCount() >= BAXTER_RATS_ALIVE);
            if (!full && b->dropsThisCycle < 2) {
                b->doorPhase = 0;
                b->timer = 0;
            } else {
                b->state = BAXTER_FLY;
                b->doorPhase = 0;
                b->timer = 0;
                b->flightT = 0;
                baxterPickTarget(b, pxWorld);
            }
        }
        break;
    }

    case BAXTER_FLY: {
        // (30/09) La vuelta alrededor de la tortuga: la fase avanza 2/3 de
        // unidad por frame (una vuelta en 96 frames) y la Y va con el coseno
        // (+16 = un cuarto de vuelta): una elipse. Antes la fase avanzaba 9
        // por frame y la tabla del seno (16 pasos) daba la vuelta cada ~7
        // frames en X y ~3,5 en Y: el destino saltaba de punta a punta todo
        // el tiempo y la nave, persiguiendolo a 5 px por frame, temblaba en
        // el lugar en vez de dar la vuelta.
        b->flightT++;
        s16 tx = b->tgx, ty = b->tgy;
        if (b->orbit) {
            u16 ph = (u16)((b->flightT * 2) / 3);
            tx = (s16)(b->tgx + ((rsin(ph) * 76) >> 8));
            ty = (s16)(b->tgy + ((rsin((u16)(ph + 16)) * 34) >> 8));
            if (b->timer > 110) { baxterPickTarget(b, pxWorld); b->timer = 0; }
        }
        s16 ddx = (s16)(tx - b->x);
        s16 ddy = (s16)(ty - b->y);
        s16 dist = (s16)(xabs(ddx) + xabs(ddy));
        if (!b->orbit && dist < 10) {
            b->state = BAXTER_DROP;
            b->timer = 0;
            b->doorPhase = 0;
            b->dropsThisCycle = 0;
            b->tgy = b->y;
            break;
        }
        if (dist > 0) {
            s16 d = (dist > 5) ? dist : 5;
            b->x += (s16)((ddx * 5) / d);
            b->y += (s16)((ddy * 5) / d);
        }
        b->x = xclamp(b->x, (s16)(b->arenaLeft + 8), (s16)(b->arenaRight - 8));
        b->y = xclamp(b->y, BAXTER_Y_MIN, bxYMax);
        if (!b->orbit && b->timer > 260) { baxterPickTarget(b, pxWorld); b->timer = 0; }
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
    u8 want;
    if (b->hurtFlash || (lowHp && ((b->timer >> 3) & 1))) want = BAXTER_ANIM_HURT;
    else if (b->state == BAXTER_DROP && b->doorPhase > 0) want = BAXTER_ANIM_DOOR;
    else want = BAXTER_ANIM_FLY;

    if (want == BAXTER_ANIM_FLY) {
        if (b->anim != BAXTER_ANIM_FLY) {
            SPR_setAutoAnimation(b->sprite, TRUE);
            SPR_setAnimationLoop(b->sprite, TRUE);
            SPR_setAnim(b->sprite, BAXTER_ANIM_FLY);
        }
    } else {
        SPR_setAutoAnimation(b->sprite, FALSE);
        SPR_setAnimAndFrame(b->sprite, want,
                            (want == BAXTER_ANIM_DOOR && b->doorPhase == 2) ? 1 : 0);
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
