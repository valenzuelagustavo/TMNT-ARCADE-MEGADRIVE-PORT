#include "krang.h"
#include "difficulty.h"  // (20/10) vida segun jugadores
#include "audio.h"    // hit_turtles, boss_hit, electric_shock_sfx, ...
#include "enemy.h"    // sprDefragLock (las explosiones comparten tiles)

// ===========================================================================
// KRANG — ver krang.h
// ===========================================================================

static s16 kabs(s16 v)                 { return (v < 0) ? -v : v; }
static s16 kclamp(s16 v, s16 a, s16 b) { return (v < a) ? a : ((v > b) ? b : v); }

static s16 kStep(u8* acc, u16 q) {
    u16 t = (u16)(*acc + q);
    *acc = (u8)(t & 0xFF);
    return (s16)(t >> 8);
}

static s16 kToward(s16 v, s16 to, s16 step) {
    if (v < to) return (to - v < step) ? to : (s16)(v + step);
    if (v > to) return (v - to < step) ? to : (s16)(v - step);
    return v;
}

static Player* nearestPlayer(Player** pls, u8 nPl, s16 x) {
    Player* best = pls[0];
    s16 bestD = 0x7FFF;
    for (u8 k = 0; k < nPl; k++) {
        if (isPlayerGameOver(pls[k])) continue;
        s16 d = kabs((s16)(getPlayerWorldX(pls[k]) + PLAYER_SPRITE_W / 2 - x));
        if (d < bestD) { bestD = d; best = pls[k]; }
    }
    return best;
}

// Tortuga golpeada, derribada o con i-frames: "en el piso" para la IA.
static bool isDown(Player* p) { return !playerCanBeHitAir(p) && !isPlayerGameOver(p); }

// ---------------------------------------------------------------------------
// PUNO COHETE, LASER DE LOS OJOS y EXPLOSIONES DEL CUERPO
// ---------------------------------------------------------------------------
#define MAX_KRANG_FISTS 2
#define MAX_KRANG_BEAMS 1

static struct {
    bool    active;
    Sprite* sprite;
    s16     x, lane;      // punta del puno (mundo) y lane
    s8      dir;
    u8      accX, t;
    u8      hitMask;
} fists[MAX_KRANG_FISTS];

// (13/10) Laser de los ojos: KRANG_BEAM_SEGS tramos diagonales (krang_eyebeam)
// desde los ojos hasta el piso y el chispazo (krang_zap) donde toca. Sale con
// Krang quieto, asi que queda fijo donde salio.
static struct {
    bool    active;
    Sprite* seg[KRANG_BEAM_SEGS];
    Sprite* zap;          // el chispazo en el piso
    s16     x, lane;      // centro de Krang al disparar (mundo) y su lane
    s8      dir;
    u8      t;
    u8      hitMask;
} beams[MAX_KRANG_BEAMS];

// (13/10) Explosiones del cuerpo: la de Baxter en PAL2 (krang_boom). Cuartos:
// 0 arriba-izq, 1 arriba-der, 2 abajo-izq, 3 abajo-der. Los de la izquierda
// con su arte (anim 0 / 1) y los de la derecha espejados apuntando a sus
// tiles (128 tiles por explosion); si no entra, el de arriba y 3 espejos
// (64); si ni eso, esa explosion no se ve (el sonido y el flash si).
// state: 0 libre, 1 esperando su turno, 2 explotando.
static struct {
    u8      state;
    u16     wait;
    Sprite* s[4];
    u8      owner[4];
    s16     cx, cy;       // centro (mundo)
    u8      frame, tick;
} booms[KRANG_BOOMS];
static u8 whiteFlash;     // ticks de sala en blanco que quedan

// Donde salen, respecto del centro del cuerpo (la primera, justo ahi).
static const s8 boomOff[KRANG_BOOMS][2] = { { 0, 0 }, { -30, -30 }, { 28, 22 }, { 10, -44 } };

static void boomsInit(void) {
    for (u16 i = 0; i < KRANG_BOOMS; i++) {
        booms[i].state = 0;
        for (u16 q = 0; q < 4; q++) booms[i].s[q] = NULL;
    }
    whiteFlash = 0;
}

static void shotsInit(void) {
    for (u16 i = 0; i < MAX_KRANG_FISTS; i++) { fists[i].active = FALSE; fists[i].sprite = NULL; }
    for (u16 i = 0; i < MAX_KRANG_BEAMS; i++) {
        beams[i].active = FALSE; beams[i].zap = NULL;
        for (u16 g = 0; g < KRANG_BEAM_SEGS; g++) beams[i].seg[g] = NULL;
    }
    boomsInit();
}

static void fistFire(s16 x, s16 lane, s8 dir) {
    for (u16 i = 0; i < MAX_KRANG_FISTS; i++) {
        if (fists[i].active) continue;
        if (!fists[i].sprite)
            fists[i].sprite = SPR_addSprite(&krang_fist, -80, -80, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        fists[i].active = TRUE;
        fists[i].x = x;
        fists[i].lane = lane;
        fists[i].dir = dir;
        fists[i].accX = 0;
        fists[i].t = 0;
        fists[i].hitMask = 0;
        if (fists[i].sprite) {
            SPR_setAutoAnimation(fists[i].sprite, FALSE);
            SPR_setAnimAndFrame(fists[i].sprite, 0, 1);
            SPR_setHFlip(fists[i].sprite, dir < 0);
            SPR_setVisibility(fists[i].sprite, VISIBLE);
        }
        XGM2_playPCMEx(rocksteady_charge_sfx, sizeof(rocksteady_charge_sfx), SOUND_PCM_CH3, 12, FALSE, FALSE);
        return;
    }
}

static void beamFire(s16 x, s16 lane, s8 dir) {
    for (u16 i = 0; i < MAX_KRANG_BEAMS; i++) {
        if (beams[i].active) continue;
        for (u16 g = 0; g < KRANG_BEAM_SEGS; g++) {
            if (!beams[i].seg[g])
                beams[i].seg[g] = SPR_addSprite(&krang_eyebeam, -80, -80, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
            if (beams[i].seg[g]) {
                SPR_setAutoAnimation(beams[i].seg[g], FALSE);
                SPR_setAnimAndFrame(beams[i].seg[g], 0, 0);
                SPR_setHFlip(beams[i].seg[g], dir < 0);
                SPR_setVisibility(beams[i].seg[g], HIDDEN);
            }
        }
        if (!beams[i].zap)
            beams[i].zap = SPR_addSprite(&krang_zap, -80, -80, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        if (beams[i].zap) {
            SPR_setAutoAnimation(beams[i].zap, FALSE);
            SPR_setVisibility(beams[i].zap, HIDDEN);
        }
        beams[i].active = TRUE;
        beams[i].x = x;
        beams[i].lane = lane;
        beams[i].dir = dir;
        beams[i].t = 0;
        beams[i].hitMask = 0;
        XGM2_playPCMEx(electric_shock_sfx, sizeof(electric_shock_sfx), SOUND_PCM_CH3, 13, FALSE, FALSE);
        return;
    }
}

static void fistHide(u16 i) {
    fists[i].active = FALSE;
    if (fists[i].sprite) SPR_setVisibility(fists[i].sprite, HIDDEN);
}

static void beamHide(u16 i) {
    beams[i].active = FALSE;
    for (u16 g = 0; g < KRANG_BEAM_SEGS; g++)
        if (beams[i].seg[g]) SPR_setVisibility(beams[i].seg[g], HIDDEN);
    if (beams[i].zap) SPR_setVisibility(beams[i].zap, HIDDEN);
}

// --- Explosiones del cuerpo ---------------------------------------------------
static void boomRelease(u16 i) {
    for (u16 q = 0; q < 4; q++)
        if (booms[i].s[q]) { SPR_releaseSprite(booms[i].s[q]); booms[i].s[q] = NULL; }
}

static Sprite* boomMaster(u16 anim) {
    Sprite* s = SPR_addSprite(&krang_boom, -128, -128, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (s) SPR_setAnimAndFrame(s, (s16)anim, 0);
    return s;
}

static Sprite* boomMirror(const Sprite* m, u16 anim, bool hf, bool vf) {
    if (!m) return NULL;
    Sprite* s = SPR_addSpriteEx(&krang_boom, -128, -128,
                                TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE,
                                               m->attribut & TILE_INDEX_MASK), 0);
    if (!s) return NULL;
    SPR_setAnimAndFrame(s, (s16)anim, 0);
    SPR_setHFlip(s, hf);
    SPR_setVFlip(s, vf);
    return s;
}

static void boomStart(u16 i) {
    const u16 T = krang_boom.maxNumTile;
    Sprite** sp = booms[i].s;
    booms[i].state = 2;
    booms[i].frame = 0;
    booms[i].tick = 0;
    XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                   SOUND_PCM_CH3, 15, FALSE, FALSE);
    if (SPR_getLargestFreeVRAMBlock() < T) return;
    sp[0] = boomMaster(0);
    if (!sp[0]) return;
    if (SPR_getLargestFreeVRAMBlock() >= T) sp[2] = boomMaster(1);
    if (sp[2]) {                                   // mitad izquierda propia
        sp[1] = boomMirror(sp[0], 0, TRUE, FALSE);
        sp[3] = boomMirror(sp[2], 1, TRUE, FALSE);
        booms[i].owner[1] = 0; booms[i].owner[2] = 2; booms[i].owner[3] = 2;
    } else {                                       // un cuarto y 3 espejos
        sp[1] = boomMirror(sp[0], 0, TRUE,  FALSE);
        sp[2] = boomMirror(sp[0], 0, FALSE, TRUE);
        sp[3] = boomMirror(sp[0], 0, TRUE,  TRUE);
        booms[i].owner[1] = booms[i].owner[2] = booms[i].owner[3] = 0;
    }
    booms[i].owner[0] = 0;
    // Las que salen despues, delante de las anteriores.
    for (u16 q = 0; q < 4; q++) if (sp[q]) {
        SPR_setAutoAnimation(sp[q], FALSE);
        SPR_setDepth(sp[q], (s16)(SPR_MIN_DEPTH + 1 + (KRANG_BOOMS - i)));
    }
}

// Programa las KRANG_BOOMS explosiones alrededor del cuerpo (x, pies y).
static void boomsChain(s16 x, s16 y) {
    sprDefragLock = 1;            // los espejos apuntan a la VRAM de su dueno
    for (u16 i = 0; i < KRANG_BOOMS; i++) {
        boomRelease(i);
        booms[i].state = 1;
        booms[i].wait = (u16)(i * KRANG_BOOM_GAP);
        booms[i].cx = (s16)(x + boomOff[i][0]);
        booms[i].cy = (s16)(y - KRANG_BOOM_DZ + boomOff[i][1]);
    }
}

static void boomsUpdate(s16 camX, s16 camY) {
    if (whiteFlash) whiteFlash--;
    bool live = FALSE, any = FALSE;
    for (u16 i = 0; i < KRANG_BOOMS; i++) {
        if (!booms[i].state) continue;
        any = TRUE;
        if (booms[i].state == 1) {
            if (booms[i].wait) { booms[i].wait--; live = TRUE; continue; }
            boomStart(i);
            if (i == 0) whiteFlash = 4;         // flash blanco con la primera
            else if (i == 2) whiteFlash = 2;    // y uno corto con la tercera
        } else if (++booms[i].tick >= KRANG_BOOM_TICKS) {
            booms[i].tick = 0;
            if (++booms[i].frame >= KRANG_BOOM_FRAMES) {
                boomRelease(i);
                booms[i].state = 0;
                continue;
            }
            for (u16 q = 0; q < 4; q++)
                if (booms[i].s[q]) SPR_setFrame(booms[i].s[q], booms[i].frame);
        }
        live = TRUE;
        Sprite** sp = booms[i].s;
        for (u16 q = 1; q < 4; q++) {
            Sprite* m = sp[booms[i].owner[q]];
            if (!sp[q] || !m || booms[i].owner[q] == q) continue;
            u16 idx = m->attribut & TILE_INDEX_MASK;
            if ((sp[q]->attribut & TILE_INDEX_MASK) != idx) SPR_setVRAMTileIndex(sp[q], idx);
        }
        s16 x0 = (s16)(booms[i].cx - KRANG_BOOM_HALF - camX);
        s16 y0 = (s16)(booms[i].cy - KRANG_BOOM_HALF - camY);
        for (u16 q = 0; q < 4; q++)
            if (sp[q])
                SPR_setPosition(sp[q], (s16)(x0 + (q & 1) * KRANG_BOOM_HALF),
                                       (s16)(y0 + (q >> 1) * KRANG_BOOM_HALF));
    }
    if (any && !live) sprDefragLock = 0;   // termino la ultima
}

static void boomsReleaseAll(void) {
    bool had = FALSE;
    for (u16 i = 0; i < KRANG_BOOMS; i++) {
        if (booms[i].state) had = TRUE;
        boomRelease(i);
        booms[i].state = 0;
    }
    if (had) sprDefragLock = 0;
    whiteFlash = 0;
}

bool krangWhiteFlash(void) { return whiteFlash != 0; }

// ---------------------------------------------------------------------------
// (23/10) "I'M INVINCIBLE!": el globo (krang_inv_bubble, PAL1) mientras
// suena krang_speech_vo, al caer a la sala (lo arranca level9_1 con
// bossVoStart, ver krangJustLanded) y al escaparse la cabeza (headSpawn). Va
// arriba del que habla, con la punta del globo (x ~37 del globo) hacia el, y
// sin meterse en el HUD (filas 0-3).
// ---------------------------------------------------------------------------
#define SAY_T          116     // lo que dura krang_speech_vo (1,92 s)
#define SAY_TAIL_X      36
#define SAY_W           64
#define SAY_H           32
#define SAY_TOP_MIN     34     // debajo del HUD
static Sprite* sayBub;
static u16     sayT;
static bool    landedNow;

static void sayStart(void) {
    if (!sayBub)
        sayBub = SPR_addSprite(&krang_inv_bubble, -128, -128, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    if (sayBub) SPR_setDepth(sayBub, SPR_MIN_DEPTH);
    sayT = SAY_T;
}

static void sayRelease(void) {
    if (sayBub) { SPR_releaseSprite(sayBub); sayBub = NULL; }
    sayT = 0;
}

// (ax, ay): la boca del que habla, en pantalla.
static void sayStep(s16 ax, s16 ay) {
    if (!sayT) return;
    if (--sayT == 0) { sayRelease(); return; }
    if (!sayBub) return;
    s16 bx = kclamp((s16)(ax - SAY_TAIL_X), 4, (s16)(320 - SAY_W - 4));
    s16 by = (s16)(ay - SAY_H);
    if (by < SAY_TOP_MIN) by = SAY_TOP_MIN;
    SPR_setPosition(sayBub, bx, by);
}

bool krangJustLanded(void) {
    bool r = landedNow;
    landedNow = FALSE;
    return r;
}

void krangShotsUpdate(Player** pls, u8 nPl, s16 camX, s16 camY) {
    for (u16 i = 0; i < MAX_KRANG_FISTS; i++) {
        if (!fists[i].active) continue;
        fists[i].t++;
        fists[i].x += fists[i].dir * kStep(&fists[i].accX, KRANG_FIST_Q);
        if (fists[i].x < camX - 72 || fists[i].x > camX + 392) { fistHide(i); continue; }
        // La punta del puno alcanza a la tortuga de su lane (una vez a cada una).
        for (u8 k = 0; k < nPl; k++) {
            u8 bit = (u8)(1 << k);
            if ((fists[i].hitMask & bit) || !playerCanBeHit(pls[k])) continue;
            s16 px = getPlayerHurtCX(pls[k]);
            if (kabs((s16)(px - (fists[i].x - fists[i].dir * 8))) >= KRANG_FIST_HALF_W) continue;
            if (kabs((s16)(getPlayerY(pls[k]) - fists[i].lane)) > KRANG_FIST_DY) continue;
            fists[i].hitMask |= bit;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
            playerHitBars(pls[k], fists[i].x, KRANG_FIST_DMG);
        }
        if (fists[i].sprite) {
            // Despega con chispas (frames 1-2) y sigue con la llama (3-4).
            u8 f = (fists[i].t < 6) ? 1 : ((fists[i].t < 10) ? 2 : (u8)(3 + ((fists[i].t >> 2) & 1)));
            SPR_setFrame(fists[i].sprite, f);
            s16 left = (fists[i].dir > 0) ? (s16)(fists[i].x - 64) : fists[i].x;
            SPR_setPosition(fists[i].sprite, left - camX,
                            fists[i].lane - KRANG_FIST_DZ - 12 - camY);
            SPR_setDepth(fists[i].sprite, (s16)(-(fists[i].lane) - 1));
        }
    }
    for (u16 i = 0; i < MAX_KRANG_BEAMS; i++) {
        if (!beams[i].active) continue;
        beams[i].t++;
        if (beams[i].t > KRANG_BEAM_LIFE) { beamHide(i); continue; }
        // Baja tramo a tramo desde los ojos; cuando toca el piso, pega.
        u8 shown = (u8)(1 + beams[i].t / KRANG_BEAM_GROW);
        if (shown > KRANG_BEAM_SEGS) shown = KRANG_BEAM_SEGS;
        bool grounded = beams[i].t >= KRANG_BEAM_GROW * (KRANG_BEAM_SEGS - 1);
        bool blink = (beams[i].t > KRANG_BEAM_LIFE - 6) && (beams[i].t & 1);
        s8 dir = beams[i].dir;
        if (grounded) {
            // Pega a la tortuga de su lane que este donde el rayo ya bajo a
            // la altura del cuerpo, hasta donde toca el piso.
            for (u8 k = 0; k < nPl; k++) {
                u8 bit = (u8)(1 << k);
                if ((beams[i].hitMask & bit) || !playerCanBeHit(pls[k])) continue;
                s16 fwd = (s16)((getPlayerHurtCX(pls[k]) - beams[i].x) * dir);
                if (fwd < KRANG_BEAM_HIT_MIN || fwd > KRANG_BEAM_HIT_MAX) continue;
                if (kabs((s16)(getPlayerY(pls[k]) - beams[i].lane)) > KRANG_BOLT_DY) continue;
                beams[i].hitMask |= bit;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
                playerHitBarsKnockdown(pls[k], beams[i].x, KRANG_BOLT_DMG);
                // (16/10) El rayo electrocuta: "turtles shock pain" (CH1, voz).
                XGM2_playPCMEx(shock_pain_vo, sizeof(shock_pain_vo), SOUND_PCM_CH2, 12, FALSE, FALSE);
            }
        }
        for (u16 g = 0; g < KRANG_BEAM_SEGS; g++) {
            Sprite* sg = beams[i].seg[g];
            if (!sg) continue;
            // Tramo g: arriba a (KRANG_BEAM_TOP - 32g) de altura, su borde a
            // (KRANG_BEAM_X0 + 32g) delante del centro (espejado si mira a la
            // izquierda: el pixel 'f' delante cae en x - f - 1).
            s16 f0 = (s16)(KRANG_BEAM_X0 + g * KRANG_BEAM_SEG_H);
            s16 left = (dir > 0) ? (s16)(beams[i].x + f0)
                                 : (s16)(beams[i].x - f0 - KRANG_BEAM_SEG_W);
            SPR_setFrame(sg, (beams[i].t >> 1) & 1);
            SPR_setPosition(sg, left - camX,
                            beams[i].lane - KRANG_BEAM_TOP + g * KRANG_BEAM_SEG_H - camY);
            SPR_setDepth(sg, (s16)(-(beams[i].lane) - 2));
            SPR_setVisibility(sg, (g < shown && !blink) ? VISIBLE : HIDDEN);
        }
        if (beams[i].zap) {
            s16 lx = (s16)(beams[i].x + dir * (KRANG_BEAM_LAND + 3));
            SPR_setVisibility(beams[i].zap, (grounded && !blink) ? VISIBLE : HIDDEN);
            SPR_setFrame(beams[i].zap, (beams[i].t >> 2) & 1);
            SPR_setPosition(beams[i].zap, lx - 16 - camX, beams[i].lane - 12 - camY);
            SPR_setDepth(beams[i].zap, (s16)(-(beams[i].lane) - 3));
        }
    }
    boomsUpdate(camX, camY);
}

void krangShotsReleaseAll(void) {
    for (u16 i = 0; i < MAX_KRANG_FISTS; i++) {
        if (fists[i].sprite) { SPR_releaseSprite(fists[i].sprite); fists[i].sprite = NULL; }
        fists[i].active = FALSE;
    }
    for (u16 i = 0; i < MAX_KRANG_BEAMS; i++) {
        for (u16 g = 0; g < KRANG_BEAM_SEGS; g++)
            if (beams[i].seg[g]) { SPR_releaseSprite(beams[i].seg[g]); beams[i].seg[g] = NULL; }
        if (beams[i].zap) { SPR_releaseSprite(beams[i].zap); beams[i].zap = NULL; }
        beams[i].active = FALSE;
    }
}

// ---------------------------------------------------------------------------
// KRANG
// ---------------------------------------------------------------------------
static u16 krangPal[16], krangPalBurn[16];

void krangInit(Krang* k) {
    sayBub = NULL; sayT = 0; landedNow = FALSE;   // (23/10)
    memset(k, 0, sizeof(Krang));
    k->state = KRANG_INACTIVE;
    k->dir = -1;
    k->hp = diffBossHp(KRANG_HP); diffBossTrack(&k->hp);
    shotsInit();
}

static void setFrame(Krang* k, u8 anim, u8 frame) {
    SPR_setAnimAndFrame(k->sprite, anim, frame);
}

static void enter(Krang* k, KrangState s) {
    k->state = s;
    k->timer = 0;
    k->hitMask = 0;
}

void krangSpawn(Krang* k, s16 x, s16 arenaL, s16 arenaR, s16 laneT, s16 laneB) {
    for (u16 i = 0; i < 16; i++) krangPal[i] = krang_boss.palette->data[i];
    bossFlashBurn(krangPal, krangPalBurn);
    PAL_setPalette(PAL2, krangPal, DMA);
    bossFlashReset(&k->flashLow);
    if (!k->sprite)
        k->sprite = SPR_addSpriteSafe(&krang_boss, -160, -160, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    k->arenaL = arenaL; k->arenaR = arenaR;
    k->laneT = laneT;   k->laneB = laneB;
    k->xq = (s16)(x * 4);
    k->yq = (s16)(((laneT + laneB) / 2) * 4);
    k->z = KRANG_DROP_Z;
    k->vz = 0;
    k->dir = -1;
    k->hp = diffBossHp(KRANG_HP); diffBossTrack(&k->hp);
    k->armReady = 1;
    k->flash = k->invuln = 0;
    enter(k, KRANG_DROP);
    if (k->sprite) {
        SPR_setAutoAnimation(k->sprite, FALSE);
        setFrame(k, KRANG_ANIM_IDLE, 0);
        SPR_setVisibility(k->sprite, VISIBLE);
    }
}

bool krangIsDying(const Krang* k) {
    return k->state == KRANG_DEATH || k->state == KRANG_HEAD || k->state == KRANG_GONE;
}
bool krangIsGone(const Krang* k) { return k->state == KRANG_GONE; }

static bool canBeHit(const Krang* k) {
    return k->sprite && !k->invuln && (k->state == KRANG_IDLE || k->state == KRANG_WALK);
}

// (21/10) Una patada voladora pega UNA vez por salto: el golpeado dura 30
// ticks y la patada sigue activa todo el vuelo, asi que al terminar el
// golpeado le volvia a pegar la misma patada (dos golpes y dos sacudones por
// salto: la "respuesta rara"). Se marca por jugador y se limpia al aterrizar.
static bool kickLanded[MAX_PLAYERS];

static void kickTrack(Player** pls, u8 nPl) {
    for (u8 i = 0; i < nPl && i < MAX_PLAYERS; i++)
        if (!isPlayerJumpKicking(pls[i])) kickLanded[i] = FALSE;
}

// (21/10) Laser: la patada a la cabeza la recibe la tortuga (ver krang.h).
static void laserCounter(Krang* k, Player** pls, u8 nPl) {
    if (!k->sprite || k->state != KRANG_LASER || k->timer < KRANG_LASER_EYES_T) return;
    s16 x = (s16)(k->xq >> 2);
    s16 y = (s16)(k->yq >> 2);
    for (u8 i = 0; i < nPl; i++) {
        if (!isPlayerJumpKicking(pls[i]) || !playerCanBeHitAir(pls[i])) continue;
        if (!playerAttackHitsBossBand(pls[i], x, y, KRANG_BODY_HALF_W,
                                      (s16)(-KRANG_HEAD_TOP), (s16)(-KRANG_HEAD_BOT),
                                      KRANG_HIT_DY, KRANG_HIT_DY)) continue;
        XGM2_playPCMEx(electric_shock_sfx, sizeof(electric_shock_sfx), SOUND_PCM_CH3, 15, FALSE, FALSE);
        XGM2_playPCMEx(shock_pain_vo, sizeof(shock_pain_vo), SOUND_PCM_CH2, 12, FALSE, FALSE);
        playerHitProjectile(pls[i], x, KRANG_BOLT_DMG);   // tambien en el aire
    }
}

bool krangPlayerHits(Krang* k, Player** pls, u8 nPl, s8* killer) {
    kickTrack(pls, nPl);
    laserCounter(k, pls, nPl);
    if (!canBeHit(k)) return FALSE;
    s16 x = (s16)(k->xq >> 2);
    s16 y = (s16)(k->yq >> 2);
    for (u8 i = 0; i < nPl; i++) {
        bool jk = isPlayerJumpKicking(pls[i]);
        if (jk && i < MAX_PLAYERS && kickLanded[i]) continue;
        if (!playerAttackHitsBoss(pls[i], x, y, KRANG_BODY_HALF_W, KRANG_BODY_H,
                                  KRANG_HIT_DY, KRANG_HIT_DY)) continue;
        if (jk && i < MAX_PLAYERS) kickLanded[i] = TRUE;
        s16 dmg = isPlayerSpecialAttack(pls[i]) ? KRANG_SPECIAL_DMG
                : (isPlayerJumpKicking(pls[i]) ? KRANG_JUMPKICK_DMG : 1);
        XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
        k->hp -= dmg;
        k->flash = 8;
        k->invuln = 12;
        if (k->hp <= 0) {
            k->hp = 0;
            enter(k, KRANG_DEATH);
            PAL_setPalette(PAL2, krangPal, DMA);
            XGM2_stop();               // el tema se corta con el ultimo golpe
            XGM2_playPCMEx(boss_hit, sizeof(boss_hit), SOUND_PCM_CH3, 15, FALSE, FALSE);
            if (killer) *killer = (s8)i;
            return TRUE;
        }
        enter(k, KRANG_HURT);
        XGM2_playPCMEx(electric_shock_sfx, sizeof(electric_shock_sfx), SOUND_PCM_CH3, 12, FALSE, FALSE);
        return FALSE;
    }
    return FALSE;
}

static void toWalk(Krang* k) { enter(k, KRANG_WALK); k->accX = k->accY = 0; }

// Despues de patada / rayo / golpeado: burla si la tortuga quedo en el piso.
static void afterAction(Krang* k, bool near, bool tgtDown, u8 tauntOdds) {
    if (near) {
        if (tgtDown) enter(k, KRANG_TAUNT);
        else toWalk(k);
    } else if ((random() % tauntOdds) == 0) {
        enter(k, KRANG_TAUNT);
    } else {
        toWalk(k);
    }
}

static void faceTarget(Krang* k, s16 x, s16 px) {
    if (x < px - KRANG_TURN_DZ) k->dir = 1;
    else if (x > px + KRANG_TURN_DZ) k->dir = -1;
}

// Ataque cuerpo a cuerpo: decide entre patada y rayo.
static void meleeOrLaser(Krang* k) {
    if (random() & 1) enter(k, KRANG_KICK);
    else enter(k, KRANG_LASER);
}

static void headSpawn(Krang* k) {
    if (!k->head)
        k->head = SPR_addSprite(&krang_head, -40, -40, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (!k->headShadow)
        k->headShadow = SPR_addSprite(&krang_head, -40, -40, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (k->head) {
        SPR_setAutoAnimation(k->head, FALSE);
        SPR_setAnimAndFrame(k->head, 0, 0);
    }
    if (k->headShadow) {
        SPR_setAutoAnimation(k->headShadow, FALSE);
        SPR_setAnimAndFrame(k->headShadow, 1, 0);
    }
    k->z = KRANG_HEAD_DZ;
    XGM2_playPCMEx(krang_speech_vo, sizeof(krang_speech_vo), SOUND_PCM_CH2, 15, FALSE, FALSE);
    sayStart();                                  // (23/10) con su globo
}

static void render(Krang* k, bool visible) {
    s16 x = (s16)(k->xq >> 2);
    s16 y = (s16)(k->yq >> 2);
    SPR_setHFlip(k->sprite, k->dir < 0);
    SPR_setPosition(k->sprite, x - k->camX - KRANG_FRAME_W / 2,
                    y - k->camY - KRANG_FOOT_OFFSET - (k->state == KRANG_DROP ? k->z : 0));
    SPR_setDepth(k->sprite, (s16)(-y));
    SPR_setVisibility(k->sprite, (visible && !(k->flash & 1)) ? VISIBLE : HIDDEN);
}

static const s8 bob[16] = { 0, 1, 2, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -2, -1 };

void krangUpdate(Krang* k, Player** pls, u8 nPl, s16 camX, s16 camY) {
    if (k->state == KRANG_INACTIVE || k->state == KRANG_GONE) return;
    k->camX = camX;
    k->camY = camY;
    s16 x = (s16)(k->xq >> 2);
    s16 y = (s16)(k->yq >> 2);

    // La cabeza: flota hablando y se escapa por arriba.
    if (k->state == KRANG_HEAD) {
        k->timer++;
        if (k->timer > KRANG_HEAD_FLOAT) k->z += 2;
        s16 hz = (s16)(k->z + ((k->timer <= KRANG_HEAD_FLOAT) ? bob[(k->timer >> 2) & 15] : 0));
        if (k->head) {
            SPR_setFrame(k->head, (u8)((k->timer / 10) % 5));
            SPR_setPosition(k->head, x - 16 - camX, y - hz - 32 - camY);
            SPR_setDepth(k->head, (s16)(-y - 1));
        }
        sayStep((s16)(x - camX), (s16)(y - hz - 32 - camY));   // (23/10)
        if (k->headShadow) {
            SPR_setPosition(k->headShadow, x - 16 - camX, y - 16 - camY);
            SPR_setDepth(k->headShadow, (s16)(-y + 1));
            SPR_setVisibility(k->headShadow, (k->timer <= KRANG_HEAD_FLOAT) ? VISIBLE : HIDDEN);
        }
        if (y - hz - 32 - camY < -40) {
            if (k->head)       { SPR_releaseSprite(k->head);       k->head = NULL; }
            if (k->headShadow) { SPR_releaseSprite(k->headShadow); k->headShadow = NULL; }
            k->state = KRANG_GONE;
            sayRelease();
        }
        return;
    }
    if (!k->sprite) return;

    Player* tgt = nearestPlayer(pls, nPl, x);
    s16 px = (s16)(getPlayerWorldX(tgt) + PLAYER_SPRITE_W / 2);
    s16 py = getPlayerY(tgt);
    s16 dX = (s16)(px - x);
    s16 dY = (s16)(py - y);
    bool alignY  = kabs(dY) <= KRANG_ALIGN_DY;
    bool near    = alignY && kabs(dX) <= KRANG_NEAR_DX;
    bool far     = alignY && kabs(dX) >  KRANG_FAR_DX;
    bool gameOn  = !isPlayerGameOver(tgt);
    bool tgtDown = isDown(tgt);
    bool visible = TRUE;

    k->timer++;
    if (k->flash)  k->flash--;
    if (k->invuln) k->invuln--;

    switch (k->state) {

    case KRANG_DROP:
        // Cae del techo con la pose de quieto; al tocar el piso, burla.
        k->vz += 1;
        k->z -= k->vz;
        k->dir = (dX >= 0) ? 1 : -1;
        if (k->z <= 0) {
            k->z = 0;
            XGM2_playPCMEx(iron_ball_sfx, sizeof(iron_ball_sfx), SOUND_PCM_CH3, 14, FALSE, FALSE);
            enter(k, KRANG_TAUNT);
            // (23/10) "I'm invincible!" al empezar la pelea: la voz la arranca
            // level9_1 (bossVoStart, despues entra el tema); aca el globo.
            landedNow = TRUE;
            sayStart();
        }
        break;

    case KRANG_IDLE:
        setFrame(k, KRANG_ANIM_IDLE, 0);
        faceTarget(k, x, px);
        if (k->timer < KRANG_IDLE_T || !gameOn) break;
        if (near) enter(k, KRANG_KICK);
        else toWalk(k);
        break;

    case KRANG_WALK: {
        setFrame(k, KRANG_ANIM_WALK, (u8)((k->timer / KRANG_WALK_FRAME) % 7));
        faceTarget(k, x, px);
        s16 sx = kStep(&k->accX, KRANG_WALK_Q);
        s16 sy = kStep(&k->accY, KRANG_WALK_Q);
        if (dX > KRANG_MIN_DIST)       k->xq += sx * 4;
        else if (dX < -KRANG_MIN_DIST) k->xq -= sx * 4;
        k->yq = (s16)(kToward(y, py, sy) * 4);
        if (!gameOn || k->timer < KRANG_WALK_REACT) break;
        if (near) meleeOrLaser(k);
        else if (far && k->armReady) { enter(k, KRANG_ARM); k->armReady = 0; }
        break;
    }

    case KRANG_KICK: {
        u8 f = (k->timer < 9) ? 0 : ((k->timer < KRANG_KICK_HIT_T) ? 1 : 2);
        setFrame(k, KRANG_ANIM_KICK, f);
        if (f == 2) {
            for (u8 i = 0; i < nPl; i++) {
                u8 bit = (u8)(1 << i);
                if ((k->hitMask & bit) || !playerCanBeHit(pls[i])) continue;
                s16 fwd = (s16)((getPlayerHurtCX(pls[i]) - x) * k->dir);
                if (fwd < 0 || fwd > KRANG_KICK_REACH) continue;
                if (kabs((s16)(getPlayerY(pls[i]) - y)) > KRANG_HIT_DY) continue;
                k->hitMask |= bit;
                XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
                playerHitBarsKnockdown(pls[i], x, KRANG_KICK_DMG);
            }
        }
        if (k->timer >= KRANG_KICK_T) { k->armReady = 1; afterAction(k, near, tgtDown, 2); }
        break;
    }

    case KRANG_LASER:
        setFrame(k, KRANG_ANIM_LASER, (u8)((k->timer < 12) ? 0 : ((k->timer < KRANG_BOLT_AT) ? 1 : 2)));
        // (13/10) Laser de los ojos en diagonal hasta el piso, delante suyo
        // (antes: un relampago desde el techo sobre la tortuga).
        if (k->timer == KRANG_BOLT_AT) beamFire(x, y, k->dir);
        if (k->timer >= KRANG_LASER_T) { k->armReady = 1; afterAction(k, near, tgtDown, 2); }
        break;

    case KRANG_ARM: {
        u8 f = (u8)(k->timer / 12);
        if (f > 3) f = 3;
        setFrame(k, KRANG_ANIM_ARM, f);
        if (k->timer == KRANG_FIST_AT) fistFire((s16)(x + k->dir * KRANG_FIST_DX), y, k->dir);
        if (k->timer >= KRANG_ARM_T) {
            if (random() & 1) toWalk(k);
            else enter(k, KRANG_TAUNT);
        }
        break;
    }

    case KRANG_TAUNT:
        // 2 frames (12 y 15 ticks) dos veces.
        setFrame(k, KRANG_ANIM_TAUNT, (u8)(((k->timer % 27) < 12) ? 0 : 1));
        if (k->timer >= KRANG_TAUNT_T) toWalk(k);
        break;

    case KRANG_HURT:
        setFrame(k, KRANG_ANIM_HURT, (u8)((k->timer >> 3) & 3));
        if (k->timer >= KRANG_HURT_TICKS) afterAction(k, near, tgtDown, 3);
        break;

    case KRANG_DEATH: {
        // Grita (60), se electrocuta estallando (150) y destello amarillo (30);
        // al final el cuerpo desaparece en la explosion de Baxter (4 seguidas,
        // la primera justo donde estaba, con flash blanco) y sale la cabeza.
        u16 t = k->timer;
        if (t < 60) {
            setFrame(k, KRANG_ANIM_DEATH, (u8)((t / 15) & 1));
        } else if (t < 210) {
            setFrame(k, KRANG_ANIM_DEATH, (u8)(2 + ((t / 6) % 5)));
            if ((t % 30) == 0)
                XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                               SOUND_PCM_CH3, 14, FALSE, FALSE);
            visible = ((t >> 1) & 3) != 0;
        } else {
            setFrame(k, KRANG_ANIM_DEATH, 7);
            visible = (t & 2) != 0;
        }
        if (t >= KRANG_DEATH_T) {
            SPR_releaseSprite(k->sprite);
            k->sprite = NULL;
            krangShotsReleaseAll();
            boomsChain(x, y);
            enter(k, KRANG_HEAD);
            headSpawn(k);
            return;
        }
        break;
    }

    default:
        break;
    }

    // Parpadeo de vida baja (PAL2 es solo suya mientras pelea).
    if (k->state != KRANG_DEATH && bossFlashStep(&k->flashLow, k->hp, diffBossHp(KRANG_HP)))
        PAL_setPalette(PAL2, k->flashLow.on ? krangPalBurn : krangPal, DMA);

    x = kclamp((s16)(k->xq >> 2), k->arenaL, k->arenaR);
    k->xq = (s16)(x * 4);
    y = kclamp((s16)(k->yq >> 2), k->laneT, k->laneB);
    k->yq = (s16)(y * 4);
    render(k, visible);
    // (23/10) El globo, arriba del pecho (la cabeza del androide esta a
    // KRANG_MOUTH_DY de los pies).
    sayStep((s16)(x + k->dir * 8 - k->camX), (s16)(y - KRANG_MOUTH_DY - k->camY));
}

void krangRelease(Krang* k) {
    if (k->sprite)     { SPR_releaseSprite(k->sprite);     k->sprite = NULL; }
    if (k->head)       { SPR_releaseSprite(k->head);       k->head = NULL; }
    if (k->headShadow) { SPR_releaseSprite(k->headShadow); k->headShadow = NULL; }
    k->state = KRANG_INACTIVE;
    krangShotsReleaseAll();
    boomsReleaseAll();
    sayRelease();                                // (23/10)
}
