#include "robot.h"
#include "audio.h"   // electric_shock_sfx (electrocución del látigo)

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

static s16 robotStep(u8* acc, u16 q) {
    u16 t = (u16)(*acc + q);
    *acc = (u8)(t & 0xFF);
    return (s16)(t >> 8);
}

static s16 robotToward(s16 v, s16 to, s16 step) {
    if (v < to) return (to - v < step) ? to : v + step;
    if (v > to) return (v - to < step) ? to : v - step;
    return v;
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
    r->laserAcc = 0;
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

static void robotUpdateLaser(Robot* r, Player** ps, u8 n) {
    if (!r->laserActive) return;
    r->laserX += r->laserDir * robotStep(&r->laserAcc, ROBOT_LASER_Q);

    // Se corta al salir de CAMARA: la X de sprite del VDP envuelve fuera de
    // pantalla y el rayo reaparecia por el otro borde.
    s16 sx = r->laserX - r->cameraOffsetX;
    if (sx + WHIP_SPRITE_W < -16 || sx > ROBOT_SCREEN_W + 16) {
        robotKillLaser(r);
        return;
    }

    for (u8 i = 0; i < n; i++) {
        Player* p = ps[i];
        if (!p) continue;
        s16 pcx = getPlayerHurtCX(p);
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

// Alcance actual del látigo según el frame del lanzamiento.
static s16 robotWhipReach(const Robot* r) {
    s16 reach = ROBOT_WHIP_REACH_MIN + (s16)r->throwFrame * ROBOT_WHIP_STEP;
    return (reach > ROBOT_WHIP_REACH_MAX) ? ROBOT_WHIP_REACH_MAX : reach;
}

// Largos reales del cable tenso (ver ROBOT_WHIP_GRAB_* en robot.h).
static const s16 robotGrabReach[ROBOT_WHIP_GRAB_N] = {
    ROBOT_WHIP_GRAB_R0, ROBOT_WHIP_GRAB_R1, ROBOT_WHIP_GRAB_R2, ROBOT_WHIP_GRAB_R3
};

// Variante de largo cuyo cable termina MAS CERCA de 'want' px del cuerpo.
static u8 robotGrabIndex(s16 want) {
    u8  best = 0;
    s16 bestErr = rabs(robotGrabReach[0] - want);
    for (u8 i = 1; i < ROBOT_WHIP_GRAB_N; i++) {
        s16 err = rabs(robotGrabReach[i] - want);
        if (err < bestErr) { bestErr = err; best = i; }
    }
    return best;
}

// La variante elegida, recortada por si la fila tiene menos frames.
static u8 robotElectroFrame(const Robot* r) {
    u8 n = (u8)r->sprite->animation->numFrame;   // frames de la anim actual
    if (n <= 1) return 0;
    return (r->grabFrame >= n) ? (u8)(n - 1) : r->grabFrame;
}

// ---------------------------------------------------------------------------
// Arranque de cada estado
// ---------------------------------------------------------------------------
static void robotEnter(Robot* r, RobotState s) {
    r->state = s;
    r->timer = 0;
}

static void robotToFlee(Robot* r) {
    robotEnter(r, ROBOT_FLEE);
    robotRestartAnim(r, ROBOT_ANIM_WALK, TRUE);
}

static void robotToBrake(Robot* r) {
    robotEnter(r, ROBOT_BRAKE);
    robotRestartAnim(r, ROBOT_ANIM_TURN, FALSE);
}

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------
void robotInit(Robot* r) {
    memset(r, 0, sizeof(Robot));
    r->sprite = NULL;
    r->state = ROBOT_INACTIVE;
    r->dir = -1;
    r->hurtDir = 1;
    r->hp = ROBOT_HP;
    r->anim = 0xFF;
    r->laserSpr = NULL;
    r->laserActive = FALSE;
    r->laserDir = 1;
}

void robotSpawn(Robot* r, s16 centerX, s16 spawnY) {
    r->x = centerX;
    r->y = spawnY;
    r->dir = -1;
    r->hp = ROBOT_HP;
    r->state = ROBOT_APPEAR;
    r->anim = 0xFF;
    r->drainTimer = 0;
    r->accX = r->accY = 0;
    r->sprite = SPR_addSprite(&robot_whip, 0, 0, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    // Comparte PAL2 (paleta de los foot soldiers), ya cargada por el nivel.
    // Si el presupuesto de VRAM de sprites esta agotado (modo 4 jugadores: 4
    // tortugas + 4 marcos de HUD + 4 robots), SPR_addSprite devuelve NULL. Un
    // robot sin sprite no actua y el nivel no terminaria nunca: se lo da por
    // terminado (ROBOT_GONE).
    if (!r->sprite) {
        r->state = ROBOT_GONE;
        return;
    }
    robotRestartAnim(r, ROBOT_ANIM_APPEAR, FALSE);
    robotRender(r);
    // Grito del robot al salir del suelo. CH2: SOUND_PCM_CH4 NO existe en el
    // driver XGM2 (solo hay 3 canales PCM reales) y corrompia el comando.
    XGM2_playPCMEx(robot_twip_sfx, sizeof(robot_twip_sfx), SOUND_PCM_CH2, 15, FALSE, FALSE);
}

bool robotIsActive(const Robot* r) {
    return (r->state != ROBOT_INACTIVE && r->state != ROBOT_GONE);
}

bool robotCanBeHit(const Robot* r) {
    // Solo mientras corre: frenando, atacando y golpeado no se le puede pegar.
    return (r->state == ROBOT_FLEE);
}

s16 robotGetCenterX(const Robot* r) { return r->x; }
s16 robotGetCenterY(const Robot* r) { return r->y; }

void robotDamage(Robot* r, s16 dmg, s16 attackerX) {
    (void)dmg;                      // cualquier golpe saca uno
    if (!robotCanBeHit(r)) return;
    r->hp -= ROBOT_SPECIAL_DMG;

    if (r->hp <= 0) {
        r->state = ROBOT_DEAD;
        robotRestartAnim(r, ROBOT_ANIM_DESTROY, FALSE);
        return;
    }
    // Retrocede alejandose del que le pego, mirandolo.
    r->hurtDir = (r->x >= attackerX) ? 1 : -1;
    r->dir = (s8)-r->hurtDir;
    robotEnter(r, ROBOT_HURT);
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

// Envoltorio de 1-2 jugadores sobre la version de N.
void robotUpdate(Robot* r, s16 cameraX, Player* p1, Player* p2, bool twoPlayers, u16 fps) {
    Player* ps[2] = { p1, (twoPlayers && p2) ? p2 : p1 };
    robotUpdateN(r, cameraX, ps, (twoPlayers && p2) ? 2 : 1, fps);
}

void robotUpdateN(Robot* r, s16 cameraX, Player** pls, u8 nPl, u16 fps) {
    if (r->state == ROBOT_INACTIVE || r->state == ROBOT_GONE || !r->sprite) return;
    r->cameraOffsetX = cameraX;
    r->timer++;

    // Jugador objetivo: el MAS CERCANO en X, entre los que haya (1..4). Los
    // que estan sin vidas no cuentan (salvo que no quede nadie).
    Player* tgt = pls[0];
    for (u8 k = 1; k < nPl; k++) {
        if (isPlayerGameOver(pls[k])) continue;
        if (isPlayerGameOver(tgt) ||
            rabs(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2 - r->x) <
            rabs(getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2 - r->x))
            tgt = pls[k];
    }
    s16 pcx  = getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2;
    s16 py   = getPlayerY(tgt);
    s16 ddx  = pcx - r->x;
    s8  toward = (ddx >= 0) ? 1 : -1;

    switch (r->state) {

        case ROBOT_APPEAR: {
            if (SPR_isAnimationDone(r->sprite)) {
                r->dir = toward;
                robotToFlee(r);
            }
            break;
        }

        case ROBOT_FLEE: {
            // Se da vuelta al salir de camara o al quedar lejos detras de la
            // tortuga: la cruza una y otra vez.
            if (r->dir < 0 && (r->x < cameraX || r->x < pcx - ROBOT_FLEE_BEHIND))
                r->dir = 1;
            else if (r->dir > 0 && (r->x > cameraX + ROBOT_SCREEN_W ||
                                    r->x > pcx + ROBOT_FLEE_BEHIND))
                r->dir = -1;
            r->x += r->dir * robotStep(&r->accX, ROBOT_FLEE_Q);
            r->y = robotToward(r->y, py - ROBOT_FLEE_LANE_DY,
                               robotStep(&r->accY, ROBOT_FLEE_LANE_Q));
            robotSetAnim(r, (r->timer < ROBOT_WALK_START_TICKS)
                            ? ROBOT_ANIM_WALK : ROBOT_ANIM_WALK_LONG, TRUE);
            if (r->timer >= ROBOT_FLEE_T) robotToBrake(r);
            break;
        }

        case ROBOT_BRAKE: {
            r->dir = toward;
            r->y = robotToward(r->y, py, robotStep(&r->accY, ROBOT_BRAKE_Q));
            if (SPR_isAnimationDone(r->sprite))
                robotSetAnim(r, ROBOT_ANIM_IDLE, TRUE);
            if (r->timer < ROBOT_BRAKE_T) break;
            r->y = py;                         // se planta en su lane
            if (random() & 1) {
                robotEnter(r, ROBOT_WINDUP);
                robotRestartAnim(r, ROBOT_ANIM_WHIP_WINDUP, FALSE);
            } else {
                robotEnter(r, ROBOT_LASER);
                robotRestartAnim(r, ROBOT_ANIM_LASER, FALSE);
            }
            break;
        }

        case ROBOT_WINDUP: {
            // Preparación; siempre antes del látigo. Sigue mirando a la
            // tortuga y alineandose.
            r->dir = toward;
            r->y = robotToward(r->y, py, robotStep(&r->accY, ROBOT_BRAKE_Q));
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
                // Enganchó: se elige el largo de cable que mejor cae a la
                // distancia real y se le pega un TIRON a la tortuga para que la
                // punta termine justo en su cuerpo.
                r->grabFrame = robotGrabIndex(fwd - ROBOT_WHIP_GRAB_INSET);
                s16 pcxNew = r->x + r->dir * (robotGrabReach[r->grabFrame]
                                              + ROBOT_WHIP_GRAB_INSET);
                playerWhipGrabAt(tgt, pcxNew - PLAYER_SPRITE_W / 2, r->y);
                robotEnter(r, ROBOT_GRAB);
                r->drainTimer = 0;
                r->electroTgl = 0;
                // Pose de agarre CONGELADA en la variante elegida.
                robotRestartAnim(r, ROBOT_ANIM_CAUGHT, FALSE);
                SPR_setAutoAnimation(r->sprite, FALSE);
                SPR_setFrame(r->sprite, robotElectroFrame(r));
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
                    robotToFlee(r);
                } else {
                    r->throwFrame--;
                    SPR_setFrame(r->sprite, r->throwFrame);
                }
            }
            break;
        }

        case ROBOT_GRAB: {
            // Pose CAUGHT congelada unos frames y despues alterna las anims de
            // electrocución (7/8) con el mismo largo de cable.
            if (r->anim == ROBOT_ANIM_CAUGHT && r->timer == ROBOT_CAUGHT_FRAMES) {
                robotSetAnim(r, ROBOT_ANIM_ELECTRO_A, FALSE);
                SPR_setAutoAnimation(r->sprite, FALSE);
                r->grabFrame = robotElectroFrame(r);
                SPR_setFrame(r->sprite, r->grabFrame);
                // Zumbido en loop hasta que zafe.
                XGM2_playPCMEx(electric_shock_sfx, sizeof(electric_shock_sfx),
                               SOUND_PCM_CH2, 15, FALSE, TRUE);
            }
            if (r->anim != ROBOT_ANIM_CAUGHT) {
                r->electroTgl++;
                if ((r->electroTgl & 7) == 0) {
                    u8 next = (r->anim == ROBOT_ANIM_ELECTRO_A)
                              ? ROBOT_ANIM_ELECTRO_B : ROBOT_ANIM_ELECTRO_A;
                    r->anim = next;
                    SPR_setAnim(r->sprite, next);
                    SPR_setAutoAnimation(r->sprite, FALSE);
                    r->grabFrame = robotElectroFrame(r);
                    SPR_setFrame(r->sprite, r->grabFrame);
                }
            }
            // Drena 1 barra cada ROBOT_ELECTRO_SECONDS.
            if (++r->drainTimer >= fps * ROBOT_ELECTRO_SECONDS) {
                r->drainTimer = 0;
                playerElectroDrain(tgt);
            }
            // Zafó (mashing) o cayó KO -> soltar y volver a correr.
            if (!playerIsGrabbed(tgt)) {
                XGM2_stopPCM(SOUND_PCM_CH2);
                robotToFlee(r);
            }
            break;
        }

        case ROBOT_LASER: {
            if (r->timer == ROBOT_LASER_FIRE_DELAY) robotFireLaser(r);
            if (r->timer >= ROBOT_LASER_T) robotToFlee(r);   // el rayo sigue solo
            break;
        }

        case ROBOT_HURT: {
            r->x += r->hurtDir * robotStep(&r->accX, ROBOT_HURT_Q);
            if (r->timer >= ROBOT_HURT_T) {
                r->dir = r->hurtDir;           // sale corriendo hacia ese lado
                robotToFlee(r);
            }
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

    r->x = rclamp(r->x, ROBOT_MIN_X, ROBOT_MAX_X);
    r->y = rclamp(r->y, ROBOT_LANE_TOP, ROBOT_LANE_BOTTOM);
    robotUpdateLaser(r, pls, nPl);
    robotRender(r);
}
