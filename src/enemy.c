#include "enemy.h"

// ---------------------------------------------------------------------------
// Cambio de animación con guarda: solo re-setea si la anim es distinta a la
// actual (evita reiniciar el ciclo cada frame). 'loop' controla si la anim
// se repite (caminar) o queda clavada en el último frame (ataques).
// ---------------------------------------------------------------------------
static void enemySetAnim(Enemy* e, u8 anim, bool loop) {
    if (e->anim == anim) return;
    e->anim = anim;
    SPR_setAnim(e->sprite, anim);
    SPR_setAnimationLoop(e->sprite, loop);
}

// Reinicia una animación desde el frame 0 AUNQUE sea la misma que ya está
// seteada. Necesario para los ataques: quedan clavados en el último frame
// (sin loop) y SPR_setAnim ignora el cambio si el índice es el mismo — un
// segundo kick consecutivo no se reiniciaría. SPR_setAnimAndFrame sí fuerza
// el reinicio porque el frame actual difiere de 0.
static void enemyRestartAnim(Enemy* e, u8 anim, bool loop) {
    e->anim = anim;
    SPR_setAnimAndFrame(e->sprite, anim, 0);
    SPR_setAnimationLoop(e->sprite, loop);
}

// ---------------------------------------------------------------------------
// FLASH DE GOLPE — silueta blanca estilo arcade
// ---------------------------------------------------------------------------
// Al recibir un golpe, el sprite cambia su ATRIBUTO de paleta a una línea
// "flash" (todo blanco) durante ENEMY_FLASH_FRAMES frames y luego vuelve a su
// paleta normal. Cambiar el atributo no cuesta DMA ni reescribe CRAM por golpe:
// la paleta blanca se carga una sola vez al inicio del nivel.
// OJO: la paleta normal del foot soldier (PAL2) ahora también la usa el fuego
// de BG_A — el flash NO la toca (solo cambia el atributo del sprite), así que
// el fuego no parpadea cuando un enemigo recibe un golpe.
// ---------------------------------------------------------------------------
static u16 enemyFlashPal = PAL3;

// ---------------------------------------------------------------------------
// IA DE GRUPO — atacantes simultáneos
// ---------------------------------------------------------------------------
// Estilo arcade: como mucho ENEMY_MAX_ATTACKERS foot soldiers pueden estar en
// ATTACK a la vez; el resto rodea al jugador a distancia (HOLD). El contador
// se incrementa al entrar en ATTACK y se decrementa en TODAS las salidas
// (timer cumplido, golpeado, muerto).
// ---------------------------------------------------------------------------
static u8 enemiesAttacking = 0;

// --- Reparto de targets en 2 jugadores ---
// enemyTargetCount[i] = cuántos enemigos vivos tienen asignado al jugador i.
// Se usa para que los spawns nuevos vayan al jugador menos "cargado".
static u8 enemyNumPlayers = 1;
static u8 enemyTargetCount[2] = {0, 0};

void resetEnemyAI(u8 numPlayers) {
    enemiesAttacking = 0;
    enemyNumPlayers = (numPlayers >= 2) ? 2 : 1;
    enemyTargetCount[0] = 0;
    enemyTargetCount[1] = 0;
}

// Decremento seguro del contador de targets (al morir o cambiar de blanco)
static void releaseTarget(u8 target) {
    if (enemyTargetCount[target] > 0)
        enemyTargetCount[target]--;
}

// Decremento seguro del contador cuando un enemigo SALE del estado ATTACK
static void leaveAttackState(Enemy* e) {
    if (e->state == ENEMY_STATE_ATTACK && enemiesAttacking > 0)
        enemiesAttacking--;
}

void initEnemyFlashPalette(u16 palLine) {
    u16 flashPal[16];
    flashPal[0] = 0x0000;                                // color 0 = transparente
    for (u16 i = 1; i < 16; i++) flashPal[i] = 0x0EEE;   // resto: blanco puro
    enemyFlashPal = palLine;
    // CPU y no DMA: el array vive en el stack y la transferencia DMA se difiere
    // al vblank, cuando este buffer ya no existiría. Son 16 words, es trivial.
    PAL_setPalette(palLine, flashPal, CPU);
}

void initEnemySpawn(Enemy* e, s16 spawnX, s16 y, s16 patrolRange, u8 palette) {
    e->x           = spawnX;
    e->y           = y;
    e->patrolLeft  = spawnX - patrolRange;
    e->patrolRight = spawnX + patrolRange;
    e->cameraOffsetX = 0;
    e->state       = ENEMY_STATE_PATROL;
    e->dir         = -1;
    e->timer       = 0;
    e->hp          = ENEMY_HP;
    e->invincible  = 0;
    e->palette     = palette;
    e->flashTimer  = 0;
    e->anim        = 0xFF;   // sentinela: fuerza el primer enemySetAnim
    e->attackType  = ENEMY_ATTACK_PUNCH;
    e->attackHit   = 0;
    e->hitToggle   = 0;
    // Cooldown inicial aleatorio: los spawns cercanos no atacan sincronizados
    e->attackCooldown = (u8)(random() & 31);

    // Target inicial: el jugador con MENOS enemigos asignados (reparto
    // parejo). En empate, al azar. En 1P siempre P1.
    if (enemyNumPlayers == 2) {
        if      (enemyTargetCount[0] < enemyTargetCount[1]) e->target = 0;
        else if (enemyTargetCount[1] < enemyTargetCount[0]) e->target = 1;
        else                                                e->target = (u8)(random() & 1);
    } else {
        e->target = 0;
    }
    enemyTargetCount[e->target]++;
    e->retargetTimer = ENEMY_RETARGET_INTERVAL;

    e->sprite = SPR_addSprite(&foot_soldier, e->x, e->y, TILE_ATTR(palette, FALSE, FALSE, FALSE));
    PAL_setPalette(palette, foot_soldier.palette->data, DMA);
    enemySetAnim(e, ENEMY_ANIM_WALK, TRUE);
}

void initEnemyDoorSpawn(Enemy* e, s16 doorCenterX, u8 palette) {
    // Reusa el init normal (crea sprite, hp, target, paleta), posicionándolo en
    // el hueco: centrado en la puerta y en la lane del fondo (donde está el
    // marco). Luego lo pasa a SPAWNING con la anim de rotura de puerta.
    initEnemySpawn(e, doorCenterX - ENEMY_SPRITE_W / 2, ENEMY_LANE_TOP, 60, palette);
    e->state = ENEMY_STATE_SPAWNING;
    e->timer = ENEMY_BREAK_DOOR_TIME;
    e->dir   = 1;   // se corrige solo al pasar a CHASE

    // ANIM_BREAK_DOOR sin loop, arrancando en el SEGUNDO frame (índice 1): el
    // primer frame es la puerta cerrada, que ya mostraba el sprite door_lvl_1.
    e->anim = ENEMY_ANIM_BREAK_DOOR;
    SPR_setAnimAndFrame(e->sprite, ENEMY_ANIM_BREAK_DOOR, 1);
    SPR_setAnimationLoop(e->sprite, FALSE);
}

void initEnemyElevatorSpawn(Enemy* e, s16 doorCenterX, u8 palette) {
    // Como el spawn de puerta, pero mostrando SÓLO los 2 últimos frames de
    // BREAK_DOOR (índices 3 y 4): la puerta del ascensor ya se abrió con su
    // propia animación, así que el foot soldier únicamente "sale" del hueco.
    initEnemySpawn(e, doorCenterX - ENEMY_SPRITE_W / 2, ENEMY_LANE_TOP, 60, palette);
    e->state = ENEMY_STATE_SPAWNING;
    e->timer = ENEMY_ELEV_SPAWN_TIME;
    e->dir   = 1;

    e->anim = ENEMY_ANIM_BREAK_DOOR;
    SPR_setAnimAndFrame(e->sprite, ENEMY_ANIM_BREAK_DOOR, 3);   // arranca en el 4to frame (índice 3)
    SPR_setAnimationLoop(e->sprite, FALSE);
}

void setEnemyCamera(Enemy* e, s16 camX) {
    e->cameraOffsetX = camX;
}

bool damageEnemy(Enemy* e, s16 dmg) {
    if (e->state == ENEMY_STATE_DEAD || e->state == ENEMY_STATE_INACTIVE)
        return FALSE;

    // Si estaba atacando, libera el "cupo" de atacante para otro enemigo
    leaveAttackState(e);
    // Golpeado → no contraataca al instante al recuperarse
    e->attackCooldown = ENEMY_HURT_COOLDOWN;

    e->hp -= dmg;
    if (e->hp <= 0) {
        // Muerte: animación de EXPLOSIÓN. Sin flash blanco (para que se vean
        // los colores de la explosión) y con la paleta normal asegurada por si
        // venía en flash de un golpe anterior. La anim corre una vez (sin loop)
        // y el sprite se libera al agotarse el timer (ver bloque DEAD).
        e->flashTimer = 0;
        SPR_setPalette(e->sprite, e->palette);
        e->state = ENEMY_STATE_DEAD;
        e->timer = ENEMY_EXPLODE_TIME;
        enemyRestartAnim(e, ENEMY_ANIM_EXPLODE, FALSE);
        return TRUE;
    }

    // Golpe NO fatal: flash blanco (cambio de atributo de paleta; updateEnemy lo
    // restaura al llegar flashTimer a 0) + estado HURT con knockback.
    SPR_setPalette(e->sprite, enemyFlashPal);
    e->flashTimer = ENEMY_FLASH_FRAMES;
    e->state = ENEMY_STATE_HURT;
    e->timer = 12;
    e->invincible = ENEMY_INVINCIBLE;
    // Anim de golpe recibido: alterna entre las tres (8/9/10) en cada golpe.
    u8 hitAnim = (u8)(ENEMY_ANIM_HIT_1 + e->hitToggle);
    if (++e->hitToggle >= 3) e->hitToggle = 0;
    enemyRestartAnim(e, hitAnim, FALSE);
    return TRUE;
}

bool enemyCanBeHit(const Enemy* e) {
    if (e->state == ENEMY_STATE_DEAD || e->state == ENEMY_STATE_INACTIVE ||
        e->state == ENEMY_STATE_SPAWNING)
        return FALSE;
    if (e->invincible > 0 || e->state == ENEMY_STATE_HURT)
        return FALSE;
    return TRUE;
}

s16 getEnemyCenterX(const Enemy* e) {
    return e->x + ENEMY_SPRITE_W / 2;
}

s16 getEnemyCenterY(const Enemy* e) {
    return e->y;
}

static s16 absS16(s16 v) {
    return (v < 0) ? -v : v;
}

static s16 clampS16(s16 val, s16 minVal, s16 maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

static s16 distS16(s16 a, s16 b) {
    return absS16(a - b);
}

// Tope de X de mundo para un enemigo en la profundidad 'y': el menor entre
// el borde físico del nivel (1376 - ancho de sprite) y la pared diagonal
// del final (ver ENEMY_END_WALL_X_TOP/BOTTOM), interpolada según la lane.
static s16 enemyMaxX(s16 y) {
    s16 laneRange = ENEMY_LANE_BOTTOM - ENEMY_LANE_TOP;
    s32 wallRange = ENEMY_END_WALL_X_BOTTOM - ENEMY_END_WALL_X_TOP;
    s16 wallX     = ENEMY_END_WALL_X_TOP + (s16)(wallRange * (y - ENEMY_LANE_TOP) / laneRange);
    s16 levelMax  = 1376 - ENEMY_SPRITE_W;
    s16 wallMax   = wallX - ENEMY_SPRITE_W;
    return (wallMax < levelMax) ? wallMax : levelMax;
}

void updateEnemy(Enemy* e, s16 player1X, s16 player1Y, s16 player2X, s16 player2Y, bool twoPlayers) {
    if (e->state == ENEMY_STATE_INACTIVE || !e->sprite) return;

    if (e->invincible > 0) e->invincible--;
    if (e->attackCooldown > 0) e->attackCooldown--;

    // Cuenta regresiva del flash de golpe: al llegar a 0, restaurar la paleta
    // normal. Va ANTES del bloque DEAD para que también funcione en el golpe
    // final (el sprite queda 30 frames en pantalla, el flash dura 6).
    if (e->flashTimer > 0) {
        e->flashTimer--;
        if (e->flashTimer == 0)
            SPR_setPalette(e->sprite, e->palette);
    }

    if (e->state == ENEMY_STATE_DEAD) {
        // Empuje inicial en X alejándose del golpe (el enemigo mira al que lo
        // golpeó, así que retrocede hacia -dir) los primeros frames de la muerte.
        if (e->timer > ENEMY_EXPLODE_TIME - ENEMY_DEATH_KNOCK_FRAMES)
            e->x = clampS16(e->x - e->dir * ENEMY_DEATH_KNOCK_SPEED,
                            ENEMY_WORLD_MIN_X, enemyMaxX(e->y));
        if (e->timer > 0) {
            e->timer--;
            if (e->timer == 0) {
                releaseTarget(e->target);   // libera su lugar en el reparto
                SPR_releaseSprite(e->sprite);
                e->sprite = NULL;
                e->state = ENEMY_STATE_INACTIVE;
                return;
            }
        }
        SPR_setPosition(e->sprite, e->x - e->cameraOffsetX, e->y - ENEMY_FOOT_OFFSET);
        return;
    }

    // Rompiendo la puerta (spawn): solo reproduce ANIM_BREAK_DOOR sin IA ni
    // colisión. Al agotarse el timer pasa a CHASE y se vuelve un enemigo normal.
    if (e->state == ENEMY_STATE_SPAWNING) {
        if (e->timer > 0) e->timer--;
        else              e->state = ENEMY_STATE_CHASE;
        SPR_setHFlip(e->sprite, (e->dir < 0));
        SPR_setPosition(e->sprite, e->x - e->cameraOffsetX, e->y - ENEMY_FOOT_OFFSET);
        SPR_setDepth(e->sprite, -(e->y));
        return;
    }

    // --- Target asignado (Fase 3) ---
    // Cada enemigo persigue a SU jugador asignado. Cada RETARGET_INTERVAL
    // frames re-evalúa: solo cambia si el otro jugador está bastante más
    // cerca (histéresis) — sin esto, el "más cercano por frame" oscilaba y en
    // la práctica los enemigos terminaban siempre sobre P1.
    if (twoPlayers && enemyNumPlayers == 2) {
        if (e->retargetTimer > 0) {
            e->retargetTimer--;
        } else {
            e->retargetTimer = ENEMY_RETARGET_INTERVAL;
            u8  other = e->target ^ 1;
            s16 dCur  = distS16(e->x, (e->target == 0) ? player1X : player2X);
            s16 dOth  = distS16(e->x, (other == 0)     ? player1X : player2X);
            if (dOth + ENEMY_RETARGET_HYSTERESIS < dCur) {
                releaseTarget(e->target);
                e->target = other;
                enemyTargetCount[other]++;
            }
        }
    } else {
        e->target = 0;
    }

    s16 targetX = (e->target == 1) ? player2X : player1X;
    s16 targetY = (e->target == 1) ? player2Y : player1Y;

    s16 dx   = targetX - e->x;
    s16 dy   = targetY - e->y;
    s16 dist = absS16(dx);

    EnemyState newState = e->state;

    switch (e->state) {

        case ENEMY_STATE_PATROL: {
            if (dist < ENEMY_AGGRO_RANGE) {
                newState = ENEMY_STATE_CHASE;
                break;
            }
            e->x += e->dir * ENEMY_SPEED;
            if (e->x <= e->patrolLeft)  { e->x = e->patrolLeft;  e->dir = 1; }
            if (e->x >= e->patrolRight) { e->x = e->patrolRight; e->dir = -1; }
            enemySetAnim(e, ENEMY_ANIM_WALK, TRUE);
            break;
        }

        case ENEMY_STATE_CHASE: {
            if (dist > ENEMY_DEAGGRO_RANGE) {
                newState = ENEMY_STATE_PATROL;
                break;
            }

            // ¿En rango de ataque? Solo si ya cumplió su cooldown personal y
            // hay "cupo" de atacantes (máximo ENEMY_MAX_ATTACKERS a la vez).
            if (dist < ENEMY_ATTACK_RANGE && absS16(dy) <= ENEMY_ATTACK_TOL_Y &&
                e->attackCooldown == 0 && enemiesAttacking < ENEMY_MAX_ATTACKERS) {
                newState = ENEMY_STATE_ATTACK;
                enemiesAttacking++;
                // Mirar al objetivo antes de golpear
                if (dx != 0) e->dir = (dx > 0) ? 1 : -1;
                // Elegir ataque según distancia: MUY pegado -> uppercut (corto);
                // a media distancia -> patada (se desplaza) o directo (más alcance).
                if (dist < ENEMY_UPPERCUT_RANGE)
                    e->attackType = ENEMY_ATTACK_PUNCH;                       // uppercut
                else
                    e->attackType = (random() & 1) ? ENEMY_ATTACK_KICK        // patada
                                                   : ENEMY_ATTACK_FRONT;      // directo
                e->attackHit  = 0;   // este swing todavía no conectó
                e->timer = (e->attackType == ENEMY_ATTACK_KICK) ? ENEMY_KICK_TIME
                                                                : ENEMY_PUNCH_TIME;
                break;
            }

            // --- Movimiento horizontal ---
            // Sin cooldown: acercarse hasta ENEMY_STOP_RANGE (sin empujar).
            // Con cooldown y demasiado cerca: RETROCEDER hasta el anillo de
            // espera (ENEMY_HOLD_RANGE) — el clásico "rodear" del beat-em-up.
            s16 moveX = 0;
            if (e->attackCooldown > 0 && dist < ENEMY_HOLD_RANGE) {
                if (dist < ENEMY_HOLD_RANGE - 8)
                    moveX = (dx > 0) ? -ENEMY_SPEED : ENEMY_SPEED;   // alejarse
                // dentro del anillo: se queda esperando
            } else if (dist > ENEMY_STOP_RANGE) {
                moveX = (dx > 0) ? ENEMY_SPEED : -ENEMY_SPEED;       // acercarse
            }
            if (moveX != 0) {
                e->x += moveX;
                e->x = clampS16(e->x, ENEMY_WORLD_MIN_X, enemyMaxX(e->y));
            }
            // Siempre MIRANDO al jugador, incluso mientras retrocede
            if (dx != 0) e->dir = (dx > 0) ? 1 : -1;

            // --- Movimiento vertical: alinearse con la profundidad del jugador ---
            s16 moveY = 0;
            if (dy > ENEMY_Y_ALIGN)       moveY =  ENEMY_SPEED;
            else if (dy < -ENEMY_Y_ALIGN) moveY = -ENEMY_SPEED;
            if (moveY != 0) {
                e->y = clampS16(e->y + moveY, ENEMY_LANE_TOP, ENEMY_LANE_BOTTOM);
            }

            // --- Animación según el desplazamiento dominante ---
            // Walk_up SOLO cuando sube y el componente vertical domina;
            // izquierda / derecha / abajo usan la caminata normal.
            if (moveX == 0 && moveY == 0) {
                enemySetAnim(e, ENEMY_ANIM_IDLE, TRUE);
            } else if (moveY < 0 && (moveX == 0 || absS16(dy) >= dist)) {
                enemySetAnim(e, ENEMY_ANIM_WALK_UP, TRUE);
            } else {
                enemySetAnim(e, ENEMY_ANIM_WALK, TRUE);
            }
            break;
        }

        case ENEMY_STATE_ATTACK: {
            if (e->timer > 0) {
                e->timer--;
                // La patada con salto avanza en X durante el lunge inicial
                if (e->attackType == ENEMY_ATTACK_KICK &&
                    e->timer > (ENEMY_KICK_TIME - ENEMY_KICK_LUNGE)) {
                    e->x += e->dir * ENEMY_KICK_SPEED;
                    e->x = clampS16(e->x, ENEMY_WORLD_MIN_X, enemyMaxX(e->y));
                }
            } else {
                // Fin del ataque: liberar el cupo de atacante y arrancar el
                // cooldown personal (con variación para que no sea metrónomo)
                leaveAttackState(e);
                e->attackCooldown = (u8)(ENEMY_ATTACK_COOLDOWN + (random() & 31));
                newState = ENEMY_STATE_CHASE;
            }
            break;
        }

        case ENEMY_STATE_HURT: {
            // Knockback. El feedback visual ahora es el flash de paleta
            // (reemplaza el parpadeo de visibilidad, que ocultaba el flash
            // la mitad de los frames y se notaba poco).
            e->x += (e->dir * -1) * 3;
            if (e->timer > 0) {
                e->timer--;
            } else {
                newState = ENEMY_STATE_CHASE;
            }
            break;
        }

        default: break;
    }

    if (newState != e->state) {
        e->state = newState;
        if (newState == ENEMY_STATE_ATTACK) {
            // Ataques sin loop: la anim corre una vez y queda en el último
            // frame hasta que el timer devuelve al CHASE (que restaura WALK).
            u8 atkAnim = (e->attackType == ENEMY_ATTACK_KICK)  ? ENEMY_ANIM_KICK
                       : (e->attackType == ENEMY_ATTACK_FRONT) ? ENEMY_ANIM_PUNCH_FRONT
                                                               : ENEMY_ANIM_PUNCH;
            enemyRestartAnim(e, atkAnim, FALSE);
        }
        // PATROL y CHASE eligen su anim frame a frame dentro del switch.
    }

    // El arte del spritesheet mira a la DERECHA: flip cuando mira a la izquierda.
    // (Antes el flip seguía siempre al jugador; ahora sigue a la dirección real
    // del enemigo, así en patrulla mira hacia donde camina.)
    SPR_setHFlip(e->sprite, (e->dir < 0));

    SPR_setPosition(e->sprite, e->x - e->cameraOffsetX, e->y - ENEMY_FOOT_OFFSET);
    SPR_setDepth(e->sprite, -(e->y));
}

// ---------------------------------------------------------------------------
// SEPARACIÓN DE GRUPO — que los foot soldiers no se apilen entre ellos
// ---------------------------------------------------------------------------
// Empuje pareado O(n²) con n≤8: si dos enemigos "caminantes" (PATROL/CHASE)
// están encimados, se empujan 1px por frame en X (y 1px en Y si también
// están pegados en profundidad). No se toca a los que están atacando (el
// lunge del kick es intencional), heridos (knockback propio) ni muertos.
// ---------------------------------------------------------------------------
static bool enemyIsSeparable(const Enemy* e) {
    return (e->state == ENEMY_STATE_PATROL || e->state == ENEMY_STATE_CHASE);
}

void separateEnemies(Enemy* list, u16 count) {
    for (u16 i = 0; i < count; i++) {
        if (!enemyIsSeparable(&list[i])) continue;
        for (u16 j = i + 1; j < count; j++) {
            if (!enemyIsSeparable(&list[j])) continue;

            s16 dx = list[j].x - list[i].x;
            s16 dy = list[j].y - list[i].y;
            if (absS16(dx) >= ENEMY_SEPARATE_X || absS16(dy) >= ENEMY_SEPARATE_Y)
                continue;

            // Empuje horizontal: cada uno 1px hacia lados opuestos.
            // Si están EXACTAMENTE en la misma X, desempata por índice.
            s16 push = (dx > 0 || (dx == 0 && (i & 1))) ? 1 : -1;
            list[i].x = clampS16(list[i].x - push, ENEMY_WORLD_MIN_X, enemyMaxX(list[i].y));
            list[j].x = clampS16(list[j].x + push, ENEMY_WORLD_MIN_X, enemyMaxX(list[j].y));

            // Empuje vertical suave solo si están casi en la misma lane
            if (dy != 0) {
                s16 pushY = (dy > 0) ? 1 : -1;
                list[i].y = clampS16(list[i].y - pushY, ENEMY_LANE_TOP, ENEMY_LANE_BOTTOM);
                list[j].y = clampS16(list[j].y + pushY, ENEMY_LANE_TOP, ENEMY_LANE_BOTTOM);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// HITBOX DE ATAQUE — foot soldier → jugador
// ---------------------------------------------------------------------------
// El golpe conecta solo durante la ventana ACTIVA de cada ataque (no en el
// windup ni en la recuperación), una sola vez por swing (attackHit).
// Jugador y enemigo comparten la grilla de 104px, así que el centro de ambos
// es frame_x + 52; el alcance se mide centro a centro.
// ---------------------------------------------------------------------------
bool enemyTryHitPlayer(Enemy* e, s16 px, s16 py) {
    if (e->state != ENEMY_STATE_ATTACK || e->attackHit || !e->sprite)
        return FALSE;

    // ¿Hitbox activa en este frame del ataque? y ¿con qué alcance?
    bool active;
    s16  reach;
    if (e->attackType == ENEMY_ATTACK_KICK) {
        active = (e->timer > ENEMY_KICK_TIME - ENEMY_KICK_LUNGE);   // todo el lunge
        reach  = ENEMY_HIT_RANGE_X;
    } else {
        active = (e->timer >= ENEMY_PUNCH_HIT_START && e->timer <= ENEMY_PUNCH_HIT_END);
        // directo = más alcance; uppercut = corto (sólo si está muy pegado).
        reach  = (e->attackType == ENEMY_ATTACK_FRONT) ? ENEMY_FRONT_REACH
                                                       : ENEMY_UPPERCUT_REACH;
    }
    if (!active)
        return FALSE;

    // Alcance horizontal HACIA ADELANTE (según e->dir), con una pequeña
    // tolerancia hacia atrás por si están encimados.
    s16 ex  = getEnemyCenterX(e);
    s16 pcx = px + ENEMY_SPRITE_W / 2;   // misma grilla de frame que el enemigo
    s16 dx  = (e->dir >= 0) ? (pcx - ex) : (ex - pcx);
    if (dx < -ENEMY_HIT_BACK_X || dx > reach)
        return FALSE;

    // Alineación en profundidad (pies)
    if (absS16(py - e->y) > ENEMY_HIT_TOL_Y)
        return FALSE;

    e->attackHit = 1;
    return TRUE;
}
