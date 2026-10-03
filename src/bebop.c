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
// (03/10) COMO EL ARCADE: cada aro es un proyectil aparte. Sale chico de la
// boca del arma y CRECE mientras vuela (un tamano cada BEBOP_SHOT_GROW frames:
// en el video 10, 16 y 24 px de alto); el disparo agachado tira tres, uno cada
// BEBOP_CSHOT_RING_GAP frames. Vuela recto por X a BEBOP_SHOT_SPEED (4 px/f,
// medido) hasta pegarle a una tortuga o salirse de camara. 'x' es el CENTRO
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

        shots[i].x += (s16)(shots[i].dir * BEBOP_SHOT_SPEED);
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
    b->sprite = NULL;
    b->state  = BEBOP_INACTIVE;
    b->x = b->y = 0;
    b->z = 0;
    b->dir = -1;
    b->hp = BEBOP_HP;
    b->anim = 0xFF;
    b->timer = 0;
    b->cooldown = 0;
    b->calmTimer = 0;
    b->hitsTaken = 0;
    b->chargeHit = 0;
    b->chargeDir = -1;
    b->upperHit = 0;
    b->frameTick = 0;
    b->frame = 0;
    b->crouchShot = 0;
    b->step = 0;
    b->alignOnly = 0;
    b->aaCooldown = 0;
    b->flashTick = 0;
    b->flashOn = 0;
    b->comboHits = 0;
    b->armored = 0;
    b->armorTimer = 0;
    b->fromX = b->fromY = 0;
    b->cameraOffsetX = 0;
    b->cameraOffsetY = 0;
    b->arena = &arena21;
    b->accX = b->accY = 0;
    b->chargeWind = 0;
    b->actT = 0;
    b->slideDir = 1;
    bebopShotInit();
}

void bebopSpawnArena(Bebop* b, const BebopArena* arena) {
    b->arena     = arena ? arena : &arena21;
    b->hp        = BEBOP_HP;
    b->dir       = -1;          // mira a la izquierda: los jugadores vienen de ahi
    b->hitsTaken = 0;
    b->cooldown  = 0;
    b->calmTimer = 0;
    b->chargeHit = 0;
    b->upperHit  = 0;
    b->frameTick = 0;
    b->step      = 0;
    b->alignOnly = 0;
    b->aaCooldown = 0;
    b->flashTick = 0;
    b->flashOn   = 0;
    b->comboHits = 0;
    b->armored   = 0;
    b->armorTimer = 0;
    b->accX = b->accY = 0;
    b->chargeWind = 0;
    b->actT = 0;
    bebopBuildPalettes();
    // (26/09) PAL3 la venia usando la TV de la vidriera del 2-1: la paleta
    // del jefe se carga recien ahora, cuando entra.
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
    // Golpeable en el piso y peleando; NO durante la entrada, el flinch, la
    // caida ni la muerte. Tampoco con armadura (contraataque) ni en los
    // frames de gracia al levantarse -- ver el anti-trabado en bebop.h.
    if (b->armored || b->armorTimer > 0) return FALSE;
    return (bool)(b->state == BEBOP_IDLE   || b->state == BEBOP_TAUNT ||
                  b->state == BEBOP_WALK   || b->state == BEBOP_CHARGE ||
                  b->state == BEBOP_UPPER  || b->state == BEBOP_SHOOT);
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

// Uppercut CON ARMADURA: el contraataque del anti-trabado. Mismo golpe que el
// antiaereo, pero no se lo puede interrumpir y al que agarra en el piso lo
// DERRIBA (asi la tortuga sale despedida y la racha se corta de verdad).
static void bebopStartCounter(Bebop* b) {
    b->state     = BEBOP_UPPER;
    b->upperHit  = 0;
    b->frameTick = 0;
    b->armored   = 1;
    b->comboHits = 0;
    b->aaCooldown = BEBOP_AA_COOLDOWN;
    bebopManual(b, BEBOP_ANIM_UPPER, 0);
}

static void bebopStartDown(Bebop* b) {
    b->hitsTaken = 0;
    b->comboHits = 0;
    b->state = BEBOP_DOWN;
    b->timer = 0;                       // (03/10) cuenta hacia ARRIBA
    b->frameTick = 0;
    b->slideDir = (s8)-b->dir;          // desliza alejandose de quien le pego
    b->accX = 0;
    bebopManual(b, BEBOP_ANIM_HURT, 3);
}

bool bebopDamage(Bebop* b, s16 dmg) {
    return bebopDamageEx(b, dmg, (bool)(dmg >= BEBOP_SPECIAL_DMG));
}

bool bebopDamageEx(Bebop* b, s16 dmg, bool special) {
    if (!bebopCanBeHit(b)) return FALSE;
    b->hp -= dmg;
    b->calmTimer = 0;

    if (b->hp <= 0) {
        b->hp = 0;
        b->state = BEBOP_DEAD;
        b->timer = BEBOP_DEAD_HOLD;
        bebopFlashOff(b);
        b->frameTick = 0;
        bebopManual(b, BEBOP_ANIM_HURT, BEBOP_HURT_FR_HIT);
        XGM2_playPCMEx(boss_scream_bebop_vo, sizeof(boss_scream_bebop_vo),
                       SOUND_PCM_CH2, 15, FALSE, FALSE);
        return TRUE;
    }

    // (03/10) Embestida (amague o corrida): el golpe saca vida pero no la
    // corta -- ni flinch, ni caida, ni contraataque, y no suma a las rachas.
    // Intocable un rato para que el mismo swing no pegue cada frame.
    if (b->state == BEBOP_CHARGE) {
        b->armorTimer = special ? BEBOP_CHARGE_HIT_CD_SP : BEBOP_CHARGE_HIT_CD;
        return FALSE;
    }

    b->hitsTaken++;
    b->comboHits++;
    // (03/10) Arcade: el especial y el golpe SEGUIDO numero BEBOP_KD_COMBO lo
    // tiran (la cuenta total de BEBOP_KD_INTERVAL queda como respaldo).
    if (special || b->comboHits >= BEBOP_KD_COMBO ||
        b->hitsTaken >= BEBOP_KD_INTERVAL) {
        bebopStartDown(b);
    } else if (b->comboHits > BEBOP_COUNTER_HITS) {
        // Ya aguanto la racha: este golpe lo absorbe y responde.
        bebopStartCounter(b);
    } else {
        b->state = BEBOP_HURT;
        b->timer = BEBOP_HURT_FRAMES;
        b->frameTick = 0;
        bebopManual(b, BEBOP_ANIM_HURT, BEBOP_HURT_FR_HIT);
    }
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
        if (isPlayerGameOver(pls[k])) continue;   // (26/09) sin vidas: no cuenta
        s16 d = (s16)abs((s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2) - cx);
        if (d < bestD) { bestD = d; best = pls[k]; }
    }
    if (!best && nPl > 0) best = pls[0];      // no queda nadie: el nivel termina
    return best;
}

static void bebopToIdle(Bebop* b) {
    b->state = BEBOP_IDLE;
    b->timer = BEBOP_IDLE_MIN;
    bebopAuto(b, BEBOP_ANIM_IDLE, TRUE);
}

// Paso de movimiento en Q8 (256 = 1 px por frame) con resto acumulado.
static s16 bebopStepQ(u8* acc, u16 q) {
    u16 t = (u16)(*acc + q);
    *acc = (u8)(t & 0xFF);
    return (s16)(t >> 8);
}

// alignOnly: 0 se acerca, 1 solo alinea la lane (para disparar), 2 (03/10) se
// ALEJA para tomar carrera y embestir (en el video, antes de cada embestida
// camina hasta el otro lado).
static void bebopStartWalk(Bebop* b, u8 alignOnly) {
    b->state = BEBOP_WALK;
    b->alignOnly = alignOnly;
    b->timer = alignOnly == 1 ? BEBOP_ALIGN_TICKS : (alignOnly == 2 ? 150 : 120);
    b->frameTick = 0;
    b->frame = 0;
    bebopManual(b, BEBOP_ANIM_WALK, 0);
}

static const u8 chargeOrder[4] = { 0, 1, 3, 2 };   // orden del arcade
static void bebopStartCharge(Bebop* b, s8 dir) {
    b->state = BEBOP_CHARGE;
    b->timer = BEBOP_CHARGE_MAX;
    b->dir = dir;
    b->chargeDir = dir;
    b->chargeHit = 0;
    b->chargeWind = BEBOP_CHARGE_WINDUP;   // (03/10) amaga en el lugar
    b->frameTick = 0;
    b->actT = 0;
    b->accX = 0;
    bebopManual(b, BEBOP_ANIM_CHARGE, chargeOrder[0]);
}

static void bebopStartUpper(Bebop* b) {
    b->state = BEBOP_UPPER;
    b->upperHit = 0;
    b->frameTick = 0;
    b->aaCooldown = BEBOP_AA_COOLDOWN;
    bebopManual(b, BEBOP_ANIM_UPPER, 0);
}

static void bebopStartShoot(Bebop* b, bool crouch) {
    b->state = BEBOP_SHOOT;
    b->crouchShot = (u8)crouch;
    b->frameTick = 0;
    b->actT = 0;
    if (crouch) { bebopManual(b, BEBOP_ANIM_SHOOT, 0); return; }   // se para y apunta
    bebopManual(b, BEBOP_ANIM_SHOOT,
                crouch ? BEBOP_SHOOT_FR_CROUCH : BEBOP_SHOOT_FR_STAND);
}

static void bebopStartTaunt(Bebop* b) {
    b->state = BEBOP_TAUNT;
    b->frameTick = 0;
    b->calmTimer = 0;
    b->actT = 0;
    bebopManual(b, BEBOP_ANIM_TAUNT, 0);
}

// Avanza un frame de una anim manejada a mano. Devuelve TRUE cuando ya paso el
// ULTIMO frame del tramo [first..last].
static bool bebopStepFrames(Bebop* b, u8 anim, u8 first, u8 last, u8 ticks) {
    if (++b->frameTick < ticks) return FALSE;
    b->frameTick = 0;
    if (b->frame >= last) return TRUE;
    u8 f = (u8)(b->frame < first ? first : b->frame + 1);
    bebopManual(b, anim, f);
    return FALSE;
}

// ---------------------------------------------------------------------------
// Un frame de jefe
// ---------------------------------------------------------------------------
void bebopUpdate(Bebop* b, Player** pls, u8 nPl, s16 camX, s16 camY) {
    b->cameraOffsetX = camX;
    b->cameraOffsetY = camY;

    bebopShotUpdate(pls, nPl, camX, camY);

    if (b->state == BEBOP_INACTIVE || b->state == BEBOP_GONE) return;

    if (b->cooldown > 0) b->cooldown--;
    if (b->aaCooldown > 0) b->aaCooldown--;
    if (b->armorTimer > 0) b->armorTimer--;
    if (b->calmTimer < 0xFFF0) b->calmTimer++;
    if (b->calmTimer >= BEBOP_COMBO_RESET) b->comboHits = 0;
    if (b->state != BEBOP_DEAD) bebopFlashUpdate(b);

    Player* t = bebopTarget(b, pls, nPl);
    s16 cx = bebopGetCenterX(b);
    s16 tcx = t ? (s16)(getPlayerWorldX(t) + PLAYER_SPRITE_W / 2) : cx;
    s16 tcy = t ? getPlayerY(t) : b->y;
    s16 distX = (s16)abs(tcx - cx);
    s16 distY = (s16)(tcy - b->y);

    // ANTIAEREO (25/09): si el objetivo SALTA cerca, corta lo que este haciendo
    // (quieto, caminando o vitoreando) y suelta el uppercut. Es reactivo, no
    // espera el cooldown de las decisiones normales: solo el suyo propio.
    if (t && (b->state == BEBOP_IDLE || b->state == BEBOP_WALK ||
              b->state == BEBOP_TAUNT) &&
        b->aaCooldown == 0 && isPlayerJumping(t) &&
        distX <= BEBOP_AA_RANGE && abs(distY) <= BEBOP_HIT_TOL_Y) {
        b->dir = (tcx < cx) ? -1 : 1;
        bebopStartUpper(b);
    }

    // El contraataque sale hacia el que lo esta golpeando.
    if (t && b->armored && b->state == BEBOP_UPPER && b->frame == 0)
        b->dir = (tcx < cx) ? -1 : 1;

    switch (b->state) {

    // --- ENTRADA: caida diagonal al techo del auto -------------------------
    case BEBOP_FALL: {
        s16 total = BEBOP_FALL_TICKS;
        s16 done  = (s16)(total - (s16)b->timer);
        // Lineal en X y cuadratica en la altura: cae acelerando, que es como
        // se lee una caida de verdad.
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
        // Parabola de salto: sube y baja sobre la recta.
        b->z = (s16)((4 * BEBOP_JUMP_APEX * done * (total - done)) / (total * total));
        if (b->timer > 0) b->timer--;
        if (b->timer == 0) {
            b->z = 0;
            b->x = (s16)(b->arena->landX - BEBOP_FRAME_W / 2);
            b->y = b->arena->landY;
            b->cooldown = BEBOP_COOLDOWN;
            bebopToIdle(b);
        }
        break;
    }

    // --- PELEA -------------------------------------------------------------
    case BEBOP_IDLE:
        if (t) b->dir = (tcx < cx) ? -1 : 1;
        if (b->timer > 0) b->timer--;
        if (b->timer == 0 && b->cooldown == 0 && t) {
            bool aligned = (bool)(abs(distY) <= BEBOP_ALIGN_Y);
            if (b->calmTimer >= BEBOP_TAUNT_IDLE) {
                bebopStartTaunt(b);
            } else if (b->step < BEBOP_STEPS - 1) {
                // Pasos de ARMA: alinearse en lane (sin acercarse) y disparar.
                // Agachado contra el que esta en el piso, parado contra el que
                // salta (el tiro parado sale mas alto).
                if (distX > BEBOP_SHOOT_RANGE) {
                    bebopStartWalk(b, 0);              // demasiado lejos: acercarse
                } else if (aligned) {
                    b->step++;
                    bebopStartShoot(b, (bool)!isPlayerJumping(t));
                } else {
                    bebopStartWalk(b, 1);              // alinearse y disparar
                }
            } else {
                // Paso de CUERPO (03/10, como el arcade): embestida. Si no
                // hay carrera, primero se aleja para tomarla.
                b->step = 0;
                if (distX >= BEBOP_CHARGE_DIST) {
                    bebopStartCharge(b, (s8)((tcx < cx) ? -1 : 1));
                } else {
                    bebopStartWalk(b, 2);
                }
            }
        }
        break;

    case BEBOP_TAUNT:
        // (03/10) Vitoreo del arcade: alterna los frames 0 y 1 cada 8.
        b->actT++;
        bebopManual(b, BEBOP_ANIM_TAUNT, (u8)((b->actT / BEBOP_TAUNT_LOOP_TICKS) & 1));
        if (b->actT >= BEBOP_TAUNT_SHOT_F) bebopToIdle(b);
        break;

    case BEBOP_WALK: {
        if (!t) { bebopToIdle(b); break; }
        b->dir = (tcx < cx) ? -1 : 1;
        // Normal: se acerca en X y alinea la lane. Modo ALINEARSE: solo lane,
        // que es lo que necesita para disparar (el tiro va por su lane).
        // Modo ALEJARSE: camina para atras (mirando a la tortuga) a tomar
        // carrera. (03/10) Velocidades del arcade en Q8.
        {
            s16 vx = bebopStepQ(&b->accX, BEBOP_WALK_X_Q);
            s16 vy = bebopStepQ(&b->accY, BEBOP_WALK_Y_Q);
            if (b->alignOnly == 0 && distX > BEBOP_CLOSE_RANGE)
                b->x = (s16)(b->x + b->dir * vx);
            else if (b->alignOnly == 2)
                b->x = (s16)(b->x - b->dir * vx);
            if (distY > BEBOP_ALIGN_Y)       b->y += vy;
            else if (distY < -BEBOP_ALIGN_Y) b->y -= vy;
        }

        if (++b->frameTick >= BEBOP_WALK_TICKS) {
            b->frameTick = 0;
            bebopManual(b, BEBOP_ANIM_WALK, (u8)((b->frame + 1) % 6));
        }
        if (b->timer > 0) b->timer--;
        if (b->alignOnly == 2) {
            // Tomo carrera (o llego a la punta de la arena): embiste.
            bool edge = (bool)(bebopGetCenterX(b) <= b->arena->xMin + 2 ||
                               bebopGetCenterX(b) >= b->arena->xMax - 2);
            if (distX >= BEBOP_CHARGE_DIST || edge || b->timer == 0)
                bebopStartCharge(b, (s8)((tcx < cx) ? -1 : 1));
        } else if (b->alignOnly && abs(distY) <= BEBOP_ALIGN_Y &&
            distX <= BEBOP_SHOOT_RANGE) {
            b->step++;
            bebopStartShoot(b, (bool)!isPlayerJumping(t));
        } else if (!b->alignOnly && distX <= BEBOP_CLOSE_RANGE) {
            bebopToIdle(b);                    // llego: que decida el IDLE
        } else if (b->timer == 0) {
            bebopToIdle(b);
        }
        break;
    }

    case BEBOP_CHARGE:
        // (03/10) Arcade: anim en orden 0,1,3,2 a 4 ticks; primero AMAGA en el
        // lugar (BEBOP_CHARGE_WINDUP, girando hacia la tortuga) y recien
        // despues corre a 3,5 px/f corrigiendo la lane.
        if (++b->frameTick >= BEBOP_CHARGE_TICKS) {
            b->frameTick = 0;
            b->actT++;
            bebopManual(b, BEBOP_ANIM_CHARGE, chargeOrder[b->actT & 3]);
        }
        if (b->chargeWind > 0) {
            b->chargeWind--;
            if (t) b->dir = b->chargeDir = (s8)((tcx < cx) ? -1 : 1);
            break;
        }
        b->x = (s16)(b->x + b->chargeDir * bebopStepQ(&b->accX, BEBOP_CHARGE_Q));
        {
            s16 vy = bebopStepQ(&b->accY, BEBOP_CHARGE_LANE_Q);
            if (distY > BEBOP_ALIGN_Y)       b->y += vy;
            else if (distY < -BEBOP_ALIGN_Y) b->y -= vy;
        }
        b->dir = b->chargeDir;
        if (b->timer > 0) b->timer--;
        // Golpea por CONTACTO real de los cuerpos, como la de Rocksteady.
        if (!b->chargeHit) {
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                s16 pcx = getPlayerHurtCX(pls[k]);   // (02/10) centro de la hurtbox
                if (abs(pcx - bebopGetCenterX(b)) >
                    (s16)(BEBOP_BODY_HALF_W + PLAYER_BODY_HALF_W)) continue;
                if (abs(getPlayerY(pls[k]) - b->y) > BEBOP_HIT_TOL_Y) continue;
                playerHitBarsKnockdown(pls[k], bebopGetCenterX(b), BEBOP_CHARGE_DMG);
                b->chargeHit = 1;
                if (b->timer > BEBOP_CHARGE_OVER) b->timer = BEBOP_CHARGE_OVER;
                break;
            }
        }
        // Freno contra los extremos de la arena.
        if (b->timer == 0 ||
            bebopGetCenterX(b) <= b->arena->xMin || bebopGetCenterX(b) >= b->arena->xMax) {
            b->cooldown = BEBOP_COOLDOWN;
            bebopToIdle(b);
        }
        break;

    case BEBOP_UPPER: {
        // Pega en los frames 3-4 y ALCANZA AL QUE ESTA EN EL AIRE: por eso usa
        // playerCanBeHitAir y playerHitProjectile (el mismo camino que el
        // antiaereo de Rocksteady), filtrando por la altura del salto.
        if (b->frame >= BEBOP_UPPER_FR_HIT && !b->upperHit) {
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHitAir(pls[k])) continue;
                if (getPlayerJumpZ(pls[k]) > BEBOP_AA_MAX_Z) continue;
                s16 pcx = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
                s16 dx  = (s16)((pcx - bebopGetCenterX(b)) * b->dir);
                if (dx < -12 || dx > BEBOP_AA_RANGE) continue;
                if (abs(getPlayerY(pls[k]) - b->y) > BEBOP_HIT_TOL_Y) continue;
                if (b->armored && getPlayerJumpZ(pls[k]) == 0)
                    playerHitBarsKnockdown(pls[k], bebopGetCenterX(b), BEBOP_COUNTER_DMG);
                else
                    playerHitProjectile(pls[k], bebopGetCenterX(b), BEBOP_UPPER_DMG);
                b->upperHit = 1;
                break;
            }
        }
        u8 ticks = (b->frame < BEBOP_UPPER_FR_HIT) ? BEBOP_UPPER_WIND_TICKS
                                                   : BEBOP_UPPER_HIT_TICKS;
        if (bebopStepFrames(b, BEBOP_ANIM_UPPER, 0, 4, ticks)) {
            b->armored  = 0;
            b->cooldown = BEBOP_COOLDOWN;
            bebopToIdle(b);
        }
        break;
    }

    case BEBOP_SHOOT: {
        if (b->crouchShot) {
            // (03/10) Disparo del arcade: parado apuntando (0, 1), agachado
            // (4, 2, 4) y TRES aros, uno cada BEBOP_CSHOT_RING_GAP frames.
            u16 a = b->actT++;
            u8 f = (a < 6) ? 0 : (a < 12) ? 1 : (a < 24) ? 4 : (a < 30) ? 2 : 4;
            bebopManual(b, BEBOP_ANIM_SHOOT, f);
            if (a >= BEBOP_CSHOT_RING1 &&
                ((a - BEBOP_CSHOT_RING1) % BEBOP_CSHOT_RING_GAP) == 0 &&
                a < BEBOP_CSHOT_RING1 + 3 * BEBOP_CSHOT_RING_GAP)
                bebopShotSpawn((s16)(bebopGetCenterX(b) + b->dir * (BEBOP_MUZZLE_CROUCH_X + 4)),
                               b->y, BEBOP_MUZZLE_CROUCH_Z, b->dir);
            if (a + 1 >= BEBOP_CSHOT_F) {
                b->cooldown = BEBOP_COOLDOWN;
                // Despues de disparar, vitorea (como en el arcade).
                if ((u16)(random() % 100) < BEBOP_TAUNT_SHOT_PCT) bebopStartTaunt(b);
                else bebopToIdle(b);
            }
            break;
        }
        u8 first = b->crouchShot ? BEBOP_SHOOT_FR_CROUCH : BEBOP_SHOOT_FR_STAND;
        u8 last  = b->crouchShot ? 4 : 1;
        u8 prev  = b->frame;
        if (bebopStepFrames(b, BEBOP_ANIM_SHOOT, first, last, BEBOP_SHOT_TICKS)) {
            b->cooldown = BEBOP_COOLDOWN;
            bebopToIdle(b);
            break;
        }
        // El tiro sale al entrar en el ULTIMO frame de la pose.
        if (prev != b->frame && b->frame == last) {
            s16 mx = b->crouchShot ? BEBOP_MUZZLE_CROUCH_X : BEBOP_MUZZLE_STAND_X;
            s16 mz = b->crouchShot ? BEBOP_MUZZLE_CROUCH_Z : BEBOP_MUZZLE_STAND_Z;
            // El aro chico (centro de la celda) justo en la boca del arma.
            bebopShotSpawn((s16)(bebopGetCenterX(b) + b->dir * (mx + 4)),
                           b->y, mz, b->dir);
        }
        break;
    }

    case BEBOP_HURT:
        if (b->timer > 0) b->timer--;
        if (b->timer == (u16)(BEBOP_HURT_FRAMES / 2))
            bebopManual(b, BEBOP_ANIM_HURT, 1);
        if (b->timer == 0) {
            b->cooldown = 0;
            bebopToIdle(b);
        }
        break;

    case BEBOP_DOWN:
        // (03/10) Caida del arcade: sentado deslizando hacia atras (frame 3),
        // arrodillado (4) y levantandose (5).
        b->timer++;
        if (b->timer <= BEBOP_KD_SLIDE_F) {
            b->x = (s16)(b->x + b->slideDir * bebopStepQ(&b->accX, BEBOP_KD_SLIDE_Q));
            bebopManual(b, BEBOP_ANIM_HURT, 3);
        } else if (b->timer <= BEBOP_KD_SLIDE_F + BEBOP_KD_KNEEL_F) {
            bebopManual(b, BEBOP_ANIM_HURT, 4);
        } else {
            bebopManual(b, BEBOP_ANIM_HURT, 5);
        }
        if (b->timer >= BEBOP_KD_SLIDE_F + BEBOP_KD_KNEEL_F + BEBOP_KD_RISE_F) {
            b->state = BEBOP_GETUP;
            b->timer = BEBOP_GETUP_WAIT;
            b->armorTimer = BEBOP_GETUP_ARMOR;
            b->comboHits  = 0;
            bebopManual(b, BEBOP_ANIM_IDLE, 0);
        }
        break;

    case BEBOP_GETUP:
        // (03/10) Ya parado: un instante y suelta el uppercut (el arcade lo
        // hace SIEMPRE al levantarse).
        if (t) b->dir = (tcx < cx) ? -1 : 1;
        if (b->timer > 0) b->timer--;
        if (b->timer == 0) {
            if (t && distX <= BEBOP_WAKE_RANGE && abs(distY) <= BEBOP_HIT_TOL_Y) {
                bebopStartCounter(b);
            } else {
                b->cooldown = BEBOP_COOLDOWN;
                bebopToIdle(b);
            }
        }
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
