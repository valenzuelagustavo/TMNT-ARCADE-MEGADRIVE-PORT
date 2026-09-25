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
static s16 bebopClampLane(s16 cx, s16 lane) {
    s16 col = (s16)(cx / LVL21_LIM_STEP);
    if (col < 0) col = 0;
    if (col >= LVL21_LIM_COLS) col = LVL21_LIM_COLS - 1;
    s16 top = (s16)lvl21WalkTop[col];
    s16 bot = (s16)lvl21WalkBot[col];
    if (top < BEBOP_LANE_TOP)    top = BEBOP_LANE_TOP;
    if (bot > BEBOP_LANE_BOTTOM) bot = BEBOP_LANE_BOTTOM;
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
// BEBOP_FLASH_TICKS frames, y por debajo de BEBOP_FLASH_CRIT_HP cada
// BEBOP_FLASH_CRIT_TICKS. Vive aca y no en el nivel porque en el 2-1 PAL3 es
// SOLO del jefe (y de su disparo, que parpadea con el -- queda bien).
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

static void bebopFlashOff(Bebop* b) {
    if (!b->flashOn) return;
    b->flashOn = 0;
    PAL_setPalette(PAL3, bebopPal, DMA);
}

static void bebopFlashUpdate(Bebop* b) {
    if (b->hp <= 0) { bebopFlashOff(b); return; }
    u8 interval = (b->hp <= BEBOP_FLASH_CRIT_HP) ? BEBOP_FLASH_CRIT_TICKS
                : ((b->hp <= BEBOP_FLASH_HP) ? BEBOP_FLASH_TICKS : 0);
    if (interval == 0) { bebopFlashOff(b); return; }
    if (b->flashTick > 0) b->flashTick--;
    if (b->flashTick == 0) {
        b->flashTick = interval;
        b->flashOn ^= 1;
        PAL_setPalette(PAL3, b->flashOn ? bebopFlashPal : bebopPal, DMA);
    }
}

// ---------------------------------------------------------------------------
// DISPARO — los aros
// ---------------------------------------------------------------------------
// El proyectil se va FORMANDO mientras viaja: el sprite tiene 5 frames y cada
// uno agrega un aro, asi que arranca con el aro chico (el que sale del caño) y
// termina con los cinco. Vuela recto por X hasta pegarle a una tortuga o
// salirse de camara. 'y' es la LANE del que disparo y no cambia; la altura
// vive en 'z', igual que las balas de Rocksteady.
//
// (25/09) DOS ARREGLOS, los dos por lo mismo: el tiro no se veia.
// 1. 'x' es el BORDE TRASERO de la celda, no su centro. El aro chico esta en
//    la columna BEBOP_SHOT_REAR y el grande termina en la 71; antes se centraba
//    la celda en la boca del arma, asi que el primer aro nacia 30 px ADENTRO
//    del cuerpo de Bebop.
// 2. La hitbox era una caja fija de +-50 px alrededor del centro de la celda:
//    contra una tortuga a menos de ~140 px el tiro pegaba EN EL PRIMER FRAME y
//    se liberaba antes de dibujarse -- el jugador perdia vida sin ver ningun
//    aro. Ahora pega solo el FRENTE del disparo: el aro mas adelantado del
//    frame actual (shotFront, medido sobre bebop_shot_gen.png), asi que el
//    golpe llega cuando el dibujo llega.
#define BEBOP_SHOT_REAR     7    // columna del aro chico dentro de la celda
#define BEBOP_SHOT_FRONT_W 14    // ancho del tramo del frente que golpea
static const u8 shotFront[BEBOP_SHOT_FRAMES] = { 13, 24, 38, 53, 71 };
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

        // Impacto: el FRENTE del disparo contra el cuerpo, lane en Y y altura
        // contra el torso. La celda se espeja con dir < 0, asi que el frente
        // se mide siempre desde el borde trasero en la direccion del viaje.
        s16 front = (s16)(shots[i].x + shots[i].dir * shotFront[shots[i].frame]);
        s16 back  = (s16)(front - shots[i].dir * BEBOP_SHOT_FRONT_W);
        s16 lo = (front < back) ? front : back;
        s16 hi = (front < back) ? back : front;
        s16 hx = front;
        for (u8 k = 0; k < nPl; k++) {
            if (!playerCanBeHitAir(pls[k])) continue;
            s16 pcx = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
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
                        (s16)(shots[i].x - camX - (shots[i].dir < 0 ? BEBOP_SHOT_W : 0)),
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
    bebopShotInit();
}

void bebopSpawn(Bebop* b) {
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
    bebopBuildPalettes();

    // Arranca la primera parabola: arriba y a la izquierda del auto.
    b->fromX = (s16)(BEBOP_CAR_X + BEBOP_FALL_FROM_DX);
    b->fromY = BEBOP_CAR_Y;
    b->x     = (s16)(b->fromX - BEBOP_FRAME_W / 2);
    b->y     = b->fromY;
    b->z     = BEBOP_FALL_FROM_DZ;
    b->state = BEBOP_FALL;
    b->timer = BEBOP_FALL_TICKS;

    // SPR_addSpriteSafe y no SPR_addSprite: a esta altura del nivel la VRAM de
    // sprites viene fragmentada por los soldiers que entraron y murieron, y el
    // jefe es el sprite mas grande de la escena (77 tiles).
    b->sprite = SPR_addSpriteSafe(&bebop_boss, 0, 0,
                                  TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    if (b->sprite) {
        bebopManual(b, BEBOP_ANIM_CHARGE, 1);   // pose recogida durante la caida
        bebopRender(b);
    }
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

bool bebopDamage(Bebop* b, s16 dmg) {
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

    b->hitsTaken++;
    b->comboHits++;
    if (b->hitsTaken >= BEBOP_KD_INTERVAL) {
        b->hitsTaken = 0;
        b->comboHits = 0;
        b->state = BEBOP_DOWN;
        b->timer = BEBOP_KD_HOLD;
        b->frameTick = 0;
        bebopManual(b, BEBOP_ANIM_HURT, BEBOP_HURT_FR_HIT);
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
        s16 d = (s16)abs((s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2) - cx);
        if (d < bestD) { bestD = d; best = pls[k]; }
    }
    return best;
}

static void bebopToIdle(Bebop* b) {
    b->state = BEBOP_IDLE;
    b->timer = BEBOP_IDLE_MIN;
    bebopAuto(b, BEBOP_ANIM_IDLE, TRUE);
}

static void bebopStartWalk(Bebop* b, bool alignOnly) {
    b->state = BEBOP_WALK;
    b->alignOnly = (u8)alignOnly;
    b->timer = alignOnly ? BEBOP_ALIGN_TICKS : 90;   // tope: no camina para siempre
    b->frameTick = 0;
    b->frame = 0;
    bebopManual(b, BEBOP_ANIM_WALK, 0);
}

static void bebopStartCharge(Bebop* b, s8 dir) {
    b->state = BEBOP_CHARGE;
    b->timer = BEBOP_CHARGE_MAX;
    b->dir = dir;
    b->chargeDir = dir;
    b->chargeHit = 0;
    bebopAuto(b, BEBOP_ANIM_CHARGE, TRUE);
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
    bebopManual(b, BEBOP_ANIM_SHOOT,
                crouch ? BEBOP_SHOOT_FR_CROUCH : BEBOP_SHOOT_FR_STAND);
}

static void bebopStartTaunt(Bebop* b) {
    b->state = BEBOP_TAUNT;
    b->frameTick = 0;
    b->calmTimer = 0;
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
        b->x = (s16)(b->fromX + ((BEBOP_CAR_X - b->fromX) * done) / total
                     - BEBOP_FRAME_W / 2);
        b->y = BEBOP_CAR_Y;
        b->z = (s16)((BEBOP_FALL_FROM_DZ * gone * gone) / (total * total));
        b->dir = 1;
        if (b->timer > 0) b->timer--;
        if (b->timer == 0) {
            b->z = 0;
            b->state = BEBOP_ON_CAR;
            b->timer = BEBOP_CAR_HOLD;
            bebopManual(b, BEBOP_ANIM_IDLE, 0);
        }
        break;
    }

    case BEBOP_ON_CAR:
        b->dir = -1;                       // se da vuelta hacia las tortugas
        if (b->timer > 0) b->timer--;
        if (b->timer == 0) {
            b->fromX = BEBOP_CAR_X;
            b->fromY = BEBOP_CAR_Y;
            b->state = BEBOP_JUMP_DOWN;
            b->timer = BEBOP_JUMP_TICKS;
            bebopManual(b, BEBOP_ANIM_CHARGE, 1);
        }
        break;

    // --- ENTRADA: salto del auto a la calle --------------------------------
    case BEBOP_JUMP_DOWN: {
        s16 total = BEBOP_JUMP_TICKS;
        s16 done  = (s16)(total - (s16)b->timer);
        b->x = (s16)(b->fromX + ((BEBOP_LAND_X - b->fromX) * done) / total
                     - BEBOP_FRAME_W / 2);
        b->y = (s16)(b->fromY + ((BEBOP_LAND_Y - b->fromY) * done) / total);
        // Parabola de salto: sube y baja sobre la recta.
        b->z = (s16)((4 * BEBOP_JUMP_APEX * done * (total - done)) / (total * total));
        if (b->timer > 0) b->timer--;
        if (b->timer == 0) {
            b->z = 0;
            b->x = (s16)(BEBOP_LAND_X - BEBOP_FRAME_W / 2);
            b->y = BEBOP_LAND_Y;
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
                    bebopStartWalk(b, FALSE);          // demasiado lejos: acercarse
                } else if (aligned) {
                    b->step++;
                    bebopStartShoot(b, (bool)!isPlayerJumping(t));
                } else {
                    bebopStartWalk(b, TRUE);           // alinearse y disparar
                }
            } else {
                // Paso de CUERPO: lejos embiste, cerca dispara a quemarropa.
                b->step = 0;
                if (distX >= BEBOP_CHARGE_DIST && abs(distY) <= BEBOP_HIT_TOL_Y) {
                    bebopStartCharge(b, (s8)((tcx < cx) ? -1 : 1));
                } else if (distX <= BEBOP_CLOSE_RANGE && abs(distY) <= BEBOP_HIT_TOL_Y) {
                    bebopStartShoot(b, TRUE);
                } else {
                    bebopStartWalk(b, FALSE);
                }
            }
        }
        break;

    case BEBOP_TAUNT:
        if (bebopStepFrames(b, BEBOP_ANIM_TAUNT, 0, 2, BEBOP_TAUNT_TICKS))
            bebopToIdle(b);
        break;

    case BEBOP_WALK: {
        if (!t) { bebopToIdle(b); break; }
        b->dir = (tcx < cx) ? -1 : 1;
        // Normal: se acerca en X y alinea la lane. Modo ALINEARSE: solo lane,
        // que es lo que necesita para disparar (el tiro va por su lane).
        if (!b->alignOnly && distX > BEBOP_CLOSE_RANGE)
            b->x = (s16)(b->x + b->dir * BEBOP_SPEED);
        if (distY > BEBOP_ALIGN_Y)       b->y += BEBOP_SPEED;
        else if (distY < -BEBOP_ALIGN_Y) b->y -= BEBOP_SPEED;

        if (++b->frameTick >= BEBOP_WALK_TICKS) {
            b->frameTick = 0;
            bebopManual(b, BEBOP_ANIM_WALK, (u8)((b->frame + 1) % 6));
        }
        if (b->timer > 0) b->timer--;
        if (b->alignOnly && abs(distY) <= BEBOP_ALIGN_Y &&
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
        b->x = (s16)(b->x + b->chargeDir * BEBOP_CHARGE_SPEED);
        b->dir = b->chargeDir;
        if (b->timer > 0) b->timer--;
        // Golpea por CONTACTO real de los cuerpos, como la de Rocksteady.
        if (!b->chargeHit) {
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                s16 pcx = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
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
            bebopGetCenterX(b) <= BEBOP_X_MIN || bebopGetCenterX(b) >= BEBOP_X_MAX) {
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
            // El aro chico (columna BEBOP_SHOT_REAR de la celda) justo en
            // la boca del arma.
            bebopShotSpawn((s16)(bebopGetCenterX(b) + b->dir * (mx - BEBOP_SHOT_REAR)),
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
        if (b->timer > 0) b->timer--;
        if (b->timer > BEBOP_KD_HOLD - 10) {
            bebopManual(b, BEBOP_ANIM_HURT, 1);          // sigue cayendo
        } else {
            bebopManual(b, BEBOP_ANIM_HURT, BEBOP_HURT_FR_DOWN);   // tirado
        }
        if (b->timer == 0) {
            b->state = BEBOP_GETUP;
            b->frameTick = 0;
            bebopManual(b, BEBOP_ANIM_HURT, BEBOP_HURT_FR_GETUP);
        }
        break;

    case BEBOP_GETUP:
        if (bebopStepFrames(b, BEBOP_ANIM_HURT, BEBOP_HURT_FR_GETUP, 5,
                            BEBOP_GETUP_TICKS)) {
            b->armorTimer = BEBOP_GETUP_ARMOR;
            b->comboHits  = 0;
            // Tortuga encima esperandolo: se levanta pegando.
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
        s16 c = clampS16b(bebopGetCenterX(b), BEBOP_X_MIN, BEBOP_X_MAX);
        b->x = (s16)(c - BEBOP_FRAME_W / 2);
        b->y = bebopClampLane(c, b->y);
    }

    bebopRender(b);
}
