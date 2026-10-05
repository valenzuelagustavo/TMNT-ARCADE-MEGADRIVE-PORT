#include "rocksteady.h"
#include "audio.h"   // hit_turtles (sonido de patada del jefe)

// ===========================================================================
// ROCKSTEADY — implementación (jefe del nivel 2)
// ===========================================================================

static s16 rabs(s16 v)                 { return (v < 0) ? -v : v; }

// (01/10) Arena en curso (ver RocksteadyArena). La del 1-2 por defecto.
static const RocksteadyArena raLevel2 = {
    ROCKSTEADY_LANE_TOP, ROCKSTEADY_LANE_BOTTOM,
    ROCKSTEADY_PATROL_LEFT, ROCKSTEADY_PATROL_RIGHT,
    ROCKSTEADY_SPAWN_X, 156,
    ROCKSTEADY_LANE_BOTTOM, ROCKSTEADY_EMERGE_STAND,
    PAL3, 0,
    0           // parpadeo: lo hace scenes.c (colores de PAL3)
};
static RocksteadyArena ra;
static s16 rclamp(s16 v, s16 a, s16 b) { return (v < a) ? a : ((v > b) ? b : v); }

// Velocidades en Q8 (256 = 1 px por frame), con el resto acumulado:
// 1,25 px/f sale 1-1-1-2...
static s16 rsStep(u8* acc, u16 q) {
    u16 t = (u16)(*acc + q);
    *acc = (u8)(t & 0xFF);
    return (s16)(t >> 8);
}
static s16 rsStepX(Rocksteady* r, u16 q) { return rsStep(&r->accX, q); }
static s16 rsStepY(Rocksteady* r, u16 q) { return rsStep(&r->accY, q); }

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
    SPR_setPosition(r->sprite, r->x - r->cameraOffsetX, r->y - ROCKSTEADY_FOOT_OFFSET - stageCamY);
    SPR_setDepth(r->sprite, -(r->y));
}

// ---------------------------------------------------------------------------
// BALAS — proyectil de vida independiente (sprite boss_bullet)
// ---------------------------------------------------------------------------
// Tres poses en una fila del sprite (sin auto-animacion): [0] recta,
// [1] diagonal hacia arriba, [2] impacto. Al pegar no se borra en el acto:
// se queda quieta mostrando el [2] y recien ahi se libera.
static struct {
    bool    active;
    Sprite* sprite;
    s16     x, y;      // mundo (esquina izq. del sprite de 16); y = lane del tiro
    s8      dir;
    u8      up;        // 1 = diagonal hacia arriba
    s16     z;         // altura sobre la lane
    u8      accX, accZ;
    u8      hitTimer;  // >0 = ya impacto: congelada mostrando el [2]
} bullets[MAX_ROCKSTEADY_BULLETS];

// Balas que le pegaron a una tortuga (el jefe cambia de plan con 2).
static u8 rsShotHits;

void rocksteadyBulletInit(void) {
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        bullets[i].active = 0;
        bullets[i].sprite = NULL;
        bullets[i].hitTimer = 0;
    }
    rsShotHits = 0;
}

// (cx, z) = boca del canon: cx en X de mundo, z en altura sobre los pies.
static void rocksteadyBulletSpawn(s16 cx, s16 lane, s16 z, s8 dir, bool up,
                                  u8 palette) {
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        if (bullets[i].active) continue;
        bullets[i].x = cx - 8;
        bullets[i].y = lane;
        bullets[i].dir = dir;
        bullets[i].up = up ? 1 : 0;
        bullets[i].z  = z;
        bullets[i].accX = bullets[i].accZ = 0;
        bullets[i].hitTimer = 0;
        bullets[i].active = 1;
        bullets[i].sprite = SPR_addSprite(&boss_bullet, -32, -32,
                                          TILE_ATTR(palette, FALSE, FALSE, FALSE));
        if (bullets[i].sprite) {
            SPR_setDepth(bullets[i].sprite, -(lane) - 1);
            SPR_setHFlip(bullets[i].sprite, (dir < 0));
            SPR_setAnimAndFrame(bullets[i].sprite, 0,
                                up ? ROCKSTEADY_BULLET_FR_UP : ROCKSTEADY_BULLET_FR_H);
        }
        return;
    }
}

static void rocksteadyBulletFree(u16 i) {
    if (bullets[i].sprite) SPR_releaseSprite(bullets[i].sprite);
    bullets[i].sprite = NULL;
    bullets[i].active = 0;
    bullets[i].hitTimer = 0;
}

void rocksteadyBulletUpdate(s16 camX) {
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        if (!bullets[i].active) continue;
        if (bullets[i].hitTimer > 0) {
            if (--bullets[i].hitTimer == 0) { rocksteadyBulletFree(i); continue; }
        } else {
            u16 q = bullets[i].up ? ROCKSTEADY_BULLET_DQ : ROCKSTEADY_BULLET_Q;
            bullets[i].x += bullets[i].dir * rsStep(&bullets[i].accX, q);
            if (bullets[i].up) bullets[i].z += rsStep(&bullets[i].accZ, ROCKSTEADY_BULLET_DQ);
            if (bullets[i].x < camX - 32 || bullets[i].x > camX + 320 + 32 ||
                bullets[i].z > ROCKSTEADY_BULLET_MAX_Z) {
                rocksteadyBulletFree(i);
                continue;
            }
        }
        if (bullets[i].sprite)
            SPR_setPosition(bullets[i].sprite, bullets[i].x - camX,
                            bullets[i].y - bullets[i].z - 8 - stageCamY);
    }
}

void rocksteadyBulletReleaseAll(void) {
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) rocksteadyBulletFree(i);
}

bool rocksteadyBulletCheckHitPlayer(s16 px, s16 py, s16 pz, s16* hitX) {
    // px = borde izquierdo del frame del jugador, py = lane, pz = jumpZ
    s16 pcx = px + PLAYER_SPRITE_W / 2;
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        if (!bullets[i].active || bullets[i].hitTimer > 0) continue;
        s16 bcx = bullets[i].x + 8;
        s16 dy  = py - bullets[i].y;
        s16 tolZ = bullets[i].up ? ROCKSTEADY_BULLET_HIT_Z_UP : ROCKSTEADY_BULLET_HIT_Z;
        if (rabs(pcx - bcx) < 16 &&
            dy >= -RS_HIT_DY_UP && dy <= RS_HIT_DY_DOWN &&
            rabs(pz + ROCKSTEADY_BULLET_TARGET_Z - bullets[i].z) < tolZ) {
            if (hitX) *hitX = bcx;
            bullets[i].hitTimer = ROCKSTEADY_BULLET_HIT_FRAMES;
            if (bullets[i].sprite)
                SPR_setAnimAndFrame(bullets[i].sprite, 0, ROCKSTEADY_BULLET_FR_HIT);
            if (rsShotHits < 255) rsShotHits++;
            return TRUE;
        }
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// API publica del jefe
// ---------------------------------------------------------------------------
void rocksteadyInit(Rocksteady* r) {
    memset(r, 0, sizeof(Rocksteady));
    r->sprite = NULL;
    r->state = ROCKSTEADY_INACTIVE;
    r->dir = -1;
    r->hp = ROCKSTEADY_HP;
    r->anim = 0xFF;
    r->slideDir = 1;
    bossFlashReset(&r->flash);
}

void rocksteadySpawn(Rocksteady* r) {
    rocksteadySpawnArena(r, &raLevel2);
}

void rocksteadySpawnArena(Rocksteady* r, const RocksteadyArena* a) {
    ra = *a;
    rocksteadyInit(r);
    r->x = ra.spawnX;
    r->y = ra.spawnY;
    r->hp = ra.hp ? ra.hp : ROCKSTEADY_HP;
    r->state = ROCKSTEADY_EMERGE;
    r->timer = 0;
    r->dur = ra.emergeStand;          // quieto en la puerta antes de bajar
    rsShotHits = 0;
    // SPR_addSpriteSafe: sprite grande que se crea con la VRAM potencialmente
    // fragmentada por los foot soldiers; desfragmenta y reintenta.
    r->sprite = SPR_addSpriteSafe(&rocksteady_boss, 0, 0,
                                  TILE_ATTR(ra.pal, FALSE, FALSE, FALSE));
    if (r->sprite) {
        rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_IDLE, TRUE);
        rocksteadyRender(r);
    }
}

bool rocksteadyIsActive(const Rocksteady* r) {
    return (r->state != ROCKSTEADY_INACTIVE && r->state != ROCKSTEADY_GONE);
}

bool rocksteadyCanBeHit(const Rocksteady* r) {
    switch (r->state) {
        case ROCKSTEADY_IDLE:
        case ROCKSTEADY_WALK:
        case ROCKSTEADY_WALK_ARMS:
        case ROCKSTEADY_SHOOT:
            return TRUE;
        case ROCKSTEADY_WINDUP:
        case ROCKSTEADY_CHARGE:
            return r->chargeHitCD == 0;   // con armadura
        default:
            return FALSE;
    }
}

s16 rocksteadyGetCenterX(const Rocksteady* r) { return r->x + ROCKSTEADY_FRAME_W / 2; }
s16 rocksteadyGetCenterY(const Rocksteady* r) { return r->y; }

// ---------------------------------------------------------------------------
// Arranque de cada estado
// ---------------------------------------------------------------------------
static void rsEnter(Rocksteady* r, RocksteadyState s, u16 dur) {
    r->state = s;
    r->timer = 0;
    r->dur = dur;
    r->fr = r->ft = 0;
}

// Anim manejada a mano (SPR_setFrame): apaga la auto-animacion.
static void rsManual(Rocksteady* r, u8 anim, u8 frame) {
    rocksteadyRestartAnim(r, anim, FALSE);
    SPR_setAutoAnimation(r->sprite, FALSE);
    SPR_setFrame(r->sprite, frame);
}

static void rsToIdle(Rocksteady* r) {
    rsEnter(r, ROCKSTEADY_IDLE, RS_IDLE_T);
    rocksteadySetAnim(r, r->armed ? ROCKSTEADY_ANIM_WALK_ARMS : ROCKSTEADY_ANIM_IDLE, TRUE);
}

static void rsToWalk(Rocksteady* r, u8 mode) {
    rsEnter(r, ROCKSTEADY_WALK, RS_WALK_T);
    r->mode = mode;
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_WALK, TRUE);
}

static void rsToWindup(Rocksteady* r, s8 dir) {
    rsEnter(r, ROCKSTEADY_WINDUP, RS_WINDUP_T);
    r->dir = dir;
    r->chargeHitCD = 0;
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_CHARGE, TRUE);
}

static void rsToKick(Rocksteady* r, s8 dir) {
    rsEnter(r, ROCKSTEADY_KICK, RS_KICK_T);
    r->dir = dir;
    rsManual(r, r->armed ? ROCKSTEADY_ANIM_KICK_ARMS : ROCKSTEADY_ANIM_KICK, 0);
}

static void rsToArms(Rocksteady* r, u8 mode, bool fast) {
    rsEnter(r, ROCKSTEADY_WALK_ARMS, fast ? RS_ARMS_FAST_T : RS_ARMS_T);
    r->mode = mode;
    r->fast = fast ? 1 : 0;
    rsManual(r, (mode == RS_MODE_AIM) ? ROCKSTEADY_ANIM_AIM : ROCKSTEADY_ANIM_WALK_ARMS, 0);
}

static void rsToShoot(Rocksteady* r) {
    rsEnter(r, ROCKSTEADY_SHOOT, RS_SHOOT_T);
    rsShotHits = 0;
    rsManual(r, ROCKSTEADY_ANIM_SHOOT, ROCKSTEADY_SHOOT_FR_H);
}

static void rsToFrenzy(Rocksteady* r) {
    rsEnter(r, ROCKSTEADY_FRENZY, RS_FRENZY_T);
    rsManual(r, ROCKSTEADY_ANIM_SHOOT, ROCKSTEADY_SHOOT_FR_REC);
}

static void rsToDraw(Rocksteady* r) {
    rsEnter(r, ROCKSTEADY_DRAW, RS_DRAW_T);
    r->armed = 1;                     // al levantarse ya lo tiene en la mano
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_DRAW, FALSE);
}

static void rsToHurt(Rocksteady* r) {
    rsEnter(r, ROCKSTEADY_HURT, RS_HURT_T);
    rsManual(r, r->armed ? ROCKSTEADY_ANIM_HURT_ARMS : ROCKSTEADY_ANIM_HURT, 0);
}

static void rsToKnockdown(Rocksteady* r) {
    rsEnter(r, ROCKSTEADY_KNOCKDOWN, RS_KD_T);
    r->combo = 0;
    r->kdHits = 0;
    r->slideDir = (s8)-r->dir;
    r->accX = 0;
    XGM2_stopPCM(SOUND_PCM_CH2);      // por si cae en plena embestida
    rsManual(r, ROCKSTEADY_ANIM_HURT, 2);
}

static void rsToDead(Rocksteady* r) {
    r->hp = 0;
    r->state = ROCKSTEADY_DEAD;
    XGM2_stopPCM(SOUND_PCM_CH2);
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_HURT, FALSE);
    // Grito de muerte en el instante del golpe fatal: canal PCM 2, prioridad
    // 15 (pisa el golpe del mismo frame). El wav dura 101 frames y entra
    // justo antes de que arranque music_ending.
    XGM2_playPCMEx(boss_scream_rocksteady_vo, sizeof(boss_scream_rocksteady_vo),
                   SOUND_PCM_CH2, 15, FALSE, FALSE);
}

static u8 rsRand(u8 n) { return (u8)(random() % n); }

// ---------------------------------------------------------------------------
// Dano recibido
// ---------------------------------------------------------------------------
void rocksteadyDamage(Rocksteady* r, s16 dmg) {
    rocksteadyDamageEx(r, dmg, (bool)(dmg >= ROCKSTEADY_SPECIAL_DMG));
}

void rocksteadyDamageEx(Rocksteady* r, s16 dmg, bool special) {
    if (!rocksteadyCanBeHit(r)) return;
    r->hp -= dmg;
    if (r->hp <= 0) { rsToDead(r); return; }

    // Embestida (amague o corrida): saca vida pero no la corta.
    if (r->state == ROCKSTEADY_WINDUP || r->state == ROCKSTEADY_CHARGE) {
        r->chargeHitCD = special ? ROCKSTEADY_CHARGE_HIT_CD_SP
                                 : ROCKSTEADY_CHARGE_HIT_CD;
        return;
    }

    u8 add = special ? 2 : 1;
    r->combo  += add;
    r->kdHits += add;
    if (r->kdHits >= RS_KD_HITS) { rsToKnockdown(r); return; }
    rsToHurt(r);
    // Con 4 golpes seguidos el contraataque sale en el update (mira a la
    // tortuga que tenga mas cerca).
}

// ---------------------------------------------------------------------------
// UPDATE PRINCIPAL
// ---------------------------------------------------------------------------
void rocksteadyUpdate(Rocksteady* r, s16 cameraX, Player* p1, Player* p2,
                      bool twoPlayers) {
    Player* ps[2] = { p1, (twoPlayers && p2) ? p2 : p1 };
    rocksteadyUpdateN(r, cameraX, ps, (twoPlayers && p2) ? 2 : 1);
}

// Mueve 'v' hacia 'to' como mucho 'step'.
static s16 rsToward(s16 v, s16 to, s16 step) {
    if (v < to) return (to - v < step) ? to : v + step;
    if (v > to) return (v - to < step) ? to : v - step;
    return v;
}

// Golpe por contacto (patada/embestida) contra todas las tortugas: voltea.
static void rsContact(Rocksteady* r, Player** pls, u8 nPl, s16 fwd, s16 back) {
    s16 bcx = r->x + ROCKSTEADY_FRAME_W / 2;
    for (u8 k = 0; k < nPl; k++) {
        Player* p = pls[k];
        if (isPlayerGameOver(p) || !playerCanBeHit(p)) continue;
        s16 dy = getPlayerY(p) - r->y;
        if (dy < -RS_HIT_DY_UP || dy > RS_HIT_DY_DOWN) continue;
        s16 rel = (getPlayerHurtCX(p) - bcx) * r->dir;   // + = adelante
        if (rel > fwd || rel < -back) continue;
        playerHitBarsKnockdown(p, bcx, ROCKSTEADY_CONTACT_DMG);
        XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
    }
}

// Avanza a mano una anim en loop de 'n' frames, 'ticks' frames de juego cada uno.
static void rsLoopAnim(Rocksteady* r, u8 n, u8 ticks) {
    if (++r->ft >= ticks) {
        r->ft = 0;
        if (++r->fr >= n) r->fr = 0;
        SPR_setFrame(r->sprite, r->fr);
    }
}

// Frenesi: 8 pasos (frames del remaster de PC) con su duracion, el frame del sheet que
// le toca y si tira (0 nada, 1 recto, 2 diagonal). En los pasos 2 y 6 se da
// vuelta.
static const u8 rsFrenzyDur[8] = { 12, 22, 12, 22, 7, 26, 7, 26 };
static const u8 rsFrenzyFr[8]  = { ROCKSTEADY_SHOOT_FR_REC, ROCKSTEADY_SHOOT_FR_H,
                                   ROCKSTEADY_SHOOT_FR_REC, ROCKSTEADY_SHOOT_FR_H,
                                   ROCKSTEADY_SHOOT_FR_UP0, ROCKSTEADY_SHOOT_FR_UP,
                                   ROCKSTEADY_SHOOT_FR_UP0, ROCKSTEADY_SHOOT_FR_UP };
static const u8 rsFrenzyShot[8] = { 0, 1, 0, 1, 0, 2, 0, 2 };

static void rsFire(Rocksteady* r, bool up) {
    s16 bcx = r->x + ROCKSTEADY_FRAME_W / 2;
    s16 mx = up ? ROCKSTEADY_MUZZLE_UP_X : ROCKSTEADY_MUZZLE_H_X;
    s16 mz = up ? ROCKSTEADY_MUZZLE_UP_Z : ROCKSTEADY_MUZZLE_H_Z;
    rocksteadyBulletSpawn(bcx + r->dir * mx, r->y, mz, r->dir, up, ra.pal);
}

void rocksteadyUpdateN(Rocksteady* r, s16 cameraX, Player** pls, u8 nPl) {
    if (r->state == ROCKSTEADY_INACTIVE || r->state == ROCKSTEADY_GONE ||
        !r->sprite) return;
    r->cameraOffsetX = cameraX;
    if (r->chargeHitCD > 0) r->chargeHitCD--;

    // Objetivo: la tortuga mas cercana en X (las que estan fuera de juego no
    // cuentan, salvo que no quede ninguna).
    s16 bcx = r->x + ROCKSTEADY_FRAME_W / 2;
    Player* tgt = pls[0];
    for (u8 k = 1; k < nPl; k++) {
        if (isPlayerGameOver(pls[k])) continue;
        if (isPlayerGameOver(tgt) ||
            rabs(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2 - bcx) <
            rabs(getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2 - bcx))
            tgt = pls[k];
    }
    s16 pcx = getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2;
    s16 py  = getPlayerY(tgt);
    s16 dx  = pcx - bcx;
    s16 dy  = py - r->y;
    s16 adx = rabs(dx);
    s8  toward = (dx >= 0) ? 1 : -1;
    bool aligned = (rabs(dy) <= RS_ALIGN_DY && adx <= RS_ALIGN_DX);
    r->timer++;

    // Contraataque: 4 golpes seguidos (y lejos de la caida) -> patada.
    if (r->state == ROCKSTEADY_HURT && r->combo >= RS_COUNTER_HITS &&
        r->kdHits < RS_KD_HITS) {
        r->combo = 0;
        rsToKick(r, toward);
    }

    switch (r->state) {

        case ROCKSTEADY_EMERGE: {
            // Quieto en la puerta (taunt) y despues baja caminando a la lane
            // de pelea.
            if (r->timer <= r->dur) break;
            rocksteadySetAnim(r, ROCKSTEADY_ANIM_WALK, TRUE);
            r->dir = toward;
            if (r->y < ra.emergeY)
                r->y = rsToward(r->y, ra.emergeY, rsStepY(r, RS_WALK_Q));
            else
                rsToIdle(r);
            break;
        }

        case ROCKSTEADY_IDLE: {
            if (rabs(dx) > RS_FACE_DEADZONE) r->dir = toward;
            if (r->timer < r->dur) break;
            if (r->armed) { rsToArms(r, aligned ? RS_MODE_AWAY : RS_MODE_CHASE, FALSE); break; }
            if (!aligned) {
                if (rsRand(2)) rsToWalk(r, RS_MODE_CHASE);
                else           rsToWindup(r, toward);
            } else {
                rsToWalk(r, RS_MODE_AHEAD);
            }
            break;
        }

        case ROCKSTEADY_WALK: {
            if (r->timer == 12) r->combo = 0;       // frame 2 de la caminata
            if (r->mode == RS_MODE_CHASE) {
                r->dir = toward;
                s16 sx = rsStepX(r, RS_WALK_Q);
                s16 sy = rsStepY(r, RS_WALK_Q);
                if (adx > RS_NEAR_DX) r->x += toward * sx;
                r->y = rsToward(r->y, py, sy);
                if (rabs(py - r->y) <= RS_ALIGN_DY && adx <= RS_ALIGN_DX &&
                    !isPlayerJumping(tgt)) {
                    rsToKick(r, toward);
                    break;
                }
            } else {
                r->x += r->dir * rsStepX(r, RS_WALK_Q);
                if (r->x <= ra.xMin || r->x >= ra.xMax) {
                    r->x = rclamp(r->x, ra.xMin, ra.xMax);
                    rsToIdle(r);
                    break;
                }
            }
            r->x = rclamp(r->x, ra.xMin, ra.xMax);
            r->y = rclamp(r->y, ra.laneTop, ra.laneBot);
            if (r->timer >= r->dur) {
                if (r->mode == RS_MODE_AHEAD) rsToWindup(r, toward);
                else                          rsToIdle(r);
            }
            break;
        }

        case ROCKSTEADY_WINDUP: {
            // Amague en el lugar, mirando a la tortuga; al terminar corre.
            r->dir = toward;
            if (r->timer >= r->dur) {
                rsEnter(r, ROCKSTEADY_CHARGE, RS_CHARGE_T);
                XGM2_playPCMEx(rocksteady_charge_sfx, sizeof(rocksteady_charge_sfx),
                               SOUND_PCM_CH2, 15, FALSE, FALSE);
            }
            break;
        }

        case ROCKSTEADY_CHARGE: {
            r->x += r->dir * rsStepX(r, RS_CHARGE_Q);
            r->y = rclamp(rsToward(r->y, py, rsStepY(r, RS_WALK_Q)), ra.laneTop, ra.laneBot);
            rsContact(r, pls, nPl, RS_CHARGE_DX, RS_CHARGE_DX);
            if (r->x <= ra.xMin || r->x >= ra.xMax) {
                r->x = rclamp(r->x, ra.xMin, ra.xMax);
                XGM2_stopPCM(SOUND_PCM_CH2);
                if (rsRand(2)) rsToWalk(r, RS_MODE_CHASE);
                else           rsToIdle(r);
            } else if (r->timer >= r->dur) {
                XGM2_stopPCM(SOUND_PCM_CH2);
                rsToIdle(r);
            }
            break;
        }

        case ROCKSTEADY_KICK: {
            if (r->timer == RS_KICK_F0) SPR_setFrame(r->sprite, 1);
            if (r->timer >= RS_KICK_F0) rsContact(r, pls, nPl, RS_KICK_FWD, RS_KICK_BACK);
            if (r->timer < r->dur) break;
            r->combo = 0;
            SPR_setAutoAnimation(r->sprite, TRUE);
            r->anim = 0xFF;
            if (!aligned) {
                if (r->armed) { if (rsRand(2)) rsToFrenzy(r); else rsToArms(r, RS_MODE_CHASE, FALSE); }
                else          { if (rsRand(2)) rsToWindup(r, toward); else rsToIdle(r); }
            } else {
                if (r->armed) rsToArms(r, RS_MODE_AWAY, FALSE);
                else          { r->dir = toward; rsToWalk(r, RS_MODE_AHEAD); }
            }
            break;
        }

        case ROCKSTEADY_HURT: {
            if (r->timer == RS_HURT_F) SPR_setFrame(r->sprite, 1);
            if (r->timer < r->dur) break;
            SPR_setAutoAnimation(r->sprite, TRUE);
            r->anim = 0xFF;
            r->dir = toward;
            if (r->armed) {
                if (rsRand(2)) rsToShoot(r);
                else           rsToArms(r, RS_MODE_AIM, FALSE);
            } else {
                u8 c = rsRand(3);
                if (c == 0)      rsToIdle(r);
                else if (c == 1) rsToWalk(r, RS_MODE_CHASE);
                else             rsToWalk(r, RS_MODE_AHEAD);
            }
            break;
        }

        case ROCKSTEADY_KNOCKDOWN: {
            if (r->timer <= RS_KD_SLIDE_T) {
                r->x = rclamp(r->x + r->slideDir * rsStepX(r, RS_SLIDE_Q), ra.xMin, ra.xMax);
                if (r->timer == RS_KD_SLIDE_T) SPR_setFrame(r->sprite, 3);
            }
            if (r->timer >= r->dur) {
                rsEnter(r, ROCKSTEADY_GETUP, RS_GETUP_T);
                SPR_setFrame(r->sprite, 4);
            }
            break;
        }

        case ROCKSTEADY_GETUP: {
            if (r->timer == RS_GETUP_F) SPR_setFrame(r->sprite, 5);
            if (r->timer < r->dur) break;
            SPR_setAutoAnimation(r->sprite, TRUE);
            r->anim = 0xFF;
            r->dir = toward;
            if (r->armed) { r->armed = 0; rsToIdle(r); }   // pierde el arma
            else          rsToDraw(r);                      // la saca
            break;
        }

        case ROCKSTEADY_DRAW: {
            if (r->timer < r->dur) break;
            r->anim = 0xFF;
            if (rsRand(2)) rsToFrenzy(r);
            else           rsToArms(r, aligned ? RS_MODE_AWAY : RS_MODE_CHASE, FALSE);
            break;
        }

        case ROCKSTEADY_WALK_ARMS: {
            // 6 frames a 14 fps (rapida: 18 fps).
            rsLoopAnim(r, 6, r->fast ? 3 : 4);
            if (r->mode == RS_MODE_CHASE) {
                r->dir = toward;
                s16 sx = rsStepX(r, RS_WALK_Q);
                s16 sy = rsStepY(r, RS_WALK_Q);
                if (adx > RS_NEAR_DX) r->x += toward * sx;
                r->y = rsToward(r->y, py, sy);
                if (rabs(py - r->y) <= RS_ALIGN_DY && adx <= RS_ALIGN_DX &&
                    !isPlayerJumping(tgt)) {
                    rsToKick(r, toward);
                    break;
                }
            } else if (r->mode == RS_MODE_AIM) {
                r->dir = toward;
                r->y = rsToward(r->y, py, rsStepY(r, RS_WALK_Q));
                if (rabs(py - r->y) <= RS_AIM_DY) { rsToShoot(r); break; }
            } else {   // RS_MODE_AWAY: se aleja hacia el borde
                r->dir = (s8)-toward;
                r->x += r->dir * rsStepX(r, RS_AWAY_Q);
                if (r->x <= ra.xMin || r->x >= ra.xMax) {
                    r->x = rclamp(r->x, ra.xMin, ra.xMax);
                    r->dir = toward;
                    rsToFrenzy(r);
                    break;
                }
            }
            r->x = rclamp(r->x, ra.xMin, ra.xMax);
            r->y = rclamp(r->y, ra.laneTop, ra.laneBot);
            if (r->timer >= r->dur) {
                if (r->mode == RS_MODE_AWAY) {
                    r->dir = toward;
                    u8 c = rsRand(3);
                    if (c == 0)      rsToArms(r, RS_MODE_CHASE, FALSE);
                    else if (c == 1) rsToShoot(r);
                    else             rsToFrenzy(r);
                } else {
                    rsToArms(r, r->mode, FALSE);
                }
            }
            break;
        }

        case ROCKSTEADY_SHOOT: {
            // 2 vueltas de 45: [0 fogonazo, 1, 2, 3, 2, 3] a 7,5 frames; la
            // bala sale con el fogonazo.
            u16 t = (u16)(r->timer - 1) % RS_SHOOT_LOOP_T;
            static const u8 seq[6] = { 0, 1, 2, 3, 2, 3 };
            u8 step = (u8)((t * 2) / 15);
            if (t == 0) rsFire(r, FALSE);
            SPR_setFrame(r->sprite, seq[step]);
            // Avanza despacio siguiendo la lane.
            r->x = rclamp(r->x + r->dir * rsStepX(r, RS_SHOOT_Q), ra.xMin, ra.xMax);
            r->y = rclamp(rsToward(r->y, py, rsStepY(r, RS_SHOOT_Q)), ra.laneTop, ra.laneBot);
            if (rsShotHits >= RS_SHOT_HITS) {
                rsShotHits = 0;
                r->dir = toward;
                if (rsRand(2)) rsToFrenzy(r);
                else           rsToArms(r, RS_MODE_CHASE, TRUE);
                break;
            }
            if (r->timer >= r->dur) { r->dir = toward; rsToArms(r, RS_MODE_CHASE, FALSE); }
            break;
        }

        case ROCKSTEADY_FRENZY: {
            // r->fr = paso (0..7), r->ft = frames que lleva en el paso.
            if (r->ft == 0) {
                if (r->fr == 2 || r->fr == 6) r->dir = (s8)-r->dir;
                SPR_setFrame(r->sprite, rsFrenzyFr[r->fr]);
                if (rsFrenzyShot[r->fr]) rsFire(r, rsFrenzyShot[r->fr] == 2);
            }
            if (++r->ft >= rsFrenzyDur[r->fr]) {
                r->ft = 0;
                if (++r->fr >= 8) { r->dir = toward; rsToArms(r, RS_MODE_CHASE, FALSE); }
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

    // Parpadeo de vida baja por linea de sprite (garage).
    if (ra.flashPal && r->sprite &&
        bossFlashStep(&r->flash, r->hp, ra.hp ? ra.hp : ROCKSTEADY_HP))
        SPR_setPalette(r->sprite, r->flash.on ? ra.flashPal : ra.pal);

    rocksteadyRender(r);
}
