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
    p->jumpVel       = 0;
    p->groundY       = startY;
    p->isJumpKicking = FALSE;
    p->apexHang      = 0;
    p->joyId         = joyId;
    p->prevJoy       = 0;
    p->dir           = 1;
    p->invincible    = 0;
    p->hurtTimer     = 0;
    p->hurtDir       = 0;
    p->hurtToggle    = 0;

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

// ---------------------------------------------------------------------------
// LÓGICA PRINCIPAL — llamar una vez por frame para cada instancia
// ---------------------------------------------------------------------------
void updatePlayer(Player* p) {
    u16 joy = JOY_readJoypad(p->joyId);

    // I-frames: cuenta regresiva + parpadeo clásico de invulnerabilidad.
    // El parpadeo usa visibilidad (no hay línea de paleta libre para flash y,
    // a diferencia de los enemigos, acá no compite con ningún otro efecto).
    if (p->invincible > 0) {
        p->invincible--;
        SPR_setVisibility(p->sprite, (p->invincible & 2) ? HIDDEN : VISIBLE);
        if (p->invincible == 0)
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
                p->x = clampS16(p->x + moveX, p->boundLeft, p->boundRight);
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
                p->state         = STATE_ATTACKING;
                p->comboStep     = 0;
                p->comboBuffered = 0;
                p->comboLinger   = COMBO_LINK_WINDOW;
                SPR_setAnimationLoop(p->sprite, FALSE);
                SPR_setAnimAndFrame(p->sprite, ANIM_SPECIAL, 0);
            } else if (cJust) {
                p->state     = STATE_JUMPING;
                p->jumpVel   = -PLAYER_JUMP_FORCE;
                p->groundY   = p->y;
                p->apexHang  = APEX_HANG;
                SPR_setAnimationLoop(p->sprite, TRUE);
                SPR_setAnim(p->sprite, ANIM_JUMP);
            } else if (bJust) {
                p->state         = STATE_ATTACKING;
                p->comboStep     = 1;
                p->comboBuffered = 0;
                p->comboLinger   = COMBO_LINK_WINDOW;
                SPR_setAnimationLoop(p->sprite, FALSE);
                SPR_setAnimAndFrame(p->sprite, ANIM_ATTACK_1, 0);
            } else if (justPressed(joy, p->prevJoy, BUTTON_A)) {
                p->state         = STATE_ATTACKING;
                p->comboStep     = 0;
                p->comboBuffered = 0;
                p->comboLinger   = COMBO_LINK_WINDOW;
                SPR_setAnimationLoop(p->sprite, FALSE);
                SPR_setAnimAndFrame(p->sprite, ANIM_KICK, 0);
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
                SPR_setAnimationLoop(p->sprite, TRUE);   // restaurar loop normal
                SPR_setAnim(p->sprite, ANIM_IDLE);
            }
            break;
        }

        case STATE_JUMPING: {
            p->y += p->jumpVel;

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

            if (joy & BUTTON_RIGHT) { p->x = clampS16(p->x + PLAYER_SPEED, p->boundLeft, p->boundRight); SPR_setHFlip(p->sprite, FALSE); p->dir = 1; }
            if (joy & BUTTON_LEFT)  { p->x = clampS16(p->x - PLAYER_SPEED, p->boundLeft, p->boundRight); SPR_setHFlip(p->sprite, TRUE);  p->dir = -1; }

            if (!p->isJumpKicking && (justPressed(joy, p->prevJoy, BUTTON_A) || justPressed(joy, p->prevJoy, BUTTON_B))) {
                p->isJumpKicking = TRUE;
                // La patada en salto tiene 2 frames: con el loop desactivado la
                // animación avanza al segundo (último) frame y queda "trabada"
                // ahí hasta tocar el piso.
                SPR_setAnimationLoop(p->sprite, FALSE);
                SPR_setAnim(p->sprite, ANIM_JUMP_KICK);
            }

            if (p->y >= p->groundY) {
                p->y       = p->groundY;
                p->jumpVel = 0;
                p->isJumpKicking = FALSE;
                p->state   = STATE_IDLE;
                SPR_setAnimationLoop(p->sprite, TRUE);   // restaurar loop normal
                SPR_setAnim(p->sprite, ANIM_IDLE);
            }
            break;
        }

        case STATE_HURT: {
            // Knockback: deslizarse alejándose del atacante los primeros frames
            if (p->hurtTimer > 0) {
                p->hurtTimer--;
                p->x = clampS16(p->x + p->hurtDir * PLAYER_HURT_KNOCK_SPEED,
                                p->boundLeft, p->boundRight);
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

        case STATE_GRABBED: {
            // El sistema de enemigos lo liberará
            break;
        }

        default: break;
    }

    p->prevJoy = joy;

    // Renderizar el sprite en posición de PANTALLA (mundo - cámara).
    // X: posición mundo menos cámara. Y: pies menos offset del frame para
    // que el arte (que vive en la parte baja del frame) quede sobre el suelo.
    SPR_setPosition(p->sprite, p->x - p->cameraOffsetX, p->y - PLAYER_FOOT_OFFSET);

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
    // En el aire (jump kick) el lane es groundY, no la Y del vuelo.
    s16 laneY = (p->state == STATE_JUMPING) ? p->groundY : p->y;
    if (absPS16(targetFeetY - laneY) > PLAYER_ATK_TOL_Y)
        return FALSE;

    return TRUE;
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
    // Saltando no se recibe daño (esquive aéreo estilo arcade). GRABBED lo
    // manejará el sistema de agarre cuando exista.
    if (p->state == STATE_HURT || p->state == STATE_JUMPING || p->state == STATE_GRABBED)
        return FALSE;
    return TRUE;
}

void damagePlayer(Player* p, s16 attackerX) {
    if (!playerCanBeHit(p)) return;

    // ¿De qué lado vino el golpe? El empuje va hacia el lado contrario.
    s16 centerX = p->x + PLAYER_SPRITE_W / 2;
    s8  side    = (attackerX >= centerX) ? 1 : -1;
    p->hurtDir  = -side;

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
    p->invincible = PLAYER_HURT_INVINCIBLE;

    // Un golpe corta cualquier combo en curso
    p->comboStep     = 0;
    p->comboBuffered = 0;
    p->comboLinger   = 0;

    // Sin loop + reinicio forzado desde el frame 0: dos golpes seguidos por
    // la espalda repiten HIT_BEHIND_1 y SPR_setAnim solo lo ignoraría (mismo
    // índice); SPR_setAnimAndFrame sí reinicia.
    SPR_setAnimationLoop(p->sprite, FALSE);
    SPR_setAnimAndFrame(p->sprite, anim, 0);
}
