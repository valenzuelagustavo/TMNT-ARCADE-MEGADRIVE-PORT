#include "robot.h"

// ===========================================================================
// ROBOT DEL LÁTIGO — implementación (sheet nuevo: 13 anims, frame 184x80)
// ===========================================================================

static s16 rabs(s16 v)                 { return (v < 0) ? -v : v; }
static s16 rclamp(s16 v, s16 a, s16 b) { return (v < a) ? a : ((v > b) ? b : v); }

// Cambia de anim (con auto-animación ON: la mayoría se reproducen solas).
static void robotSetAnim(Robot* r, u8 a, bool loop) {
    if (r->anim == a) return;
    r->anim = a;
    SPR_setAutoAnimation(r->sprite, TRUE);
    SPR_setAnim(r->sprite, a);
    SPR_setAnimationLoop(r->sprite, loop);
}

// Reinicia desde el frame 0 aunque sea la misma anim.
static void robotRestartAnim(Robot* r, u8 a, bool loop) {
    r->anim = a;
    SPR_setAutoAnimation(r->sprite, TRUE);
    SPR_setAnimAndFrame(r->sprite, a, 0);
    SPR_setAnimationLoop(r->sprite, loop);
}

// El arte del robot mira SIEMPRE a la derecha -> flip cuando mira a la izquierda.
// Como el frame es ancho (184) y el cuerpo está a la izquierda, al espejar hay
// que correr la X para que el CENTRO del cuerpo (r->x) quede en su lugar.
static void robotRender(Robot* r) {
    bool flip = (r->dir < 0);
    s16 fl = r->x - (flip ? (ROBOT_FRAME_W - ROBOT_BODY_CX) : ROBOT_BODY_CX);
    SPR_setHFlip(r->sprite, flip);
    SPR_setPosition(r->sprite, fl - r->cameraOffsetX, r->y - ROBOT_FOOT_OFFSET);
    SPR_setDepth(r->sprite, -(r->y));
}

// ---------------------------------------------------------------------------
// LÁSER — proyectil de vida independiente (sub-sprite whip_waves, anim láser)
// ---------------------------------------------------------------------------
static void robotKillLaser(Robot* r) {
    if (r->laserSpr) { SPR_releaseSprite(r->laserSpr); r->laserSpr = NULL; }
    r->laserActive = FALSE;
}

static void robotFireLaser(Robot* r) {
    if (r->laserActive) return;
    r->laserActive = TRUE;
    r->laserDir = r->dir;
    // Sale de la posición del arma (centro del cuerpo) hacia 'dir'.
    r->laserX = (r->dir > 0) ? r->x : (r->x - WHIP_SPRITE_W);
    r->laserY = r->y;
    s16 drawY = (r->y - ROBOT_FOOT_OFFSET) + 40;   // a la altura del arma
    r->laserSpr = SPR_addSprite(&whip_waves, r->laserX - r->cameraOffsetX, drawY,
                                TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (r->laserSpr) {
        SPR_setHFlip(r->laserSpr, (r->laserDir < 0));
        SPR_setAnimationLoop(r->laserSpr, FALSE);
        SPR_setAnim(r->laserSpr, WHIP_ANIM_LASER);
        SPR_setDepth(r->laserSpr, -(r->y) - 1);
    }
}

static void robotUpdateLaser(Robot* r, Player* p1, Player* p2, bool twoP) {
    if (!r->laserActive) return;
    r->laserX += r->laserDir * ROBOT_LASER_SPEED;
    if (r->laserX + WHIP_SPRITE_W < 0 || r->laserX > 1376) { robotKillLaser(r); return; }

    Player* ps[2] = { p1, p2 };
    u8 n = twoP ? 2 : 1;
    for (u8 i = 0; i < n; i++) {
        Player* p = ps[i];
        if (!p) continue;
        s16 pcx = getPlayerWorldX(p) + PLAYER_SPRITE_W / 2;
        s16 py  = getPlayerY(p);
        if (pcx >= r->laserX && pcx <= r->laserX + WHIP_SPRITE_W &&
            rabs(py - r->laserY) <= ROBOT_LASER_TOL_Y && playerCanBeHit(p)) {
            playerHitBars(p, r->laserX + WHIP_SPRITE_W / 2, ROBOT_LASER_DMG);
            robotKillLaser(r);
            return;
        }
    }
    if (r->laserSpr)
        SPR_setPosition(r->laserSpr, r->laserX - r->cameraOffsetX,
                        (r->laserY - ROBOT_FOOT_OFFSET) + 40);
}

// ---------------------------------------------------------------------------
// Rutina: caminar hacia el extremo más lejano
// ---------------------------------------------------------------------------
static void robotStartWalk(Robot* r) {
    s16 dl = rabs(r->x - ROBOT_PATROL_LEFT);
    s16 dr = rabs(r->x - ROBOT_PATROL_RIGHT);
    r->patrolTarget = (dr >= dl) ? ROBOT_PATROL_RIGHT : ROBOT_PATROL_LEFT;
    r->state = ROBOT_WALK;
    r->walkTimer = 0;
    robotSetAnim(r, ROBOT_ANIM_WALK, TRUE);
}

// Alcance actual del látigo según el frame del lanzamiento.
static s16 robotWhipReach(const Robot* r) {
    s16 reach = ROBOT_WHIP_REACH_MIN + (s16)r->throwFrame * ROBOT_WHIP_STEP;
    return (reach > ROBOT_WHIP_REACH_MAX) ? ROBOT_WHIP_REACH_MAX : reach;
}

// Frame de la anim de electrocución (la que esté seteada AHORA) cuya EXTENSIÓN
// del látigo coincide con la que tenía al enganchar. Usa throwFrame (el frame
// del throw que conectó = la distancia real robot->player en ese instante) y lo
// escala al numFrame REAL de la anim de electro, que puede diferir del throw.
// Antes se escalaba con throwFrames (frames del THROW) y se medía la distancia
// con el borde del sprite -> frame mal seteado.
static u8 robotElectroFrame(const Robot* r) {
    u8 n = (u8)r->sprite->animation->numFrame;   // frames de la anim electro actual
    if (n <= 1) return 0;
    u8 tf = (r->throwFrames > 1) ? (u8)(r->throwFrames - 1) : 1;
    u16 f = (u16)r->throwFrame * (n - 1) / tf;    // 0 = mínima ext. .. n-1 = máxima
    return (f >= n) ? (u8)(n - 1) : (u8)f;
}

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------
void robotInit(Robot* r) {
    r->sprite = NULL;
    r->state = ROBOT_INACTIVE;
    r->x = r->y = 0;
    r->cameraOffsetX = 0;
    r->dir = -1;
    r->hp = ROBOT_HP;
    r->anim = 0xFF;
    r->flashTimer = 0;
    r->timer = 0;
    r->patrolTarget = ROBOT_PATROL_LEFT;
    r->walkTimer = 0;
    r->attackCooldown = 0;
    r->drainTimer = 0;
    r->electroTgl = 0;
    r->grabFrame = 0;
    r->throwFrame = 0;
    r->throwFrames = 0;
    r->throwTick = 0;
    r->laserSpr = NULL;
    r->laserActive = FALSE;
    r->laserX = r->laserY = 0;
    r->laserDir = 1;
}

void robotSpawn(Robot* r, s16 centerX) {
    r->x = centerX;
    r->y = ROBOT_SPAWN_Y;
    r->dir = -1;
    r->hp = ROBOT_HP;
    r->state = ROBOT_APPEAR;
    r->anim = 0xFF;
    r->attackCooldown = 0;
    r->drainTimer = 0;
    r->sprite = SPR_addSprite(&robot_whip, 0, 0, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    // Comparte PAL2 (paleta de los foot soldiers), ya cargada por el nivel.
    robotRestartAnim(r, ROBOT_ANIM_APPEAR, FALSE);
    robotRender(r);
}

bool robotIsActive(const Robot* r) {
    return (r->state != ROBOT_INACTIVE && r->state != ROBOT_GONE);
}

bool robotCanBeHit(const Robot* r) {
    // Hittable mientras camina, gira o ataca; NO durante aparición, agarre,
    // golpe (i-frames) ni muerte.
    return (r->state == ROBOT_WALK || r->state == ROBOT_TURN ||
            r->state == ROBOT_WINDUP || r->state == ROBOT_THROW ||
            r->state == ROBOT_RETRACT || r->state == ROBOT_LASER);
}

s16 robotGetCenterX(const Robot* r) { return r->x; }
s16 robotGetCenterY(const Robot* r) { return r->y; }

void robotDamage(Robot* r, s16 dmg) {
    if (!robotCanBeHit(r)) return;
    r->hp -= dmg;

    if (r->hp <= 0) {
        r->state = ROBOT_DEAD;
        robotRestartAnim(r, ROBOT_ANIM_DESTROY, FALSE);
        return;
    }
    // Golpeado no fatal: HURT.
    r->state = ROBOT_HURT;
    r->timer = ROBOT_HURT_FRAMES;
    robotRestartAnim(r, ROBOT_ANIM_HURT, FALSE);
}

// Comienza el lanzamiento del látigo (control manual de frames para poder
// recogerlo al revés si no engancha).
static void robotBeginThrow(Robot* r) {
    r->state = ROBOT_THROW;
    robotRestartAnim(r, ROBOT_ANIM_WHIP_THROW, FALSE);
    SPR_setAutoAnimation(r->sprite, FALSE);   // frames a mano
    r->throwFrames = (u8)r->sprite->animation->numFrame;
    if (r->throwFrames == 0) r->throwFrames = 1;
    r->throwFrame = 0;
    r->throwTick = 0;
    SPR_setAnimAndFrame(r->sprite, ROBOT_ANIM_WHIP_THROW, 0);
}

void robotUpdate(Robot* r, s16 cameraX, Player* p1, Player* p2, bool twoPlayers, u16 fps) {
    if (r->state == ROBOT_INACTIVE || r->state == ROBOT_GONE || !r->sprite) return;
    r->cameraOffsetX = cameraX;

    if (r->attackCooldown > 0) r->attackCooldown--;

    // Jugador objetivo: el más cercano en X.
    Player* tgt = p1;
    if (twoPlayers && p2 &&
        rabs(getPlayerWorldX(p2) - r->x) < rabs(getPlayerWorldX(p1) - r->x))
        tgt = p2;
    s16 pcx  = getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2;
    s16 py   = getPlayerY(tgt);
    s16 ddx  = pcx - r->x;
    s16 distX = rabs(ddx);

    switch (r->state) {

        case ROBOT_APPEAR: {
            if (SPR_isAnimationDone(r->sprite)) {
                r->dir = (ddx >= 0) ? 1 : -1;
                robotStartWalk(r);
            }
            break;
        }

        case ROBOT_WALK: {
            s16 step = (r->patrolTarget > r->x) ? ROBOT_SPEED : -ROBOT_SPEED;
            r->dir = (step > 0) ? 1 : -1;
            r->x += step;
            // Arranque con [3] y luego [12] si el desplazamiento sigue.
            r->walkTimer++;
            robotSetAnim(r, (r->walkTimer < ROBOT_WALK_START_TICKS)
                            ? ROBOT_ANIM_WALK : ROBOT_ANIM_WALK_LONG, TRUE);
            if (rabs(r->x - r->patrolTarget) <= ROBOT_ARRIVE_MARGIN) {
                r->x = r->patrolTarget;
                r->state = ROBOT_TURN;
                r->timer = ROBOT_TURN_MAX;
                robotRestartAnim(r, ROBOT_ANIM_TURN, FALSE);
            }
            break;
        }

        case ROBOT_TURN: {
            r->dir = (ddx >= 0) ? 1 : -1;
            if      (py > r->y + ROBOT_Y_ALIGN) r->y += ROBOT_SPEED;
            else if (py < r->y - ROBOT_Y_ALIGN) r->y -= ROBOT_SPEED;
            r->y = rclamp(r->y, ROBOT_LANE_TOP, ROBOT_LANE_BOTTOM);
            if (r->timer > 0) r->timer--;
            if (SPR_isAnimationDone(r->sprite) || r->timer == 0) {
                if (distX > ROBOT_WHIP_REACH_MAX) {
                    r->state = ROBOT_LASER;
                    r->timer = 0;
                    robotRestartAnim(r, ROBOT_ANIM_LASER, FALSE);
                } else {
                    r->state = ROBOT_WINDUP;
                    robotRestartAnim(r, ROBOT_ANIM_WHIP_WINDUP, FALSE);
                }
            }
            break;
        }

        case ROBOT_WINDUP: {
            // Preparación; siempre antes del látigo. Al terminar -> lanzar.
            if (SPR_isAnimationDone(r->sprite)) robotBeginThrow(r);
            break;
        }

        case ROBOT_THROW: {
            // El látigo se estira frame a frame. En cada frame se chequea si
            // engancha; si llega al final sin contacto, se recoge (RETRACT).
            s16 reach = robotWhipReach(r);
            s16 fwd = (r->dir >= 0) ? (pcx - r->x) : (r->x - pcx);
            if (fwd >= 0 && fwd <= reach && rabs(py - r->y) <= ROBOT_WHIP_TOL_Y &&
                playerCanBeHit(tgt) && !playerIsGrabbed(tgt)) {
                // ¡Enganchó!
                playerWhipGrab(tgt);
                r->state = ROBOT_GRAB;
                r->drainTimer = 0;
                r->electroTgl = 0;
                robotRestartAnim(r, ROBOT_ANIM_CAUGHT, FALSE);
            } else if (++r->throwTick >= ROBOT_THROW_TICKS) {
                r->throwTick = 0;
                if (r->throwFrame + 1 >= r->throwFrames) {
                    r->state = ROBOT_RETRACT;   // no enganchó -> recoger
                } else {
                    r->throwFrame++;
                    SPR_setFrame(r->sprite, r->throwFrame);
                }
            }
            break;
        }

        case ROBOT_RETRACT: {
            if (++r->throwTick >= ROBOT_THROW_TICKS) {
                r->throwTick = 0;
                if (r->throwFrame == 0) {
                    robotStartWalk(r);
                    r->attackCooldown = ROBOT_ATTACK_COOLDOWN;
                } else {
                    r->throwFrame--;
                    SPR_setFrame(r->sprite, r->throwFrame);
                }
            }
            break;
        }

        case ROBOT_GRAB: {
            // Tras "atrapada" alterna las anims de electrocución (7/8). La
            // tortuga reproduce su anim 18 (la maneja playerWhipGrab).
            if (r->anim == ROBOT_ANIM_CAUGHT && SPR_isAnimationDone(r->sprite)) {
                // Congelar la electrocución en el frame cuya EXTENSIÓN del látigo
                // coincide con la que tenía al enganchar (acorde a la distancia
                // robot->player en ese momento).
                robotSetAnim(r, ROBOT_ANIM_ELECTRO_A, FALSE);
                SPR_setAutoAnimation(r->sprite, FALSE);
                r->grabFrame = robotElectroFrame(r);
                SPR_setFrame(r->sprite, r->grabFrame);
            }
            if (r->anim != ROBOT_ANIM_CAUGHT) {
                r->electroTgl++;
                if ((r->electroTgl & 7) == 0) {
                    u8 next = (r->anim == ROBOT_ANIM_ELECTRO_A)
                              ? ROBOT_ANIM_ELECTRO_B : ROBOT_ANIM_ELECTRO_A;
                    r->anim = next;
                    SPR_setAnim(r->sprite, next);
                    SPR_setAutoAnimation(r->sprite, FALSE);
                    // Re-escala al numFrame de ESTA anim (A y B pueden diferir).
                    r->grabFrame = robotElectroFrame(r);
                    SPR_setFrame(r->sprite, r->grabFrame);
                }
            }
            // Drena 1 barra por segundo.
            if (++r->drainTimer >= fps) { r->drainTimer = 0; playerElectroDrain(tgt); }
            // Zafó (mashing) o cayó KO -> soltar y seguir.
            if (!playerIsGrabbed(tgt)) {
                robotStartWalk(r);
                r->attackCooldown = ROBOT_ATTACK_COOLDOWN;
            }
            break;
        }

        case ROBOT_LASER: {
            r->timer++;
            if (r->timer == ROBOT_LASER_FIRE_DELAY) robotFireLaser(r);
            if (SPR_isAnimationDone(r->sprite)) {
                robotStartWalk(r);            // el rayo sigue viajando solo
                r->attackCooldown = ROBOT_ATTACK_COOLDOWN;
            }
            break;
        }

        case ROBOT_HURT: {
            if (r->timer > 0) r->timer--;
            else              robotStartWalk(r);
            break;
        }

        case ROBOT_DEAD: {
            if (SPR_isAnimationDone(r->sprite)) {
                SPR_releaseSprite(r->sprite); r->sprite = NULL;
                robotKillLaser(r);
                r->state = ROBOT_GONE;
                return;
            }
            break;
        }

        default: break;
    }

    robotUpdateLaser(r, p1, p2, twoPlayers);
    robotRender(r);
}
