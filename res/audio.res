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
// (17/09, 2da pasada) FIRE! se queda en su version FM+PSG; FIGHT! pasa a la
// version CON SAMPLES.
//
// La "Fire con samples" DEJA MUDOS los canales PCM del juego: con ella sonando,
// un SFX disparado en CH2 (y en CH3) no sale por el DAC -- medido con un tono
// de 3,7 kHz: +24 dB sobre la musica con el tema viejo y CERO con el nuevo, 20
// segundos seguidos. La de Fight NO hace eso (probado por Gustavo en la pelea
// contra Rocksteady), asi que esa se usa.
//
// Aparte de eso, en las dos la bateria esta PITCHEADA: traen el mismo bloque
// PCM de 7690 bytes y le cambian la frecuencia de reproduccion con comandos de
// stream -- Fire 72 veces entre 11 valores distintos (12.713..32.000 Hz) y
// Fight 83 veces entre 16 (11.326..32.000 Hz). XGM2 toca PCM a UNA sola
// frecuencia fija (13,3 kHz, o 6,65 a media velocidad) y no puede representar
// eso, asi que los samples no suenan a la altura escrita. Para que suenen bien
// hay que exportarlos a UNA frecuencia fija y, si hacen falta varias alturas,
// usar una muestra distinta por altura.
// Ver DEVLOG, entradas del 17/09.
XGM2 music_sega      "musica_intro.vgm"
XGM2 music_level1    "music/Fire!_(Stage 1-1).vgm"
XGM2 music_level2    "music/05 - April's Room (Stage 1-2).vgm"
XGM2 music_charselect "music/03 - Choose Your Turtle.vgm"
XGM2 music_profiles  "music/02 - Character Profiles.vgm"
XGM2 music_credits   "music/00 - SanSenpai Credit.vgm"
XGM2 music_ending    "music/07 - April is Kidnapped (Cutscene).vgm"
XGM2 music_intro_arcade "music/01 - Opening Demo.vgm"

// Tema del jefe (Rocksteady). Arranca cuando se abre la puerta de la capsula
// del taladro y se mantiene toda la pelea: el VGM trae punto de loop (32,5s de
// duracion, loopSamples != 0), asi que con XGM2_setLoopNumber(-1) repite solo.
XGM2 music_boss      "music/Fight con samples (Prueba Gus).vgm"

// Jingle de nivel completado (4s, SIN punto de loop -> hay que reproducirlo con
// XGM2_setLoopNumber(0) ANTES del play, si no el driver lo repite). Declarado y
// listo para usar; todavia no esta enganchado a ninguna escena.
XGM2 music_scene_clear "music/09 - Scene Clear Theme 1.vgm"

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
WAV scream_april "audio/scream_april.wav" XGM2
WAV iron_ball_sfx "audio/iron_ball.wav" XGM2
WAV attack_turtles "audio/attack_turtles.wav" XGM2
WAV hit_turtles "audio/hit_turtles.wav" XGM2
WAV boss_hit "audio/boss_hit.wav" XGM2
WAV foot_soldier_explode "audio/foot_soldier_explode.wav" XGM2
WAV drill_sfx "audio/drill.wav" XGM2
WAV electric_shock_sfx "audio/electric_shock.wav" XGM2
WAV capsule_door_sfx "audio/capsule_door.wav" XGM2
WAV say_your_p_sfx "audio/say_your_p.wav" XGM2
WAV shredder_laugh_sfx "audio/shredder_laugh.wav" XGM2

// Voice over de la cinematica de rescate (SCENE_CINEMATIC_FIRE, escena A,
// globos "Fire!!" / "Hang on, April") y del arranque de la 2da parte del
// nivel 1 (SCENE_LEVEL2, "April's Room") y del robot del latigo (final de
// la 1ra parte). Origen: WAV estereo 48000 Hz, convertidos a mono 11025 Hz
// 8-bit (misma convencion que el resto de esta seccion) antes de pasarlos
// por rescomp.
WAV fire_vo "audio/fire.wav" XGM2
WAV hang_on_april_vo "audio/hang_on_april.wav" XGM2
WAV robot_twip_sfx "audio/robot_twip.wav" XGM2
WAV help_me_april_vo "audio/help_me_april.wav" XGM2

// --- Tanda del 14/09 -------------------------------------------------------
// Los cinco llegaron como WAV ESTEREO 8-bit 48000 Hz y se convirtieron a la
// convencion de la casa: MONO, 8-bit, 11025 Hz, normalizados a pico ~97%
// (ver claude/audio-mix-voz-vs-musica.md: pico ~100%, y el RMS sale del
// contenido). Medidos despues de convertir:
//   leo_raph_attack        0,33s  pico 96,9%  rms 31,8%   3.672 B
//   mike_don_attack        0,27s  pico 96,9%  rms 25,8%   3.032 B
//   cowabunga              1,22s  pico 96,9%  rms 24,5%  13.451 B
//   boss_scream_rocksteady 1,69s  pico 96,9%  rms 17,4%  18.633 B
//   lost_life_turtles      2,22s  pico 96,9%  rms 18,7%  24.476 B
// Los dos gruñidos de ataque quedan con RMS por encima de la banda de voz
// (20-24%) a proposito: son golpes cortos que tienen que cortar por encima de
// la musica, igual que hit_turtles (23,2%).
WAV leo_raph_attack_vo  "audio/leo_raph_attack.wav" XGM2
WAV mike_don_attack_vo  "audio/mike_don_attack.wav" XGM2
WAV cowabunga_vo        "audio/cowabunga.wav" XGM2
WAV boss_scream_rocksteady_vo "audio/boss_scream_rocksteady.wav" XGM2
WAV lost_life_turtles_vo "audio/lost_life_turtles.wav" XGM2

// (24/09) Grito de Bebop, el jefe del 2-1. Llego como los otros (ESTEREO
// 8-bit 48000 Hz, 2,562 s) y se paso a la convencion de la casa: MONO, 8-bit,
// 11025 Hz, silencio de las puntas recortado y normalizado a pico 96,9%.
// Medido despues de convertir: 2,562 s  pico 96,9%  rms 19,6%  28.246 B.
// El original quedo como boss_scream_bebop_orig48k.wav.
WAV boss_scream_bebop_vo "audio/boss_scream_bebop.wav" XGM2

// --- Caida por la boca de tormenta (2-1, 20/09) ----------------------------
// "Duuuh, who put the light out". Llego como WAV ESTEREO 8-bit 48000 Hz de
// 1,308 s y se paso a la convencion de la casa: MONO, 8-bit, 11025 Hz,
// recortado el silencio de las puntas y normalizado a pico 96,9%.
// Medido despues de convertir:
//   who_put_the_light_out   1,301s  pico 96,9%  rms 21,4%  14.340 B
// El RMS cae justo en la banda de voz (20-24%), asi que se mezcla igual que
// el resto de los voice over (ver claude/audio-mix-voz-vs-musica.md).
// El original de 48k queda al lado como who_put_the_light_out_orig48k.wav.
WAV who_put_light_vo "audio/who_put_the_light_out.wav" XGM2
