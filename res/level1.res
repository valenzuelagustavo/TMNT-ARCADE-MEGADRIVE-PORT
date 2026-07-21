// =============================================================================
// level1.res  Graficos del Nivel 1: "The streets of New York"
// =============================================================================
// bg_level1: 1376x224px (172x28 tiles)  el nivel COMPLETO.
// Es mas ancho que cualquier plano de la MegaDrive (max 128 tiles = 1024px),
// asi que NO se dibuja entero: scenes.c hace STREAMING de columnas sobre un
// plano circular de 64 tiles (ver bgInit/bgUpdate).
//
// Compresion NONE: es obligatorio para poder leer el tilemap directamente
// desde ROM (bg_level1.tilemap->tilemap[]) e ir copiando columna por columna.
// Con BEST/APLIB el mapa queda comprimido y no se puede indexar al vuelo.
// El tileset deduplicado es de ~495 tiles unicos -> entra holgado en VRAM.
// =============================================================================

// --- Fondo principal (nivel completo) ---
IMAGE bg_level1 "/images/lvl_1_scene/bg01_completa.png" NONE

// --- Fuego de primer plano (tira VERTICAL: 8 frames de 64x64 apilados) ---
// fire_strip.png (64x512) se genera a partir de fire_512x224.png tomando la
// banda inferior (y=160..224) de cada uno de los 8 frames de 64px.
// Se anima por STREAMING de tiles: solo UN frame (64 tiles) vive en VRAM y
// cada 8 frames de juego scenes.c lo pisa con el siguiente via DMA (2KB).
//
// NONE NONE es CRITICO (mismo motivo que la fuente de abajo): sin comprimir
// para poder indexar los tiles de cada frame directo desde ROM, y sin
// deduplicar para que los 64 tiles de cada frame queden CONTIGUOS y en orden.
// El fuego NO lleva PALETTE propia: comparte la paleta del foot_soldier
// (PAL2), los PNGs estan cuantizados sobre la misma paleta indexada.
TILESET fire_tiles "/sprites/fire_strip.png" NONE NONE

// --- HUD: marcos de vidas / puntos / barra de vida (72x32 cada uno) ---
// Van en la franja superior de 32px que el fondo deja libre (sus 4 primeras
// filas de tiles usan un indice fuera de la linea -> transparente en juego).
// Comparten la paleta de las tortugas (PAL1): NO llevan PALETTE propia.
IMAGE hud_1p "/images/hud/hud_1p.png" BEST
IMAGE hud_2p "/images/hud/hud_2p.png" BEST

// --- Barra de vida (11 frames de 32x8 apilados en vertical) ---
// hp_bar.png (32x88): frame[0] = 10 barras (vida llena), frame[10] = 0 barras.
// Cada frame son 4x1 = 4 tiles; se anima por STREAMING igual que el fuego:
// UN frame (4 tiles) vive en VRAM por jugador y, al recibir un golpe, scenes.c
// lo pisa con el frame siguiente via DMA. NONE NONE es CRITICO: sin comprimir
// para indexar los tiles de cada frame directo desde ROM (hp_bar.tiles) y sin
// deduplicar para que los 4 tiles de cada frame queden CONTIGUOS y en orden
// (frame N -> tiles [N*4 .. N*4+3]). Comparte la paleta de las tortugas
// (PAL1): NO lleva PALETTE propia (los indices del PNG coinciden con esa paleta).
TILESET hp_bar "/sprites/hp_bar.png" NONE NONE

// --- Fuente arcade para el titulo del nivel (solo ASCII en este bloque) ---
// 95 tiles de 8x8 en orden ASCII (32..126) -> compatible con VDP_loadFont.
// TILESET (tiles) + PALETTE (blanco/azul) exportados del mismo PNG.
//
// OJO: el segundo NONE es CRITICO. Es el parametro "opt" de rescomp: por
// defecto (ALL) deduplica tiles repetidos, y una fuente tiene muchos (los
// vacios y las minusculas que duplican A-Z). Si se deduplica, los indices
// se corren y VDP_drawText dibuja letras equivocadas (el mapeo char->tile
// es 1:1 con el orden ASCII).
// Sintaxis: TILESET name file [compression [opt]] -> NONE NONE = sin
// comprimir y sin optimizar: cada tile conserva su posicion ASCII.
TILESET title_font     "/images/font/font_tmnt_arcade.png" NONE NONE
PALETTE title_font_pal "/images/font/font_tmnt_arcade.png"

// --- Globo de dialogo "Attack!!" (intro del nivel) ---
// 64x32px = 8x4 tiles, UN solo frame (time 0 -> sin animacion automatica).
// NO lleva PALETTE propia: el PNG esta indexado sobre la MISMA paleta de las
// tortugas (indices 0/7/8/9/11 coinciden con negro/dorado/verde/cyan/lavanda
// de esa paleta), asi que se dibuja con TILE_ATTR(PAL1,...) sin gastar una
// linea de paleta. Las 4 del nivel ya estan ocupadas: PAL0 fondo, PAL1
// tortugas, PAL2 enemigos+fuego, PAL3 flash/HUD. Mismo truco que hp_bar/hud.
SPRITE attack_bubble "sprites/attack_bubble.png" 8 4 NONE 0
