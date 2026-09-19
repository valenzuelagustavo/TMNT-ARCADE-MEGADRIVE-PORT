// =============================================================================
// level2_1.res  Fondo del nivel 2-1: la calle (SCENE 2 del arcade)
// =============================================================================
// GENERADO POR tools/gen_level2_1_bg.py -- NO EDITAR A MANO.
//
// POR QUE ESTA PARTIDO EN SECCIONES
// El nivel completo son ~2069 tiles unicos y en VRAM entran ~770 (el resto se
// lo llevan los dos planos, el HUD y el presupuesto de SPR_initEx). Asi que el
// fondo se streamea POR TILES: el generador parte el recorrido en 13
// secciones y level2_1.c mantiene en VRAM, en un buffer circular de
// LVL21_RING_TILES tiles, solo las que la camara esta usando.
//
// NONE NONE es OBLIGATORIO en todas:
//   - sin comprimir, porque los tiles se copian a mano con VDP_loadTileData a
//     una direccion de VRAM que cambia en cada vuelta del ring;
//   - sin deduplicar, porque el tilemap del mundo (src/level2_1_map.c) indexa
//     los tiles por su posicion EXACTA dentro de la seccion.
//
// La paleta va aparte (un PNG de 1x16) y se carga en PAL0.
// =============================================================================

PALETTE lvl21_pal "images/lvl_2_scene/genesis/lvl21_pal.png"

TILESET lvl21_sec00 "images/lvl_2_scene/genesis/lvl21_sec00.png" NONE NONE
TILESET lvl21_sec01 "images/lvl_2_scene/genesis/lvl21_sec01.png" NONE NONE
TILESET lvl21_sec02 "images/lvl_2_scene/genesis/lvl21_sec02.png" NONE NONE
TILESET lvl21_sec03 "images/lvl_2_scene/genesis/lvl21_sec03.png" NONE NONE
TILESET lvl21_sec04 "images/lvl_2_scene/genesis/lvl21_sec04.png" NONE NONE
TILESET lvl21_sec05 "images/lvl_2_scene/genesis/lvl21_sec05.png" NONE NONE
TILESET lvl21_sec06 "images/lvl_2_scene/genesis/lvl21_sec06.png" NONE NONE
TILESET lvl21_sec07 "images/lvl_2_scene/genesis/lvl21_sec07.png" NONE NONE
TILESET lvl21_sec08 "images/lvl_2_scene/genesis/lvl21_sec08.png" NONE NONE
TILESET lvl21_sec09 "images/lvl_2_scene/genesis/lvl21_sec09.png" NONE NONE
TILESET lvl21_sec10 "images/lvl_2_scene/genesis/lvl21_sec10.png" NONE NONE
TILESET lvl21_sec11 "images/lvl_2_scene/genesis/lvl21_sec11.png" NONE NONE
TILESET lvl21_sec12 "images/lvl_2_scene/genesis/lvl21_sec12.png" NONE NONE
