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
