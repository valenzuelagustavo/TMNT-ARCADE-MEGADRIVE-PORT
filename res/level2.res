// =============================================================================
// level2.res  Graficos del Nivel 2: pasillo en llamas (2da parte)
// =============================================================================
// bg_test: 440x192px (55x24 tiles), la sala cerrada del pasillo en llamas.
// Compresion NONE (igual que bg_level1): obligatorio para leer el tilemap
// directamente desde ROM (bg_test.tilemap->tilemap[]) y dibujar columna por
// columna. El tileset deduplicado es de ~467 tiles unicos -> entra holgado
// en VRAM.
// =============================================================================

// --- Fondo del nivel 2 (sala cerrada de 440px) ---
IMAGE bg_test "/images/lvl_1_scene/bg_test.png" NONE

// --- Humo del techo (tira VERTICAL: 8 frames de 64x64 apilados) ---
// smoke_lvl1.png (64x512) se genera igual que fire_strip.png. Se anima por
// STREAMING de tiles: solo UN frame (64 tiles) vive en VRAM y cada 8 frames
// de juego scenes.c lo pisa con el siguiente via DMA (2KB).
//
// NONE NONE es CRITICO (mismo motivo que fire_tiles): sin comprimir para
// poder indexar los tiles de cada frame directo desde ROM, y sin deduplicar
// para que los 64 tiles de cada frame queden CONTIGUOS y en orden.
// El humo NO lleva PALETTE propia: comparte la paleta de las tortugas (PAL1),
// el PNG esta cuantizado sobre esa misma paleta indexada.
TILESET smoke_tiles "/sprites/smoke_lvl1.png" NONE NONE

// =============================================================================
// Jefe Rocksteady (pasillo en llamas)
// =============================================================================
// Capsula del taladro como SPRITE (672x208 = grilla 7x2 de celdas 96x104 =
// 12x13 tiles). Indice [0] = 7 frames de emergencia (la capsula sube desde el
// piso con la puerta cerrada, mientras tiembla la pantalla); indice [1] = 1
// frame de puerta abierta, congelado por el resto del nivel (Rocksteady sale
// por esa puerta). Las 6 celdas sobrantes de la fila 1 estan 100% transparentes
// y rescomp las recorta.
// time = 0: la animacion se controla MANUAL desde scenes.c (SPR_setAnimAndFrame)
// sincronizada con el temblor de pantalla, y el frame final queda congelado.
// El PNG esta cuantizado con la paleta del fondo: se dibuja con TILE_ATTR(PAL0,
// FALSE, ...) (prioridad baja -> la banda de fuego, prioridad alta, tapa su base
// y la hace "salir del piso").
SPRITE taladro_capsula "sprites/taladro_capsula.png" 12 13 NONE 0

// April (rehén atada al fondo de la sala). Usa la paleta de las tortugas
// (PAL1): el PNG esta cuantizado sobre esa misma paleta indexada (4bpp).
// 64x64 = DOS frames de 32x64 lado a lado (2 cols de celdas 4x8 tiles).
// time = 12: animacion automatica (~5 fps, balanceo).
SPRITE april "sprites/april.png" 4 8 NONE 12

// Globo de dialogo del jefe "SAY YOUR PRAYERS!" (96x32 = grilla 12x4 de celdas
// de 8px). Un solo frame (time = 0). Usa la paleta de las tortugas (PAL1),
// igual que attack_bubble del nivel 1: el PNG esta cuantizado sobre esa misma
// paleta indexada (sin PALETTE propia -> no gasta linea de paleta). Aparece
// ~2 tiles sobre Rocksteady apenas emerge de la capsula y desaparece con el
// mismo ciclo del globo "Attack!!" (solido -> parpadeo -> se suelta).
SPRITE say_your_prayers "sprites/say_your_prayers.png" 12 4 NONE 0

// Rocksteady (jefe): 832x1144 = grilla 8x11 de celdas de 104x104 (13x13 tiles).
// 11 anims: [0]Idle [1]Caminar [2]Estampida [3]Patada [4]Recibe golpes/cae
// [5]Saca arma [6]Camina con arma [7]Camina apuntando [8]Patada con arma
// [9]Dispara [10]Recibe golpes con arma. FAST (LZ4W) es el recomendado para
// sprites streameados por frame (frame mas caro ~59 tiles unicos).
// time = 6 (frames por frame de animacion): el motor de sprites NO avanza la
// auto-animacion si el timer del frame es 0 (ver sprite_eng.c), asi que 0
// dejaria a Rocksteady congelado en el frame 0. 6 = ~10 fps, igual que robot_whip.
// Se dibuja en PAL3 (paleta del boss, que se carga al aparecer). OJO: el PNG
// NO usa el indice 1 de su paleta (se remapeo a 2): PAL3[1] queda blanco para
// el texto del HUD (VDP_setTextPalette(PAL3)) sin manchar el sprite.
SPRITE rocksteady_boss "sprites/rocksteady_boss.png" 13 13 FAST 6

// Bala del disparo de Rocksteady (fase 2): 16x16 = 2x2 tiles, un frame.
// Paleta indexada con la del boss (indice 11 = nucleo blanco, 4 = halo oro).
SPRITE boss_bullet "sprites/boss_bullet.png" 2 2 FAST 0

// Shredder (cutscene de victoria del nivel 2): 432x240 = grilla 6x3 de celdas
// 72x80 (9x10 tiles). Indices de animacion: [0] Idle (1 frame, apenas sale de la
// capsula) [1] Walk (6 frames) [2] Rapto (3 frames: 0-1 toma a April, que va
// INCLUIDA en el sprite - por eso se libera el sprite propio de April -, y el
// frame 2 es la pose de salto que queda CONGELADA mientras Shredder vuela en
// arco hacia la ventana del extremo derecho). Tiene PALETA PROPIA que se carga
// en PAL3 cuando muere Rocksteady (PAL3[1] se fuerza blanco para el HUD: el PNG
// no usa el indice 1, igual que rocksteady_boss). El arte mira a la derecha
// (flip con SPR_setHFlip al caminar hacia April). FAST para sprites streameados
// por frame; time = 6 (~10 fps, igual que el jefe).
SPRITE shredder_lvl1 "sprites/shredder_lvl1.png" 9 10 FAST 6
