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

// Velocidades del jefe. (13/09) Se quito el "anger" que las subia tras N
// golpes: colgaba de la fase 2, que ya no existe como progresion.
// (03/10) Velocidades del arcade en Q8 (256 = 1 px por frame), con el resto
// acumulado: 1,25 px/f sale 1-1-1-2...
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

// (13/09) SE QUITO el flash BLANCO al recibir daño, a pedido de Gustavo.
// Escribia 0x0EEE en los indices 2..15 de PAL3 durante 8 frames en CADA golpe
// que conectaba; con ROCKSTEADY_HP en 124 eran ~124 destellos por pelea.
// El parpadeo por HP BAJO no se toco: es otro efecto y vive en scenes.c
// (bossPal/flashPal), donde alterna la paleta normal con una "quemada".
// ---------------------------------------------------------------------------
// BALAS — proyectil de vida independiente (sub-sprite boss_bullet, PAL3)
// ---------------------------------------------------------------------------
// La bala tiene TRES poses en una fila del sprite (sin auto-animacion, el
// frame lo pone el codigo): [0] horizontal, [1] hacia arriba, [2] impacto.
// Al pegar no se borra en el acto -- se queda quieta mostrando el [2] durante
// ROCKSTEADY_BULLET_HIT_FRAMES y recien ahi se libera.
static struct {
    bool    active;
    Sprite* sprite;
    s16     x, y;      // mundo; y = lane (pies) del lanzador
    s8      dir;
    s8      dz;        // velocidad de SUBIDA (0 = tiro recto, >0 = sube)
    s16     z;         // altura VISUAL sobre la lane (misma idea que el jumpZ
                       // del jugador: 'y' es PROFUNDIDAD y no cambia en vuelo)
    u8      hitTimer;  // >0 = ya impacto: congelada mostrando el frame [2]
    s16     cameraOffsetX;
} bullets[MAX_ROCKSTEADY_BULLETS];

void rocksteadyBulletInit(void) {
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        bullets[i].active = 0;
        bullets[i].sprite = NULL;
        bullets[i].hitTimer = 0;
    }
}

// (cx, z) = CENTRO de la boca del cañon: cx en X de mundo, z en altura sobre
// los PIES del jefe. 'lane' es la profundidad (y del jefe), que la bala
// conserva durante todo el vuelo. dz > 0 = tiro HACIA ARRIBA.
static void rocksteadyBulletSpawn(s16 cx, s16 lane, s16 z, s8 dir, s8 dz,
                                  u8 palette) {
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        if (bullets[i].active) continue;
        // x/y guardan la esquina superior izquierda del sprite de 16x16.
        bullets[i].x = cx - 8;
        bullets[i].y = lane;
        bullets[i].dir = dir;
        bullets[i].dz = dz;
        bullets[i].z  = z;
        bullets[i].hitTimer = 0;
        bullets[i].cameraOffsetX = 0;
        bullets[i].active = 1;
        bullets[i].sprite = SPR_addSprite(&boss_bullet,
                                          bullets[i].x, lane - z - 8 - stageCamY,
                                          TILE_ATTR(palette, FALSE, FALSE, FALSE));
        if (bullets[i].sprite) {
            SPR_setDepth(bullets[i].sprite, -(lane) - 1);
            SPR_setHFlip(bullets[i].sprite, (dir < 0));
            // Pose segun para que se disparo: recta o hacia arriba. El sprite
            // va con time 0 (sin auto-anim), asi que este frame queda fijo.
            SPR_setAnimAndFrame(bullets[i].sprite, 0,
                                (dz > 0) ? ROCKSTEADY_BULLET_FR_UP
                                         : ROCKSTEADY_BULLET_FR_H);
        }
        return;   // slot encontrado
    }
}

void rocksteadyBulletUpdate(s16 camX) {
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        if (!bullets[i].active) continue;
        bullets[i].cameraOffsetX = camX;

        // Ya impacto: queda clavada en el lugar mostrando el frame de impacto
        // y se libera al agotarse la ventana. No se mueve ni vuelve a pegar.
        if (bullets[i].hitTimer > 0) {
            bullets[i].hitTimer--;
            if (bullets[i].hitTimer == 0) {
                if (bullets[i].sprite) SPR_releaseSprite(bullets[i].sprite);
                bullets[i].sprite = NULL;
                bullets[i].active = 0;
                continue;
            }
            if (bullets[i].sprite)
                SPR_setPosition(bullets[i].sprite,
                                bullets[i].x - bullets[i].cameraOffsetX,
                                bullets[i].y - bullets[i].z - 8 - stageCamY);
            continue;
        }

        bullets[i].x += bullets[i].dir * ROCKSTEADY_BULLET_SPEED;
        bullets[i].z += bullets[i].dz;

        // Fuera de pantalla: por los costados, o cuando el tiro hacia arriba
        // ya subio por encima del area de juego.
        if (bullets[i].x < camX - 32 || bullets[i].x > camX + 320 + 32 ||
            bullets[i].z > ROCKSTEADY_BULLET_MAX_Z || bullets[i].z < 0) {
            if (bullets[i].sprite) SPR_releaseSprite(bullets[i].sprite);
            bullets[i].sprite = NULL;
            bullets[i].active = 0;
            continue;
        }
        if (bullets[i].sprite)
            SPR_setPosition(bullets[i].sprite,
                            bullets[i].x - bullets[i].cameraOffsetX,
                            bullets[i].y - bullets[i].z - 8 - stageCamY);
    }
}

void rocksteadyBulletReleaseAll(void) {
    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        if (bullets[i].sprite) SPR_releaseSprite(bullets[i].sprite);
        bullets[i].sprite = NULL;
        bullets[i].active = 0;
        bullets[i].hitTimer = 0;
    }
}

bool rocksteadyBulletCheckHitPlayer(s16 px, s16 py, s16 pz, s16* hitX) {
    // px = borde izquierdo del frame del jugador (104px)
    // py = lane (pies) · pz = altura visual del salto (jumpZ)
    s16 pcx = px + PLAYER_SPRITE_W / 2;   // centro del jugador
    s16 pcy = py;                         // pies del jugador

    for (u16 i = 0; i < MAX_ROCKSTEADY_BULLETS; i++) {
        if (!bullets[i].active) continue;
        if (bullets[i].hitTimer > 0) continue;   // ya pego: no vuelve a contar

        s16 bcx = bullets[i].x + 8;   // centro de la bala (16px wide → +8)
        s16 bcy = bullets[i].y;

        // X e Y(lane) como siempre, MAS la altura: la bala guarda su altura
        // REAL sobre el piso (arranca en la boca del cañon) y se compara
        // contra la del torso del jugador (jumpZ + TARGET_Z). Asi el tiro
        // recto le pega al que esta parado y el de arriba al que salta.
        s16 targetZ = pz + ROCKSTEADY_BULLET_TARGET_Z;
        // El antiaereo (dz > 0) usa una ventana de altura mas ancha: llega al
        // jugador ya muy arriba y con la del tiro recto no conectaba casi nunca.
        s16 tolZ = (bullets[i].dz > 0) ? ROCKSTEADY_BULLET_HIT_Z_UP
                                       : ROCKSTEADY_BULLET_HIT_Z;
        if (abs(pcx - bcx) < 16 && abs(pcy - bcy) < 16 &&
            abs(targetZ - bullets[i].z) < tolZ) {
            // Impacto: la bala NO se borra en el acto -- pasa al frame [2] y
            // se queda ahi un momento para que se vea el golpe.
            if (hitX) *hitX = bcx;
            bullets[i].hitTimer = ROCKSTEADY_BULLET_HIT_FRAMES;
            if (bullets[i].sprite)
                SPR_setAnimAndFrame(bullets[i].sprite, 0, ROCKSTEADY_BULLET_FR_HIT);
            return TRUE;
        }
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// API pública del jefe
// ---------------------------------------------------------------------------
void rocksteadyInit(Rocksteady* r) {
    r->sprite = NULL;
    r->state = ROCKSTEADY_INACTIVE;
    r->armed = 0;
    r->x = r->y = 0;
    r->cameraOffsetX = 0;
    r->dir = -1;
    r->hp = ROCKSTEADY_HP;
    r->anim = 0xFF;
    r->timer = 0;
    r->attackCooldown = 0;
    r->hitsTaken = 0;
    r->knockdowns = 0;
    r->comboHits = 0;
    r->counterPending = 0;
    r->kickCooldown = 0;
    r->attacksDone = 0;
    r->chargeHit = 0;
    r->chargeWind = 0;
    r->chargeDir = -1;
    r->unarmedStep = 0;
    r->kickOnArrive = 0;
    r->shotFrame = 0;
    r->shotsFired = 0;
    r->shotTimer = 0;
    r->shotUp = 0;
    r->farTimer = 0;
    bossFlashReset(&r->flash);
    r->accX = r->accY = 0;
    r->slideDir = 1;
}

void rocksteadySpawn(Rocksteady* r) {
    rocksteadySpawnArena(r, &raLevel2);
}

void rocksteadySpawnArena(Rocksteady* r, const RocksteadyArena* a) {
    ra = *a;
    // 1-2: 8 tiles a la izquierda de la cápsula (puerta abierta), y 156 y no
    // 148: al corregir ROCKSTEADY_FOOT_OFFSET (96 -> 104) el sprite se dibuja
    // 8px mas arriba para la misma lane, asi que la lane de aparicion sube
    // otros 8 para que los pies sigan cayendo en el umbral de la puerta.
    r->x = ra.spawnX;
    r->y = ra.spawnY;
    r->dir = -1;
    r->armed = 0;   // entra SIN arma: la primera tanda es de embestidas
    r->hp = ra.hp ? ra.hp : ROCKSTEADY_HP;
    bossFlashReset(&r->flash);
    r->hitsTaken = 0;
    r->knockdowns = 0;
    r->comboHits = 0;
    r->counterPending = 0;
    r->kickCooldown = 0;
    r->attacksDone = 0;
    r->attackCooldown = 0;
    r->chargeHit = 0;
    r->chargeWind = 0;
    r->chargeDir = -1;
    r->unarmedStep = 0;
    r->kickOnArrive = 0;
    r->farTimer = 0;
    r->state = ROCKSTEADY_EMERGE;
    r->timer = ra.emergeStand;            // quieto en la puerta (taunt) antes de bajar
    r->anim = 0xFF;
    // SPR_addSpriteSafe (no SPR_addSprite): mismo riesgo que la capsula del
    // taladro (ver scenes.c) -- Rocksteady (sprite grande) se crea recien
    // terminada la oleada A, con VRAM potencialmente fragmentada por el
    // spawn/muerte de los foot soldiers. SPR_addSpriteSafe desfragmenta y
    // reintenta si la asignacion falla la primera vez.
    r->sprite = SPR_addSpriteSafe(&rocksteady_boss, 0, 0,
                              TILE_ATTR(ra.pal, FALSE, FALSE, FALSE));
    // La paleta del boss (ra.pal) ya la cargo el nivel.
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

// Centro VISUAL del cuerpo: r->x ancla el BORDE IZQUIERDO del frame de 104px
// (SPR_setPosition esquina superior izquierda, sin compensación como el robot),
// así que el centro del cuerpo queda a +FRAME_W/2. Devolver el borde como si
// fuera centro desplazaba la hurtbox 52px: por la derecha de la pantalla los
// golpes exigían solaparse con el jefe y por la izquierda conectaban desde
// más lejos del contacto real.
s16 rocksteadyGetCenterX(const Rocksteady* r) { return r->x + ROCKSTEADY_FRAME_W / 2; }
// Y = pies (lane), igual que las tortugas y los foot soldiers.
s16 rocksteadyGetCenterY(const Rocksteady* r) { return r->y; }

// ¿Ya completó la tanda de ataques y toca cambiar de arma?
static bool rocksteadySwapDue(const Rocksteady* r) {
    return (r->attacksDone >= (r->armed ? ROCKSTEADY_ATTACKS_PER_SWAP
                                        : ROCKSTEADY_UNARMED_ATTACKS));
}

// (03/10) Caida: anim [4] a mano (ver ROCKSTEADY_KD_* en rocksteady.h).
static void rocksteadyStartKnockdown(Rocksteady* r) {
    r->hitsTaken = 0;
    r->comboHits = 0;      // tirado: la cuenta de "seguidos" arranca de nuevo
    r->counterPending = 0; // desde el piso no hay contraataque
    r->knockdowns++;
    r->state = ROCKSTEADY_KNOCKDOWN;
    r->timer = 0;          // cuenta hacia ARRIBA
    r->slideDir = (s8)-r->dir;
    r->accX = 0;
    XGM2_stopPCM(SOUND_PCM_CH2);   // por si lo tiraron en plena embestida
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_HURT, FALSE);
    SPR_setAutoAnimation(r->sprite, FALSE);
    SPR_setFrame(r->sprite, 2);
}

void rocksteadyDamage(Rocksteady* r, s16 dmg) {
    rocksteadyDamageEx(r, dmg, (bool)(dmg >= ROCKSTEADY_SPECIAL_DMG));
}

void rocksteadyDamageEx(Rocksteady* r, s16 dmg, bool special) {
    if (!rocksteadyCanBeHit(r)) return;
    r->hp -= dmg;

    if (r->hp <= 0) {
        r->hp = 0;
        r->state = ROCKSTEADY_DEAD;
        rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_HURT, FALSE);
        // Grito de muerte (14/09, pedido de Gustavo), en el instante exacto del
        // golpe fatal. Canal PCM 2 y prioridad 15: pisa el hit_turtles / boss_hit
        // del mismo golpe, que es lo que se quiere -- el grito manda.
        // ENTRA JUSTO: el wav dura 1,69s = 101 frames NTSC, y desde acá hasta
        // que arranca music_ending (que reinicia el driver y cortaría el PCM)
        // hay 42 frames de anim de muerte (7 frames a 6 ticks) + los 60 de
        // CUT_SILENCE_FRAMES = 102. El XGM2_stop() del primer frame de la
        // cutscene no molesta: sólo pone en NULL los streams FM/PSG, no toca
        // los canales PCM (verificado en src/snd/xgm2.c de SGDK v2.11).
        XGM2_playPCMEx(boss_scream_rocksteady_vo, sizeof(boss_scream_rocksteady_vo),
                       SOUND_PCM_CH2, 15, FALSE, FALSE);
        return;
    }

    r->hitsTaken++;
    // Contraataque: golpes SEGUIDOS sin poder responder → al terminar este
    // flinch suelta la patada (lo ejecuta update antes del switch). Se resetea
    // si cae (knock-down) para que no acumule de una caída a otra.
    r->comboHits++;
    if (r->comboHits >= ROCKSTEADY_COUNTER_HITS) {
        r->comboHits = 0;
        r->counterPending = 1;
    }
    // SIN ARMA, cada ROCKSTEADY_KD_INTERVAL golpes el jefe CAE (knock-down).
    // Con el arma NO cae: la anim de caida [4] lo dibuja desarmado y el arma
    // desapareceria de golpe.
    // (03/10) El ESPECIAL lo tira SIEMPRE, armado o no (en el video cae con
    // el arma en la mano y se levanta apuntando).
    if (special || (!r->armed && r->hitsTaken >= ROCKSTEADY_KD_INTERVAL)) {
        rocksteadyStartKnockdown(r);
        return;
    }

    // Flinch normal (anim según la fase). ARMADURA durante la patada: si está
    // pateando (normal o con arma) recibe el daño pero NO se interrumpe el
    // swing. Sin esto, el golpe siguiente del combo cancelaba la patada
    // contraataque antes de terminar (la anim dura ~48 ticks de juego, un hit
    // llega cada ~20) y el jefe quedaba en stagger eterno sin responder.
    if (r->state != ROCKSTEADY_KICK && r->state != ROCKSTEADY_KICK_ARMS) {
        r->state = r->armed ? ROCKSTEADY_HURT_ARMS : ROCKSTEADY_HURT;
        r->timer = ROCKSTEADY_HURT_FRAMES;
        rocksteadyRestartAnim(r, r->armed ? ROCKSTEADY_ANIM_HURT_ARMS
                                          : ROCKSTEADY_ANIM_HURT,
                              FALSE);
    }
}

// ---------------------------------------------------------------------------
// Inicio de los ataques
// ---------------------------------------------------------------------------
// El llamador ya fijo r->dir mirando al jugador: esa direccion queda LATCHEADA
// para toda la embestida (14/09, pedido de Gustavo). Antes el caso CHARGE
// re-apuntaba cada frame y el jefe podia frenar y volverse a mitad de la
// corrida, que es justo lo que una embestida no tiene que poder hacer: una vez
// que arranca, se esquiva.
static void rocksteadyStartCharge(Rocksteady* r, s8 dir) {
    r->state = ROCKSTEADY_CHARGE;
    r->timer = ROCKSTEADY_CHARGE_MAX;
    r->chargeHit = 0;
    r->chargeWind = ROCKSTEADY_CHARGE_WINDUP;   // amaga en el lugar y despues corre
    r->dir       = dir;
    r->chargeDir = dir;
    r->kickOnArrive = 0;   // si la embestida corta un acercamiento a mitad,
                           // la patada pendiente se cancela con el
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_CHARGE, TRUE);
}

static void rocksteadyStartApproach(Rocksteady* r) {
    r->state = ROCKSTEADY_APPROACH;
    rocksteadySetAnim(r, ROCKSTEADY_ANIM_WALK, TRUE);
}

static void rocksteadyStartKick(Rocksteady* r) {
    r->kickCooldown = ROCKSTEADY_KICK_COOLDOWN;
    r->state = ROCKSTEADY_KICK;
    r->timer = 0;   // contador del frame de impacto
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_KICK, FALSE);
}

static void rocksteadyStartKickArms(Rocksteady* r) {
    r->kickCooldown = ROCKSTEADY_KICK_COOLDOWN;
    r->state = ROCKSTEADY_KICK_ARMS;
    r->timer = 0;
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_KICK_ARMS, FALSE);
}

static void rocksteadyStartAimWalk(Rocksteady* r) {
    r->state = ROCKSTEADY_AIM_WALK;
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_AIM, TRUE);
}

// 'up' vale para TODA la rafaga: la pose del jefe y la bala tienen que contar
// lo mismo. Antes la anim [9] se reproducia entera (o sea, pasaba por las dos
// poses) y cada bala decidia su direccion por separado al salir, asi que muy
// seguido se lo veia apuntando arriba mientras tiraba recto.
static void rocksteadyStartShoot(Rocksteady* r, bool up) {
    r->state = ROCKSTEADY_SHOOT;
    r->shotFrame = 0;
    r->shotsFired = 0;
    r->shotTimer = 0;
    r->shotUp = up ? 1 : 0;
    // Frames a MANO: se arranca directamente en el primer frame del par que
    // corresponde, no en el 0.
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_SHOOT, FALSE);
    SPR_setAutoAnimation(r->sprite, FALSE);
    SPR_setFrame(r->sprite, r->shotUp ? ROCKSTEADY_SHOOT_FR_UP
                                      : ROCKSTEADY_SHOOT_FR_H);
}

// Cambio de modo: saca O guarda el arma. La sheet tiene UNA sola animacion de
// manipular el arma (la [5], "draw"), asi que se usa para las dos direcciones:
// a la escala del sprite se lee como "manotea el arma" en los dos sentidos.
// El 'armed' se invierte al TERMINAR la anim (ver ROCKSTEADY_ARMS_INTRO), asi
// el cambio coincide con el final del gesto.
static void rocksteadyStartArmsIntro(Rocksteady* r) {
    r->state = ROCKSTEADY_ARMS_INTRO;
    r->attacksDone = 0;
    rocksteadyRestartAnim(r, ROCKSTEADY_ANIM_DRAW, FALSE);
}

// Vuelve a IDLE tras un flinch / cooldown.
static void rocksteadyToIdle(Rocksteady* r) {
    r->state = ROCKSTEADY_IDLE;
    r->timer = r->armed ? ROCKSTEADY_ARMED_IDLE : ROCKSTEADY_IDLE_MIN;
    if (r->attackCooldown < ROCKSTEADY_ATTACK_COOLDOWN)
        r->attackCooldown = ROCKSTEADY_ATTACK_COOLDOWN;
    // Anim de reposo según la fase.
    rocksteadySetAnim(r, r->armed ? ROCKSTEADY_ANIM_WALK_ARMS
                                  : ROCKSTEADY_ANIM_IDLE,
                      TRUE);
}

// Tras un flinch/knock-down en fase 1: si todavía no bajó a la lane de pelea
// (le pegaron durante la intro, en la puerta a y=148), vuelve a EMERGE en modo
// "bajar al arena" en vez de decidir ataques desde arriba (las patadas no
// conectarían por la tolerancia de Y). Si ya está en la lane, a IDLE normal.
static void rocksteadyResumeFromHit(Rocksteady* r) {
    if (!r->armed && r->y < ra.emergeY) {
        r->state = ROCKSTEADY_EMERGE;
        r->timer = 0;
    } else {
        rocksteadyToIdle(r);
    }
}

// ---------------------------------------------------------------------------
// UPDATE PRINCIPAL
// ---------------------------------------------------------------------------
// (14/09) Envoltorio de 1-2 jugadores sobre la version de N.
void rocksteadyUpdate(Rocksteady* r, s16 cameraX, Player* p1, Player* p2,
                      bool twoPlayers) {
    Player* ps[2] = { p1, (twoPlayers && p2) ? p2 : p1 };
    rocksteadyUpdateN(r, cameraX, ps, (twoPlayers && p2) ? 2 : 1);
}

void rocksteadyUpdateN(Rocksteady* r, s16 cameraX, Player** pls, u8 nPl) {
    if (r->state == ROCKSTEADY_INACTIVE || r->state == ROCKSTEADY_GONE ||
        !r->sprite) return;
    r->cameraOffsetX = cameraX;

    if (r->attackCooldown > 0) r->attackCooldown--;
    if (r->kickCooldown > 0)   r->kickCooldown--;

    // Jugador objetivo: el MAS CERCANO en X (centro del frame), entre los que
    // haya (1..4). Se re-evalua cada frame, asi que el jefe "elige uno" y lo
    // persigue mientras siga siendo el mas cercano.
    // (26/09) Los que estan sin vidas no cuentan (salvo que no quede nadie).
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
    // Centro VISUAL del cuerpo (r->x ancla el borde izquierdo del frame):
    // todas las distancias de decisión e impacto se miden desde acá.
    s16 bcx  = r->x + ROCKSTEADY_FRAME_W / 2;
    s16 distX = rabs(pcx - bcx);

    // CONTRAATAQUE: recibió ROCKSTEADY_COUNTER_HITS golpes seguidos → corta
    // el flinch y suelta la patada hacia el jugador más cercano (fase 1:
    // patada; fase 2: patada con el arma). Evita que la tortuga encadene
    // golpes sin dejarlo responder.
    if (r->counterPending && r->kickCooldown > 0)
        r->counterPending = 0;          // (01/10) pateo hace poco: no hay contra
    if (r->counterPending &&
        (r->state == ROCKSTEADY_HURT || r->state == ROCKSTEADY_HURT_ARMS)) {
        r->counterPending = 0;
        r->dir = (pcx >= bcx) ? 1 : -1;
        if (r->armed) rocksteadyStartKickArms(r);
        else          rocksteadyStartKick(r);
    }

    // ANTI-CAMPING: si el objetivo se mantiene FUERA de alcance mientras el
    // jefe está neutral (decidiendo o acercándose), suelta la EMBESTIDA para
    // castigar la distancia. Al entrar en contacto el contador vuelve a cero
    // (y también se pausa durante flinch/knock-down/ataques en curso).
    if ((r->state == ROCKSTEADY_IDLE || r->state == ROCKSTEADY_APPROACH ||
         r->state == ROCKSTEADY_AIM_WALK) &&
        distX > ROCKSTEADY_KICK_RANGE) {
        if (r->farTimer < ROCKSTEADY_FAR_FRAMES) r->farTimer++;
        if (r->farTimer >= ROCKSTEADY_FAR_FRAMES) {
            r->farTimer = 0;
            rocksteadyStartCharge(r, (pcx >= bcx) ? 1 : -1);
        }
    } else {
        r->farTimer = 0;
    }

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
            r->dir = (pcx >= bcx) ? 1 : -1;
            r->x = rclamp(r->x + r->dir * rsStepX(r, ROCKSTEADY_WALK_X_Q), ra.xMin, ra.xMax);
            if (r->y < ra.emergeY) r->y = rclamp(r->y + rsStepY(r, ROCKSTEADY_WALK_Y_Q), r->y, ra.emergeY);
            if (r->y >= ra.emergeY) {
                // Garantiza el primer golpe del combate (informe, sección 1):
                // en vez de arrancar pasivo en IDLE, dispara una embestida
                // scripteada hacia el jugador apenas termina de bajar, sin
                // importar la distancia (igual que en el arcade original).
                rocksteadyStartCharge(r, (pcx >= bcx) ? 1 : -1);
                // (03/10) Arcade: despues de esta embestida va a PATEAR
                // (embestida + patada y saca el arma).
                r->unarmedStep = 2;
            }
            break;
        }

        case ROCKSTEADY_IDLE: {
            // Aunque esté "quieto" SIGUE alineando la lane: el pedido es que
            // busque SIEMPRE el eje Y del jugador, no sólo mientras camina.
            //
            // Y si se está moviendo, tiene que VERSE caminando. Antes se
            // quedaba con la pose de IDLE (o la de andar con el arma) mientras
            // se deslizaba en Y, que era lo que reportó Gustavo: "cuando se
            // mueve en el eje Y varias veces no activa la imagen de caminar".
            bool moved = FALSE;
            if (py > r->y + 2)      { r->y += 1; moved = TRUE; }
            else if (py < r->y - 2) { r->y -= 1; moved = TRUE; }
            r->y = rclamp(r->y, ra.laneTop, ra.laneBot);
            r->dir = (pcx >= bcx) ? 1 : -1;
            if (moved) {
                rocksteadySetAnim(r, r->armed ? ROCKSTEADY_ANIM_WALK_ARMS
                                              : ROCKSTEADY_ANIM_WALK, TRUE);
            } else {
                rocksteadySetAnim(r, r->armed ? ROCKSTEADY_ANIM_WALK_ARMS
                                              : ROCKSTEADY_ANIM_IDLE, TRUE);
            }

            // Sin cooldown ni timer de decisión → elegir ataque.
            if (r->attackCooldown > 0 || r->timer > 0) {
                if (r->timer > 0) r->timer--;
                break;
            }
            // ¿Completó la tanda? Guarda o saca el arma y arranca la otra.
            if (rocksteadySwapDue(r)) {
                rocksteadyStartArmsIntro(r);
                break;
            }
            if (r->armed) {
                // CON ARMA: acercarse apuntando y alineando lane. El disparo
                // lo decide AIM_WALK cuando está en rango (recto si el
                // jugador está alineado, hacia arriba si está saltando).
                rocksteadyStartAimWalk(r);
            } else {
                // SIN ARMA: ROTA entre esperar / embestir / acercarse a patear
                // (14/09, pedido de Gustavo). Ver el bloque
                // ROCKSTEADY_UNARMED_* de rocksteady.h: antes, con el jugador
                // pegado, ninguna de las dos ramas viejas aplicaba y el jefe se
                // quedaba plantado para siempre.
                // El paso elegido puede CEDER el turno al siguiente si la
                // distancia no le sirve (no hay pista para embestir), pero como
                // mucho se prueban los tres: nunca se cuelga.
                for (u8 attempt = 0; attempt < ROCKSTEADY_UNARMED_STEPS; attempt++) {
                    u8 step = r->unarmedStep;
                    r->unarmedStep = (u8)((r->unarmedStep + 1) % ROCKSTEADY_UNARMED_STEPS);
                    if (step == 0) {
                        // ESPERAR en el lugar. No cuenta como ataque completado
                        // (no toca attacksDone): el arma se alterna por ataques,
                        // no por tiempo.
                        r->timer = (u16)(ROCKSTEADY_WAIT_FRAMES + (random() & 31));
                        break;
                    }
                    if (step == 1) {
                        if (distX < ROCKSTEADY_CHARGE_MIN_DIST) continue;  // sin pista
                        rocksteadyStartCharge(r, (pcx >= bcx) ? 1 : -1);
                        break;
                    }
                    // step == 2: acercarse y PATEAR de verdad (no como contra).
                    // (01/10) Si pateo hace poco, cede el turno.
                    if (r->kickCooldown > 0) continue;
                    if (distX <= ROCKSTEADY_KICK_RANGE) rocksteadyStartKick(r);
                    else { r->kickOnArrive = 1; rocksteadyStartApproach(r); }
                    break;
                }
            }
            break;
        }

        case ROCKSTEADY_APPROACH: {
            // Camina hacia el jugador (fase 1, sin arma) alineando lane; al
            // llegar a contacto se PLANTA y espera: la patada es SOLO el
            // contraataque por golpes seguidos, no un ataque espontáneo.
            // Si mientras camina el jugador se aleja más de lo esperado,
            // escala a EMBESTIDA en vez de perseguir caminando para siempre.
            if (distX > ROCKSTEADY_CHARGE_TRIGGER_DIST) { rocksteadyStartCharge(r, (pcx >= bcx) ? 1 : -1); break; }
            r->dir = (pcx >= bcx) ? 1 : -1;
            r->x += r->dir * rsStepX(r, ROCKSTEADY_WALK_X_Q);
            {
                s16 vy = rsStepY(r, ROCKSTEADY_WALK_Y_Q);
                if      (py > r->y + 2) r->y += vy;
                else if (py < r->y - 2) r->y -= vy;
            }
            r->y = rclamp(r->y, ra.laneTop, ra.laneBot);
            r->x = rclamp(r->x, ra.xMin, ra.xMax);
            if (distX <= ROCKSTEADY_KICK_RANGE) {
                // Llego: si venia a patear, patea; si no, se planta (IDLE).
                if (r->kickOnArrive) { r->kickOnArrive = 0; rocksteadyStartKick(r); }
                else                  rocksteadyToIdle(r);
            }
            break;
        }

        case ROCKSTEADY_CHARGE: {
            // Estampida: la DIRECCIÓN quedó latcheada al arrancar
            // (r->chargeDir) y no se re-apunta más — una vez lanzada, se
            // esquiva. Sigue corrigiendo la lane, que es lo que la hace
            // peligrosa sin volverla teledirigida. Al impactar NO frena en
            // seco: sigue un tramo más (overshoot, ROCKSTEADY_CHARGE_OVER)
            // para que la carga recorra más eje X, dañando una sola vez
            // (chargeHit).
            // (01/10) Preparacion: quieto en el lugar con la anim de la
            // embestida, girando hacia el jugador. Al terminar se latchea la
            // direccion y recien ahi corre (y puede dañar).
            if (r->chargeWind > 0) {
                r->chargeWind--;
                r->dir = r->chargeDir = (pcx >= bcx) ? 1 : -1;
                // (01/10) Arranca a correr: el sonido de la corrida de
                // Rocksteady hacia el logo de SEGA (dura ~1.1 s).
                if (r->chargeWind == 0)
                    XGM2_playPCMEx(rocksteady_charge_sfx, sizeof(rocksteady_charge_sfx),
                                   SOUND_PCM_CH2, 15, FALSE, FALSE);
                break;
            }
            r->dir = r->chargeDir;
            r->x += r->dir * rsStepX(r, ROCKSTEADY_CHARGE_Q);
            {
                s16 vy = rsStepY(r, ROCKSTEADY_LANE_Q);
                if      (py > r->y + 2) r->y += vy;
                else if (py < r->y - 2) r->y -= vy;
            }
            r->y = rclamp(r->y, ra.laneTop, ra.laneBot);

            // "La embestida golpea al player si lo toca": el impacto se mide
            // por SOLAPE REAL de los dos cuerpos (media anchura del jefe +
            // media anchura del jugador), no por un radio fijo de 56px que
            // pegaba bastante antes del contacto visual.
            if (!r->chargeHit &&
                rabs(getPlayerHurtCX(tgt) - bcx) < (ROCKSTEADY_BODY_HALF_W + PLAYER_BODY_HALF_W) &&
                rabs(py - r->y) < ROCKSTEADY_HIT_TOL_Y &&
                playerCanBeHit(tgt)) {
                playerHitBars(tgt, r->x, ROCKSTEADY_CHARGE_DMG);   // 4 barras (30/08, a pedido de Gustavo)
                r->chargeHit = 1;
                r->timer = ROCKSTEADY_CHARGE_OVER;   // sigue embistiendo un tramo
            }
            if (--r->timer == 0 || r->x <= ra.xMin || r->x >= ra.xMax) {
                r->x = rclamp(r->x, ra.xMin, ra.xMax);
                XGM2_stopPCM(SOUND_PCM_CH2);   // (01/10) corta el sonido de la corrida
                r->chargeHit = 0;
                r->attacksDone++;          // embestida COMPLETADA
                rocksteadyToIdle(r);
            }
            break;
        }

        case ROCKSTEADY_KICK:
        case ROCKSTEADY_KICK_ARMS: {
            // Un solo golpe, en el frame 2 de la anim (de 8). Ventana corta
            // medida desde el CENTRO del cuerpo: sólo conecta en contacto
            // real (antes se medía desde el borde izquierdo del frame y la
            // patada pegaba desde muy lejos). El impacto DERRIBA a la tortuga.
            if (r->timer < 3) r->timer++;
            if (r->timer == 2 &&
                rabs(pcx - bcx) < ROCKSTEADY_KICK_RANGE &&
                rabs(py - r->y) < ROCKSTEADY_HIT_TOL_Y &&
                playerCanBeHit(tgt)) {
                playerHitBarsKnockdown(tgt, bcx, ROCKSTEADY_BULLET_DMG);
                // Impacto: mismo "pum" que la patada de las tortugas.
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles),
                               SOUND_PCM_CH2, 15, FALSE, FALSE);
            }
            if (SPR_isAnimationDone(r->sprite)) {
                r->attacksDone++;          // patada COMPLETADA
                rocksteadyToIdle(r);
            }
            break;
        }

        case ROCKSTEADY_HURT:
        case ROCKSTEADY_HURT_ARMS: {
            if (r->timer > 0) r->timer--;
            if (r->timer == 0) rocksteadyResumeFromHit(r);
            break;
        }

        case ROCKSTEADY_KNOCKDOWN: {
            // (03/10) Caida del arcade: vuela deslizando hacia atras, queda en
            // el piso y se levanta en dos pasos.
            u16 t = ++r->timer;
            if (t <= ROCKSTEADY_KD_FLY_F) {
                r->x = rclamp(r->x + r->slideDir * rsStepX(r, ROCKSTEADY_KD_FLY_Q),
                              ra.xMin, ra.xMax);
                SPR_setFrame(r->sprite, 2);
            } else if (t <= ROCKSTEADY_KD_FLY_F + ROCKSTEADY_KD_DOWN_F) {
                SPR_setFrame(r->sprite, 3);
            } else if (t <= ROCKSTEADY_KD_FLY_F + ROCKSTEADY_KD_DOWN_F + ROCKSTEADY_KD_UP1_F) {
                SPR_setFrame(r->sprite, 4);
            } else if (t <= ROCKSTEADY_KD_FLY_F + ROCKSTEADY_KD_DOWN_F +
                            ROCKSTEADY_KD_UP1_F + ROCKSTEADY_KD_UP2_F) {
                SPR_setFrame(r->sprite, 5);
            } else {
                SPR_setAutoAnimation(r->sprite, TRUE);
                r->anim = 0xFF;            // que toIdle vuelva a poner su anim
                rocksteadyResumeFromHit(r);   // se levanta y sigue peleando
            }
            break;
        }

        case ROCKSTEADY_ARMS_INTRO: {
            // Termino el gesto de manipular el arma -> cambia de modo. Va acá
            // y no al empezar para que el cambio coincida con el final de la
            // animacion y no se vea el sprite disparando "sin sacar" el arma.
            if (SPR_isAnimationDone(r->sprite)) {
                r->armed ^= 1;
                r->attacksDone = 0;
                r->hitsTaken   = 0;   // la cuenta de caidas arranca por modo
                rocksteadyToIdle(r);
            }
            break;
        }

        case ROCKSTEADY_AIM_WALK: {
            // Se acerca apuntando y SIEMPRE alineando la lane del jugador.
            // Abre fuego cuando esta en rango y:
            //   - el jugador esta SALTANDO -> tiro hacia arriba (no hace falta
            //     estar alineado: en el aire la lane del jugador no cambia,
            //     pero visualmente esta por encima), o
            //   - el jugador esta alineado en Y -> tiro horizontal.
            r->dir = (pcx >= bcx) ? 1 : -1;
            r->x += r->dir * rsStepX(r, ROCKSTEADY_AIM_X_Q);
            {
                s16 vy = rsStepY(r, ROCKSTEADY_WALK_Y_Q);
                if      (py > r->y + 2) r->y += vy;
                else if (py < r->y - 2) r->y -= vy;
            }
            r->y = rclamp(r->y, ra.laneTop, ra.laneBot);
            r->x = rclamp(r->x, ra.xMin, ra.xMax);
            if (distX <= ROCKSTEADY_SHOOT_RANGE &&
                (isPlayerJumping(tgt) ||
                 rabs(py - r->y) <= ROCKSTEADY_SHOOT_ALIGN_Y))
                rocksteadyStartShoot(r, isPlayerJumping(tgt));
            break;
        }

        case ROCKSTEADY_SHOOT: {
            // Frames a mano: cada ROCKSTEADY_SHOT_TICKS avanza un frame de la
            // anim y en los frames 3/5/7 sale una bala del cañón. Dirección de
            // CADA bala según el estado VIVO del jugador: en el piso → recta;
            // saltando → diagonal hacia arriba (anti-aéreo).
            // Dos frames por bala dentro del par que corresponde a la
            // direccion de la rafaga: el PAR (fogonazo, ahi sale la bala) y el
            // IMPAR. shotFrame cuenta pasos, no frames del sheet.
            if (++r->shotTimer >= ROCKSTEADY_SHOT_TICKS) {
                r->shotTimer = 0;
                if (r->shotFrame >= ROCKSTEADY_SHOT_COUNT * 2) {
                    r->attacksDone++;      // rafaga COMPLETADA
                    rocksteadyToIdle(r);
                    break;
                }
                // (14/09) La direccion se RE-DECIDE al empezar cada par, no una
                // sola vez por rafaga: si el jugador salta en medio de la
                // rafaga, las balas que faltan salen hacia arriba. Lo que NO se
                // puede hacer es decidirla por bala DENTRO del par (era el bug
                // viejo: se lo veia apuntando arriba y tirando recto), asi que
                // se fija en el frame PAR y el IMPAR del par la respeta.
                if ((r->shotFrame & 1) == 0)
                    r->shotUp = isPlayerJumping(tgt) ? 1 : 0;
                u8 base = r->shotUp ? ROCKSTEADY_SHOOT_FR_UP
                                    : ROCKSTEADY_SHOOT_FR_H;
                SPR_setFrame(r->sprite, base + (r->shotFrame & 1));
                if ((r->shotFrame & 1) == 0) {
                    // Sale de la BOCA DEL CAÑON de la pose que se esta
                    // dibujando, medida sobre el fogonazo del sheet. El offset
                    // en X acompaña al flip (el arte mira a la derecha).
                    s16 mx = r->shotUp ? ROCKSTEADY_MUZZLE_UP_X
                                       : ROCKSTEADY_MUZZLE_H_X;
                    s16 mz = r->shotUp ? ROCKSTEADY_MUZZLE_UP_Z
                                       : ROCKSTEADY_MUZZLE_H_Z;
                    rocksteadyBulletSpawn(bcx + r->dir * mx, r->y, mz, r->dir,
                                          r->shotUp ? ROCKSTEADY_UPSHOT_DZ : 0,
                                          ra.pal);
                    r->shotsFired++;
                }
                r->shotFrame++;
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

    // (03/10) Parpadeo de vida baja por linea de sprite (garage): pasa a la
    // linea con la paleta quemada sin tocar los colores que comparte.
    if (ra.flashPal && r->sprite &&
        bossFlashStep(&r->flash, r->hp, ra.hp ? ra.hp : ROCKSTEADY_HP))
        SPR_setPalette(r->sprite, r->flash.on ? ra.flashPal : ra.pal);

    rocksteadyRender(r);
}
