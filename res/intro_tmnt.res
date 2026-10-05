// =============================================================================
// intro_tmnt.res - Intro arcade de TMNT (SCENE_INTRO_ARCADE)
// =============================================================================
// Reconstruida a partir del analisis frame a frame de la intro original
// (7 escenas, 940 ticks NTSC). Todos los PNG los genera
// tools/gen_intro_assets.py dentro de images/intro_tmnt/genesis/ a partir del
// arte fuente de images/intro_tmnt/ y de images/intro_tmnt/assets/.
//
// PALETAS (el generador fuerza paletas COMPARTIDAS para no gastar lineas):
//   Escenas A/B/C: intro_dolly + intro_luz + intro_tapa -> una sola PAL0 (16 col, al limite)
//   Escena  A:     intro_nube_chica + intro_nube_grande -> PAL2 (paleta propia,
//                  no entran en la PAL0 de arriba que ya esta al tope)
//   Escena  C:     intro_turtles -> PAL1 (paleta unificada de las 4 tortugas)
//   Escena  D:     intro_quad -> PAL0 (16 col, los 4 retratos juntos)
//   Escenas E/F:   intro_banner + intro_logo + intro_konami -> PAL0 (13 col)
//
// COMPRESION: todo NONE. Los tilemaps se indexan/dibujan por partes desde ROM
// (streaming de filas del dolly, crecimiento del haz, crecimiento de los
// cuadrantes, wipe del logo) y eso exige mapa sin comprimir.
// COMENTARIOS EN ASCII PURO (rescomp lee los .res con Cp1252).
// =============================================================================

// --- Escenas A/B: tira vertical completa del dolly (256x1496 = 32x187 tiles,
//     1001 tiles unicos). NO entra en un plano: se streamea por filas. ---
IMAGE  intro_dolly   "images/intro_tmnt/genesis/intro_dolly.png"   NONE

// --- Escena C: haz de luz (64x224, 11 tiles unicos) y tapa de alcantarilla ---
IMAGE  intro_luz     "images/intro_tmnt/genesis/intro_luz.png"     NONE
SPRITE intro_tapa    "images/intro_tmnt/genesis/intro_tapa.png"    8 4 NONE 0

// --- Escena A: nubes que cruzan el cielo por debajo de la luna, cada una en
//     su direccion. Paleta propia PAL2 (5 colores): la PAL0 de arriba ya
//     esta al limite de 16. Sprites estaticos (sin animacion, time 0). ---
SPRITE intro_nube_chica  "images/intro_tmnt/genesis/intro_nube_chica.png"  11 4 NONE 0
SPRITE intro_nube_grande "images/intro_tmnt/genesis/intro_nube_grande.png" 26 5 NONE 0

// --- Escena C: salto de las 4 tortugas. Hoja REDUCIDA: fila 6 (ANIM_JUMP)
//     frames 1..9 de cada personaje, recortados a 72x80 (9x10 tiles):
//     0..7 = vuelo, 8 = aterrizaje.
//     Filas: 0=Leo 1=Mike 2=Don 3=Raph (mismo orden que personajeSeleccionado).
//     time 0 = animacion manual desde el codigo. ---
SPRITE intro_turtles "images/intro_tmnt/genesis/intro_turtles.png" 9 10 FAST 0

// --- Escena D: los 4 retratos ya compuestos en sus cuadrantes (256x224) ---
// (22/09) Rip nuevo, con mejor uso de color:
// "Arcade---Teenage-Mutant-Ninja-Turtles---TELA-1.png". NO se puede apuntar
// directo a ese PNG: mide 252 de ancho (rescomp exige multiplo de 8) y su
// indice 0 es el magenta del fondo de Raph (en Megadrive el 0 es transparente).
// tools/gen_intro_quad_tela.py lo lleva a 256 y libera el indice 0; SI SE
// RETOCA EL RIP HAY QUE VOLVER A CORRERLO. intro_quad.png (el viejo) queda en
// la carpeta sin usar: lo sigue generando gen_intro_assets.py.
IMAGE  intro_quad    "images/intro_tmnt/genesis/intro_quad_tela.png"    NONE

// --- Escenas E/F: banner, logo TURTLES y copyright de Konami ---
IMAGE  intro_banner  "images/intro_tmnt/genesis/intro_banner.png"  NONE
IMAGE  intro_logo    "images/intro_tmnt/genesis/intro_logo.png"    NONE
IMAGE  intro_konami  "images/intro_tmnt/genesis/intro_konami.png"  NONE
