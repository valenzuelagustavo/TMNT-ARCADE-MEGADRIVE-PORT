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
//   Escena  D:     intro_quad -> PAL0 (16 col, los 4 retratos juntos, 320x224)
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
// (16/10) SACADO (sin uso, ver intro_arcade.c):
// SPRITE intro_nube_grande "images/intro_tmnt/genesis/intro_nube_grande.png" 26 5 NONE 0

// --- Escena C: salto de las 4 tortugas. Hoja REDUCIDA: fila 6 (ANIM_JUMP)
//     frames 1..9 de cada personaje, recortados a 72x80 (9x10 tiles):
//     0..7 = vuelo, 8 = aterrizaje.
//     Filas: 0=Leo 1=Mike 2=Don 3=Raph (mismo orden que personajeSeleccionado).
//     time 0 = animacion manual desde el codigo. ---
SPRITE intro_turtles "images/intro_tmnt/genesis/intro_turtles.png" 9 10 FAST 0

// --- Escena D: los 4 retratos ya compuestos en sus cuadrantes ---
// (09/10) Reversion a 320x224: "Arcade - Teenage Mutant Ninja Turtles - TELA 1
// 320X224.png", paleta nueva y marco negro. tools/gen_intro_quad_tela.py la
// copia a intro_quad_tela.png (ya no hay padding ni corrimiento). Lo de abajo
// es la historia del rip anterior de 252 de ancho.
// (22/09) Rip nuevo, con mejor uso de color:
// "Arcade---Teenage-Mutant-Ninja-Turtles---TELA-1.png". NO se puede apuntar
// directo a ese PNG: mide 252 de ancho (rescomp exige multiplo de 8) y su
// indice 0 es el magenta del fondo de Raph (en Megadrive el 0 es transparente).
// tools/gen_intro_quad_tela.py lo lleva a 256 y libera el indice 0; SI SE
// RETOCA EL RIP HAY QUE VOLVER A CORRERLO. intro_quad.png (el viejo) queda en
// la carpeta sin usar: lo sigue generando gen_intro_assets.py.
IMAGE  intro_quad    "images/intro_tmnt/genesis/intro_quad_tela.png"    NONE

// --- Escenas E/F: banner, logo TURTLES y copyright de Konami ---
// (09/10) Arte nuevo con la paleta mejorada: la pantalla 1 de
// "Arcade - ... -TELA START.png", a 320 de ancho, partida por
// tools/gen_title_screens.py en el banner (filas de tile 0..4), el logo
// (6..15, solo los pixeles de "TURTLES") y el resto (desde la fila 5: TM,
// logo de Konami y el texto de copyright).
// (14/10) Ahora sale de "... -TELA START _GENESIS_FINAL.png", con "GENESIS"
// debajo del logo: banner en las filas 0..5, logo con GENESIS y TM en las
// 4..15 (va en BG_A, por encima del banner) y el resto desde la 16. Comparten paleta; su color 0 es el
// celeste del fondo. Reemplazan a intro_banner/intro_logo/intro_konami (los
// PNG viejos quedan en genesis/ sin declarar).
IMAGE  title_banner  "images/intro_tmnt/genesis/title_banner.png"  NONE
IMAGE  title_logo    "images/intro_tmnt/genesis/title_logo.png"    NONE
IMAGE  title_rest    "images/intro_tmnt/genesis/title_rest.png"    NONE
