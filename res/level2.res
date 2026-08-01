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
// Taladro que emerge por la pared (STRIP HORIZONTAL: 6 frames de 80x56 lado a
// lado). NONE NONE es critico: sin comprimir para indexar tiles desde ROM y sin
// deduplicar para que el orden de tiles sea el del PNG (fila por fila).
// Los tiles de cada frame NO son contiguos (el strip es horizontal): el tilemap
// se arma en codigo calculando r*60 + frame*10 + c (ver scenes.c taladroDraw).
TILESET taladro_emergin "/sprites/taladro_emergin.png" NONE NONE

// Puerta del taladro (STRIP VERTICAL: 2 frames de 80x144 apilados; frame 0 =
// cerrada, frame 1 = abierta). En el strip vertical los tiles de cada frame SI
// son contiguos (180 tiles/frame), igual que fire_strip/smoke_lvl1.
TILESET taladro_out "/sprites/taladro_out.png" NONE NONE

// April (rehén atada al fondo de la sala). Usa la paleta de las tortugas
// (PAL1): el PNG esta cuantizado sobre esa misma paleta indexada (4bpp).
// 64x64 = 8x8 tiles, un solo frame.
SPRITE april "sprites/april.png" 8 8 NONE 0

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
