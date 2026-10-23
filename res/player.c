#include "player.h"
#include "player_hitbox.h"   // playerAtkReach: alcance por frame medido sobre el arte
#include "player_offsets.h"  // playerFrameDXTab: desplazamiento en X de cada frame
#include "audio.h"

s16 stageCamY = 0;   // ver player.h

// ===========================================================================
// MÓDULO DE JUGADOR — MULTI-INSTANCIA
// ===========================================================================
// Toda la lógica opera sobre un Player* recibido por parámetro, de modo que
// pueden coexistir varios jugadores (P1 con JOY_1, P2 con JOY_2, ...) cada
// uno con su propio sprite, estado, combo y salto.
// ===========================================================================

// ===========================================================================
// ESTADO PERSISTENTE ENTRE NIVELES (vidas + puntaje)
// ===========================================================================
// Cada nivel crea un Player NUEVO con initPlayer (que resetea todo el struct),
// así que las vidas y el puntaje se perderían al pasar de nivel. Para que se
// mantengan a lo largo de toda la partida, guardamos acá el estado "meta" por
// joystick (slot 0 = JOY_1 / P1, slot 1 = JOY_2 / P2). initPlayer arranca con
// estos valores; playerPersistSave() los actualiza al ganar un nivel;
// playerPersistReset() los vuelve al default (partida nueva — lo llama
// scenes.c en la selección de personajes).
// (13/09) La BARRA DE VIDA tambien persiste: al pasar del
// 1-1 al apartamento de April se recargaba sola y regalaba la barra entera.
// Como en el arcade, la barra se arrastra de una parte a la otra; lo unico que
// la rellena es perder una vida (revivir) o empezar partida nueva.
// ---------------------------------------------------------------------------
// Vidas iniciales configurables desde la pantalla OPCIONES (3/5/7).
u8 vidasIniciales = PLAYER_START_LIVES;

// (10/10) Opciones: esquema de botones y dificultad (ver player.h).
u8 controlTipo = CONTROL_TIPO_A;
u8 dificultad  = DIFICULTAD_NORMAL;

u16 playerRemapJoy(u16 raw) {
    // Boton FISICO A/B/C -> boton LOGICO que espera el codigo (TYPE A).
    static const u16 dst[CONTROL_TIPOS][3] = {
        { BUTTON_A, BUTTON_B, BUTTON_C },   // TYPE A: identidad
        { BUTTON_B, BUTTON_C, BUTTON_A },   // TYPE B: A golpe, B salto, C especial
        { BUTTON_C, BUTTON_B, BUTTON_A },   // TYPE C: A salto, B golpe, C especial
    };
    if (controlTipo == CONTROL_TIPO_A || controlTipo >= CONTROL_TIPOS) return raw;
    u16 out = raw & (u16)~(BUTTON_A | BUTTON_B | BUTTON_C);
    if (raw & BUTTON_A) out |= dst[controlTipo][0];
    if (raw & BUTTON_B) out |= dst[controlTipo][1];
    if (raw & BUTTON_C) out |= dst[controlTipo][2];
    return out;
}

// Barras que saca un golpe de 'bars' segun la dificultad (ver player.h).
static u8 playerDiffBars(Player* p, u8 bars) {
    if (dificultad == DIFICULTAD_ARCADE || bars == 0 || bars >= PLAYER_MAX_HEALTH)
        return bars;
    u16 q = (u16)bars * NORMAL_DMG_NUM + p->dmgCarry;
    p->dmgCarry = (u8)(q % NORMAL_DMG_DEN);
    return (u8)(q / NORMAL_DMG_DEN);
}

static u8  s_persistLives[MAX_PLAYERS];
static u16 s_persistScore[MAX_PLAYERS];
static s16 s_persistHealth[MAX_PLAYERS];
// (01/10) El jugador termino el nivel FUERA de juego (game over sin continuar,
// o sin continues). Antes el nivel siguiente lo reinicializaba vivo: en 2P la
// tortuga muerta volvia a aparecer al pasar de pantalla.
static bool s_persistOut[MAX_PLAYERS];
// (20/10) Para la tabla de puntajes (hiscore.c): quien jugo esta partida, con
// que tortuga y su ultimo puntaje (se actualiza con cada punto, asi vale igual
// si la partida termina por game over o despues de los creditos).
static bool s_finalPlayed[MAX_PLAYERS];
static u8   s_finalChar[MAX_PLAYERS];
static u16  s_finalScore[MAX_PLAYERS];
static bool s_persistInit = FALSE;

static void persistEnsureInit(void) {
    if (s_persistInit) return;
    s_persistInit = TRUE;
    for (u8 i = 0; i < MAX_PLAYERS; i++) {
        s_persistLives[i]  = PLAYER_START_LIVES;
        s_persistScore[i]  = 0;
        s_persistHealth[i] = PLAYER_MAX_HEALTH;
        s_persistOut[i]    = FALSE;
    }
}

// (15/09) Antes esto era `return (joyId == JOY_2) ? 1 : 0;` y con el modo de 4
// TODOS los jugadores caian en el slot 0: el multitap les da JOY_3/JOY_4/JOY_5,
// ninguno es JOY_2. O sea que los cuatro compartian vidas, puntaje y barra, y
// al cambiar de nivel el ultimo en guardar le pisaba el estado a los otros
// tres. Ahora el slot sale del INDICE de jugador, resolviendo el joyId contra
// el mismo mapa que usa scenes.c (playerJoy), asi sirve con y sin multitap.
extern u16 playerJoy(u8 k);

static u8 persistSlot(u16 joyId) {
    for (u8 k = 0; k < MAX_PLAYERS; k++)
        if (playerJoy(k) == joyId) return k;
    return 0;
}

// ---------------------------------------------------------------------------
// DESPLAZAMIENTO POR FRAME (09/10) -- ver tools/gen_players_new.py
// ---------------------------------------------------------------------------
// Las hojas nuevas corren el dibujo dentro de la celda para que entre el arma:
// cada frame trae cuanto hay que correr el sprite en X para que el CUERPO
// quede quieto. La posicion "base" (sin el desplazamiento) se guarda en
// sprite->data junto con el personaje, porque la auto-animacion cambia el
// frame DENTRO de SPR_update: el callback de cambio de frame vuelve a ubicar
// el sprite antes de que se arme la tabla de sprites, asi no hay un frame
// corrido.
static s16 playerFrameDX(const Sprite* s, u16 ch) {
    s16 a = s->animInd, f = s->frameInd;
    if (ch >= PDX_CHARS || a < 0 || a >= PDX_ANIMS || f < 0 || f >= PDX_FRAMES) return 0;
    s16 dx = playerFrameDXTab[ch][a][f];
    return (s->attribut & TILE_ATTR_HFLIP_MASK) ? -dx : dx;
}

static void playerOnFrameChange(Sprite* s) {
    s16 base = (s16)(u16)(s->data & 0xFFFF);
    u16 ch   = (u16)(s->data >> 16);
    SPR_setPosition(s, base + playerFrameDX(s, ch), s->y - 0x80);
}

// Ubica el sprite de la tortuga: (sx, sy) = esquina del frame en pantalla.
void playerSetSpritePos(Player* p, s16 sx, s16 sy) {
    if (!p->sprite) return;
    p->sprite->data = ((u32)p->charIndex << 16) | (u16)sx;
    SPR_setPosition(p->sprite, sx + playerFrameDX(p->sprite, p->charIndex), sy);
}

// ---------------------------------------------------------------------------
// FUNCIÓN DE INICIALIZACIÓN
// ---------------------------------------------------------------------------
void initPlayer(Player* p, u8 selectedCharacter, u16 joyId, u8 palette, s16 startX, s16 startY) {
    const SpriteDefinition* spriteDef = &leo_player;

    switch(selectedCharacter) {
        case 0: spriteDef = &leo_player;  break;
        case 1: spriteDef = &mike_player; break;
        case 2: spriteDef = &don_player;  break;
        case 3: spriteDef = &raph_player; break;
    }
    // Indexa playerAtkReach (el alcance por frame medido sobre el arte).
    p->charIndex = (selectedCharacter < PHB_CHARS) ? selectedCharacter : 0;

    p->x             = startX;
    p->y             = startY;       // Pies sobre la vereda
    // Lane y pared del nivel 1 por defecto; los otros niveles las pisan con
    // setPlayerLane() / setPlayerEndWall() después de initPlayer().
    p->laneTop       = BOUND_LANE_TOP;
    p->laneBottom    = BOUND_LANE_BOTTOM;
    p->wallXTop      = LEVEL_END_WALL_X_TOP;
    p->wallXBottom   = LEVEL_END_WALL_X_BOTTOM;
    p->state         = STATE_IDLE;
    p->boundLeft     = 0;
    p->boundRight    = 288;
    p->cameraOffsetX = 0;
    p->comboStep     = 0;
    p->arcAtk        = 0;
    p->arcTarget     = NULL;
    p->comboBuffered = 0;
    p->comboLinger   = 0;
    p->airFrame      = 1;
    p->airTimer      = 0;
    p->attackIsSpecial = 0;
    p->specialTick   = 0;
    p->specialSpin   = 0;
    p->bcWindow      = 0;
    p->jumpVq        = 0;
    p->jumpZq        = 0;
    p->jumpZ         = 0;
    p->isJumpKicking = FALSE;
    p->kickCarry     = 0;
    p->joyId         = joyId;
    p->dmgCarry      = 0;
    p->prevJoy       = 0;
    p->dir           = 1;
    p->invincible    = 0;
    p->hurtTimer     = 0;
    p->hurtDir       = 0;
    p->hurtToggle    = 0;
    p->koTimer       = 0;
    p->kdPhase       = 0;
    p->kdTimer       = 0;
    p->kdFront       = 0;
    p->kdSlide       = 0;
    p->kdSlideTick   = 0;
    p->blinkTimer    = 0;
    p->gameOver      = FALSE;
    // Barra de vida: viene del estado persistente, igual que vidas y puntaje
    // (ver la nota de arriba). Red de seguridad: si quedo en 0 o basura, se
    // arranca con la barra llena en vez de con la tortuga muerta.
    persistEnsureInit();
    p->health        = s_persistHealth[persistSlot(joyId)];
    if (p->health <= 0 || p->health > PLAYER_MAX_HEALTH)
        p->health    = PLAYER_MAX_HEALTH;
    // Vidas y puntaje vienen del estado persistente entre niveles (no se
    // reinician al cambiar de nivel). El slot depende del joystick: JOY_1 ->
    // P1, JOY_2 -> P2.
    p->lives         = s_persistLives[persistSlot(joyId)];
    p->score         = s_persistScore[persistSlot(joyId)];
    {
        u8 fs = persistSlot(joyId);
        s_finalPlayed[fs] = TRUE;
        s_finalChar[fs]   = p->charIndex;
        s_finalScore[fs]  = p->score;
    }
    p->numAnims      = (u8)spriteDef->numAnimation;   // habilita anims nuevas si la sheet las tiene
    p->grabTimer     = 0;
    p->idleTimer     = 0;
    p->idleTwice     = 0;
    p->grabType      = GRAB_TYPE_WHIP;
    p->palIdx        = palette;
    p->hitBtnSpr     = NULL;     // (18/10) el motor de sprites ya se reinicio
    p->hitBtnT       = 0;
    p->heldFrame     = 0;
    p->heldTimer     = 0;
    p->heldHit       = 0;
    p->mhPhase       = 0;
    p->mhTimer       = 0;
    p->mhOutX        = 0;
    p->mhOutY        = 0;
    p->walkAccX      = 0;
    p->walkAccY      = 0;
    p->walkTick      = 0;
    p->kdArcade      = 0;
    p->kdIdx         = 0;
    p->kdT           = 0;
    p->kdAirT        = 0;
    p->kdAirTotal    = 0;
    p->koPhase       = 0;
    p->jumpT         = 0;

    p->sprite = SPR_addSprite(spriteDef, p->x, p->y, TILE_ATTR(palette, FALSE, FALSE, FALSE));
    // Las 4 tortugas comparten la misma paleta unificada (PAL1); la carga el
    // fade-in de nivel (levelFadeIn) al final del setup para no revelar los
    // sprites durante la carga. Ya está en CRAM cuando revive el jugador.
    //
    // (15/09) OJO CON EL NULL. SPR_addSprite devuelve NULL si no queda lugar en
    // el presupuesto del motor (SPR_initEx), cosa que con CUATRO tortugas de 64
    // tiles cada una pasa de verdad. Sin esta guarda se llamaba
    // SPR_setAnim(NULL) y a partir de ahi todo el modulo leia el "struct
    // Sprite" desde la direccion 0 -- que en la MegaDrive es ROM, asi que NO
    // crashea: devuelve basura. El sintoma es peor que un crash, porque
    // SPR_isAnimationDone() nunca da TRUE y los estados que esperan el fin de
    // una anim (HURT, ATTACKING) se cuelgan para siempre.
    if (p->sprite) {
        SPR_setAnim(p->sprite, ANIM_IDLE);
        p->sprite->data = ((u32)p->charIndex << 16) | (u16)p->x;
        SPR_setFrameChangeCallback(p->sprite, playerOnFrameChange);
    }

    // (01/10) Venia FUERA de juego del nivel anterior: arranca como quedo,
    // sin vidas, tirado (STATE_KO congelado) e INVISIBLE. El continue lo ve
    // con outCarried y lo deja afuera sin mostrar la cuenta; si quedan
    // continues puede volver con START (revivePlayer limpia esto).
    p->outCarried = FALSE;
    if (s_persistOut[persistSlot(joyId)]) {
        p->outCarried = TRUE;
        p->gameOver   = TRUE;
        p->lives      = 0;
        p->health     = 0;
        p->state      = STATE_KO;
        p->koTimer    = 0;
        if (p->sprite) SPR_setVisibility(p->sprite, HIDDEN);
    }
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

void setPlayerLane(Player* p, s16 top, s16 bottom) {
    p->laneTop    = top;
    p->laneBottom = bottom;
    if (p->y < top)    p->y = top;
    if (p->y > bottom) p->y = bottom;
}

void setPlayerEndWall(Player* p, s16 xTop, s16 xBottom) {
    p->wallXTop    = xTop;
    p->wallXBottom = xBottom;
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

// Setea un frame de la anim ANIM_HELD (agarre) con clamp al largo real de la
// animación. Las sheets viejas (Raph/Don) tienen el HELD "corrido" a otra fila
// con menos frames; sin el clamp, setear el frame 2/3 leería fuera de la anim.
static void setHeldFrame(Player* p, u8 frame) {
    u16 nf = p->sprite->animation->numFrame;
    if (nf == 0) return;
    if (frame >= nf) frame = (u8)(nf - 1);
    SPR_setAnimAndFrame(p->sprite, ANIM_HELD, frame);
}

// Pared diagonal del final del nivel: interpola linealmente entre los dos
// extremos calibrados (LEVEL_END_WALL_X_TOP/BOTTOM) según la profundidad
// 'y'. Devuelve la X de mundo del borde SÓLIDO para esa lane.
static s16 levelEndWallX(const Player* p, s16 y) {
    s16 laneRange = p->laneBottom - p->laneTop;
    if (laneRange <= 0) return p->wallXTop;
    s32 wallRange = p->wallXBottom - p->wallXTop;
    return p->wallXTop + (s16)(wallRange * (y - p->laneTop) / laneRange);
}

// ---------------------------------------------------------------------------
// LÓGICA PRINCIPAL — llamar una vez por frame para cada instancia
// ---------------------------------------------------------------------------
// (16/10) Patada voladora: "jump attack.wav" del arcade. CH3: en CH2 lo
// cortarian los golpes y CH1 es de las voces.
static void playJumpAttack(const Player* p) {
    (void)p;
    XGM2_playPCMEx(jump_attack_sfx, sizeof(jump_attack_sfx), SOUND_PCM_CH3, 14, FALSE, FALSE);
}

// Gruñido de ataque POR PERSONAJE (14/09). Leo (charIndex 0)
// y Raph (3) comparten un wav, Mike (1) y Don (2) el otro. Va en el canal PCM 3,
// el mismo donde estaba el "Attack!!" generico de los golpes normales: asi el
// golpe que conecta (hit_turtles, canal 2) se superpone en vez de cortarlo.
// El "Attack!!" (attack_turtles) queda SOLO para los especiales (A y B+C), que
// asi mantienen su acento propio.
// (16/10) Wavs reemplazados por los del arcade (ver audio.res).
static void playAttackGrunt(const Player* p) {
    if (p->charIndex == 0 || p->charIndex == 3)
        XGM2_playPCMEx(leo_raph_attack_vo, sizeof(leo_raph_attack_vo),
                       SOUND_PCM_CH2, 8, FALSE, FALSE);
    else
        XGM2_playPCMEx(mike_don_attack_vo, sizeof(mike_don_attack_vo),
                       SOUND_PCM_CH2, 8, FALSE, FALSE);
}

// ---------------------------------------------------------------------------
// ESPECIAL (26/09) — ver PLAYER_SPECIAL_* en player.h
// ---------------------------------------------------------------------------
// Duracion de cada frame de ANIM_SPECIAL en ticks. Las cuatro hojas tienen 5
// frames: preparacion (piso), tomar impulso, dos de giro con el caparazon de
// espaldas y el remate. El giro se estira un poco respecto del 5-5-5-5-5 del
// .res para que el swing se lea entero y el saltito tenga aire. Si una hoja
// trae mas frames, los que sobran duran como el ultimo.
// (01/10) Saltito mas alto (PLAYER_SPECIAL_HOP 32) y mas largo: los frames
// en el aire pasan de 6-6-6-7 a 8-8-8-9 (30 -> 38 ticks en total) para que
// el giro se lea entero antes de apoyar.
// (16/10) +4 ticks (38 -> 42) para que entre el sonido nuevo del especial
// (special_attack_sfx, 0,33 s): uno mas en cada frame del giro.
static const u8 specialFrameTicks[] = { 5, 9, 9, 9, 10 };
#define SPECIAL_TICK_TABLE ((u16)(sizeof(specialFrameTicks) / sizeof(specialFrameTicks[0])))
// (09/10) Hoja nueva: el giro son 7 frames y solo pega el 6to, el del arma
// estirada adelante (SPECIAL_HIT_FRAMES en tools/gen_player_hitbox.py). 30 ticks en
// total (con la tabla de arriba eran 56 y se veia muy lento); el frame del
// golpe dura un poco mas para que se lea.
// (16/10) +4 ticks (30 -> 34): el giro (frames 2 y 3) y el golpe (6to) un
// tick mas cada uno, y el remate otro, para acompanar special_attack_sfx.
static const u8 specialFrameTicksNew[] = { 4, 4, 5, 5, 4, 7, 5 };
#define SPECIAL_TICK_TABLE_NEW ((u16)(sizeof(specialFrameTicksNew) / sizeof(specialFrameTicksNew[0])))

// (20/10) Raph, boton A: el giro (ANIM_ATTACK_4 de su hoja). Todos los frames
// menos el ultimo ruedan; el ultimo es la patada y dura mas.
static bool playerHasRaphSpin(const Player* p) {
    return p->charIndex == 3 && p->numAnims > ANIM_ATTACK_4 && p->sprite;
}

static u16 specialAnimOf(const Player* p) {
    return p->specialSpin ? ANIM_ATTACK_4 : ANIM_SPECIAL;
}

static u16 specialTicksOf(const Player* p, u16 f) {
    if (p->specialSpin) {
        u16 nf = p->sprite->definition->animations[ANIM_ATTACK_4]->numFrame;
        return (f + 1 >= nf) ? PLAYER_RAPH_KICK_TICKS : PLAYER_RAPH_SPIN_TICKS;
    }
    if (playerHasNewSheet(p))
        return specialFrameTicksNew[(f < SPECIAL_TICK_TABLE_NEW) ? f : (SPECIAL_TICK_TABLE_NEW - 1)];
    return specialFrameTicks[(f < SPECIAL_TICK_TABLE) ? f : (SPECIAL_TICK_TABLE - 1)];
}

static u16 specialNumFrames(const Player* p) {
    u16 anim = specialAnimOf(p);
    if (!p->sprite || p->numAnims <= anim) return 1;
    return p->sprite->definition->animations[anim]->numFrame;
}

// Ticks acumulados hasta el comienzo del frame 'f'.
static u16 specialTicksBefore(const Player* p, u16 f) {
    u16 nf = specialNumFrames(p);
    u16 t = 0;
    for (u16 i = 0; i < f && i < nf; i++) t += specialTicksOf(p, i);
    return t;
}

// Altura del saltito en este tick: 0 durante la preparacion, arco
// parabolico desde que despega hasta el ultimo tick, 0 otra vez al apoyar.
static s16 specialHopZ(const Player* p) {
    if (p->specialSpin) return 0;          // el giro de Raph rueda por el piso
    u16 nf = specialNumFrames(p);
    s32 t0 = specialTicksBefore(p, PLAYER_SPECIAL_HOP_FROM);
    s32 tEnd = specialTicksBefore(p, nf);
    s32 t = p->specialTick;
    if (t <= t0 || t >= tEnd) return 0;
    s32 span = tEnd - t0;
    return (s16)((4 * PLAYER_SPECIAL_HOP * (t - t0) * (tEnd - t)) / (span * span));
}

// (12/10) PATINETA (Scene 6): la tortuga se dibuja esta cantidad de px mas
// arriba (parada sobre la tabla) y al moverse no camina (pose quieta).
s16 playerSkateLift = 0;

s16 playerDrawZ(const Player* p) {
    s16 z = (s16)(p->jumpZ + playerSkateLift);
    // (09/10) Cada pose del salto a la altura que tiene en el arcade.
    if (p->state == STATE_JUMPING && p->sprite && p->sprite->animInd == ANIM_JUMP &&
        p->charIndex < PDX_CHARS && p->sprite->frameInd >= 0 && p->sprite->frameInd < PDX_FRAMES)
        z += playerJumpDYTab[p->charIndex][p->sprite->frameInd];
    if (p->state == STATE_ATTACKING && p->attackIsSpecial)
        z += specialHopZ(p);
    return z;
}

static void startSpecial(Player* p) {
    p->state           = STATE_ATTACKING;
    p->comboStep       = 0;
    p->comboBuffered   = 0;
    p->comboLinger     = 0;
    p->attackIsSpecial = 1;
    p->specialTick     = 0;
    p->specialSpin     = 0;
    p->bcWindow        = 0;
    p->idleTimer       = 0;
    p->idleTwice       = 0;
    // Venga del piso o de los primeros frames de un salto (B+C tardio), el
    // especial arranca apoyado: el arco lo pone specialHopZ.
    p->jumpZ           = 0;
    p->jumpZq          = 0;
    p->jumpVq          = 0;
    p->isJumpKicking   = JUMPKICK_NONE;
    p->kickCarry       = 0;
    SPR_setAutoAnimation(p->sprite, FALSE);
    SPR_setAnimationLoop(p->sprite, FALSE);
    SPR_setAnimAndFrame(p->sprite, ANIM_SPECIAL, 0);
    // (16/10) "turtles special attack" del arcade (antes el "Attack!!" de
    // attack_turtles, que queda declarado pero sin uso).
    XGM2_playPCMEx(special_attack_sfx, sizeof(special_attack_sfx), SOUND_PCM_CH3, 15, FALSE, FALSE);
}

// (20/10) Raph, boton A: el giro con patada. Mismo estado que el especial
// (mata foot soldiers de un golpe, pega mientras dura con el alcance de cada
// frame), pero sin saltito y avanzando en X (ver specialStep).
static void startRaphSpin(Player* p) {
    startSpecial(p);
    p->specialSpin = 1;
    SPR_setAnimAndFrame(p->sprite, ANIM_ATTACK_4, 0);
}

// Un tick del especial. Devuelve TRUE cuando termino (ya quedo en IDLE).
static bool specialStep(Player* p, s16 effRight) {
    p->specialTick++;
    u16 nf  = specialNumFrames(p);
    u16 anim = specialAnimOf(p);
    u16 acc = 0;
    for (u16 f = 0; f < nf; f++) {
        acc += specialTicksOf(p, f);
        if (p->specialTick < acc) {
            if (p->sprite->frameInd != (s16)f || p->sprite->animInd != (s16)anim)
                SPR_setAnimAndFrame(p->sprite, (s16)anim, (s16)f);
            // El giro de Raph rueda hacia adelante; la patada final, quieta.
            if (p->specialSpin && f + 1 < nf)
                p->x = clampS16(p->x + p->dir * PLAYER_RAPH_SPIN_VX, p->boundLeft, effRight);
            return FALSE;
        }
    }
    p->state           = STATE_IDLE;
    p->attackIsSpecial = 0;
    p->specialTick     = 0;
    p->specialSpin     = 0;
    SPR_setAutoAnimation(p->sprite, TRUE);
    SPR_setAnimationLoop(p->sprite, TRUE);
    SPR_setAnim(p->sprite, ANIM_IDLE);
    return TRUE;
}

// ---------------------------------------------------------------------------
// COMBO DEL ARCADE (09/10) -- ver PLAYER_ARC_* en player.h
// ---------------------------------------------------------------------------
// Medido en el segundo video de gameplayARCADE/tmnt.avi (60 fps, un frame del
// arcade por frame de video). Cada golpe es una tabla de poses: frame de la
// anim y cuantos frames dura. Los frames van a mano (auto-animacion apagada)
// porque los tiempos cambian de pose a pose y la patada repite el frame 1.
enum { ARC_A1, ARC_A2, ARC_KICK, ARC_A3, ARC_A4, ARC_THROW, ARC_MOVES };
typedef struct { u8 frame, ticks; } AtkPose;
typedef struct { u8 anim, n; AtkPose pose[8]; } AtkSeq;

// Leo: los tiempos del video (frames 1881..2075, 1722..1790, 2088..2145,
// 4186..4245, 4330..4392).
static const AtkSeq atkLeo[ARC_MOVES] = {
    // corte en diagonal que termina de espaldas (16)
    { ANIM_ATTACK_1, 5, { {0,3}, {1,3}, {2,2}, {3,3}, {4,5} } },
    // corte horizontal (17)
    { ANIM_ATTACK_2, 6, { {0,3}, {1,3}, {2,2}, {3,2}, {4,3}, {5,4} } },
    // patada de espaldas: sube la pierna, la estira y la recoge (20)
    { ANIM_KICK,     4, { {0,4}, {1,4}, {2,8}, {1,4} } },
    // corte de arriba (16)
    { ANIM_ATTACK_3, 4, { {0,4}, {1,2}, {2,4}, {3,6} } },
    // estocada agachado (16)
    { ANIM_ATTACK_4, 4, { {0,4}, {1,3}, {2,3}, {3,6} } },
    // la llave: 6 poses de 4 (24). El soldier sale volando al empezar la 5ta.
    { ANIM_THROW,    6, { {0,4}, {1,4}, {2,4}, {3,4}, {4,4}, {5,4} } },
};
// Don: no hay video; sus filas tienen otra cantidad de frames, asi que los
// mismos totales repartidos entre sus poses.
static const AtkSeq atkDon[ARC_MOVES] = {
    { ANIM_ATTACK_1, 4, { {0,4}, {1,4}, {2,3}, {3,5} } },
    { ANIM_ATTACK_2, 7, { {0,3}, {1,3}, {2,2}, {3,2}, {4,3}, {5,2}, {6,3} } },
    { ANIM_KICK,     4, { {0,4}, {1,4}, {2,8}, {1,4} } },
    { ANIM_ATTACK_3, 5, { {0,3}, {1,2}, {2,3}, {3,3}, {4,5} } },
    { ANIM_ATTACK_4, 4, { {0,4}, {1,3}, {2,3}, {3,6} } },
    { ANIM_THROW,    7, { {0,4}, {1,4}, {2,3}, {3,3}, {4,4}, {5,3}, {6,3} } },
};

// (20/10) Mike, hoja nueva (rip 50887_mike.png): sus golpes tienen los
// mismos frames que los de Leo (5/6/4/4) y la llave 7, como la de Don.
static const AtkSeq atkMike[ARC_MOVES] = {
    { ANIM_ATTACK_1, 5, { {0,3}, {1,3}, {2,2}, {3,3}, {4,5} } },
    { ANIM_ATTACK_2, 6, { {0,3}, {1,3}, {2,2}, {3,2}, {4,3}, {5,4} } },
    { ANIM_KICK,     4, { {0,4}, {1,4}, {2,8}, {1,4} } },
    { ANIM_ATTACK_3, 4, { {0,4}, {1,2}, {2,4}, {3,6} } },
    { ANIM_ATTACK_4, 4, { {0,4}, {1,3}, {2,3}, {3,6} } },
    { ANIM_THROW,    7, { {0,4}, {1,4}, {2,3}, {3,3}, {4,4}, {5,3}, {6,3} } },
};

// (20/10) Raph, hoja nueva (rip 50923_RAPH.png): tres golpes con el sai
// (3/6/3 frames) y la patada de 4. Su "Attack" 4 del rip es el giro del
// especial (boton A), asi que el 4to golpe del combo repite la estocada
// agachado de ATTACK_1. Los totales, los de Leo.
static const AtkSeq atkRaph[ARC_MOVES] = {
    { ANIM_ATTACK_1, 3, { {0,4}, {1,5}, {2,7} } },
    { ANIM_ATTACK_2, 6, { {0,3}, {1,3}, {2,2}, {3,3}, {4,3}, {5,3} } },
    { ANIM_KICK,     4, { {0,4}, {1,4}, {2,8}, {3,4} } },
    { ANIM_ATTACK_3, 3, { {0,4}, {1,4}, {2,8} } },
    { ANIM_ATTACK_1, 3, { {0,4}, {1,5}, {2,7} } },
    { ANIM_THROW,    6, { {0,4}, {1,4}, {2,4}, {3,4}, {4,4}, {5,4} } },
};

static const AtkSeq* arcSeq(const Player* p, u8 move) {
    if (p->charIndex == 3 && playerHasNewSheet(p)) return atkRaph + move;
    if (p->charIndex == 2) return atkDon + move;
    if (p->charIndex == 1) return atkMike + move;
    return atkLeo + move;
}

static void arcSetPose(Player* p) {
    const AtkSeq* s = arcSeq(p, p->arcMove);
    SPR_setAnimAndFrame(p->sprite, s->anim, s->pose[p->arcIdx].frame);
}

static void arcStartMove(Player* p, u8 move) {
    p->arcAtk    = 1;
    p->arcMove   = move;
    p->arcIdx    = 0;
    p->arcT      = 0;
    p->arcHit    = (move == ARC_THROW);   // la llave siempre "conecto"
    p->arcBuf    = 0;
    p->arcLink   = 0;
    p->arcTarget = NULL;
    SPR_setAutoAnimation(p->sprite, FALSE);
    SPR_setAnimationLoop(p->sprite, FALSE);
    // La llave de Leo va ESPEJADA: en el rip le da la espalda al soldier y lo
    // tira por encima del hombro hacia donde mira. La de Don no: engancha al
    // soldier adelante con el bo y lo revolea por arriba hacia atras. Igual en
    // los dos el soldier esta adelante y sale volando para atras; la
    // direccion (p->dir) no cambia.
    // (20/10) Mike tampoco: engancha al soldier adelante (frame 0) y lo revolea.
    bool mirror = (move == ARC_THROW) && (p->charIndex != 2) && (p->charIndex != 1);
    SPR_setHFlip(p->sprite, mirror ? (p->dir > 0) : (p->dir < 0));
    arcSetPose(p);
    playAttackGrunt(p);
}

// Cortado desde afuera (golpe, agarre, KO...): deja el sprite derecho.
static void arcCut(Player* p) {
    if (p->arcAtk && p->sprite) {
        SPR_setHFlip(p->sprite, p->dir < 0);
        SPR_setAutoAnimation(p->sprite, TRUE);
    }
    p->arcAtk    = 0;
    p->arcTarget = NULL;
}

static void arcEnd(Player* p) {
    arcCut(p);
    p->state           = STATE_IDLE;
    p->comboStep       = 0;
    p->comboBuffered   = 0;
    p->comboLinger     = 0;
    p->attackIsSpecial = 0;
    SPR_setAnimationLoop(p->sprite, TRUE);
    SPR_setAnim(p->sprite, ANIM_IDLE);
}

// Golpe siguiente: la llave si el que recibio el golpe anterior esta pegado,
// si no el que toca en la cadena.
static void arcNext(Player* p) {
    p->arcStep++;
    p->comboStep = p->arcStep;
    void* t = p->arcTarget;
    if (t && enemyThrowStart(t, p)) {
        arcStartMove(p, ARC_THROW);
        return;
    }
    u8 move = ARC_A4;
    if (p->arcStep == 2) move = (random() & 1) ? ARC_A3 : ARC_KICK;
    arcStartMove(p, move);
}

static void arcComboStep(Player* p, u16 joy) {
    if (justPressed(joy, p->prevJoy, BUTTON_B)) p->arcBuf = 1;
    const AtkSeq* s = arcSeq(p, p->arcMove);
    if (p->arcIdx < s->n) {
        if (++p->arcT < s->pose[p->arcIdx].ticks) return;
        p->arcT = 0;
        if (p->arcIdx + 1 < s->n) {
            p->arcIdx++;
            arcSetPose(p);
            return;
        }
        p->arcIdx = s->n;            // termino: queda la ultima pose (no pega)
    }
    bool more = p->arcHit && p->arcStep < PLAYER_ARC_COMBO_LEN;
    if (more && p->arcBuf) { arcNext(p); return; }
    if (more && p->arcLink < PLAYER_ARC_LINK_F) { p->arcLink++; return; }
    arcEnd(p);
}

void playerAttackLanded(Player* p, void* target) {
    if (p->state != STATE_ATTACKING || !p->arcAtk) return;
    p->arcHit = 1;
    if (target) p->arcTarget = target;
}

bool playerIsThrowing(const Player* p) {
    return (p->state == STATE_ATTACKING && p->arcAtk && p->arcMove == ARC_THROW);
}

static void koArcadeStep(Player* p, s16 effRight);
static bool volteoStep(Player* p, s16 effRight, bool stopAtFloor);

// ---------------------------------------------------------------------------
// CAMINAR (09/10) -- ver PLAYER_WALK_* en player.h
// ---------------------------------------------------------------------------
// Movimiento de este frame segun el pad, en el piso y en el aire. Control
// arcade: velocidades Q8 por eje con acumulador (1,75 px/f sale 1-2-2-2).
// allowX = FALSE en el golpe en el aire (avanza solo, ver STATE_JUMPING).
// Devuelve TRUE si hay alguna direccion apretada (en *dyOut la vertical).
static bool walkStep(Player* p, u16 joy, s16 effRight, bool allowX, s8* dyOut) {
    s8 dx = 0, dy = 0;
    if (allowX) {
        if (joy & BUTTON_RIGHT) { dx =  1; SPR_setHFlip(p->sprite, FALSE); p->dir =  1; }
        if (joy & BUTTON_LEFT)  { dx = -1; SPR_setHFlip(p->sprite, TRUE);  p->dir = -1; }
    }
    if (joy & BUTTON_UP)   dy = -1;
    if (joy & BUTTON_DOWN) dy =  1;
    if (dyOut) *dyOut = dy;
    if (!dx) p->walkAccX = 0;
    if (!dy) p->walkAccY = 0;
    if (!dx && !dy) return FALSE;

    s16 mx, my;
    if (PLAYER_ARCADE_CONTROL) {
        bool diag = (dx != 0) && (dy != 0);
        if (dx) p->walkAccX += dx * (diag ? PLAYER_DIAG_X_Q : PLAYER_WALK_X_Q);
        if (dy) p->walkAccY += dy * (diag ? PLAYER_DIAG_Y_Q : PLAYER_WALK_Y_Q);
        mx = p->walkAccX / 256;  p->walkAccX -= mx * 256;
        my = p->walkAccY / 256;  p->walkAccY -= my * 256;
    } else {
        mx = dx * PLAYER_SPEED;
        my = dy * PLAYER_SPEED;
    }
    p->x = clampS16(p->x + mx, p->boundLeft, effRight);
    p->y = clampS16(p->y + my, p->laneTop, p->laneBottom);
    return TRUE;
}

// Pose de la caminata. Control arcade: frames a mano, PLAYER_WALK_TICKS cada
// uno (la auto-animacion del .res va a 5).
static void walkAnim(Player* p, u16 anim) {
    SPR_setAnimationLoop(p->sprite, TRUE);
    if (playerSkateLift) {            // (12/10) en patineta: se desliza parada
        if (!SPR_getAutoAnimation(p->sprite)) SPR_setAutoAnimation(p->sprite, TRUE);
        SPR_setAnim(p->sprite, ANIM_IDLE);
        return;
    }
    if (!PLAYER_ARCADE_CONTROL) {
        SPR_setAutoAnimation(p->sprite, TRUE);
        SPR_setAnim(p->sprite, anim);
        return;
    }
    u16 n = p->sprite->definition->animations[anim]->numFrame;
    if (n == 0) n = 1;
    // (09/10) En el arcade la caminata arranca por la ULTIMA pose (f7, el
    // paso corto) y sigue con f0, f1... (video 2, frames 1841, 2160, 3740).
    if (p->sprite->animInd != (s16)anim || SPR_getAutoAnimation(p->sprite))
        p->walkTick = (u8)((n - 1) * PLAYER_WALK_TICKS);
    SPR_setAutoAnimation(p->sprite, FALSE);
    if (p->walkTick >= n * PLAYER_WALK_TICKS) p->walkTick = 0;
    SPR_setAnimAndFrame(p->sprite, anim, p->walkTick / PLAYER_WALK_TICKS);
    p->walkTick++;
}

// ---------------------------------------------------------------------------
// (18/10) CARTEL "HIT BUTTONS": mientras un foot soldier la tiene agarrada por
// la espalda, arriba de la cabeza aparece hit_buttons (chars.res, 80x32, dos
// frames que se alternan). Va en la paleta de las tortugas (la misma linea
// que el jugador) y con prioridad alta, por delante del primer plano. El
// sprite existe solo mientras dura el agarre (40 tiles).
// ---------------------------------------------------------------------------
#define HITBTN_W        80
#define HITBTN_H        32
#define HITBTN_HEAD     66   // de los pies al borde de abajo del cartel
#define HITBTN_TICKS    10   // frames por cada uno de los dos cuadros

static void hitButtonsRelease(Player* p) {
    if (p->hitBtnSpr) { SPR_releaseSprite(p->hitBtnSpr); p->hitBtnSpr = NULL; }
}

static void hitButtonsUpdate(Player* p) {
    if (!playerIsFootGrabbed(p)) { hitButtonsRelease(p); return; }
    if (!p->hitBtnSpr) {
        p->hitBtnSpr = SPR_addSprite(&hit_buttons, -128, -128,
                                     TILE_ATTR(p->palIdx, TRUE, FALSE, FALSE));
        if (!p->hitBtnSpr) return;          // sin VRAM: sin cartel, el agarre sigue
        SPR_setAutoAnimation(p->hitBtnSpr, FALSE);
        SPR_setFrame(p->hitBtnSpr, 0);
        SPR_setDepth(p->hitBtnSpr, SPR_MIN_DEPTH);
        p->hitBtnT = 0;
    }
    if (++p->hitBtnT >= 2 * HITBTN_TICKS) p->hitBtnT = 0;
    SPR_setFrame(p->hitBtnSpr, (p->hitBtnT < HITBTN_TICKS) ? 0 : 1);
    s16 cx = (s16)(p->x + PLAYER_SPRITE_W / 2 - p->cameraOffsetX);
    SPR_setPosition(p->hitBtnSpr, (s16)(cx - HITBTN_W / 2),
                    (s16)(p->y - HITBTN_HEAD - HITBTN_H - stageCamY));
}

void updatePlayer(Player* p) {
    // (15/09) Guarda de sprite NULO: si el motor de sprites se quedo sin lugar
    // (pasa con 4 tortugas), este jugador no tiene sprite. Sin la guarda, todo
    // lo que sigue leeria el struct Sprite desde la direccion 0 (ROM) y
    // devolveria basura -- y los estados que esperan SPR_isAnimationDone se
    // colgarian para siempre. Sin sprite el jugador queda invisible e inerte,
    // pero el juego sigue.
    if (!p->sprite) { hitButtonsRelease(p); return; }
    // (18/10) Al principio, antes de cualquier return temprano de los estados:
    // asi el cartel se suelta siempre que termina el agarre.
    hitButtonsUpdate(p);

    u16 joy = playerRemapJoy(JOY_readJoypad(p->joyId));   // (10/10) CONTROL PAD de opciones

    // Límite derecho EFECTIVO de este frame: el menor entre el borde de
    // pantalla (dinámico, dado por la cámara) y la pared diagonal del
    // final del nivel en la profundidad actual (fija en coordenadas de
    // mundo). La pared está dibujada en PERSPECTIVA, no vertical, así que
    // este límite depende de 'y' — ver LEVEL_END_WALL_X_TOP/BOTTOM.
    // wallXTop == 0 -> el nivel no tiene pared diagonal (p.ej. el 2-1).
    s16 effRight = p->boundRight;
    if (p->wallXTop) {
        s16 wallRight = levelEndWallX(p, p->y) - PLAYER_SPRITE_W;
        if (wallRight < effRight) effRight = wallRight;
    }

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
            p->bcWindow = 0;   // la ventana B+C solo vive dentro del golpe/salto
            s8 moveY = 0;

            if (walkStep(p, joy, effRight, TRUE, &moveY)) {
                p->state = STATE_WALKING;
                // Reiniciar el timer de la pose de espera: hubo movimiento
                p->idleTimer = 0;
                p->idleTwice = 0;
                walkAnim(p, (moveY < 0) ? ANIM_WALK_BACK : ANIM_WALK_FRONT);
            } else {
                p->state = STATE_IDLE;
                // La caminata arcade va con los frames a mano. (22/10) Con la
                // hoja nueva la pose quieta tambien (ver abajo).
                if (!playerHasNewSheet(p) && !SPR_getAutoAnimation(p->sprite)) {
                    SPR_setAutoAnimation(p->sprite, TRUE);
                    SPR_setAnimationLoop(p->sprite, TRUE);
                    SPR_setAnim(p->sprite, ANIM_IDLE);
                }
                if (playerHasNewSheet(p)) {
                    // (22/10) Hoja nueva: la fila IDLE ya ES la animacion de
                    // espera (IDLE2 es igual cuadro por cuadro) y en loop la
                    // tortuga la hacia todo el tiempo. Ahora queda CLAVADA en
                    // la primera pose; tras PLAYER_IDLE_ANIM_DELAY cuadros
                    // quieta reproduce la animacion UNA vez y vuelve a la pose
                    // fija (y asi, cada PLAYER_IDLE_ANIM_DELAY).
                    if (p->idleTwice) {
                        if (SPR_isAnimationDone(p->sprite)) {
                            p->idleTwice = 0;
                            p->idleTimer = 0;
                            SPR_setAutoAnimation(p->sprite, FALSE);
                            SPR_setAnimAndFrame(p->sprite, ANIM_IDLE, 0);
                        }
                    } else if (++p->idleTimer >= PLAYER_IDLE_ANIM_DELAY) {
                        p->idleTimer = 0;
                        p->idleTwice = 1;
                        SPR_setAutoAnimation(p->sprite, TRUE);
                        SPR_setAnimationLoop(p->sprite, FALSE);
                        SPR_setAnimAndFrame(p->sprite, ANIM_IDLE2, 0);
                    } else if (p->sprite->animInd != ANIM_IDLE || p->sprite->frameInd != 0 ||
                               SPR_getAutoAnimation(p->sprite)) {
                        SPR_setAutoAnimation(p->sprite, FALSE);
                        SPR_setAnimAndFrame(p->sprite, ANIM_IDLE, 0);
                    }
                } else if (p->numAnims > ANIM_IDLE2) {
                    // Pose de espera nueva: tras PLAYER_IDLE_ANIM_DELAY frames
                    // quieto se reproduce ANIM_IDLE2 UNA vez (sin loop) y se
                    // vuelve a ANIM_IDLE. En las sheets viejas (Raph/Don) el
                    // índice 1 es otra cosa → quedan "corridas" (aceptado).
                    if (p->idleTwice) {
                        if (SPR_isAnimationDone(p->sprite)) {
                            p->idleTwice = 0;
                            p->idleTimer = 0;
                            SPR_setAnimationLoop(p->sprite, TRUE);
                            SPR_setAnim(p->sprite, ANIM_IDLE);
                        }
                    } else if (++p->idleTimer >= PLAYER_IDLE_ANIM_DELAY) {
                        p->idleTimer = 0;
                        p->idleTwice = 1;
                        SPR_setAnimationLoop(p->sprite, FALSE);
                        SPR_setAnimAndFrame(p->sprite, ANIM_IDLE2, 0);
                    } else {
                        SPR_setAnim(p->sprite, ANIM_IDLE);
                    }
                } else {
                    SPR_setAnim(p->sprite, ANIM_IDLE);
                }
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
                startSpecial(p);
            } else if (cJust) {
                // SALTO: la anim se controla a MANO por fases (subida/ápice/
                // aterrizaje), así que se apaga la auto-animación del sprite.
                // 'y' NO se toca al saltar: sigue siendo la lane real, y el
                // jugador puede seguir moviéndola en el aire (ver abajo).
                p->state     = STATE_JUMPING;
                // (28/09) Punto fijo: ver PLAYER_JUMP_V0_Q en player.h.
                p->jumpVq    = -PLAYER_JUMP_V0_Q;
                p->jumpZq    = 0;
                p->jumpZ     = 0;
                p->jumpT     = 0;
                if (PLAYER_ARCADE_CONTROL) {
                    // (09/10) Salto del arcade: ver PLAYER_AJ_* en player.h
                    p->jumpVq = -PLAYER_AJ_V_Q;
                    p->jumpZq = (s32)PLAYER_AJ_POP << PLAYER_JUMP_Q;
                    p->jumpZ  = PLAYER_AJ_POP;
                }
                p->airFrame  = 1;
                p->airTimer  = 0;
                p->idleTimer = 0;
                p->idleTwice = 0;
                p->bcWindow  = PLAYER_SPECIAL_BC_WINDOW;   // B tardio -> especial
                SPR_setAutoAnimation(p->sprite, FALSE);
                SPR_setAnimAndFrame(p->sprite, ANIM_JUMP, 0);
            } else if (bJust && PLAYER_ARCADE_CONTROL && playerHasNewSheet(p)) {
                // (09/10) Combo del arcade: el primer golpe es ATTACK_1 o
                // ATTACK_2 al azar (ver PLAYER_ARC_* en player.h).
                p->state           = STATE_ATTACKING;
                p->comboStep       = 1;
                p->comboBuffered   = 0;
                p->comboLinger     = 0;
                p->attackIsSpecial = 0;
                p->idleTimer       = 0;
                p->idleTwice       = 0;
                p->bcWindow        = PLAYER_SPECIAL_BC_WINDOW;   // C tardio -> especial
                p->arcStep         = 1;
                arcStartMove(p, (random() & 1) ? ARC_A2 : ARC_A1);
            } else if (bJust) {
                p->state         = STATE_ATTACKING;
                p->comboStep     = 1;
                p->comboBuffered = 0;
                p->comboLinger   = COMBO_LINK_WINDOW;
                p->attackIsSpecial = 0;
                p->idleTimer = 0;
                p->idleTwice = 0;
                p->bcWindow  = PLAYER_SPECIAL_BC_WINDOW;   // C tardio -> especial
                SPR_setAutoAnimation(p->sprite, TRUE);
                SPR_setAnimationLoop(p->sprite, FALSE);
                SPR_setAnimAndFrame(p->sprite, ANIM_ATTACK_1, 0);
                playAttackGrunt(p);
            } else if (justPressed(joy, p->prevJoy, BUTTON_A)) {
                // ESPECIAL (A): antes disparaba ANIM_KICK; el kick queda
                // reservado para otro uso futuro.
                // (20/10) Raph: el giro con patada (B+C sigue con el sai).
                if (playerHasRaphSpin(p)) startRaphSpin(p);
                else                      startSpecial(p);
            }
            break;
        }

        case STATE_ATTACKING: {
            // Especial: frames y saltito a mano (ver specialStep).
            if (p->attackIsSpecial) {
                specialStep(p, effRight);
                break;
            }
            // B+C con el C un poco tarde: el primer golpe del combo se
            // convierte en el especial.
            if (p->bcWindow > 0) {
                p->bcWindow--;
                if (p->comboStep == 1 && justPressed(joy, p->prevJoy, BUTTON_C)) {
                    arcCut(p);
                    startSpecial(p);
                    break;
                }
            }
            if (p->arcAtk) {
                arcComboStep(p, joy);
                break;
            }

            // BUFFER de input: un press de B en CUALQUIER momento del swing
            // queda guardado y encadena al terminar la anim. Antes solo valía
            // el press del frame exacto de fin de anim (ventana de 1 frame).
            if (justPressed(joy, p->prevJoy, BUTTON_B))
                p->comboBuffered = 1;

            // Swing todavía en curso (anims de ataque corren sin loop)
            if (!SPR_isAnimationDone(p->sprite))
                break;

            // Swing terminado: ¿se encadena el siguiente golpe del combo?
            // (09/10) La hoja nueva trae un 4to golpe (ANIM_ATTACK_4).
            u8 comboLen = playerHasNewSheet(p) ? PLAYER_COMBO_LEN_NEW : 3;
            bool canChain = (p->comboStep > 0 && p->comboStep < comboLen);

            if (canChain && p->comboBuffered) {
                p->comboStep++;
                p->comboBuffered = 0;
                p->comboLinger   = COMBO_LINK_WINDOW;
                SPR_setAnimAndFrame(p->sprite,
                                    (p->comboStep == 2) ? ANIM_ATTACK_2 :
                                    (p->comboStep == 3) ? ANIM_ATTACK_3 : ANIM_ATTACK_4, 0);
                playAttackGrunt(p);
            } else if (canChain && p->comboLinger > 0) {
                // Ventana de enlace: quedarse unos frames en la pose final
                // esperando el press que encadena
                p->comboLinger--;
            } else {
                p->state         = STATE_IDLE;
                p->comboStep     = 0;
                arcCut(p);
                p->comboBuffered = 0;
                p->comboLinger   = 0;
                p->attackIsSpecial = 0;
                SPR_setAnimationLoop(p->sprite, TRUE);   // restaurar loop normal
                SPR_setAnim(p->sprite, ANIM_IDLE);
            }
            break;
        }

        case STATE_JUMPING: {
            // B+C con el B un poco tarde: el salto recien empezado se
            // convierte en el especial (antes de que el B arme la patada).
            if (p->bcWindow > 0) {
                p->bcWindow--;
                if (!p->isJumpKicking && justPressed(joy, p->prevJoy, BUTTON_B)) {
                    startSpecial(p);
                    break;
                }
            }
            // jumpZ = altura VISUAL sobre el piso (crece al saltar, vuelve a
            // 0 al aterrizar). 'y' ya NO se toca acá: sigue siendo la lane
            // real de profundidad, libre de moverse con arriba/abajo.
            p->jumpZq -= p->jumpVq;
            p->jumpZ   = (s16)(p->jumpZq >> PLAYER_JUMP_Q);
            if (p->jumpT < 255) p->jumpT++;

            if (PLAYER_ARCADE_CONTROL) {
                // (09/10) Salto del arcade: recto hasta PLAYER_AJ_RISE_F,
                // despues gravedad pareja con tope (mas alto con el golpe).
                if (p->jumpT >= PLAYER_AJ_RISE_F) {
                    s32 cap = (s32)(p->isJumpKicking ? PLAYER_AJ_KICK_FALL_MAX
                                                     : PLAYER_AJ_FALL_MAX) << PLAYER_JUMP_Q;
                    p->jumpVq += PLAYER_AJ_GRAV_Q;
                    if (p->jumpVq > cap) p->jumpVq = cap;
                }
            } else
            // (28/09) Gravedad en punto fijo (Q8). Ver el bloque "Salto" de
            // player.h: subida frenada por la gravedad, apice con la gravedad
            // a la mitad (cuelga de forma continua, sin congelarse), caida
            // sin patada que acelera hasta la velocidad pareja de siempre y
            // caida con patada mas pesada, con su tope.
            if (p->isJumpKicking && p->jumpVq >= 0) {
                p->jumpVq += PLAYER_KICK_GRAV_Q;
                if (p->jumpVq > ((s32)PLAYER_KICK_FALL_MAX << PLAYER_JUMP_Q))
                    p->jumpVq = (s32)PLAYER_KICK_FALL_MAX << PLAYER_JUMP_Q;
            } else if (p->jumpVq > -PLAYER_APEX_BAND_Q && p->jumpVq < PLAYER_APEX_BAND_Q) {
                p->jumpVq += PLAYER_APEX_GRAV_Q;
            } else if (p->jumpVq < 0) {
                p->jumpVq += PLAYER_GRAVITY_Q;
            } else {
                p->jumpVq += PLAYER_FALL_ACCEL_Q;
                if (p->jumpVq > ((s32)PLAYER_FALL_SPEED << PLAYER_JUMP_Q))
                    p->jumpVq = (s32)PLAYER_FALL_SPEED << PLAYER_JUMP_Q;
            }

            // --- Movimiento en el aire (X e Y) ---
            // Como en el arcade: saltando se puede seguir reposicionando en
            // X Y TAMBIÉN en Y (la lane), no solo en X — más movilidad.
            // Con patada FUERTE la tortuga viaja SOLA con ímpetu en X (más
            // rápido que el control normal) y la trayectoria queda
            // comprometida: sin control manual en ese caso. Sin patada
            // (o con la débil), control aéreo normal en ambos ejes.
            if (PLAYER_ARCADE_CONTROL) {
                // (09/10) Arcade: con el golpe en el aire (con o sin
                // direccion) sale disparada hacia donde mira; arriba/abajo
                // siguen moviendo la lane. Sin golpe, el control de caminar.
                if (p->isJumpKicking) {
                    walkStep(p, joy, effRight, FALSE, NULL);
                    p->x = clampS16(p->x + p->dir * ((p->isJumpKicking == JUMPKICK_STRONG)
                                                     ? PLAYER_AIR_ATK_VX_DIR : PLAYER_AIR_ATK_VX),
                                    p->boundLeft, effRight);
                } else {
                    walkStep(p, joy, effRight, TRUE, NULL);
                }
            } else if (p->isJumpKicking == JUMPKICK_STRONG) {
                // Desplazamiento con ímpetu + el extra fraccional del
                // kickCarry (~+16px sobre el vuelo completo).
                p->kickCarry += PLAYER_JUMPKICK_EXTRA_Q;
                s16 extra = (s16)(p->kickCarry >> 4);
                p->kickCarry &= 0xF;
                p->x = clampS16(p->x + p->dir * (PLAYER_JUMPKICK_SPEED + extra),
                                p->boundLeft, effRight);
            } else {
                if (joy & BUTTON_RIGHT) { p->x = clampS16(p->x + PLAYER_SPEED, p->boundLeft, effRight); SPR_setHFlip(p->sprite, FALSE); p->dir = 1; }
                if (joy & BUTTON_LEFT)  { p->x = clampS16(p->x - PLAYER_SPEED, p->boundLeft, effRight); SPR_setHFlip(p->sprite, TRUE);  p->dir = -1; }
                if (joy & BUTTON_UP)    { p->y = clampS16(p->y - PLAYER_SPEED, p->laneTop, p->laneBottom); }
                if (joy & BUTTON_DOWN)  { p->y = clampS16(p->y + PLAYER_SPEED, p->laneTop, p->laneBottom); }
            }

            // --- Inicio de la patada en salto (una sola por salto) ---
            // Golpe solo            -> anteultimo frame de ANIM_JUMP_KICK (débil)
            // Golpe + direccion X   -> ultimo frame, con ímpetu (viaja más lejos)
            if (!p->isJumpKicking && (justPressed(joy, p->prevJoy, BUTTON_A) || justPressed(joy, p->prevJoy, BUTTON_B))) {
                if      (joy & BUTTON_RIGHT) { p->dir =  1; SPR_setHFlip(p->sprite, FALSE); }
                else if (joy & BUTTON_LEFT)  { p->dir = -1; SPR_setHFlip(p->sprite, TRUE);  }

                bool fuerte = (bool)(joy & (BUTTON_LEFT | BUTTON_RIGHT));
                // (09/10) Sin direccion y en lo alto del salto: el giro.
                bool giro = !fuerte && PLAYER_ARCADE_CONTROL && playerHasNewSheet(p) &&
                            p->jumpZ >= PLAYER_AJ_SPIN_MIN_Z;
                p->isJumpKicking = fuerte ? JUMPKICK_STRONG : (giro ? JUMPKICK_SPIN : JUMPKICK_SOFT);
                if (!giro) playJumpAttack(p);   // (16/10) el giro lleva el grunido
                // (09/10) Arcade: si todavia subia, el golpe la frena arriba.
                if (PLAYER_ARCADE_CONTROL && p->jumpVq < 0) p->jumpVq = 0;
                // Auto-anim ya está apagada desde el inicio del salto: el
                // frame elegido queda clavado hasta aterrizar.
                // (22/09) Las sheets no tienen todas los mismos frames: Leo y
                // Mike traen 2 (debil, fuerte) y Don y Raph 3, con una pose de
                // arranque (piernas recogidas) delante. Debil y fuerte son
                // SIEMPRE los dos ultimos; si hay un tercero se muestra primero
                // PLAYER_JUMPKICK_TUCK_TICKS y despues se estira la patada.
                // airTimer no se usa mientras se patea: hace de contador.
                if (giro) {
                    // El giro va a mano con los tiempos del especial del piso
                    // (specialTicksOf); airTimer cuenta los ticks. Solo pega
                    // en el frame del arma estirada (SPECIAL_HIT_FRAMES).
                    SPR_setAutoAnimation(p->sprite, FALSE);
                    SPR_setAnimationLoop(p->sprite, FALSE);
                    p->specialSpin = 0;     // el giro en el aire es el del arma
                    SPR_setAnimAndFrame(p->sprite, ANIM_SPECIAL, 0);
                    p->airTimer = 0;
                    playAttackGrunt(p);         // golpe normal: el grunido, no el "Attack!!"
                } else if (playerHasNewSheet(p)) {
                    // (09/10) Hoja nueva: cada golpe en el aire es su propia
                    // anim (sin direccion / con direccion), quedando en el
                    // ultimo frame hasta aterrizar. (22/10) Frames A MANO con
                    // PLAYER_AIRKICK_F0/F1_TICKS (ver el paso de mas abajo);
                    // airTimer cuenta los ticks.
                    SPR_setAutoAnimation(p->sprite, FALSE);
                    SPR_setAnimationLoop(p->sprite, FALSE);
                    p->airTimer = 0;
                    // (20/10) Raph: la patada voladora con direccion
                    // (JUMP_KICK_DIR) es tambien la de media altura; su
                    // JUMP_KICK queda para lo mas bajo del salto.
                    bool dirAnim = fuerte ||
                        (p->charIndex == 3 && p->jumpZ >= PLAYER_RAPH_LOW_KICK_Z);
                    SPR_setAnimAndFrame(p->sprite, dirAnim ? ANIM_JUMP_KICK_DIR : ANIM_JUMP_KICK, 0);
                } else {
                SPR_setAnim(p->sprite, ANIM_JUMP_KICK);
                {
                    u16 nk = p->sprite->animation->numFrame;
                    u16 target = (nk >= 2) ? (u16)(nk - (fuerte ? 1 : 2)) : 0;
                    if (nk >= 3) {
                        SPR_setFrame(p->sprite, 0);
                        p->airTimer = PLAYER_JUMPKICK_TUCK_TICKS;
                    } else {
                        SPR_setFrame(p->sprite, target);
                        p->airTimer = 0;
                    }
                }
                }
            } else if (p->isJumpKicking == JUMPKICK_SPIN && p->sprite->animInd == ANIM_SPECIAL) {
                // El giro: frames a mano; al terminar vuelve a los frames del
                // salto (ya no pega: ANIM_JUMP no tiene alcance).
                if (p->airTimer < 255) p->airTimer++;
                u16 nf  = specialNumFrames(p);
                u16 acc = 0, f = 0;
                for (f = 0; f < nf; f++) {
                    acc += specialTicksOf(p, f);
                    if (p->airTimer < acc) break;
                }
                if (f < nf) {
                    if (p->sprite->frameInd != (s16)f) SPR_setFrame(p->sprite, f);
                } else {
                    SPR_setAnimAndFrame(p->sprite, ANIM_JUMP, 0);
                }
            } else if (p->isJumpKicking && !playerHasNewSheet(p) && p->airTimer > 0) {
                if (--p->airTimer == 0) {
                    u16 nk = p->sprite->animation->numFrame;
                    SPR_setFrame(p->sprite,
                        nk - (p->isJumpKicking == JUMPKICK_STRONG ? 1 : 2));
                }
            } else if (p->isJumpKicking && playerHasNewSheet(p) &&
                       (p->sprite->animInd == ANIM_JUMP_KICK ||
                        p->sprite->animInd == ANIM_JUMP_KICK_DIR)) {
                // (22/10) Patada voladora de la hoja nueva, a mano: arranque,
                // poses intermedias cortas y la ultima (la patada estirada)
                // clavada hasta aterrizar.
                if (p->airTimer < 255) p->airTimer++;
                u16 nk = p->sprite->animation->numFrame;
                u16 t  = p->airTimer;
                u16 f  = (t < PLAYER_AIRKICK_F0_TICKS) ? 0
                       : (u16)(1 + (t - PLAYER_AIRKICK_F0_TICKS) / PLAYER_AIRKICK_F1_TICKS);
                if (nk == 0) nk = 1;
                if (f > nk - 1) f = (u16)(nk - 1);
                if (p->sprite->frameInd != (s16)f) SPR_setFrame(p->sprite, f);
            }

            // --- Fases de la animación del salto (solo si NO está pateando) ---
            // Subida: frame 0. Ápice y caída: loop del frame 1 al anteúltimo.
            // Justo antes de tocar el suelo (~2 frames de anticipación,
            // predicho con la velocidad actual): último frame.
            if ((!p->isJumpKicking ||
                 (p->isJumpKicking == JUMPKICK_SPIN && p->sprite->animInd == ANIM_JUMP)) &&
                playerHasNewSheet(p) && PLAYER_ARCADE_CONTROL) {
                // (09/10) Como el arcade: frame 0 los primeros
                // PLAYER_AJ_F0_F frames, cada pose de la vuelta
                // PLAYER_AJ_POSE_F frames y el ultimo hasta tocar el piso.
                u16 n = p->sprite->animation->numFrame;
                u16 f = 0;
                if (p->jumpT >= PLAYER_AJ_F0_F) {
                    f = 1 + (p->jumpT - PLAYER_AJ_F0_F) / PLAYER_AJ_POSE_F;
                    if (f > n - 1) f = n - 1;
                }
                SPR_setFrame(p->sprite, f);
            } else if (!p->isJumpKicking && playerHasNewSheet(p)) {
                // Control del port: frame 0 subiendo, la vuelta desde el
                // apice (3 frames por pose) y el ultimo antes de tocar el piso.
                u16 n = p->sprite->animation->numFrame;
                u16 f = 0;
                if (p->jumpVq < 0) {
                    p->airTimer = 0;
                } else if (p->jumpZq <= (p->jumpVq << 1)) {
                    f = n - 1;
                } else if (n > 2) {
                    f = 1 + p->airTimer / 3;
                    if (f > n - 2) f = n - 2;
                    if (p->airTimer < 255) p->airTimer++;
                }
                SPR_setFrame(p->sprite, f);
            } else if (!p->isJumpKicking) {
                u16 n = p->sprite->animation->numFrame;
                if (p->jumpVq < 0) {
                    SPR_setFrame(p->sprite, 0);
                } else if (p->jumpVq > 0 && p->jumpZq <= (p->jumpVq << 1)) {
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
            if (p->jumpZq <= 0) {
                p->jumpZ   = 0;
                p->jumpZq  = 0;
                p->jumpVq  = 0;
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
            if (PLAYER_ARCADE_CONTROL) { koArcadeStep(p, effRight); break; }
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

        case STATE_KNOCKED_DOWN: {
            if (p->kdArcade) {
                // (09/10) Volteo del control arcade
                if (volteoStep(p, effRight, FALSE)) {
                    p->kdArcade = 0;
                    p->state    = STATE_IDLE;
                    SPR_setAutoAnimation(p->sprite, TRUE);
                    SPR_setAnimationLoop(p->sprite, TRUE);
                    SPR_setAnim(p->sprite, ANIM_IDLE);
                }
                break;
            }
            // Derribo: la secuencia de anims va a mano — de frente HIT_3 (13),
            // de espaldas HIT_BEHIND_1 (15) → HIT_BEHIND_2 (16) — y después
            // tirada en el piso un momento → GET_UP_1/2 → IDLE.
            //
            // ARRASTRE: las dos anims de caída son golpes potentes que dan por
            // sentado que el personaje VIAJA, pero el arte no lleva ese avance
            // adentro (ver PLAYER_KD_SLIDE_* en player.h). Así que el motor lo
            // desplaza mientras CAE (fases 0 y 1), con una velocidad que decae
            // hasta frenar: al tocar el piso ya no se mueve más.
            if (p->kdSlide > 0 && p->kdPhase <= 1) {
                p->x = clampS16(p->x + p->hurtDir * p->kdSlide,
                                p->boundLeft, effRight);
                if (p->kdSlideTick > 0) p->kdSlideTick--;
                else {
                    p->kdSlide--;
                    p->kdSlideTick = PLAYER_KD_SLIDE_DECAY;
                }
            }
            switch (p->kdPhase) {
                case 0:   // retroceso terminado → caída de espaldas
                    if (SPR_isAnimationDone(p->sprite)) {
                        SPR_setAnimAndFrame(p->sprite, ANIM_HIT_BEHIND_2, 0);
                        p->kdPhase = 1;
                    }
                    break;
                case 1:   // cayó (último frame = tirada) → esperar en el piso
                    if (SPR_isAnimationDone(p->sprite)) {
                        p->kdPhase = 2;
                        p->kdTimer = PLAYER_KD_HOLD_FRAMES;
                        p->kdSlide = 0;   // en el piso ya no se arrastra
                    }
                    break;
                case 2:   // tirada; al terminar, se levanta
                    if (p->kdTimer > 0) {
                        p->kdTimer--;
                    } else {
                        p->kdPhase = 3;
                        // Cada caída tiene SU levantada: de frente GET_UP_1,
                        // de espaldas GET_UP_2. Sheets viejas sin esas anims:
                        // se levanta directo.
                        u16 getUp = p->kdFront ? ANIM_GET_UP_1 : ANIM_GET_UP_2;
                        if (p->numAnims > getUp)
                            SPR_setAnimAndFrame(p->sprite, getUp, 0);
                        else
                            p->kdPhase = 4;
                    }
                    break;
                case 3:   // levantándose
                    if (SPR_isAnimationDone(p->sprite))
                        p->kdPhase = 4;
                    break;
                default:  // 4 = listo → volver a jugable
                    p->state = STATE_IDLE;
                    SPR_setAnimationLoop(p->sprite, TRUE);
                    SPR_setAnim(p->sprite, ANIM_IDLE);
                    break;
            }
            break;
        }

        case STATE_GRABBED: {
            // Agarrado por el látigo del robot o por la espalda (foot soldier).
            // Se ZAFA masheando A/B/C (el metro grabTimer baja con cada press);
            // el robot además drena vida mientras agarra (playerElectroDrain).
            // La liberación por mash la hace playerReleaseGrab (i-frames para
            // que no lo vuelvan a agarrar en el acto).
            u16 mash = 0;
            if (justPressed(joy, p->prevJoy, BUTTON_A)) mash += PLAYER_GRAB_MASH_STEP;
            if (justPressed(joy, p->prevJoy, BUTTON_B)) mash += PLAYER_GRAB_MASH_STEP;
            if (justPressed(joy, p->prevJoy, BUTTON_C)) mash += PLAYER_GRAB_MASH_STEP;

            // (18/10) Mordida del Mouser: cada boton sacude el brazo un frame
            // (0 -> 1 -> 2 -> 0...). La anim no corre sola: va al ritmo del
            // jugador. Se hace ANTES del zafe para que el ultimo toque se vea.
            if (p->grabType == GRAB_TYPE_MOUSER && mash > 0) {
                p->heldFrame = (u8)((p->heldFrame + 1) % 3);
                if (playerHasBiteAnim(p))
                    SPR_setAnimAndFrame(p->sprite, ANIM_MOUSER_BITE, p->heldFrame);
                else
                    setHeldFrame(p, p->heldFrame);
            }

            if (mash > 0 && p->grabTimer > 0) {
                p->grabTimer = (p->grabTimer > mash) ? (u8)(p->grabTimer - mash) : 0;
                if (p->grabTimer == 0) {
                    playerReleaseGrab(p);
                    break;
                }
            }

            if (p->grabType == GRAB_TYPE_FOOT) {
                // Agarre por la espalda (foot soldier): anim HELD a MANO. El
                // loop 0→1→2 da vida a la pose de "sostenido"; tras recibir un
                // golpe (damagePlayer) se muestra el frame 3 (golpe en pleno
                // agarre) PLAYER_HELD_HIT_FRAMES frames y se vuelve al loop.
                if (p->heldHit > 0) {
                    if (--p->heldHit == 0)
                        setHeldFrame(p, 0);
                } else if (++p->heldTimer >= PLAYER_HELD_LOOP_TICKS) {
                    p->heldTimer = 0;
                    p->heldFrame = (u8)((p->heldFrame + 1) % 3);
                    setHeldFrame(p, p->heldFrame);
                }
            }
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
    s16 drawY = p->y - PLAYER_FOOT_OFFSET - playerDrawZ(p);
    playerSetSpritePos(p, p->x - p->cameraOffsetX, drawY - stageCamY);

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
    // El especial corre con la auto-animacion apagada (ver specialStep):
    // pega mientras dura, el alcance de cada frame lo pone attackReachNow.
    if (p->state == STATE_ATTACKING && p->attackIsSpecial)
        return TRUE;
    // Combo del arcade: pega mientras corren las poses (no en la espera del
    // final ni en la llave, que no tiene alcance).
    if (p->state == STATE_ATTACKING && p->arcAtk)
        return (p->arcMove != ARC_THROW && p->arcIdx < arcSeq(p, p->arcMove)->n);
    if (p->state == STATE_ATTACKING)
        return !SPR_isAnimationDone(p->sprite);
    // Patada en salto: activa todo el tiempo que dura el vuelo con la patada
    if (p->state == STATE_JUMPING && p->isJumpKicking)
        return TRUE;
    return FALSE;
}

bool playerAttackHits(const Player* p, s16 targetCX, s16 targetFeetY) {
    // Objetivo PUNTUAL (shuriken en vuelo, bala del jefe): sin cuerpo ni
    // altura, alcanza con el maximo del frame.
    return playerAttackHitsBox(p, targetCX, targetFeetY, 0, 0);
}

// Alcance del golpe EN ESTE FRAME: hasta donde llega el pixel opaco mas
// adelantado del frame que se esta dibujando, medido desde el centro del frame
// de 104x104 (ver player_hitbox.h). Sirve igual mirando a izquierda o derecha:
// el arte siempre mira a la derecha y el motor lo espeja.
//
// Como el valor sale de la animacion en curso, el hitbox "acompaña" al arma: en
// los frames de preparacion el golpe no llega, y recien conecta cuando el arma
// esta de verdad extendida. Vale para el combo, la patada, el especial y la
// patada en salto sin distinguir casos: todos son animaciones de la misma hoja.
static s16 attackReachNow(const Player* p, s16 targetFeetY, s16 targetBodyH) {
    if (!p->sprite) return PHB_NONE;
    s16 a = p->sprite->animInd;
    s16 f = p->sprite->frameInd;
    if (a < 0 || a >= PHB_ANIMS || f < 0 || f >= PHB_FRAMES) return PHB_NONE;
    s8 slot = phbSlotOfAnim[a];
    if (slot < 0) return PHB_NONE;          // esta animacion no golpea

    // Objetivo puntual (shuriken, bala): no hay cuerpo con el que cruzar
    // franjas, se usa el maximo del frame.
    if (targetBodyH <= 0)
        return (s16)playerAtkReachMax[p->charIndex][slot][f];

    // Tope del frame del jugador TAL COMO SE DIBUJA: 'y' son los pies y jumpZ
    // lo levanta. Por eso saltar corre las franjas hacia arriba y una patada
    // en el aire deja de tocar al enemigo que quedo abajo.
    s16 top  = p->y - PLAYER_FOOT_OFFSET - playerDrawZ(p);
    s16 tTop = targetFeetY - targetBodyH;   // tope del cuerpo del objetivo

    const s8* bands = playerAtkReach[p->charIndex][slot][f];
    s16 best = PHB_NONE;
    for (u16 k = 0; k < PHB_BANDS; k++) {
        s16 bTop = top + (s16)(k * PHB_BAND);
        if (bTop + (PHB_BAND - 1) < tTop) continue;  // franja arriba del cuerpo
        if (bTop > targetFeetY) break;               // ya paso los pies
        if (bands[k] > best) best = bands[k];
    }
    return best;
}

bool playerAttackHitsBox(const Player* p, s16 targetCX, s16 targetFeetY,
                         s16 targetHalfW, s16 targetBodyH) {
    return playerAttackHitsBand(p, targetCX, targetFeetY, targetHalfW,
                                (s16)(-targetBodyH), 0);
}

s16 getPlayerHurtCX(const Player* p) {
    s16 c = (s16)(p->x + PLAYER_SPRITE_W / 2);
    return (s16)((p->dir >= 0) ? c + PLAYER_HURT_DX : c - PLAYER_HURT_DX);
}

s16 getPlayerHurtX(const Player* p) {
    return (s16)(getPlayerHurtCX(p) - PLAYER_SPRITE_W / 2);
}

// (21/10) Jefes: la lane tiene que estar dentro de SU alcance (ver player.h).
static bool bossLaneOk(const Player* p, s16 targetFeetY, s16 tolUp, s16 tolDown) {
    s16 dy = (s16)(p->y - targetFeetY);
    return (dy >= -tolUp && dy <= tolDown);
}

bool playerAttackHitsBoss(const Player* p, s16 targetCX, s16 targetFeetY,
                          s16 targetHalfW, s16 targetBodyH, s16 tolUp, s16 tolDown) {
    if (!bossLaneOk(p, targetFeetY, tolUp, tolDown)) return FALSE;
    return playerAttackHitsBox(p, targetCX, targetFeetY, targetHalfW, targetBodyH);
}

bool playerAttackHitsBossBand(const Player* p, s16 targetCX, s16 targetFeetY,
                              s16 targetHalfW, s16 yTop, s16 yBot, s16 tolUp, s16 tolDown) {
    if (!bossLaneOk(p, targetFeetY, tolUp, tolDown)) return FALSE;
    return playerAttackHitsBand(p, targetCX, targetFeetY, targetHalfW, yTop, yBot);
}

bool playerAttackHitsBand(const Player* p, s16 targetCX, s16 targetFeetY,
                          s16 targetHalfW, s16 yTop, s16 yBot) {
    if (!isPlayerAttackActive(p))
        return FALSE;

    // Alcance de ESTE frame a la ALTURA de ESTE objetivo. PHB_NONE = el arte
    // del jugador no tiene un solo pixel en la franja del cuerpo del enemigo
    // (patada en salto por encima de la cabeza, por ejemplo). (02/10) La
    // franja va de pies+yTop a pies+yBot (antes siempre terminaba en los pies).
    s16 reach = attackReachNow(p, (s16)(targetFeetY + yBot), (s16)(yBot - yTop));
    if (reach == PHB_NONE)
        return FALSE;
    reach += PLAYER_ATK_SLACK;

    // Alcance horizontal medido desde el CENTRO del frame, hacia adelante.
    // (El código anterior medía desde el borde izquierdo: pegando a la
    // derecha la ventana cubría -12..+28px del centro — el golpe pegaba
    // "arriba" de la tortuga y nunca adelante, donde frenan los enemigos.)
    s16 pcx = p->x + PLAYER_SPRITE_W / 2;
    s16 dx  = (p->dir >= 0) ? (targetCX - pcx) : (pcx - targetCX);
    // Solape horizontal entre la caja del ataque [-ATK_BACK, +reach] y la
    // hurtbox del objetivo [dx-halfW, dx+halfW]: el golpe conecta si toca el
    // CUERPO, no sólo si el centro entra en alcance (objetivo puntual con
    // halfW = 0: shurikens, balas del jefe).
    if (dx - targetHalfW > reach)
        return FALSE;
    if (dx + targetHalfW < -PLAYER_ATK_BACK)
        return FALSE;

    // Profundidad: tolerancia SIMÉTRICA alrededor del lane del jugador.
    // 'y' es siempre la lane real (también en el aire: jumpZ es un offset
    // solo visual y no la toca), así que no hace falta ningún caso especial.
    if (absPS16(targetFeetY - p->y) > PLAYER_ATK_TOL_Y)
        return FALSE;

    playerAttackLanded((Player*)p, NULL);   // combo arcade: habilita el siguiente
    return TRUE;
}

// (26/09) Objetivo que VUELA (la nave de Baxter): no tiene lane, asi que no
// se aplica la tolerancia de profundidad. Solo cuenta el alcance del frame del
// jugador a la ALTURA del objetivo (franjas [top..bot] de mundo contra el arte
// tal como se dibuja, con el salto y el saltito del especial incluidos): la
// tortuga en el piso le pega cuando la nave baja, y saltando cuando va alta.
bool playerAttackHitsFlying(const Player* p, s16 targetCX, s16 top, s16 bot,
                            s16 targetHalfW) {
    if (!isPlayerAttackActive(p))
        return FALSE;
    s16 reach = attackReachNow(p, bot, (s16)(bot - top));
    if (reach == PHB_NONE)
        return FALSE;
    reach += PLAYER_ATK_SLACK;
    s16 pcx = p->x + PLAYER_SPRITE_W / 2;
    s16 dx  = (p->dir >= 0) ? (targetCX - pcx) : (pcx - targetCX);
    if (dx - targetHalfW > reach)
        return FALSE;
    if (dx + targetHalfW < -PLAYER_ATK_BACK)
        return FALSE;
    playerAttackLanded((Player*)p, NULL);
    return TRUE;
}

bool isPlayerSpecialAttack(const Player* p) {
    return (p->state == STATE_ATTACKING && p->attackIsSpecial);
}

// TRUE si la tortuga está en el aire ejecutando la patada con salto (suave o
// fuerte). Sirve para dispararle un SFX de impacto sólo cuando el golpe que
// conecta es la patada aérea, no el combo de tierra ni el especial.
bool isPlayerJumpKicking(const Player* p) {
    return (p->state == STATE_JUMPING && p->isJumpKicking != JUMPKICK_NONE);
}

// En el aire saltando (con o sin patada voladora). La usa Rocksteady para
// apuntar sus balas: jugador en el piso → tiro recto, saltando → hacia arriba.
bool isPlayerJumping(const Player* p) {
    return (p->state == STATE_JUMPING);
}

s16 getPlayerJumpZ(const Player* p) {
    return p->jumpZ;
}

// (16/09) Dejarse caer desde una plataforma hasta una lane MAS ADELANTE (Y
// mayor). Nivel 2-1: la cornisa de los portones esta 78px por detras de la
// calle, y al caminar hacia abajo la tortuga se tira a la vereda.
//
// El truco es el mismo que usa el salto: 'y' es PROFUNDIDAD y jumpZ es una
// altura VISUAL. Se mueve 'y' de golpe a la lane de destino y se le suma a
// jumpZ exactamente esa diferencia, asi el sprite NO se teletransporta (queda
// dibujado donde estaba) y despues cae solo con la gravedad del salto hasta
// que jumpZ vuelve a 0. (29/09) Arranca con un saltito (PLAYER_DROP_HOP_Q):
// sube ~3 px, cuelga un instante y cae acelerando hasta PLAYER_FALL_SPEED.
// Antes arrancaba ya cayendo y la bajada se sentia brusca.
void playerFallTo(Player* p, s16 newFeetY) {
    if (!p->sprite || p->gameOver) { p->y = newFeetY; return; }
    s16 drop = newFeetY - p->y;
    if (drop <= 0) { p->y = newFeetY; return; }
    p->y      = newFeetY;
    p->jumpZ  += drop;
    p->jumpZq  = (s32)p->jumpZ << PLAYER_JUMP_Q;
    // (29/09) Parado: se baja con un SALTITO (PLAYER_DROP_HOP_Q hacia arriba)
    // en vez de caer a plomo. Ya en el aire: sigue con la velocidad que traia.
    if (p->state != STATE_JUMPING) p->jumpVq = -PLAYER_DROP_HOP_Q;
    else if (p->jumpVq < PLAYER_APEX_BAND_Q) p->jumpVq = PLAYER_APEX_BAND_Q;
    if (p->state != STATE_JUMPING) {
        p->state         = STATE_JUMPING;
        p->isJumpKicking = JUMPKICK_NONE;
        p->airFrame      = 1;
        p->airTimer      = 0;
        p->jumpT         = PLAYER_AJ_LATE_T;
        SPR_setAutoAnimation(p->sprite, FALSE);
        SPR_setAnimationLoop(p->sprite, FALSE);
        SPR_setAnimAndFrame(p->sprite, ANIM_JUMP, 0);
    }
}

s8 getPlayerDir(const Player* p) {
    return p->dir;
}

s16 getPlayerY(const Player* p) {
    return p->y;
}

// ---------------------------------------------------------------------------
// VOLTEO (09/10) -- golpe recibido del control arcade, ver PLAYER_KD_* en
// player.h. Cada paso es un frame de una anim, cuantos frames dura y si va
// en el aire (los pasos en el aire arman la parabola).
// ---------------------------------------------------------------------------
typedef struct { u8 anim, frame, ticks, air; } KdStep;

// Leo, hoja nueva: HIT_3 f0-2 volando, f3-8 en el piso, GET_UP_1 levantandose
// (los tiempos del volteo medido en el video, Pruebas_TMNT_Control).
static const KdStep kdLeoNew[] = {
    { ANIM_HIT_3, 0, 3, 1 }, { ANIM_HIT_3, 1, 8, 1 }, { ANIM_HIT_3, 2, 7, 1 },
    { ANIM_HIT_3, 3, 3, 0 }, { ANIM_HIT_3, 4, 3, 0 }, { ANIM_HIT_3, 5, 4, 0 },
    { ANIM_HIT_3, 6, 3, 0 }, { ANIM_HIT_3, 7, 3, 0 }, { ANIM_HIT_3, 8, 3, 0 },
    { ANIM_GET_UP_1, 0, 4, 0 }, { ANIM_GET_UP_1, 1, 4, 0 },
};
// Don, hoja nueva: su HIT_3 cae antes (el f2 ya esta tirado de espaldas), asi
// que vuela con f0-1 el mismo tiempo total (18 frames).
static const KdStep kdDonNew[] = {
    { ANIM_HIT_3, 0, 3, 1 }, { ANIM_HIT_3, 1, 15, 1 },
    { ANIM_HIT_3, 2, 3, 0 }, { ANIM_HIT_3, 3, 3, 0 }, { ANIM_HIT_3, 4, 4, 0 },
    { ANIM_HIT_3, 5, 3, 0 }, { ANIM_HIT_3, 6, 3, 0 }, { ANIM_HIT_3, 7, 3, 0 },
    { ANIM_GET_UP_1, 0, 4, 0 }, { ANIM_GET_UP_1, 1, 4, 0 },
};
// (20/10) Mike, hoja nueva: como Don, vuela con f0-1 (los dos del rip ya en
// el aire) el mismo tiempo total; despues 6 en el piso y GET_UP_1 de 3.
static const KdStep kdMikeNew[] = {
    { ANIM_HIT_3, 0, 3, 1 }, { ANIM_HIT_3, 1, 15, 1 },
    { ANIM_HIT_3, 2, 3, 0 }, { ANIM_HIT_3, 3, 3, 0 }, { ANIM_HIT_3, 4, 3, 0 },
    { ANIM_HIT_3, 5, 4, 0 }, { ANIM_HIT_3, 6, 3, 0 }, { ANIM_HIT_3, 7, 3, 0 },
    { ANIM_GET_UP_1, 0, 3, 0 }, { ANIM_GET_UP_1, 1, 3, 0 }, { ANIM_GET_UP_1, 2, 3, 0 },
};
// Hojas viejas (Raph en el modo de 4): HIT_3 de 6 + GET_UP_1 de 4.
static const KdStep kdOld[] = {
    { ANIM_HIT_3, 0, 3, 1 }, { ANIM_HIT_3, 1, 8, 1 }, { ANIM_HIT_3, 2, 7, 1 },
    { ANIM_HIT_3, 3, 4, 0 }, { ANIM_HIT_3, 4, 4, 0 }, { ANIM_HIT_3, 5, 5, 0 },
    { ANIM_GET_UP_1, 0, 4, 0 }, { ANIM_GET_UP_1, 1, 4, 0 },
    { ANIM_GET_UP_1, 2, 4, 0 }, { ANIM_GET_UP_1, 3, 4, 0 },
};
#define KD_N(t) ((u8)(sizeof(t) / sizeof(t[0])))

static const KdStep* kdTable(const Player* p, u8* n) {
    if (playerHasNewSheet(p)) {
        // (20/10) Raph: su HIT_3 es como el de Don (8 + levantada de 2).
        if (p->charIndex == 2 || p->charIndex == 3) { *n = KD_N(kdDonNew); return kdDonNew; }
        if (p->charIndex == 1) { *n = KD_N(kdMikeNew); return kdMikeNew; }
        *n = KD_N(kdLeoNew); return kdLeoNew;
    }
    *n = KD_N(kdOld); return kdOld;
}

static void kdShow(Player* p) {
    u8 n;
    const KdStep* k = &kdTable(p, &n)[p->kdIdx];
    u16 nf = p->sprite->definition->animations[k->anim]->numFrame;
    u16 f = (k->frame < nf) ? k->frame : (u16)(nf - 1);
    SPR_setAnimAndFrame(p->sprite, k->anim, f);
}

// Arranca el volteo. 'side' = de que lado esta quien pego (+1 a la derecha):
// la tortuga se da vuelta hacia el y sale despedida hacia el otro lado.
static void startVolteo(Player* p, s8 side) {
    u8 n;
    const KdStep* t = kdTable(p, &n);
    u16 air = 0;
    for (u8 i = 0; i < n; i++) if (t[i].air) air += t[i].ticks;
    p->dir        = side;
    p->hurtDir    = -side;
    SPR_setHFlip(p->sprite, side < 0);
    p->kdIdx      = 0;
    p->kdT        = 0;
    p->kdAirT     = 0;
    p->kdAirTotal = (u8)(air ? air : 1);
    p->jumpZ      = 0;
    p->jumpZq     = 0;
    p->jumpVq     = 0;
    p->isJumpKicking = JUMPKICK_NONE;
    p->kickCarry  = 0;
    p->hurtTimer  = 0;
    SPR_setAutoAnimation(p->sprite, FALSE);
    SPR_setAnimationLoop(p->sprite, FALSE);
    kdShow(p);
}

static u16 volteoTotal(const Player* p) {
    u8 n;
    const KdStep* t = kdTable(p, &n);
    u16 tot = 0;
    for (u8 i = 0; i < n; i++) tot += t[i].ticks;
    return tot;
}

// Un frame del volteo. TRUE al terminar. stopAtFloor (muerte): corta al tocar
// el piso, despues viene la pose de knockeada.
static bool volteoStep(Player* p, s16 effRight, bool stopAtFloor) {
    u8 n;
    const KdStep* t = kdTable(p, &n);
    const KdStep* k = &t[p->kdIdx];
    if (k->air) {
        s32 tt = ++p->kdAirT, T = p->kdAirTotal;
        p->jumpZ = (s16)((4 * PLAYER_KD_Z * tt * (T - tt)) / (T * T));
        p->x = clampS16(p->x + p->hurtDir * PLAYER_KD_VX, p->boundLeft, effRight);
    } else {
        p->jumpZ = 0;
        if (p->kdT < PLAYER_KD_ROLL_F && p->kdIdx > 0 && t[p->kdIdx - 1].air)
            p->x = clampS16(p->x + p->hurtDir, p->boundLeft, effRight);   // rueda al caer
    }
    if (++p->kdT >= k->ticks) {
        p->kdT = 0;
        p->kdIdx++;
        if (p->kdIdx >= n) { p->jumpZ = 0; return TRUE; }
        if (stopAtFloor && !t[p->kdIdx].air) { p->jumpZ = 0; return TRUE; }
        kdShow(p);
    }
    return FALSE;
}

// Pose de knockeada (sin vida): ANIM_KO en loop; sheets viejas sin ella, el
// frame tirado de HIT_BEHIND_2.
static void showKoPose(Player* p) {
    if (p->numAnims > ANIM_KO) {
        SPR_setAutoAnimation(p->sprite, TRUE);
        SPR_setAnimationLoop(p->sprite, TRUE);
        SPR_setAnimAndFrame(p->sprite, ANIM_KO, 0);
    } else {
        SPR_setAutoAnimation(p->sprite, FALSE);
        SPR_setAnimAndFrame(p->sprite, ANIM_HIT_BEHIND_2, PLAYER_KO_FRAME);
    }
}

// Muerte del control arcade (STATE_KO): vuela, tirada, se esfuma, invisible y
// reaparece parpadeando. Ver PLAYER_DEAD_* en player.h.
static void koArcadeStep(Player* p, s16 effRight) {
    switch (p->koPhase) {
        case 0:                                     // volando
            if (volteoStep(p, effRight, TRUE)) {
                p->koPhase = 1;
                p->koTimer = PLAYER_DEAD_LIE_F;
                showKoPose(p);
            }
            break;
        case 1:                                     // tirada
            if (p->koTimer > 0) p->koTimer--;
            if (p->koTimer == 0) {
                if (p->lives == 0) {
                    // Sin vidas: queda tirada; scenes.c lo detecta.
                    p->gameOver = TRUE;
                    p->koPhase  = 4;
                } else {
                    p->koPhase = 2;
                    p->koTimer = PLAYER_DEAD_FADE_F;
                }
            }
            break;
        case 2:                                     // se esfuma parpadeando 2/2
            if (p->koTimer > 0) p->koTimer--;
            SPR_setVisibility(p->sprite, (p->koTimer & 2) ? HIDDEN : VISIBLE);
            if (p->koTimer == 0) {
                p->koPhase = 3;
                p->koTimer = PLAYER_DEAD_GONE_F;
                SPR_setVisibility(p->sprite, HIDDEN);
            }
            break;
        case 3:                                     // invisible
            if (p->koTimer > 0) p->koTimer--;
            if (p->koTimer == 0) {
                p->koPhase    = 0;
                p->health     = PLAYER_MAX_HEALTH;
                p->state      = STATE_IDLE;
                p->jumpZ      = 0;
                p->invincible = PLAYER_ARCADE_RESPAWN_INV;
                p->blinkTimer = PLAYER_ARCADE_RESPAWN_INV;
                SPR_setVisibility(p->sprite, VISIBLE);
                SPR_setAutoAnimation(p->sprite, TRUE);
                SPR_setAnimationLoop(p->sprite, TRUE);
                SPR_setAnim(p->sprite, ANIM_IDLE);
            }
            break;
        default:                                    // fuera de juego
            break;
    }
}

// ---------------------------------------------------------------------------
// DAÑO RECIBIDO
// ---------------------------------------------------------------------------
bool playerCanBeHit(const Player* p) {
    if (p->gameOver) return FALSE;     // (01/10) fuera de juego
    if (p->invincible > 0) return FALSE;
    // Dentro de la boca de tormenta no la alcanza nada (esta bajo tierra).
    if (p->mhPhase != 0) return FALSE;
    // Saltando no se recibe daño (esquive aéreo estilo arcade). KO = ya está
    // en el piso. AGARRADO SÍ se puede: los otros foot soldiers le pegan a la
    // tortuga inmovilizada (damagePlayer lo resuelve mostrando el frame 3 del
    // HELD sin soltar el agarre). El doble agarre se bloquea aparte con
    // playerIsGrabbed (ver playerWhipGrab y el grab de enemy.c).
    if (p->state == STATE_HURT || p->state == STATE_JUMPING ||
        p->state == STATE_KO || p->state == STATE_KNOCKED_DOWN)
        return FALSE;
    return TRUE;
}

// Igual que playerCanBeHit pero SÍ acepta a la tortuga en el aire (14/09).
// Es para los proyectiles que discriminan por altura: el esquive aéreo del
// arcade vale contra golpes cuerpo a cuerpo, no contra un tiro antiaéreo que
// te está apuntando justo ahí arriba.
bool playerCanBeHitAir(const Player* p) {
    if (p->gameOver) return FALSE;     // (01/10) fuera de juego
    if (p->invincible > 0) return FALSE;
    if (p->mhPhase != 0) return FALSE;
    if (p->state == STATE_HURT || p->state == STATE_KO ||
        p->state == STATE_KNOCKED_DOWN)
        return FALSE;
    return TRUE;
}

// KNOCKOUT: se agotó la barra -> pierde una vida y queda tirada un momento
// (ver STATE_KO). El llamador ya fijó p->hurtDir (dirección del deslizamiento).
static void playerEnterKO(Player* p) {
    if (p->lives > 0) p->lives--;
    // Grito de vida perdida (14/09). Canal PCM 3 y no el 2: el golpe fatal ya
    // disparo hit_turtles en el 2 y asi se escuchan los dos (impacto + grito)
    // en vez de que este pise al otro.
    XGM2_playPCMEx(lost_life_turtles_vo, sizeof(lost_life_turtles_vo),
                   SOUND_PCM_CH2, 12, FALSE, FALSE);
    p->state      = STATE_KO;
    p->koTimer    = PLAYER_KO_FRAMES;
    p->hurtTimer  = PLAYER_HURT_KNOCK_FRAMES;   // deslizamiento inicial
    p->invincible = PLAYER_KO_FRAMES;           // intocable en el piso (SIN parpadeo)

    // (09/10) Control arcade: sale volando hacia atras (si el golpe vino de un
    // lado) y sigue la secuencia de koArcadeStep.
    if (PLAYER_ARCADE_CONTROL) {
        p->kdArcade   = 0;
        p->hurtTimer  = 0;
        p->invincible = 0;                      // STATE_KO ya no recibe golpes
        if (p->hurtDir != 0) {
            p->koPhase = 0;
            startVolteo(p, (s8)(-p->hurtDir));
        } else {
            p->koPhase = 1;
            p->koTimer = PLAYER_DEAD_LIE_F;
            p->jumpZ   = 0;
            showKoPose(p);
        }
        return;
    }

    // Pose de knockeado. ANIM_KO son CUATRO frames (la tortuga tirada con las
    // estrellitas girando), no una pose fija: antes se congelaba en el frame 0
    // y las estrellas no se movian (13/09). Ahora corre
    // en LOOP mientras dura koTimer -- es una animacion de espera, no una que
    // termina. La caida en si no se ve porque el KO entra directo a esta pose.
    // Fallback para sheets viejas sin ANIM_KO: ultimo frame de la caida de
    // espaldas, ese si congelado (no hay otra cosa que animar).
    if (p->numAnims > ANIM_KO) {
        SPR_setAutoAnimation(p->sprite, TRUE);
        SPR_setAnimationLoop(p->sprite, TRUE);
        SPR_setAnimAndFrame(p->sprite, ANIM_KO, 0);
    } else {
        SPR_setAutoAnimation(p->sprite, FALSE);
        SPR_setAnimAndFrame(p->sprite, ANIM_HIT_BEHIND_2, PLAYER_KO_FRAME);
    }
}

// Núcleo del daño recibido: resta 'bars' barras. Si llega a 0 -> knockout; si
// no, reacción de golpe (frente/espalda) con knockback e i-frames.
// (17/10) 'exact': las barras se restan TAL CUAL, sin pasar por
// playerDiffBars (ver footHitBars).
static void playerTakeHitEx(Player* p, s16 attackerX, u8 bars, bool volteo, bool exact) {
    // Golpe recibido EN EL AIRE (sólo lo permite playerCanBeHitAir, hoy las
    // balas del jefe): hay que bajar la tortuga al piso a mano, porque el
    // estado pasa a HURT y nadie más vuelve a tocar jumpZ — si no, se queda
    // flotando a la altura donde la agarró el disparo (14/09; mismo bug que
    // tuvo el foot soldier blanco al ser golpeado en pleno salto).
    if (p->state == STATE_JUMPING) {
        p->jumpZ         = 0;
        p->jumpZq        = 0;
        p->jumpVq        = 0;
        p->isJumpKicking = JUMPKICK_NONE;
        p->kickCarry     = 0;
    }

    // ¿De qué lado vino el golpe? El empuje va hacia el lado contrario.
    s16 centerX = p->x + PLAYER_SPRITE_W / 2;
    s8  side    = (attackerX >= centerX) ? 1 : -1;
    p->hurtDir  = -side;

    // Un golpe corta cualquier combo o especial en curso
    p->comboStep     = 0;
    arcCut(p);
    p->comboBuffered = 0;
    p->comboLinger   = 0;
    p->attackIsSpecial = 0;

    // Vida: restar 'bars' barras (clamp a 0). (10/10) En NORMAL, menos.
    if (!exact) bars = playerDiffBars(p, bars);
    if (p->health > (s16)bars) p->health -= (s16)bars;
    else                       p->health = 0;

    if (p->health == 0) { playerEnterKO(p); return; }

    // (09/10) Control arcade: el golpe VOLTEA a la tortuga (ver PLAYER_KD_*).
    if (PLAYER_ARCADE_CONTROL && volteo && p->state != STATE_GRABBED) {
        p->state      = STATE_KNOCKED_DOWN;
        p->kdArcade   = 1;
        startVolteo(p, side);
        u16 inv = volteoTotal(p) + PLAYER_KD_INV_EXTRA;
        p->invincible = (u8)((inv > 255) ? 255 : inv);
        return;
    }

    // Golpe normal (todavía con vida). Por la espalda si el atacante está del
    // lado contrario a la mirada (la tortuga NO se da vuelta).
    bool behind = (side != p->dir);
    u16  anim;
    if (behind) {
        anim = ANIM_HIT_BEHIND_1;
    } else {
        anim = p->hurtToggle ? ANIM_HIT_2 : ANIM_HIT_1;
        p->hurtToggle ^= 1;
    }

    p->state      = STATE_HURT;
    p->hurtTimer  = PLAYER_HURT_KNOCK_FRAMES;
    p->invincible = PLAYER_HURT_INVINCIBLE;

    // (14/09) setAutoAnimation(TRUE) OBLIGATORIO acá. STATE_HURT sale cuando
    // SPR_isAnimationDone da TRUE, o sea que necesita que la anim CORRA. El
    // salto apaga la auto-animación (maneja sus frames a mano) y sólo la vuelve
    // a prender al aterrizar: si el golpe llega en el aire — cosa que recién
    // ahora puede pasar, con las balas del jefe — la anim de hit quedaba
    // congelada en el frame 0, isAnimationDone nunca daba TRUE y la tortuga se
    // quedaba trabada en esa pose PARA SIEMPRE (visto en las pruebas con las
    // dos tortugas trabadas tras un disparo).
    SPR_setAutoAnimation(p->sprite, TRUE);
    SPR_setAnimationLoop(p->sprite, FALSE);
    SPR_setAnimAndFrame(p->sprite, anim, 0);
}

static void playerTakeHit(Player* p, s16 attackerX, u8 bars, bool volteo) {
    playerTakeHitEx(p, attackerX, bars, volteo, FALSE);
}

// (17/10) Golpe de foot soldier y de sus proyectiles (shuriken, cuchillo,
// lanza, boomerang, bala). GAME LEVEL NORMAL: SIEMPRE 1 barra exacta (antes
// era el 3/4 del golpe de 2 del control arcade: 1, 2, 1, 2...). ARCADE: como
// siempre (PLAYER_ARCADE_HIT_BARS con el control arcade, 1 si no).
static void footHit(Player* p, s16 attackerX) {
    bool volteo = PLAYER_ARCADE_CONTROL;
    if (dificultad == DIFICULTAD_NORMAL)
        playerTakeHitEx(p, attackerX, 1, volteo, TRUE);
    else
        playerTakeHit(p, attackerX, PLAYER_ARCADE_CONTROL ? PLAYER_ARCADE_HIT_BARS : 1, volteo);
}

// Golpe de foot soldier (1 barra).
void damagePlayer(Player* p, s16 attackerX) {
    // AgarraDO por un foot soldier: el golpe NO lo suelta. Muestra el frame 3
    // del HELD (golpe en pleno agarre) y sigue agarrado — se zafa masheando o
    // si le pegan al soldier. Si la barra llega a 0 → knockout (que sí termina
    // el agarre al pasar a STATE_KO).
    if (p->state == STATE_GRABBED && p->grabType == GRAB_TYPE_FOOT) {
        // (17/10) NORMAL: 1 barra exacta (ver footHit).
        if (p->health > 0) p->health -= (dificultad == DIFICULTAD_NORMAL) ? 1 : playerDiffBars(p, 1);
        if (p->health == 0) {
            p->hurtDir = 0;
            playerEnterKO(p);
            return;
        }
        p->heldHit = PLAYER_HELD_HIT_FRAMES;
        SPR_setAutoAnimation(p->sprite, FALSE);
        setHeldFrame(p, 3);
        return;
    }
    if (!playerCanBeHit(p)) return;
    // Control arcade: 2 barras y volteo (el golpe tipico del arcade).
    footHit(p, attackerX);
}

// (15/10) Como damagePlayer pero tambien EN EL AIRE: el salto con espadazo
// del blanco le gana a la patada voladora (ver enemyDamagePlayer).
void damagePlayerAir(Player* p, s16 attackerX) {
    if (!playerCanBeHitAir(p)) return;
    footHit(p, attackerX);
}

// Golpe que resta VARIAS barras (p.ej. el láser del robot = 4).
void playerHitBars(Player* p, s16 attackerX, u8 bars) {
    if (!playerCanBeHit(p)) return;
    playerTakeHit(p, attackerX, bars, TRUE);
}

// Golpe de PROYECTIL: igual que damagePlayer pero también conecta con la
// tortuga EN EL AIRE (playerCanBeHitAir). Lo usan las balas de Rocksteady
// (14/09): el tiro hacia arriba existe justamente para castigar el salto, y
// con la regla general de "saltando no te pegan" no podía conectar nunca.
// La altura ya la filtra quien llama (la bala compara su z contra el torso),
// así que el tiro recto sigue pasando por debajo del que está en el ápex.
void playerHitProjectile(Player* p, s16 attackerX, u8 bars) {
    if (!playerCanBeHitAir(p)) return;
    playerTakeHit(p, attackerX, bars, FALSE);   // balas: golpe liviano
}

// (18/10) Llamarada del lanzallamas de Granitor. Mismo golpe liviano que un
// proyectil (resta las barras y corta lo que estuviera haciendo), pero en vez
// del HIT la tortuga queda QUIETA con la cabeza carbonizada (ANIM_BURN, 2
// frames en loop) PLAYER_BURN_FRAMES, sin empuje. Sale por STATE_HURT como
// cualquier golpe (hurtTimer en 0 + fin de la vuelta de la anim).
#define PLAYER_BURN_FRAMES  45
void playerBurnHit(Player* p, s16 attackerX, u8 bars) {
    if (!playerCanBeHitAir(p)) return;
    playerTakeHit(p, attackerX, bars, FALSE);
    if (p->state != STATE_HURT || !playerHasBurnAnim(p)) return;   // KO, o sin la anim
    p->hurtDir    = 0;                       // sin empuje: se queda donde estaba
    p->hurtTimer  = PLAYER_BURN_FRAMES;
    if (p->invincible < PLAYER_BURN_FRAMES + PLAYER_HURT_INVINCIBLE)
        p->invincible = PLAYER_BURN_FRAMES + PLAYER_HURT_INVINCIBLE;
    SPR_setAutoAnimation(p->sprite, TRUE);
    SPR_setAnimationLoop(p->sprite, TRUE);
    SPR_setAnimAndFrame(p->sprite, ANIM_BURN, 0);
}

// Golpe fuerte que DERRIBA (patada de Rocksteady): cae de espaldas, queda
// tirada PLAYER_KD_HOLD_FRAMES y se levanta. Con la barra en 0 → KO normal.
// Agarrada: degrada a golpe normal para no romper la lógica del agarre.
void playerHitBarsKnockdown(Player* p, s16 attackerX, u8 bars) {
    if (!playerCanBeHit(p)) return;
    if (p->state == STATE_GRABBED) {
        playerTakeHit(p, attackerX, bars, FALSE);
        return;
    }
    if (PLAYER_ARCADE_CONTROL) {
        playerTakeHit(p, attackerX, bars, TRUE);
        return;
    }

    s16 centerX = p->x + PLAYER_SPRITE_W / 2;
    s8  side    = (attackerX >= centerX) ? 1 : -1;
    p->hurtDir  = -side;   // sale despedida hacia el lado contrario al golpe

    // Un golpe corta cualquier combo o especial en curso
    p->comboStep       = 0;
    arcCut(p);
    p->comboBuffered   = 0;
    p->comboLinger     = 0;
    p->attackIsSpecial = 0;

    bars = playerDiffBars(p, bars);   // (10/10) NORMAL: menos dano
    if (p->health > (s16)bars) p->health -= (s16)bars;
    else                       p->health = 0;
    if (p->health == 0) { playerEnterKO(p); return; }

    // ¿De frente o por la espalda? La tortuga NO se da vuelta, así que el
    // golpe es "por la espalda" si vino del lado contrario al que mira.
    // Cada lado tiene su propia cadena de animaciones en la sheet:
    //   DE FRENTE   anim 13 (sale despedida hacia atras) -> 14 (se levanta)
    //   DE ESPALDAS anim 15 (trastabilla) -> 16 (rueda)  -> 17 (se levanta)
    p->kdFront = (side == p->dir) ? 1 : 0;

    // Secuencia de derribo, animaciones a MANO con auto-anim encendida y sin
    // loop (mismo patrón que STATE_HURT: isAnimationDone marca el paso).
    p->state      = STATE_KNOCKED_DOWN;
    p->kdArcade   = 0;
    p->kdTimer    = 0;
    p->hurtTimer  = 0;                          // el arrastre lo lleva kdSlide
    p->invincible = PLAYER_KD_INVINCIBLE;       // intocable toda la secuencia
    p->kdSlide     = PLAYER_KD_SLIDE_SPEED;
    p->kdSlideTick = PLAYER_KD_SLIDE_DECAY;
    // (26/09) El especial apaga la auto-animacion: si el derribo lo corta,
    // hay que prenderla o la secuencia se queda congelada en el frame 0.
    SPR_setAutoAnimation(p->sprite, TRUE);
    SPR_setAnimationLoop(p->sprite, FALSE);
    if (p->kdFront) {
        // De frente no hay frame de retroceso: la anim 13 YA es la caida
        // entera, asi que se entra directo a la fase 1.
        p->kdPhase = 1;
        SPR_setAnimAndFrame(p->sprite, ANIM_HIT_3, 0);
    } else {
        p->kdPhase = 0;
        SPR_setAnimAndFrame(p->sprite, ANIM_HIT_BEHIND_1, 0);
    }
}

// ---------------------------------------------------------------------------
// AGARRES (látigo del robot y espalda del foot soldier)
// ---------------------------------------------------------------------------
// Suelta a la tortuga de CUALQUIER agarre: vuelve a STATE_IDLE con i-frames
// para que no la vuelvan a agarrar en el acto. La llaman el mash del
// STATE_GRABBED, enemy.c (le pegaron al soldier que la tenía) y el tope de
// seguridad por tiempo del grab de foot soldier.
void playerReleaseGrab(Player* p) {
    if (p->state != STATE_GRABBED) return;
    p->state      = STATE_IDLE;
    p->invincible = PLAYER_HURT_INVINCIBLE;
    p->grabTimer  = 0;
    p->heldHit    = 0;
    SPR_setAutoAnimation(p->sprite, TRUE);
    SPR_setAnimationLoop(p->sprite, TRUE);
    SPR_setAnim(p->sprite, ANIM_IDLE);
}

// Pone a la tortuga en STATE_GRABBED reproduciendo la anim de agarre/
// electrocución. Se zafa masheando (ver STATE_GRABBED en updatePlayer).
void playerWhipGrab(Player* p) {
    if (!playerCanBeHit(p)) return;          // no agarrable (KO, salto, hurt, i-frames…)
    if (playerIsGrabbed(p)) return;          // ya agarrada por un foot soldier: no doble-agarre

    p->comboStep     = 0;
    arcCut(p);
    p->comboBuffered = 0;
    p->comboLinger   = 0;
    p->attackIsSpecial = 0;

    p->state     = STATE_GRABBED;
    p->grabType  = GRAB_TYPE_WHIP;
    p->grabTimer = PLAYER_GRAB_ESCAPE;   // metro de forcejeo (baja masheando)
    SPR_setAutoAnimation(p->sprite, TRUE);
    SPR_setAnimationLoop(p->sprite, TRUE);
    if (p->numAnims > ANIM_WHIP_SHOCK)
        SPR_setAnimAndFrame(p->sprite, ANIM_WHIP_SHOCK, 0);
    else
        SPR_setAnimAndFrame(p->sprite, ANIM_HELD, 0);   // fallback (todas las sheets)
}

// Igual que playerWhipGrab pero COLOCANDO a la tortuga donde termina el cable
// (14/09). El látigo del robot tiene 4 largos dibujados y nada más, así que el
// enganche elige el que mejor cae y de paso pega este tirón para que la punta
// quede justo sobre el cuerpo — y alinea la lane con la del robot, que es a la
// altura donde está dibujado el cable. Sin esto el cable sobresalía por detrás
// de la tortuga (o le quedaba corto) y encima iba a otra altura.
// 'worldX' es la X de MUNDO del sprite (esquina), no el centro.
void playerWhipGrabAt(Player* p, s16 worldX, s16 lane) {
    if (!playerCanBeHit(p)) return;
    if (playerIsGrabbed(p)) return;
    playerWhipGrab(p);
    if (!playerIsGrabbed(p)) return;          // no enganchó: no mover nada
    p->x = clampS16(worldX, p->boundLeft, p->boundRight);
    p->y = clampS16(lane,   p->laneTop,   p->laneBottom);
}

// Agarre por la espalda del foot soldier morado: misma mecánica que el látigo
// (STATE_GRABBED + mash) pero con la anim HELD a MANO — los frames 0-2 se
// alternan en updatePlayer y el frame 3 (golpe en el agarre) lo muestra
// damagePlayer cuando le pegan mientras está agarrada. Igual que el látigo,
// bloquea el doble agarre (si ya está agarrada, no hace nada).
void playerFootGrab(Player* p) {
    if (!playerCanBeHit(p)) return;          // no agarrable (KO, salto, hurt, i-frames…)
    if (playerIsGrabbed(p)) return;          // ya agarrada (látigo u otro soldier)

    p->comboStep     = 0;
    arcCut(p);
    p->comboBuffered = 0;
    p->comboLinger   = 0;
    p->attackIsSpecial = 0;

    p->state     = STATE_GRABBED;
    p->grabType  = GRAB_TYPE_FOOT;
    p->grabTimer = PLAYER_GRAB_ESCAPE;   // metro de forcejeo (baja masheando)
    p->heldFrame = 0;
    p->heldTimer = 0;
    p->heldHit   = 0;
    SPR_setAutoAnimation(p->sprite, FALSE);
    SPR_setAnimAndFrame(p->sprite, ANIM_HELD, 0);
}

// (18/10) Mordida de un Mouser: el agarre de siempre (STATE_GRABBED + mash)
// con la anim de la mordida, que avanza un frame por boton (updatePlayer).
void playerMouserGrab(Player* p) {
    if (!playerCanBeHit(p)) return;
    if (playerIsGrabbed(p)) return;

    p->comboStep     = 0;
    arcCut(p);
    p->comboBuffered = 0;
    p->comboLinger   = 0;
    p->attackIsSpecial = 0;

    p->state     = STATE_GRABBED;
    p->grabType  = GRAB_TYPE_MOUSER;
    p->grabTimer = PLAYER_GRAB_ESCAPE;
    p->heldFrame = 0;
    p->heldTimer = 0;
    p->heldHit   = 0;
    SPR_setAutoAnimation(p->sprite, FALSE);
    SPR_setAnimAndFrame(p->sprite, playerHasBiteAnim(p) ? ANIM_MOUSER_BITE : ANIM_HELD, 0);
}

bool playerIsGrabbed(const Player* p) {
    return (p->state == STATE_GRABBED);
}

bool playerIsFootGrabbed(const Player* p) {
    return (p->state == STATE_GRABBED && p->grabType == GRAB_TYPE_FOOT);
}

// Drena 1 barra (llamado por el robot ~1 vez por segundo mientras agarra). Si
// deja la vida en 0, knockout (que además termina el agarre al pasar a KO).
void playerElectroDrain(Player* p) {
    if (p->state != STATE_GRABBED) return;
    if (p->health > 0) p->health--;
    if (p->health == 0) {
        p->hurtDir = 0;
        playerEnterKO(p);
    }
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
    // (11/10) VIDA EXTRA cada PLAYER_EXTRA_LIFE_PTS puntos, solo en GAME LEVEL
    // NORMAL (en ARCADE no hay). Se cuenta por "centena cruzada": si un mismo
    // golpe cruza dos (no pasa hoy, el puntaje sube de a 1-5) da las dos. Tope
    // PLAYER_MAX_LIVES: las vidas del HUD son UN digito.
    u16 before = p->score;
    p->score += points;
    s_finalScore[persistSlot(p->joyId)] = p->score;   // (20/10) tabla de puntajes
    if (dificultad == DIFICULTAD_NORMAL) {
        u16 gained = (u16)(p->score / PLAYER_EXTRA_LIFE_PTS - before / PLAYER_EXTRA_LIFE_PTS);
        u8 livesBefore = p->lives;
        while (gained-- > 0 && p->lives < PLAYER_MAX_LIVES) p->lives++;
        // (16/10) "1 up" del arcade. CH3: en CH2 lo cortarian los golpes.
        if (p->lives != livesBefore)
            XGM2_playPCMEx(one_up_sfx, sizeof(one_up_sfx), SOUND_PCM_CH3, 15, FALSE, FALSE);
    }
}

// ---------------------------------------------------------------------------
// PERSISTENCIA ENTRE NIVELES
// ---------------------------------------------------------------------------
void playerPersistSave(const Player* p) {
    u8 slot = persistSlot(p->joyId);
    s_persistLives[slot]  = p->lives;
    s_persistScore[slot]  = p->score;
    s_persistHealth[slot] = p->health;
    s_persistOut[slot]    = p->gameOver;
}

void playerPersistClearOut(const Player* p) {
    persistEnsureInit();
    s_persistOut[persistSlot(p->joyId)] = FALSE;
}

bool playerFinalPeek(u8 k, u16* score, u8* ch) {
    if (k >= MAX_PLAYERS || !s_finalPlayed[k]) return FALSE;
    *score = s_finalScore[k];
    *ch    = s_finalChar[k];
    return TRUE;
}

void playerFinalClear(void) {
    for (u8 i = 0; i < MAX_PLAYERS; i++) s_finalPlayed[i] = FALSE;
}

void playerPersistPeek(u8 k, u8* lives, u16* score, s16* health, bool* out) {
    persistEnsureInit();
    if (k >= MAX_PLAYERS) k = 0;
    *lives  = s_persistLives[k];
    *score  = s_persistScore[k];
    *health = s_persistHealth[k];
    *out    = s_persistOut[k];
}

void playerPersistSetHealth(u8 k, s16 health) {
    persistEnsureInit();
    if (k >= MAX_PLAYERS) return;
    s_persistHealth[k] = health;
}

void playerPersistReset(void) {
    s_persistInit = TRUE;
    for (u8 i = 0; i < MAX_PLAYERS; i++) {
        s_finalPlayed[i]   = FALSE;
        s_finalScore[i]    = 0;
        s_persistLives[i]  = vidasIniciales;
        s_persistScore[i]  = 0;
        s_persistHealth[i] = PLAYER_MAX_HEALTH;
        s_persistOut[i]    = FALSE;
    }
}

bool isPlayerGameOver(const Player* p) {
    // TRUE recién cuando terminó la pose de knockeado sin vidas restantes
    // (se activa al final del STATE_KO), no en el instante del golpe: así el
    // jugador ve la caída antes del game over.
    return p->gameOver;
}

// ---------------------------------------------------------------------------
// MOVIMIENTO SCRIPTEADO (cutscenes) — sin leer input
// ---------------------------------------------------------------------------
// Renderiza la tortuga en su posición actual (mundo - cámara). La cámara la fija
// scenes.c con setPlayerCamera antes de la cutscene.
static void playerRenderAt(Player* p) {
    // (15/09) Guarda de sprite NULO: si el motor de sprites se quedo sin lugar
    // (pasa con 4 tortugas), este jugador no tiene sprite. Sin la guarda, todo
    // lo que sigue leeria el struct Sprite desde la direccion 0 (ROM) y
    // devolveria basura -- y los estados que esperan SPR_isAnimationDone se
    // colgarian para siempre. Sin sprite el jugador queda invisible e inerte,
    // pero el juego sigue.
    if (!p->sprite) return;

    playerSetSpritePos(p, p->x - p->cameraOffsetX, p->y - PLAYER_FOOT_OFFSET - stageCamY);
    SPR_setDepth(p->sprite, -(p->y));
}

void playerCutsceneStand(Player* p) {
    // (15/09) Guarda de sprite NULO: si el motor de sprites se quedo sin lugar
    // (pasa con 4 tortugas), este jugador no tiene sprite. Sin la guarda, todo
    // lo que sigue leeria el struct Sprite desde la direccion 0 (ROM) y
    // devolveria basura -- y los estados que esperan SPR_isAnimationDone se
    // colgarian para siempre. Sin sprite el jugador queda invisible e inerte,
    // pero el juego sigue.
    if (!p->sprite || p->gameOver) return;

    SPR_setAutoAnimation(p->sprite, TRUE);
    SPR_setAnimationLoop(p->sprite, TRUE);
    SPR_setAnim(p->sprite, ANIM_IDLE);
    playerRenderAt(p);
}

bool playerCutsceneWalkTo(Player* p, s16 targetX, s16 targetY) {
    // Sin sprite (ver la guarda de updatePlayer) se da por LLEGADO: si no, la
    // cutscene de salida se quedaria esperando a un jugador que no existe.
    if (!p->sprite || p->gameOver) return TRUE;
    bool arrived = TRUE;

    s16 dx = targetX - p->x;
    if (dx > 0)      { p->x += (dx <  PLAYER_SPEED) ? dx :  PLAYER_SPEED; p->dir =  1; SPR_setHFlip(p->sprite, FALSE); arrived = FALSE; }
    else if (dx < 0) { p->x += (dx > -PLAYER_SPEED) ? dx : -PLAYER_SPEED; p->dir = -1; SPR_setHFlip(p->sprite, TRUE);  arrived = FALSE; }

    s16 dy = targetY - p->y;
    if (dy > 0)      { p->y += (dy <  PLAYER_SPEED) ? dy :  PLAYER_SPEED; arrived = FALSE; }
    else if (dy < 0) { p->y += (dy > -PLAYER_SPEED) ? dy : -PLAYER_SPEED; arrived = FALSE; }

    SPR_setAutoAnimation(p->sprite, TRUE);
    SPR_setAnimationLoop(p->sprite, TRUE);
    SPR_setAnim(p->sprite, arrived ? ANIM_IDLE : ANIM_WALK_FRONT);
    playerRenderAt(p);
    return arrived;
}

// Congela a la tortuga en un frame de "caminar hacia arriba" (ANIM_WALK_BACK):
// con la auto-anim apagada se queda fija en ese frame, como observando la
// cutscene de victoria (Shredder raptando a April). No lee input.
// ---------------------------------------------------------------------------
// CAIDA POR LA BOCA DE TORMENTA (20/09) — nivel 2-1
// ---------------------------------------------------------------------------
// La anim 21 son NUEVE frames y se maneja a mano, igual que la salida por
// alcantarilla del foot soldier: la auto-animacion de SGDK no sirve porque el
// frame 3 (vacio) tiene que sostenerse lo que dure el voice over, mucho mas
// que los 5 ticks del sheet.
//
// Tres fases (p->mhPhase), cada una con su cadencia:
//   1  caida   se hunde        PLAYER_MH_FALL_TICKS por frame
//   2  vacio   frame vacio     PLAYER_MH_HOLD_TICKS de una (globo + VO)
//   3  salida  sale del pozo   PLAYER_MH_OUT_TICKS por frame
// (cuantos frames tiene cada tramo depende de la sheet: ver mhLayout)
//
// La tortuga se dibuja SIEMPRE en el agujero (no se mueve mientras cae ni
// mientras sale); recien al terminar la fase 3 se la teletransporta a
// (mhOutX, mhOutY), que la escena puso unos px por debajo del agujero. Si
// reapareciera encima se volveria a caer en el acto, que es justo lo que
// hay que evitar.
// ---------------------------------------------------------------------------
// (22/09) El layout NO es igual en las 4 sheets: Leo tiene 9 frames
// (3 caida + vacio + 5 salida), Mike y Raph 8 (3+1+4) y Don 7 (2+1+4). Con el
// layout de Leo clavado, a Don el "frame 3" le caia en la salida (la mano
// asomando) en lugar del vacio. Se lee de la definicion del sprite: el frame
// vacio es el primero con numSprite == 0 (rescomp conserva los frames vacios
// en medio de una fila), lo anterior es la caida y lo posterior la salida.
// Sin sprite se usa el layout de Leo, que solo importa para el tiempo.
typedef struct { u8 fall; u8 empty; u8 outFirst; u8 out; } MhLayout;

static MhLayout mhLayout(const Player* p) {
    MhLayout l = { 3, 3, 4, 5 };
    if (!p->sprite) return l;
    const Animation* an = p->sprite->definition->animations[ANIM_MANHOLE];
    u8 n = an->numFrame;
    u8 e = 0;
    while (e < n && an->frames[e]->numSprite != 0) e++;
    if (e >= n) e = (n > 1) ? (u8)(n / 2) : 0;   // sin frame vacio: se sostiene el del medio
    l.empty = e; l.fall = e; l.outFirst = (u8)(e + 1);
    if (l.fall == 0) l.fall = 1;
    l.out = (n > l.outFirst) ? (u8)(n - l.outFirst) : 1;
    if (l.outFirst >= n) l.outFirst = (u8)(n - 1);
    return l;
}

bool playerManholeFall(Player* p, s16 outX, s16 outY) {
    if (p->mhPhase != 0)        return FALSE;   // ya esta adentro
    if (!playerCanBeHit(p))     return FALSE;   // i-frames, KO, salto, hurt...
    if (p->state == STATE_GRABBED) return FALSE;
    // Sheets viejas sin la anim 21: no hay con que dibujar la caida.
    if (p->numAnims <= ANIM_MANHOLE) return FALSE;

    // El pozo cuesta PLAYER_MANHOLE_DMG barras. Si lo deja en 0 no hay caida:
    // manda el knockout normal (perder una vida y revivir es mas fuerte que
    // quedar atrapado en una animacion de 2 segundos).
    u8 mhDmg = playerDiffBars(p, PLAYER_MANHOLE_DMG);   // (10/10) NORMAL: menos dano
    if (p->health > (s16)mhDmg) p->health -= (s16)mhDmg;
    else                        p->health = 0;
    if (p->health == 0) {
        p->hurtDir = 0;
        playerEnterKO(p);
        return FALSE;
    }

    // Cortar cualquier cosa en curso (combo, especial, salto).
    p->comboStep       = 0;
    arcCut(p);
    p->comboBuffered   = 0;
    p->comboLinger     = 0;
    p->attackIsSpecial = 0;
    p->jumpZ           = 0;
    p->jumpZq          = 0;
    p->jumpVq          = 0;
    p->hurtTimer       = 0;
    p->state           = STATE_IDLE;

    p->mhPhase = 1;
    p->mhTimer = PLAYER_MH_FALL_TICKS * mhLayout(p).fall;
    p->mhOutX  = outX;
    p->mhOutY  = outY;

    if (p->sprite) {
        SPR_setAutoAnimation(p->sprite, FALSE);
        SPR_setAnimationLoop(p->sprite, FALSE);
        SPR_setAnimAndFrame(p->sprite, ANIM_MANHOLE, 0);
    }
    return TRUE;
}

bool playerInManhole(const Player* p)       { return (bool)(p->mhPhase != 0); }

void playerDropIn(Player* p, s16 height) {
    if (p->gameOver) return;           // (01/10) fuera de juego: no entra
    p->state         = STATE_JUMPING;
    p->jumpZ         = height;
    p->jumpZq        = (s32)height << PLAYER_JUMP_Q;
    p->jumpVq        = (s32)PLAYER_FALL_SPEED << PLAYER_JUMP_Q;   // ya cayendo
    p->airFrame      = 1;
    p->airTimer      = 3;            // hoja nueva: ya en los frames del aire
    p->jumpT         = PLAYER_AJ_LATE_T;
    p->isJumpKicking = JUMPKICK_NONE;
    p->comboStep     = 0;
    arcCut(p);
    p->hurtTimer     = 0;
    if (p->sprite) {
        SPR_setAutoAnimation(p->sprite, FALSE);
        SPR_setAnimAndFrame(p->sprite, ANIM_JUMP, 1);
    }
}
bool playerManholeSpeaking(const Player* p) { return (bool)(p->mhPhase == 2); }

bool playerManholeStep(Player* p) {
    if (p->mhPhase == 0) return TRUE;

    // Sin sprite (VRAM de sprites llena con 4 tortugas) la secuencia igual
    // corre por tiempo: si no, este jugador se quedaria en el pozo para
    // siempre. Misma regla que el resto del modulo.
    const MhLayout L = mhLayout(p);
    s16 frame = L.empty;

    if (p->mhTimer > 0) p->mhTimer--;

    switch (p->mhPhase) {
        case 1: {                                   // hundiendose
            s16 done = (s16)(PLAYER_MH_FALL_TICKS * L.fall) - (s16)p->mhTimer;
            frame = done / PLAYER_MH_FALL_TICKS;
            if (frame > L.fall - 1) frame = L.fall - 1;
            if (p->mhTimer == 0) { p->mhPhase = 2; p->mhTimer = PLAYER_MH_HOLD_TICKS; }
            break;
        }
        case 2:                                     // abajo: globo + voice over
            frame = L.empty;
            if (p->mhTimer == 0) {
                p->mhPhase = 3;
                p->mhTimer = PLAYER_MH_OUT_TICKS * L.out;
            }
            break;

        case 4: {                                   // (11/10) salida del nivel: se hunde y queda
            if (p->mhTimer > 0) {
                s16 done = (s16)(PLAYER_MH_FALL_TICKS * L.fall) - (s16)p->mhTimer;
                frame = done / PLAYER_MH_FALL_TICKS;
                if (frame > L.fall - 1) frame = L.fall - 1;
            } else {
                frame = L.empty;
            }
            break;
        }

        default: {                                  // saliendo
            s16 done = (s16)(PLAYER_MH_OUT_TICKS * L.out) - (s16)p->mhTimer;
            frame = L.outFirst + done / PLAYER_MH_OUT_TICKS;
            if (frame > L.outFirst + L.out - 1)
                frame = L.outFirst + L.out - 1;
            if (p->mhTimer == 0) {
                // Afuera: se la corre por debajo del agujero y vuelve el control.
                p->mhPhase = 0;
                p->x       = p->mhOutX;
                p->y       = p->mhOutY;
                p->state   = STATE_IDLE;
                // Unos i-frames para que no la reciba un golpe justo al salir.
                p->invincible = PLAYER_HURT_INVINCIBLE;
                if (p->sprite) {
                    SPR_setAutoAnimation(p->sprite, TRUE);
                    SPR_setAnimationLoop(p->sprite, TRUE);
                    SPR_setAnim(p->sprite, ANIM_IDLE);
                }
                playerRenderAt(p);
                return TRUE;
            }
            break;
        }
    }

    if (p->sprite) SPR_setFrame(p->sprite, frame);
    playerRenderAt(p);
    return FALSE;
}

// ---------------------------------------------------------------------------
// SALIDA DEL NIVEL 2-1 (11/10) -- ver player.h
// ---------------------------------------------------------------------------
void playerExitJumpStart(Player* p, s16 tx, s16 ty, u16 frames, s16 peak) {
    p->exX0 = p->x;  p->exY0 = p->y;
    p->exTX = tx;    p->exTY = ty;
    p->exT  = 0;
    p->exN  = frames ? frames : 1;
    p->mhOutX = peak;                     // alto del arco (mhOutX no se usa aca)
    p->comboStep = 0;
    arcCut(p);
    p->isJumpKicking = JUMPKICK_NONE;
    p->state  = STATE_JUMPING;
    p->jumpZ  = 0;
    if (tx > p->x)      p->dir =  1;
    else if (tx < p->x) p->dir = -1;
    if (p->sprite) {
        SPR_setAutoAnimation(p->sprite, FALSE);
        SPR_setAnimationLoop(p->sprite, FALSE);
        SPR_setHFlip(p->sprite, p->dir < 0);
        SPR_setAnimAndFrame(p->sprite, ANIM_JUMP, 1);
    }
}

bool playerExitJumpStep(Player* p) {
    if (p->exT < p->exN) p->exT++;
    s32 t = p->exT, n = p->exN;
    p->x = (s16)(p->exX0 + ((s32)(p->exTX - p->exX0) * t) / n);
    p->y = (s16)(p->exY0 + ((s32)(p->exTY - p->exY0) * t) / n);
    // Parabola: 0 en las puntas, 'peak' en la mitad.
    p->jumpZ  = (s16)((4 * (s32)p->mhOutX * t * (n - t)) / (n * n));
    p->jumpZq = (s32)p->jumpZ << PLAYER_JUMP_Q;
    if (p->sprite) {
        // Poses del salto repartidas a lo largo del arco (sin la 0, que es
        // la de agacharse antes de despegar).
        u8 nf = p->sprite->animation ? p->sprite->animation->numFrame : 2;
        s16 f = (nf > 2) ? (s16)(1 + (t * (nf - 2)) / n) : 0;
        if (f > nf - 1) f = (s16)(nf - 1);
        SPR_setFrame(p->sprite, f);
    }
    playerRenderAt(p);
    if (p->exT >= p->exN) {
        p->jumpZ = 0; p->jumpZq = 0;
        p->state = STATE_IDLE;
        return TRUE;
    }
    return FALSE;
}

void playerManholeEnter(Player* p) {
    p->state  = STATE_IDLE;
    p->jumpZ  = 0;
    p->jumpZq = 0;
    p->jumpVq = 0;
    p->mhPhase = 4;
    // Sheets viejas sin la anim 21: no hay con que dibujar el hundimiento,
    // asi que desaparece en el acto.
    if (p->numAnims <= ANIM_MANHOLE || !p->sprite) {
        p->mhTimer = 0;
        if (p->sprite) SPR_setVisibility(p->sprite, HIDDEN);
        return;
    }
    p->mhTimer = PLAYER_MH_FALL_TICKS * mhLayout(p).fall;
    SPR_setAutoAnimation(p->sprite, FALSE);
    SPR_setAnimationLoop(p->sprite, FALSE);
    SPR_setAnimAndFrame(p->sprite, ANIM_MANHOLE, 0);
}

bool playerManholeGone(const Player* p) {
    return (bool)(p->mhPhase == 4 && p->mhTimer == 0);
}

void playerCutsceneWatch(Player* p) {
    // (15/09) Guarda de sprite NULO: si el motor de sprites se quedo sin lugar
    // (pasa con 4 tortugas), este jugador no tiene sprite. Sin la guarda, todo
    // lo que sigue leeria el struct Sprite desde la direccion 0 (ROM) y
    // devolveria basura -- y los estados que esperan SPR_isAnimationDone se
    // colgarian para siempre. Sin sprite el jugador queda invisible e inerte,
    // pero el juego sigue.
    if (!p->sprite || p->gameOver) return;

    SPR_setAutoAnimation(p->sprite, FALSE);
    SPR_setAnimationLoop(p->sprite, FALSE);
    SPR_setAnimAndFrame(p->sprite, ANIM_WALK_BACK, 1);
    playerRenderAt(p);
}
