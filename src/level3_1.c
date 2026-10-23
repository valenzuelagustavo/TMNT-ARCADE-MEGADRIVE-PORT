// ===========================================================================
// level3_1.c - Scene 3: la CLOACA (sewer) (26/09)
// ===========================================================================
// Primer nivel armado con material del proyecto del companero (Ray Project).
// (28/09) Fondo y primer plano NUEVOS, sacados de la hoja "Arcade - Teenage
// Mutant Ninja Turtles - Backgrounds - Stage 3.png" (tools/gen_level3_1_bg.py);
// la colision sigue siendo la pintada de Ray (col_sewer.bin ->
// level3_1_limits.c, ver tools/gen_level3_1.py).
//
// Desde la Scene 4 el motor vive en stage_level.c: aca solo quedan los datos
// del nivel (fondo, franja caminable, oleadas, musica) y los ganchos del jefe.
//
// FONDO: 2253 tiles unicos, en el formato ancho de stage_bg.c (no entra en
// un IMAGE); peor caso en 42 columnas: 796. PRIMER PLANO: los caños (74
// tiles) en BG_A con prioridad alta; las tortugas pasan por detras.
//
// ESCALON (28/09): la vereda y el canal ya no son una sola franja. La cara
// del escalon (el recuadro negro pintado en "ejemplo de escalon.png":
// filas 192..213 del fondo = Y de mundo 160..181; el 29/09 se agrando un tile
// hacia abajo, hasta 189) no se pisa: desde la vereda
// uno se deja caer al agua, y desde el agua hay que SALTAR para subir (ver
// ledgeTop/ledgeBot en stage_level.h).
//
// JEFE: BAXTER STOCKMAN (baxter.c). Con las oleadas limpias y la camara en el
// fondo entra volando por la izquierda; el nivel se gana cuando su nave
// explota y no quedan ratas. PAL3 es suya.
// Musica: music_level3 (res/audio.res), el tema del sewer.
// ===========================================================================

#include <genesis.h>
#include "scenes.h"
#include "level3_1.h"          // pal_sewer, bg_sewer_tiles/map, bg_sewer_fg (rescomp)
#include "level3_1_limits.h"   // lvl31WalkTop (generado)
#include "level3_1_fgtop.h"    // lvl31FgTopX (generado, tools/gen_level3_1_bg.py)
#include "stage_level.h"
#include "enemy.h"             // ENEMY_TYPE_*
#include "baxter.h"
#include "boss_vo.h"           // (01/10)
#include "audio.h"
#include "player.h"

#define SCREEN_W            320
// (18/10) Tope de la vereda: 96 + los 8 px que se bajo la colision (ver
// SIDEWALK_DY en tools/gen_level3_1.py), para no pisar la pared.
#define LVL31_WALK_MIN      104
#define LVL31_W            1248
#define LVL31_LEDGE_TOP     160     // ultima Y de pies de la vereda
#define LVL31_LEDGE_BOT     190     // primera Y de pies del canal (29/09: +1 tile)

static const SbgRaw sewer31 = {
    (const u32*) bg_sewer_tiles,
    (const u16*) bg_sewer_map,
    LVL31_W / 8, 32,        // (29/09) 32 filas: el fondo entero hasta la fila 255
    &pal_sewer,
};

// ---------------------------------------------------------------------------
// OLEADAS: se disparan cuando los PIES del que va adelante pasan trigX; la
// camara queda clavada en lockX hasta que caen todos los de la oleada.
// ---------------------------------------------------------------------------
// (12/10) ORDENADAS SEGUN EL ARCADE (partida completa grabada,
// gameplayARCADE/tmnt.avi, 6:45-10:10): que foot soldier entra, por donde y
// con la camara en que X (se midio el scroll del video contra este fondo: el
// recorrido del arcade es el mismo, 0 -> 928). En la cloaca del arcade hay
// morados, blancos del CUCHILLO y los ROSAS del martillo (el HAMMER del port).
// Los morados salen del AGUA (salpicon y salto). Las camaras clavadas son las
// del port (lockX = donde se frenaba el arcade). trigX ~ lockX + 172 (la
// camara va 172 px detras de los pies del lider).
#define P  ENEMY_TYPE_FOOT_SOLDIER
#define KN ENEMY_TYPE_FOOT_SOLDIER_KNIFE
#define H  ENEMY_TYPE_FOOT_SOLDIER_HAMMER
#define WA STAGE_IN_WATER
// (13/10) MOUSERS DE LOS HUECOS: SP_MOUSERS con side = cuantos y x = cual
// hueco (swHoles, mas abajo). Del video: los huecos de la pared ya estan
// pintados en el fondo (el arcade los rompe en ese momento) y de ahi saltan
// los Mousers uno detras de otro mientras pelean los foot soldiers.
#define SP_MOUSERS (STAGE_SPECIAL + 0)
#define MO SP_MOUSERS
static const StageWave waves31[] = {
    // Al caer a la cloaca: un blanco por la izquierda y un morado del agua.
    {    0,   0, 2, { KN, P },  { -1, WA },  { 0, 198 } },
    // Blanco y dos rosas por la derecha.
    {  240,  70, 3, { KN, H, H }, { +1, +1, +1 }, { 0 } },
    // Rosa y blanco por la derecha.
    {  372, 200, 2, { H, KN }, { +1, +1 }, { 0 } },
    // La grande (arcade 7:16-8:06): de los dos lados y dos morados del agua.
    // (13/10) + los Mousers del hueco bajo (4) y del alto (2) (arcade 7:37).
    {  444, 272, 8, { H, KN, P, MO, KN, H, P, MO }, { +1, -1, WA, 4, -1, -1, WA, 2 },
                    { 0, 0, 550, 0, 0, 0, 470, 1 } },
    // Dos morados, uno por cada lado (en el arcade se trepan a los caños).
    // (13/10) + 4 Mousers del hueco bajo (arcade 7:51).
    {  548, 376, 3, { P, MO, P }, { +1, 4, -1 }, { 0, 0, 0 } },
    // (13/10) Antes de las rejas: 7 Mousers del hueco alto (arcade 8:05).
    {  692, 520, 1, { MO }, { 7 }, { 2 } },
    // Las rejas: cuatro blancos.
    {  822, 650, 4, { KN, KN, KN, KN }, { +1, +1, +1, -1 }, { 0 } },
    // Blancos por la izquierda, un rosa por la derecha, dos morados del agua.
    {  975, 803, 5, { KN, H, P, KN, P }, { -1, +1, WA, -1, WA },
                    { 0, 0, 1012, 0, 942 } },
    // Antes de la sala de Baxter: morado del agua y otro por la derecha.
    // (13/10) + 5 Mousers del hueco alto de la sala (arcade 8:42).
    { 1061, 889, 3, { P, MO, P }, { WA, 5, +1 }, { 1110, 4, 0 } },
    // Ya en la sala: el ultimo blanco por la izquierda, y despues Baxter.
    { 1100, 928, 1, { KN }, { -1 }, { 0 } },
};
#undef P
#undef KN
#undef H
#undef WA
#undef MO

// ---------------------------------------------------------------------------
// MISILES DEL AGUA (29/09)
// ---------------------------------------------------------------------------
// Si una tortuga se queda en el CANAL (pies >= LVL31_LEDGE_BOT) unos 4
// segundos, sale un misil del agua. Uno cada 4 segundos mientras siga en el
// agua; subirse a la vereda reinicia la cuenta.
//   - (29/09) EMERGE: aparece del lado de la pantalla MAS LEJANO a la
//     tortuga (SW_MIS_EDGE px adentro del borde), por su lane, saliendo del
//     agua desde abajo (SW_MIS_Z0) y sube hasta SW_MIS_Z. Recien ahi sale
//     derecho en X hacia la tortuga hasta dejar la camara. Mientras sube
//     no pega.
//   - Pega a las TORTUGAS (1 barra); a los foot soldiers no les hace nada.
//   - Saltando por encima (pies a mas de SW_MIS_CLEAR) se lo esquiva.
//   - Al pegar desaparece y en su lugar queda la explosion (7 frames, una
//     vez); al terminar se suelta el sprite.
// El arte (el de Traag, ver tools/gen_sewer_missile.py) va en PAL2: el misil
// mira a la DERECHA y la explosion impacta de derecha a izquierda, asi que
// el misil se espeja si va a la izquierda y la explosion si iba a la derecha.
// VRAM: 32 tiles por misil y 16 por explosion; a lo sumo uno de cada por
// tortuga (el misil cruza la pantalla en ~1,5 s y sale otro cada 4 s).
#define SW_WATER_SECS       4
#define SW_MIS_MAX          MAX_PLAYERS
#define SW_MIS_SPEED        4       // px/frame
#define SW_MIS_Z           28       // altura de vuelo sobre los pies (antes 18)
#define SW_MIS_Z0         (-10)     // arranca por debajo de la linea del agua
#define SW_MIS_RISE         2       // px/frame que sube al emerger
#define SW_MIS_EDGE        40       // px adentro del borde donde emerge
#define SW_MIS_HALF_X      14       // |dx| con el centro de la tortuga para pegar
#define SW_MIS_TOL_Y       12       // |dy| de lane para pegar
#define SW_MIS_CLEAR       40       // con los pies mas alto que esto, pasa por abajo
#define SW_MIS_DMG          1
#define SW_EXPL_FRAMES      7
#define SW_EXPL_TICKS       4       // frames de juego por frame de la explosion

// ---------------------------------------------------------------------------
// AGUA ANIMADA (29/09): como en el arcade, los dos colores del agua que no son
// el violeta se intercambian cada SW_WATER_CYCLE frames. El agua tiene sus
// propios indices en PAL0 (1 y 14, ver tools/gen_level3_1_bg.py): los grises
// originales (4 y 5) siguen siendo de los caños y las escaleras, que no se
// tocan. Son 2 palabras de CRAM por cambio, por la cola de DMA.
// ---------------------------------------------------------------------------
#define SW_WATER_A          1
#define SW_WATER_B         14
#define SW_WATER_CYCLE      8

static u16 swWaterTick;
static u8  swWaterPhase;

static void swWaterUpdate(void) {
    if (++swWaterTick < SW_WATER_CYCLE) return;
    swWaterTick = 0;
    swWaterPhase ^= 1;
    // Por la cola de DMA (en el VBlank): escribir la CRAM en pleno cuadro
    // deja puntitos en la imagen en un Mega Drive real. static: la cola lee
    // el buffer recien en el VBlank.
    static u16 ca, cb;
    const u16* c = pal_sewer.data;
    ca = swWaterPhase ? c[SW_WATER_B] : c[SW_WATER_A];
    cb = swWaterPhase ? c[SW_WATER_A] : c[SW_WATER_B];
    PAL_setColors(SW_WATER_A, &ca, 1, DMA_QUEUE);
    PAL_setColors(SW_WATER_B, &cb, 1, DMA_QUEUE);
}

static u16 swWater[MAX_PLAYERS];
// (29/09) Con el jefe no salen misiles nuevos (los que ya estan terminan):
// ademas de no meterse en la pelea, la VRAM de sprites no da para las dos
// cosas desde que la camara vertical agrando el cache del fondo.
static bool swBoss;
static struct { Sprite* spr; s16 x, lane, z; s8 dir; bool rising; } swMis[SW_MIS_MAX];
static struct { Sprite* spr; s16 x, lane; u8 frame, tick; } swBoom[SW_MIS_MAX];
// (16/10) Salpicon del misil al salir del agua (sewer_mis_splash, 5 frames de
// 64x64 en PAL3, gen_robot_enemies.py): una pasada en el punto donde emerge.
#define SW_SPLASH_FRAMES    5
#define SW_SPLASH_TICKS     5
static struct { Sprite* spr; s16 x, lane; u8 frame, tick; } swSplash[SW_MIS_MAX];

// ---------------------------------------------------------------------------
// MOUSERS DE LOS HUECOS (13/10)
// ---------------------------------------------------------------------------
// Los huecos rotos de la pared (pintados en el fondo, centro de la boca en
// coordenadas de mundo). Cada SP_MOUSERS abre uno: salta un Mouser cada
// SW_HOLE_GAP frames (con a lo sumo SW_HOLE_ALIVE vivos), cae al piso y
// ataca como los de Baxter (misma IA, baxter.c). Arte: la rata de Baxter en
// la paleta del foot blanco (PAL3 durante el nivel, gen_sewer_mouser.py).
#define SW_HOLES_N       5
#define SW_HOLE_MAX      2      // huecos soltando a la vez
#define SW_HOLE_GAP     45      // frames entre un Mouser y el siguiente
#define SW_HOLE_FIRST   20      // del hueco ya abierto al primer Mouser
#define SW_HOLE_ALIVE    3
static const s16 swHoles[SW_HOLES_N][2] = {
    { 449, 79 },    // bajo, a la derecha del primer caño redondo
    { 531, 45 },    // alto, al lado del parche claro
    { 692, 45 },    // alto, antes del caño de las rejas
    { 861, 71 },    // bajo, despues de las rejas (sin Mousers: queda roto)
    { 1045, 45 },   // alto, en la sala de Baxter
};
static struct { u8 on, hole, left; u16 t; } swHole[SW_HOLE_MAX];

// ---------------------------------------------------------------------------
// RATAS DECORATIVAS (18/10)
// ---------------------------------------------------------------------------
// Tres ratitas (sewer_rat, 32x16, PAL0) cruzan la primera parte del nivel por
// la BASE DE LA PARED (la linea de pasto, y = 124 del fondo), detras de todo
// lo que camina: dos entran por la derecha y una por la izquierda. Corren,
// frenan una o dos veces (anim [0]) y siguen hasta salir por el lado
// contrario. Puramente esteticas: no chocan ni se golpean.
// La hoja mira a la IZQUIERDA: corriendo a la derecha se espeja.
#define SW_RAT_N          3
#define SW_RAT_W         32
#define SW_RAT_TOP       85     // borde de arriba del sprite (mundo) = 124 - 15 - 32, +8 (18/10:
                                // mas abajo, sobre la vereda; ver LVL31_WALK_MIN)
#define SW_RAT_SPEED_Q  400     // 1,56 px/f (8.8)
#define SW_RAT_ANIM_T     4     // frames de juego por cuadro de la corrida
#define SW_RAT_MAX_LIFE 1800    // red de seguridad: se suelta igual a los 30 s
enum { RAT_WAIT = 0, RAT_RUN, RAT_STOP, RAT_DONE };
// camX: la camara que la hace entrar; dir: -1 entra por la derecha y corre a
// la izquierda, +1 al reves; stops: cuantas veces frena (1 o 2).
static const struct { s16 camX; s8 dir; u8 stops; } swRatDef[SW_RAT_N] = {
    {  40, -1, 1 },
    { 170, +1, 2 },
    { 320, -1, 2 },
};
static struct {
    Sprite* spr; u8 st, stops, frame, tick; s16 x; u8 acc; u16 t, life;
} swRat[SW_RAT_N];

static void swRatRelease(u16 i) {
    if (swRat[i].spr) { SPR_releaseSprite(swRat[i].spr); swRat[i].spr = NULL; }
}

static void swRatsUpdate(s16 camX) {
    for (u16 i = 0; i < SW_RAT_N; i++) {
        s8 dir = swRatDef[i].dir;
        if (swRat[i].st == RAT_DONE) continue;
        if (swRat[i].st == RAT_WAIT) {
            if (swBoss || camX < swRatDef[i].camX) continue;
            if (!sprVramFits(sewer_rat.maxNumTile)) continue;
            Sprite* s = SPR_addSprite(&sewer_rat, -64, -64, TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
            if (!s) continue;
            SPR_setAutoAnimation(s, FALSE);
            SPR_setAnimAndFrame(s, 1, 0);
            SPR_setHFlip(s, dir > 0);
            // Detras de los personajes, delante de los parches de la pared.
            SPR_setDepth(s, SPR_MAX_DEPTH - 1);
            swRat[i].spr = s;
            swRat[i].st = RAT_RUN;
            swRat[i].stops = swRatDef[i].stops;
            swRat[i].x = (dir < 0) ? (s16)(camX + SCREEN_W) : (s16)(camX - SW_RAT_W);
            swRat[i].acc = 0; swRat[i].frame = 0; swRat[i].tick = 0; swRat[i].life = 0;
            swRat[i].t = (u16)(50 + (random() % 50));   // corrida hasta el 1er freno
        }
        if (++swRat[i].life >= SW_RAT_MAX_LIFE) { swRatRelease(i); swRat[i].st = RAT_DONE; continue; }
        if (swRat[i].st == RAT_RUN) {
            u16 q = (u16)(swRat[i].acc + SW_RAT_SPEED_Q);
            swRat[i].acc = (u8)(q & 0xFF);
            swRat[i].x = (s16)(swRat[i].x + dir * (s16)(q >> 8));
            if (++swRat[i].tick >= SW_RAT_ANIM_T) {
                swRat[i].tick = 0;
                if (++swRat[i].frame >= 3) swRat[i].frame = 0;
                SPR_setAnimAndFrame(swRat[i].spr, 1, swRat[i].frame);
            }
            // Solo frena con la rata entera en pantalla.
            s16 sx = (s16)(swRat[i].x - camX);
            bool inView = (bool)(sx >= 8 && sx <= SCREEN_W - SW_RAT_W - 8);
            if (swRat[i].stops && swRat[i].t) swRat[i].t--;
            if (swRat[i].stops && !swRat[i].t && inView) {
                swRat[i].stops--;
                swRat[i].st = RAT_STOP;
                swRat[i].t = (u16)(30 + (random() % 45));   // cuanto se queda quieta
                SPR_setAnimAndFrame(swRat[i].spr, 0, 0);
            }
            // Salio por el lado contrario.
            if ((dir < 0 && sx < -SW_RAT_W) || (dir > 0 && sx > SCREEN_W)) {
                swRatRelease(i); swRat[i].st = RAT_DONE; continue;
            }
        } else if (swRat[i].st == RAT_STOP) {
            if (--swRat[i].t == 0) {
                swRat[i].st = RAT_RUN;
                swRat[i].t = (u16)(40 + (random() % 50));
                swRat[i].frame = 0; swRat[i].tick = 0;
                SPR_setAnimAndFrame(swRat[i].spr, 1, 0);
            }
        }
        SPR_setPosition(swRat[i].spr, (s16)(swRat[i].x - camX), (s16)(SW_RAT_TOP - stageCamY));
    }
}

// ---------------------------------------------------------------------------
// LA PARED SE RAJA (13/10): como en el arcade, los huecos con Mousers estan
// SANOS hasta que llegan. El fondo ya los trae rotos, asi que se tapan con un
// sprite de pared (PAL0, prioridad baja: los caños del primer plano quedan
// adelante) que al abrirse pasa por dos rajaduras y se suelta
// (tools/gen_sewer_holes.py: sewer_hole_N, 3 frames). El sprite existe solo
// mientras el hueco esta en pantalla (30 tiles cada uno).
// ---------------------------------------------------------------------------
#define SW_CRACK_T       6      // frames por paso de la rajadura
#define SW_PATCH_N       4
enum { PATCH_CLOSED = 0, PATCH_CRACKING, PATCH_OPEN };
static const struct { s16 x, y, w; u8 hole; const SpriteDefinition* def; } swPatchDef[SW_PATCH_N] = {
    {  424,  58, 48, 0, &sewer_hole_0 },   // y de mundo = y del fondo - 32
    {  508,  29, 48, 1, &sewer_hole_1 },
    {  669,  29, 48, 2, &sewer_hole_2 },
    { 1022,  29, 48, 4, &sewer_hole_3 },
};
static struct { u8 st, frame, tick; Sprite* spr; } swPatch[SW_PATCH_N];

static s8 swPatchOf(u8 hole) {
    for (u16 k = 0; k < SW_PATCH_N; k++) if (swPatchDef[k].hole == hole) return (s8)k;
    return -1;
}

static bool swHoleIsOpen(u8 hole) {
    s8 k = swPatchOf(hole);
    return (k < 0) || swPatch[(u8)k].st == PATCH_OPEN;
}

static void swPatchesUpdate(s16 camX) {
    for (u16 k = 0; k < SW_PATCH_N; k++) {
        if (swPatch[k].st == PATCH_OPEN) continue;
        if (swPatch[k].st == PATCH_CRACKING && ++swPatch[k].tick >= SW_CRACK_T) {
            swPatch[k].tick = 0;
            if (++swPatch[k].frame >= 3) {
                swPatch[k].st = PATCH_OPEN;
                if (swPatch[k].spr) { SPR_releaseSprite(swPatch[k].spr); swPatch[k].spr = NULL; }
                continue;
            }
            if (swPatch[k].spr) SPR_setFrame(swPatch[k].spr, swPatch[k].frame);
        }
        s16 sx = (s16)(swPatchDef[k].x - camX);
        bool inView = (bool)(sx < SCREEN_W && sx + swPatchDef[k].w > 0);
        if (!inView) {
            if (swPatch[k].spr) { SPR_releaseSprite(swPatch[k].spr); swPatch[k].spr = NULL; }
            continue;
        }
        if (!swPatch[k].spr) {
            if (!sprVramFits(swPatchDef[k].def->maxNumTile)) continue;
            swPatch[k].spr = SPR_addSprite(swPatchDef[k].def, -64, -64,
                                           TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
            if (!swPatch[k].spr) continue;
            SPR_setAutoAnimation(swPatch[k].spr, FALSE);
            SPR_setFrame(swPatch[k].spr, swPatch[k].frame);
            SPR_setDepth(swPatch[k].spr, SPR_MAX_DEPTH);   // detras de todos
        }
        SPR_setPosition(swPatch[k].spr, sx, (s16)(swPatchDef[k].y - stageCamY));
    }
}

static bool swHoleStart(u8 hole, u8 count) {
    if (hole >= SW_HOLES_N || !count) return TRUE;
    for (u16 i = 0; i < SW_HOLE_MAX; i++) {
        if (swHole[i].on) continue;
        swHole[i].on = 1; swHole[i].hole = hole; swHole[i].left = count;
        swHole[i].t = SW_HOLE_GAP - SW_HOLE_FIRST;
        s8 k = swPatchOf(hole);             // se raja (si estaba sano)
        if (k >= 0 && swPatch[(u8)k].st == PATCH_CLOSED) {
            swPatch[(u8)k].st = PATCH_CRACKING; swPatch[(u8)k].tick = 0;
            XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                           SOUND_PCM_CH3, 10, FALSE, FALSE);
        }
        return TRUE;
    }
    return FALSE;                           // se reintenta
}

static void swHolesUpdate(void) {
    for (u16 i = 0; i < SW_HOLE_MAX; i++) {
        if (!swHole[i].on) continue;
        if (!swHoleIsOpen(swHole[i].hole)) continue;   // todavia se esta rajando
        if (++swHole[i].t < SW_HOLE_GAP) continue;
        if (baxterRatAliveCount() >= SW_HOLE_ALIVE) continue;
        s16 hx = swHoles[swHole[i].hole][0], hy = swHoles[swHole[i].hole][1];
        // Cae a una lane de la vereda, cerca de la pared.
        s16 top = stageWalkTopAt(hx);
        if (top < LVL31_WALK_MIN) top = LVL31_WALK_MIN;
        s16 land = (s16)(top + 4 + (s16)(random() % 24));
        if (land > LVL31_LEDGE_TOP) land = LVL31_LEDGE_TOP;
        s16 x = (s16)(hx - 8 + (s16)(random() % 16));
        // Los pies arrancan en la boca del hueco (+10: la rata mide 40).
        if (!baxterRatSpawnAt(&sewer_mouser, &sewer_mouser_boom, x, land, (s16)(land - (hy + 10))))
            continue;                       // sin lugar/VRAM: el frame siguiente
        swHole[i].t = 0;
        if (--swHole[i].left == 0) swHole[i].on = 0;
    }
}

static bool swSpecial(u8 type, s8 side, s16 x, s16 camX) {
    (void)camX;
    if (type == SP_MOUSERS) return swHoleStart((u8)x, (u8)side);
    return TRUE;
}

static u16 swSpecialAlive(void) {
    u16 n = 0;
    for (u16 i = 0; i < SW_HOLE_MAX; i++) if (swHole[i].on) n++;
    if (!swBoss) n = (u16)(n + baxterRatAliveCount());
    return n;
}

static void swInit(void) {
    swBoss       = FALSE;
    swWaterTick  = 0;
    swWaterPhase = 0;
    for (u16 i = 0; i < MAX_PLAYERS; i++) swWater[i] = 0;
    for (u16 i = 0; i < SW_MIS_MAX; i++) { swMis[i].spr = NULL; swBoom[i].spr = NULL; swSplash[i].spr = NULL; }
    for (u16 i = 0; i < SW_HOLE_MAX; i++) swHole[i].on = 0;
    for (u16 k = 0; k < SW_PATCH_N; k++) {
        swPatch[k].st = PATCH_CLOSED; swPatch[k].frame = 0; swPatch[k].tick = 0; swPatch[k].spr = NULL;
    }
    for (u16 i = 0; i < SW_RAT_N; i++) { swRat[i].spr = NULL; swRat[i].st = RAT_WAIT; }
}

static void swRelease(void) {
    for (u16 i = 0; i < SW_MIS_MAX; i++) {
        if (swMis[i].spr)  { SPR_releaseSprite(swMis[i].spr);  swMis[i].spr  = NULL; }
        if (swBoom[i].spr) { SPR_releaseSprite(swBoom[i].spr); swBoom[i].spr = NULL; }
        if (swSplash[i].spr) { SPR_releaseSprite(swSplash[i].spr); swSplash[i].spr = NULL; }
    }
    baxterRatReleaseAll();
    for (u16 i = 0; i < SW_RAT_N; i++) swRatRelease(i);
    for (u16 k = 0; k < SW_PATCH_N; k++)
        if (swPatch[k].spr) { SPR_releaseSprite(swPatch[k].spr); swPatch[k].spr = NULL; }
}

static void swFire(const Player* p, s16 camX) {
    for (u16 i = 0; i < SW_MIS_MAX; i++) {
        if (swMis[i].spr) continue;
        s16 px = (s16)(getPlayerWorldX(p) + PLAYER_SPRITE_W / 2);
        // Emerge del lado mas LEJANO a la tortuga y va hacia ella.
        s8  dir = (px - camX < SCREEN_W / 2) ? -1 : 1;
        s16 x   = (dir < 0) ? (s16)(camX + SCREEN_W - SW_MIS_EDGE)
                            : (s16)(camX + SW_MIS_EDGE);
        // (16/10) Arte nuevo en la paleta del foot blanco: PAL3 (con Baxter, que
        // carga la suya en PAL3, no salen misiles: ver swBoss en swUpdate).
        Sprite* s = SPR_addSprite(&sewer_missil, -64, -64, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        if (!s) return;                     // sin VRAM: este no sale
        SPR_setAnimationLoop(s, TRUE);
        SPR_setHFlip(s, dir < 0);           // el arte mira a la derecha
        swMis[i].spr  = s;
        swMis[i].x    = x;
        swMis[i].lane = getPlayerY(p);
        swMis[i].dir  = dir;
        swMis[i].z    = SW_MIS_Z0;
        swMis[i].rising = TRUE;
        SPR_setPosition(s, (s16)(x - camX - 32), (s16)(swMis[i].lane - SW_MIS_Z0 - 16 - stageCamY));
        // El salpicon donde sale (si no hay VRAM, sale sin salpicon).
        if (!swSplash[i].spr) {
            Sprite* w = SPR_addSprite(&sewer_mis_splash, -64, -64, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
            if (w) {
                SPR_setAutoAnimation(w, FALSE);
                SPR_setAnimAndFrame(w, 0, 0);
                swSplash[i].spr   = w;
                swSplash[i].x     = x;
                swSplash[i].lane  = swMis[i].lane;
                swSplash[i].frame = 0;
                swSplash[i].tick  = 0;
            }
        }
        return;
    }
}

static void swBoomAt(s16 x, s16 lane, s8 dir) {
    for (u16 i = 0; i < SW_MIS_MAX; i++) {
        if (swBoom[i].spr) continue;
        Sprite* s = SPR_addSprite(&sewer_explosao, -32, -32, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        if (!s) return;
        SPR_setAutoAnimation(s, FALSE);
        SPR_setAnimAndFrame(s, 0, 0);
        SPR_setHFlip(s, dir > 0);           // el arte impacta de derecha a izquierda
        swBoom[i].spr   = s;
        swBoom[i].x     = x;
        swBoom[i].lane  = lane;
        swBoom[i].frame = 0;
        swBoom[i].tick  = 0;
        XGM2_playPCMEx(foot_soldier_explode, sizeof(foot_soldier_explode),
                       SOUND_PCM_CH3, 13, FALSE, FALSE);
        return;
    }
}

static void swUpdate(Player** pls, u8 nPl, s16 camX) {
    const u16 period = (u16)((IS_PAL_SYSTEM ? 50 : 60) * SW_WATER_SECS);

    swWaterUpdate();
    swPatchesUpdate(camX);
    swRatsUpdate(camX);         // (18/10) decorativas

    // (13/10) Mousers de los huecos (con Baxter los mueve bossUpdate31).
    if (!swBoss) {
        baxterRatSetArena(camX, (s16)(camX + SCREEN_W), LVL31_WALK_MIN, 216);   // como con Baxter
        swHolesUpdate();
        baxterRatUpdateAll(pls, nPl, camX, stageWalkTopAt);
        baxterRatPlayerHits(pls, nPl);
    }

    // Cuenta del agua por tortuga.
    for (u8 k = 0; k < nPl; k++) {
        const Player* p = pls[k];
        if (isPlayerGameOver(p) || !p->sprite) { swWater[k] = 0; continue; }
        s16 y = getPlayerY(p);
        if (y >= LVL31_LEDGE_BOT && !swBoss) {
            if (++swWater[k] >= period) { swWater[k] = 0; swFire(p, camX); }
        } else if (y <= LVL31_LEDGE_TOP) {
            swWater[k] = 0;                 // arriba de la vereda: se corta
        }
    }

    // Misiles.
    for (u16 i = 0; i < SW_MIS_MAX; i++) {
        if (!swMis[i].spr) continue;
        if (swMis[i].rising) {                      // saliendo del agua
            swMis[i].z += SW_MIS_RISE;
            if (swMis[i].z >= SW_MIS_Z) { swMis[i].z = SW_MIS_Z; swMis[i].rising = FALSE; }
            SPR_setPosition(swMis[i].spr, (s16)(swMis[i].x - camX - 32),
                            (s16)(swMis[i].lane - swMis[i].z - 16 - stageCamY));
            SPR_setDepth(swMis[i].spr, (s16)(-(swMis[i].lane) - 1));
            continue;
        }
        swMis[i].x += swMis[i].dir * SW_MIS_SPEED;
        if (swMis[i].x < camX - 48 || swMis[i].x > camX + SCREEN_W + 48) {
            SPR_releaseSprite(swMis[i].spr);
            swMis[i].spr = NULL;
            continue;
        }
        bool hit = FALSE;
        for (u8 k = 0; k < nPl && !hit; k++) {
            Player* p = pls[k];
            if (!playerCanBeHitAir(p)) continue;
            if (getPlayerJumpZ(p) > SW_MIS_CLEAR) continue;         // lo salto
            s16 px = (s16)(getPlayerWorldX(p) + PLAYER_SPRITE_W / 2);
            s16 dx = (s16)(px - swMis[i].x), dy = (s16)(getPlayerY(p) - swMis[i].lane);
            if (dx < 0) dx = (s16)-dx;
            if (dy < 0) dy = (s16)-dy;
            if (dx >= SW_MIS_HALF_X || dy >= SW_MIS_TOL_Y) continue;
            XGM2_playPCMEx(hit_turtles, sizeof(hit_turtles), SOUND_PCM_CH3, 15, FALSE, FALSE);
            playerHitProjectile(p, swMis[i].x, SW_MIS_DMG);
            hit = TRUE;
        }
        if (hit) {
            swBoomAt(swMis[i].x, swMis[i].lane, swMis[i].dir);
            SPR_releaseSprite(swMis[i].spr);
            swMis[i].spr = NULL;
            continue;
        }
        SPR_setPosition(swMis[i].spr, (s16)(swMis[i].x - camX - 32),
                        (s16)(swMis[i].lane - swMis[i].z - 16 - stageCamY));
        SPR_setDepth(swMis[i].spr, (s16)(-(swMis[i].lane) - 1));
    }

    // Salpicones: una pasada de los 5 frames y se sueltan.
    for (u16 i = 0; i < SW_MIS_MAX; i++) {
        if (!swSplash[i].spr) continue;
        if (++swSplash[i].tick >= SW_SPLASH_TICKS) {
            swSplash[i].tick = 0;
            if (++swSplash[i].frame >= SW_SPLASH_FRAMES) {
                SPR_releaseSprite(swSplash[i].spr);
                swSplash[i].spr = NULL;
                continue;
            }
            SPR_setFrame(swSplash[i].spr, swSplash[i].frame);
        }
        // El centro del agua del arte esta en la fila 59 de la celda de 64.
        SPR_setPosition(swSplash[i].spr, (s16)(swSplash[i].x - camX - 32),
                        (s16)(swSplash[i].lane - 59 - stageCamY));
        SPR_setDepth(swSplash[i].spr, (s16)(-(swSplash[i].lane)));
    }

    // Explosiones: una pasada de los 7 frames y se sueltan.
    for (u16 i = 0; i < SW_MIS_MAX; i++) {
        if (!swBoom[i].spr) continue;
        if (++swBoom[i].tick >= SW_EXPL_TICKS) {
            swBoom[i].tick = 0;
            if (++swBoom[i].frame >= SW_EXPL_FRAMES) {
                SPR_releaseSprite(swBoom[i].spr);
                swBoom[i].spr = NULL;
                continue;
            }
            SPR_setFrame(swBoom[i].spr, swBoom[i].frame);
        }
        SPR_setPosition(swBoom[i].spr, (s16)(swBoom[i].x - camX - 16),
                        (s16)(swBoom[i].lane - SW_MIS_Z - 16 - stageCamY));
        SPR_setDepth(swBoom[i].spr, (s16)(-(swBoom[i].lane) - 2));
    }
}

// ---------------------------------------------------------------------------
// Ganchos del jefe: Baxter y sus ratas
// ---------------------------------------------------------------------------
static Baxter baxter;

static void bossInit31(void) {
    baxterInit(&baxter);
    baxterRatInitAll();
}

static void bossStart31(s16 camX, s16 levelW) {
    (void)levelW;
    swBoss = TRUE;
    baxterSpawn(&baxter, (s16)(camX + 16), (s16)(camX + SCREEN_W - 16), LVL31_WALK_MIN, 216);
    // (14/10) Sin voice over: el "I'm invincible!" es de Krang, no de Baxter
    // (estaba mal asignado). El tema del jefe lo arranca stage_level al
    // entrar (bossMusicByVo = FALSE).
}

static bool bossUpdate31(Player** pls, u8 nPl, s16 camX) {
    baxterUpdate(&baxter, pls, nPl, camX);
    baxterRatUpdateAll(pls, nPl, camX, stageWalkTopAt);
    s8 killer = -1;
    if (baxterPlayerHits(&baxter, pls, nPl, &killer) && killer >= 0)
        addPlayerScore(pls[(u8)killer], 5);
    baxterRatPlayerHits(pls, nPl);
    return baxterIsGone(&baxter) && baxterRatBusyCount() == 0;
}

static bool bossDying31(void) {
    return baxter.state == BAXTER_DEAD || baxter.state == BAXTER_GONE;
}

static void bossRelease31(void) {
    baxterRelease(&baxter);
}

static const StageLevel level31 = {
    .bgRaw        = &sewer31,
    .fg           = &bg_sewer_fg,
    .fgTop        = &sewer_fg_top,  // los caños siguen en la franja del HUD
    .fgTopX       = lvl31FgTopX,
    .fgTopN       = LVL31_FGTOP_N,
    .fgTopF       = lvl31FgTopF,    // frames compartidos (6 columnas distintas)
    .fgTopFrames  = LVL31_FGTOP_FRAMES,
    .camYMin      = -32,            // (29/09) la camara sube 32 px: se ve el techo
    .bgSlots      = 864,            // >= 861, el peor caso medido (fondo de 32 filas)
    .levelW       = LVL31_W,
    .walkTop      = lvl31WalkTop,
    .walkCols     = LVL31_COLS,
    .walkYMin     = LVL31_WALK_MIN, // tope de la vereda (pies)
    .walkYMax     = 216,            // piso del canal
    .ledgeTop     = LVL31_LEDGE_TOP,
    .ledgeBot     = LVL31_LEDGE_BOT,
    .waves        = waves31,
    .nWaves       = sizeof(waves31) / sizeof(waves31[0]),
    .bossFeetX    = 1000,
    .music        = music_level3,
    .musicVol     = 80,
    .bossMusic    = music_boss,
    .bossMusicVol = 80,
    .bossMusicByVo = FALSE,     // (14/10) sin voz: el tema entra directo
    .bossInit     = bossInit31,
    .bossStart    = bossStart31,
    .bossUpdate   = bossUpdate31,
    .bossDying    = bossDying31,
    .bossRelease  = bossRelease31,
    .levelInit    = swInit,
    .levelUpdate  = swUpdate,
    .levelRelease = swRelease,
    .specialSpawn = swSpecial,
    .specialAlive = swSpecialAlive,
    .nextScene    = SCENE_CUT_SHREDDER_APRIL,   // (18/10) fin de la SCENE 2
};

SceneId showScene31() {
    return stageLevelRun(&level31);
}
