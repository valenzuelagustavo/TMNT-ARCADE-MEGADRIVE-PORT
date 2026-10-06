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
// TECHO DE TILES DEL FONDO: 529. NO es holgado -- leer antes de cambiar el PNG.
// showScene11 reparte la VRAM de usuario de forma RELATIVA al tamano del
// tileset del fondo, asi que cada tile que crece el fondo empuja todo lo demas:
//
//   TILE_USER_INDEX 16 + fondo 524 + fuego 64 + barras del HUD 8
//                      + sparks 16 + spark ascensor 15 + sparks_2 40  = 683
//   area de sprites (TILE_FONT_INDEX 1440 - SPR_initEx 752)           = 688
//                                                             margen  = +5
//
// Pasado ese techo, el fuego -- que reescribe sus 64 tiles cada 8 frames por
// DMA -- empieza a pisar el area de sprites y la pantalla se corrompe SIN
// ningun error: no hay assert que avise. Si hace falta un fondo mas grande hay
// que bajar el SPR_initEx(752) de showScene11, y eso se paga en cuantos
// enemigos entran a la vez.
//
// 13/09: bg01_completa.png (487 tiles) -> bg01_final.png (524).
// 19/09: bg01_final.png (524) -> Arcade-...-Stage-1_15-CORES.png (399). Rip con
//        paleta optimizada a 15 colores: ahorra ~125 tiles de VRAM, que pasan
//        directo al motor de sprites (SPR_initEx se calcula solo).
//        OJO con esta imagen: el color 0 de su paleta NO es negro, es un rojo
//        oscuro (94,0,10) que usan los contraescalones y el hueco de la puerta
//        del fondo. En Megadrive el indice 0 de un tile es TRANSPARENTE, asi
//        que esos pixeles no salen del plano: salen del color de fondo (reg 7).
//        Funciona porque showScene11 hace VDP_setBackgroundColor(0) y el fondo
//        vive en PAL0 -> el backdrop ES PAL0[0] = ese mismo rojo. Si alguna vez
//        se mueve el fondo a otra linea de paleta, o se cambia el backdrop,
//        esos pixeles se van a ver del color equivocado.
//        door_lvl_1.png y ascensor_door.png NO tienen paleta propia (comparten
//        PAL0), asi que se reindexaron al vuelo contra estos 15 colores. Es un
//        remapeo por cercania: conviene re-exportarlos desde Aseprite con la
//        paleta nueva cuando haya tiempo.
// =============================================================================

// --- Fondo principal (nivel completo) ---
IMAGE bg_level1 "images/lvl_1_scene/Arcade---Teenage-Mutant-Ninja-Turtles---Backgrounds---Stage-1_15-CORES.png" NONE

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
// (24/09) El strip lo genera ahora tools/gen_fire_strip.py a partir del sheet
// nuevo ("CHAMAS 7 CORES.png", 8 frames de banda ancha): elige la
// ventana de 64 px que menos costura deja al repetirse y la pega abajo de la
// celda. Los 7 colores del fuego son un subconjunto de la paleta de enemigos.
TILESET fire_tiles "sprites/fire_strip_new.png" NONE NONE

// --- HUD: marcos de vidas / puntos / barra de vida (72x32 cada uno) ---
// Spritesheet de 4 animaciones de 1 frame (celda 72x32), UNA por tortuga en
// ORDEN DE PERSONAJE: fila 0=Leo(azul) 1=Mike(rojo) 2=Don(purpura)
// 3=Raph(dorado). time 0 -> sin auto-animacion: scenes.c elige la fila con
// SPR_setAnim(sprite, personajeSeleccionado). Se dibujan como SPRITES de alto
// nivel en la franja superior de 32px (siempre por encima de los planos, sin
// gastar VRAM de tiles de fondo). Comparten la paleta de las tortugas (PAL1):
// NO llevan PALETTE propia.
SPRITE hud_1p "images/hud/hud_1p.png" 9 4 NONE 0
SPRITE hud_2p "images/hud/hud_2p.png" 9 4 NONE 0
// (16/09) Marcos 3UP/4UP para el modo de 4 jugadores. Generados a partir de
// hud_2p.png repintando SOLO la caja del digito (x 8..15, y 0..13 de cada
// fila) con el fondo local del marco (filas 9-10 = borde claro, fila 11 =
// sombra) y dibujando el glifo nuevo con el color de la tortuga + sombra.
// Misma paleta y mismas 4 filas de color -> NO llevan PALETTE propia.
SPRITE hud_3p "images/hud/hud_3p.png" 9 4 NONE 0
SPRITE hud_4p "images/hud/hud_4p.png" 9 4 NONE 0
// (06/10) HUD de 3-4 jugadores: el contorno compartido y los carteles
// "1UP".."4UP" (tools/gen_hud4_shared.py). El contorno va optimizado por
// TILES (28 en vez de 35): los cuatro marcos usan esos mismos tiles.
SPRITE hud4_outline "images/hud/hud4_outline.png" 9 4 NONE 0 NONE TILE MAX
SPRITE hud4_label   "images/hud/hud4_label.png"   3 2 NONE 0

// --- Retrato de la tortuga elegida (32x32, 4 filas en ORDEN DE PERSONAJE) ---
// frames_hud.png (32x128): fila 0=Leo 1=Mike 2=Don 3=Raph. time 0 -> sin
// auto-animacion: scenes.c elige la fila con SPR_setAnim(sprite, personaje).
// Vive en el espacio del borde que queda libre al correr los marcos del HUD
// hacia adentro. Comparte la paleta de las tortugas (PAL1): el PNG es 4bpp
// indexado sobre esa misma paleta -> NO lleva PALETTE propia.
SPRITE turtle_portrait "images/hud/frames_hud.png" 4 4 NONE 0

// --- Barra de vida (11 frames de 32x16 apilados en vertical) ---
// (17/09) Paso de 32x8 a 32x16 -- el DOBLE de alto -- para parecerse a la del
// arcade, que ocupa casi todo el interior del marco. La genera
// tools/gen_hud_bar.py, no se dibuja a mano.
// hp_bar.png (32x176): frame[0] = 10 barras (vida llena), frame[10] = 0.
// Cada frame son 4x2 = 8 tiles; se anima por STREAMING igual que el fuego:
// UN frame (8 tiles) vive en VRAM por jugador y, al recibir un golpe, scenes.c
// lo pisa con el frame siguiente via DMA. NONE NONE es CRITICO: sin comprimir
// para indexar los tiles de cada frame directo desde ROM (hp_bar.tiles) y sin
// deduplicar para que los 8 tiles de cada frame queden CONTIGUOS y en orden
// (frame N -> tiles [N*8 .. N*8+7]). Comparte la paleta de las tortugas
// (PAL1): NO lleva PALETTE propia (los indices del PNG coinciden con esa paleta).
TILESET hp_bar "sprites/hp_bar.png" NONE NONE

// --- Vidas: un digito grande en verde, como el arcade (10 de 8x16) ---
// lives_digits.png (8x160), generado por tools/gen_hud_bar.py estirando el
// glifo de hud_font. Mismo streaming que la barra: 2 tiles de VRAM por
// jugador y un DMA cuando cambia el numero. digito N -> tiles [N*2 .. N*2+1].
TILESET lives_digits "images/hud/lives_digits.png" NONE NONE

// (26/09) Los digitos de vidas ahora van en el COLOR DE CADA TORTUGA (arte del
// proyecto del companero, tools/gen_lives_digits_turtles.py): 4 x 10 digitos
// de 8x16 apilados en orden de charIndex (Leo, Mike, Don, Raph), en PAL1.
// Digito d de la tortuga t -> tiles [(t*10 + d) * 2 .. +1]. lives_digits (el
// verde) queda para el caso de un charIndex fuera de rango.
TILESET lives_digits_turtles "images/hud/lives_digits_turtles.png" NONE NONE

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
TILESET title_font     "images/font/font_tmnt_arcade.png" NONE NONE
PALETTE title_font_pal "images/font/font_tmnt_arcade.png"

// --- Fuente arcade del HUD (vidas/puntaje), MISMA regla que title_font ---
// 95 tiles de 8x8 en orden ASCII (32..126), compatible con VDP_loadFont.
// NONE NONE es CRITICO por el mismo motivo (sin dedup -> mapeo char->tile 1:1).
// NO lleva PALETTE propia: el PNG esta indexado sobre la MISMA paleta de las
// tortugas (los indices 11/13 del PNG = lavanda/gris claro de PAL1), asi que
// el HUD se dibuja con VDP_setTextPalette(PAL1) sin gastar una linea de
// paleta (PAL0-3 ya estan ocupadas en ambos niveles). Mismo truco que
// attack_bubble/hp_bar/hud.
TILESET hud_font "images/font/font_tmnt_arcade_2.png" NONE NONE

// --- Globo de dialogo "Attack!!" (intro del nivel) --
// 64x32px = 8x4 tiles, UN solo frame (time 0 -> sin animacion automatica).
// NO lleva PALETTE propia: el PNG esta indexado sobre la MISMA paleta de las
// tortugas (indices 0/7/8/9/11 coinciden con negro/dorado/verde/cyan/lavanda
// de esa paleta), asi que se dibuja con TILE_ATTR(PAL1,...) sin gastar una
// linea de paleta. Las 4 del nivel ya estan ocupadas: PAL0 fondo, PAL1
// tortugas, PAL2 enemigos+fuego, PAL3 flash/HUD. Mismo truco que hp_bar/hud.
SPRITE attack_bubble "sprites/attack_bubble.png" 8 4 NONE 0

// --- Globo "Duuuh, who put the light out" (caida por la alcantarilla, 2-1) ---
// 96x32px = 12x4 tiles, UN solo frame. Lo usa el 2-1 cuando una tortuga se cae
// por una boca de tormenta destapada; vive en level1.res y no en level2.res
// porque el 2-1 ya incluye level1.h (de ahi saca hud_font) y level2.res son los
// recursos de la escena 1-2 (el departamento de April), que es otra cosa.
// Igual que attack_bubble, NO lleva PALETTE propia: verificado indice por
// indice, los 0/7/8/9/11 del PNG son los mismos colores de Megadrive (9 bits)
// que la paleta de las tortugas, asi que se dibuja con TILE_ATTR(PAL1,...).
SPRITE light_out_bubble "sprites/light_out_baloon.png" 12 4 NONE 0

// --- "HURRY UP!" (aviso de desplazamiento de camara) ---
// 160x32px = spritesheet de 5 frames de 32x32 (4x4 tiles). time 6 -> anima
// ciclicamente los 5 frames. Comparte la paleta de las tortugas (PAL1).
SPRITE hurry_sheet "sprites/hurry_sheet.png" 4 4 NONE 6

// --- Bola de hierro (obstaculo que cae rebotando por las escaleras) ---
// 64x32px = spritesheet de 2 frames de 32x32 (4x4 tiles) -> giro de la esfera.
// time 6 -> alterna los 2 frames cada 6 ticks (~10fps). NO lleva PALETTE propia:
// el PNG esta indexado sobre la MISMA paleta de las tortugas (PAL1), igual que
// attack_bubble/hp_bar, asi que se dibuja con TILE_ATTR(PAL1,...) sin gastar
// una linea de paleta. Solo 2 frames x 16 tiles = 32 tiles en ROM.
SPRITE iron_ball "sprites/iron_ball.png" 4 4 NONE 6

// --- Sparks: efecto de fuego detras de las puertas rompibles ---
// 32x32px = 4x4 tiles, UN solo frame. Se ubica detras de cada puerta
// (door_lvl_1) y usa la paleta de los foot soldiers (PAL2).
//
// La animacion YA NO es por rotacion de paleta (indices 5-8 de PAL2): esos
// mismos indices los usa fire_tiles (el fuego de primer plano, SIEMPRE
// visible) para su propio dibujo, asi que rotar PAL2 tambien le temblaba el
// color al fuego de fondo -- se notaba en pantalla, y no hay una 5ta linea
// de paleta libre en el nivel para aislarlas (las 4 ya estan repartidas).
// Fix: sparksStreamInit/Update en scenes.c streamean tiles REALES (mismo
// truco que fire_tiles/smoke_tiles), tomados de sparks_strip.png (4 frames
// apilados, generados por tools/gen_sparks_frames.py rotando los PIXELES
// en vez de la paleta -- resultado visual identico, cero escrituras a CRAM).
// Este recurso (sparks, un solo frame) se sigue usando SOLO como molde de
// tamano para SPR_addSpriteEx (2x2... 4x4 tiles); sus propios tiles nunca
// se suben a VRAM (auto-upload apagado).
// (24/09) La tira de frames ES el recurso SPRITE (1 animacion x N frames, sin
// compresion, time 0 = el codigo maneja el frame). Antes era un SPRITE de un
// frame usado como "molde" + un TILESET aparte con los frames reordenados a
// mano por columna: eso solo coincide con lo que arma rescomp cuando el frame
// entra en UN sprite de hardware (4x4 tiles, las puertas). spark_ascensor
// (5x3) y sparks_2 (8x5) rescomp las parte en varios sprites de hardware y
// descarta los tiles vacios (13 y 35 tiles, no 15 y 40) -> se veian rotas.
// Ahora el codigo streamea animations[0]->frames[f]->tileset, en el orden y
// con la cantidad exacta que decidio rescomp. Tiras: tools/gen_sparks_anim.py.
// (26/09) Arte NUEVO (chamas-sheet.png, tools/gen_chamas.py): el
// hueco ENTERO de la puerta rota -- marco, interior negro, llamas y piso --,
// 7 frames de 40x80 (el arte son 33x79, exacto el hueco del fondo). El negro
// del interior es opaco, asi que los 50 tiles estan llenos en todos los
// frames y el streaming al bloque compartido sigue valiendo.
SPRITE sparks "sprites/door_fire_gen.png" 5 10 NONE 0

// --- Puerta rompible (spawn point del nivel) ---
// 40x80px = 5x10 tiles, UN solo frame (time 0). Se dibuja sobre cada hueco de
// puerta abierta del nivel. NO lleva PALETTE propia: el PNG comparte la paleta
// del FONDO (PAL0) -- los indices coinciden con los slots del fondo -- asi que se
// dibuja con TILE_ATTR(PAL0,...). Al acercarse el jugador se remueve y el foot
// soldier la reemplaza rompiendola (ENEMY_ANIM_BREAK_DOOR).
SPRITE door_lvl_1 "sprites/door_lvl_1.png" 5 10 NONE 0

// --- Spark ascensor: fuego en los huecos de ascensores ---
// 40x24px (5x3 tiles), UN solo frame. Misma paleta que sparks (PAL2).
// Se ubica detras de cada ascensor_door y queda fijo en el mundo. Streameado
// igual que sparks (ver comentario arriba) -- molde de tamano nada mas.
// (24/09) 3 frames (el arte nuevo de spark_ascensor_strip.png), no 4.
SPRITE spark_ascensor "sprites/spark_ascensor_anim.png" 5 3 NONE 0

// --- Sparks 2: efecto decorativo fijo en X=330 ---
// 64x36px... en rigor 64x40 (8x5 tiles), UN solo frame. Misma paleta que
// sparks (PAL2). Streameado igual que sparks (ver comentario arriba).
// (26/09) REEMPLAZADO por floor_fire (abajo): ya no se compila.

// --- Fuego del piso (26/09): decorativo, en DOS puntos del suelo ---
// Arte nuevo de chamas-sheet.png (tools/gen_chamas.py): 9 frames de 56x32
// (7x4 tiles), PAL2. La llama cambia de forma en cada frame, asi que NO se
// streamea como los sparks: es un sprite comun con auto-animacion, y cada
// instancia se crea solo cuando esta cerca de camara (ver showScene11).
SPRITE floor_fire "sprites/floor_fire_gen.png" 7 4 NONE 5

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
IMAGE bg_b_final "images/lvl_1_scene/BG_B_final_lvl1.png" BEST
IMAGE bg_a_final "images/lvl_1_scene/BG_A_final_lvl1.png" BEST
