#include "player.h"

// ===========================================================================
// MÓDULO DE JUGADOR — MULTI-INSTANCIA
// ===========================================================================
// Toda la lógica opera sobre un Player* recibido por parámetro, de modo que
// pueden coexistir varios jugadores (P1 con JOY_1, P2 con JOY_2, ...) cada
// uno con su propio sprite, estado, combo y salto.
// ===========================================================================

// ---------------------------------------------------------------------------
// FUNCIÓN DE INICIALIZACIÓN
// ---------------------------------------------------------------------------
void initPlayer(Player* p, u8 selectedCharacter, u16 joyId, u8 palette, s16 startX, s16 startY) {
    const SpriteDefinition* spriteDef = &leo_player;

    switch(selectedCharacter) {
        case 0: spriteDef = &leo_player;  break;
        case 1: spriteDef = &mike_player; break;
        case 2: spriteDef = &don_player;  break;   // Columna 2 = Don
        case 3: spriteDef = &raph_player; break;   // Columna 3 = Raph
    }

    p->x             = startX;
    p->y             = startY;       // Pies sobre la vereda
    p->state         = STATE_IDLE;
    p->boundLeft     = 0;
    p->boundRight    = 288;
    p->cameraOffsetX = 0;
    p->comboStep     = 0;
    p->comboBuffered = 0;
    p->comboLinger   = 0;
    p->airFrame      = 1;
    p->airTimer      = 0;
    p->attackIsSpecial = 0;
    p->jumpVel       = 0;
    p->jumpZ         = 0;
    p->isJumpKicking = FALSE;
    p->apexHang      = 0;
    p->joyId         = joyId;
    p->prevJoy       = 0;
    p->dir           = 1;
    p->invincible    = 0;
    p->hurtTimer     = 0;
    p->hurtDir       = 0;
    p->hurtToggle    = 0;
    p->koTimer       = 0;
    p->blinkTimer    = 0;
    p->gameOver      = FALSE;
    p->health        = PLAYER_MAX_HEALTH;   // barra de vida llena
    p->lives         = PLAYER_START_LIVES;
    p->score         = 0;

    p->sprite = SPR_addSprite(spriteDef, p->x, p->y, TILE_ATTR(palette, FALSE, FALSE, FALSE));
    // Las 4 tortugas comparten la misma paleta unificada; cargarla en 'palette'.
    PAL_setPalette(palette, spriteDef->palette->data, DMA);
    SPR_setAnim(p->sprite, ANIM_IDLE);
}

// ---------------------------------------------------------------------------
// API DE CÁMARA / LÍMITES
// ---------------------------------------------------------------------------
s16 getPlayerWorldX(const Player* p) {
    return p->x;
}

void setPlayerCamera(Player* p, s16 camX) {
    p->cameraOffsetX = camX;
}

void setPlayerLeftBound(Player* p, s16 leftBound) {
    p->boundLeft = leftBound;
}

void setPlayerRightBound(Player* p, s16 rightBound) {
    p->boundRight = rightBound;
}

// ---------------------------------------------------------------------------
// HELPERS INTERNOS
// ---------------------------------------------------------------------------
static s16 clampS16(s16 val, s16 minVal, s16 maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

static bool justPressed(u16 joy, u16 prev, u16 button) {
    return (bool)((joy & button) && !(prev & button));
}

// Pared diagonal del final del nivel: interpola linealmente entre los dos
// extremos calibrados (LEVEL_END_WALL_X_TOP/BOTTOM) según la profundidad
// 'y'. Devuelve la X de mundo del borde SÓLIDO para esa lane.
static s16 levelEndWallX(s16 y) {
    s16 laneRange = BOUND_LANE_BOTTOM - BOUND_LANE_TOP;
    s32 wallRange = LEVEL_END_WALL_X_BOTTOM - LEVEL_END_WALL_X_TOP;
    return LEVEL_END_WALL_X_TOP + (s16)(wallRange * (y - BOUND_LANE_TOP) / laneRange);
}

// ---------------------------------------------------------------------------
// LÓGICA PRINCIPAL — llamar una vez por frame para cada instancia
// ---------------------------------------------------------------------------
void updatePlayer(Player* p) {
    u16 joy = JOY_readJoypad(p->joyId);

    // Límite derecho EFECTIVO de este frame: el menor entre el borde de
    // pantalla (dinámico, dado por la cámara) y la pared diagonal del
    // final del nivel en la profundidad actual (fija en coordenadas de
    // mundo). La pared está dibujada en PERSPECTIVA, no vertical, así que
    // este límite depende de 'y' — ver LEVEL_END_WALL_X_TOP/BOTTOM.
    s16 wallRight = levelEndWallX(p->y) - PLAYER_SPRITE_W;
    s16 effRight  = (wallRight < p->boundRight) ? wallRight : p->boundRight;

    // I-frames: invulnerabilidad "lógica" SIN efecto visual. Un golpe normal
    // ya no hace parpadear al sprite (queda visible durante los i-frames).
    if (p->invincible > 0)
        p->invincible--;

    // Parpadeo: SOLO al revivir tras perder una vida (se activa en el respawn
    // del STATE_KO). Usa visibilidad; no toca la invulnerabilidad de arriba.
    if (p->blinkTimer > 0) {
        p->blinkTimer--;
        SPR_setVisibility(p->sprite, (p->blinkTimer & 2) ? HIDDEN : VISIBLE);
        if (p->blinkTimer == 0)
            SPR_setVisibility(p->sprite, VISIBLE);   // asegurar visible al final
    }

    switch (p->state) {

        case STATE_IDLE:
        case STATE_WALKING: {
            s16 moveX = 0;
            s16 moveY = 0;

            if (joy & BUTTON_RIGHT) { moveX =  PLAYER_SPEED; SPR_setHFlip(p->sprite, FALSE); p->dir = 1; }
            if (joy & BUTTON_LEFT)  { moveX = -PLAYER_SPEED; SPR_setHFlip(p->sprite, TRUE);  p->dir = -1; }
            if (joy & BUTTON_UP)    { moveY = -PLAYER_SPEED; }
            if (joy & BUTTON_DOWN)  { moveY =  PLAYER_SPEED; }

            if (moveX != 0 || moveY != 0) {
                p->x = clampS16(p->x + moveX, p->boundLeft, effRight);
                p->y = clampS16(p->y + moveY, BOUND_LANE_TOP, BOUND_LANE_BOTTOM);
                p->state = STATE_WALKING;

                if (moveY < 0) SPR_setAnim(p->sprite, ANIM_WALK_BACK);
                else           SPR_setAnim(p->sprite, ANIM_WALK_FRONT);
            } else {
                p->state = STATE_IDLE;
                SPR_setAnim(p->sprite, ANIM_IDLE);
            }

            // Detectar presses individuales antes de evaluar combos
            bool bJust = justPressed(joy, p->prevJoy, BUTTON_B);
            bool cJust = justPressed(joy, p->prevJoy, BUTTON_C);

            // Especial: B+C simultáneos (uno recién presionado, el otro activo).
            // Se chequea primero para que no se confunda con salto o golpe solo.
            // Los ataques arrancan SIN loop y desde el frame 0: la anim corre
            // una vez y queda congelada al final (ventana de enlace del combo).
            if ((bJust && (joy & BUTTON_C)) || (cJust && (joy & BUTTON_B))) {
                // ESPECIAL (B+C): mata foot soldiers de un golpe
                p->state         = STATE_ATTACKING;
                p->comboStep     = 0;
                p->comboBuffered = 0;
                p->comboLinger   = COMBO_LINK_WINDOW;
                p->attackIsSpecial = 1;
                SPR_setAnimationLoop(p->sprite, FALSE);
                SPR_setAnimAndFrame(p->sprite, ANIM_SPECIAL, 0);
            } else if (cJust) {
                // SALTO: la anim se controla a MANO por fases (subida/ápice/
                // aterrizaje), así que se apaga la auto-animación del sprite.
                // 'y' NO se toca al saltar: sigue siendo la lane real, y el
                // jugador puede seguir moviéndola en el aire (ver abajo).
                p->state     = STATE_JUMPING;
                p->jumpVel   = -PLAYER_JUMP_FORCE;
                p->jumpZ     = 0;
                p->apexHang  = APEX_HANG;
                p->airFrame  = 1;
                p->airTimer  = 0;
                SPR_setAutoAnimation(p->sprite, FALSE);
                SPR_setAnimAndFrame(p->sprite, ANIM_JUMP, 0);
            } else if (bJust) {
                p->state         = STATE_ATTACKING;
                p->comboStep     = 1;
                p->comboBuffered = 0;
                p->comboLinger   = COMBO_LINK_WINDOW;
                p->attackIsSpecial = 0;
                SPR_setAnimationLoop(p->sprite, FALSE);
                SPR_setAnimAndFrame(p->sprite, ANIM_ATTACK_1, 0);
            } else if (justPressed(joy, p->prevJoy, BUTTON_A)) {
                // ESPECIAL (A): antes disparaba ANIM_KICK; el kick queda
                // reservado para otro uso futuro.
                p->state         = STATE_ATTACKING;
                p->comboStep     = 0;
                p->comboBuffered = 0;
                p->comboLinger   = COMBO_LINK_WINDOW;
                p->attackIsSpecial = 1;
                SPR_setAnimationLoop(p->sprite, FALSE);
                SPR_setAnimAndFrame(p->sprite, ANIM_SPECIAL, 0);
            }
            break;
        }

        case STATE_ATTACKING: {
            // BUFFER de input: un press de B en CUALQUIER momento del swing
            // queda guardado y encadena al terminar la anim. Antes solo valía
            // el press del frame exacto de fin de anim (ventana de 1 frame).
            if (justPressed(joy, p->prevJoy, BUTTON_B))
                p->comboBuffered = 1;

            // Swing todavía en curso (anims de ataque corren sin loop)
            if (!SPR_isAnimationDone(p->sprite))
                break;

            // Swing terminado: ¿se encadena el siguiente golpe del combo?
            bool canChain = (p->comboStep > 0 && p->comboStep < 3);

            if (canChain && p->comboBuffered) {
                p->comboStep++;
                p->comboBuffered = 0;
                p->comboLinger   = COMBO_LINK_WINDOW;
                SPR_setAnimAndFrame(p->sprite,
                                    (p->comboStep == 2) ? ANIM_ATTACK_2
                                                        : ANIM_ATTACK_3, 0);
            } else if (canChain && p->comboLinger > 0) {
                // Ventana de enlace: quedarse unos frames en la pose final
                // esperando el press que encadena
                p->comboLinger--;
            } else {
                p->state         = STATE_IDLE;
                p->comboStep     = 0;
                p->comboBuffered = 0;
                p->comboLinger   = 0;
                p->attackIsSpecial = 0;
                SPR_setAnimationLoop(p->sprite, TRUE);   // restaurar loop normal
                SPR_setAnim(p->sprite, ANIM_IDLE);
            }
            break;
        }

        case STATE_JUMPING: {
            // jumpZ = altura VISUAL sobre el piso (crece al saltar, vuelve a
            // 0 al aterrizar). 'y' ya NO se toca acá: sigue siendo la lane
            // real de profundidad, libre de moverse con arriba/abajo.
            p->jumpZ -= p->jumpVel;

            // Float en el ápex: cuando la vel llega a 0 (punto más alto) y el
            // jugador no mueve horizontalmente, pausar la gravedad APEX_HANG frames.
            // Da esa sensación de "colgar" en el aire del beat-em-up clásico.
            bool noHoriz = !(joy & BUTTON_LEFT) && !(joy & BUTTON_RIGHT);
            if (p->jumpVel == 0 && noHoriz && p->apexHang > 0) {
                p->apexHang--;
                // No se aplica gravedad este frame → vel queda en 0
            } else {
                p->jumpVel += GRAVITY;
            }

            // --- Movimiento en el aire (X e Y) ---
            // Como en el arcade: saltando se puede seguir reposicionando en
            // X Y TAMBIÉN en Y (la lane), no solo en X — más movilidad.
            // Con patada FUERTE la tortuga viaja SOLA con ímpetu en X (más
            // rápido que el control normal) y la trayectoria queda
            // comprometida: sin control manual en ese caso. Sin patada
            // (o con la débil), control aéreo normal en ambos ejes.
            if (p->isJumpKicking == JUMPKICK_STRONG) {
                p->x = clampS16(p->x + p->dir * PLAYER_JUMPKICK_SPEED, p->boundLeft, effRight);
            } else {
                if (joy & BUTTON_RIGHT) { p->x = clampS16(p->x + PLAYER_SPEED, p->boundLeft, effRight); SPR_setHFlip(p->sprite, FALSE); p->dir = 1; }
                if (joy & BUTTON_LEFT)  { p->x = clampS16(p->x - PLAYER_SPEED, p->boundLeft, effRight); SPR_setHFlip(p->sprite, TRUE);  p->dir = -1; }
                if (joy & BUTTON_UP)    { p->y = clampS16(p->y - PLAYER_SPEED, BOUND_LANE_TOP, BOUND_LANE_BOTTOM); }
                if (joy & BUTTON_DOWN)  { p->y = clampS16(p->y + PLAYER_SPEED, BOUND_LANE_TOP, BOUND_LANE_BOTTOM); }
            }

            // --- Inicio de la patada en salto (una sola por salto) ---
            // Golpe solo            -> frame 0 de ANIM_JUMP_KICK (débil)
            // Golpe + direccion X   -> frame 1, con ímpetu (viaja más lejos)
            if (!p->isJumpKicking && (justPressed(joy, p->prevJoy, BUTTON_A) || justPressed(joy, p->prevJoy, BUTTON_B))) {
                if      (joy & BUTTON_RIGHT) { p->dir =  1; SPR_setHFlip(p->sprite, FALSE); }
                else if (joy & BUTTON_LEFT)  { p->dir = -1; SPR_setHFlip(p->sprite, TRUE);  }

                bool fuerte = (bool)(joy & (BUTTON_LEFT | BUTTON_RIGHT));
                p->isJumpKicking = fuerte ? JUMPKICK_STRONG : JUMPKICK_SOFT;
                // Auto-anim ya está apagada desde el inicio del salto: el
                // frame elegido queda clavado hasta aterrizar.
                SPR_setAnimAndFrame(p->sprite, ANIM_JUMP_KICK, fuerte ? 1 : 0);
            }

            // --- Fases de la animación del salto (solo si NO está pateando) ---
            // Subida: frame 0. Ápice y caída: loop del frame 1 al anteúltimo.
            // Justo antes de tocar el suelo (~2 frames de anticipación,
            // predicho con la velocidad actual): último frame.
            if (!p->isJumpKicking) {
                u16 n = p->sprite->animation->numFrame;
                if (p->jumpVel < 0) {
                    SPR_setFrame(p->sprite, 0);
                } else if (p->jumpVel > 0 && p->jumpZ <= (p->jumpVel << 1)) {
                    SPR_setFrame(p->sprite, n - 1);
                } else if (n >= 3) {
                    if (++p->airTimer >= PLAYER_JUMP_LOOP_TICKS) {
                        p->airTimer = 0;
                        p->airFrame++;
                        if (p->airFrame > n - 2) p->airFrame = 1;
                    }
                    SPR_setFrame(p->sprite, p->airFrame);
                }
            }

            // --- Aterrizaje ---
            if (p->jumpZ <= 0) {
                p->jumpZ   = 0;
                p->jumpVel = 0;
                p->isJumpKicking = JUMPKICK_NONE;
                p->state   = STATE_IDLE;
                SPR_setAutoAnimation(p->sprite, TRUE);   // devolver la anim al motor
                SPR_setAnimationLoop(p->sprite, TRUE);
                SPR_setAnim(p->sprite, ANIM_IDLE);
            }
            break;
        }

        case STATE_HURT: {
            // Knockback: deslizarse alejándose del atacante los primeros frames
            if (p->hurtTimer > 0) {
                p->hurtTimer--;
                p->x = clampS16(p->x + p->hurtDir * PLAYER_HURT_KNOCK_SPEED,
                                p->boundLeft, effRight);
            }
            // La anim de hit corre SIN loop (se setea en damagePlayer): cuando
            // termina, volver a IDLE y restaurar el loop normal. El knockback
            // actúa además como duración MÍNIMA del estado: con una anim muy
            // corta (1 frame) isAnimationDone daría TRUE al instante y la
            // reacción no llegaría a verse.
            if (p->hurtTimer == 0 && SPR_isAnimationDone(p->sprite)) {
                p->state = STATE_IDLE;
                SPR_setAnimationLoop(p->sprite, TRUE);
                SPR_setAnim(p->sprite, ANIM_IDLE);
            }
            break;
        }

        case STATE_KO: {
            // Tortuga knockeada: la anim de caída (ANIM_HIT_BEHIND_2) corre una
            // vez y queda CONGELADA en su último frame (tortuga tirada). Un
            // pequeño deslizamiento inicial, y se mantiene 'un momento'.
            if (p->hurtTimer > 0) {
                p->hurtTimer--;
                p->x = clampS16(p->x + p->hurtDir * PLAYER_HURT_KNOCK_SPEED,
                                p->boundLeft, effRight);
            }
            if (p->koTimer > 0) {
                p->koTimer--;
                if (p->koTimer == 0) {
                    if (p->lives > 0) {
                        // Revivir: recargar la barra y volver a ser jugable.
                        // ACÁ sí arranca el parpadeo de invulnerabilidad.
                        p->health     = PLAYER_MAX_HEALTH;
                        p->state      = STATE_IDLE;
                        p->invincible = PLAYER_RESPAWN_INVINCIBLE;
                        p->blinkTimer = PLAYER_RESPAWN_INVINCIBLE;
                        // Reactivar la auto-animación (se apagó al congelar el KO)
                        SPR_setAutoAnimation(p->sprite, TRUE);
                        SPR_setAnimationLoop(p->sprite, TRUE);
                        SPR_setAnim(p->sprite, ANIM_IDLE);
                    } else {
                        // Sin vidas: game over. Queda tirada; scenes.c lo detecta.
                        p->gameOver = TRUE;
                    }
                }
            }
            break;
        }

        case STATE_GRABBED: {
            // El sistema de enemigos lo liberará
            break;
        }

        default: break;
    }

    p->prevJoy = joy;

    // Renderizar el sprite en posición de PANTALLA (mundo - cámara).
    // X: posición mundo menos cámara. Y: pies menos offset del frame para
    // que el arte (que vive en la parte baja del frame) quede sobre el
    // suelo, menos jumpZ (altura visual del salto, 0 si no está saltando).
    // Durante el ESPECIAL el sprite se dibuja unos px más arriba (el arte es
    // un saltito en el lugar) — offset solo visual, la Y lógica no cambia.
    s16 drawY = p->y - PLAYER_FOOT_OFFSET - p->jumpZ;
    if (p->state == STATE_ATTACKING && p->attackIsSpecial)
        drawY -= PLAYER_SPECIAL_LIFT;
    SPR_setPosition(p->sprite, p->x - p->cameraOffsetX, drawY);

    // Prioridad por profundidad (Y-sorting estilo beat-em-up): quien tiene
    // mayor Y de pies está MÁS CERCA de la cámara y debe dibujarse adelante.
    // En SGDK, menor 'depth' = más al frente, por eso usamos -y. Sirve igual
    // para P1, P2 y futuros enemigos que usen este mismo criterio.
    SPR_setDepth(p->sprite, -(p->y));
}

static s16 absPS16(s16 v) {
    return (v < 0) ? -v : v;
}

// ---------------------------------------------------------------------------
// HITBOX DE ATAQUE — tortuga → enemigos
// ---------------------------------------------------------------------------
bool isPlayerAttackActive(const Player* p) {
    // Swing de ataque en curso. La pose congelada de la ventana de enlace
    // (anim terminada) ya NO pega: la hitbox vive solo durante la animación.
    if (p->state == STATE_ATTACKING)
        return !SPR_isAnimationDone(p->sprite);
    // Patada en salto: activa todo el tiempo que dura el vuelo con la patada
    if (p->state == STATE_JUMPING && p->isJumpKicking)
        return TRUE;
    return FALSE;
}

bool playerAttackHits(const Player* p, s16 targetCX, s16 targetFeetY) {
    if (!isPlayerAttackActive(p))
        return FALSE;

    // Alcance horizontal medido desde el CENTRO del frame, hacia adelante.
    // (El código anterior medía desde el borde izquierdo: pegando a la
    // derecha la ventana cubría -12..+28px del centro — el golpe pegaba
    // "arriba" de la tortuga y nunca adelante, donde frenan los enemigos.)
    s16 pcx = p->x + PLAYER_SPRITE_W / 2;
    s16 dx  = (p->dir >= 0) ? (targetCX - pcx) : (pcx - targetCX);
    if (dx < -PLAYER_ATK_BACK || dx > PLAYER_ATK_REACH)
        return FALSE;

    // Profundidad: tolerancia SIMÉTRICA alrededor del lane del jugador.
    // 'y' es siempre la lane real (también en el aire: jumpZ es un offset
    // solo visual y no la toca), así que no hace falta ningún caso especial.
    if (absPS16(targetFeetY - p->y) > PLAYER_ATK_TOL_Y)
        return FALSE;

    return TRUE;
}

bool isPlayerSpecialAttack(const Player* p) {
    return (p->state == STATE_ATTACKING && p->attackIsSpecial);
}

s8 getPlayerDir(const Player* p) {
    return p->dir;
}

s16 getPlayerY(const Player* p) {
    return p->y;
}

// ---------------------------------------------------------------------------
// DAÑO RECIBIDO
// ---------------------------------------------------------------------------
bool playerCanBeHit(const Player* p) {
    if (p->invincible > 0) return FALSE;
    // Saltando no se recibe daño (esquive aéreo estilo arcade). KO = ya está
    // en el piso; GRABBED lo manejará el sistema de agarre cuando exista.
    if (p->state == STATE_HURT || p->state == STATE_JUMPING ||
        p->state == STATE_KO   || p->state == STATE_GRABBED)
        return FALSE;
    return TRUE;
}

void damagePlayer(Player* p, s16 attackerX) {
    if (!playerCanBeHit(p)) return;

    // ¿De qué lado vino el golpe? El empuje va hacia el lado contrario.
    s16 centerX = p->x + PLAYER_SPRITE_W / 2;
    s8  side    = (attackerX >= centerX) ? 1 : -1;
    p->hurtDir  = -side;

    // Un golpe corta cualquier combo o especial en curso
    p->comboStep     = 0;
    p->comboBuffered = 0;
    p->comboLinger   = 0;
    p->attackIsSpecial = 0;

    // --- Vida: el golpe resta una barra ---
    if (p->health > 0) p->health--;

    if (p->health == 0) {
        // === KNOCKOUT: se agotó la barra -> pierde una vida ===
        // Se muestra el último frame de ANIM_HIT_BEHIND_2 (tortuga knockeada)
        // un momento; recién ahí revive (o es game over). Ver STATE_KO.
        if (p->lives > 0) p->lives--;

        p->state      = STATE_KO;
        p->koTimer    = PLAYER_KO_FRAMES;
        p->hurtTimer  = PLAYER_HURT_KNOCK_FRAMES;   // deslizamiento inicial
        p->invincible = PLAYER_KO_FRAMES;           // intocable mientras está en el piso (SIN parpadeo)

        // Saltar DIRECTO al frame de la tortuga tirada y CONGELARLO (auto-anim
        // apagada): no se reproducen los frames de la caída, sólo la pose KO.
        SPR_setAutoAnimation(p->sprite, FALSE);
        SPR_setAnimAndFrame(p->sprite, ANIM_HIT_BEHIND_2, PLAYER_KO_FRAME);
        return;
    }

    // === Golpe normal (todavía con vida) ===
    // Golpe por la espalda: el atacante está del lado contrario a la mirada.
    // La tortuga NO se da vuelta: la anim HIT_BEHIND muestra la reacción.
    bool behind = (side != p->dir);

    u16 anim;
    if (behind) {
        anim = ANIM_HIT_BEHIND_1;
    } else {
        anim = p->hurtToggle ? ANIM_HIT_2 : ANIM_HIT_1;
        p->hurtToggle ^= 1;
    }

    p->state      = STATE_HURT;
    p->hurtTimer  = PLAYER_HURT_KNOCK_FRAMES;
    p->invincible = PLAYER_HURT_INVINCIBLE;   // i-frames SIN parpadeo

    // Sin loop + reinicio forzado desde el frame 0: dos golpes seguidos por
    // la espalda repiten HIT_BEHIND_1 y SPR_setAnim solo lo ignoraría (mismo
    // índice); SPR_setAnimAndFrame sí reinicia.
    SPR_setAnimationLoop(p->sprite, FALSE);
    SPR_setAnimAndFrame(p->sprite, anim, 0);
}

// ---------------------------------------------------------------------------
// VIDA / VIDAS / PUNTAJE
// ---------------------------------------------------------------------------
s16 getPlayerHealth(const Player* p) {
    return p->health;
}

u8 getPlayerLives(const Player* p) {
    return p->lives;
}

u16 getPlayerScore(const Player* p) {
    return p->score;
}

void addPlayerScore(Player* p, u16 points) {
    p->score += points;
}

bool isPlayerGameOver(const Player* p) {
    // TRUE recién cuando terminó la pose de knockeado sin vidas restantes
    // (se activa al final del STATE_KO), no en el instante del golpe: así el
    // jugador ve la caída antes del game over.
    return p->gameOver;
}
