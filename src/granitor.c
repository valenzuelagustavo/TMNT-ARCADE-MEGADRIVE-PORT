#include "granitor.h"
#include "audio.h"    // boss_hit, hit_turtles, foot_soldier_explode

// ===========================================================================
// GRANITOR + LLAMAS — ver granitor.h
// ===========================================================================
// La conducta (tiempos, rangos, la llamarada que crece y queda ardiendo) es
// la del companero (Ray Project); lo que cambio al portarla esta marcado con
// (port).
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
// LLAMAS: nacen chicas, CRECEN (anim 0, a mano) mientras avanzan y quedan
// como llamarada grande FIJA (anim 1, loop) ardiendo en el piso.
// ---------------------------------------------------------------------------
#define FLAME_Z   55    // (port) altura de la boca del lanzallamas sobre los pies

static struct {
    bool    active;
    Sprite* sprite;
    s16     x;             // centro (mundo)
    s16     lane;          // (port) lane de Granitor al disparar: pega en esa
    s8      dir;
    u8      mode;          // 0 = viajando/creciendo, 1 = llamarada fija
    u8      growFrame;
    u8      growTick;
    u16     t;
    u8      hitTravel;     // (port) mascara de jugadores ya quemados
    u8      hitBurn;
} flames[MAX_GRANITOR_FLAMES];

static void flameInitAll(void) {
    for (u16 i = 0; i < MAX_GRANITOR_FLAMES; i++) {
        flames[i].active = FALSE;
        flames[i].sprite = NULL;
    }
}

static void flameFire(s16 x, s16 lane, s8 dir) {
    for (u16 i = 0; i < MAX_GRANITOR_FLAMES; i++) {
        if (flames[i].active) continue;
        if (!flames[i].sprite)
            flames[i].sprite = SPR_addSprite(&granitor_flame, -64, -64,
                                             TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        if (!flames[i].sprite) return;          // (port) sin VRAM: no dispara
        flames[i].active    = TRUE;
        flames[i].x         = x;
        flames[i].lane      = lane;
        flames[i].dir       = dir;
        flames[i].mode      = 0;
        flames[i].growFrame = 0;
        flames[i].growTick  = 0;
        flames[i].t         = 0;
        flames[i].hitTravel = 0;
        flames[i].hitBurn   = 0;
        SPR_setAutoAnimation(flames[i].sprite, FALSE);
        SPR_setAnimAndFrame(flames[i].sprite, 0, 0);
        SPR_setHFlip(flames[i].sprite, dir < 0);
        SPR_setVisibility(flames[i].sprite, VISIBLE);
        return;
    }
}

void granitorFlameUpdate(Player** pls, u8 nPl, s16 camX) {
    for (u16 i = 0; i < MAX_GRANITOR_FLAMES; i++) {
        if (!flames[i].active) continue;
        flames[i].t++;
        if (flames[i].mode == 0) {
            flames[i].x += flames[i].dir * GRAN_FLAME_SPEED;
            if (++flames[i].growTick >= GRAN_FLAME_GROW_T) {
                flames[i].growTick = 0;
                if (flames[i].growFrame < 6) {
                    flames[i].growFrame++;
                    SPR_setFrame(flames[i].sprite, flames[i].growFrame);
                }
            }
            if (flames[i].t > GRAN_FLAME_TRAVEL_T || flames[i].growFrame >= 6) {
                flames[i].mode = 1;
                flames[i].t = 0;
                SPR_setAutoAnimation(flames[i].sprite, TRUE);
                SPR_setAnim(flames[i].sprite, 1);
                SPR_setAnimationLoop(flames[i].sprite, TRUE);
            }
        } else if (flames[i].t > GRAN_FLAME_BURN_T) {
            flames[i].active = FALSE;
            SPR_setVisibility(flames[i].sprite, HIDDEN);
            continue;
        }

        // (port) Quema a cada tortuga una vez en el viaje y una vez ardiendo.
        s16 w = (flames[i].mode == 1) ? 22 : 14;
        for (u8 k = 0; k < nPl; k++) {
            u8 bit = (u8)(1 << k);
            if (!playerCanBeHit(pls[k])) continue;
            s16 px = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
            if (gabs((s16)(px - flames[i].x)) >= w) continue;
            if (gabs((s16)(getPlayerY(pls[k]) - flames[i].lane)) >= GRAN_FLAME_TOL_Y) continue;
            u8* mask = (flames[i].mode == 1) ? &flames[i].hitBurn : &flames[i].hitTravel;
            if (*mask & bit) continue;
            *mask |= bit;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
            playerHitProjectile(pls[k], flames[i].x, GRAN_FLAME_DMG);
        }

        SPR_setPosition(flames[i].sprite, flames[i].x - camX - 16,
                        flames[i].lane - FLAME_Z - 16);
        SPR_setDepth(flames[i].sprite, (s16)(-(flames[i].lane) - 1));
    }
}

void granitorFlameReleaseAll(void) {
    for (u16 i = 0; i < MAX_GRANITOR_FLAMES; i++) {
        if (flames[i].sprite) { SPR_releaseSprite(flames[i].sprite); flames[i].sprite = NULL; }
        flames[i].active = FALSE;
    }
}

// ---------------------------------------------------------------------------
// GRANITOR
// ---------------------------------------------------------------------------
void granitorInit(Granitor* g) {
    g->sprite = NULL;
    g->state = GRAN_INACTIVE;
    g->xq = 0; g->yq = 0;
    g->camX = 0;
    g->dir = -1;
    g->hp = GRAN_HP;
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
    flameInitAll();
}

void granitorSpawn(Granitor* g, s16 arenaLeft, s16 arenaRight, s16 laneTop, s16 laneBot,
                   GranitorTopAtFn topAt) {
    PAL_setPalette(PAL3, granitor_boss.palette->data, DMA);
    if (!g->sprite)
        g->sprite = SPR_addSpriteSafe(&granitor_boss, -160, -160,
                                      TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
    g->state = GRAN_ENTER;
    g->hp = GRAN_HP;
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
        SPR_setAnim(g->sprite, GRAN_ANIM_WALK);
        SPR_setAnimationLoop(g->sprite, TRUE);
        g->anim = GRAN_ANIM_WALK;
    }
}

bool granitorIsGone(const Granitor* g)  { return g->state == GRAN_GONE; }
bool granitorIsDying(const Granitor* g) { return g->state == GRAN_DEATH || g->state == GRAN_GONE; }

bool granitorCanBeHit(const Granitor* g) {
    return (g->state != GRAN_INACTIVE && g->state != GRAN_DEATH &&
            g->state != GRAN_GONE && g->sprite);
}

static void granSetAnim(Granitor* g, u8 a, bool loop) {
    if (g->anim == a) return;
    g->anim = a;
    SPR_setAutoAnimation(g->sprite, TRUE);
    SPR_setAnimAndFrame(g->sprite, a, 0);
    SPR_setAnimationLoop(g->sprite, loop);
}

static void granManual(Granitor* g, u8 a, u8 f) {
    g->anim = a;
    SPR_setAutoAnimation(g->sprite, FALSE);
    SPR_setAnimAndFrame(g->sprite, a, f);
}

// (port) Tope de la lane en esta X: el de la arena y el de la pared del nivel.
static s16 granLaneTop(const Granitor* g, s16 x) {
    s16 t = g->laneTop;
    if (g->topAt) {
        s16 w = g->topAt(x);
        if (w > t) t = w;
    }
    return (t > g->laneBot) ? g->laneBot : t;
}

static void granKill(Granitor* g) {
    g->hp = 0;
    g->state = GRAN_DEATH;
    g->timer = 0;
    g->dPhase = 0;
    g->blinks = 0;
    granManual(g, GRAN_ANIM_DAMAGE, GRAN_DMG_FLASH);
    XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH2, 15, FALSE, FALSE);
}

bool granitorPlayerHits(Granitor* g, Player** pls, u8 nPl, s8* killer) {
    if (!granitorCanBeHit(g) || g->invuln) return FALSE;
    s16 gx = (s16)(g->xq >> 2);
    s16 gy = (s16)(g->yq >> 2);
    for (u8 k = 0; k < nPl; k++) {
        if (!playerAttackHitsBox(pls[k], gx, gy, GRAN_BODY_HALF_W, GRAN_BODY_H)) continue;
        s16 dmg = isPlayerSpecialAttack(pls[k]) ? GRAN_SPECIAL_DMG : 1;
        XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
        g->hp -= dmg;
        g->flash = 8;
        g->invuln = GRAN_INVULN;
        if (g->hp <= 0) {
            granKill(g);
            if (killer) *killer = (s8)k;
            return TRUE;
        }
        g->calm = 0;
        bool canReact = (g->state == GRAN_IDLE || g->state == GRAN_WALK ||
                         g->state == GRAN_HURT);
        if (canReact && g->comboHits >= GRAN_COUNTER_HITS) {
            // (port) Absorbe el golpe y contraataca hacia el que le pega.
            g->comboHits = 0;
            s16 kx = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
            g->dir = (kx >= gx) ? 1 : -1;
            g->state = GRAN_PUNCH;
            g->timer = 0;
            g->pFrame = 0; g->pLanded = 0;
            granManual(g, GRAN_ANIM_PUNCH, 0);
        } else if (canReact || g->state == GRAN_ENTER) {
            // El flinch solo interrumpe si no esta atacando (como en el original).
            g->comboHits++;
            g->state = GRAN_HURT;
            g->timer = 0;
            granManual(g, GRAN_ANIM_DAMAGE, GRAN_DMG_FLASH);
        }
        XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 12, FALSE, FALSE);
        return FALSE;
    }
    return FALSE;
}

static void granitorRender(Granitor* g) {
    s16 x = (s16)(g->xq >> 2);
    s16 y = (s16)(g->yq >> 2);
    SPR_setHFlip(g->sprite, (g->dir < 0));
    SPR_setPosition(g->sprite, x - g->camX - GRAN_FRAME_W / 2, y - GRAN_FOOT_OFFSET);
    SPR_setDepth(g->sprite, (s16)(-y));
    SPR_setVisibility(g->sprite, (g->flash & 1) ? HIDDEN : VISIBLE);
}

void granitorUpdate(Granitor* g, Player** pls, u8 nPl, s16 camX) {
    if (g->state == GRAN_INACTIVE || g->state == GRAN_GONE || !g->sprite) return;

    g->camX = camX;
    s16 x  = (s16)(g->xq >> 2);
    s16 y  = (s16)(g->yq >> 2);
    Player* tgt = nearestPlayer(pls, nPl, x);
    s16 px = (s16)(getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2);
    s16 py = getPlayerY(tgt);
    s16 dX = (s16)(px - x);
    s16 dY = (s16)(py - y);

    g->timer++;
    if (g->calm < 255) g->calm++;
    if (g->calm > GRAN_COMBO_RESET) g->comboHits = 0;
    if (g->attackCd) g->attackCd--;
    if (g->flash)    g->flash--;
    if (g->invuln)   g->invuln--;

    switch (g->state) {

    case GRAN_ENTER:
        granSetAnim(g, GRAN_ANIM_WALK, TRUE);
        g->dir = -1;
        g->xq -= GRAN_WALK_Q * 2;
        if ((s16)(g->xq >> 2) <= g->arenaRight - 60) {
            g->xq = (s16)((g->arenaRight - 60) * 4);
            g->state = GRAN_IDLE;
            g->timer = 0;
            g->attackCd = 30;
        }
        break;

    case GRAN_IDLE:
        granSetAnim(g, g->idleToggle ? GRAN_ANIM_IDLE2 : GRAN_ANIM_IDLE1, TRUE);
        g->dir = (dX >= 0) ? 1 : -1;
        if (g->timer > GRAN_IDLE_DECIDE) {
            g->idleToggle ^= 1;
            g->timer = 0;
            if (!g->attackCd && !isPlayerGameOver(tgt)) {
                if (gabs(dX) <= GRAN_PUNCH_RANGE && gabs(dY) <= GRAN_PUNCH_TOL_Y) {
                    g->state = GRAN_PUNCH;
                    g->pFrame = 0; g->pLanded = 0;
                    granManual(g, GRAN_ANIM_PUNCH, 0);
                    break;
                }
                // (port) Dispara alineado: la llamarada sale del arma y va
                // por SU lane; si no esta alineado, camina (y se alinea).
                if (gabs(dX) >= GRAN_FIRE_RANGE && gabs(dY) <= GRAN_FLAME_TOL_Y) {
                    g->state = GRAN_FIRE;
                    g->fFired = 0;
                    granManual(g, GRAN_ANIM_FIRE, 0);
                    break;
                }
            }
            g->state = GRAN_WALK;
        }
        break;

    case GRAN_WALK:
        granSetAnim(g, GRAN_ANIM_WALK, TRUE);
        g->dir = (dX >= 0) ? 1 : -1;
        if (gabs(dX) > GRAN_MIN_DIST)
            g->xq += (dX >= 0) ? GRAN_WALK_Q : -GRAN_WALK_Q;
        if (gabs(dY) > GRAN_WALK_DY / 2)       // (port) se alinea mejor en lane
            g->yq += (dY >= 0) ? 2 : -2;
        if (g->timer > 56) {
            g->state = GRAN_IDLE;
            g->timer = 0;
        }
        break;

    case GRAN_FIRE:
        // Un frame: la llamarada nace a los 10 ticks, a la altura del arma.
        if (!g->fFired && g->timer > 10) {
            g->fFired = 1;
            flameFire((s16)(x + g->dir * GRAN_FLAME_DX), y, g->dir);
        }
        if (g->timer > GRAN_FIRE_HOLD) {
            g->state = GRAN_IDLE;
            g->timer = 0;
            g->attackCd = GRAN_FIRE_CD;
        }
        break;

    case GRAN_PUNCH: {
        // 6 pasos logicos [0,1,2,3,3,3]: golpe en el 5to, remate en el 6to.
        u16 hold = (g->pFrame < 4) ? GRAN_PUNCH_TICKS
                 : (g->pFrame == 4) ? GRAN_PUNCH_IMPACT_T
                 : GRAN_PUNCH_RECOV_T;
        if (g->pFrame < 4) {
            s16 adv = (g->pFrame >= 2) ? (GRAN_PUNCH_ADV + 1) : GRAN_PUNCH_ADV;
            g->xq += g->dir * adv * 4;
        }
        if (g->pFrame == 4 && !g->pLanded) {
            // (port) pega a cualquier tortuga en el alcance, no solo al objetivo
            s16 hx = (s16)(x + g->dir * GRAN_PUNCH_HIT_DX);
            for (u8 k = 0; k < nPl; k++) {
                if (!playerCanBeHit(pls[k])) continue;
                s16 kx = (s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2);
                if (gabs((s16)(kx - hx)) >= GRAN_PUNCH_HIT_W) continue;
                if (gabs((s16)(getPlayerY(pls[k]) - y)) > GRAN_PUNCH_TOL_Y) continue;
                g->pLanded = 1;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH2, 15, FALSE, FALSE);
                playerHitBarsKnockdown(pls[k], x, GRAN_PUNCH_DMG);
            }
        }
        if (g->timer > hold) {
            g->timer = 0;
            g->pFrame++;
            if (g->pFrame > 5) {
                g->state = GRAN_IDLE;
                g->attackCd = GRAN_PUNCH_CD;
            } else {
                static const u8 map[6] = { 0, 1, 2, 3, 3, 3 };
                SPR_setFrame(g->sprite, map[g->pFrame]);
            }
        }
        break;
    }

    case GRAN_HURT:
        if (g->timer >= GRAN_HURT_TICKS) {
            g->state = GRAN_IDLE;
            g->timer = 0;
        }
        break;

    case GRAN_DEATH:
        g->flash = 0;
        if (g->dPhase == 0) {
            if (g->timer > 8) {
                g->timer = 0;
                g->blinks++;
                SPR_setFrame(g->sprite, (g->blinks & 1) ? GRAN_DMG_FLASH : GRAN_DMG_BREAK);
                if (g->blinks >= GRAN_DEATH_BLINK * 2) {
                    g->dPhase = 1;
                    SPR_setFrame(g->sprite, GRAN_DMG_BREAK);
                }
            }
        } else if (g->dPhase == 1) {
            if (g->timer > GRAN_DEATH_BREAK_T) {
                g->dPhase = 2;
                g->timer = 0;
                SPR_setFrame(g->sprite, GRAN_DMG_PILE);
                XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                               SOUND_PCM_CH3, 15, FALSE, FALSE);
            }
        } else if (g->timer > GRAN_DEATH_PILE_T) {
            g->state = GRAN_GONE;
            SPR_releaseSprite(g->sprite);
            g->sprite = NULL;
            granitorFlameReleaseAll();
            return;
        }
        break;

    default:
        break;
    }

    // Arena y lane (con la pared del nivel).
    x = gclamp((s16)(g->xq >> 2), g->arenaLeft, g->arenaRight);
    if (g->state != GRAN_ENTER) g->xq = (s16)(x * 4);
    y = gclamp((s16)(g->yq >> 2), granLaneTop(g, x), g->laneBot);
    g->yq = (s16)(y * 4);

    granitorRender(g);
}

void granitorRelease(Granitor* g) {
    if (g->sprite) { SPR_releaseSprite(g->sprite); g->sprite = NULL; }
    g->state = GRAN_INACTIVE;
    granitorFlameReleaseAll();
}
