#include "rocksteady.h"
#include "audio.h"   // hit_turtles (sonido de patada del jefe)

// ===========================================================================
// ROCKSTEADY — implementación (jefe del nivel 2)
// ===========================================================================

static s16 rabs(s16 v)                 { return (v < 0) ? -v : v; }
static s16 rclamp(s16 v, s16 a, s16 b) { return (v < a) ? a : ((v > b) ? b : v); }

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
    s16     cameraOffsetX;
} bullets[MAX_ROCKSTEADY_BULLETS];

void rocksteadyBulletInit(void) {
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        bullets[i].active = 0;
        bullets[i].sprite = NULL;
    }
}

static void rocksteadyBulletSpawn(s16 x, s16 y, s8 dir, u8 palette) {
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        if (bullets[i].active) continue;
        bullets[i].x = x;
        bullets[i].y = y;
        bullets[i].dir = dir;
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

        // Fuera de pantalla (con margen de 32px a cada lado)
        if (bullets[i].x < camX - 32 || bullets[i].x > camX + 320 + 32) {
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

bool rocksteadyBulletBreakByPlayerAttack(const Player* p) {
    // Igual que los shurikens: la hitbox del ataque de la tortuga rompe las
    // balas que la cruzan SIN dañar al jugador. Devuelve TRUE si rompió
    // alguna (para tocar un SFX). Se llama por jugador, ANTES de
    // rocksteadyBulletCheckHitPlayer.
    bool broke = FALSE;
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        if (!bullets[i].active) continue;
        s16 bcx = bullets[i].x + 8;
        s16 bcy = bullets[i].y;
        if (playerAttackHits(p, bcx, bcy)) {
            if (bullets[i].sprite) SPR_releaseSprite(bullets[i].sprite);
            bullets[i].sprite = NULL;
            bullets[i].active = 0;
            broke = TRUE;
        }
    }
    return broke;
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
    r->moveToggle = 0;
    r->chargeHit = 0;
    r->shotFrame = 0;
    r->shotsFired = 0;
    r->shotTimer = 0;
}

void rocksteadySpawn(Rocksteady* r) {
    r->x = ROCKSTEADY_SPAWN_X;   // 8 tiles a la izquierda de la cápsula (puerta abierta)
    r->y = 148;   // Subido junto con la cápsula (4 tiles): pies alineados a la puerta
    r->dir = -1;
    r->phase = 1;
    r->hp = ROCKSTEADY_HP;
    r->hitsTaken = 0;
    r->knockdowns = 0;
    r->attackCooldown = 0;
    r->moveToggle = 0;
    r->chargeHit = 0;
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
            r->state == ROCKSTEADY_SHOOT || r->state == ROCKSTEADY_KICK_ARMS);
}

s16 rocksteadyGetCenterX(const Rocksteady* r) { return r->x; }
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
    // Fase 1: cada ROCKSTEADY_KD_INTERVAL golpes el jefe CAE (knock-down).
    if (r->phase == 1 && r->hitsTaken >= ROCKSTEADY_KD_INTERVAL) {
        r->hitsTaken = 0;
        r->knockdowns++;
        r->state = ROCKSTEADY_KNOCKDOWN;
        r->timer = ROCKSTEADY_KD_HOLD;
        rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_HURT, FALSE);
        return;
    }

    // Flinch normal (anim según la fase).
    r->state = (r->phase == 1) ? ROCKSTEADY_HURT : ROCKSTEADY_HURT_ARMS;
    r->timer = ROCKSTEADY_HURT_FRAMES;
    rocksteadyRestartAnim(r, (r->phase == 1)
                            ? ROCKSTEADY_ANIM_HURT : ROCKSTEADY_ANIM_HURT_ARMS,
                          FALSE);
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
    s16 distX = rabs(pcx - r->x);

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
            r->dir = (pcx >= r->x) ? 1 : -1;
            r->x += r->dir * ROCKSTEADY_SPEED;
            if (r->y < ROCKSTEADY_LANE_BOTTOM) r->y = rclamp(r->y + ROCKSTEADY_SPEED, 148, ROCKSTEADY_LANE_BOTTOM);
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
                // Cerca → patada (solo si la tortuga está cerca). Lejos →
                // alterna estampida / acercarse caminando. Media → camina.
                if (distX <= ROCKSTEADY_MELEE_RANGE) {
                    rocksteadyStartKick(r);
                } else if (distX > ROCKSTEADY_CHARGE_MIN) {
                    if (r->moveToggle & 1) rocksteadyStartCharge(r);
                    else                   rocksteadyStartApproach(r);
                    r->moveToggle++;
                } else {
                    rocksteadyStartApproach(r);
                }
            } else {
                if (distX < ROCKSTEADY_MELEE_RANGE) rocksteadyStartKickArms(r);
                else rocksteadyStartAimWalk(r);
            }
            break;
        }

        case ROCKSTEADY_APPROACH: {
            // Camina hacia el jugador (fase 1, sin arma) alineando lane; al
            // quedar en rango de patada, patea.
            r->dir = (pcx >= r->x) ? 1 : -1;
            r->x += r->dir * ROCKSTEADY_SPEED;
            if      (py > r->y + 2) r->y += ROCKSTEADY_SPEED;
            else if (py < r->y - 2) r->y -= ROCKSTEADY_SPEED;
            r->y = rclamp(r->y, ROCKSTEADY_LANE_TOP, ROCKSTEADY_LANE_BOTTOM);
            r->x = rclamp(r->x, ROCKSTEADY_PATROL_LEFT, ROCKSTEADY_PATROL_RIGHT);
            if (distX <= ROCKSTEADY_MELEE_RANGE) rocksteadyStartKick(r);
            break;
        }

        case ROCKSTEADY_CHARGE: {
            // Estampida: corre hacia el centro del jugador re-aimando cada
            // frame (cubre los cambios de lane). Al impactar NO frena en seco:
            // sigue un tramo más (overshoot, ROCKSTEADY_CHARGE_OVER) para que
            // la carga recorra más eje X, dañando una sola vez (chargeHit).
            r->dir = (pcx >= r->x) ? 1 : -1;
            r->x += r->dir * ROCKSTEADY_CHARGE_SPEED;
            if      (py > r->y + 2) r->y += ROCKSTEADY_SPEED;
            else if (py < r->y - 2) r->y -= ROCKSTEADY_SPEED;
            r->y = rclamp(r->y, ROCKSTEADY_LANE_TOP, ROCKSTEADY_LANE_BOTTOM);

            if (!r->chargeHit &&
                rabs(pcx - r->x) < 50 && rabs(py - r->y) < ROCKSTEADY_HIT_TOL_Y &&
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
            // Un solo golpe, en el frame 2 de la anim (de 8).
            if (r->timer < 3) r->timer++;
            if (r->timer == 2 &&
                rabs(pcx - r->x) < ROCKSTEADY_MELEE_RANGE &&
                rabs(py - r->y) < ROCKSTEADY_HIT_TOL_Y &&
                playerCanBeHit(tgt)) {
                playerHitBars(tgt, r->x, ROCKSTEADY_BULLET_DMG);
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
            // Se acerca apuntando hasta quedar en rango de disparo.
            r->dir = (pcx >= r->x) ? 1 : -1;
            r->x += r->dir * ROCKSTEADY_SPEED;
            if      (py > r->y + 2) r->y += ROCKSTEADY_SPEED;
            else if (py < r->y - 2) r->y -= ROCKSTEADY_SPEED;
            r->y = rclamp(r->y, ROCKSTEADY_LANE_TOP, ROCKSTEADY_LANE_BOTTOM);
            r->x = rclamp(r->x, ROCKSTEADY_PATROL_LEFT, ROCKSTEADY_PATROL_RIGHT);
            if (distX <= ROCKSTEADY_SHOOT_RANGE) rocksteadyStartShoot(r);
            break;
        }

        case ROCKSTEADY_SHOOT: {
            // Frames a mano: cada ROCKSTEADY_SHOT_TICKS avanza un frame de la
            // anim y en los frames 3/5/7 sale una bala del cañón.
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
                    s16 gx = r->x + r->dir * 22;   // del cañón (pegado al cuerpo)
                    rocksteadyBulletSpawn(gx, r->y, r->dir, PAL3);
                    r->shotsFired++;
                }
            }
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
