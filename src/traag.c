#include "traag.h"
#include "audio.h"    // boss_hit, hit_turtles, foot_soldier_explode

// ===========================================================================
// TRAAG + MISILES — ver traag.h
// ===========================================================================
// La conducta (tiempos, rangos, el misil con su explosion) es la del
// companero (Ray Project); lo que cambio al portarla esta marcado con (port).
// Mismo esqueleto que granitor.c (Ray hizo a Granitor copiando a Traag).
// ===========================================================================

static s16 gabs(s16 v)                 { return (v < 0) ? -v : v; }
static s16 gclamp(s16 v, s16 a, s16 b) { return (v < a) ? a : ((v > b) ? b : v); }

// (port) El jugador EN JUEGO mas cercano; si no queda ninguno, el P1.
static Player* nearestPlayer(Player** pls, u8 nPl, s16 x) {
    Player* best = pls[0];
    s16 bestD = 0x7FFF;
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k])) continue;
        s16 d = gabs((s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2 - x));
        if (d < bestD) { bestD = d; best = pls[k]; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// MISILES: rectos, por la lane de Traag, a la altura del cano. Revientan al
// tocar a una tortuga (o si una tortuga les pega) y la explosion quema un
// ratito.
// ---------------------------------------------------------------------------
static struct {
    bool    active;
    Sprite* sprite;
    s16     x;             // centro (mundo)
    s16     lane;          // (port) lane de Traag al disparar
    s8      dir;
} missiles[MAX_TRAAG_MISSILES];

static struct {
    bool    active;
    Sprite* sprite;
    s16     x, lane;
    u8      frame, tick, t;
    u8      hitMask;       // (port) jugadores ya quemados
} booms[MAX_TRAAG_EXPLOSIONS];

static s16 msCamX, msCamY;

static void missileInitAll(void) {
    for (u16 i = 0; i < MAX_TRAAG_MISSILES; i++) { missiles[i].active = FALSE; missiles[i].sprite = NULL; }
    for (u16 i = 0; i < MAX_TRAAG_EXPLOSIONS; i++) { booms[i].active = FALSE; booms[i].sprite = NULL; }
}

static void boomAt(s16 x, s16 lane) {
    for (u16 i = 0; i < MAX_TRAAG_EXPLOSIONS; i++) {
        if (booms[i].active) continue;
        if (!booms[i].sprite)
            booms[i].sprite = SPR_addSprite(&traag_explosao, -48, -48,
                                            TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        if (!booms[i].sprite) return;
        booms[i].active  = TRUE;
        booms[i].x       = x;
        booms[i].lane    = lane;
        booms[i].frame   = 0;
        booms[i].tick    = 0;
        booms[i].t       = 0;
        booms[i].hitMask = 0;
        SPR_setAutoAnimation(booms[i].sprite, FALSE);
        SPR_setAnimAndFrame(booms[i].sprite, 0, 0);
        SPR_setVisibility(booms[i].sprite, VISIBLE);
        XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                       SOUND_PCM_CH3, 13, FALSE, FALSE);
        return;
    }
}

static void missileHide(u16 i) {
    missiles[i].active = FALSE;
    if (missiles[i].sprite) SPR_setVisibility(missiles[i].sprite, HIDDEN);
}

static void missileFire(s16 x, s16 lane, s8 dir) {
    for (u16 i = 0; i < MAX_TRAAG_MISSILES; i++) {
        if (missiles[i].active) continue;
        if (!missiles[i].sprite)
            missiles[i].sprite = SPR_addSprite(&traag_missil, -80, -80,
                                               TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        if (!missiles[i].sprite) return;        // (port) sin VRAM: no dispara
        missiles[i].active = TRUE;
        missiles[i].x      = x;
        missiles[i].lane   = lane;
        missiles[i].dir    = dir;
        SPR_setAutoAnimation(missiles[i].sprite, TRUE);
        SPR_setAnim(missiles[i].sprite, 0);
        SPR_setAnimationLoop(missiles[i].sprite, TRUE);
        SPR_setHFlip(missiles[i].sprite, dir < 0);   // (port) el arte mira a la derecha
        SPR_setVisibility(missiles[i].sprite, VISIBLE);
        return;
    }
}

void traagMissileUpdate(Player** pls, u8 nPl, s16 camX, s16 camY) {
    msCamX = camX;
    msCamY = camY;
    for (u16 i = 0; i < MAX_TRAAG_MISSILES; i++) {
        if (!missiles[i].active) continue;
        missiles[i].x += missiles[i].dir * TRAAG_MISSILE_SPEED;
        if (missiles[i].x < camX - 40 || missiles[i].x > camX + 360) { missileHide(i); continue; }

        bool hit = FALSE;
        for (u8 k = 0; k < nPl && !hit; k++) {
            // Si una tortuga le pega, revienta ahi (sin dano).
            if (playerAttackHitsBox(pls[k], missiles[i].x, missiles[i].lane, 12, TRAAG_GUN_DY + 12)) {
                hit = TRUE;
                break;
            }
            if (!playerCanBeHit(pls[k])) continue;
            s16 px = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
            if (gabs((s16)(px - missiles[i].x)) >= 12) continue;
            if (gabs((s16)(getPlayerY(pls[k]) - missiles[i].lane)) >= TRAAG_MISSILE_TOL_Y) continue;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
            playerHitProjectile(pls[k], missiles[i].x, TRAAG_MISSILE_DMG);
            hit = TRUE;
        }
        if (hit) {
            boomAt(missiles[i].x, missiles[i].lane);
            missileHide(i);
            continue;
        }
        SPR_setPosition(missiles[i].sprite, missiles[i].x - camX - 32,
                        missiles[i].lane - TRAAG_GUN_DY - camY - 16);
        SPR_setDepth(missiles[i].sprite, (s16)(-(missiles[i].lane) - 1));
    }
    for (u16 i = 0; i < MAX_TRAAG_EXPLOSIONS; i++) {
        if (!booms[i].active) continue;
        booms[i].t++;
        if (++booms[i].tick >= TRAAG_EXPL_TICKS) {
            booms[i].tick = 0;
            if (booms[i].frame < 6) {
                booms[i].frame++;
                SPR_setFrame(booms[i].sprite, booms[i].frame);
            }
        }
        if (booms[i].t > TRAAG_EXPL_TICKS * 7 + 4) {
            booms[i].active = FALSE;
            SPR_setVisibility(booms[i].sprite, HIDDEN);
            continue;
        }
        // La explosion quema al principio (una vez a cada tortuga).
        if (booms[i].t <= TRAAG_EXPL_HIT_T) {
            for (u8 k = 0; k < nPl; k++) {
                u8 bit = (u8)(1 << k);
                if ((booms[i].hitMask & bit) || !playerCanBeHit(pls[k])) continue;
                s16 px = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
                if (gabs((s16)(px - booms[i].x)) >= 16) continue;
                if (gabs((s16)(getPlayerY(pls[k]) - booms[i].lane)) >= TRAAG_MISSILE_TOL_Y) continue;
                booms[i].hitMask |= bit;
                playerHitProjectile(pls[k], booms[i].x, TRAAG_EXPL_DMG);
            }
        }
        SPR_setPosition(booms[i].sprite, booms[i].x - camX - 16,
                        booms[i].lane - TRAAG_GUN_DY - camY - 16);
        SPR_setDepth(booms[i].sprite, (s16)(-(booms[i].lane) - 2));
    }
}

void traagMissileReleaseAll(void) {
    for (u16 i = 0; i < MAX_TRAAG_MISSILES; i++) {
        if (missiles[i].sprite) { SPR_releaseSprite(missiles[i].sprite); missiles[i].sprite = NULL; }
        missiles[i].active = FALSE;
    }
    for (u16 i = 0; i < MAX_TRAAG_EXPLOSIONS; i++) {
        if (booms[i].sprite) { SPR_releaseSprite(booms[i].sprite); booms[i].sprite = NULL; }
        booms[i].active = FALSE;
    }
}

// ---------------------------------------------------------------------------
// TRAAG
// ---------------------------------------------------------------------------
void traagInit(Traag* g) {
    g->sprite = NULL;
    g->state = TRAAG_INACTIVE;
    g->xq = 0; g->yq = 0;
    g->camX = 0;
    g->camY = 0;
    g->dir = -1;
    g->hp = TRAAG_HP;
    g->anim = 0xFF;
    g->timer = 0;
    g->attackCd = 0;
    g->flash = 0;
    g->invuln = 0;
    g->idleToggle = 0;
    g->pFrame = 0; g->pLanded = 0;
    g->fFired = 0;
    g->comboHits = 0; g->calm = 0;
    g->dPhase = 0; g->blinks = 0;
    g->arenaLeft = 0; g->arenaRight = 0;
    g->laneTop = 142; g->laneBot = 196;
    g->topAt = NULL;
    missileInitAll();
}

void traagSpawn(Traag* g, s16 arenaLeft, s16 arenaRight, s16 laneTop, s16 laneBot,
                   TraagTopAtFn topAt) {
    PAL_setPalette(PAL3, traag_boss.palette->data, DMA);
    if (!g->sprite)
        g->sprite = SPR_addSpriteSafe(&traag_boss, -160, -160,
                                      TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    g->state = TRAAG_ENTER;
    g->hp = TRAAG_HP;
    g->xq = (s16)((arenaRight + 56) * 4);
    g->yq = (s16)(((laneTop + laneBot) / 2) * 4);
    g->dir = -1;
    g->anim = 0xFF;
    g->attackCd = 0;
    g->flash = 0;
    g->invuln = 0;
    g->timer = 0;
    g->comboHits = 0;
    g->calm = 0;
    g->arenaLeft = arenaLeft;
    g->arenaRight = arenaRight;
    g->laneTop = laneTop;
    g->laneBot = laneBot;
    g->topAt = topAt;
    if (g->sprite) {
        SPR_setVisibility(g->sprite, VISIBLE);
        SPR_setAutoAnimation(g->sprite, TRUE);
        SPR_setAnim(g->sprite, TRAAG_ANIM_WALK);
        SPR_setAnimationLoop(g->sprite, TRUE);
        g->anim = TRAAG_ANIM_WALK;
    }
}

bool traagIsGone(const Traag* g)  { return g->state == TRAAG_GONE; }
bool traagIsDying(const Traag* g) { return g->state == TRAAG_DEATH || g->state == TRAAG_GONE; }

bool traagCanBeHit(const Traag* g) {
    return (g->state != TRAAG_INACTIVE && g->state != TRAAG_DEATH &&
            g->state != TRAAG_GONE && g->sprite);
}

static void traagSetAnim(Traag* g, u8 a, bool loop) {
    if (g->anim == a) return;
    g->anim = a;
    SPR_setAutoAnimation(g->sprite, TRUE);
    SPR_setAnimAndFrame(g->sprite, a, 0);
    SPR_setAnimationLoop(g->sprite, loop);
}

static void traagManual(Traag* g, u8 a, u8 f) {
    g->anim = a;
    SPR_setAutoAnimation(g->sprite, FALSE);
    SPR_setAnimAndFrame(g->sprite, a, f);
}

// (port) Tope de la lane en esta X: el de la arena y el de la pared del nivel.
static s16 traagLaneTop(const Traag* g, s16 x) {
    s16 t = g->laneTop;
    if (g->topAt) {
        s16 w = g->topAt(x);
        if (w > t) t = w;
    }
    return (t > g->laneBot) ? g->laneBot : t;
}

static void traagKill(Traag* g) {
    g->hp = 0;
    g->state = TRAAG_DEATH;
    g->timer = 0;
    g->dPhase = 0;
    g->blinks = 0;
    traagManual(g, TRAAG_ANIM_DAMAGE, TRAAG_DMG_FLASH);
    XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH2, 15, FALSE, FALSE);
}

bool traagPlayerHits(Traag* g, Player** pls, u8 nPl, s8* killer) {
    if (!traagCanBeHit(g) || g->invuln) return FALSE;
    s16 gx = (s16)(g->xq >> 2);
    s16 gy = (s16)(g->yq >> 2);
    for (u8 k = 0; k < nPl; k++) {
        if (!playerAttackHitsBox(pls[k], gx, gy, TRAAG_BODY_HALF_W, TRAAG_BODY_H)) continue;
        s16 dmg = isPlayerSpecialAttack(pls[k]) ? TRAAG_SPECIAL_DMG : 1;
        XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
        g->hp -= dmg;
        g->flash = 8;
        g->invuln = TRAAG_INVULN;
        if (g->hp <= 0) {
            traagKill(g);
            if (killer) *killer = (s8)k;
            return TRUE;
        }
        g->calm = 0;
        bool canReact = (g->state == TRAAG_IDLE || g->state == TRAAG_WALK ||
                         g->state == TRAAG_HURT);
        if (canReact && g->comboHits >= TRAAG_COUNTER_HITS) {
            // (port) Absorbe el golpe y contraataca hacia el que le pega.
            g->comboHits = 0;
            s16 kx = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
            g->dir = (kx >= gx) ? 1 : -1;
            g->state = TRAAG_PUNCH;
            g->timer = 0;
            g->pFrame = 0; g->pLanded = 0;
            traagManual(g, TRAAG_ANIM_PUNCH, 0);
        } else if (canReact || g->state == TRAAG_ENTER) {
            // El flinch solo interrumpe si no esta atacando (como en el original).
            g->comboHits++;
            g->state = TRAAG_HURT;
            g->timer = 0;
            traagManual(g, TRAAG_ANIM_DAMAGE, TRAAG_DMG_FLASH);
        }
        XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 12, FALSE, FALSE);
        return FALSE;
    }
    return FALSE;
}

static void traagRender(Traag* g) {
    s16 x = (s16)(g->xq >> 2);
    s16 y = (s16)(g->yq >> 2);
    SPR_setHFlip(g->sprite, (g->dir < 0));
    SPR_setPosition(g->sprite, x - g->camX - TRAAG_FRAME_W / 2, y - g->camY - TRAAG_FOOT_OFFSET);
    SPR_setDepth(g->sprite, (s16)(-y));
    SPR_setVisibility(g->sprite, (g->flash & 1) ? HIDDEN : VISIBLE);
}

void traagUpdate(Traag* g, Player** pls, u8 nPl, s16 camX, s16 camY) {
    if (g->state == TRAAG_INACTIVE || g->state == TRAAG_GONE || !g->sprite) return;

    g->camX = camX;
    g->camY = camY;
    s16 x  = (s16)(g->xq >> 2);
    s16 y  = (s16)(g->yq >> 2);
    Player* tgt = nearestPlayer(pls, nPl, x);
    s16 px = (s16)(getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2);
    s16 py = getPlayerY(tgt);
    s16 dX = (s16)(px - x);
    s16 dY = (s16)(py - y);

    g->timer++;
    if (g->calm < 255) g->calm++;
    if (g->calm > TRAAG_COMBO_RESET) g->comboHits = 0;
    if (g->attackCd) g->attackCd--;
    if (g->flash)    g->flash--;
    if (g->invuln)   g->invuln--;

    switch (g->state) {

    case TRAAG_ENTER:
        traagSetAnim(g, TRAAG_ANIM_WALK, TRUE);
        g->dir = -1;
        g->xq -= TRAAG_WALK_Q * 2;
        if ((s16)(g->xq >> 2) <= g->arenaRight - 60) {
            g->xq = (s16)((g->arenaRight - 60) * 4);
            g->state = TRAAG_IDLE;
            g->timer = 0;
            g->attackCd = 30;
        }
        break;

    case TRAAG_IDLE:
        traagSetAnim(g, g->idleToggle ? TRAAG_ANIM_IDLE2 : TRAAG_ANIM_IDLE1, TRUE);
        g->dir = (dX >= 0) ? 1 : -1;
        if (g->timer > TRAAG_IDLE_DECIDE) {
            g->idleToggle ^= 1;
            g->timer = 0;
            if (!g->attackCd && !isPlayerGameOver(tgt)) {
                if (gabs(dX) <= TRAAG_PUNCH_RANGE && gabs(dY) <= TRAAG_PUNCH_TOL_Y) {
                    g->state = TRAAG_PUNCH;
                    g->pFrame = 0; g->pLanded = 0;
                    traagManual(g, TRAAG_ANIM_PUNCH, 0);
                    break;
                }
                // (port) Dispara alineado: el misil sale del arma y va
                // por SU lane; si no esta alineado, camina (y se alinea).
                if (gabs(dX) >= TRAAG_FIRE_RANGE && gabs(dY) <= TRAAG_MISSILE_TOL_Y) {
                    g->state = TRAAG_FIRE;
                    g->fFired = 0;
                    g->fFrame = 0;
                    g->dblShot = 0;
                    traagManual(g, TRAAG_ANIM_FIRE, 0);
                    break;
                }
            }
            g->state = TRAAG_WALK;
        }
        break;

    case TRAAG_WALK:
        traagSetAnim(g, TRAAG_ANIM_WALK, TRUE);
        g->dir = (dX >= 0) ? 1 : -1;
        if (gabs(dX) > TRAAG_MIN_DIST)
            g->xq += (dX >= 0) ? TRAAG_WALK_Q : -TRAAG_WALK_Q;
        if (gabs(dY) > TRAAG_WALK_DY / 2)       // (port) se alinea mejor en lane
            g->yq += (dY >= 0) ? 2 : -2;
        if (g->timer > 56) {
            g->state = TRAAG_IDLE;
            g->timer = 0;
        }
        break;

    case TRAAG_FIRE:
        // f0: el misil nace EXACTAMENTE aca; f1: retroceso. A veces rafaga
        // doble (segundo misil durante el retroceso).
        if (g->fFrame == 0) {
            if (!g->fFired) {
                g->fFired = 1;
                missileFire((s16)(x + g->dir * TRAAG_GUN_DX), y, g->dir);
            }
            if (g->timer > TRAAG_FIRE_F0_TICKS) {
                g->fFrame = 1;
                g->timer = 0;
                SPR_setFrame(g->sprite, 1);
            }
        } else if (g->timer == 8 && !g->dblShot) {
            g->dblShot = 1;
            if (random() & 1)
                missileFire((s16)(x + g->dir * TRAAG_GUN_DX), y, g->dir);
        } else if (g->timer > TRAAG_FIRE_F1_TICKS) {
            g->state = TRAAG_IDLE;
            g->timer = 0;
            g->attackCd = TRAAG_FIRE_CD;
        }
        break;

    case TRAAG_PUNCH: {
        // 6 frames reales: preparo 0-3 (avanza), IMPACTO en el 4, remate 5.
        u16 hold = (g->pFrame < 4) ? TRAAG_PUNCH_TICKS
                 : (g->pFrame == 4) ? TRAAG_PUNCH_IMPACT_T
                 : TRAAG_PUNCH_RECOV_T;
        if (g->pFrame < 4) {
            s16 adv = (g->pFrame >= 2) ? (TRAAG_PUNCH_ADV + 1) : TRAAG_PUNCH_ADV;
            g->xq += g->dir * adv * 4;
        }
        if (g->pFrame == 4 && !g->pLanded) {
            // (port) pega a cualquier tortuga en el alcance, no solo al objetivo
            s16 hx = (s16)(x + g->dir * TRAAG_PUNCH_HIT_DX);
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                s16 kx = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
                if (gabs((s16)(kx - hx)) >= TRAAG_PUNCH_HIT_W) continue;
                if (gabs((s16)(getPlayerY(pls[k]) - y)) > TRAAG_PUNCH_TOL_Y) continue;
                g->pLanded = 1;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                playerHitBarsKnockdown(pls[k], x, TRAAG_PUNCH_DMG);
            }
        }
        if (g->timer > hold) {
            g->timer = 0;
            g->pFrame++;
            if (g->pFrame > 5) {
                g->state = TRAAG_IDLE;
                g->attackCd = TRAAG_PUNCH_CD;
            } else {
                SPR_setFrame(g->sprite, g->pFrame);
            }
        }
        break;
    }

    case TRAAG_HURT:
        if (g->timer >= TRAAG_HURT_TICKS) {
            g->state = TRAAG_IDLE;
            g->timer = 0;
        }
        break;

    case TRAAG_DEATH:
        g->flash = 0;
        if (g->dPhase == 0) {
            if (g->timer > 8) {
                g->timer = 0;
                g->blinks++;
                SPR_setFrame(g->sprite, (g->blinks & 1) ? TRAAG_DMG_FLASH : TRAAG_DMG_BREAK);
                if (g->blinks >= TRAAG_DEATH_BLINK * 2) {
                    g->dPhase = 1;
                    SPR_setFrame(g->sprite, TRAAG_DMG_BREAK);
                }
            }
        } else if (g->dPhase == 1) {
            if (g->timer > TRAAG_DEATH_BREAK_T) {
                g->dPhase = 2;
                g->timer = 0;
                SPR_setFrame(g->sprite, TRAAG_DMG_PILE);
                XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                               SOUND_PCM_CH3, 15, FALSE, FALSE);
            }
        } else if (g->timer > TRAAG_DEATH_PILE_T) {
            g->state = TRAAG_GONE;
            SPR_releaseSprite(g->sprite);
            g->sprite = NULL;
            traagMissileReleaseAll();
            return;
        }
        break;

    default:
        break;
    }

    // Arena y lane (con la pared del nivel).
    x = gclamp((s16)(g->xq >> 2), g->arenaLeft, g->arenaRight);
    if (g->state != TRAAG_ENTER) g->xq = (s16)(x * 4);
    y = gclamp((s16)(g->yq >> 2), traagLaneTop(g, x), g->laneBot);
    g->yq = (s16)(y * 4);

    traagRender(g);
}

void traagRelease(Traag* g) {
    if (g->sprite) { SPR_releaseSprite(g->sprite); g->sprite = NULL; }
    g->state = TRAAG_INACTIVE;
    traagMissileReleaseAll();
}
