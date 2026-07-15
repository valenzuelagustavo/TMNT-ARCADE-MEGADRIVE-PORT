// =============================================================================
// level1.res — Gráficos del Nivel 1: "The streets of New York"
// =============================================================================
// bg_level1: 1376x224px (172x28 tiles) — el nivel COMPLETO.
// Es más ancho que cualquier plano de la MegaDrive (máx 128 tiles = 1024px),
// así que NO se dibuja entero: scenes.c hace STREAMING de columnas sobre un
// plano circular de 64 tiles (ver bgInit/bgUpdate).
//
// Compresión NONE: es obligatorio para poder leer el tilemap directamente
// desde ROM (bg_level1.tilemap->tilemap[]) e ir copiando columna por columna.
// Con BEST/APLIB el mapa queda comprimido y no se puede indexar al vuelo.
// El tileset deduplicado es de ~495 tiles únicos → entra holgado en VRAM.
// =============================================================================

// --- Fondo principal (nivel completo) ---
IMAGE bg_level1 "/images/lvl_1_scene/bg01_completa.png" NONE

// --- Fuente arcade para el título del nivel ---
// 95 tiles de 8x8 en orden ASCII (32..126) → compatible con VDP_loadFont.
// TILESET (tiles) + PALETTE (blanco/azul) exportados del mismo PNG.
TILESET title_font     "/images/font/font_tmnt_arcade.png" NONE
PALETTE title_font_pal "/images/font/font_tmnt_arcade.png"

// --- Capas adicionales / Enemigos (pendientes) ---
// SPRITE foot_soldier "/sprites/enemies/foot_soldier.png" N N FAST K
