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
// segundos seguidos. La de Fight NO hace eso (probado en la pelea
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
XGM2 music_level1    "music/04 - Fire! (Stage 1-1).vgm"
XGM2 music_level2    "music/05 - April's Room (Stage 1-2).vgm"
XGM2 music_charselect "music/03 - Choose Your Turtle.vgm"
XGM2 music_profiles  "music/02 - Character Profiles.vgm"
XGM2 music_credits   "music/00 - SanSenpai Credit.vgm"
XGM2 music_ending    "music/07 - April is Kidnapped (Cutscene).vgm"
XGM2 music_intro_arcade "music/01 - Opening Demo.vgm"
XGM2 music_level3    "music/10 - The Sewers (Stage 2-2).vgm"
// (01/10) Temas del garage (Scene 4) y de la autopista (Scene 5).
XGM2 music_garage    "music/11 - Parking Garage (Scene 2-3) & The Factory (Scene 4).vgm"
XGM2 music_freeway   "music/13 - Highway Blockade (Scene 3-1).vgm"

// (06/10) Highway Chaser (Scene 3-2): el skate del 6-1. 45,5 s con loop de
// 29,3 s; trae el bloque PCM de bateria pero no lo dispara (FM+PSG puro).
XGM2 music_skate     "music/14 - Highway Chaser (Scene 3-2).vgm"
// (06/10) The Technodrome (Scene 5): el 8-1 hasta que aparece Traag. 66,5 s
// con loop de 47 s, sin PCM. La sala de Shredder (9-1) sigue con el del jefe.
XGM2 music_technodrome "music/16 - The Technodrome (Scene 5).vgm"

// (24/09) Tema del Stage 2-1 (Downtown). 77,7 s con punto de loop a los 38,5 s
// (loop de 39,2 s), asi que con XGM2_setLoopNumber(-1) repite solo. Trae un
// bloque PCM de 7690 bytes (el mismo de bateria que los "con samples"), pero
// NUNCA lo dispara: 0 comandos de start de stream (0x95), solo setup. O sea
// que suena FM+PSG puro y no le pisa los canales PCM a los voice over.
XGM2 music_stage2_1  "music/08 - Downtown (Stage 2-1).vgm"

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
// (01/10) Embestida de Rocksteady: el sonido de la CORRIDA hacia el logo de SEGA
// (musica_intro.vgm, no el golpe: es un tema XGM2 de FM y no se puede tocar
// encima de la musica del nivel). Renderizado a WAV con libgme (1.1 s, 8-bit 13300 Hz
// mono, normalizado) para tocarlo como PCM.
WAV rocksteady_charge_sfx "audio/rocksteady_charge.wav" XGM2
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
// (medido sobre los wav: pico ~100%, y el RMS sale del
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
// el resto de los voice over (mismo criterio de mezcla).
// El original de 48k queda al lado como who_put_the_light_out_orig48k.wav.
WAV who_put_light_vo "audio/who_put_the_light_out.wav" XGM2

// --- TV de la vidriera (2-1, 27/09) ----------------------------------------
// Cuando se prende la tele: April pide ayuda y Shredder la interrumpe.
// * "Help me!" es la MISMA toma que help_me_april.wav (help_me.wav, el
//   original de 48k, correlaciona 0,99 con la ya convertida), asi que se
//   reusa help_me_april_vo y no se declara otra copia (1,28 s, rms 22,4%).
// * "Tonight I dine on turtle soup!" llego como WAV ESTEREO 8-bit 48000 Hz de
//   2,145 s. Convencion de la casa: MONO, 8-bit, 11025 Hz, puntas recortadas,
//   pico 96,9%. Normalizada al pico quedaba en rms 11,7% (unos pocos picos
//   muy por encima del resto), muy por debajo de la banda de voz: se le dio
//   ganancia hasta que el percentil 99,5 toco el techo y lo que pasaba del 80%
//   se redondeo con una rodilla suave (tanh; toca solo el 1,1% de las
//   muestras). Medido despues de convertir:
//   dinne_turtle            2,145s  pico 96,9%  rms 22,0%  23.649 B
// El original de 48k queda al lado como dinne_turtle_orig48k.wav.
WAV dinne_turtle_vo "audio/dinne_turtle.wav" XGM2

// --- Baxter Stockman, jefe de la Scene 3 (30/09) ----------------------------
// "I'm invincible!" al aparecer el jefe. Llego ya MONO 8-bit 11025 Hz (2,0 s)
// pero bajo: pico 43%, rms 7,8%. Se recorto la cola de silencio y se le dio
// ganancia (x2,79) hasta rms 22% (la banda de voz), redondeando lo que pasaba
// del 80% con la rodilla suave (tanh, toca el 0,8% de las muestras), igual
// que dinne_turtle. Medido despues de convertir:
//   im_invinsible_baxter    1,947s  pico 96,6%  rms 22,0%  21.469 B
// El original queda al lado como im_invinsible_baxter_orig.wav.
WAV im_invinsible_baxter_vo "audio/im_invinsible_baxter.wav" XGM2
