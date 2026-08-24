#include "rocksteady.h"
#include "audio.h"   // hit_turtles (sonido de patada del jefe)

// ===========================================================================
// ROCKSTEADY — implementación (jefe del nivel 2)
// ===========================================================================

static s16 rabs(s16 v)                 { return (v < 0) ? -v : v; }
static s16 rclamp(s16 v, s16 a, s16 b) { return (v < a) ? a : ((v > b) ? b : v); }

// Velocidad de movimiento del jefe (aplica anger multiplier en fase 2).
static s16 rocksteadyMoveSpeed(const Rocksteady* r) {
    s16 spd = ROCKSTEADY_SPEED;
    if (r->angerActive) spd += ROCKSTEADY_ANGER_SPEED_MULT;
    return spd;
}
static s16 rocksteadyChargeSpeed(const Rocksteady* r) {
    s16 spd = ROCKSTEADY_CHARGE_SPEED;
    if (r->angerActive) spd += ROCKSTEADY_ANGER_SPEED_MULT;
    return spd;
}

// Cambia de anim (con auto-animación ON: las animaciones se reproducen solas
// al ritmo del 'time' del recurso, 6 frames por frame de animación).
static void rocksteadySetAnim(Rocksteady* r, u8 a, bool loop) {
    if (r->anim == a) return;
    r->anim = a;
    SPR_setAutoAnimation(r->sprite, TRUE);
    SPR_setAnim(r->sprite, a);
    SPR_setAnimationLoop(r->sprite, loop);
}

// Reinicia desde el frame 0 aunque sea la misma anim.
static void rocksteadyRestartAnim(Rocksteady* r, u8 a, bool loop) {
    r->anim = a;
    SPR_setAutoAnimation(r->sprite, TRUE);
    SPR_setAnimAndFrame(r->sprite, a, 0);
    SPR_setAnimationLoop(r->sprite, loop);
}

// El arte mira SIEMPRE a la derecha → flip cuando mira a la izquierda.
// El frame es CUADRADO (104x104) y el cuerpo está centrado → sin compensar X
// (a diferencia del frame ancho del robot).
static void rocksteadyRender(Rocksteady* r) {
    bool flip = (r->dir < 0);
    SPR_setHFlip(r->sprite, flip);
    SPR_setPosition(r->sprite, r->x - r->cameraOffsetX, r->y - ROCKSTEADY_FOOT_OFFSET);
    SPR_setDepth(r->sprite, -(r->y));
}

// ---------------------------------------------------------------------------
// BALAS — proyectil de vida independiente (sub-sprite boss_bullet, PAL3)
// ---------------------------------------------------------------------------
static struct {
    bool    active;
    Sprite* sprite;
    s16     x, y;      // mundo; y = lane (pies) del lanzador
    s8      dir;
    s8      dy;        // velocidad vertical (0=horizontal, neg=arriba, pos=abajo)
    s16     cameraOffsetX;
} bullets[MAX_ROCKSTEADY_BULLETS];

void rocksteadyBulletInit(void) {
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        bullets[i].active = 0;
        bullets[i].sprite = NULL;
    }
}

static void rocksteadyBulletSpawn(s16 x, s16 y, s8 dir, s8 dy, u8 palette) {
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        if (bullets[i].active) continue;
        bullets[i].x = x;
        bullets[i].y = y;
        bullets[i].dir = dir;
        bullets[i].dy = dy;
        bullets[i].cameraOffsetX = 0;
        bullets[i].active = 1;
        bullets[i].sprite = SPR_addSprite(&boss_bullet,
                                          x, y - ROCKSTEADY_FOOT_OFFSET + 40,
                                          TILE_ATTR(palette, FALSE, FALSE, FALSE));
        if (bullets[i].sprite) {
            SPR_setDepth(bullets[i].sprite, -(y) - 1);
            SPR_setHFlip(bullets[i].sprite, (dir < 0));
        }
        return;   // slot encontrado
    }
}

void rocksteadyBulletUpdate(s16 camX) {
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        if (!bullets[i].active) continue;
        bullets[i].cameraOffsetX = camX;
        bullets[i].x += bullets[i].dir * ROCKSTEADY_BULLET_SPEED;
        bullets[i].y += bullets[i].dy;

        // Fuera de pantalla (con margen de 32px a cada lado, o fuera del lane)
        if (bullets[i].x < camX - 32 || bullets[i].x > camX + 320 + 32 ||
            bullets[i].y < ROCKSTEADY_LANE_TOP - 32 || bullets[i].y > ROCKSTEADY_LANE_BOTTOM + 32) {
            if (bullets[i].sprite) SPR_releaseSprite(bullets[i].sprite);
            bullets[i].sprite = NULL;
            bullets[i].active = 0;
            continue;
        }
        if (bullets[i].sprite)
            SPR_setPosition(bullets[i].sprite,
                            bullets[i].x - bullets[i].cameraOffsetX,
                            bullets[i].y - ROCKSTEADY_FOOT_OFFSET + 40);
    }
}

void rocksteadyBulletReleaseAll(void) {
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        if (bullets[i].sprite) SPR_releaseSprite(bullets[i].sprite);
        bullets[i].sprite = NULL;
        bullets[i].active = 0;
    }
}

bool rocksteadyBulletCheckHitPlayer(s16 px, s16 py, s16* hitX) {
    // px = borde izquierdo del frame del jugador (104px)
    s16 pcx = px + PLAYER_SPRITE_W / 2;   // centro del jugador
    s16 pcy = py;                         // pies del jugador

    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        if (!bullets[i].active) continue;

        s16 bcx = bullets[i].x + 8;   // centro de la bala (16px wide → +8)
        s16 bcy = bullets[i].y;

        if (abs(pcx - bcx) < 16 && abs(pcy - bcy) < 16) {
            // Impacto: destruir la bala
            if (hitX) *hitX = bcx;
            if (bullets[i].sprite) SPR_releaseSprite(bullets[i].sprite);
            bullets[i].sprite = NULL;
            bullets[i].active = 0;
            return TRUE;
        }
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// API pública del jefe
// ---------------------------------------------------------------------------
void rocksteadyInit(Rocksteady* r) {
    r->sprite = NULL;
    r->state = ROCKSTEADY_INACTIVE;
    r->phase = 1;
    r->x = r->y = 0;
    r->cameraOffsetX = 0;
    r->dir = -1;
    r->hp = ROCKSTEADY_HP;
    r->anim = 0xFF;
    r->timer = 0;
    r->attackCooldown = 0;
    r->hitsTaken = 0;
    r->knockdowns = 0;
    r->comboHits = 0;
    r->counterPending = 0;
    r->moveToggle = 0;
    r->chargeHit = 0;
    r->shotFrame = 0;
    r->shotsFired = 0;
    r->shotTimer = 0;
    r->farTimer = 0;
    r->angerHits = 0;
    r->angerActive = 0;
    r->barrageCount = 0;
    r->barrageTimer = 0;
    r->barrageShots = 0;
    r->barrageShotTimer = 0;
    r->retreatTargetX = 0;
    r->retreatBaseY = 0;
    r->cornerChargesLeft = 0;
    r->cornerChargeTimer = 0;
}

void rocksteadySpawn(Rocksteady* r) {
    r->x = ROCKSTEADY_SPAWN_X;   // 8 tiles a la izquierda de la cápsula (puerta abierta)
    r->y = 148;   // Subido junto con la cápsula (4 tiles): pies alineados a la puerta
    r->dir = -1;
    r->phase = 1;
    r->hp = ROCKSTEADY_HP;
    r->hitsTaken = 0;
    r->knockdowns = 0;
    r->comboHits = 0;
    r->counterPending = 0;
    r->attackCooldown = 0;
    r->moveToggle = 0;
    r->chargeHit = 0;
    r->farTimer = 0;
    r->angerHits = 0;
    r->angerActive = 0;
    r->barrageCount = 0;
    r->barrageTimer = 0;
    r->barrageShots = 0;
    r->barrageShotTimer = 0;
    r->retreatTargetX = 0;
    r->retreatBaseY = 0;
    r->cornerChargesLeft = 0;
    r->cornerChargeTimer = 0;
    r->state = ROCKSTEADY_EMERGE;
    r->timer = ROCKSTEADY_EMERGE_STAND;   // quieto en la puerta (taunt) antes de bajar
    r->anim = 0xFF;
    r->sprite = SPR_addSprite(&rocksteady_boss, 0, 0,
                              TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    // PAL3 ya fue cargada con la paleta del boss (PAL3[1] = blanco, HUD).
    if (r->sprite) {
        // Aparece parado en la puerta, reproduciendo su IDLE (no camina todavía).
        rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_IDLE, TRUE);
        rocksteadyRender(r);
    }
}

bool rocksteadyIsActive(const Rocksteady* r) {
    return (r->state != ROCKSTEADY_INACTIVE && r->state != ROCKSTEADY_GONE);
}

bool rocksteadyCanBeHit(const Rocksteady* r) {
    // Golpeable mientras camina, decide, ataca o dispara; NO durante el
    // flinch (i-frames), el knock-down, la transición de arma ni la muerte.
    return (r->state == ROCKSTEADY_EMERGE || r->state == ROCKSTEADY_IDLE ||
            r->state == ROCKSTEADY_APPROACH || r->state == ROCKSTEADY_CHARGE ||
            r->state == ROCKSTEADY_KICK || r->state == ROCKSTEADY_AIM_WALK ||
            r->state == ROCKSTEADY_SHOOT || r->state == ROCKSTEADY_KICK_ARMS ||
            r->state == ROCKSTEADY_RETREAT || r->state == ROCKSTEADY_CORNER_CHARGE);
}

// Centro VISUAL del cuerpo: r->x ancla el BORDE IZQUIERDO del frame de 104px
// (SPR_setPosition esquina superior izquierda, sin compensación como el robot),
// así que el centro del cuerpo queda a +FRAME_W/2. Devolver el borde como si
// fuera centro desplazaba la hurtbox 52px: por la derecha de la pantalla los
// golpes exigían solaparse con el jefe y por la izquierda conectaban desde
// más lejos del contacto real.
s16 rocksteadyGetCenterX(const Rocksteady* r) { return r->x + ROCKSTEADY_FRAME_W / 2; }
// Y = pies (lane), igual que las tortugas y los foot soldiers.
s16 rocksteadyGetCenterY(const Rocksteady* r) { return r->y; }

// ¿Ya corresponde pasar a la fase 2? Mitad de HP O 4 knock-downs (lo primero).
static bool rocksteadyGoPhase2(const Rocksteady* r) {
    return (r->hp <= ROCKSTEADY_PHASE2_HP || r->knockdowns >= ROCKSTEADY_KD_MAX);
}

void rocksteadyDamage(Rocksteady* r, s16 dmg) {
    if (!rocksteadyCanBeHit(r)) return;
    r->hp -= dmg;

    if (r->hp <= 0) {
        r->hp = 0;
        r->state = ROCKSTEADY_DEAD;
        rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_HURT, FALSE);
        return;
    }

    r->hitsTaken++;
    // Contraataque: golpes SEGUIDOS sin poder responder → al terminar este
    // flinch suelta la patada (lo ejecuta update antes del switch). Se resetea
    // si cae (knock-down) para que no acumule de una caída a otra.
    r->comboHits++;
    if (r->comboHits >= ROCKSTEADY_COUNTER_HITS) {
        r->comboHits = 0;
        r->counterPending = 1;
    }
    // Anger: en fase 2, tras ROCKSTEADY_ANGER_THRESHOLD golpes se mueve más rápido.
    if (r->phase == 2 && !r->angerActive) {
        r->angerHits++;
        if (r->angerHits >= ROCKSTEADY_ANGER_THRESHOLD) r->angerActive = 1;
    }
    // Fase 1: cada ROCKSTEADY_KD_INTERVAL golpes el jefe CAE (knock-down).
    if (r->phase == 1 && r->hitsTaken >= ROCKSTEADY_KD_INTERVAL) {
        r->hitsTaken = 0;
        r->comboHits = 0;      // tirado: la cuenta de "seguidos" arranca de nuevo
        r->counterPending = 0; // desde el piso no hay contraataque
        r->knockdowns++;
        r->state = ROCKSTEADY_KNOCKDOWN;
        r->timer = ROCKSTEADY_KD_HOLD;
        rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_HURT, FALSE);
        return;
    }

    // Flinch normal (anim según la fase). ARMADURA durante la patada: si está
    // pateando (normal o con arma) recibe el daño pero NO se interrumpe el
    // swing. Sin esto, el golpe siguiente del combo cancelaba la patada
    // contraataque antes de terminar (la anim dura ~48 ticks de juego, un hit
    // llega cada ~20) y el jefe quedaba en stagger eterno sin responder.
    if (r->state != ROCKSTEADY_KICK && r->state != ROCKSTEADY_KICK_ARMS) {
        r->state = (r->phase == 1) ? ROCKSTEADY_HURT : ROCKSTEADY_HURT_ARMS;
        r->timer = ROCKSTEADY_HURT_FRAMES;
        rocksteadyRestartAnim(r, (r->phase == 1)
                                ? ROCKSTEADY_ANIM_HURT : ROCKSTEADY_ANIM_HURT_ARMS,
                              FALSE);
    }
}

// ---------------------------------------------------------------------------
// Inicio de los ataques
// ---------------------------------------------------------------------------
static void rocksteadyStartCharge(Rocksteady* r) {
    r->state = ROCKSTEADY_CHARGE;
    r->timer = ROCKSTEADY_CHARGE_MAX;
    r->chargeHit = 0;
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_CHARGE, TRUE);
}

static void rocksteadyStartApproach(Rocksteady* r) {
    r->state = ROCKSTEADY_APPROACH;
    rocksteadySetAnim(r, ROCKSTEADY_ANIM_WALK, TRUE);
}

static void rocksteadyStartKick(Rocksteady* r) {
    r->state = ROCKSTEADY_KICK;
    r->timer = 0;   // contador del frame de impacto
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_KICK, FALSE);
}

static void rocksteadyStartKickArms(Rocksteady* r) {
    r->state = ROCKSTEADY_KICK_ARMS;
    r->timer = 0;
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_KICK_ARMS, FALSE);
}

static void rocksteadyStartAimWalk(Rocksteady* r) {
    r->state = ROCKSTEADY_AIM_WALK;
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_AIM, TRUE);
}

static void rocksteadyStartShoot(Rocksteady* r) {
    r->state = ROCKSTEADY_SHOOT;
    r->shotFrame = 0;
    r->shotsFired = 0;
    r->shotTimer = 0;
    // Frames a MANO para sincronizar la salida de cada bala con la anim.
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_SHOOT, FALSE);
    SPR_setAutoAnimation(r->sprite, FALSE);
}

// Transición a la fase 2: el jefe saca el arma (anim [5]).
static void rocksteadyStartArmsIntro(Rocksteady* r) {
    r->state = ROCKSTEADY_ARMS_INTRO;
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_DRAW, FALSE);
}

// Retirada al fondo: se aleja y dispara 3-4 ráfagas con patrón Y.
static void rocksteadyStartRetreat(Rocksteady* r, s16 playerX) {
    r->state = ROCKSTEADY_RETREAT;
    r->retreatTargetX = (playerX > rocksteadyGetCenterX(r)) ? ROCKSTEADY_PATROL_LEFT : ROCKSTEADY_PATROL_RIGHT;
    r->retreatBaseY = r->y;
    r->barrageCount = 0;
    r->barrageTimer = 30;   // tiempo para llegar al fondo antes de disparar
    r->barrageShots = 0;
    r->barrageShotTimer = 0;
    rocksteadySetAnim(r, ROCKSTEADY_ANIM_WALK_ARMS, TRUE);
}

// Carga desde la esquina: se va a la esquina, guarda arma, carga varias veces.
static void rocksteadyStartCornerCharge(Rocksteady* r) {
    r->state = ROCKSTEADY_CORNER_CHARGE;
    r->retreatTargetX = ROCKSTEADY_CORNER_X;
    if (r->cornerChargesLeft == 0)
        r->cornerChargesLeft = ROCKSTEADY_CORNER_CHARGES;   // inicio de secuencia
    r->cornerChargeTimer = 0;
    rocksteadySetAnim(r, ROCKSTEADY_ANIM_WALK_ARMS, TRUE);
}

// Vuelve a IDLE tras un flinch / cooldown.
static void rocksteadyToIdle(Rocksteady* r) {
    r->state = ROCKSTEADY_IDLE;
    r->timer = ROCKSTEADY_IDLE_MIN;
    if (r->attackCooldown < ROCKSTEADY_ATTACK_COOLDOWN)
        r->attackCooldown = ROCKSTEADY_ATTACK_COOLDOWN;
    // Anim de reposo según la fase.
    rocksteadySetAnim(r, (r->phase == 1)
                         ? ROCKSTEADY_ANIM_IDLE : ROCKSTEADY_ANIM_WALK_ARMS,
                      TRUE);
}

// Tras un flinch/knock-down en fase 1: si todavía no bajó a la lane de pelea
// (le pegaron durante la intro, en la puerta a y=148), vuelve a EMERGE en modo
// "bajar al arena" en vez de decidir ataques desde arriba (las patadas no
// conectarían por la tolerancia de Y). Si ya está en la lane, a IDLE normal.
static void rocksteadyResumeFromHit(Rocksteady* r) {
    if (r->phase == 1 && r->y < ROCKSTEADY_LANE_BOTTOM) {
        r->state = ROCKSTEADY_EMERGE;
        r->timer = 0;
    } else {
        rocksteadyToIdle(r);
    }
}

// ---------------------------------------------------------------------------
// UPDATE PRINCIPAL
// ---------------------------------------------------------------------------
void rocksteadyUpdate(Rocksteady* r, s16 cameraX, Player* p1, Player* p2,
                      bool twoPlayers) {
    if (r->state == ROCKSTEADY_INACTIVE || r->state == ROCKSTEADY_GONE ||
        !r->sprite) return;
    r->cameraOffsetX = cameraX;

    if (r->attackCooldown > 0) r->attackCooldown--;

    // Jugador objetivo: el más cercano en X (centro del frame).
    Player* tgt = p1;
    if (twoPlayers && p2 &&
        rabs(getPlayerWorldX(p2) + PLAYER_SPRITE_W / 2 - r->x) <
        rabs(getPlayerWorldX(p1) + PLAYER_SPRITE_W / 2 - r->x))
        tgt = p2;
    s16 pcx  = getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2;
    s16 py   = getPlayerY(tgt);
    // Centro VISUAL del cuerpo (r->x ancla el borde izquierdo del frame):
    // todas las distancias de decisión e impacto se miden desde acá.
    s16 bcx  = r->x + ROCKSTEADY_FRAME_W / 2;
    s16 distX = rabs(pcx - bcx);

    // CONTRAATAQUE: recibió ROCKSTEADY_COUNTER_HITS golpes seguidos → corta
    // el flinch y suelta la patada hacia el jugador más cercano (fase 1:
    // patada; fase 2: patada con el arma). Evita que la tortuga encadene
    // golpes sin dejarlo responder.
    if (r->counterPending &&
        (r->state == ROCKSTEADY_HURT || r->state == ROCKSTEADY_HURT_ARMS)) {
        r->counterPending = 0;
        r->dir = (pcx >= bcx) ? 1 : -1;
        if (r->phase == 1) rocksteadyStartKick(r);
        else               rocksteadyStartKickArms(r);
    }

    // ANTI-CAMPING: si el objetivo se mantiene FUERA de alcance mientras el
    // jefe está neutral (decidiendo o acercándose), suelta la EMBESTIDA para
    // castigar la distancia. Al entrar en contacto el contador vuelve a cero
    // (y también se pausa durante flinch/knock-down/ataques en curso).
    if ((r->state == ROCKSTEADY_IDLE || r->state == ROCKSTEADY_APPROACH ||
         r->state == ROCKSTEADY_AIM_WALK) &&
        distX > ROCKSTEADY_KICK_RANGE) {
        if (r->farTimer < ROCKSTEADY_FAR_FRAMES) r->farTimer++;
        if (r->farTimer >= ROCKSTEADY_FAR_FRAMES) {
            r->farTimer = 0;
            rocksteadyStartCharge(r);
        }
    } else {
        r->farTimer = 0;
    }

    switch (r->state) {

        case ROCKSTEADY_EMERGE: {
            // Salió por la puerta ALTA de la cápsula (spawn y=148): se queda
            // QUIETO reproduciendo su IDLE mientras suena el taunt (timer =
            // ROCKSTEADY_EMERGE_STAND ≈ duración de say_your_p). Después baja
            // a la lane de pelea (180) y entra en IDLE (comienza la batalla).
            if (r->timer > 0) { r->timer--; break; }
            // Al empezar a moverse por el nivel usa la anim de CAMINAR [1]
            // (ya no se queda con el IDLE del taunt).
            rocksteadySetAnim(r, ROCKSTEADY_ANIM_WALK, TRUE);
            r->dir = (pcx >= bcx) ? 1 : -1;
            r->x += r->dir * rocksteadyMoveSpeed(r);
            if (r->y < ROCKSTEADY_LANE_BOTTOM) r->y = rclamp(r->y + rocksteadyMoveSpeed(r), 148, ROCKSTEADY_LANE_BOTTOM);
            if (r->y >= ROCKSTEADY_LANE_BOTTOM) rocksteadyToIdle(r);
            break;
        }

        case ROCKSTEADY_IDLE: {
            // Sin cooldown ni timer de decisión → elegir ataque.
            if (r->attackCooldown > 0 || r->timer > 0) {
                if (r->timer > 0) r->timer--;
                break;
            }
            // ¿Ya le tocó pasar de fase (aunque esté quieto)?
            if (r->phase == 1 && rocksteadyGoPhase2(r)) {
                rocksteadyStartArmsIntro(r);
                break;
            }
            if (r->phase == 1) {
                // Fase 1 sin arma: SOLO acercarse. La patada ya no sale por
                // decisión propia (es exclusivamente el contraataque tras
                // ROCKSTEADY_COUNTER_HITS golpes seguidos); si el jugador se
                // mantiene lejos, el timer anti-camping dispara la embestida.
                if (distX > ROCKSTEADY_KICK_RANGE) rocksteadyStartApproach(r);
            } else {
                // Fase 2 CON ARMA: DISPARAR (alineando primero el eje Y del
                // jugador) o EMBESTIR si se mantiene lejos (anti-camping de
                // arriba). La patada con arma también es SOLO contraataque.
                // El enojo conserva la retirada al fondo y la carga de esquina.
                if (r->cornerChargesLeft > 0) {
                    rocksteadyStartCornerCharge(r);
                } else if (r->angerActive && (r->moveToggle & 3) == 3) {
                    // Con anger: cada 4to turno se retira al fondo y dispara.
                    rocksteadyStartRetreat(r, pcx);
                } else if (r->angerActive && (r->moveToggle & 7) == 7) {
                    // Cada 8vo turno (solo con anger): se va a la esquina y carga.
                    rocksteadyStartCornerCharge(r);
                } else {
                    rocksteadyStartAimWalk(r);
                }
                r->moveToggle++;
            }
            break;
        }

        case ROCKSTEADY_APPROACH: {
            // Camina hacia el jugador (fase 1, sin arma) alineando lane; al
            // llegar a contacto se PLANTA y espera: la patada es SOLO el
            // contraataque por golpes seguidos, no un ataque espontáneo.
            r->dir = (pcx >= bcx) ? 1 : -1;
            s16 spd = rocksteadyMoveSpeed(r);
            r->x += r->dir * spd;
            if      (py > r->y + 2) r->y += spd;
            else if (py < r->y - 2) r->y -= spd;
            r->y = rclamp(r->y, ROCKSTEADY_LANE_TOP, ROCKSTEADY_LANE_BOTTOM);
            r->x = rclamp(r->x, ROCKSTEADY_PATROL_LEFT, ROCKSTEADY_PATROL_RIGHT);
            if (distX <= ROCKSTEADY_KICK_RANGE) rocksteadyToIdle(r);
            break;
        }

        case ROCKSTEADY_CHARGE: {
            // Estampida: corre hacia el centro del jugador re-aimando cada
            // frame (cubre los cambios de lane). Al impactar NO frena en seco:
            // sigue un tramo más (overshoot, ROCKSTEADY_CHARGE_OVER) para que
            // la carga recorra más eje X, dañando una sola vez (chargeHit).
            r->dir = (pcx >= bcx) ? 1 : -1;
            s16 chgSpd = rocksteadyChargeSpeed(r);
            r->x += r->dir * chgSpd;
            s16 ms = rocksteadyMoveSpeed(r);
            if      (py > r->y + 2) r->y += ms;
            else if (py < r->y - 2) r->y -= ms;
            r->y = rclamp(r->y, ROCKSTEADY_LANE_TOP, ROCKSTEADY_LANE_BOTTOM);

            if (!r->chargeHit &&
                rabs(pcx - bcx) < ROCKSTEADY_CHARGE_HIT_RANGE &&
                rabs(py - r->y) < ROCKSTEADY_HIT_TOL_Y &&
                playerCanBeHit(tgt)) {
                playerHitBars(tgt, r->x, ROCKSTEADY_BULLET_DMG);
                r->chargeHit = 1;
                r->timer = ROCKSTEADY_CHARGE_OVER;   // sigue embistiendo un tramo
            }
            if (--r->timer == 0 || r->x <= ROCKSTEADY_PATROL_LEFT ||
                r->x >= ROCKSTEADY_PATROL_RIGHT) {
                r->x = rclamp(r->x, ROCKSTEADY_PATROL_LEFT, ROCKSTEADY_PATROL_RIGHT);
                r->chargeHit = 0;
                rocksteadyToIdle(r);
            }
            break;
        }

        case ROCKSTEADY_KICK:
        case ROCKSTEADY_KICK_ARMS: {
            // Un solo golpe, en el frame 2 de la anim (de 8). Ventana corta
            // medida desde el CENTRO del cuerpo: sólo conecta en contacto
            // real (antes se medía desde el borde izquierdo del frame y la
            // patada pegaba desde muy lejos). El impacto DERRIBA a la tortuga.
            if (r->timer < 3) r->timer++;
            if (r->timer == 2 &&
                rabs(pcx - bcx) < ROCKSTEADY_KICK_RANGE &&
                rabs(py - r->y) < ROCKSTEADY_HIT_TOL_Y &&
                playerCanBeHit(tgt)) {
                playerHitBarsKnockdown(tgt, bcx, ROCKSTEADY_BULLET_DMG);
                // Impacto: mismo "pum" que la patada de las tortugas.
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                               SOUND_PCM_CH2, 15, FALSE, FALSE);
            }
            if (SPR_isAnimationDone(r->sprite)) rocksteadyToIdle(r);
            break;
        }

        case ROCKSTEADY_HURT:
        case ROCKSTEADY_HURT_ARMS: {
            if (r->timer > 0) r->timer--;
            if (r->timer == 0) {
                // Fase 1: si ya perdió la mitad aunque nunca haya caído → arma.
                if (r->phase == 1 && rocksteadyGoPhase2(r)) {
                    rocksteadyStartArmsIntro(r);
                } else {
                    rocksteadyResumeFromHit(r);
                }
            }
            break;
        }

        case ROCKSTEADY_KNOCKDOWN: {
            if (r->timer > 0) { r->timer--; break; }
            // Se levanta: con el arma (fase 2) o a seguir peleando.
            if (rocksteadyGoPhase2(r)) rocksteadyStartArmsIntro(r);
            else rocksteadyResumeFromHit(r);
            break;
        }

        case ROCKSTEADY_ARMS_INTRO: {
            if (SPR_isAnimationDone(r->sprite)) {
                r->phase = 2;
                rocksteadyToIdle(r);
            }
            break;
        }

        case ROCKSTEADY_AIM_WALK: {
            // Se acerca apuntando hasta quedar en rango de disparo Y
            // ALINEADO con el eje Y del jugador (para que el tiro recto vaya
            // a su altura). Recién entonces abre fuego.
            r->dir = (pcx >= bcx) ? 1 : -1;
            s16 spd = rocksteadyMoveSpeed(r);
            r->x += r->dir * spd;
            if      (py > r->y + 2) r->y += spd;
            else if (py < r->y - 2) r->y -= spd;
            r->y = rclamp(r->y, ROCKSTEADY_LANE_TOP, ROCKSTEADY_LANE_BOTTOM);
            r->x = rclamp(r->x, ROCKSTEADY_PATROL_LEFT, ROCKSTEADY_PATROL_RIGHT);
            if (distX <= ROCKSTEADY_SHOOT_RANGE &&
                rabs(py - r->y) <= ROCKSTEADY_SHOOT_ALIGN_Y)
                rocksteadyStartShoot(r);
            break;
        }

        case ROCKSTEADY_SHOOT: {
            // Frames a mano: cada ROCKSTEADY_SHOT_TICKS avanza un frame de la
            // anim y en los frames 3/5/7 sale una bala del cañón. Dirección de
            // CADA bala según el estado VIVO del jugador: en el piso → recta;
            // saltando → diagonal hacia arriba (anti-aéreo).
            if (++r->shotTimer >= ROCKSTEADY_SHOT_TICKS) {
                r->shotTimer = 0;
                r->shotFrame++;
                u8 nf = (u8)r->sprite->animation->numFrame;
                if (r->shotFrame >= nf) {
                    rocksteadyToIdle(r);
                    break;
                }
                SPR_setFrame(r->sprite, r->shotFrame);
                if (r->shotsFired < ROCKSTEADY_SHOT_COUNT &&
                    (r->shotFrame == ROCKSTEADY_SHOT_FRAME_A ||
                     r->shotFrame == ROCKSTEADY_SHOT_FRAME_B ||
                     r->shotFrame == ROCKSTEADY_SHOT_FRAME_C)) {
                    s16 gx = bcx + r->dir * 22;
                    s8 dy = isPlayerJumping(tgt) ? ROCKSTEADY_UPSHOT_DY : 0;
                    rocksteadyBulletSpawn(gx, r->y, r->dir, dy, PAL3);
                    r->shotsFired++;
                }
            }
            break;
        }

        case ROCKSTEADY_RETREAT: {
            // Se retira al extremo opuesto del jugador y dispara 3-4 ráfagas.
            // Cada ráfaga: 3 balas en la MISMA dirección (alterna horizontal /
            // diagonal arriba entre ráfagas). Entre ráfagas: reposiciona Y.
            if (r->barrageCount >= ROCKSTEADY_BARRAGE_COUNT) {
                rocksteadyToIdle(r);
                break;
            }
            // Fase de aproximación al fondo: aún no llegó.
            if (r->barrageTimer > 0 && r->barrageCount == 0 && r->barrageShots == 0) {
                r->dir = (r->retreatTargetX >= r->x) ? 1 : -1;
                s16 spd = rocksteadyMoveSpeed(r);
                r->x += r->dir * spd;
                r->x = rclamp(r->x, ROCKSTEADY_PATROL_LEFT, ROCKSTEADY_PATROL_RIGHT);
                if (rabs(r->retreatTargetX - r->x) < spd + 2) {
                    r->x = r->retreatTargetX;
                    r->barrageTimer = 20;   // pausa antes de la 1ra ráfaga
                }
                break;
            }
            // Espera entre ráfagas.
            if (r->barrageTimer > 0) { r->barrageTimer--; break; }
            // Disparar la ráfaga actual: 3 balas en secuencia rápida, todas
            // en la misma dirección (alterna por ráfaga).
            if (r->barrageShots < ROCKSTEADY_BARRAGE_SHOTS) {
                if (r->barrageShotTimer > 0) { r->barrageShotTimer--; break; }
                s16 gx = bcx + r->dir * 22;
                s8 dy = (r->barrageCount & 1) ? ROCKSTEADY_UPSHOT_DY : 0;
                rocksteadyBulletSpawn(gx, r->y, r->dir, dy, PAL3);
                r->barrageShots++;
                r->barrageShotTimer = 8;
                break;
            }
            // Ráfaga completa: siguiente ráfaga, reposiciona verticalmente.
            r->barrageCount++;
            r->barrageShots = 0;
            r->barrageShotTimer = 0;
            if (r->barrageCount < ROCKSTEADY_BARRAGE_COUNT) {
                r->barrageTimer = ROCKSTEADY_BARRAGE_INTERVAL;
                // Reposiciona verticalmente: alterna entre 3 posiciones.
                if (r->barrageCount % 2 == 0) r->y = r->retreatBaseY;
                else if (r->barrageCount % 3 == 0) r->y = ROCKSTEADY_LANE_TOP + 10;
                else r->y = ROCKSTEADY_LANE_BOTTOM - 10;
                r->y = rclamp(r->y, ROCKSTEADY_LANE_TOP, ROCKSTEADY_LANE_BOTTOM);
            }
            break;
        }

        case ROCKSTEADY_CORNER_CHARGE: {
            // Se va a la esquina, guarda arma (camina sin arma), y carga varias veces.
            if (r->cornerChargesLeft == 0) {
                rocksteadyToIdle(r);
                break;
            }
            // Fase de aproximación a la esquina: camina con arma hacia el fondo.
            if (r->cornerChargeTimer == 0 && r->cornerChargesLeft == ROCKSTEADY_CORNER_CHARGES) {
                r->dir = -1;   // siempre va hacia la izquierda (esquina)
                s16 spd = rocksteadyMoveSpeed(r);
                r->x += r->dir * spd;
                r->x = rclamp(r->x, ROCKSTEADY_PATROL_LEFT, ROCKSTEADY_PATROL_RIGHT);
                if (rabs(r->retreatTargetX - r->x) < spd + 2) {
                    r->x = r->retreatTargetX;
                    r->cornerChargeTimer = 30;   // pausa antes de la 1ra carga
                    r->cornerChargesLeft--;       // 1ra carga ya contabilizada
                }
                break;
            }
            // Espera entre cargas.
            if (r->cornerChargeTimer > 0) { r->cornerChargeTimer--; break; }
            // Ejecutar carga: cambia a anim de charge sin arma.
            rocksteadySetAnim(r, ROCKSTEADY_ANIM_CHARGE, TRUE);
            r->dir = 1;   // siempre carga hacia la derecha (hacia el jugador)
            r->chargeHit = 0;
            r->timer = ROCKSTEADY_CHARGE_MAX;
            r->state = ROCKSTEADY_CHARGE;
            break;
        }

        case ROCKSTEADY_DEAD: {
            if (SPR_isAnimationDone(r->sprite)) {
                SPR_releaseSprite(r->sprite);
                r->sprite = NULL;
                rocksteadyBulletReleaseAll();
                r->state = ROCKSTEADY_GONE;
                return;
            }
            break;
        }

        default: break;
    }

    rocksteadyRender(r);
}
