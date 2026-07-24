#include "robot.h"

// ===========================================================================
// ROBOT DEL LÁTIGO — implementación
// ===========================================================================

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static s16 rabs(s16 v)                    { return (v < 0) ? -v : v; }
static s16 rclamp(s16 v, s16 a, s16 b)    { return (v < a) ? a : ((v > b) ? b : v); }

static void robotSetAnim(Robot* r, u8 a, bool loop) {
    if (r->anim == a) return;
    r->anim = a;
    SPR_setAnim(r->sprite, a);
    SPR_setAnimationLoop(r->sprite, loop);
}

// Reinicia desde el frame 0 aunque sea la misma anim (para anims sin loop que
// quedan congeladas en el último frame y hay que volver a disparar).
static void robotRestartAnim(Robot* r, u8 a, bool loop) {
    r->anim = a;
    SPR_setAnimAndFrame(r->sprite, a, 0);
    SPR_setAnimationLoop(r->sprite, loop);
}

// Dirección hacia la que MIRA el arte de cada animación. El idle está dibujado
// mirando a la IZQUIERDA; el resto (giro, caminar, ataques, etc.) a la DERECHA.
// Ajustar acá si alguna animación quedara dibujada al revés.
static s8 robotArtDir(u8 a) {
    return (a == ROBOT_ANIM_IDLE) ? -1 : 1;
}

// ---------------------------------------------------------------------------
// Sub-sprite del LÁTIGO / LÁSER (whip_waves) — creación y posicionado
// ---------------------------------------------------------------------------
static void robotReleaseWhip(Robot* r) {
    if (r->whipSpr) { SPR_releaseSprite(r->whipSpr); r->whipSpr = NULL; }
}

static void robotKillLaser(Robot* r) {
    if (r->laserSpr) { SPR_releaseSprite(r->laserSpr); r->laserSpr = NULL; }
    r->laserActive = FALSE;
}

// Coloca el sub-sprite del látigo saliendo de la "mano" del robot, extendido
// hacia 'dir'. El arte del látigo se dibuja desde su base (izquierda); cuando
// el robot mira a la izquierda se espeja y se ancla por el borde derecho.
static void robotWhipPos(Robot* r, Sprite* spr) {
    s16 handWorldX = (r->dir > 0) ? (r->x + ROBOT_SPRITE_W - 8) : (r->x + 8);
    s16 sy = (r->y - ROBOT_FOOT_OFFSET) + WHIP_HAND_Y_OFFSET;
    if (r->dir > 0) {
        SPR_setHFlip(spr, FALSE);
        SPR_setPosition(spr, handWorldX - r->cameraOffsetX, sy);
    } else {
        SPR_setHFlip(spr, TRUE);
        SPR_setPosition(spr, (handWorldX - WHIP_SPRITE_W) - r->cameraOffsetX, sy);
    }
    SPR_setDepth(spr, -(r->y) - 1);   // delante del robot
}

static void robotSpawnWhip(Robot* r) {
    robotReleaseWhip(r);
    r->whipSpr = SPR_addSprite(&whip_waves, 0, 0, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (r->whipSpr) {
        SPR_setAnimationLoop(r->whipSpr, FALSE);
        SPR_setAnimAndFrame(r->whipSpr, WHIP_ANIM_SEARCH, 0);
        robotWhipPos(r, r->whipSpr);
    }
    r->whipElectroTgl = 0;
}

// Extiende el látigo según la distancia (frame = distancia / 32, tope 2).
static void robotUpdateWhipSearch(Robot* r, s16 distX) {
    if (!r->whipSpr) return;
    s16 fr = distX / ROBOT_WHIP_STEP;
    fr = rclamp(fr, 0, 2);
    SPR_setAnimAndFrame(r->whipSpr, WHIP_ANIM_SEARCH, (u16)fr);
    robotWhipPos(r, r->whipSpr);
}

// Electrocución: intercala los frames A/B (una y una) al máximo de extensión.
static void robotUpdateWhipElectro(Robot* r) {
    if (!r->whipSpr) return;
    r->whipElectroTgl++;
    u8 e = (u8)((r->whipElectroTgl >> 2) & 1);   // cambia cada ~4 frames
    SPR_setAnimAndFrame(r->whipSpr, e ? WHIP_ANIM_ELECTRO_B : WHIP_ANIM_ELECTRO_A, 2);
    robotWhipPos(r, r->whipSpr);
}

// ---------------------------------------------------------------------------
// LÁSER — proyectil de vida independiente
// ---------------------------------------------------------------------------
static void robotFireLaser(Robot* r) {
    if (r->laserActive) return;
    r->laserActive = TRUE;
    r->laserDir = r->dir;
    s16 handWorldX = (r->dir > 0) ? (r->x + ROBOT_SPRITE_W - 8) : (r->x + 8);
    r->laserX = (r->dir > 0) ? handWorldX : (handWorldX - WHIP_SPRITE_W);
    r->laserY = r->y;
    r->laserSpr = SPR_addSprite(&whip_waves,
                                r->laserX - r->cameraOffsetX,
                                (r->laserY - ROBOT_FOOT_OFFSET) + WHIP_HAND_Y_OFFSET,
                                TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (r->laserSpr) {
        SPR_setHFlip(r->laserSpr, (r->laserDir < 0));
        SPR_setAnimationLoop(r->laserSpr, FALSE);   // se forma y queda en el último frame
        SPR_setAnim(r->laserSpr, WHIP_ANIM_LASER);
        SPR_setDepth(r->laserSpr, -(r->laserY) - 1);
    }
}

static void robotUpdateLaser(Robot* r, Player* p1, Player* p2, bool twoP) {
    if (!r->laserActive) return;

    r->laserX += r->laserDir * ROBOT_LASER_SPEED;

    // ¿Salió del nivel?
    if (r->laserX + WHIP_SPRITE_W < 0 || r->laserX > 1376) { robotKillLaser(r); return; }

    // ¿Impactó a algún jugador?
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
                        (r->laserY - ROBOT_FOOT_OFFSET) + WHIP_HAND_Y_OFFSET);
}

// ---------------------------------------------------------------------------
// Rutina: elegir el extremo más lejano y caminar hacia él
// ---------------------------------------------------------------------------
static void robotStartWalk(Robot* r) {
    s16 dl = rabs(r->x - ROBOT_PATROL_LEFT);
    s16 dr = rabs(r->x - ROBOT_PATROL_RIGHT);
    r->patrolTarget = (dr >= dl) ? ROBOT_PATROL_RIGHT : ROBOT_PATROL_LEFT;
    r->state = ROBOT_WALK;
    robotSetAnim(r, ROBOT_ANIM_WALK, TRUE);
}

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------
void robotInit(Robot* r) {
    r->sprite        = NULL;
    r->state         = ROBOT_INACTIVE;
    r->x = r->y      = 0;
    r->cameraOffsetX = 0;
    r->dir           = -1;
    r->hp            = ROBOT_HP;
    r->anim          = 0xFF;
    r->flashTimer    = 0;
    r->timer         = 0;
    r->patrolTarget  = ROBOT_PATROL_LEFT;
    r->attackCooldown = 0;
    r->drainTimer    = 0;
    r->whipSpr       = NULL;
    r->whipElectroTgl = 0;
    r->laserSpr      = NULL;
    r->laserActive   = FALSE;
    r->laserX = r->laserY = 0;
    r->laserDir      = 1;
}

void robotSpawn(Robot* r, s16 centerX) {
    r->x   = centerX - ROBOT_SPRITE_W / 2;
    r->y   = ROBOT_SPAWN_Y;
    r->dir = -1;
    r->hp  = ROBOT_HP;
    r->state = ROBOT_APPEAR;
    r->anim  = 0xFF;
    r->attackCooldown = 0;
    r->drainTimer = 0;

    r->sprite = SPR_addSprite(&robot_whip,
                              r->x - r->cameraOffsetX, r->y - ROBOT_FOOT_OFFSET,
                              TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    // Comparte la paleta de los foot soldiers (PAL2, ya cargada por el nivel):
    // no se recarga.
    robotRestartAnim(r, ROBOT_ANIM_APPEAR, FALSE);
}

bool robotIsActive(const Robot* r) {
    return (r->state != ROBOT_INACTIVE && r->state != ROBOT_GONE);
}

bool robotCanBeHit(const Robot* r) {
    // No durante la aparición (inmune), el agarre, el golpe (i-frames) ni la
    // muerte. Sí mientras camina, gira o ataca.
    return (r->state == ROBOT_WALK || r->state == ROBOT_TURN ||
            r->state == ROBOT_LASER || r->state == ROBOT_WHIP);
}

s16 robotGetCenterX(const Robot* r) { return r->x + ROBOT_SPRITE_W / 2; }
s16 robotGetCenterY(const Robot* r) { return r->y; }

void robotDamage(Robot* r, s16 dmg) {
    if (!robotCanBeHit(r)) return;

    r->hp -= dmg;

    if (r->hp <= 0) {
        // Destruido: explota. Suelta el látigo si lo tenía afuera.
        robotReleaseWhip(r);
        r->flashTimer = 0;
        SPR_setPalette(r->sprite, PAL2);
        r->state = ROBOT_DEAD;
        robotRestartAnim(r, ROBOT_ANIM_DESTROY, FALSE);
        return;
    }

    // Golpeado (no fatal): flash blanco (PAL3 = paleta flash del nivel) + HURT.
    robotReleaseWhip(r);
    SPR_setPalette(r->sprite, PAL3);
    r->flashTimer = ROBOT_FLASH_FRAMES;
    r->state = ROBOT_HURT;
    r->timer = ROBOT_HURT_FRAMES;
    robotRestartAnim(r, ROBOT_ANIM_HURT, FALSE);
}

void robotUpdate(Robot* r, s16 cameraX, Player* p1, Player* p2, bool twoPlayers, u16 fps) {
    if (r->state == ROBOT_INACTIVE || r->state == ROBOT_GONE || !r->sprite) return;

    r->cameraOffsetX = cameraX;

    // Restaurar paleta al terminar el flash de golpe
    if (r->flashTimer > 0) {
        r->flashTimer--;
        if (r->flashTimer == 0) SPR_setPalette(r->sprite, PAL2);
    }
    if (r->attackCooldown > 0) r->attackCooldown--;

    // Jugador objetivo: el más cercano en X.
    Player* tgt = p1;
    if (twoPlayers && p2 &&
        rabs(getPlayerWorldX(p2) - r->x) < rabs(getPlayerWorldX(p1) - r->x))
        tgt = p2;

    s16 pcx  = getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2;
    s16 py   = getPlayerY(tgt);
    s16 rcx  = r->x + ROBOT_SPRITE_W / 2;
    s16 ddx  = pcx - rcx;
    s16 distX = rabs(ddx);

    switch (r->state) {

        case ROBOT_APPEAR: {
            // Inmune mientras sale del suelo. Al terminar la anim, arranca.
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
            robotSetAnim(r, ROBOT_ANIM_WALK, TRUE);
            if (rabs(r->x - r->patrolTarget) <= ROBOT_ARRIVE_MARGIN) {
                r->x = r->patrolTarget;
                r->state = ROBOT_TURN;
                r->timer = ROBOT_TURN_MAX;
                robotRestartAnim(r, ROBOT_ANIM_TURN, FALSE);
            }
            break;
        }

        case ROBOT_TURN: {
            // Gira mirando al jugador y se ALINEA en Y con él.
            r->dir = (ddx >= 0) ? 1 : -1;
            if      (py > r->y + ROBOT_Y_ALIGN) r->y += ROBOT_SPEED;
            else if (py < r->y - ROBOT_Y_ALIGN) r->y -= ROBOT_SPEED;
            r->y = rclamp(r->y, ROBOT_LANE_TOP, ROBOT_LANE_BOTTOM);

            if (r->timer > 0) r->timer--;
            if (SPR_isAnimationDone(r->sprite) || r->timer == 0) {
                // Decidir ataque por distancia: lejos = láser, en rango = látigo.
                if (distX > ROBOT_WHIP_REACH) {
                    r->state = ROBOT_LASER;
                    r->timer = 0;
                    robotRestartAnim(r, ROBOT_ANIM_LASER, FALSE);
                } else {
                    r->state = ROBOT_WHIP;
                    r->timer = ROBOT_WHIP_HOLD;
                    robotRestartAnim(r, ROBOT_ANIM_WHIP, FALSE);
                    robotSpawnWhip(r);
                }
            }
            break;
        }

        case ROBOT_LASER: {
            r->timer++;
            if (r->timer == ROBOT_LASER_FIRE_DELAY) robotFireLaser(r);
            if (SPR_isAnimationDone(r->sprite)) {
                robotStartWalk(r);            // el rayo sigue viajando por su cuenta
                r->attackCooldown = ROBOT_ATTACK_COOLDOWN;
            }
            break;
        }

        case ROBOT_WHIP: {
            robotSetAnim(r, ROBOT_ANIM_WHIP_HOLD, TRUE);
            robotUpdateWhipSearch(r, distX);
            // ¿Atrapó a la tortuga? (en rango, alineada y agarrable)
            if (distX <= ROBOT_WHIP_REACH && rabs(py - r->y) <= ROBOT_WHIP_TOL_Y &&
                playerCanBeHit(tgt)) {
                playerWhipGrab(tgt);
                r->state = ROBOT_GRAB;
                r->drainTimer = 0;
                robotRestartAnim(r, ROBOT_ANIM_CAUGHT, FALSE);
                if (r->whipSpr) SPR_setAnimAndFrame(r->whipSpr, WHIP_ANIM_CONTACT, 2);
            } else if (r->timer > 0) {
                r->timer--;
            } else {
                robotReleaseWhip(r);          // volvió sin atrapar
                robotStartWalk(r);
                r->attackCooldown = ROBOT_ATTACK_COOLDOWN;
            }
            break;
        }

        case ROBOT_GRAB: {
            // Tras "atrapada" pasa a electrocución (loop).
            if (r->anim == ROBOT_ANIM_CAUGHT && SPR_isAnimationDone(r->sprite))
                robotRestartAnim(r, ROBOT_ANIM_ELECTRO, TRUE);
            robotUpdateWhipElectro(r);

            // Drena 1 barra por segundo.
            if (++r->drainTimer >= fps) {
                r->drainTimer = 0;
                playerElectroDrain(tgt);
            }
            // El jugador zafó (mashing) o cayó KO -> soltar y seguir.
            if (!playerIsGrabbed(tgt)) {
                robotReleaseWhip(r);
                robotStartWalk(r);
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

    // Proyectil láser (independiente del estado del robot)
    robotUpdateLaser(r, p1, p2, twoPlayers);

    // Render del robot
    SPR_setHFlip(r->sprite, (r->dir != robotArtDir(r->anim)));
    SPR_setPosition(r->sprite, r->x - cameraX, r->y - ROBOT_FOOT_OFFSET);
    SPR_setDepth(r->sprite, -(r->y));
}
