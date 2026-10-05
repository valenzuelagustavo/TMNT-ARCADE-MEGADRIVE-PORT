#include "bebop.h"
#include "level2_1_limits.h"   // lvl21WalkTop/Bot: la calle real del tramo final
#include "audio.h"             // boss_scream_bebop_vo

// ===========================================================================
// BEBOP — jefe del 2-1. Ver bebop.h para la conducta y las medidas.
// ===========================================================================

#define BEBOP_SCREEN_W  320   // el 2-1 corre en H40

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------
static s16 clampS16b(s16 v, s16 lo, s16 hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// La calle no es un rectangulo: la vereda baja en diagonal. Se usa la MISMA
// tabla que recorta a las tortugas, con un margen para que el jefe no quede
// con los pies dentro de la pared.
static s16 lvl21Col(s16 cx) {
    s16 col = (s16)(cx / LVL21_LIM_STEP);
    if (col < 0) col = 0;
    if (col >= LVL21_LIM_COLS) col = LVL21_LIM_COLS - 1;
    return col;
}
static s16 lvl21TopAt(s16 cx) { return (s16)lvl21WalkTop[lvl21Col(cx)]; }
static s16 lvl21BotAt(s16 cx) { return (s16)lvl21WalkBot[lvl21Col(cx)]; }

// La arena de siempre: el final de la calle del 2-1, frente al auto.
static const BebopArena arena21 = {
    BEBOP_X_MIN, BEBOP_X_MAX,
    BEBOP_LANE_TOP, BEBOP_LANE_BOTTOM,
    lvl21TopAt, lvl21BotAt,
    BEBOP_CAR_X, BEBOP_CAR_Y,
    BEBOP_LAND_X, BEBOP_LAND_Y,
    FALSE, 0,
    0           // parpadeo: colores de PAL3 (PAL3 es solo de Bebop)
};

static s16 bebopClampLane(const BebopArena* A, s16 cx, s16 lane) {
    s16 top = A->topAt ? A->topAt(cx) : A->laneTop;
    s16 bot = A->botAt ? A->botAt(cx) : A->laneBot;
    if (top < A->laneTop) top = A->laneTop;
    if (bot > A->laneBot) bot = A->laneBot;
    if (top > bot) top = bot;
    return clampS16b(lane, top, bot);
}

static void bebopSetAnim(Bebop* b, u8 a, bool loop) {
    if (!b->sprite) return;
    if (b->anim != a) {
        b->anim = a;
        SPR_setAnim(b->sprite, a);
    }
    SPR_setAnimationLoop(b->sprite, loop);
}

// Anim MANUAL: se apaga la auto-animacion y el frame lo elige el estado. Es lo
// mismo que hace el foot soldier de la alcantarilla, y hace falta acá porque
// varias filas son DOS poses distintas pegadas (disparo parado/agachado,
// golpe/tirado/levantarse).
static void bebopManual(Bebop* b, u8 a, u8 frame) {
    if (!b->sprite) return;
    b->anim  = a;
    b->frame = frame;
    SPR_setAutoAnimation(b->sprite, FALSE);
    SPR_setAnimAndFrame(b->sprite, a, frame);
}

static void bebopAuto(Bebop* b, u8 a, bool loop) {
    if (!b->sprite) return;
    SPR_setAutoAnimation(b->sprite, TRUE);
    b->anim = 0xFF;
    bebopSetAnim(b, a, loop);
}

static void bebopRender(Bebop* b) {
    if (!b->sprite) return;
    SPR_setHFlip(b->sprite, (bool)(b->dir < 0));
    SPR_setPosition(b->sprite,
                    (s16)(b->x - b->cameraOffsetX),
                    (s16)(b->y - b->z - BEBOP_FOOT_OFFSET - b->cameraOffsetY));
    // Profundidad por lane, como todo el resto de la escena. EXCEPCION: en la
    // entrada pasa por la franja de arriba, donde estan los marcos del HUD
    // (que son sprites con profundidad 0), asi que ahi se lo manda ATRAS del
    // HUD -- si no, cae por delante de la barra de vida.
    if (b->state == BEBOP_FALL || b->state == BEBOP_ON_CAR ||
        b->state == BEBOP_JUMP_DOWN)
        SPR_setDepth(b->sprite, 1);
    else
        SPR_setDepth(b->sprite, (s16)(-(b->y)));
}

// ---------------------------------------------------------------------------
// FLASH POR VIDA BAJA (25/09) — mismo efecto que Rocksteady en scenes.c
// ---------------------------------------------------------------------------
// Dos copias de la paleta del jefe: la normal y una "quemada" con cada canal
// duplicado (clampeado a 0xF). Por debajo de BEBOP_FLASH_HP se alternan cada
// BEBOP_FLASH_TICKS frames (03/10: 4/4 constante, como el arcade).
// En el 2-1 PAL3 es SOLO del jefe (y de su disparo, que parpadea con el), asi
// que se cambian los colores de PAL3. En el garage (arena->flashPal != 0) PAL3
// la comparten Rocksteady y April: ahi se cambia la LINEA del sprite de Bebop
// a arena->flashPal, donde el nivel cargo la paleta quemada.
static u16 bebopPal[16];
static u16 bebopFlashPal[16];

static void bebopBuildPalettes(void) {
    for (u16 i = 0; i < 16; i++) {
        u16 c = bebop_boss.palette->data[i];
        bebopPal[i] = c;
        if (i == 0) { bebopFlashPal[i] = c; continue; }
        u16 r = (c >> 8) & 0xF, g = (c >> 4) & 0xF, bl = c & 0xF;
        r  = (u16)((r  << 1) | (r  >> 3)); if (r  > 0xF) r  = 0xF;
        g  = (u16)((g  << 1) | (g  >> 3)); if (g  > 0xF) g  = 0xF;
        bl = (u16)((bl << 1) | (bl >> 3)); if (bl > 0xF) bl = 0xF;
        bebopFlashPal[i] = (u16)((r << 8) | (g << 4) | bl);
    }
}

static void bebopFlashApply(Bebop* b) {
    u8 line = b->arena ? b->arena->flashPal : 0;
    if (line) {
        if (b->sprite) SPR_setPalette(b->sprite, b->flashOn ? line : PAL3);
    } else {
        PAL_setPalette(PAL3, b->flashOn ? bebopFlashPal : bebopPal, DMA);
    }
}

static void bebopFlashOff(Bebop* b) {
    if (!b->flashOn) return;
    b->flashOn = 0;
    bebopFlashApply(b);
}

static void bebopFlashUpdate(Bebop* b) {
    if (b->hp <= 0) { bebopFlashOff(b); return; }
    u8 interval = (b->hp <= BEBOP_FLASH_HP) ? BEBOP_FLASH_TICKS : 0;
    if (interval == 0) { bebopFlashOff(b); return; }
    if (b->flashTick > 0) b->flashTick--;
    if (b->flashTick == 0) {
        b->flashTick = interval;
        b->flashOn ^= 1;
        bebopFlashApply(b);
    }
}

// ---------------------------------------------------------------------------
// DISPARO — los aros
// ---------------------------------------------------------------------------
// Cada aro es un proyectil aparte. Sale chico de la boca del arma y CRECE
// mientras vuela (un tamano cada BEBOP_SHOT_GROW frames); el disparo tira
// tres, uno cada BEBOP_SHOT_RING_GAP frames. Vuela recto por X a
// BEBOP_SHOT_Q hasta pegarle a una tortuga o salirse de camara. 'x' es el CENTRO
// del aro; 'y' es la LANE del que disparo y no cambia; la altura vive en 'z'.
// Golpea con el aro entero (mitad de su ancho en cada tamano).
static const u8 ringHalfW[BEBOP_SHOT_FRAMES] = { 4, 5, 6, 7, 8 };
static struct {
    u8      active;
    Sprite* sprite;
    s16     x, y, z;
    s8      dir;
    u8      frame;
    u8      tick;
    u8      acc;       // resto Q8 del avance
} shots[MAX_BEBOP_SHOTS];

static void bebopShotRelease(u16 i) {
    if (shots[i].sprite) SPR_releaseSprite(shots[i].sprite);
    shots[i].sprite = NULL;
    shots[i].active = 0;
}

void bebopShotInit(void) {
    for (u16 i = 0; i < MAX_BEBOP_SHOTS; i++) {
        shots[i].sprite = NULL;
        shots[i].active = 0;
    }
}

static void bebopShotSpawn(s16 x, s16 lane, s16 z, s8 dir) {
    for (u16 i = 0; i < MAX_BEBOP_SHOTS; i++) {
        if (shots[i].active) continue;
        shots[i].x     = x;
        shots[i].y     = lane;
        shots[i].z     = z;
        shots[i].dir   = dir;
        shots[i].frame = 0;
        shots[i].tick  = 0;
        shots[i].acc   = 0;
        // Sin sprite (VRAM llena) el tiro igual vuela y hace daño, misma regla
        // que la dinamita y la tapa del 2-1.
        shots[i].sprite = SPR_addSprite(&bebop_shot_spr, 0, 0,
                                        TILE_ATTR(PAL3, TRUE, FALSE, FALSE));
        if (shots[i].sprite) {
            SPR_setAutoAnimation(shots[i].sprite, FALSE);
            SPR_setAnimAndFrame(shots[i].sprite, 0, 0);
            SPR_setHFlip(shots[i].sprite, (bool)(dir < 0));
        }
        shots[i].active = 1;
        return;
    }
}

static void bebopShotUpdate(Player** pls, u8 nPl, s16 camX, s16 camY) {
    for (u16 i = 0; i < MAX_BEBOP_SHOTS; i++) {
        if (!shots[i].active) continue;

        {
            u16 q = (u16)(shots[i].acc + BEBOP_SHOT_Q);
            shots[i].acc = (u8)(q & 0xFF);
            shots[i].x += (s16)(shots[i].dir * (s16)(q >> 8));
        }
        if (shots[i].frame < BEBOP_SHOT_FRAMES - 1 &&
            ++shots[i].tick >= BEBOP_SHOT_GROW) {
            shots[i].tick = 0;
            shots[i].frame++;
            if (shots[i].sprite)
                SPR_setAnimAndFrame(shots[i].sprite, 0, shots[i].frame);
        }

        s16 sx = (s16)(shots[i].x - camX);
        if (sx < -(BEBOP_SHOT_W + BEBOP_SHOT_MARGIN) ||
            sx >  (s16)(BEBOP_SCREEN_W + BEBOP_SHOT_W + BEBOP_SHOT_MARGIN)) {
            bebopShotRelease(i);
            continue;
        }

        // Impacto: el aro (su ancho del tamano actual) contra el cuerpo, lane
        // en Y y altura contra el torso.
        s16 lo = (s16)(shots[i].x - ringHalfW[shots[i].frame]);
        s16 hi = (s16)(shots[i].x + ringHalfW[shots[i].frame]);
        s16 hx = shots[i].x;
        for (u8 k = 0; k < nPl; k++) {
            if (!playerCanBeHitAir(pls[k])) continue;
            s16 pcx = getPlayerHurtCX(pls[k]);   // (02/10) centro de la hurtbox
            s16 py  = getPlayerY(pls[k]);
            if (pcx + PLAYER_BODY_HALF_W < lo || pcx - PLAYER_BODY_HALF_W > hi)
                continue;
            if (abs(py - shots[i].y) > BEBOP_SHOT_TOL_Y) continue;
            s16 pz = (s16)(getPlayerJumpZ(pls[k]) + BEBOP_SHOT_TORSO_Z);
            if (abs(pz - shots[i].z) > BEBOP_SHOT_TOL_Z) continue;
            playerHitProjectile(pls[k], hx, BEBOP_SHOT_DMG);
            bebopShotRelease(i);
            break;
        }

        if (!shots[i].active || !shots[i].sprite) continue;
        SPR_setPosition(shots[i].sprite,
                        (s16)(shots[i].x - camX - BEBOP_SHOT_W / 2),
                        (s16)(shots[i].y - shots[i].z - camY - BEBOP_SHOT_H / 2));
        SPR_setDepth(shots[i].sprite, (s16)(-(shots[i].y) - 2));
    }
}

void bebopShotReleaseAll(void) {
    for (u16 i = 0; i < MAX_BEBOP_SHOTS; i++) bebopShotRelease(i);
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------
void bebopInit(Bebop* b) {
    memset(b, 0, sizeof(Bebop));
    b->sprite = NULL;
    b->state  = BEBOP_INACTIVE;
    b->dir = -1;
    b->hp = BEBOP_HP;
    b->anim = 0xFF;
    b->arena = &arena21;
    b->slideDir = 1;
    bebopShotInit();
}

void bebopSpawnArena(Bebop* b, const BebopArena* arena) {
    b->arena     = arena ? arena : &arena21;
    b->hp        = BEBOP_HP;
    b->dir       = -1;          // mira a la izquierda: los jugadores vienen de ahi
    b->frameTick = 0;
    b->chargeHit = 0;
    b->combo     = 0;
    b->kdHits    = 0;
    b->flashTick = 0;
    b->flashOn   = 0;
    b->armorTimer = 0;
    b->accX = b->accY = 0;
    bebopBuildPalettes();
    // PAL3 la venia usando la TV de la vidriera del 2-1: la paleta del jefe se
    // carga recien ahora, cuando entra.
    PAL_setPalette(PAL3, bebopPal, DMA);

    const BebopArena* A = b->arena;
    if (A->startOnCar) {
        // Aparece ya parado en el apoyo (el ascensor del garage).
        b->fromX = A->carX;
        b->fromY = A->carY;
        b->x     = (s16)(A->carX - BEBOP_FRAME_W / 2);
        b->y     = A->carY;
        b->z     = 0;
        b->state = BEBOP_ON_CAR;
        b->timer = A->carHold ? A->carHold : BEBOP_CAR_HOLD;
    } else {
        // Arranca la primera parabola: arriba y a la izquierda del auto.
        b->fromX = (s16)(A->carX + BEBOP_FALL_FROM_DX);
        b->fromY = A->carY;
        b->x     = (s16)(b->fromX - BEBOP_FRAME_W / 2);
        b->y     = b->fromY;
        b->z     = BEBOP_FALL_FROM_DZ;
        b->state = BEBOP_FALL;
        b->timer = BEBOP_FALL_TICKS;
    }

    // SPR_addSpriteSafe y no SPR_addSprite: a esta altura del nivel la VRAM de
    // sprites viene fragmentada por los soldiers que entraron y murieron, y el
    // jefe es el sprite mas grande de la escena (77 tiles).
    b->sprite = SPR_addSpriteSafe(&bebop_boss, 0, 0,
                                  TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    if (b->sprite) {
        if (A->startOnCar) bebopManual(b, BEBOP_ANIM_IDLE, 0);
        else               bebopManual(b, BEBOP_ANIM_CHARGE, 1);   // pose recogida
        bebopRender(b);
    }
}

void bebopSpawn(Bebop* b) {
    bebopSpawnArena(b, &arena21);
}

bool bebopIsActive(const Bebop* b) {
    return (bool)(b->state != BEBOP_INACTIVE && b->state != BEBOP_GONE);
}

bool bebopIsGone(const Bebop* b) { return (bool)(b->state == BEBOP_GONE); }

bool bebopCanBeHit(const Bebop* b) {
    switch (b->state) {
        case BEBOP_IDLE:
        case BEBOP_WALK:
        case BEBOP_SHOOT:
            return TRUE;
        case BEBOP_WINDUP:
        case BEBOP_CHARGE:
            return (bool)(b->armorTimer == 0);   // con armadura
        default:
            return FALSE;
    }
}

s16 bebopGetCenterX(const Bebop* b) { return (s16)(b->x + BEBOP_FRAME_W / 2); }
s16 bebopGetCenterY(const Bebop* b) { return b->y; }
s16 bebopHp(const Bebop* b)         { return b->hp; }

void bebopRelease(Bebop* b) {
    bebopFlashOff(b);
    if (b->sprite) SPR_releaseSprite(b->sprite);
    b->sprite = NULL;
    bebopShotReleaseAll();
}

// ---------------------------------------------------------------------------
// Arranque de cada estado
// ---------------------------------------------------------------------------
static void bebopEnter(Bebop* b, BebopState s) {
    b->state = s;
    b->timer = 0;
    b->frameTick = 0;
}

static void bebopToIdle(Bebop* b) {
    bebopEnter(b, BEBOP_IDLE);
    bebopAuto(b, BEBOP_ANIM_IDLE, TRUE);
}

static void bebopToWalk(Bebop* b) {
    bebopEnter(b, BEBOP_WALK);
    bebopManual(b, BEBOP_ANIM_WALK, 0);
}

static const u8 chargeOrder[4] = { 0, 1, 3, 2 };   // orden del arcade
static void bebopToWindup(Bebop* b, s8 dir) {
    bebopEnter(b, BEBOP_WINDUP);
    b->dir = dir;
    b->chargeHit = 0;
    b->armorTimer = 0;
    b->frame = 0;
    bebopManual(b, BEBOP_ANIM_CHARGE, chargeOrder[0]);
}

static void bebopToUpper(Bebop* b, s8 dir) {
    bebopEnter(b, BEBOP_UPPER);
    b->dir = dir;
    bebopManual(b, BEBOP_ANIM_UPPER, 0);
}

static void bebopToShoot(Bebop* b) {
    bebopEnter(b, BEBOP_SHOOT);
    bebopManual(b, BEBOP_ANIM_SHOOT, 0);
}

static void bebopToHurt(Bebop* b) {
    bebopEnter(b, BEBOP_HURT);
    bebopManual(b, BEBOP_ANIM_HURT, BEBOP_HURT_FR_HIT);
}

static void bebopToDown(Bebop* b) {
    bebopEnter(b, BEBOP_DOWN);
    b->combo = 0;
    b->kdHits = 0;
    b->slideDir = (s8)-b->dir;          // desliza alejandose de quien le pego
    b->accX = 0;
    bebopManual(b, BEBOP_ANIM_HURT, 3);
}

static u8 bebopRand(u8 n) { return (u8)(random() % n); }

bool bebopDamage(Bebop* b, s16 dmg) {
    return bebopDamageEx(b, dmg, (bool)(dmg >= BEBOP_SPECIAL_DMG));
}

bool bebopDamageEx(Bebop* b, s16 dmg, bool special) {
    if (!bebopCanBeHit(b)) return FALSE;
    b->hp -= dmg;

    if (b->hp <= 0) {
        b->hp = 0;
        bebopEnter(b, BEBOP_DEAD);
        b->timer = BEBOP_DEAD_HOLD;
        bebopFlashOff(b);
        bebopManual(b, BEBOP_ANIM_HURT, BEBOP_HURT_FR_HIT);
        XGM2_playPCMEx(boss_scream_bebop_vo, sizeof(boss_scream_bebop_vo),
                       SOUND_PCM_CH2, 15, FALSE, FALSE);
        return TRUE;
    }

    // Embestida (amague o corrida): el golpe saca vida pero no la corta.
    if (b->state == BEBOP_WINDUP || b->state == BEBOP_CHARGE) {
        b->armorTimer = special ? BEBOP_CHARGE_HIT_CD_SP : BEBOP_CHARGE_HIT_CD;
        return FALSE;
    }

    u8 add = special ? 2 : 1;
    b->combo  += add;
    b->kdHits += add;
    if (b->kdHits >= BEBOP_KD_HITS) bebopToDown(b);
    else                            bebopToHurt(b);
    // Con 4 golpes seguidos el contraataque sale en el update.
    return FALSE;
}

// ---------------------------------------------------------------------------
// Decisiones
// ---------------------------------------------------------------------------
static Player* bebopTarget(Bebop* b, Player** pls, u8 nPl) {
    Player* best = NULL;
    s16 bestD = 0x7FFF;
    s16 cx = bebopGetCenterX(b);
    for (u8 k = 0; k < nPl; k++) {
        if (!pls[k]) continue;
        if (isPlayerGameOver(pls[k])) continue;   // sin vidas: no cuenta
        s16 d = (s16)abs((s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2) - cx);
        if (d < bestD) { bestD = d; best = pls[k]; }
    }
    if (!best && nPl > 0) best = pls[0];      // no queda nadie: el nivel termina
    return best;
}

// Paso de movimiento en Q8 (256 = 1 px por frame) con resto acumulado.
static s16 bebopStepQ(u8* acc, u16 q) {
    u16 t = (u16)(*acc + q);
    *acc = (u8)(t & 0xFF);
    return (s16)(t >> 8);
}

static s16 bebopToward(s16 v, s16 to, s16 step) {
    if (v < to) return (to - v < step) ? to : (s16)(v + step);
    if (v > to) return (v - to < step) ? to : (s16)(v - step);
    return v;
}

// Golpe por contacto (golpe y embestida) contra todas las tortugas: voltea.
// Devuelve TRUE si le pego a alguna.
static bool bebopContact(Bebop* b, Player** pls, u8 nPl, s16 fwd, s16 back) {
    s16 cx = bebopGetCenterX(b);
    bool hit = FALSE;
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k]) || !playerCanBeHit(pls[k])) continue;
        s16 dy = (s16)(getPlayerY(pls[k]) - b->y);
        if (dy < -BEBOP_HIT_DY_UP || dy > BEBOP_HIT_DY_DOWN) continue;
        s16 rel = (s16)((getPlayerHurtCX(pls[k]) - cx) * b->dir);   // + = adelante
        if (rel > fwd || rel < -back) continue;
        playerHitBarsKnockdown(pls[k], cx, BEBOP_CONTACT_DMG);
        hit = TRUE;
    }
    return hit;
}

// ---------------------------------------------------------------------------
// Un frame de jefe
// ---------------------------------------------------------------------------
void bebopUpdate(Bebop* b, Player** pls, u8 nPl, s16 camX, s16 camY) {
    b->cameraOffsetX = camX;
    b->cameraOffsetY = camY;

    bebopShotUpdate(pls, nPl, camX, camY);

    if (b->state == BEBOP_INACTIVE || b->state == BEBOP_GONE) return;

    if (b->armorTimer > 0) b->armorTimer--;
    if (b->state != BEBOP_DEAD) bebopFlashUpdate(b);

    Player* t = bebopTarget(b, pls, nPl);
    s16 cx = bebopGetCenterX(b);
    s16 tcx = t ? (s16)(getPlayerWorldX(t) + PLAYER_SPRITE_W / 2) : cx;
    s16 tcy = t ? getPlayerY(t) : b->y;
    s16 distX = (s16)abs(tcx - cx);
    s16 distY = (s16)(tcy - b->y);
    s8  toward = (tcx < cx) ? -1 : 1;
    bool alignY  = (bool)(abs(distY) <= BEBOP_ALIGN_Y);
    bool alNear  = (bool)(t && alignY && distX <= BEBOP_ALIGN_X);
    bool alFar   = (bool)(t && alignY && distX >  BEBOP_ALIGN_X);
    bool fighting = (bool)(b->state != BEBOP_FALL && b->state != BEBOP_ON_CAR &&
                           b->state != BEBOP_JUMP_DOWN && b->state != BEBOP_DEAD);
    if (fighting) b->timer++;

    // Contraataque: 4 golpes seguidos (y lejos de la caida) -> golpe.
    if (b->state == BEBOP_HURT && b->combo >= BEBOP_COUNTER_HITS &&
        b->kdHits < BEBOP_KD_HITS) {
        b->combo = 0;
        bebopToUpper(b, toward);
    }

    switch (b->state) {

    // --- ENTRADA: caida diagonal al techo del auto -------------------------
    case BEBOP_FALL: {
        s16 total = BEBOP_FALL_TICKS;
        s16 done  = (s16)(total - (s16)b->timer);
        // Lineal en X y cuadratica en la altura: cae acelerando.
        s16 gone = (s16)(total - done);
        b->x = (s16)(b->fromX + ((b->arena->carX - b->fromX) * done) / total
                     - BEBOP_FRAME_W / 2);
        b->y = b->arena->carY;
        b->z = (s16)((BEBOP_FALL_FROM_DZ * gone * gone) / (total * total));
        b->dir = 1;
        if (b->timer > 0) b->timer--;
        if (b->timer == 0) {
            b->z = 0;
            b->state = BEBOP_ON_CAR;
            b->timer = b->arena->carHold ? b->arena->carHold : BEBOP_CAR_HOLD;
            bebopManual(b, BEBOP_ANIM_IDLE, 0);
        }
        break;
    }

    case BEBOP_ON_CAR:
        b->dir = -1;                       // se da vuelta hacia las tortugas
        if (b->timer > 0) b->timer--;
        if (b->timer == 0) {
            b->fromX = b->arena->carX;
            b->fromY = b->arena->carY;
            b->state = BEBOP_JUMP_DOWN;
            b->timer = BEBOP_JUMP_TICKS;
            bebopManual(b, BEBOP_ANIM_CHARGE, 1);
        }
        break;

    // --- ENTRADA: salto del auto a la calle --------------------------------
    case BEBOP_JUMP_DOWN: {
        s16 total = BEBOP_JUMP_TICKS;
        s16 done  = (s16)(total - (s16)b->timer);
        b->x = (s16)(b->fromX + ((b->arena->landX - b->fromX) * done) / total
                     - BEBOP_FRAME_W / 2);
        b->y = (s16)(b->fromY + ((b->arena->landY - b->fromY) * done) / total);
        b->z = (s16)((4 * BEBOP_JUMP_APEX * done * (total - done)) / (total * total));
        if (b->timer > 0) b->timer--;
        if (b->timer == 0) {
            b->z = 0;
            b->x = (s16)(b->arena->landX - BEBOP_FRAME_W / 2);
            b->y = b->arena->landY;
            bebopToIdle(b);
        }
        break;
    }

    // --- PELEA -------------------------------------------------------------
    case BEBOP_IDLE:
        if (t) b->dir = toward;
        if (b->timer < BEBOP_IDLE_T || !t) break;
        if (alNear) {
            bebopToWalk(b);
        } else if (alFar) {
            u8 c = bebopRand(3);
            if (c == 0)      bebopToShoot(b);
            else if (c == 1) bebopToWalk(b);
            else             bebopToIdle(b);
        } else {
            if (bebopRand(2)) bebopToWalk(b);
            else              bebopToWindup(b, toward);
        }
        break;

    case BEBOP_WALK: {
        if (!t) { bebopToIdle(b); break; }
        if (b->timer == 2 * BEBOP_WALK_TICKS) b->combo = 0;   // frame 2
        b->dir = toward;
        {
            s16 vx = bebopStepQ(&b->accX, BEBOP_WALK_Q);
            s16 vy = bebopStepQ(&b->accY, BEBOP_WALK_Q);
            if (distX > BEBOP_NEAR_DX) b->x = (s16)(b->x + b->dir * vx);
            b->y = bebopToward(b->y, tcy, vy);
        }
        if (++b->frameTick >= BEBOP_WALK_TICKS) {
            b->frameTick = 0;
            bebopManual(b, BEBOP_ANIM_WALK, (u8)((b->frame + 1) % 6));
        }
        if (alNear && !isPlayerJumping(t)) { bebopToUpper(b, toward); break; }
        if (alFar) {
            u8 c = bebopRand(3);
            if (c == 0)      { bebopToShoot(b); break; }
            else if (c == 1) { bebopToIdle(b);  break; }
        }
        if (b->timer >= BEBOP_WALK_T) bebopToIdle(b);
        break;
    }

    case BEBOP_WINDUP:
        // Amague en el lugar, girando hacia la tortuga.
        if (t) b->dir = toward;
        if (++b->frameTick >= BEBOP_CHARGE_TICKS) {
            b->frameTick = 0;
            b->frame++;
            bebopManual(b, BEBOP_ANIM_CHARGE, chargeOrder[b->frame & 3]);
        }
        if (b->timer >= BEBOP_WINDUP_T) {
            u8 f = b->frame;
            bebopEnter(b, BEBOP_CHARGE);
            b->frame = f;
            b->accX = 0;
        }
        break;

    case BEBOP_CHARGE: {
        if (++b->frameTick >= BEBOP_CHARGE_TICKS) {
            b->frameTick = 0;
            b->frame++;
            bebopManual(b, BEBOP_ANIM_CHARGE, chargeOrder[b->frame & 3]);
        }
        b->x = (s16)(b->x + b->dir * bebopStepQ(&b->accX, BEBOP_CHARGE_Q));
        b->y = bebopToward(b->y, tcy, bebopStepQ(&b->accY, BEBOP_WALK_Q));
        // Pega UNA vez por contacto.
        if (!b->chargeHit &&
            bebopContact(b, pls, nPl, BEBOP_CHARGE_DX, BEBOP_CHARGE_DX))
            b->chargeHit = 1;
        s16 c = bebopGetCenterX(b);
        if (c <= b->arena->xMin || c >= b->arena->xMax) {
            bebopToIdle(b);                    // llego al borde
        } else if (b->timer >= BEBOP_CHARGE_T) {
            if (c <= b->arena->xMin + BEBOP_EDGE_NEAR) bebopToWindup(b, toward);
            else                                       bebopToIdle(b);
        }
        break;
    }

    case BEBOP_UPPER: {
        // 5 frames a 14 fps (~4 frames de juego cada uno); desde el frame 3
        // pega por contacto y voltea.
        u8 f = (u8)((b->timer * 5) / (BEBOP_UPPER_T + 1));
        if (f > 4) f = 4;
        if (f != b->frame) bebopManual(b, BEBOP_ANIM_UPPER, f);
        if (f >= BEBOP_UPPER_FR_HIT)
            bebopContact(b, pls, nPl, BEBOP_UPPER_FWD, BEBOP_UPPER_BACK);
        if (b->timer < BEBOP_UPPER_T) break;
        b->combo = 0;
        if (alNear) bebopToWalk(b);
        else if (bebopRand(2)) bebopToWindup(b, toward);
        else bebopToIdle(b);
        break;
    }

    case BEBOP_SHOOT: {
        // Frames del remaster de PC 7/7/15/13: se para (0), apunta (1), se agacha y
        // dispara (4) y vuelve (2). Tres aros, uno cada 6 frames.
        u16 a = (u16)(b->timer - 1);
        u8 f = (a < 7) ? 0 : (a < 14) ? 1 : (a < 29) ? 4 : 2;
        if (f != b->frame) bebopManual(b, BEBOP_ANIM_SHOOT, f);
        if (a >= BEBOP_SHOT_RING1 &&
            ((a - BEBOP_SHOT_RING1) % BEBOP_SHOT_RING_GAP) == 0 &&
            a < BEBOP_SHOT_RING1 + BEBOP_SHOT_RINGS * BEBOP_SHOT_RING_GAP)
            bebopShotSpawn((s16)(bebopGetCenterX(b) + b->dir * (BEBOP_MUZZLE_CROUCH_X + 4)),
                           b->y, BEBOP_MUZZLE_CROUCH_Z, b->dir);
        if (b->timer >= BEBOP_SHOOT_T) bebopToWalk(b);
        break;
    }

    case BEBOP_HURT:
        if (b->timer == BEBOP_HURT_T / 2) bebopManual(b, BEBOP_ANIM_HURT, 1);
        if (b->timer < BEBOP_HURT_T) break;
        if (t) b->dir = toward;
        if (bebopRand(2)) bebopToWalk(b);
        else              bebopToIdle(b);
        break;

    case BEBOP_DOWN:
        // Sentado deslizando hacia atras (frame 3) y arrodillado (4).
        if (b->timer <= BEBOP_KD_SLIDE_F) {
            b->x = (s16)(b->x + b->slideDir * bebopStepQ(&b->accX, BEBOP_SLIDE_Q));
            if (b->timer == BEBOP_KD_SLIDE_F) bebopManual(b, BEBOP_ANIM_HURT, 4);
        }
        if (b->timer >= BEBOP_KD_T) {
            bebopEnter(b, BEBOP_GETUP);
            bebopManual(b, BEBOP_ANIM_HURT, 5);
        }
        break;

    case BEBOP_GETUP:
        // Se levanta y dispara.
        if (b->timer < BEBOP_GETUP_T) break;
        if (t) b->dir = toward;
        bebopToShoot(b);
        break;

    case BEBOP_DEAD:
        if (b->timer > 0) b->timer--;
        if (b->timer > BEBOP_DEAD_HOLD - 12) {
            bebopManual(b, BEBOP_ANIM_HURT, 1);
        } else {
            bebopManual(b, BEBOP_ANIM_HURT, BEBOP_HURT_FR_DOWN);
        }
        if (b->timer == 0) {
            b->state = BEBOP_GONE;
            bebopRelease(b);
            return;
        }
        break;

    default:
        break;
    }

    // Recorte del area: durante la entrada NO se aplica (esta en el aire, y la
    // tabla de la calle lo dejaria clavado, igual que a los foot soldiers).
    if (b->state != BEBOP_FALL && b->state != BEBOP_ON_CAR &&
        b->state != BEBOP_JUMP_DOWN) {
        s16 c = clampS16b(bebopGetCenterX(b), b->arena->xMin, b->arena->xMax);
        b->x = (s16)(c - BEBOP_FRAME_W / 2);
        b->y = bebopClampLane(b->arena, c, b->y);
    }

    bebopRender(b);
}
