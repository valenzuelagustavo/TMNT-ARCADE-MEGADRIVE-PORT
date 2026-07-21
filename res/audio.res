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

// --- Voice over ---
// Grito del arranque del nivel ("Attack!!"). El WAV de origen es mono 8-bit a
// 11025 Hz; rescomp lo reconvierte a 8-bit SIGNED, lo resamplea al rate por
// defecto de XGM2 (13.3 kHz) y ajusta el tamano a un multiplo de 256 bytes.
// Se dispara con XGM2_playPCMEx sobre un canal PCM libre (CH2), asi la musica
// del nivel (CH1) sigue sonando por debajo. Sintaxis: WAV name file driver
// [out_rate] -> out_rate se omite = 13300 (hay que reproducirlo a rate normal).
WAV attack_vo "audio/attack.wav" XGM2
