// =============================================================================
// profiles.res - Escena de perfiles (SCENE_PROFILES, modo atracto)
// =============================================================================
// Pantalla que entra sola cuando el jugador deja la seleccion de cantidad de
// jugadores quieta 30 segundos: muestra el retrato de una tortuga al azar, sus
// datos al costado y su descripcion abajo (los dos textos se dibujan con la
// fuente arcade del proyecto, title_font, NO son imagenes).
//
// Los retratos son de 64x128 px = 8x16 tiles, un solo frame cada uno
// (time 0 -> sin animacion automatica). Van como SPRITE y no como IMAGE porque
// el retrato ENTRA deslizandose desde el borde derecho: un sprite se mueve
// pixel a pixel sin tocar el tilemap.
//
// Cada PNG tiene su PROPIA paleta (los 4 no entran juntos en 16 colores), pero
// como en pantalla hay uno solo por vez, el codigo carga en PAL0 la paleta del
// que salio sorteado. El indice 0 esta declarado transparente en los 4 PNG y no
// lo usa el arte -> no hay agujeros.
//
// COMENTARIOS EN ASCII PURO (rescomp lee los .res con Cp1252).
// =============================================================================

SPRITE profile_leo  "images/profiles/leo_profile.png"  8 16 FAST 0
SPRITE profile_mike "images/profiles/mike_profile.png" 8 16 FAST 0
SPRITE profile_don  "images/profiles/don_profile.png"  8 16 FAST 0
SPRITE profile_raph "images/profiles/raph_profile.png" 8 16 FAST 0
