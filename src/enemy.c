#include "enemy.h"

// Ancho visible de la MegaDrive en píxeles. Se redefine localmente (igual
// que en scenes.c/intro_arcade.c) para no acoplar enemy.c a un header de
// escena sólo por esta constante de hardware.
#define ENEMY_SCREEN_W 320

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
// MAPEO DE ANIMACIONES — el foot soldier naranja tiene un orden DISTINTO
// de animaciones en su spritesheet. Estos helpers devuelven el índice
// correcto según el tipo de enemigo.
// ---------------------------------------------------------------------------
static u8 enemyAnimIdle(const Enemy* e) {
    switch (e->type) {
        case ENEMY_TYPE_FOOT_SOLDIER_ORANGE: return ORANGE_ANIM_IDLE;
        case ENEMY_TYPE_FOOT_SOLDIER_WHITE:  return WHITE_ANIM_IDLE;
        default:                             return ENEMY_ANIM_IDLE;
    }
}
static u8 enemyAnimWalk(const Enemy* e) {
    switch (e->type) {
        case ENEMY_TYPE_FOOT_SOLDIER_ORANGE: return ORANGE_ANIM_WALK;
        case ENEMY_TYPE_FOOT_SOLDIER_WHITE:  return WHITE_ANIM_WALK;
        default:                             return ENEMY_ANIM_WALK;
    }
}
static u8 enemyAnimWalkUp(const Enemy* e) {
    switch (e->type) {
        case ENEMY_TYPE_FOOT_SOLDIER_ORANGE: return ORANGE_ANIM_WALK_UP;
        case ENEMY_TYPE_FOOT_SOLDIER_WHITE:  return WHITE_ANIM_WALK_UP;
        default:                             return ENEMY_ANIM_WALK_UP;
    }
}
static u8 enemyAnimKick(const Enemy* e) {
    switch (e->type) {
        case ENEMY_TYPE_FOOT_SOLDIER_ORANGE: return ORANGE_ANIM_KICK;
        case ENEMY_TYPE_FOOT_SOLDIER_WHITE:  return WHITE_ANIM_JUMP;
        default:                             return ENEMY_ANIM_KICK;
    }
}
static u8 enemyAnimPunchFront(const Enemy* e) {
    switch (e->type) {
        case ENEMY_TYPE_FOOT_SOLDIER_ORANGE: return ORANGE_ANIM_PUNCH_FRONT;
        case ENEMY_TYPE_FOOT_SOLDIER_WHITE:  return WHITE_ANIM_SLASH_LONG;
        default:                             return ENEMY_ANIM_PUNCH_FRONT;
    }
}
static u8 enemyAnimUppercut(const Enemy* e) {
    switch (e->type) {
        case ENEMY_TYPE_FOOT_SOLDIER_ORANGE: return ORANGE_ANIM_UPPERCUT;
        case ENEMY_TYPE_FOOT_SOLDIER_WHITE:  return WHITE_ANIM_SLASH_MID;
        default:                             return ENEMY_ANIM_PUNCH;
    }
}
static u8 enemyAnimExplode(const Enemy* e) {
    switch (e->type) {
        case ENEMY_TYPE_FOOT_SOLDIER_ORANGE: return ORANGE_ANIM_EXPLODE;
        case ENEMY_TYPE_FOOT_SOLDIER_WHITE:  return WHITE_ANIM_EXPLODE;
        default:                             return ENEMY_ANIM_EXPLODE;
    }
}
static u8 enemyAnimHit(Enemy* e) {
    if (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) return ORANGE_ANIM_HIT;
    if (e->type == ENEMY_TYPE_FOOT_SOLDIER_WHITE)  return WHITE_ANIM_HIT;
    u8 hitAnim = (u8)(ENEMY_ANIM_HIT_1 + e->hitToggle);
    if (++e->hitToggle >= 3) e->hitToggle = 0;
    return hitAnim;
}

// Devuelve la duración del ataque actual según el tipo de enemigo.
static u16 enemyAttackTime(const Enemy* e) {
    if (e->type == ENEMY_TYPE_FOOT_SOLDIER_WHITE) {
        // El salto NO se mide por timer: dura lo que dure el arco (ver el
        // camino de ENEMY_ATTACK_JUMP en ENEMY_STATE_ATTACK). Se devuelve un
        // tope de seguridad por si algo lo dejara colgado en el aire.
        if (e->attackType == ENEMY_ATTACK_JUMP)
            return WHITE_JUMP_SAFETY;
        return WHITE_SLASH_TIME;
    }
    if (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) {
        switch (e->attackType) {
            case ENEMY_ATTACK_KICK:    return ORANGE_KICK_TIME;
            case ENEMY_ATTACK_SHURIKEN: return ORANGE_SHURIKEN_TIME;
            case ENEMY_ATTACK_FRONT:   return ORANGE_PUNCH_TIME;
            default:                   return ORANGE_UPPERCUT_TIME;  // PUNCH
        }
    }
    return (e->attackType == ENEMY_ATTACK_KICK) ? ENEMY_KICK_TIME
                                                : ENEMY_PUNCH_TIME;
}

// Devuelve la duración de la explosión de muerte según el tipo.
static u16 enemyExplodeTime(const Enemy* e) {
    switch (e->type) {
        case ENEMY_TYPE_FOOT_SOLDIER_ORANGE: return ORANGE_EXPLODE_TIME;
        case ENEMY_TYPE_FOOT_SOLDIER_WHITE:  return WHITE_EXPLODE_TIME;
        default:                             return ENEMY_EXPLODE_TIME;
    }
}

// Devuelve el rango del hitbox del ataque actual según el tipo.
static s16 enemyAttackReach(const Enemy* e) {
    if (e->attackType == ENEMY_ATTACK_KICK) return ENEMY_HIT_RANGE_X;
    if (e->attackType == ENEMY_ATTACK_FRONT) return ENEMY_FRONT_REACH;
    if (e->attackType == ENEMY_ATTACK_SHURIKEN) return 0;  // no hitbox melee
    // UPPERCUT
    return ENEMY_UPPERCUT_REACH;
}

// ---------------------------------------------------------------------------
// COMBOS DEL FOOT SOLDIER MORADO (fiel al arcade)
// ---------------------------------------------------------------------------
// En el arcade el foot soldier lanza cadenas de 2-3 golpes (ATTACK S0 = puños,
// S1/S2 = variantes de patada): cada golpe es un PASO con su propia animación,
// duración y ventana de hitbox. El morado entra a ENEMY_STATE_ATTACK con un
// combo de N pasos; arranca en el paso 0 y al expirar su timer avanza al
// siguiente (reseteando attackHit para que cada golpe conecte una vez). El
// timer del estado es el del paso actual; los i-frames del jugador (45) hacen
// que una cadena completa rara vez conecte entera — como el knockback del
// arcade, el golpe 1 suele sacarte del alcance de los siguientes.
typedef struct {
    u8   anim;      // Índice de animación (ENEMY_ANIM_* del morado)
    u16  time;      // Duración total del paso (ticks, calzada a FAST 8 del sheet)
    u16  hitStart;  // Timer mínimo (inclusive) con hitbox activa
    u16  hitEnd;    // Timer máximo (inclusive) con hitbox activa
    s16  reach;     // Alcance del golpe (centro a centro)
    u16  lunge;     // Frames iniciales con desplazamiento en X (patada; 0 = fijo)
} ComboStep;

// Combo de puño (arcade ATTACK S0): doble directo + uppercut final.
static const ComboStep purpleComboPunch[] = {
    { ENEMY_ANIM_PUNCH_FRONT, 16, ENEMY_PUNCH_HIT_START, ENEMY_PUNCH_HIT_END, ENEMY_FRONT_REACH,    0 },
    { ENEMY_ANIM_PUNCH_FRONT, 16, ENEMY_PUNCH_HIT_START, ENEMY_PUNCH_HIT_END, ENEMY_FRONT_REACH,    0 },
    { ENEMY_ANIM_PUNCH,       16, ENEMY_PUNCH_HIT_START, ENEMY_PUNCH_HIT_END, ENEMY_UPPERCUT_REACH, 0 }
};
// Combo de patada (arcade ATTACK S1/S2): directo + patada con salto (lunge).
static const ComboStep purpleComboKick[] = {
    { ENEMY_ANIM_PUNCH_FRONT, 16, ENEMY_PUNCH_HIT_START, ENEMY_PUNCH_HIT_END, ENEMY_FRONT_REACH, 0 },
    { ENEMY_ANIM_KICK, ENEMY_KICK_TIME,
      (u16)(ENEMY_KICK_TIME - ENEMY_KICK_LUNGE + 1), ENEMY_KICK_TIME,
      ENEMY_HIT_RANGE_X, ENEMY_KICK_LUNGE }
};
// Doble directo (variante corta, arcade ATTACK S0 sin uppercut).
static const ComboStep purpleComboFront[] = {
    { ENEMY_ANIM_PUNCH_FRONT, 16, ENEMY_PUNCH_HIT_START, ENEMY_PUNCH_HIT_END, ENEMY_FRONT_REACH, 0 },
    { ENEMY_ANIM_PUNCH_FRONT, 16, ENEMY_PUNCH_HIT_START, ENEMY_PUNCH_HIT_END, ENEMY_FRONT_REACH, 0 }
};

// ---------------------------------------------------------------------------
// COMBOS DEL FOOT SOLDIER BLANCO (espada larga)
// ---------------------------------------------------------------------------
// Mismo motor de pasos que el morado, pero con espada: no tiene punos ni
// patadas, tiene tres cortes (anims 3/4/5) y un salto con espadazo al caer
// (anims 6/7, que va por su propio camino porque necesita el arco vertical).
//
// FRONT = la ESTOCADA LARGA sola (anim 3). Es el ataque que define al enemigo:
// llega a 50px, mas que cualquier cosa del morado, y por eso no encadena --
// pega desde lejos y se recompone.
static const ComboStep whiteComboFront[] = {
    { WHITE_ANIM_SLASH_LONG, WHITE_SLASH_TIME,
      WHITE_LONG_HIT_START, WHITE_LONG_HIT_END, WHITE_SLASH_LONG_REACH, 0 }
};
// PUNCH = pegado al jugador: los dos cortes medios encadenados (4 y despues 5),
// que es como se ve en el arcade cuando te tiene encima.
static const ComboStep whiteComboPunch[] = {
    { WHITE_ANIM_SLASH_MID,  WHITE_SLASH_TIME,
      WHITE_MID_HIT_START, WHITE_MID_HIT_END, WHITE_SLASH_MID_REACH,  0 },
    { WHITE_ANIM_SLASH_MID2, WHITE_SLASH_TIME,
      WHITE_MID_HIT_START, WHITE_MID_HIT_END, WHITE_SLASH_MID2_REACH, 0 }
};

// Devuelve la tabla del combo según el tipo de enemigo y de ataque. El naranja
// no usa combos → comboLen queda en 0 y va por el camino simple.
static const ComboStep* comboStepsFor(u8 type, u8 attackType) {
    if (type == ENEMY_TYPE_FOOT_SOLDIER_WHITE) {
        return (attackType == ENEMY_ATTACK_PUNCH) ? whiteComboPunch
                                                  : whiteComboFront;
    }
    switch (attackType) {
        case ENEMY_ATTACK_KICK:  return purpleComboKick;
        case ENEMY_ATTACK_PUNCH: return purpleComboPunch;
        default:                 return purpleComboFront;   // FRONT
    }
}

static u8 comboLengthFor(u8 type, u8 attackType) {
    if (type == ENEMY_TYPE_FOOT_SOLDIER_WHITE) {
        // El salto va por fuera del motor de combos (necesita el arco en Z).
        if (attackType == ENEMY_ATTACK_JUMP) return 0;
        return (attackType == ENEMY_ATTACK_PUNCH)
               ? (u8)(sizeof(whiteComboPunch) / sizeof(ComboStep))
               : (u8)(sizeof(whiteComboFront) / sizeof(ComboStep));
    }
    switch (attackType) {
        case ENEMY_ATTACK_KICK:  return (u8)(sizeof(purpleComboKick)  / sizeof(ComboStep));
        case ENEMY_ATTACK_PUNCH: return (u8)(sizeof(purpleComboPunch) / sizeof(ComboStep));
        default:                 return (u8)(sizeof(purpleComboFront) / sizeof(ComboStep));
    }
}

// ---------------------------------------------------------------------------
// UTILIDADES
// ---------------------------------------------------------------------------
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

static s16 enemyMaxX(const Enemy* e) {
    s16 levelMax = e->levelMaxX - e->w;
    if (!e->wallXTop) return levelMax;        // nivel sin pared diagonal
    s16 laneRange = e->laneBottom - e->laneTop;
    if (laneRange <= 0) return levelMax;
    s32 wallRange = e->wallXBottom - e->wallXTop;
    s16 wallX     = e->wallXTop + (s16)(wallRange * (e->y - e->laneTop) / laneRange);
    s16 wallMax   = wallX - e->w;
    return (wallMax < levelMax) ? wallMax : levelMax;
}

// Cota izquierda de movimiento por tipo. El morado conserva la negativa
// (-w) para poder nacer con la voltereta por la espalda desde fuera de
// pantalla; el naranja (kiter) NUNCA retrocede a la izquierda del borde
// visible de la cámara: si se retirara hasta X negativa quedaría fuera
// del nivel, inalcanzable para el jugador (que no pasa del borde de la
// cámara) e imposible de matar.
static s16 enemyMinX(const Enemy* e) {
    if (e->state == ENEMY_STATE_SPAWNING) return -(s16)e->w;
    if (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) return e->cameraOffsetX;
    return -(s16)e->w;
}

// ---------------------------------------------------------------------------
// Salto del foot soldier BLANCO — un paso de física (29/09: punto fijo Q8)
// ---------------------------------------------------------------------------
// SUBIDA: la gravedad de las tortugas (apice de 107 px). Al entrar en la zona
// del apice (vel < WHITE_JUMP_BAND_Q) pasa a la gravedad de la caida lenta
// (14/09, pedido de Gustavo): el espadazo aereo se ve planeado, no plomada.
// El avance en X baja a WHITE_FALL_SPEED_X mientras cae (ver whiteJumpSpeedX).
static void whiteJumpStart(Enemy* e) {
    e->jumpVel  = WHITE_JUMP_V0_Q;
    e->jumpZq   = 0;
    e->jumpZ    = 0;
    e->gravTick = 0;
}

static void whiteJumpStep(Enemy* e) {
    e->jumpZq += e->jumpVel;
    e->jumpZ   = (s16)(e->jumpZq >> 8);
    e->jumpVel -= (e->jumpVel >= WHITE_JUMP_BAND_Q) ? WHITE_JUMP_GRAV_Q
                                                    : WHITE_FALL_GRAV_Q;
}

static void whiteJumpStop(Enemy* e) {
    e->jumpZ    = 0;
    e->jumpZq   = 0;
    e->jumpVel  = 0;
    e->gravTick = 0;
}

// px/frame de avance horizontal segun la fase del salto (ver arriba).
static s16 whiteJumpSpeedX(const Enemy* e) {
    return (e->jumpVel > 0) ? WHITE_JUMP_SPEED : WHITE_FALL_SPEED_X;
}

// ---------------------------------------------------------------------------
// IA DE GRUPO — atacantes simultáneos
// ---------------------------------------------------------------------------
static u8 enemiesAttacking = 0;

// --- Reparto de targets en 2 jugadores ---
// (14/09) Hasta 4 jugadores: el modo secreto de 4 tortugas reparte los
// enemigos entre los cuatro igual que antes entre dos.
static u8 enemyNumPlayers = 1;
static u8 enemyTargetCount[ENEMY_MAX_TARGETS] = {0, 0, 0, 0};

void resetEnemyAI(u8 numPlayers) {
    enemiesAttacking = 0;
    if (numPlayers < 1) numPlayers = 1;
    if (numPlayers > ENEMY_MAX_TARGETS) numPlayers = ENEMY_MAX_TARGETS;
    enemyNumPlayers = numPlayers;
    for (u8 i = 0; i < ENEMY_MAX_TARGETS; i++) enemyTargetCount[i] = 0;
}

static void releaseTarget(u8 target) {
    if (enemyTargetCount[target] > 0)
        enemyTargetCount[target]--;
}

static void leaveAttackState(Enemy* e) {
    // El shuriken es a distancia: no ocupa cupo de atacante melee, así que
    // tampoco lo libera (nunca lo incrementó — ver el trigger de ataque).
    if (e->state == ENEMY_STATE_ATTACK &&
        e->attackType != ENEMY_ATTACK_SHURIKEN && enemiesAttacking > 0)
        enemiesAttacking--;
}

// ---------------------------------------------------------------------------
// SISTEMA DE SHURIKENS — proyectiles del foot soldier naranja
// ---------------------------------------------------------------------------
static Shuriken shurikens[MAX_SHURIKENS];

void shurikenInit(void) {
    for (u16 i = 0; i < MAX_SHURIKENS; i++) {
        shurikens[i].active = 0;
        shurikens[i].sprite = NULL;
    }
}

void shurikenSpawn(s16 x, s16 y, s8 dir, u8 palette) {
    for (u16 i = 0; i < MAX_SHURIKENS; i++) {
        if (shurikens[i].active) continue;
        shurikens[i].x = x;
        shurikens[i].y = y;
        shurikens[i].dir = dir;
        shurikens[i].cameraOffsetX = 0;
        shurikens[i].active = 1;
        shurikens[i].sprite = SPR_addSprite(&shuriken_sprite,
                                            x, y - ENEMY_FOOT_OFFSET_ORANGE + 40,
                                            TILE_ATTR(palette, FALSE, FALSE, FALSE));
        if (shurikens[i].sprite) {
            SPR_setDepth(shurikens[i].sprite, -(y) - 1);
            SPR_setHFlip(shurikens[i].sprite, (dir < 0));
        }
        return;   // slot encontrado
    }
}

void shurikenUpdate(s16 camX) {
    for (u16 i = 0; i < MAX_SHURIKENS; i++) {
        if (!shurikens[i].active) continue;
        shurikens[i].cameraOffsetX = camX;
        shurikens[i].x += shurikens[i].dir * ORANGE_SHURIKEN_SPEED;

        // Fuera de pantalla (con margen de 32px a cada lado)
        if (shurikens[i].x < camX - 32 || shurikens[i].x > camX + 320 + 32) {
            if (shurikens[i].sprite) SPR_releaseSprite(shurikens[i].sprite);
            shurikens[i].sprite = NULL;
            shurikens[i].active = 0;
            continue;
        }
        if (shurikens[i].sprite)
            SPR_setPosition(shurikens[i].sprite,
                            shurikens[i].x - shurikens[i].cameraOffsetX,
                            shurikens[i].y - ENEMY_FOOT_OFFSET_ORANGE + 40);
    }
}

void shurikenReleaseAll(void) {
    for (u16 i = 0; i < MAX_SHURIKENS; i++) {
        if (shurikens[i].sprite) SPR_releaseSprite(shurikens[i].sprite);
        shurikens[i].sprite = NULL;
        shurikens[i].active = 0;
    }
}

bool shurikenCheckHitPlayer(s16 px, s16 py, s16* hitX) {
    // px = borde izquierdo del frame del jugador (frame de 104px)
    s16 pcx = px + PLAYER_SPRITE_W / 2;   // centro del jugador
    s16 pcy = py;                         // pies del jugador

    for (u16 i = 0; i < MAX_SHURIKENS; i++) {
        if (!shurikens[i].active) continue;

        s16 scx = shurikens[i].x + 8;   // centro del shuriken (16px wide → +8)
        s16 scy = shurikens[i].y;

        if (absS16(pcx - scx) < 16 && absS16(pcy - scy) < 16) {
            // Impacto: destruir el shuriken
            if (hitX) *hitX = scx;
            if (shurikens[i].sprite) SPR_releaseSprite(shurikens[i].sprite);
            shurikens[i].sprite = NULL;
            shurikens[i].active = 0;
            return TRUE;
        }
    }
    return FALSE;
}

bool shurikenBreakByPlayerAttack(const Player* p) {
    // La hitbox del ataque CUERPO A CUERPO de la tortuga (MISMA geometría que
    // contra los enemigos: playerAttackHits) rompe los shurikens que cruza: el
    // proyectil desaparece SIN dañar al jugador. Devuelve TRUE si rompió
    // alguno (para tocar un SFX). Se llama por jugador, ANTES de
    // shurikenCheckHitPlayer: un shuriken roto este frame ya no pega.
    // (23/09) SOLO cuerpo a cuerpo: la patada voladora NO rompe shurikens.
    // El jugador no puede limpiar la pantalla saltando; para cortar el tiro
    // hay que estar en el piso y golpear.
    if (isPlayerJumpKicking(p)) return FALSE;

    bool broke = FALSE;
    for (u16 i = 0; i < MAX_SHURIKENS; i++) {
        if (!shurikens[i].active) continue;
        s16 scx = shurikens[i].x + 8;   // centro del shuriken (16px wide → +8)
        s16 scy = shurikens[i].y;       // lane (pies) del lanzador = la del shuriken
        if (playerAttackHits(p, scx, scy)) {
            if (shurikens[i].sprite) SPR_releaseSprite(shurikens[i].sprite);
            shurikens[i].sprite = NULL;
            shurikens[i].active = 0;
            broke = TRUE;
        }
    }
    return broke;
}


// ---------------------------------------------------------------------------
// DINAMITA del foot soldier MORADO (18/09)
// ---------------------------------------------------------------------------
// Ataque GUIONADO: lo tira UNA vez el morado que se asoma por la escalera del
// 1-1 y cae SIEMPRE en el mismo punto. Por eso no hay pool -- hay un cartucho
// y una explosion -- y el vuelo no persigue a nadie: es una parabola fija
// entre la mano del que tira y el punto de caida.
//
// Convencion de coordenadas, la misma que el salto del jugador:
//   x  = X de mundo del CENTRO del cartucho
//   y  = LANE (profundidad). Interpola de la lane del que tira a la del punto
//        de caida, asi el sprite pasa por delante/detras segun corresponde.
//   z  = altura VISUAL sobre el piso. Solo se resta al dibujar.
//
// La altura es una recta de z0 a 0 MAS una parabola de TNT_ARC_APEX px, que es
// la misma cuenta del arco de la cinematica: 0 en las dos puntas y maxima en
// el medio.
// ---------------------------------------------------------------------------
typedef enum { TNT_OFF, TNT_FLYING, TNT_BLAST } TntState;

static struct {
    TntState state;
    Sprite*  sprite;
    s16      x0, y0, z0;     // salida (mano del que tira)
    s16      x1, y1;         // caida (punto fijo)
    s16      x,  y,  z;      // posicion actual
    u16      t;              // frames transcurridos
    u8       palette;
} tnt;

void tntInit(void) {
    tnt.state  = TNT_OFF;
    tnt.sprite = NULL;
    tnt.t      = 0;
}

void tntLaunch(s16 x, s16 y, s8 dir, s16 landX, s16 landY, u8 palette) {
    // Ya hay uno en el aire o explotando: no se encima otro (no deberia pasar,
    // el ataque es de una sola vez).
    if (tnt.state != TNT_OFF) return;

    // La mano esta en TNT_HAND_DX dentro del frame de 64px mirando a la
    // derecha; espejada cuando mira a la izquierda.
    s16 handDx = (dir >= 0) ? TNT_HAND_DX : (s16)(ENEMY_SPRITE_W_PURPLE - TNT_HAND_DX);

    tnt.x0 = (s16)(x + handDx);
    tnt.y0 = y;
    tnt.z0 = TNT_HAND_Z;
    tnt.x1 = landX;
    tnt.y1 = landY;
    tnt.x  = tnt.x0;
    tnt.y  = tnt.y0;
    tnt.z  = tnt.z0;
    tnt.t  = 0;
    tnt.palette = palette;

    // PRIORIDAD ALTA: el fuego del primer plano del 1-1 es un plano de alta
    // prioridad y se comia media explosion. El cartucho y el estallido van por
    // encima de todo el fondo, como en el arcade.
    tnt.sprite = SPR_addSprite(&tnt_sprite, 0, 0,
                               TILE_ATTR(palette, TRUE, FALSE, FALSE));
    // Sin sprite (VRAM llena) el cartucho igual VUELA y explota: el daño no
    // depende del sprite. Se ve raro pero no rompe la escena, que es la regla
    // que ya seguimos con los robots del final.
    tnt.state = TNT_FLYING;
}

// Dibuja el cartucho o la explosion en pantalla segun la camara.
// camY es 0 en el 1-1 (no hay scroll vertical) y el scroll real en el 2-1.
static void tntDraw(s16 camX, s16 camY) {
    if (!tnt.sprite) return;
    if (tnt.state == TNT_FLYING) {
        SPR_setPosition(tnt.sprite,
                        (s16)(tnt.x - TNT_W / 2 - camX),
                        (s16)(tnt.y - tnt.z - TNT_W / 2 - camY));
        // Profundidad por lane, igual que todo el resto de la escena.
        SPR_setDepth(tnt.sprite, (s16)(-(tnt.y) - 1));
    } else {
        SPR_setPosition(tnt.sprite,
                        (s16)(tnt.x1 - TNT_EXPLOSION_W / 2 - camX),
                        (s16)(tnt.y1 - TNT_EXPLOSION_W / 2 - camY));
        SPR_setDepth(tnt.sprite, (s16)(-(tnt.y1) - 1));
    }
}

bool tntUpdate(s16 camX) { return tntUpdateEx(camX, 0); }

bool tntUpdateEx(s16 camX, s16 camY) {
    if (tnt.state == TNT_OFF) return FALSE;

    if (tnt.state == TNT_FLYING) {
        tnt.t++;
        u16 T = TNT_FLIGHT_FRAMES;
        u16 t = (tnt.t > T) ? T : tnt.t;

        tnt.x = (s16)(tnt.x0 + ((s32)(tnt.x1 - tnt.x0) * t) / T);
        tnt.y = (s16)(tnt.y0 + ((s32)(tnt.y1 - tnt.y0) * t) / T);
        // Recta de z0 a 0 + parabola de TNT_ARC_APEX (0 en las puntas)
        s32 base = (s32)tnt.z0 - ((s32)tnt.z0 * t) / T;
        s32 arc  = ((s32)TNT_ARC_APEX * 4 * t * (T - t)) / ((s32)T * T);
        tnt.z = (s16)(base + arc);

        if (tnt.t >= T) {
            // Toco el piso: el cartucho se convierte en la explosion. Se
            // reusa el MISMO slot de sprite: se libera el del tnt (9 tiles) y
            // se pide el de la explosion (64), asi nunca conviven los dos.
            if (tnt.sprite) SPR_releaseSprite(tnt.sprite);
            tnt.sprite = SPR_addSprite(&explosion_sprite, 0, 0,
                                       TILE_ATTR(tnt.palette, TRUE, FALSE, FALSE));
            if (tnt.sprite) SPR_setAnimationLoop(tnt.sprite, FALSE);
            tnt.state = TNT_BLAST;
            tnt.t     = 0;
            tntDraw(camX, camY);
            return TRUE;          // el frame del impacto (la escena toca el SFX)
        }
        tntDraw(camX, camY);
        return FALSE;
    }

    // --- Explosion ---
    tnt.t++;
    if (tnt.t >= TNT_EXPLOSION_FRAMES) {
        if (tnt.sprite) SPR_releaseSprite(tnt.sprite);
        tnt.sprite = NULL;
        tnt.state  = TNT_OFF;
        return FALSE;
    }
    tntDraw(camX, camY);
    return FALSE;
}

bool tntBlastActive(void) {
    return (bool)(tnt.state == TNT_BLAST && tnt.t <= TNT_BLAST_DMG_FRAMES);
}

s16 tntBlastX(void) { return tnt.x1; }
s16 tntBlastY(void) { return tnt.y1; }

bool tntBlastHits(s16 px, s16 py, s16 halfW) {
    if (!tntBlastActive()) return FALSE;
    return (bool)(absS16(px - tnt.x1) <= (s16)(TNT_BLAST_RADIUS_X + halfW) &&
                  absS16(py - tnt.y1) <= TNT_BLAST_RADIUS_Y);
}

void tntReleaseAll(void) {
    if (tnt.sprite) SPR_releaseSprite(tnt.sprite);
    tnt.sprite = NULL;
    tnt.state  = TNT_OFF;
    tnt.t      = 0;
}

// ---------------------------------------------------------------------------
// TAPA VOLADORA de la alcantarilla (19/09)
// ---------------------------------------------------------------------------
// Mucho mas simple que el TNT: tiro RECTO por X, sin parabola ni punto de
// caida. Sale a la altura del pecho (LID_HAND_Z sobre los pies del que tira),
// mantiene la lane del que la tiro durante todo el vuelo y viaja hasta salir
// de camara, donde se libera el sprite.
//
// Por que no se consume al pegar: en el arcade la tapa sigue de largo y puede
// barrer a las dos tortugas. De que un mismo jugador no se coma dos golpes en
// frames seguidos se encarga la invencibilidad de playerCanBeHit(), que ya
// aplica damagePlayer -- no hace falta una mascara propia como en la explosion
// del TNT (esa SI la necesita porque el estallido dura 24 frames quieto).
// ---------------------------------------------------------------------------
static struct {
    u8      active;
    Sprite* sprite;
    s16     x, y, z;     // centro (mundo), lane, altura visual
    s8      dir;
    u8      friendly;    // 1 = devuelta de un golpe: ya no pega a las tortugas
    s8      owner;       // jugador que la devolvio (para el puntaje), -1 = nadie
    u8      hitMask;     // enemigos ya golpeados por ESTA tapa (bit por indice)
} lids[LID_MAX];

void lidInit(void) {
    for (u16 i = 0; i < LID_MAX; i++) {
        lids[i].active = 0;
        lids[i].sprite = NULL;
    }
}

static void lidRelease(u16 i) {
    if (lids[i].sprite) SPR_releaseSprite(lids[i].sprite);
    lids[i].sprite = NULL;
    lids[i].active = 0;
}

void lidLaunch(s16 x, s16 y, s8 dir, u8 palette) {
    for (u16 i = 0; i < LID_MAX; i++) {
        if (lids[i].active) continue;

        // Las manos estan en LID_HAND_DX dentro del frame de 64px mirando a la
        // derecha; espejadas cuando mira a la izquierda.
        s16 handDx = (dir >= 0) ? LID_HAND_DX
                                : (s16)(ENEMY_SPRITE_W_PURPLE - LID_HAND_DX);
        lids[i].x   = (s16)(x + handDx);
        lids[i].y   = y;
        lids[i].z   = LID_HAND_Z;
        lids[i].dir = (dir >= 0) ? 1 : -1;
        lids[i].friendly = 0;
        lids[i].owner    = -1;
        lids[i].hitMask  = 0;
        // Prioridad alta por la misma razon que el TNT: que ningun plano de
        // primer plano se la coma. En el 2-1 da igual, pero es gratis.
        lids[i].sprite = SPR_addSprite(&lid_sprite, 0, 0,
                                       TILE_ATTR(palette, TRUE, FALSE, FALSE));
        // Sin sprite (VRAM llena) la tapa igual VUELA y hace daño: la hitbox no
        // depende del sprite. Misma regla que el cartucho de dinamita.
        lids[i].active = 1;
        return;
    }
    // Pool lleno: no se tira. Con las bocas de tormenta separadas como estan no
    // deberia pasar nunca.
}

void lidUpdate(s16 camX, s16 camY) {
    for (u16 i = 0; i < LID_MAX; i++) {
        if (!lids[i].active) continue;

        lids[i].x += (s16)(lids[i].dir * LID_SPEED);

        // Fuera de camara -> se libera el sprite y el slot.
        s16 sx = (s16)(lids[i].x - camX);
        if (sx < -(LID_W / 2 + LID_MARGIN) ||
            sx >  (s16)(ENEMY_SCREEN_W + LID_W / 2 + LID_MARGIN)) {
            lidRelease(i);
            continue;
        }

        if (lids[i].sprite) {
            SPR_setPosition(lids[i].sprite,
                            (s16)(sx - LID_W / 2),
                            (s16)(lids[i].y - lids[i].z - LID_H / 2 - camY));
            // Profundidad por lane, como el resto de la escena.
            SPR_setDepth(lids[i].sprite, (s16)(-(lids[i].y) - 1));
        }
    }
}

bool lidHits(s16 px, s16 py, s16 halfW, s16* outX) {
    for (u16 i = 0; i < LID_MAX; i++) {
        if (!lids[i].active) continue;
        if (lids[i].friendly) continue;   // devuelta: ahora es de las tortugas
        if (absS16(px - lids[i].x) > (s16)(LID_HIT_HALF_W + halfW)) continue;
        if (absS16(py - lids[i].y) > LID_HIT_RADIUS_Y) continue;
        if (outX) *outX = lids[i].x;
        return TRUE;
    }
    return FALSE;
}

// (23/09) DEVOLVER LA TAPA de un golpe cuerpo a cuerpo.
// La patada voladora no la devuelve (misma regla que el shuriken). Solo se
// devuelve UNA vez: al invertirse queda 'friendly' y deja de pegarle a las
// tortugas, asi que dos golpes seguidos del mismo swing no la dejan temblando
// en el lugar. Se le da vuelta el sprite para que la elipse acompañe el viaje.
bool lidReflectByPlayerAttack(const Player* p, s8 playerIdx) {
    if (isPlayerJumpKicking(p)) return FALSE;

    bool any = FALSE;
    for (u16 i = 0; i < LID_MAX; i++) {
        if (!lids[i].active || lids[i].friendly) continue;
        if (!playerAttackHits(p, lids[i].x, lids[i].y)) continue;
        lids[i].dir      = (s8)(-lids[i].dir);
        lids[i].friendly = 1;
        lids[i].owner    = playerIdx;
        lids[i].hitMask  = 0;
        if (lids[i].sprite)
            SPR_setHFlip(lids[i].sprite, (bool)(lids[i].dir < 0));
        any = TRUE;
    }
    return any;
}

// TRUE si una tapa DEVUELTA barre a este enemigo. Cada tapa golpea a cada
// enemigo UNA sola vez (mascara por indice): sin eso, con 5 px por frame le
// sacaria vida en cada frame que lo cruza. Si owner no es NULL sale ahi el
// jugador que la devolvio, para sumarle el punto si el golpe mata.
bool lidHitsEnemy(u16 enemyIdx, s16 ex, s16 ey, s16 halfW, s8* owner) {
    u8 bit = (u8)(1 << (enemyIdx & 7));
    for (u16 i = 0; i < LID_MAX; i++) {
        if (!lids[i].active || !lids[i].friendly) continue;
        if (lids[i].hitMask & bit) continue;
        if (absS16(ex - lids[i].x) > (s16)(LID_HIT_HALF_W + halfW)) continue;
        if (absS16(ey - lids[i].y) > LID_HIT_RADIUS_Y) continue;
        lids[i].hitMask |= bit;
        if (owner) *owner = lids[i].owner;
        return TRUE;
    }
    return FALSE;
}

void lidReleaseAll(void) {
    for (u16 i = 0; i < LID_MAX; i++) lidRelease(i);
}

// ---------------------------------------------------------------------------
// SPAWN DE ENEMIGOS
// ---------------------------------------------------------------------------
void initEnemySpawn(Enemy* e, s16 spawnX, s16 y, s16 patrolRange, u8 palette, u8 type) {
    e->x           = spawnX;
    e->y           = y;
    e->patrolLeft  = spawnX - patrolRange;
    e->patrolRight = spawnX + patrolRange;
    e->cameraOffsetX = 0;
    // Limites del nivel 1 por defecto; los otros niveles los pisan con
    // setEnemyBounds() despues de initEnemySpawn().
    e->laneTop     = ENEMY_LANE_TOP;
    e->laneBottom  = ENEMY_LANE_BOTTOM;
    e->wallXTop    = ENEMY_END_WALL_X_TOP;
    e->wallXBottom = ENEMY_END_WALL_X_BOTTOM;
    e->levelMaxX   = 1376;
    e->state       = ENEMY_STATE_PATROL;
    e->dir         = -1;
    e->timer       = 0;
    e->hp          = (type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) ? ENEMY_HP_ORANGE
                   : (type == ENEMY_TYPE_FOOT_SOLDIER_WHITE)  ? ENEMY_HP_WHITE
                                                              : ENEMY_HP_PURPLE;
    e->invincible  = 0;
    e->palette     = palette;
    e->type        = type;
    e->anim        = 0xFF;   // sentinela: fuerza el primer enemySetAnim
    e->attackType  = ENEMY_ATTACK_PUNCH;
    e->attackHit   = 0;
    e->hitToggle   = 0;
    e->flankTimer  = 0;
    e->attackCooldown = (u8)(random() & 31);
    e->lastMoveDir = 0;
    e->turnTimer   = 0;
    e->somersault  = 0;
    e->tntThrow    = 0;
    e->lidThrow    = 0;
    e->tntLandX    = 0;
    e->tntLandY    = 0;
    e->grabTarget  = 0;
    e->grabbed     = NULL;
    e->grabTimer   = 0;
    e->stancePhase = 0;
    e->stanceToggle = 0;
    e->comboStep  = 0;
    e->comboLen   = 0;
    e->jumpZ      = 0;
    e->jumpZq     = 0;
    e->jumpVel    = 0;
    e->gravTick = 0;

    // Dimensiones de frame según el tipo (sheet morada 64x80 con los pies en
    // el borde; la naranja mantiene la grilla vieja 104x104).
    if (type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) {
        e->w          = ENEMY_SPRITE_W_ORANGE;
        e->h          = ENEMY_SPRITE_H_ORANGE;
        e->footOffset = ENEMY_FOOT_OFFSET_ORANGE;
    } else if (type == ENEMY_TYPE_FOOT_SOLDIER_WHITE) {
        e->w          = ENEMY_SPRITE_W_WHITE;
        e->h          = ENEMY_SPRITE_H_WHITE;
        e->footOffset = ENEMY_FOOT_OFFSET_WHITE;
    } else {
        e->w          = ENEMY_SPRITE_W_PURPLE;
        e->h          = ENEMY_SPRITE_H_PURPLE;
        e->footOffset = ENEMY_FOOT_OFFSET_PURPLE;
    }

    // Se elige el jugador con MENOS enemigos encima; si hay empate, al azar
    // entre los empatados (generalizado a 1..4 el 14/09).
    if (enemyNumPlayers > 1) {
        u8 best = 0;
        for (u8 i = 1; i < enemyNumPlayers; i++)
            if (enemyTargetCount[i] < enemyTargetCount[best]) best = i;
        u8 tied[ENEMY_MAX_TARGETS]; u8 nTied = 0;
        for (u8 i = 0; i < enemyNumPlayers; i++)
            if (enemyTargetCount[i] == enemyTargetCount[best]) tied[nTied++] = i;
        e->target = tied[(u8)(random() % nTied)];
    } else {
        e->target = 0;
    }
    enemyTargetCount[e->target]++;
    e->retargetTimer = ENEMY_RETARGET_INTERVAL;

    // Elegir spritesheet y paleta según el tipo
    const SpriteDefinition* sheetDef = &foot_soldier;
    if (type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) {
        sheetDef = &foot_soldier_orange;
    } else if (type == ENEMY_TYPE_FOOT_SOLDIER_WHITE) {
        sheetDef = &foot_soldier_white;
    }
    e->sprite = SPR_addSprite(sheetDef, e->x, e->y,
                              TILE_ATTR(palette, FALSE, FALSE, FALSE));

    // Cargar paleta desde el PNG (que ya tiene blanco en índice 1 para el HUD).
    PAL_setPalette(palette, sheetDef->palette->data, DMA);

    enemySetAnim(e, enemyAnimWalk(e), TRUE);
}

void initEnemyDoorSpawn(Enemy* e, s16 doorCenterX, u8 palette) {
    initEnemySpawn(e, doorCenterX - ENEMY_SPRITE_W_PURPLE / 2, ENEMY_LANE_TOP, 60, palette,
                   ENEMY_TYPE_FOOT_SOLDIER);
    e->state = ENEMY_STATE_SPAWNING;
    e->timer = ENEMY_BREAK_DOOR_TIME;
    e->dir   = 1;

    e->anim = ENEMY_ANIM_BREAK_DOOR;
    SPR_setAnimAndFrame(e->sprite, ENEMY_ANIM_BREAK_DOOR, 1);
    SPR_setAnimationLoop(e->sprite, FALSE);
}

void initEnemyElevatorSpawn(Enemy* e, s16 doorCenterX, u8 palette) {
    initEnemySpawn(e, doorCenterX - ENEMY_SPRITE_W_PURPLE / 2, ENEMY_LANE_TOP, 60, palette,
                   ENEMY_TYPE_FOOT_SOLDIER);
    e->state = ENEMY_STATE_SPAWNING;
    e->timer = ENEMY_ELEV_SPAWN_TIME;
    e->dir   = 1;

    e->anim = ENEMY_ANIM_BREAK_DOOR;
    SPR_setAnimAndFrame(e->sprite, ENEMY_ANIM_BREAK_DOOR, 3);
    SPR_setAnimationLoop(e->sprite, FALSE);
}

void initEnemyKickSpawn(Enemy* e, s16 spawnX, s16 y, s8 dir, u8 palette, u8 type) {
    initEnemySpawn(e, spawnX, y, 0, palette, type);
    e->state = ENEMY_STATE_SPAWNING;
    e->timer = ENEMY_KICK_TIME;
    e->dir   = dir;

    e->anim = (type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE)
              ? ORANGE_ANIM_KICK : ENEMY_ANIM_KICK;
    SPR_setAnimAndFrame(e->sprite, e->anim, 0);
    SPR_setAnimationLoop(e->sprite, FALSE);
}

// Spawn SALTANDO del blanco (anims 6): entra desde fuera de pantalla con el
// mismo arco vertical que su ataque aereo -- 107px de alto, igual que el salto
// de las tortugas -- avanzando WHITE_JUMP_SPEED px/frame, y al tocar el piso
// pasa a CHASE. SIN hitbox: es una entrada, no un golpe (por eso no muestra la
// anim 7 de espadazo al caer, se queda girando como bolita hasta aterrizar).
void initEnemyWhiteJumpSpawn(Enemy* e, s16 spawnX, s16 y, s8 dir, u8 palette) {
    initEnemySpawn(e, spawnX, y, 0, palette, ENEMY_TYPE_FOOT_SOLDIER_WHITE);
    e->state   = ENEMY_STATE_SPAWNING;
    e->dir     = dir;
    whiteJumpStart(e);
    whiteJumpStep(e);          // ya en el aire (jumpZ > 0: ver SPAWNING)
    // Tope de seguridad: el arco termina solo al aterrizar, esto es por si
    // algo lo dejara colgado (un clamp de X en un borde, por ejemplo).
    e->timer   = WHITE_JUMP_SAFETY;

    e->anim = WHITE_ANIM_JUMP;
    SPR_setAnimAndFrame(e->sprite, WHITE_ANIM_JUMP, 0);
    SPR_setAnimationLoop(e->sprite, FALSE);
}

// Spawn CAMINANDO (29/09): ver enemy.h. somersault = 2 marca la caminata de
// entrada (el bloque SPAWNING de updateEnemyN lo mueve a ENEMY_SPEED).
void initEnemyWalkInSpawn(Enemy* e, s16 spawnX, s16 y, s8 dir, u8 palette, u8 type) {
    initEnemySpawn(e, spawnX, y, 0, palette, type);
    e->state      = ENEMY_STATE_SPAWNING;
    e->dir        = dir;
    e->somersault = 2;
    e->timer      = (u16)((e->w / 2 + ENEMY_WALKIN_MARGIN) / ENEMY_WALKIN_SPEED);
    enemySetAnim(e, enemyAnimWalk(e), TRUE);
    SPR_setAnimationLoop(e->sprite, TRUE);
}

// Spawn con VOLTERETA (anim 15): el morado entra desde fuera de pantalla
// haciendo la voltereta (7 frames) mientras avanza ENEMY_SOMERSAULT_SPEED
// px/frame (más rápido que el walk), y al terminar pasa a CHASE. Usada para
// las oleadas "por la espalda".
void initEnemySomersaultSpawn(Enemy* e, s16 spawnX, s16 y, s8 dir, u8 palette, u8 type) {
    initEnemySpawn(e, spawnX, y, 0, palette, type);
    e->state     = ENEMY_STATE_SPAWNING;
    e->timer     = ENEMY_SOMERSAULT_TIME;
    e->dir       = dir;
    e->somersault = 1;

    e->anim = ENEMY_ANIM_VOLTERETA;
    SPR_setAnimAndFrame(e->sprite, ENEMY_ANIM_VOLTERETA, 0);
    SPR_setAnimationLoop(e->sprite, FALSE);
}

// Spawn GUIONADO de la ESCALERA (18/09, anim 16): el morado se asoma, se
// planta, tira UN cartucho de dinamita al punto fijo (landX, landY) y al
// terminar la animacion pasa a CHASE como cualquier otro. El cartucho sale
// solo en el frame 11 (ver ENEMY_TNT_RELEASE_TIMER) y el flag se apaga ahi
// mismo: no vuelve a tirar en toda la pelea.
void initEnemyTntSpawn(Enemy* e, s16 spawnX, s16 y, s8 dir, u8 palette,
                       s16 landX, s16 landY) {
    initEnemySpawn(e, spawnX, y, 0, palette, ENEMY_TYPE_FOOT_SOLDIER);
    e->state    = ENEMY_STATE_SPAWNING;
    e->timer    = ENEMY_TNT_TIME;
    e->dir      = dir;
    e->tntThrow = 1;
    e->tntLandX = landX;
    e->tntLandY = landY;

    e->anim = ENEMY_ANIM_TNT;
    SPR_setAnimAndFrame(e->sprite, ENEMY_ANIM_TNT, 0);
    SPR_setAnimationLoop(e->sprite, FALSE);
}

// Spawn GUIONADO de ALCANTARILLA (19/09, anim 17): el morado sale de la boca
// de tormenta, tira la TAPA en direccion 'dir' y al terminar la animacion pasa
// a CHASE como cualquier otro. La tapa sale sola en el frame 4 (ver
// ENEMY_MANHOLE_RELEASE_TIMER) y el flag se apaga ahi: no vuelve a tirar.
void initEnemyManholeSpawn(Enemy* e, s16 spawnX, s16 y, s8 dir, u8 palette) {
    initEnemySpawn(e, spawnX, y, 0, palette, ENEMY_TYPE_FOOT_SOLDIER);
    e->state    = ENEMY_STATE_SPAWNING;
    e->timer    = ENEMY_MANHOLE_TIME;
    e->dir      = dir;
    e->lidThrow = 1;
    // 'y' es la lane FINAL (ya con el HOP_DROP sumado); este offset inicial lo
    // vuelve a dibujar sobre el agujero hasta que arranca el salto.
    e->jumpZ    = ENEMY_MANHOLE_HOP_DROP;

    e->anim = ENEMY_ANIM_MANHOLE;
    SPR_setAnimAndFrame(e->sprite, ENEMY_ANIM_MANHOLE, 0);
    SPR_setAnimationLoop(e->sprite, FALSE);
    // El frame lo maneja manholeStep() a mano: el f1 se SOSTIENE todo el salto,
    // asi que la animacion no puede correr sola.
    SPR_setAutoAnimation(e->sprite, FALSE);
}

// Un paso de la salida por la alcantarilla: elige el frame y la altura visual
// a partir del timer (que cuenta hacia atras desde ENEMY_MANHOLE_TIME).
static void manholeStep(Enemy* e) {
    const s16 HOP  = ENEMY_MANHOLE_HOP_TIME;
    const s16 REST = ENEMY_MANHOLE_REST_TIME;
    s16 rem = (s16)e->timer;
    s16 frame;

    if (rem > (s16)(HOP + REST)) {
        // f0: la tapa quieta sobre la boca (el jumpZ la deja calzada ahi).
        frame    = 0;
        e->jumpZ = ENEMY_MANHOLE_HOP_DROP;
    } else if (rem > REST) {
        // f1 SOSTENIDO: sale de un salto. Parabola de HOP_APEX (0 en las dos
        // puntas, maxima en el medio) mas el offset de la boca bajando a 0, que
        // es lo que lo hace aterrizar por delante del agujero.
        frame = 1;
        s16 t = (s16)(HOP + REST) - rem;            // 0..HOP-1
        s32 arc = ((s32)ENEMY_MANHOLE_HOP_APEX * 4 * t * (HOP - t))
                  / ((s32)HOP * HOP);
        s16 lift = (s16)(ENEMY_MANHOLE_HOP_DROP
                         - ((s16)ENEMY_MANHOLE_HOP_DROP * t) / HOP);
        e->jumpZ = (s16)(arc + lift);
    } else {
        // f2..f5, 8 ticks cada uno: ya esta en el piso y tira la tapa.
        frame    = (s16)(2 + (REST - rem) / 8);
        if (frame > 5) frame = 5;
        e->jumpZ = 0;
    }
    if (e->sprite) SPR_setFrame(e->sprite, frame);
}

void setEnemyBounds(Enemy* e, s16 laneTop, s16 laneBottom,
                    s16 wallXTop, s16 wallXBottom, s16 levelW) {
    e->laneTop     = laneTop;
    e->laneBottom  = laneBottom;
    e->wallXTop    = wallXTop;
    e->wallXBottom = wallXBottom;
    e->levelMaxX   = levelW;
    if (e->y < laneTop)    e->y = laneTop;
    if (e->y > laneBottom) e->y = laneBottom;
}

void setEnemyCamera(Enemy* e, s16 camX) {
    e->cameraOffsetX = camX;
}

// ---------------------------------------------------------------------------
// DAÑO Y MUERTE
// ---------------------------------------------------------------------------
bool damageEnemy(Enemy* e, s16 dmg) {
    if (e->state == ENEMY_STATE_DEAD || e->state == ENEMY_STATE_INACTIVE)
        return FALSE;

    // Si estaba AGARRANDO a un jugador, el golpe lo libera (el otro jugador
    // le pega al soldier para rescatar al agarrado).
    if (e->state == ENEMY_STATE_GRAB && e->grabbed) {
        playerReleaseGrab(e->grabbed);
        e->grabbed = NULL;
        e->state   = ENEMY_STATE_CHASE;   // el golpe lo saca del agarre
    }

    leaveAttackState(e);
    e->attackCooldown = ENEMY_HURT_COOLDOWN;

    // Si lo agarraron EN EL AIRE (el blanco entrando de un salto, o en pleno
    // salto con espadazo), el golpe lo baja al piso de una. Sin esto el jumpZ
    // quedaba congelado en el valor que tuviera -- hasta 107px -- y el sprite
    // se dibujaba flotando a esa altura para SIEMPRE, porque el unico codigo
    // que mueve el arco vive en SPAWNING y en el ataque de salto, y el golpe
    // saca al enemigo de los dos. (Bug reportado por Gustavo el 13/09: "uno
    // salta y queda desfasado, como si su piso estuviera a la altura de la
    // cabeza de April".)
    whiteJumpStop(e);

    e->hp -= dmg;
    if (e->hp <= 0) {
        e->state = ENEMY_STATE_DEAD;
        e->deathDir = 0;             // sin empuje salvo que lo pida enemyDeathPush
        e->deathSpeed = 0;
        e->deathTick = 0;
        e->timer = enemyExplodeTime(e);
        enemyRestartAnim(e, enemyAnimExplode(e), FALSE);
        return TRUE;
    }

    // Golpe NO fatal: estado HURT sin retroceso (el enemigo no se mueve en X).
    e->state = ENEMY_STATE_HURT;
    e->timer = 12;
    e->invincible = ENEMY_INVINCIBLE;
    enemyRestartAnim(e, enemyAnimHit(e), FALSE);
    return TRUE;
}

void enemyDeathPush(Enemy* e, s16 fromX, s8 facing, bool special) {
    if (e->state != ENEMY_STATE_DEAD) return;
    s16 cx = getEnemyCenterX(e);
    s8 dir = (cx > fromX) ? 1 : (cx < fromX) ? -1 : ((facing >= 0) ? 1 : -1);
    e->deathDir   = dir;
    e->deathSpeed = special ? ENEMY_DEATH_PUSH_SPECIAL : ENEMY_DEATH_PUSH;
    e->deathTick  = 0;
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
    return e->x + e->w / 2;
}

s16 getEnemyCenterY(const Enemy* e) {
    return e->y;
}

s16 enemyBodyHalfW(const Enemy* e) {
    switch (e->type) {
        case ENEMY_TYPE_FOOT_SOLDIER_ORANGE: return ENEMY_BODY_HALF_W_ORANGE;
        case ENEMY_TYPE_FOOT_SOLDIER_WHITE:  return ENEMY_BODY_HALF_W_WHITE;
        default:                             return ENEMY_BODY_HALF_W_PURPLE;
    }
}

s16 enemyBodyH(const Enemy* e) {
    switch (e->type) {
        case ENEMY_TYPE_FOOT_SOLDIER_ORANGE: return ENEMY_BODY_H_ORANGE;
        case ENEMY_TYPE_FOOT_SOLDIER_WHITE:  return ENEMY_BODY_H_WHITE;
        default:                             return ENEMY_BODY_H_PURPLE;
    }
}

// ---------------------------------------------------------------------------
// UPDATE PRINCIPAL
// ---------------------------------------------------------------------------
// (14/09) El nucleo trabaja con un ARREGLO de jugadores (1..4). updateEnemy
// queda como envoltorio de 1-2 jugadores para no tocar los llamadores viejos.
void updateEnemy(Enemy* e, Player* player1, Player* player2, bool twoPlayers) {
    Player* pls[2] = { player1, twoPlayers ? player2 : player1 };
    updateEnemyN(e, pls, twoPlayers ? 2 : 1);
}

void updateEnemyN(Enemy* e, Player** pls, u8 nPl) {
    if (nPl < 1) nPl = 1;
    if (nPl > ENEMY_MAX_TARGETS) nPl = ENEMY_MAX_TARGETS;
    if (e->state == ENEMY_STATE_INACTIVE || !e->sprite) return;

    if (e->invincible > 0) e->invincible--;
    if (e->attackCooldown > 0) e->attackCooldown--;

    // RED DE SEGURIDAD del arco del blanco. jumpZ solo lo mueven dos lugares:
    // la entrada saltando (SPAWNING) y el ataque de salto (ATTACK con
    // attackType JUMP). Cualquier otra cosa que saque al enemigo de esos dos
    // estados -- un golpe, la muerte, un cambio de estado desde la escena --
    // dejaria el offset congelado y el sprite flotando a esa altura para
    // siempre. Asi que fuera de esos dos casos el jumpZ se fuerza a 0: es
    // imposible que un enemigo quede "desfasado del piso" por esta via.
    if (e->jumpZ != 0 &&
        !(e->state == ENEMY_STATE_SPAWNING) &&
        !(e->state == ENEMY_STATE_ATTACK && e->attackType == ENEMY_ATTACK_JUMP)) {
        whiteJumpStop(e);
    }

    u16 explodeTime = enemyExplodeTime(e);

    if (e->state == ENEMY_STATE_DEAD) {
        // (27/09) Empuje de la muerte (ver ENEMY_DEATH_PUSH): se desliza
        // frenando, dentro de los limites del nivel.
        if (e->deathDir && e->deathSpeed) {
            e->x = clampS16((s16)(e->x + e->deathDir * e->deathSpeed),
                            enemyMinX(e), enemyMaxX(e));
            if (++e->deathTick >= ENEMY_DEATH_PUSH_STEP) {
                e->deathTick = 0;
                e->deathSpeed--;
            }
        }
        if (e->timer > 0) {
            e->timer--;
            if (e->timer == 0) {
                releaseTarget(e->target);
                SPR_releaseSprite(e->sprite);
                e->sprite = NULL;
                e->state = ENEMY_STATE_INACTIVE;
                return;
            }
        }
        SPR_setPosition(e->sprite, e->x - e->cameraOffsetX, e->y - e->footOffset - e->jumpZ);
        return;
    }

    if (e->state == ENEMY_STATE_SPAWNING) {
        // Kick entry: desplazarse durante el SPAWNING
        if (e->anim == ENEMY_ANIM_KICK || e->anim == ORANGE_ANIM_KICK) {
            u16 kickLunge = (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE)
                            ? ORANGE_KICK_LUNGE : ENEMY_KICK_LUNGE;
            u16 kickTime  = enemyAttackTime(e);
            if (e->timer > (kickTime - kickLunge)) {
                e->x += e->dir * ENEMY_KICK_SPEED;
                e->x = clampS16(e->x, enemyMinX(e), enemyMaxX(e));
            }
        }
        // Entrada SALTANDO del blanco: el arco manda sobre el timer, igual que
        // en su ataque; al tocar el piso se corta el SPAWNING y pasa a CHASE.
        if (e->type == ENEMY_TYPE_FOOT_SOLDIER_WHITE && e->jumpZ > 0) {
            s16 spdX = whiteJumpSpeedX(e);
            whiteJumpStep(e);
            e->x += e->dir * spdX;
            e->x  = clampS16(e->x, enemyMinX(e), enemyMaxX(e));
            if (e->jumpZ <= 0) {
                whiteJumpStop(e);
                e->timer   = 0;   // aterrizo: que el bloque de abajo lo pase a CHASE
            }
        }
        // Tirada de DINAMITA: el sprite hace toda la animacion solo; lo unico
        // que hay que hacer es soltar el cartucho en el frame justo.
        if (e->tntThrow && e->timer == ENEMY_TNT_RELEASE_TIMER) {
            tntLaunch(e->x, e->y, e->dir, e->tntLandX, e->tntLandY, e->palette);
            e->tntThrow = 0;      // una sola vez, nunca mas
        }
        // ...y despues BAJA DE LA ESCALERA: el jumpZ con el que lo spawnea la
        // escena (que es lo que lo dibuja sobre el escalon) se va a cero de a
        // 1px por frame mientras se recompone, asi no pega el salto al pasar a
        // CHASE.
        if (e->anim == ENEMY_ANIM_TNT && e->jumpZ > 0 &&
            e->timer < ENEMY_TNT_RELEASE_TIMER)
            e->jumpZ--;
        // Salida por la ALCANTARILLA: aca el sprite NO se anima solo (el f1 se
        // sostiene durante todo el salto), asi que el frame y la altura los
        // pone manholeStep(). La tapa sale en el frame justo.
        if (e->anim == ENEMY_ANIM_MANHOLE) {
            manholeStep(e);
            if (e->lidThrow && e->timer == ENEMY_MANHOLE_RELEASE_TIMER) {
                lidLaunch(e->x, e->y, e->dir, e->palette);
                e->lidThrow = 0;      // una sola vez, nunca mas
            }
        }
        // Voltereta de entrada: avanza en X durante TODO el SPAWNING (más rápido
        // que el walk; el sprite hace la voltereta sola con la anim 15).
        if (e->somersault == 2) {                 // (29/09) entrada caminando
            e->x += e->dir * ENEMY_WALKIN_SPEED;
            e->x = clampS16(e->x, enemyMinX(e), enemyMaxX(e));
        } else if (e->somersault) {
            e->x += e->dir * ENEMY_SOMERSAULT_SPEED;
            e->x = clampS16(e->x, enemyMinX(e), enemyMaxX(e));
        }
        if (e->timer > 0) e->timer--;
        else {
            e->state = ENEMY_STATE_CHASE;
            e->jumpZ = 0;
            // La salida por alcantarilla corre con la animacion automatica
            // APAGADA (ver manholeStep): si no se vuelve a prender, el soldier
            // se queda congelado en el ultimo frame para el resto del nivel.
            SPR_setAutoAnimation(e->sprite, TRUE);
        }
        SPR_setHFlip(e->sprite, (e->dir < 0));
        SPR_setPosition(e->sprite, e->x - e->cameraOffsetX, e->y - e->footOffset - e->jumpZ);
        SPR_setDepth(e->sprite, -(e->y));
        return;
    }

    // --- Target asignado ---
    if (nPl > 1) {
        if (e->target >= nPl) e->target = 0;
        // (26/09) Si su objetivo se quedo sin vidas (tirado contando el
        // CONTINUE?, o ya fuera y sin sprite) se cambia YA al mas cercano de
        // los que siguen jugando, sin esperar el retarget ni la histeresis.
        bool tgtOut = isPlayerGameOver(pls[e->target]);
        if (tgtOut) e->retargetTimer = 0;
        if (e->retargetTimer > 0) {
            e->retargetTimer--;
        } else {
            e->retargetTimer = ENEMY_RETARGET_INTERVAL;
            // Se cambia al MAS CERCANO, con histeresis para no oscilar.
            s16 dCur = tgtOut ? 0x7FFF : distS16(e->x, getPlayerWorldX(pls[e->target]));
            u8  best = e->target; s16 dBest = dCur;
            for (u8 i = 0; i < nPl; i++) {
                if (i == e->target) continue;
                if (isPlayerGameOver(pls[i])) continue;
                s16 d = distS16(e->x, getPlayerWorldX(pls[i]));
                if (tgtOut) d = (s16)(d - ENEMY_RETARGET_HYSTERESIS);   // sin histeresis
                if (d < dBest) { dBest = d; best = i; }
            }
            if (best != e->target && dBest + ENEMY_RETARGET_HYSTERESIS < dCur) {
                releaseTarget(e->target);
                e->target = best;
                enemyTargetCount[best]++;
            }
        }
    } else {
        e->target = 0;
    }

    Player* targetP = pls[e->target];
    s16 targetX   = getPlayerWorldX(targetP);
    s16 targetY   = getPlayerY(targetP);
    s8  targetDir = getPlayerDir(targetP);

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
            enemySetAnim(e, enemyAnimWalk(e), TRUE);
            break;
        }

        case ENEMY_STATE_CHASE: {
            if (dist > ENEMY_DEAGGRO_RANGE) {
                newState = ENEMY_STATE_PATROL;
                break;
            }

            // -----------------------------------------------------------------
            // SELECCIÓN DE ATAQUE — según el tipo de enemigo
            //   · Naranja: kiter. Patada si el jugador lo acorrala; si no,
            //     shuriken a distancia (no ocupa cupo de atacante melee).
            //   · Morado: flanqueo. Sólo pega si ya está EN LA ESPALDA del
            //     jugador... salvo que se le haya agotado el tiempo de flanqueo
            //     (MORADO_FLANK_TIMEOUT), en cuyo caso encara de frente.
            // -----------------------------------------------------------------
            bool wantAttack = FALSE;

            // --- Morado: intentar AGARRE por la espalda ---
            // Si ya está detrás del jugador y pegado (y el jugador no está
            // agarrado por nadie), en vez de pegar puede sujetarlo por la
            // espalda: el jugador queda inmovilizado mostrando ANIM_HELD y
            // tiene que zafarse masheando (o que le peguen al soldier). El
            // soldier queda invisible (la anim GRAB es toda transparente: así
            // no tapa el agarre del jugador). Con probabilidad ~1/4 por frame;
            // el resto de las veces pega el uppercut normal.
            if (e->type == ENEMY_TYPE_FOOT_SOLDIER) {
                Player* gp = targetP;
                bool flanking = (e->flankTimer < MORADO_FLANK_TIMEOUT);
                bool onBack   = (((s32)(e->x - targetX)) * targetDir) < 0;
                if (flanking && onBack && !playerIsGrabbed(gp) &&
                    dist < ENEMY_GRAB_RANGE && absS16(dy) <= ENEMY_ATTACK_TOL_Y &&
                    e->attackCooldown == 0 && enemiesAttacking < ENEMY_MAX_ATTACKERS &&
                    (random() & 3) == 0) {
                    newState = ENEMY_STATE_GRAB;
                    playerFootGrab(gp);
                    e->grabTarget = e->target;
                    e->grabbed    = gp;
                    e->grabTimer  = ENEMY_GRAB_MAX_TIME;
                    e->attackCooldown = ENEMY_ATTACK_COOLDOWN;
                    e->flankTimer = 0;
                    if (dx != 0) e->dir = (dx > 0) ? 1 : -1;
                    enemyRestartAnim(e, ENEMY_ANIM_GRAB, FALSE);
                    break;   // salta el resto del CHASE este frame
                }
            }

            if (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) {
                // "onScreen": si no está visible, no puede lanzar shurikens —
                // si pudiera, quedaría plantado fuera de cámara disparando
                // dentro de ORANGE_SHURIKEN_RANGE_MAX del jugador pero
                // inalcanzable para él (no pasa del borde de cámara) e
                // inmatable. "dist" se mide contra el jugador, no contra la
                // cámara, así que por sí sola no detecta este caso.
                s16  screenX  = e->x - e->cameraOffsetX;
                bool onScreen = (screenX > -(s16)e->w) && (screenX < ENEMY_SCREEN_W);
                if (e->attackCooldown == 0 && absS16(dy) <= ENEMY_ATTACK_TOL_Y) {
                    if (dist <= ORANGE_KICK_RANGE && enemiesAttacking < ENEMY_MAX_ATTACKERS) {
                        // Jugador CERCANO → melee. A corta distancia alterna
                        // patada y directo (usa la anim 4, que hoy no se ve);
                        // más lejos (aún dentro del alcance del kick) va la
                        // patada con salto, que se desplaza 48px y alcanza.
                        if (dist <= ENEMY_FRONT_REACH)
                            e->attackType = (random() & 1) ? ENEMY_ATTACK_KICK
                                                           : ENEMY_ATTACK_FRONT;
                        else
                            e->attackType = ENEMY_ATTACK_KICK;
                        wantAttack = TRUE;
                    } else if (onScreen && dist >= ORANGE_SHURIKEN_RANGE_MIN &&
                               dist <= ORANGE_SHURIKEN_RANGE_MAX) {
                        e->attackType = ENEMY_ATTACK_SHURIKEN;   // a distancia
                        wantAttack = TRUE;
                    }
                }
            } else if (e->type == ENEMY_TYPE_FOOT_SOLDIER_WHITE) {
                // --- Blanco: espadachin, pelea DE FRENTE ---
                // No flanquea ni agarra como el morado: su ventaja es el
                // alcance, asi que encara y busca la distancia de la estocada.
                //   pegado            -> cortes medios encadenados (anims 4+5)
                //   media distancia   -> ESTOCADA LARGA (anim 3), 50px
                //   lejos             -> salta encima y baja con la espada
                // El salto NO cuenta contra ENEMY_MAX_ATTACKERS mientras esta
                // en el aire seria injusto, pero si ocupa cupo: es un ataque
                // melee que termina pegado al jugador.
                if (e->attackCooldown == 0 && enemiesAttacking < ENEMY_MAX_ATTACKERS) {
                    if (dist < WHITE_ATTACK_RANGE && absS16(dy) <= ENEMY_ATTACK_TOL_Y) {
                        e->attackType = (dist < WHITE_LONG_RANGE_MIN)
                                        ? ENEMY_ATTACK_PUNCH   // cortes medios
                                        : ENEMY_ATTACK_FRONT;  // estocada larga
                        wantAttack = TRUE;
                    } else if (dist >= WHITE_JUMP_RANGE_MIN &&
                               dist <= WHITE_JUMP_RANGE_MAX &&
                               absS16(dy) <= ENEMY_ATTACK_TOL_Y &&
                               (random() & 3) == 0) {
                        e->attackType = ENEMY_ATTACK_JUMP;
                        wantAttack = TRUE;
                    }
                }
            } else {
                // Morado (y cualquier otro melee): ataca si está en rango y —
                // mientras dure el flanqueo— sólo desde la espalda del jugador.
                bool flanking = (e->flankTimer < MORADO_FLANK_TIMEOUT);
                bool onBack   = (((s32)(e->x - targetX)) * targetDir) < 0;
                if (dist < ENEMY_ATTACK_RANGE && absS16(dy) <= ENEMY_ATTACK_TOL_Y &&
                    e->attackCooldown == 0 && enemiesAttacking < ENEMY_MAX_ATTACKERS &&
                    (!flanking || onBack)) {
                    if (dist < ENEMY_UPPERCUT_RANGE)
                        e->attackType = ENEMY_ATTACK_PUNCH;      // uppercut pegado
                    else
                        e->attackType = (random() & 1) ? ENEMY_ATTACK_KICK
                                                       : ENEMY_ATTACK_FRONT;
                    wantAttack = TRUE;
                }
            }

            if (wantAttack) {
                newState = ENEMY_STATE_ATTACK;
                // El shuriken es a distancia: no cuenta como atacante melee.
                if (e->attackType != ENEMY_ATTACK_SHURIKEN) enemiesAttacking++;
                if (dx != 0) e->dir = (dx > 0) ? 1 : -1;
                e->attackHit  = 0;
                e->flankTimer = 0;   // reinicia la frustración de flanqueo
                // El morado ataca con COMBOS (cadenas de 2-3 golpes como el
                // arcade ATTACK S0/S1/S2): comboLen > 0 activa la tabla de
                // pasos; el naranja deja comboLen en 0 (ataque simple).
                e->comboLen   = (e->type == ENEMY_TYPE_FOOT_SOLDIER ||
                                 e->type == ENEMY_TYPE_FOOT_SOLDIER_WHITE)
                                ? comboLengthFor(e->type, e->attackType) : 0;
                e->comboStep  = 0;
                e->timer      = enemyAttackTime(e);
                break;
            }

            // -----------------------------------------------------------------
            // MOVIMIENTO HORIZONTAL — según el tipo
            // -----------------------------------------------------------------
            s16 moveX = 0;
            if (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE) {
                // El naranja NO se aleja del jugador (ya no kitea): si el
                // jugador está FUERA del rango del shuriken, se acerca
                // caminando hasta entrar en rango; dentro del rango se queda
                // quieto lanzando shurikens, y si el jugador se acerca ataca
                // melee (selección de ataque de arriba). El clamp de
                // enemyMinX (borde de cámara) queda como red de seguridad.
                // Si además quedó fuera de cámara (p.ej. lo empujó un golpe,
                // o el jugador retrocedió) SIEMPRE se acerca, aunque "dist"
                // ya esté dentro del rango del shuriken — si no, se quedaría
                // fuera de pantalla inmatable (ver onScreen más arriba).
                s16  screenX  = e->x - e->cameraOffsetX;
                bool onScreen = (screenX > -(s16)e->w) && (screenX < ENEMY_SCREEN_W);
                if (!onScreen || dist > ORANGE_SHURIKEN_RANGE_MAX)
                    moveX = (dx > 0) ? ENEMY_SPEED : -ENEMY_SPEED;   // acercarse
                // dentro del rango de lanzamiento y en pantalla → se queda en X
            } else if (e->type == ENEMY_TYPE_FOOT_SOLDIER_WHITE) {
                // Blanco: va DERECHO al jugador y se planta a distancia de
                // estocada. Nada de rodearlo por la espalda -- con 50px de
                // alcance no lo necesita, y el arcade lo muestra encarando.
                // Durante el cooldown retrocede un poco para recomponer la
                // distancia en vez de quedar pegado y comerse los golpes.
                if (e->attackCooldown > 0 && dist < WHITE_LONG_RANGE_MIN) {
                    moveX = (dx > 0) ? -ENEMY_SPEED : ENEMY_SPEED;
                } else if (dist > WHITE_SLASH_LONG_REACH) {
                    moveX = (dx > 0) ? ENEMY_SPEED : -ENEMY_SPEED;
                }
            } else {
                // Morado: apunta a un punto DETRÁS del jugador (espalda = lado
                // opuesto a su mirada). Si ya agotó el flanqueo, va directo.
                // Durante el cooldown retrocede para no quedar pegado.
                bool flanking = (e->flankTimer < MORADO_FLANK_TIMEOUT);
                s16  goalX    = flanking ? (targetX - targetDir * MORADO_BACK_STANDOFF)
                                         : targetX;
                s16  gdx      = goalX - e->x;
                if (e->attackCooldown > 0 && dist < ENEMY_HOLD_RANGE) {
                    if (dist < ENEMY_HOLD_RANGE - 8)
                        moveX = (dx > 0) ? -ENEMY_SPEED : ENEMY_SPEED;
                } else if (absS16(gdx) > MORADO_GOAL_TOL) {
                    moveX = (gdx > 0) ? ENEMY_SPEED : -ENEMY_SPEED;
                }
                if (flanking) e->flankTimer++;   // cuenta frames sin poder atacar
            }
            if (moveX != 0) {
                e->x += moveX;
                e->x = clampS16(e->x, enemyMinX(e), enemyMaxX(e));
            }

            // --- Morado: GIRO al invertir el sentido de la maniobra ---
            // La anim 11 (giro, 2 frames) se reproduce cuando el soldier
            // revierte su dirección horizontal mientras flanquea (p.ej. el
            // jugador se da vuelta y la espalda cambia de lado). El HFlip se
            // aplica con 'dir', así que el giro se dibuja para cada lado.
            if (e->type == ENEMY_TYPE_FOOT_SOLDIER) {
                s8 wantDir = 0;
                if (moveX > 0)       wantDir = 1;
                else if (moveX < 0)  wantDir = -1;
                if (wantDir != 0 && e->lastMoveDir != 0 && wantDir != e->lastMoveDir) {
                    e->dir = wantDir;
                    newState = ENEMY_STATE_TURN;
                    e->timer = ENEMY_GIRO_TIME;
                    enemyRestartAnim(e, ENEMY_ANIM_GIRO, FALSE);
                    e->lastMoveDir = 0;
                    break;   // salta el resto del CHASE este frame
                }
                e->lastMoveDir = wantDir;
            }

            // Siempre MIRANDO al jugador (para que el ataque salga hacia él)
            if (dx != 0) e->dir = (dx > 0) ? 1 : -1;

            // --- Movimiento vertical ---
            s16 moveY = 0;
            if (dy > ENEMY_Y_ALIGN)       moveY =  ENEMY_SPEED;
            else if (dy < -ENEMY_Y_ALIGN) moveY = -ENEMY_SPEED;
            if (moveY != 0) {
                e->y = clampS16(e->y + moveY, e->laneTop, e->laneBottom);
            }

            // --- Animación ---
            if (moveX == 0 && moveY == 0) {
                if (e->type == ENEMY_TYPE_FOOT_SOLDIER) {
                    // Morado: posturas de espera. Si otro soldier está atacando,
                    // se pone en GUARDIA (anim 12); si no, alterna entre el IDLE
                    // clásico (fila 0) y la nueva postura de espera (anim 13,
                    // "otra postura de espera") cada ENEMY_STANCE_SWITCH frames.
                    if (enemiesAttacking > 0) {
                        enemySetAnim(e, ENEMY_ANIM_GUARD, TRUE);
                    } else {
                        if (++e->stancePhase >= ENEMY_STANCE_SWITCH) {
                            e->stancePhase = 0;
                            e->stanceToggle ^= 1;
                        }
                        enemySetAnim(e, e->stanceToggle ? ENEMY_ANIM_STANCE
                                                        : ENEMY_ANIM_IDLE, TRUE);
                    }
                } else {
                    enemySetAnim(e, enemyAnimIdle(e), TRUE);
                }
            } else if (moveY < 0 && (moveX == 0 || absS16(dy) >= dist)) {
                enemySetAnim(e, enemyAnimWalkUp(e), TRUE);
            } else {
                enemySetAnim(e, enemyAnimWalk(e), TRUE);
            }
            break;
        }

        case ENEMY_STATE_ATTACK: {
            if (e->comboLen > 0) {
                // --- Combo del morado: secuencia de golpes ---
                // Cada paso corre su animación con su propio timer y ventana de
                // hitbox; al expirar se avanza al siguiente (attackHit = 0 para
                // que cada golpe conecte una vez) hasta terminar el combo.
                const ComboStep* steps = comboStepsFor(e->type, e->attackType);
                const ComboStep* step  = &steps[e->comboStep];
                if (e->timer > 0) {
                    e->timer--;
                    // Patada con salto: desplazamiento en X durante el lunge
                    if (step->lunge > 0 && e->timer > (step->time - step->lunge)) {
                        e->x += e->dir * ENEMY_KICK_SPEED;
                        e->x = clampS16(e->x, enemyMinX(e), enemyMaxX(e));
                    }
                    if (e->timer == 0) {
                        if (e->comboStep + 1 < e->comboLen) {
                            e->comboStep++;
                            e->attackHit = 0;
                            const ComboStep* ns = &steps[e->comboStep];
                            e->timer = ns->time;
                            enemyRestartAnim(e, ns->anim, FALSE);
                        } else {
                            leaveAttackState(e);
                            e->attackCooldown = (u8)(ENEMY_ATTACK_COOLDOWN + (random() & 31));
                            newState = ENEMY_STATE_CHASE;
                        }
                    }
                }
            } else if (e->attackType == ENEMY_ATTACK_JUMP) {
                // --- Salto con espadazo del BLANCO ---
                // El arco manda, no el timer: sube ~15 frames y
                // baja otros tantos, y el ataque termina cuando toca el piso.
                // (e->timer sigue corriendo solo como tope de seguridad.)
                if (e->timer > 0) e->timer--;

                s16 spdX = whiteJumpSpeedX(e);
                whiteJumpStep(e);

                // Avanza hacia el jugador mientras esta en el aire (mas
                // despacio en la bajada, ver whiteJumpSpeedX).
                e->x += e->dir * spdX;
                e->x  = clampS16(e->x, enemyMinX(e), enemyMaxX(e));

                // Al empezar a CAER saca la espada: ahi se enciende el hitbox
                // (ver enemyTryHitPlayerBox).
                if (e->jumpVel < 0 && e->anim != WHITE_ANIM_AIR_SLASH)
                    enemyRestartAnim(e, WHITE_ANIM_AIR_SLASH, FALSE);

                if (e->jumpZ <= 0 || e->timer == 0) {
                    whiteJumpStop(e);
                    leaveAttackState(e);
                    e->attackCooldown = (u8)(ENEMY_ATTACK_COOLDOWN + (random() & 31));
                    newState = ENEMY_STATE_CHASE;
                }
            } else if (e->timer > 0) {
                e->timer--;

                // Kick con salto: desplazamiento en X durante el lunge
                u16 kickLunge = (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE)
                                ? ORANGE_KICK_LUNGE : ENEMY_KICK_LUNGE;
                u16 kickTime  = enemyAttackTime(e);
                if (e->attackType == ENEMY_ATTACK_KICK &&
                    e->timer > (kickTime - kickLunge)) {
                    e->x += e->dir * ENEMY_KICK_SPEED;
                    e->x = clampS16(e->x, enemyMinX(e), enemyMaxX(e));
                }

                // Shuriken: spawnear proyectil en el frame 1 (timer == SPAWN_TIMER).
                // Nace 2 tiles más cerca del soldier que el borde del frame
                // (w/2 - ORANGE_SHURIKEN_NEAR_OFFSET) — antes aparecía pegado
                // a la punta del frame, lejos del cuerpo.
                if (e->attackType == ENEMY_ATTACK_SHURIKEN &&
                    e->timer == ORANGE_SHURIKEN_SPAWN_TIMER) {
                    s16 spawnX = getEnemyCenterX(e) + e->dir * (e->w / 2 - ORANGE_SHURIKEN_NEAR_OFFSET);
                    shurikenSpawn(spawnX, e->y, e->dir, e->palette);
                }
            } else {
                leaveAttackState(e);
                e->attackCooldown = (u8)(ENEMY_ATTACK_COOLDOWN + (random() & 31));
                newState = ENEMY_STATE_CHASE;
            }
            break;
        }

        case ENEMY_STATE_HURT: {
            // Sin retroceso: el golpe NO desplaza al enemigo en X (queda clavado
            // en el lugar mientras muestra la anim de daño).
            if (e->timer > 0) {
                e->timer--;
            } else {
                newState = ENEMY_STATE_CHASE;
            }
            break;
        }

        case ENEMY_STATE_TURN: {
            // Giro (anim 11, 2 frames): el soldier queda quieto dando la vuelta
            // y al terminar retoma el flanqueo. 'dir' ya quedó en la nueva
            // dirección (la setea el detección de reversión en CHASE).
            if (e->timer > 0) {
                e->timer--;
            } else {
                newState = ENEMY_STATE_CHASE;
                enemySetAnim(e, enemyAnimWalk(e), TRUE);
            }
            break;
        }

        case ENEMY_STATE_GRAB: {
            // Agarre por la espalda: el soldier queda CLAVADO a la espalda del
            // jugador (que está inmovilizado en su propio STATE_GRABBED)
            // mostrando la anim [14] (grab). La tortuga muestra la anim [18]
            // (ANIM_HELD): frames 0-2 mientras está agarrada y el frame 3
            // cuando le pegan en pleno agarre (lo maneja damagePlayer). Suelta
            // cuando el jugador zafa (mash), le pegan al soldier (damageEnemy
            // lo libera) o expira el tope de seguridad (grabTimer).
            Player* gp = e->grabbed;
            if (!gp || !playerIsGrabbed(gp)) {
                e->grabbed = NULL;
                newState = ENEMY_STATE_CHASE;
                enemySetAnim(e, enemyAnimWalk(e), TRUE);
                break;
            }
            s8  gdir = getPlayerDir(gp);
            s16 gx   = getPlayerWorldX(gp);
            s16 gy   = getPlayerY(gp);
            // Espalda del jugador: centro del player − gdir * ENEMY_GRAB_BACK_OFFSET,
            // con el soldier centrado en ese punto (su centro = x + w/2).
            e->y  = gy;
            e->x = clampS16(gx + (PLAYER_SPRITE_W / 2) - (s16)gdir * ENEMY_GRAB_BACK_OFFSET
                            - (ENEMY_SPRITE_W_PURPLE / 2),
                            enemyMinX(e), enemyMaxX(e));
            e->dir = (s8)-gdir;
            if (e->grabTimer > 0) {
                e->grabTimer--;
                if (e->grabTimer == 0) {
                    // Tope de seguridad: el jugador no zafó ni lo rescataron.
                    playerReleaseGrab(gp);
                    e->grabbed = NULL;
                    newState = ENEMY_STATE_CHASE;
                    enemySetAnim(e, enemyAnimWalk(e), TRUE);
                    break;
                }
            }
            break;
        }

        default: break;
    }

    if (newState != e->state) {
        e->state = newState;
        if (newState == ENEMY_STATE_ATTACK) {
            if (e->comboLen > 0) {
                // Combo del morado: arranca en el paso 0 (su anim y timer).
                const ComboStep* s = comboStepsFor(e->type, e->attackType);
                e->comboStep = 0;
                e->timer     = s->time;
                enemyRestartAnim(e, s->anim, FALSE);
            } else if (e->attackType == ENEMY_ATTACK_JUMP) {
                // Salto del blanco: arranca el arco vertical. Mismo esquema
                // que el salto de la tortuga (jumpZ es un offset VISUAL; la
                // lane 'y' no cambia en el aire), asi que la profundidad y el
                // orden de dibujo siguen siendo los del piso.
                whiteJumpStart(e);
                enemyRestartAnim(e, WHITE_ANIM_JUMP, FALSE);
            } else {
                u8 atkAnim;
                if (e->attackType == ENEMY_ATTACK_KICK)
                    atkAnim = enemyAnimKick(e);
                else if (e->attackType == ENEMY_ATTACK_FRONT)
                    atkAnim = enemyAnimPunchFront(e);
                else if (e->attackType == ENEMY_ATTACK_SHURIKEN)
                    atkAnim = ORANGE_ANIM_SHURIKEN;
                else
                    atkAnim = enemyAnimUppercut(e);
                enemyRestartAnim(e, atkAnim, FALSE);
            }
        }
    }

    SPR_setHFlip(e->sprite, (e->dir < 0));
    SPR_setPosition(e->sprite, e->x - e->cameraOffsetX, e->y - e->footOffset - e->jumpZ);
    SPR_setDepth(e->sprite, -(e->y));
}

// ---------------------------------------------------------------------------
// SEPARACIÓN DE GRUPO
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

            s16 push = (dx > 0 || (dx == 0 && (i & 1))) ? 1 : -1;
            list[i].x = clampS16(list[i].x - push, enemyMinX(&list[i]), enemyMaxX(&list[i]));
            list[j].x = clampS16(list[j].x + push, enemyMinX(&list[j]), enemyMaxX(&list[j]));

            if (dy != 0) {
                s16 pushY = (dy > 0) ? 1 : -1;
                list[i].y = clampS16(list[i].y - pushY, list[i].laneTop, list[i].laneBottom);
                list[j].y = clampS16(list[j].y + pushY, list[j].laneTop, list[j].laneBottom);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// HITBOX DE ATAQUE — enemigo → jugador
// ---------------------------------------------------------------------------
bool enemyTryHitPlayer(Enemy* e, s16 px, s16 py) {
    return enemyTryHitPlayerBox(e, px, py, 0);
}

bool enemyTryHitPlayerBox(Enemy* e, s16 px, s16 py, s16 targetHalfW) {
    if (e->state != ENEMY_STATE_ATTACK || e->attackHit || !e->sprite)
        return FALSE;

    // Shuriken no tiene hitbox melee
    if (e->attackType == ENEMY_ATTACK_SHURIKEN)
        return FALSE;

    bool active;
    s16  reach;
    if (e->comboLen > 0) {
        // Combo del morado: la ventana y el alcance los da el paso actual.
        const ComboStep* step = &comboStepsFor(e->type, e->attackType)[e->comboStep];
        active = (e->timer >= step->hitStart && e->timer <= step->hitEnd);
        reach  = step->reach;
    } else if (e->attackType == ENEMY_ATTACK_JUMP) {
        // El blanco solo pega en la BAJADA, con la espada ya sacada: subiendo
        // es un ovillo que gira y no tiene filo.
        active = (e->jumpVel < 0);
        reach  = WHITE_AIR_SLASH_REACH;
    } else if (e->attackType == ENEMY_ATTACK_KICK) {
        u16 kickLunge = (e->type == ENEMY_TYPE_FOOT_SOLDIER_ORANGE)
                        ? ORANGE_KICK_LUNGE : ENEMY_KICK_LUNGE;
        u16 kickTime  = enemyAttackTime(e);
        active = (e->timer > kickTime - kickLunge);
        reach  = ENEMY_HIT_RANGE_X;
    } else {
        active = (e->timer >= ENEMY_PUNCH_HIT_START && e->timer <= ENEMY_PUNCH_HIT_END);
        reach  = enemyAttackReach(e);
    }
    if (!active)
        return FALSE;

    s16 ex  = getEnemyCenterX(e);
    s16 pcx = px + PLAYER_SPRITE_W / 2;
    s16 dx  = (e->dir >= 0) ? (pcx - ex) : (ex - pcx);
    // Solape horizontal: ataque [-BACK_X, +reach] vs cuerpo del jugador
    // [dx-halfW, dx+halfW]. Con halfW = 0 es el chequeo puntual de siempre.
    if (dx - targetHalfW > reach)
        return FALSE;
    if (dx + targetHalfW < -ENEMY_HIT_BACK_X)
        return FALSE;

    if (absS16(py - e->y) > ENEMY_HIT_TOL_Y)
        return FALSE;

    e->attackHit = 1;
    return TRUE;
}
