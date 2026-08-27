// ===========================================================================
// scene_profiles.c - Perfiles de las tortugas (SCENE_PROFILES, modo atracto)
// ===========================================================================
// Pantalla que entra SOLA cuando el jugador deja quieta la seleccion de
// cantidad de jugadores durante PLAYER_SELECT_IDLE_SECS segundos (ver el
// contador de inactividad en showPlayerSelect, scenes.c). Muestra el perfil de
// UNA tortuga sorteada al azar, igual que la pantalla de "character profiles"
// del arcade:
//
//   1. El retrato ENTRA deslizandose desde el borde derecho, cruza el medio de
//      la pantalla y frena a la izquierda (movimiento con desaceleracion).
//   2. Apenas frena, aparecen sus datos a la derecha del retrato, linea por
//      linea (nombre, edad, altura, peso y arma).
//   3. Abajo de los dos, la descripcion aparece letra a letra, con la misma
//      tecnica que la pantalla de creditos (drawTextTypewriter).
//   4. Suena "02 - Character Profiles" de fondo.
//
// CUALQUIER boton (de cualquiera de los dos joysticks) corta y devuelve a la
// seleccion de jugadores. Si no se toca nada, al terminar el texto se queda un
// rato en pantalla y vuelve igual: la proxima vez que se agoten los 30 segundos
// sale otra tortuga (nunca la misma dos veces seguidas).
//
// Los TEXTOS son texto de verdad dibujado con la fuente arcade del proyecto
// (title_font), no imagenes: salieron de transcribir res/images/profiles/
// *_data.png y *_profile info.png. La fuente cubre ASCII 32..126, asi que no
// hay acentos (los textos son los del arcade, en ingles).
// ===========================================================================

#include "scenes.h"
#include "profiles.h"   // profile_leo, profile_mike, profile_don, profile_raph
#include "level1.h"     // title_font, title_font_pal (la fuente arcade)
#include "audio.h"      // music_profiles ("02 - Character Profiles.vgm")

// ---------------------------------------------------------------------------
// Volumen y tiempos
// ---------------------------------------------------------------------------
#define PROFILE_PLANE_W       64   // Plano de 64 tiles de ancho, como los menus
#define PROFILE_MUSIC_VOL     90
#define PROFILE_SLIDE_TICKS   44   // Lo que tarda el retrato en entrar
#define PROFILE_DATA_DELAY    10   // Ticks entre linea y linea de los datos
#define PROFILE_CHAR_DELAY     2   // Ticks entre letra y letra de la descripcion
#define PROFILE_HOLD_TICKS   240   // ~4 s con todo en pantalla antes de volver
#define PROFILE_FADE_TICKS    20

// ---------------------------------------------------------------------------
// Layout (pantalla de 320x224 = 40x28 tiles)
// ---------------------------------------------------------------------------
#define PROFILE_W             64   // El retrato es de 64x128 px
#define PROFILE_H            128
#define PROFILE_REST_X        40   // X final: cruza el medio y frena a la izquierda
#define PROFILE_Y             16   // Fila 2
#define PROFILE_START_X      320   // Entra pegado al borde derecho, fuera de cuadro

#define DATA_COL              14   // Los datos arrancan a la derecha del retrato
#define DATA_ROW               4
#define DATA_ROW_STEP          2
#define DATA_LINES             5

#define INFO_COL               2   // La descripcion va abajo de los dos
#define INFO_ROW              20
#define INFO_ROW_STEP          2
#define INFO_LINES             4

// Cualquier boton de cualquiera de los dos joysticks corta la escena.
#define PROFILE_ANY_BUTTON (BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT | \
                            BUTTON_A  | BUTTON_B    | BUTTON_C    | BUTTON_START | \
                            BUTTON_X  | BUTTON_Y    | BUTTON_Z    | BUTTON_MODE)

// ---------------------------------------------------------------------------
// Contenido de los 4 perfiles
// ---------------------------------------------------------------------------
// Transcripto de res/images/profiles/*_data.png y *_profile info.png. El orden
// es el mismo que usa el resto del juego: 0=Leo 1=Mike 2=Don 3=Raph.
// ---------------------------------------------------------------------------
typedef struct {
    const SpriteDefinition *portrait;
    const char             *data[DATA_LINES];   // nombre, edad, altura, peso, arma
    const char             *info[INFO_LINES];   // descripcion
} TurtleProfile;

static const TurtleProfile turtleProfiles[4] = {
    {   &profile_leo,
        { "LEONARDO", "16", "5'01''", "155 LBS", "KATANA BLADE" },
        { " LEADER OF THE BOYS.",
          " IF GETS SERIOUS, HIS SWORDS START",
          "SLICING EVERYTHING IN SIGHT .....",
          "INCLUDING SALAMI PIZZA!" } },

    {   &profile_mike,
        { "MICHAELANGELO", "15", "5'00''", "150 LBS", "NUNCHAKUS" },
        { " PARTY DUDE AND PIZZA CONNOISSEUR",
          "EXTRAORDINAIRE.",
          " TAKES OCCASIONAL BREAK FROM",
          "PARTYING TO SMASH HEADS...AND FOOTS!" } },

    {   &profile_don,
        { "DONATELLO", "15", "4'09''", "145 LBS", "BO STAFF" },
        { " HIPPEST MACHINE FREAK THIS SIDE",
          "OF SHELLVILLE.",
          " AVOIDS SUSHI LIKE A BAD CASE",
          "OF RUST!" } },

    {   &profile_raph,
        { "RAPHAEL", "15", "5'01''", "147 LBS", "PAIR OF SAI" },
        { " WILD BOY OF THE BUNCH.",
          " RAW ENERGY CAN FINISH OFF A FOOT..",
          "OR A PIZZA BEFORE YOU CAN SAY",
          "'TURTLES'." } },
};

// Ultima tortuga mostrada, para no repetirla dos veces seguidas.
static u8 lastProfileShown = 0xFF;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool profileAnyButton(void) {
    return (bool)((JOY_readJoypad(JOY_1) | JOY_readJoypad(JOY_2)) & PROFILE_ANY_BUTTON);
}

// Un tick de la escena. TRUE = el jugador toco algo y hay que volver.
static bool profileTick(void) {
    SPR_update();
    SYS_doVBlankProcess();
    return profileAnyButton();
}

static bool profileWait(u16 ticks) {
    while (ticks--) if (profileTick()) return TRUE;
    return FALSE;
}

// Dibuja el texto letra a letra. TRUE = cortado por el jugador.
// (Misma idea que drawTextTypewriter de scenes.c, pero cortando con CUALQUIER
// boton en vez de solo START, que es la regla de esta escena.)
static bool profileTypewriter(const char *text, u16 x, u16 y) {
    char buf[2];
    buf[1] = 0;
    for (u16 i = 0; text[i] != 0; i++) {
        buf[0] = text[i];
        if (buf[0] != ' ') VDP_drawText(buf, x + i, y);   // el espacio solo cuenta tiempo
        if (profileWait(PROFILE_CHAR_DELAY)) return TRUE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Escena
// ---------------------------------------------------------------------------
SceneId showProfiles(void) {
    clearScene();

    VDP_setScreenWidth320();
    VDP_setPlaneSize(PROFILE_PLANE_W, 32, TRUE);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setBackgroundColor(0);

    SPR_end();
    SPR_initEx(420);   // Alcanza de sobra: el retrato son 128 tiles

    // --- Sorteo: cualquiera de las 4, pero nunca la misma dos veces seguidas ---
    u8 pick = (u8)(random() & 3);
    if (pick == lastProfileShown) pick = (u8)((pick + 1 + (random() % 3)) & 3);
    lastProfileShown = pick;
    const TurtleProfile *tp = &turtleProfiles[pick];

    // --- Fuente arcade en PAL2 (misma receta que la seleccion de jugadores:
    //     PAL_setPalette escribe UNA linea de 16, PAL_setColors escribiria las
    //     64 entradas del PNG y pisaria las otras paletas) ---
    VDP_loadFont(&title_font, DMA);
    PAL_setPalette(PAL2, title_font_pal.data, DMA);
    VDP_setTextPalette(PAL2);

    // --- Retrato: su paleta va a PAL0 (en pantalla hay uno solo por vez) ---
    PAL_setPalette(PAL0, tp->portrait->palette->data, DMA);
    Sprite *portrait = SPR_addSprite(tp->portrait, PROFILE_START_X, PROFILE_Y,
                                     TILE_ATTR(PAL0, FALSE, FALSE, FALSE));

    playMusicVol(music_profiles, PROFILE_MUSIC_VOL);

    // Por las dudas: esperar a que no quede ningun boton apretado, asi el mismo
    // pulso que trajo la escena no la corta al instante.
    while (profileAnyButton()) SYS_doVBlankProcess();

    bool quit = FALSE;

    // --- 1. El retrato entra desde la derecha y frena (desaceleracion) ---
    // Interpolacion con ease-out cuadratica: recorre casi todo el camino al
    // principio y llega frenando, en vez de clavarse de golpe.
    for (u16 t = 1; t <= PROFILE_SLIDE_TICKS && !quit; t++) {
        s32 p = ((s32)t * 256) / PROFILE_SLIDE_TICKS;              // 0..256
        s32 e = 65536 - (256 - p) * (256 - p);                     // ease-out 0..65536
        s16 x = (s16)(PROFILE_START_X -
                      (((s32)(PROFILE_START_X - PROFILE_REST_X) * e) / 65536));
        SPR_setPosition(portrait, x, PROFILE_Y);
        quit = profileTick();
    }
    SPR_setPosition(portrait, PROFILE_REST_X, PROFILE_Y);

    // --- 2. Los datos aparecen a la derecha del retrato, linea por linea ---
    for (u16 i = 0; i < DATA_LINES && !quit; i++) {
        VDP_drawText(tp->data[i], DATA_COL, DATA_ROW + i * DATA_ROW_STEP);
        quit = profileWait(PROFILE_DATA_DELAY);
    }

    // --- 3. La descripcion, letra a letra, abajo de los dos ---
    for (u16 i = 0; i < INFO_LINES && !quit; i++)
        quit = profileTypewriter(tp->info[i], INFO_COL, INFO_ROW + i * INFO_ROW_STEP);

    // --- 4. Se queda un rato con todo en pantalla ---
    if (!quit) quit = profileWait(PROFILE_HOLD_TICKS);

    // --- Salida ---
    // Si se corto con un boton, el retrato y los textos quedan como estaban:
    // el fundido a negro se encarga. Se espera a que suelten para que el mismo
    // pulso no confirme tambien una opcion del menu al que volvemos.
    PAL_fadeOutAll(PROFILE_FADE_TICKS, FALSE);
    while (PAL_isDoingFade()) SYS_doVBlankProcess();
    while (profileAnyButton()) SYS_doVBlankProcess();

    SPR_releaseSprite(portrait);
    SPR_update();
    SYS_doVBlankProcess();

    VDP_loadFont(&font_default, DMA);   // Devolver la fuente por defecto
    SPR_end();
    SPR_initEx(752);

    clearScene();                        // Corta la musica y limpia planos
    return SCENE_PLAYER_SELECT;
}
