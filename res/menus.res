// =============================================================================
// menus.res — Gráficos de los menús y pantallas de selección
// =============================================================================
// Contiene imágenes de fondo y sprites de UI para:
//   - Pantalla de selección de número de jugadores (showPlayerSelect)
//   - Pantalla de selección de personaje (showCharSelect)
//   - Cualquier otro menú futuro (Game Over, Rankings, etc.)
// =============================================================================

// --- Imágenes de fondo (tilemaps) ---
IMAGE logo "/images/title_scene/logo_tmnt.png" BEST ALL
IMAGE characters_greyscale "/images/title_scene/characters_greyscale.png" BEST ALL

// --- Sprites de interfaz ---
SPRITE selector_turtle "/images/title_scene/selector_turtle.png" 8 8 FAST 0
SPRITE character_selector "/images/title_scene/character_selector.png" 8 16 FAST
SPRITE sega_logo_sprite "/images/title_scene/sega_logo.png" 17 10 FAST 8
SPRITE faces_hud "/images/title_scene/sprite_sheet_faces.png" 4 4 FAST 4
