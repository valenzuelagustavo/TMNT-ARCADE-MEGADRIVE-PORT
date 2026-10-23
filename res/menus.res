// =============================================================================
// menus.res — Gráficos de los menús y pantallas de selección
// =============================================================================
// Contiene imágenes de fondo y sprites de UI para:
//   - Pantalla de selección de número de jugadores (showPlayerSelect)
//   - Pantalla de selección de personaje (showCharSelect)
//   - Cualquier otro menú futuro (Game Over, Rankings, etc.)
// =============================================================================

// --- Imágenes de fondo (tilemaps) ---
// (09/10) Fondo de la seleccion de players y de OPCIONES: la pantalla 2 de
// "intro_tmnt/Arcade - ... -TELA START _GENESIS_FINAL.png" (14/10, con
// "GENESIS" debajo del logo; paleta nueva) SIN el menu ni el
// cursor (van aparte, abajo). La arma tools/gen_title_screens.py. Reemplaza a title_scene/logo_tmnt.png.
// (22/10) NONE, no BEST (como char_select_bg): comprimido, VDP_loadTileSet
// pide 373 tiles (~12 KB) al heap para descomprimir. Despues de una partida
// entera el heap queda fragmentado, no hay un bloque tan grande, la carga
// fallaba sin avisar y el menu salia SIN EL LOGO (el mapa se dibujaba con
// tiles vacios). Sin comprimir se copia directo de la ROM (~12 KB de ROM).
IMAGE title_menu_bg "images/intro_tmnt/genesis/title_menu_bg.png" NONE ALL
// El menu, tal cual el arte (letras del arcade, renglones cada 17 px): el
// de 3 opciones y el del secreto de 4 jugadores (suma "4 TURTLES").
IMAGE title_menu3 "images/intro_tmnt/genesis/title_menu3.png" NONE ALL
IMAGE title_menu4 "images/intro_tmnt/genesis/title_menu4.png" NONE ALL

// --- Sprites de interfaz ---
// (09/10) La cabecita de cursor del arte nuevo (24x16), misma paleta que
// title_menu_bg (PAL0). Reemplaza a title_scene/selector_turtle.png.
SPRITE title_cursor "images/intro_tmnt/genesis/title_cursor.png" 3 2 NONE 0
// (16/10) SACADO (sin uso):
// SPRITE sega_logo_sprite "images/title_scene/sega_logo.png" 17 10 FAST 8

// --- Seleccion de personaje (08/10) ---
// Cortados de "hud/select player_15 CORES.png" por tools/gen_char_select.py.
// Comparten la paleta de 15 colores del PNG fuente (todo en PAL0). Reemplazan
// a characters_greyscale / character_selector / faces_hud (los PNG viejos
// siguen en images/title_scene/, sin declarar).
// (10/10) NONE, no BEST: comprimido, VDP_loadTileSet tiene que pedir un buffer
// de 715 tiles (~23 KB) al heap para descomprimir. Viniendo de la pantalla de
// players el heap no tenia un bloque libre tan grande, VDP_drawImageEx
// devolvia FALSE sin dibujar nada y la seleccion quedaba en negro (solo se
// veian los sprites). Sin comprimir se copia directo de la ROM.
IMAGE  char_select_bg    "images/hud/char_select_bg.png" NONE ALL
// Globo "nUP": fila = jugador (1UP..4UP), frame = tortuga (Leo, Mike, Don, Raph).
SPRITE char_select_badge "images/hud/char_select_badge.png" 4 4 NONE 0
// Cabeza de cada marco: fila = tortuga, frame 0 ojos abiertos / 1 cerrados.
SPRITE char_select_head  "images/hud/char_select_head.png" 3 2 NONE 0

// --- Opciones (10/10) ---
// Cortados de "option_menu/TELA OPTION_15 CORES.png" por tools/gen_option_menu.py.
// Paleta del PNG fuente en PAL0. option_font: ASCII 32..126, NONE NONE como
// title_font (sin dedup, el mapeo char->tile tiene que ser 1:1).
IMAGE   option_bg    "images/option_menu/option_bg.png" NONE ALL   // NONE: ver char_select_bg
TILESET option_font  "images/option_menu/option_font.png" NONE NONE
SPRITE  option_arrow "images/option_menu/option_arrow.png" 1 1 NONE 0

// --- Creditos: reconocimiento al adaptador musical ---
// sansenpai35_logo.png es el nombre del artista (200x32, 1 frame) y
// skeleton_music.png es el esqueleto animado (3 frames de 80x80, auto-anim
// cada 6 ticks). OJO: los PNG deben ser indexados (colorType 3) y con alto
// multiplo de 8; el logo original de 200x25 se paddeo a 200x32.
SPRITE sansenpai_logo "sprites/logos/sansenpai35_logo.png" 25 4 FAST 0
SPRITE skeleton_music "sprites/logos/skeleton_music.png" 10 10 FAST 6

// (20/10) Retratos del selector sin el fondo plano (tools/gen_highscore.py):
// carga de iniciales y "BEST 10 PLAYERS" (hiscore.c). Un frame por tortuga.
SPRITE hs_portraits "/images/hiscore/hs_portraits.png" 8 16 NONE 0
