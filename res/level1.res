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
// Spritesheet de 4 animaciones de 1 frame (celda 72x32), UNA por tortuga en
// ORDEN DE PERSONAJE: fila 0=Leo(azul) 1=Mike(rojo) 2=Don(purpura)
// 3=Raph(dorado). time 0 -> sin auto-animacion: scenes.c elige la fila con
// SPR_setAnim(sprite, personajeSeleccionado). Se dibujan como SPRITES de alto
// nivel en la franja superior de 32px (siempre por encima de los planos, sin
// gastar VRAM de tiles de fondo). Comparten la paleta de las tortugas (PAL1):
// NO llevan PALETTE propia.
SPRITE hud_1p "/images/hud/hud_1p.png" 9 4 NONE 0
SPRITE hud_2p "/images/hud/hud_2p.png" 9 4 NONE 0

// --- Retrato de la tortuga elegida (32x32, 4 filas en ORDEN DE PERSONAJE) ---
// frames_hud.png (32x128): fila 0=Leo 1=Mike 2=Don 3=Raph. time 0 -> sin
// auto-animacion: scenes.c elige la fila con SPR_setAnim(sprite, personaje).
// Vive en el espacio del borde que queda libre al correr los marcos del HUD
// hacia adentro. Comparte la paleta de las tortugas (PAL1): el PNG es 4bpp
// indexado sobre esa misma paleta -> NO lleva PALETTE propia.
SPRITE turtle_portrait "/images/hud/frames_hud.png" 4 4 NONE 0

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

// --- Fuente arcade del HUD (vidas/puntaje), MISMA regla que title_font ---
// 95 tiles de 8x8 en orden ASCII (32..126), compatible con VDP_loadFont.
// NONE NONE es CRITICO por el mismo motivo (sin dedup -> mapeo char->tile 1:1).
// NO lleva PALETTE propia: el PNG esta indexado sobre la MISMA paleta de las
// tortugas (los indices 11/13 del PNG = lavanda/gris claro de PAL1), asi que
// el HUD se dibuja con VDP_setTextPalette(PAL1) sin gastar una linea de
// paleta (PAL0-3 ya estan ocupadas en ambos niveles). Mismo truco que
// attack_bubble/hp_bar/hud.
TILESET hud_font "/images/font/font_tmnt_arcade_2.png" NONE NONE

// --- Globo de dialogo "Attack!!" (intro del nivel) --
// 64x32px = 8x4 tiles, UN solo frame (time 0 -> sin animacion automatica).
// NO lleva PALETTE propia: el PNG esta indexado sobre la MISMA paleta de las
// tortugas (indices 0/7/8/9/11 coinciden con negro/dorado/verde/cyan/lavanda
// de esa paleta), asi que se dibuja con TILE_ATTR(PAL1,...) sin gastar una
// linea de paleta. Las 4 del nivel ya estan ocupadas: PAL0 fondo, PAL1
// tortugas, PAL2 enemigos+fuego, PAL3 flash/HUD. Mismo truco que hp_bar/hud.
SPRITE attack_bubble "sprites/attack_bubble.png" 8 4 NONE 0

// --- Bola de hierro (obstaculo que cae rebotando por las escaleras) ---
// 64x32px = spritesheet de 2 frames de 32x32 (4x4 tiles) -> giro de la esfera.
// time 6 -> alterna los 2 frames cada 6 ticks (~10fps). NO lleva PALETTE propia:
// el PNG esta indexado sobre la MISMA paleta de las tortugas (PAL1), igual que
// attack_bubble/hp_bar, asi que se dibuja con TILE_ATTR(PAL1,...) sin gastar
// una linea de paleta. Solo 2 frames x 16 tiles = 32 tiles en ROM.
SPRITE iron_ball "sprites/iron_ball.png" 4 4 NONE 6

// --- Sparks: efecto de fuego detras de las puertas rompibles ---
// 32x32px = 2x2 tiles, UN solo frame. Se ubica detras de cada puerta
// (door_lvl_1) y usa la paleta de los foot soldiers (PAL2). La animacion
// es puramente por rotacion de paleta (indices 5-8) en el game loop.
SPRITE sparks "sprites/sparks.png" 4 4 FAST 0

// --- Puerta rompible (spawn point del nivel) ---
// 40x80px = 5x10 tiles, UN solo frame (time 0). Se dibuja sobre cada hueco de
// puerta abierta del nivel. NO lleva PALETTE propia: el PNG comparte la paleta
// del FONDO (PAL0) -- los indices coinciden con los slots del fondo -- asi que se
// dibuja con TILE_ATTR(PAL0,...). Al acercarse el jugador se remueve y el foot
// soldier la reemplaza rompiendola (ENEMY_ANIM_BREAK_DOOR).
SPRITE door_lvl_1 "sprites/door_lvl_1.png" 5 10 NONE 0

// --- Spark ascensor: fuego en los huecos de ascensores ---
// 40x24px (5x3 tiles), UN solo frame. Misma paleta que sparks (PAL2).
// Se ubica detras de cada ascensor_door y queda fijo en el mundo.
SPRITE spark_ascensor "sprites/spark_ascensor.png" 5 3 FAST 0

// --- Sparks 2: efecto decorativo fijo en X=330 ---
// 64x36px (8x5 tiles), UN solo frame. Misma paleta que sparks (PAL2).
SPRITE sparks_2 "sprites/sparks_2.png" 8 5 FAST 0

// --- Puertas de ascensor (spawn animado) ---
// 192x80px = spritesheet de 4 frames de 48x80 (6x10 tiles) -> animacion de
// apertura. Se ubican DOS instancias, una en cada hueco de ascensor. NO lleva
// PALETTE propia: comparte la paleta del FONDO (PAL0), igual que door_lvl_1.
// time 8 -> los 4 frames en 32 ticks (calza con ELEV_DOOR_ANIM_TIME de scenes.c).
SPRITE ascensor_door "sprites/ascensor_door.png" 6 10 NONE 8

// --- Robot del latigo (mini-jefe del final) ---
// 2024x1040 = grilla de frames de 184x80 (23x10 tiles), 13 animaciones (0..12).
// Frame ANCHO porque el latigo se estira DENTRO del sprite (el cuerpo va a la
// izquierda). Comparte la paleta de los foot soldiers (PAL2), NO lleva PALETTE
// propia. FAST = compresion rapida (sprite grande, no se streamea).
// time 6 -> anim mas rapida que el 8 original (aparicion, giro, caminata,
// windup del latigo, laser, etc.). El throw/recogida y la electro NO dependen
// de esto (van a mano con ROBOT_THROW_TICKS / frame congelado).
SPRITE robot_whip "sprites/robot_whip.png" 23 10 FAST 6

// --- Laser del robot (sub-sprite) ---
// whip_waves 288x80: solo se usa su animacion de LASER (el latigo ya esta
// integrado en el sprite del robot). Frame 96x16 (12x2 tiles). Comparte PAL2.
SPRITE whip_waves "sprites/whip_waves.png" 12 2 NONE 8

// --- Cutscene final (Shredder rapta a April) -- imagen combinada en 2 planos ---
// Dos imagenes de 320x224, cada una con SU paleta de 16 colores: BG_B_final es
// el fondo (plano BG_B, PAL0) y BG_A_final va ENCIMA (plano BG_A, PAL1) con el
// indice 0 transparente, de modo que juntas forman una imagen de ~32 colores.
// BEST = maxima compresion (son de un solo uso). La cutscene libera la VRAM de
// sprites (SPR_end) mientras las muestra: entre las dos suman ~1000 tiles.
IMAGE bg_b_final "/images/lvl_1_scene/BG_B_final_lvl1.png" BEST
IMAGE bg_a_final "/images/lvl_1_scene/BG_A_final_lvl1.png" BEST
