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

static void robotUpdateLaser(Robot* r, Player** ps, u8 n) {
    if (!r->laserActive) return;
    r->laserX += r->laserDir * ROBOT_LASER_SPEED;

    // Se corta al salir de CAMARA, no del mundo (13/09). Antes el tope era el
    // ancho del nivel (1376), asi que el laser seguia vivo cientos de px fuera
    // de pantalla; como la X de sprite del VDP son 9 bits con el origen
    // corrido 128, apenas la X de pantalla baja de -128 el valor ENVUELVE y el
    // sprite reaparece por el otro borde -- que es lo que reporto Gustavo
    // ("sale por un lado y sigue atravesandola por el otro"). El sprite es
    // ancho (96px) asi que llegaba a esa zona enseguida.
    // De paso arregla algo que no se veia: fuera de pantalla el laser seguia
    // pudiendo golpear al jugador.
    s16 sx = r->laserX - r->cameraOffsetX;
    if (sx + WHIP_SPRITE_W < -16 || sx > ROBOT_SCREEN_W + 16) {
        robotKillLaser(r);
        return;
    }

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
// (14/09) Reescrito: ver el bloque ROBOT_WHIP_GRAB_* de robot.h. La variante
// de largo ya se eligio al enganchar (r->grabFrame) midiendo la distancia real
// contra los largos REALES del PNG; aca solo se la recorta por si la fila
// tuviera menos frames de los esperados.
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

static u8 robotElectroFrame(const Robot* r) {
    u8 n = (u8)r->sprite->animation->numFrame;   // frames de la anim actual
    if (n <= 1) return 0;
    return (r->grabFrame >= n) ? (u8)(n - 1) : r->grabFrame;
}

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------
void robotInit(Robot* r) {
    r->sprite = NULL;
    r->state = ROBOT_INACTIVE;
    r->x = r->y = 0;
    r->retreatY = 0;
    r->cameraOffsetX = 0;
    r->dir = -1;
    r->hurtDir = 1;
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

void robotSpawn(Robot* r, s16 centerX, s16 spawnY) {
    r->x = centerX;
    r->y = spawnY;
    r->dir = -1;
    r->hp = ROBOT_HP;
    r->state = ROBOT_APPEAR;
    r->anim = 0xFF;
    r->attackCooldown = 0;
    r->drainTimer = 0;
    r->sprite = SPR_addSprite(&robot_whip, 0, 0, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    // Comparte PAL2 (paleta de los foot soldiers), ya cargada por el nivel.
    // (16/09) Si el presupuesto de VRAM de sprites esta agotado (modo 4
    // jugadores: 4 tortugas + 4 marcos de HUD + 4 robots), SPR_addSprite
    // devuelve NULL. Un robot sin sprite NO ACTUA: robotUpdate sale en su
    // primera linea, se queda en APPEAR para siempre y la condicion de
    // victoria del nivel nunca se cumple -- el nivel quedaba intrabajable.
    // Lo damos por terminado (ROBOT_GONE) en vez de dejarlo colgado.
    if (!r->sprite) {
        r->state = ROBOT_GONE;
        return;
    }
    robotRestartAnim(r, ROBOT_ANIM_APPEAR, FALSE);
    robotRender(r);
    // Grito del robot al salir del suelo ("twip"). CH2, misma linea que usan
    // las demas voces de entrada/anuncio del juego (say_your_p_sfx, etc.) --
    // SOUND_PCM_CH4 NO existe de verdad en el driver XGM2 de SGDK (solo hay
    // 3 canales PCM reales, PCM0..PCM2 = CH1..CH3); usarlo corrompia el
    // comando del Z80 y dejaba un pitido constante (bug encontrado 29/08).
    XGM2_playPCMEx(robot_twip_sfx, sizeof(robot_twip_sfx), SOUND_PCM_CH2, 15, FALSE, FALSE);
}

bool robotIsActive(const Robot* r) {
    return (r->state != ROBOT_INACTIVE && r->state != ROBOT_GONE);
}

bool robotCanBeHit(const Robot* r) {
    // Hittable mientras camina, gira o ataca; NO durante aparición, agarre,
    // golpe (i-frames) ni muerte.
    return (r->state == ROBOT_WALK || r->state == ROBOT_TURN ||
            r->state == ROBOT_WINDUP || r->state == ROBOT_THROW ||
            r->state == ROBOT_RETRACT || r->state == ROBOT_LASER ||
            r->state == ROBOT_RETREAT);
}

s16 robotGetCenterX(const Robot* r) { return r->x; }
s16 robotGetCenterY(const Robot* r) { return r->y; }

void robotDamage(Robot* r, s16 dmg, s16 attackerX) {
    if (!robotCanBeHit(r)) return;
    r->hp -= dmg;

    if (r->hp <= 0) {
        r->state = ROBOT_DEAD;
        robotRestartAnim(r, ROBOT_ANIM_DESTROY, FALSE);
        return;
    }
    // Golpeado no fatal: HURT + empuje bien grande en la dirección del golpe
    // (lejos del atacante), para que cueste un poco más rematarlo pegado.
    // Distancia a proposito exagerada (30/08) para poder verla clarito y
    // ajustarla desde ROBOT_HURT_KNOCK_SPEED en robot.h.
    r->hurtDir = (r->x >= attackerX) ? 1 : -1;
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

// (14/09) Envoltorio de 1-2 jugadores sobre la version de N.
void robotUpdate(Robot* r, s16 cameraX, Player* p1, Player* p2, bool twoPlayers, u16 fps) {
    Player* ps[2] = { p1, (twoPlayers && p2) ? p2 : p1 };
    robotUpdateN(r, cameraX, ps, (twoPlayers && p2) ? 2 : 1, fps);
}

void robotUpdateN(Robot* r, s16 cameraX, Player** pls, u8 nPl, u16 fps) {
    if (r->state == ROBOT_INACTIVE || r->state == ROBOT_GONE || !r->sprite) return;
    r->cameraOffsetX = cameraX;

    if (r->attackCooldown > 0) r->attackCooldown--;

    // Jugador objetivo: el MAS CERCANO en X, entre los que haya (1..4).
    // (26/09) Los que estan sin vidas no cuentan (salvo que no quede nadie).
    Player* tgt = pls[0];
    for (u8 k = 1; k < nPl; k++) {
        if (isPlayerGameOver(pls[k])) continue;
        if (isPlayerGameOver(tgt) ||
            rabs(getPlayerWorldX(pls[k]) - r->x) < rabs(getPlayerWorldX(tgt) - r->x))
            tgt = pls[k];
    }
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
                // ¡Enganchó! (14/09) El cable tiene 4 largos dibujados y nada
                // mas: se elige el que mejor cae a la distancia real y se le
                // pega un TIRON al jugador para que la punta termine justo en
                // su cuerpo, en la misma lane que el robot. Asi el cable no
                // sobresale por detras ni queda corto.
                // La punta tiene que caer INSET px antes del centro del torso:
                //   punta = pcx - dir*INSET  ->  largo pedido = fwd - INSET
                r->grabFrame = robotGrabIndex(fwd - ROBOT_WHIP_GRAB_INSET);
                // …y el jugador se corre para que ese largo quede exacto.
                s16 pcxNew = r->x + r->dir * (robotGrabReach[r->grabFrame]
                                              + ROBOT_WHIP_GRAB_INSET);
                playerWhipGrabAt(tgt, pcxNew - PLAYER_SPRITE_W / 2, r->y);
                r->state = ROBOT_GRAB;
                r->drainTimer = 0;
                r->electroTgl = 0;
                r->timer = ROBOT_CAUGHT_FRAMES;
                // Pose de agarre CONGELADA en la variante elegida: la fila [6]
                // NO es una animacion (son 4 largos del mismo cable), asi que
                // reproducirla estiraba el cable delante del jugador.
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
            // (14/09) La pose CAUGHT ahora está CONGELADA (ver el enganche en
            // ROBOT_THROW), así que no se puede esperar a SPR_isAnimationDone:
            // se cuentan ROBOT_CAUGHT_FRAMES a mano.
            if (r->anim == ROBOT_ANIM_CAUGHT && r->timer > 0 && --r->timer == 0) {
                // Electrocución con el MISMO largo de cable que el agarre.
                robotSetAnim(r, ROBOT_ANIM_ELECTRO_A, FALSE);
                SPR_setAutoAnimation(r->sprite, FALSE);
                r->grabFrame = robotElectroFrame(r);
                SPR_setFrame(r->sprite, r->grabFrame);
                // Empieza la electrocución: zumbido en loop hasta que zafe.
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
                    // Re-escala al numFrame de ESTA anim (A y B pueden diferir).
                    r->grabFrame = robotElectroFrame(r);
                    SPR_setFrame(r->sprite, r->grabFrame);
                }
            }
            // Drena 1 barra por segundo.
            if (++r->drainTimer >= fps) { r->drainTimer = 0; playerElectroDrain(tgt); }
            // Zafó (mashing) o cayó KO -> soltar y seguir.
            if (!playerIsGrabbed(tgt)) {
                XGM2_stopPCM(SOUND_PCM_CH2);   // cortar el zumbido de la electrocución
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
            // Empuje: sólo durante los primeros ROBOT_HURT_KNOCK_FRAMES del
            // HURT (r->timer cuenta hacia abajo desde ROBOT_HURT_FRAMES).
            // OJO: clampeado contra ROBOT_HURT_KNOCK_MIN_X/MAX_X (mas ancho
            // que el corral de patrulla ROBOT_PATROL_LEFT/RIGHT) -- clampear
            // contra el corral de patrulla fue el bug reportado 30/08: el
            // corral es angosto (150px) y el empuje quedaba comido apenas el
            // robot estaba cerca de una punta.
            if (r->timer > (ROBOT_HURT_FRAMES - ROBOT_HURT_KNOCK_FRAMES))
                r->x = rclamp(r->x + r->hurtDir * ROBOT_HURT_KNOCK_SPEED,
                              ROBOT_HURT_KNOCK_MIN_X, ROBOT_HURT_KNOCK_MAX_X);
            if (r->timer > 0) { r->timer--; break; }

            // Terminado el empuje, RETIRADA: el extremo de patrulla mas lejos
            // DEL JUGADOR (no de si mismo) y la lane opuesta a la suya.
            {
                s16 dl = rabs(pcx - ROBOT_PATROL_LEFT);
                s16 dr = rabs(pcx - ROBOT_PATROL_RIGHT);
                r->patrolTarget = (dr >= dl) ? ROBOT_PATROL_RIGHT : ROBOT_PATROL_LEFT;
                s16 midLane = (ROBOT_LANE_TOP + ROBOT_LANE_BOTTOM) / 2;
                r->retreatY = (py >= midLane) ? ROBOT_LANE_TOP : ROBOT_LANE_BOTTOM;
                r->state = ROBOT_RETREAT;
                r->timer = ROBOT_RETREAT_MAX_FRAMES;
                r->walkTimer = 0;
                robotSetAnim(r, ROBOT_ANIM_WALK, TRUE);
            }
            break;
        }

        case ROBOT_RETREAT: {
            // Se aleja en X y cruza de lane al mismo tiempo. Al llegar (o al
            // agotarse el tope) sigue con el ciclo normal por el TURN, que es
            // el que decide latigo o laser segun la distancia.
            bool doneX = (rabs(r->x - r->patrolTarget) <= ROBOT_ARRIVE_MARGIN);
            bool doneY = (rabs(r->y - r->retreatY) <= ROBOT_RETREAT_ARRIVE);
            if (!doneX) {
                s16 step = (r->patrolTarget > r->x) ? ROBOT_SPEED : -ROBOT_SPEED;
                r->x += step;
                r->dir = (step > 0) ? 1 : -1;
            }
            if (!doneY) {
                r->y += (r->retreatY > r->y) ? ROBOT_SPEED : -ROBOT_SPEED;
                r->y = rclamp(r->y, ROBOT_LANE_TOP, ROBOT_LANE_BOTTOM);
            }
            r->walkTimer++;
            robotSetAnim(r, (r->walkTimer < ROBOT_WALK_START_TICKS)
                            ? ROBOT_ANIM_WALK : ROBOT_ANIM_WALK_LONG, TRUE);
            if (r->timer > 0) r->timer--;
            if ((doneX && doneY) || r->timer == 0) {
                r->x = rclamp(r->x, ROBOT_PATROL_LEFT, ROBOT_PATROL_RIGHT);
                r->state = ROBOT_TURN;
                r->timer = ROBOT_TURN_MAX;
                robotRestartAnim(r, ROBOT_ANIM_TURN, FALSE);
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

    robotUpdateLaser(r, pls, nPl);
    robotRender(r);
}
