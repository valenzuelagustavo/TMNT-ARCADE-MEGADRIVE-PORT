// ===========================================================================
// cut_scenes.c - Cinematicas de UNA imagen entre niveles (23/10)
// ===========================================================================
// SCENE_CUT_VAN (despues del nivel de las patinetas): la imagen del choque de
// la camioneta (cut_accident, cutscenes.res). Fundido desde negro, se queda
// CUT_HOLD_SECS segundos (START de cualquier joystick la adelanta), fundido a
// negro y sigue el titulo de la SCENE 5. (27/10) Si juega Leo, se le suma su
// sprite (cut_leo) sobre la imagen.
//
// SCENE_CUT_SPLINTER (27/10, despues de la fabrica): Splinter agradece a las
// tortugas ("Thank you, my turtles!"). El fondo (cut_splinter_bg) lleva el
// globo y el cuerpo de Splinter; solo la parte que cambia entre los 13 cuadros
// (cabeza y boca, 64x40) es un sprite (cut_splinter_mouth), que recorre sus
// cuadros al ritmo de la voz. START la saltea.
//
// SCENE_CUT_TECHNODROME (29/10, antes del nivel del Technodrome): el Technodrome
// (fondo en BG_B + primer plano en BG_A) con la musica "It's Technodrome" (una
// pasada, sin loop). Si juega Leo, aparece su sprite y al rato el globo "There
// it is". Las venas grises del tunel titilan con animacion de paleta (cut_techno_pal.h).
// START la saltea. Arte: tools/gen_cut_techno.py.
//
// SCENE_CUT_KISS (30/10, despues del garage): April besa a la tortuga ("I owe you one"). 24 cuadros
// del arcade en dos planos (la tortuga en BG_B, April en BG_A); la BANDANA (y las munequeras) se
// repinta con el color de la tortuga que juega, o con la del jugador de mas puntos si juegan varios.
// START la saltea. Arte: tools/gen_cut_kiss.py.
// ===========================================================================
#include <genesis.h>
#include "scenes.h"
#include "cutscenes.h"   // cut_accident, cut_splinter_* (rescomp)
#include "audio.h"       // thank_you_vo, come_get_vo, shell_brain_vo
#include "cut_techno_pal.h"  // (29/10) animacion por paleta del fondo del Technodrome (generado)
#include "cut_kiss.h"        // (30/10) medidas de la cutscene del beso (generado)
#include "hud.h"         // numJugadores, playerChar

#define CUT_HOLD_SECS   4
#define CUT_FADE_TICKS 20

// (27/10) ovl: sprite opcional sobre la imagen (la tortuga del choque), con su
// paleta en PAL1; se funde junto con el fondo.
static SceneId cutImage(const Image* img, const SpriteDefinition* ovl, s16 ovlX, s16 ovlY,
                        SceneId next) {
    clearScene();                     // paletas en negro, planos y sprites limpios
    while ((JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2)) & BUTTON_START)
        SYS_doVBlankProcess();

    VDP_setScreenWidth320();
    VDP_setBackgroundColor(0);
    if (ovl) {
        SPR_end();
        SPR_initEx(96);               // un sprite de 64 tiles
    }
    VDP_drawImageEx(BG_B, img,
                    TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX),
                    0, 0, FALSE, TRUE);
    u16 pal[32];
    u16 lastCol = 15;
    memcpy(pal, img->palette->data, 16 * sizeof(u16));
    if (ovl) {
        Sprite* s = SPR_addSprite(ovl, ovlX, ovlY, TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
        if (s) {
            SPR_setAutoAnimation(s, FALSE);
            memcpy(pal + 16, ovl->palette->data, 16 * sizeof(u16));
            lastCol = 31;
        }
        SPR_update();
    }
    SYS_doVBlankProcess();
    PAL_fadeIn(0, lastCol, pal, CUT_FADE_TICKS, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    u16 timer = (u16)((IS_PAL_SYSTEM ? 50 : 60) * CUT_HOLD_SECS);
    u16 prev = 0xFFFF;
    while (timer > 0) {
        timer--;
        u16 v = JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2);
        if ((v & ~prev & BUTTON_START) && prev != 0xFFFF) break;
        prev = v;
        SPR_update();
        SYS_doVBlankProcess();
    }

    clearScene();                     // fundido a negro
    if (ovl) {
        SPR_end();
        SPR_initEx(752);              // lo que esperan las demas escenas
    }
    return next;
}

// (27/10) Posicion (pantalla) del sprite de la tortuga: la de LEO_TURTLES.png
#define CUT_TURTLE_X    80
#define CUT_TURTLE_Y    96

SceneId showCutVan(void) {
    // La tortuga (sprite) se muestra si alguna de las que juegan es esa. Por
    // ahora solo esta Leo (personaje 0); las otras, cuando lleguen sus PNG.
    bool leo = FALSE;
    u8 n = numJugadores();
    for (u8 k = 0; k < n; k++) if (playerChar(k) == 0) leo = TRUE;
    return cutImage(&cut_accident, leo ? &cut_leo : NULL, CUT_TURTLE_X, CUT_TURTLE_Y, SCENE_S5_TITLE);
}

// ---------------------------------------------------------------------------
// SCENE_CUT_SPLINTER
// ---------------------------------------------------------------------------
#define SPL_MOUTH_X      168
#define SPL_MOUTH_Y      104
#define SPL_FRAMES        13
#define SPL_PRE_TICKS     24     // quieto un momento antes de hablar
#define SPL_TALK_NTSC     74     // la voz dura 1,24 s: 74 cuadros a 60 Hz ...
#define SPL_TALK_PAL      62     // ... y 62 a 50 Hz
#define SPL_POST_SECS      2     // se queda con el ultimo cuadro
#define SPL_VO_PRIO       15

static bool splStart(u16* prev) {
    u16 v = JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2);
    bool go = (v & ~(*prev) & BUTTON_START) != 0;
    *prev = v;
    return go;
}

SceneId showCutSplinter(void) {
    clearScene();
    while ((JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2)) & BUTTON_START)
        SYS_doVBlankProcess();

    VDP_setScreenWidth320();
    VDP_setBackgroundColor(0);
    SPR_end();
    SPR_initEx(64);                   // un solo sprite de 40 tiles

    VDP_drawImageEx(BG_B, &cut_splinter_bg,
                    TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX),
                    0, 0, FALSE, TRUE);
    Sprite* mouth = SPR_addSprite(&cut_splinter_mouth, SPL_MOUTH_X, SPL_MOUTH_Y,
                                  TILE_ATTR(PAL0, FALSE, FALSE, FALSE));
    if (mouth) {
        SPR_setAutoAnimation(mouth, FALSE);
        SPR_setFrame(mouth, 0);
    }
    SPR_update();
    SYS_doVBlankProcess();
    PAL_fadeIn(0, 15, cut_splinter_bg.palette->data, CUT_FADE_TICKS, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    const u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    const u16 talk = IS_PAL_SYSTEM ? SPL_TALK_PAL : SPL_TALK_NTSC;
    u16 prev = 0xFFFF;
    bool skip = FALSE;

    for (u16 t = 0; t < SPL_PRE_TICKS && !skip; t++) {
        skip = splStart(&prev) && prev != 0xFFFF;
        SPR_update();
        SYS_doVBlankProcess();
    }
    if (!skip)
        XGM2_playPCMEx(thank_you_vo, sizeof(thank_you_vo), SOUND_PCM_CH2, SPL_VO_PRIO, TRUE, FALSE);
    for (u16 t = 0; t < talk && !skip; t++) {
        if (mouth) SPR_setFrame(mouth, (s16)(((u32)t * SPL_FRAMES) / talk));
        skip = splStart(&prev);
        SPR_update();
        SYS_doVBlankProcess();
    }
    if (mouth) SPR_setFrame(mouth, 0);       // boca cerrada
    for (u16 t = 0; t < fps * SPL_POST_SECS && !skip; t++) {
        skip = splStart(&prev);
        SPR_update();
        SYS_doVBlankProcess();
    }
    if (skip) XGM2_stopPCM(SOUND_PCM_CH2);

    clearScene();                     // fundido a negro
    SPR_end();
    SPR_initEx(752);                  // lo que esperan las demas escenas
    return SCENE_DOJO;
}

// ---------------------------------------------------------------------------
// SCENE_CUT_SHREDDER_APRIL (28/10, despues de The Sewer)
// ---------------------------------------------------------------------------
// Shredder se lleva a April (fondo + April como sprite); Bebop y Rocksteady van
// en el primer plano (BG_A). Rocksteady provoca a las tortugas: suena
// "Come and get'em" y despues "shell-brain", con el globo en pantalla y la boca
// (sprite de 64x64, cerrada/abierta) moviendose mientras dura cada voz. Al
// terminar el globo desaparece y la boca queda cerrada. START la saltea.
// Arte: tools/gen_cut_sewer.py. Paletas: fondo PAL0, April PAL1, primer plano,
// boca y globo PAL2.
#define SWR_APRIL_X      120
#define SWR_APRIL_Y       80
#define SWR_MOUTH_X      222
#define SWR_MOUTH_Y      160
#define SWR_BUBBLE_X     192
#define SWR_BUBBLE_Y      96
#define SWR_PRE_TICKS     45     // quieto antes de que hable
#define SWR_GAP_TICKS      8     // entre las dos frases
#define SWR_MOUTH_TICKS    6     // cuadros por posicion de la boca
#define SWR_END_OPEN      10     // al sacar el globo: la boca abierta un instante
#define SWR_POST_SECS      3     // se queda con el cuadro final
#define SWR_VO_RATE    6650      // Hz de los WAV (audio.res los deja a 6650, halfRate)

// Habla durante 'ticks' cuadros (la boca abre y cierra). TRUE si se salteo.
static bool swrTalk(Sprite* mouth, u16 ticks, u16* prev) {
    bool skip = FALSE;
    for (u16 t = 0; t < ticks && !skip; t++) {
        if (mouth) SPR_setFrame(mouth, (s16)((t / SWR_MOUTH_TICKS) & 1 ? 0 : 1));
        skip = splStart(prev);
        SPR_update();
        SYS_doVBlankProcess();
    }
    if (mouth) SPR_setFrame(mouth, 0);
    return skip;
}

static bool swrWait(u16 ticks, u16* prev) {
    bool skip = FALSE;
    for (u16 t = 0; t < ticks && !skip; t++) {
        skip = splStart(prev);
        SPR_update();
        SYS_doVBlankProcess();
    }
    return skip;
}

SceneId showCutShredderApril(void) {
    clearScene();
    while ((JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2)) & BUTTON_START)
        SYS_doVBlankProcess();

    VDP_setScreenWidth320();
    VDP_setBackgroundColor(0);
    SPR_end();
    SPR_initEx(320);                  // April (160 tiles) + boca (64) + globo (48)

    u16 ti = TILE_USER_INDEX;
    VDP_drawImageEx(BG_B, &cut_sewer_bg, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, ti),
                    0, 0, FALSE, TRUE);
    ti = (u16)(ti + cut_sewer_bg.tileset->numTile);
    VDP_drawImageEx(BG_A, &cut_sewer_fg, TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, ti),
                    0, 0, FALSE, TRUE);

    Sprite* april = SPR_addSprite(&cut_sewer_april, SWR_APRIL_X, SWR_APRIL_Y,
                                  TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
    Sprite* mouth = SPR_addSprite(&cut_sewer_mouth, SWR_MOUTH_X, SWR_MOUTH_Y,
                                  TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    Sprite* bubble = SPR_addSprite(&cut_sewer_bubble, SWR_BUBBLE_X, SWR_BUBBLE_Y,
                                   TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (april) SPR_setAutoAnimation(april, FALSE);
    if (mouth) { SPR_setAutoAnimation(mouth, FALSE); SPR_setFrame(mouth, 0); }
    if (bubble) { SPR_setAutoAnimation(bubble, FALSE); SPR_setVisibility(bubble, HIDDEN); }
    SPR_update();

    u16 pal[48];
    memcpy(pal, cut_sewer_bg.palette->data, 16 * sizeof(u16));
    memcpy(pal + 16, cut_sewer_april.palette->data, 16 * sizeof(u16));
    memcpy(pal + 32, cut_sewer_fg.palette->data, 16 * sizeof(u16));
    SYS_doVBlankProcess();
    PAL_fadeIn(0, 47, pal, CUT_FADE_TICKS, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    const u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    u16 prev = 0xFFFF;
    bool skip = FALSE;

    skip = swrWait(SWR_PRE_TICKS, &prev) && prev != 0xFFFF;
    if (!skip) {
        if (bubble) SPR_setVisibility(bubble, VISIBLE);
        XGM2_playPCMEx(come_get_vo, sizeof(come_get_vo), SOUND_PCM_CH2, SPL_VO_PRIO, TRUE, FALSE);
        skip = swrTalk(mouth, (u16)(((u32)sizeof(come_get_vo) * fps) / SWR_VO_RATE), &prev);
    }
    if (!skip) skip = swrWait(SWR_GAP_TICKS, &prev);
    if (!skip) {
        XGM2_playPCMEx(shell_brain_vo, sizeof(shell_brain_vo), SOUND_PCM_CH2, SPL_VO_PRIO, TRUE, FALSE);
        skip = swrTalk(mouth, (u16)(((u32)sizeof(shell_brain_vo) * fps) / SWR_VO_RATE), &prev);
    }
    if (bubble) SPR_setVisibility(bubble, HIDDEN);
    if (!skip) {                      // sin globo, la boca abierta un instante y se cierra
        if (mouth) SPR_setFrame(mouth, 1);
        skip = swrWait(SWR_END_OPEN, &prev);
        if (mouth) SPR_setFrame(mouth, 0);
    }
    if (!skip) skip = swrWait((u16)(fps * SWR_POST_SECS), &prev);
    if (skip) XGM2_stopPCM(SOUND_PCM_CH2);

    clearScene();                     // fundido a negro
    SPR_end();
    SPR_initEx(752);                  // lo que esperan las demas escenas
    return SCENE_S3_TITLE;
}

// ---------------------------------------------------------------------------
// SCENE_CUT_TECHNODROME (29/10, antes del nivel del Technodrome)
// ---------------------------------------------------------------------------
// Fondo PAL0 (BG_B), Technodrome PAL1 (BG_A), Leo y globo PAL2 (sprites). La
// VRAM va justa (881 + 460 tiles de planos y 62 de sprites), asi que se usa el
// plano de 64x32 (el de mas espacio de tiles) y SPR_initEx(64).
#define TCH_LEO_X          194
#define TCH_LEO_Y          184
#define TCH_BUBBLE_X       178
#define TCH_BUBBLE_Y       152
#define TCH_BUBBLE_SECS      2     // el globo sale a los 2 s
#define TCH_TOTAL_SECS      10     // la musica dura 9,8 s (una pasada)
#define TCH_MUSIC_VOL       80
#define TCH_PAL_STEP_TICKS   8     // ticks por paso de la animacion de paleta (5 pasos = un ciclo)

SceneId showCutTechnodrome(void) {
    clearScene();
    while ((JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2)) & BUTTON_START)
        SYS_doVBlankProcess();

    VDP_setScreenWidth320();
    SPR_end();
    VDP_setPlaneSize(64, 32, TRUE);   // ~1440 tiles de usuario
    VDP_setBackgroundColor(0);
    SPR_initEx(64);                   // Leo (30 tiles) + globo (32)

    u16 ti = TILE_USER_INDEX;
    VDP_drawImageEx(BG_B, &cut_techno_bg, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, ti),
                    0, 0, FALSE, TRUE);
    ti = (u16)(ti + cut_techno_bg.tileset->numTile);
    VDP_drawImageEx(BG_A, &cut_techno_fg, TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, ti),
                    0, 0, FALSE, TRUE);

    // Leo y su globo, solo si juega Leo (las otras tortugas, cuando lleguen sus PNG)
    bool leo = FALSE;
    u8 n = numJugadores();
    for (u8 k = 0; k < n; k++) if (playerChar(k) == 0) leo = TRUE;
    Sprite* leoSpr = NULL;
    Sprite* bubble = NULL;
    if (leo) {
        leoSpr = SPR_addSprite(&cut_techno_leo, TCH_LEO_X, TCH_LEO_Y,
                               TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        bubble = SPR_addSprite(&cut_techno_bubble, TCH_BUBBLE_X, TCH_BUBBLE_Y,
                               TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        if (leoSpr) SPR_setAutoAnimation(leoSpr, FALSE);
        if (bubble) { SPR_setAutoAnimation(bubble, FALSE); SPR_setVisibility(bubble, HIDDEN); }
    }
    SPR_update();

    u16 pal[48];
    memcpy(pal, cut_techno_bg.palette->data, 16 * sizeof(u16));
    memcpy(pal + 16, cut_techno_fg.palette->data, 16 * sizeof(u16));
    memcpy(pal + 32, cut_techno_leo.palette->data, 16 * sizeof(u16));
    pal[0] = 0;                       // el backdrop (indice 0 de PAL0) en negro
    SYS_doVBlankProcess();

    // Musica: una sola pasada. setLoopNumber va SIEMPRE antes del play (el
    // driver lo latchea en ese instante); se restaura al salir.
    XGM2_setLoopNumber(0);
    playMusicVol(music_techno_cutscene, TCH_MUSIC_VOL);

    PAL_fadeIn(0, 47, pal, CUT_FADE_TICKS, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    const u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    u16 prev = 0xFFFF;
    bool skip = FALSE;
    const u16 total = (u16)(fps * TCH_TOTAL_SECS);
    for (u16 t = 0; t < total && !skip; t++) {
        if (bubble && t == fps * TCH_BUBBLE_SECS) SPR_setVisibility(bubble, VISIBLE);
        // Venas del tunel: cuatro indices de PAL0 cambian de color en cada paso
        if (t % TCH_PAL_STEP_TICKS == 0) {
            const u16* row = TCH_PAL_ANIM[(t / TCH_PAL_STEP_TICKS) % TCH_PAL_STEPS];
            for (u8 k = 0; k < TCH_PAL_N; k++) PAL_setColor(TCH_PAL_IDX[k], row[k]);
        }
        skip = splStart(&prev);
        SPR_update();
        SYS_doVBlankProcess();
    }

    XGM2_setLoopNumber(-1);
    clearScene();                     // fundido a negro (y corta la musica)
    SPR_end();
    SPR_initEx(752);                  // lo que esperan las demas escenas
    return SCENE_S6_TITLE;
}

// ---------------------------------------------------------------------------
// SCENE_CUT_KISS (30/10, despues del garage)
// ---------------------------------------------------------------------------
// Dos planos de 40x28 tiles (la tortuga en BG_B con PAL0, April en BG_A con PAL1) y el globo como
// sprite (PAL2). Los 893 tiles unicos de los 24 cuadros se cargan una vez; de un cuadro al
// siguiente solo se reescriben los tiles del mapa que cambian (kiss_delta: ~3,6 KB en total).
// VRAM: 893 tiles de planos + 32 del globo (plano de 64x32 y SPR_initEx(64), como el Technodrome).
#define KISS_STEP_TICKS   10     // ticks por cuadro del arcade (24 cuadros = 4 s a 60 Hz)
#define KISS_POST_SECS     2     // se queda con el ultimo cuadro
#define KISS_LOAD_CHUNK   64     // tiles por cuadro de video al cargar (2 KB de DMA)
#define KISS_BAND_LIGHT   10     // indices de la bandana en PAL0 (el claro y el oscuro de Leo)
#define KISS_BAND_DARK    11

// Colores de la bandana por tortuga (orden de playerChar: Leo, Mike, Don, Raph; 4 = Slash)
static const u16 KISS_BAND[5][2] = {
    { RGB24_TO_VDPCOLOR(0x0077CE), RGB24_TO_VDPCOLOR(0x480091) },   // Leo: azul (el del arcade)
    { RGB24_TO_VDPCOLOR(0xFF5700), RGB24_TO_VDPCOLOR(0xCC3C00) },   // Mike: naranja
    { RGB24_TO_VDPCOLOR(0x9124DA), RGB24_TO_VDPCOLOR(0x480091) },   // Don: violeta
    { RGB24_TO_VDPCOLOR(0xCE0000), RGB24_TO_VDPCOLOR(0x920000) },   // Raph: rojo
    { RGB24_TO_VDPCOLOR(0xA83080), RGB24_TO_VDPCOLOR(0x702858) },   // Slash
};

// Tortuga que recibe el beso: la del jugador con mas puntos (en empate, el de menor numero de
// jugador; los que se quedaron sin vidas solo cuentan si no queda otro).
static u8 kissTurtle(void) {
    u8 n = numJugadores();
    u8 best = 0;
    u16 bestScore = 0;
    bool bestOut = TRUE;
    for (u8 k = 0; k < n; k++) {
        u8 lives;
        u16 score;
        s16 hp;
        bool out;
        playerPersistPeek(k, &lives, &score, &hp, &out);
        if (k == 0 || (bestOut && !out) || (out == bestOut && score > bestScore)) {
            best = k;
            bestScore = score;
            bestOut = out;
        }
    }
    u8 ch = playerChar(best);
    return (ch > 4) ? 0 : ch;
}

// Voz "I owe you one": cuando salga el globo. Aun sin audio: agregar el WAV a audio.res
// (WAV owe_you_vo "audio/6650/owe_you.wav" XGM2 6650) y descomentar la linea.
static void kissVoice(void) {
    // XGM2_playPCMEx(owe_you_vo, sizeof(owe_you_vo), SOUND_PCM_CH2, SPL_VO_PRIO, TRUE, FALSE);
}

// Aplica una lista del delta (n, n x (posicion, entrada)) a un plano; devuelve lo que sigue.
static const u16* kissApply(const u16* d, VDPPlane plane, u16 ti) {
    u16 n = *d++;
    while (n--) {
        u16 pos = d[0];
        u16 e = d[1];
        d += 2;
        VDP_setTileMapXY(plane, (u16)(e + ti), (u16)(pos % KISS_COLS), (u16)(pos / KISS_COLS));
    }
    return d;
}

SceneId showCutKiss(void) {
    clearScene();
    while ((JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2)) & BUTTON_START)
        SYS_doVBlankProcess();

    VDP_setScreenWidth320();
    SPR_end();
    VDP_setPlaneSize(64, 32, TRUE);
    VDP_setBackgroundColor(0);
    SPR_initEx(64);                   // el globo (32 tiles)

    // Tiles: por la cola de DMA, de a KISS_LOAD_CHUNK por cuadro (la pantalla esta en negro)
    const u16 ti = TILE_USER_INDEX;
    for (u16 t = 0; t < KISS_TILES; t = (u16)(t + KISS_LOAD_CHUNK)) {
        u16 n = (u16)(KISS_TILES - t);
        if (n > KISS_LOAD_CHUNK) n = KISS_LOAD_CHUNK;
        DMA_queueDma(DMA_VRAM, (void*)((const u32*)kiss_tiles + (u16)(t * 8)),
                     (u16)((ti + t) * 32), (u16)(n * 16), 2);
        SYS_doVBlankProcess();
    }
    // Cuadro 1: el mapa de la tortuga (BG_B) y el de April (BG_A); la paleta va en cada entrada
    const u16* map = (const u16*)kiss_map;
    VDP_setTileMapDataRectEx(BG_B, map, ti, 0, 0, KISS_COLS, KISS_ROWS, KISS_COLS, CPU);
    VDP_setTileMapDataRectEx(BG_A, map + KISS_COLS * KISS_ROWS, ti, 0, 0, KISS_COLS, KISS_ROWS,
                             KISS_COLS, CPU);
    const u16* delta = (const u16*)kiss_delta;

    Sprite* bubble = SPR_addSprite(&kiss_bubble, KISS_BUB_X, KISS_BUB_Y,
                                   TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    if (bubble) { SPR_setAutoAnimation(bubble, FALSE); SPR_setVisibility(bubble, HIDDEN); }
    SPR_update();

    // Paletas: PAL0 la tortuga (con la bandana del jugador), PAL1 April, PAL2 el globo (la de la
    // tortuga SIN repintar). El indice 0 de cada una es transparente / backdrop: negro.
    u16 pal[48];
    memcpy(pal, kiss_pal_t.data, 16 * sizeof(u16));
    memcpy(pal + 16, kiss_pal_a.data, 16 * sizeof(u16));
    memcpy(pal + 32, kiss_pal_t.data, 16 * sizeof(u16));
    pal[0] = pal[16] = pal[32] = 0;
    u8 ch = kissTurtle();
    pal[KISS_BAND_LIGHT] = KISS_BAND[ch][0];
    pal[KISS_BAND_DARK] = KISS_BAND[ch][1];
    SYS_doVBlankProcess();
    PAL_fadeIn(0, 47, pal, CUT_FADE_TICKS, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();

    const u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    // A 50 Hz los cuadros duran lo mismo en segundos: 10 ticks NTSC = 8 PAL
    const u16 step = IS_PAL_SYSTEM ? (u16)((KISS_STEP_TICKS * 5 + 3) / 6) : KISS_STEP_TICKS;
    u16 prev = 0xFFFF;
    bool skip = FALSE;
    for (u16 f = 0; f < KISS_FRAMES && !skip; f++) {
        if (f > 0) {
            delta = kissApply(delta, BG_B, ti);
            delta = kissApply(delta, BG_A, ti);
        }
        if (f == KISS_BUB_FIRST) {
            if (bubble) SPR_setVisibility(bubble, VISIBLE);
            kissVoice();
        }
        if (f == KISS_BUB_LAST + 1 && bubble) SPR_setVisibility(bubble, HIDDEN);
        for (u16 t = 0; t < step && !skip; t++) {
            skip = splStart(&prev) && prev != 0xFFFF;
            SPR_update();
            SYS_doVBlankProcess();
        }
    }
    for (u16 t = 0; t < fps * KISS_POST_SECS && !skip; t++) {
        skip = splStart(&prev);
        SPR_update();
        SYS_doVBlankProcess();
    }
    if (skip) XGM2_stopPCM(SOUND_PCM_CH2);

    clearScene();                     // fundido a negro
    SPR_end();
    SPR_initEx(752);                  // lo que esperan las demas escenas
    return SCENE_S4_TITLE;
}
