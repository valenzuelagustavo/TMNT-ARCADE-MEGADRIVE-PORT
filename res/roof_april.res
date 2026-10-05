// =============================================================================
// roof_april.res - Cinematica del rescate de April (SCENE_CINEMATIC_FIRE)
// =============================================================================
// Va DESPUES de la seleccion de personaje y ANTES del titulo del nivel 1.
// Reconstruida a partir del analisis frame a frame del clip original
// (323 frames @ 30 fps).
// Todos los PNG los genera tools/gen_roof_assets.py dentro de
// images/roof_april_scene/genesis/ a partir del arte suelto de
// images/roof_april_scene/Assets/.
//
// TRES ESCENAS con corte duro + wipe diagonal entre ellas:
//   A  calle y fachada del edificio en llamas (fondo 256x224, modo H32)
//   B  tunel de techos, pasada de las 4 tortugas (fondo 256x224, modo H32)
//   C  azotea de April vista de cerca         (fondo 320x224, modo H40)
//
// PALETAS (el generador fuerza paletas COMPARTIDAS para no gastar lineas):
//   Escena A: roof_bg_a -> PAL0 | roof_climb + roof_climb_far -> PAL1 (las 4
//             tortugas juntas, 15 colores) | roof_splinter -> PAL2 |
//             roof_baloon -> PAL3
//   Escena B: roof_bg_b -> PAL0 | roof_tiny + roof_far + roof_near -> PAL1
//             (los tres salen del mismo PNG *_scale.png: MISMA paleta)
//   Escena C: roof_bg_c -> PAL0 | leo_player -> PAL1 (ver abajo: la escena C
//             NO usa este arte, usa las hojas del juego de chars.res)
//
// COMPRESION: los fondos van NONE porque el wipe de entrada los dibuja POR
// PARTES desde ROM (VDP_setTileMapEx sobre un sub-rectangulo del tilemap) y
// eso exige el mapa sin comprimir. Los sprites van FAST.
//
// LA ESCENA C NO SALE DE ACA. Usa las hojas del JUEGO (chars.res, 104x104):
// es la escena que empalma con el gameplay, asi que las tortugas tienen que
// verse como en el nivel. De este archivo la escena C solo usa el fondo.
//
// "ZOOM" SIN ESCALADO: la MegaDrive no escala sprites. El acercamiento de la
// escena B esta resuelto como en el arcade, por SUSTITUCION de sprite:
// roof_tiny (48x64) -> roof_far (72x88) -> roof_near (104x120) segun se va
// acercando. Son la MISMA pose redibujada a tres tamanos, y el generador las
// centra por la caja de su contenido, asi que el swap no pega un salto.
// (roof_tiny/far/near quedaron siendo exclusivos de la escena B.)
//
// COMENTARIOS EN ASCII PURO (rescomp lee los .res con Cp1252).
// =============================================================================

// --- Fondos: uno por escena, cada uno con su paleta en PAL0 ---
IMAGE  roof_bg_a     "images/roof_april_scene/genesis/roof_bg_a.png"     NONE
IMAGE  roof_bg_b     "images/roof_april_scene/genesis/roof_bg_b.png"     NONE
IMAGE  roof_bg_c     "images/roof_april_scene/genesis/roof_bg_c.png"     NONE

// --- Escena A: el grupo trepando la fachada ---
// 2 frames de 56x64 (7x8 tiles) x 4 filas. Frame 0 = apoyado (con sombra),
// frame 1 = en el aire. Filas: 0=Leo 1=Mike 2=Don 3=Raph (mismo orden que
// personajeSeleccionado). time 0 = la animacion la maneja el codigo.
SPRITE roof_climb     "images/roof_april_scene/genesis/roof_climb.png"     7 8 FAST 0
// Los dos ultimos saltos ya son sobre la fachada, mas lejos de camara: mismo
// truco de sustitucion que la escena B, con la celda reducida a 2/3.
SPRITE roof_climb_far "images/roof_april_scene/genesis/roof_climb_far.png" 5 6 FAST 0

// --- Escena B: pasada de las tortugas (tres tamanos) ---
// 1 frame por fila, mismas 4 filas/orden que roof_climb. El arte suelto trae
// dos tamanos; roof_tiny es roof_far reducido a 2/3 por el generador, porque
// con solo dos escalones la tortuga entra enorme en el punto de fuga.
SPRITE roof_tiny     "images/roof_april_scene/genesis/roof_tiny.png"     6  8 FAST 0
SPRITE roof_far      "images/roof_april_scene/genesis/roof_far.png"      9 11 FAST 0
SPRITE roof_near     "images/roof_april_scene/genesis/roof_near.png"    13 15 FAST 0

// --- Escena A: Splinter (2 poses de idle) y globos de dialogo ---
// roof_baloon: frame 0 = "FIRE!!", frame 1 = "HANG ON, APRIL".
SPRITE roof_splinter "images/roof_april_scene/genesis/roof_splinter.png" 6  8 FAST 0
SPRITE roof_baloon   "images/roof_april_scene/genesis/roof_baloon.png"   8  4 FAST 0

// --- Escena A: humo animado sobre el incendio (streaming de tiles, NO
// sprite: son 152 tiles/frame, con SPRITE cargarian los 3 frames a la vez
// en VRAM = 362 tiles de golpe, justo lo que hizo falta recortarle a esta
// escena ayer). Va como fondo: mismo truco que fire_strip.png/smoke_lvl1.png
// (tira VERTICAL de tools/gen_roof_smoke.py, para que cada frame quede
// contiguo en ROM), comparte la paleta de roof_bg_a (PAL0, paleta forzada
// en el generador). NONE NONE porque se indexa por frame*152*32 bytes, el
// dedup de rescomp rompe ese mapeo 1:1.
TILESET roof_bg_a_smoke "images/roof_april_scene/genesis/roof_bg_a_smoke_strip.png" NONE NONE
