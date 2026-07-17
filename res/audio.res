// =============================================================================
// audio.res — Todo el audio del juego
// =============================================================================
// REGLA: Toda la musica y SFX va UNICAMENTE aqui.
//        Nunca declarar audio en otros .res.
//
// Driver XGM2 (antes XGM): permite regular el volumen en tiempo real con
// XGM2_setFMVolume / XGM2_setPSGVolume (0..100). El XGM clasico no lo permite.
// En el codigo: XGM_startPlay -> XGM2_play, XGM_stopPlay -> XGM2_stop.
// =============================================================================

// --- Música ---
XGM2 music_sega    "musica_intro.vgm"
XGM2 select_music  "/music/turtles2-selectcharacter_wip2.vgm"
XGM2 music_level1  "music/fire_v3.vgm" 

// --- Efectos de Sonido ---
XGM2 golpe         "golpe.vgm"
