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
// (23/10, rama mapper) NEAR: todo lo de este archivo va en los primeros
// 3 MB de la ROM (sin bank switch). Lo lee el codigo directo de la ROM
// (fondos streameados, tiles por cuadro, mapas) o lo toca el Z80 (audio).
// Ver tools/mapper/README.md.
NEAR

XGM2 music_sega      "musica_intro.vgm"
// (17/10) Tanda nueva de vgm: cada archivo se llama como su escenario.
XGM2 music_level1    "music/04 - Fire.vgm"
XGM2 music_level2    "music/05 - April's Room.vgm"
XGM2 music_charselect "music/03 - Choose Your Turtle.vgm"
XGM2 music_profiles  "music/02 - Character Profiles.vgm"
XGM2 music_credits   "music/00 - SanSenpai Credit.vgm"
// (02/11) Jingle del GAME OVER (3,5 s, sin loop): src/gameover.c.
XGM2 music_gameover  "music/28 - Game Over.vgm"
// (02/11) Tema del CONTINUE (35 s, sin loop): suena durante la cuenta grande
// (un solo jugador), scenes.c contMusic.
XGM2 music_continue  "music/27 - Continue.vgm"
// (02/11) Nueva version del tema del Technodrome (con las muestras de bateria
// del Hyperstone Heist). Por ahora SOLO en el sound test (BGM29); el nivel 6-1
// sigue con music_technodrome. Para reemplazarlo: cambiar el playMusicVol de
// level8_1.c.
XGM2 music_technodrome_hs "music/29 - The Technodrome (Hyperstone Heist Drum Samples).vgm"
XGM2 music_ending    "music/07 - April is Kidnapped.vgm"
// (12/10) Version con FM6 LIBRE (tools/vgm_free_fm6.py): el original usaba
// FM6 como canal FM y apagaba el DAC, y con eso la voz de la intro (PCM)
// salia muda. FM5 era una copia exacta del bajo de FM4: ahora FM5 hace lo que
// hacia FM6 y FM6 queda para el PCM. Si el artista cambia el vgm, volver a
// correr el script.
// (13/10) Voz DESACTIVADA por ahora (ver INTRO_VOICE en intro_arcade.c): se
// vuelve al vgm original. Para reactivarla, usar la linea comentada.
XGM2 music_intro_arcade "music/01 - Opening Demo.vgm"
// XGM2 music_intro_arcade "music/01 - Opening Demo (FM6 libre).vgm"
XGM2 music_level3    "music/10 - The Sewers.vgm"
// (01/10) Temas del garage (Scene 4) y de la autopista (Scene 5).
XGM2 music_garage    "music/11 - Parking Garage - The Factory.vgm"
XGM2 music_freeway   "music/13 - Highway Blockade.vgm"

// (06/10) Highway Chaser (Scene 3-2): el skate del 6-1. 45,5 s con loop de
// 29,3 s; trae el bloque PCM de bateria pero no lo dispara (FM+PSG puro).
XGM2 music_skate     "music/14 - Highway Chaser.vgm"
// (06/10) The Technodrome (Scene 5): el 8-1 hasta que aparece Traag. 66,5 s
// con loop de 47 s, sin PCM. La sala de Shredder (9-1) sigue con el del jefe.
XGM2 music_technodrome "music/16 - The Technodrome.vgm"

// (24/09) Tema del Stage 2-1 (Downtown). 77,7 s con punto de loop a los 38,5 s
// (loop de 39,2 s), asi que con XGM2_setLoopNumber(-1) repite solo. Trae un
// bloque PCM de 7690 bytes (el mismo de bateria que los "con samples"), pero
// NUNCA lo dispara: 0 comandos de start de stream (0x95), solo setup. O sea
// que suena FM+PSG puro y no le pisa los canales PCM a los voice over.
XGM2 music_stage2_1  "music/08 - Downtown.vgm"

// (10/10) Declarados solo para el SOUND TEST de opciones (BGM13 y BGM16): por
// ahora ninguna escena los usa. 4,2 s y 5,6 s, sin loop, FM+PSG (traen el
// bloque PCM de bateria pero no lo disparan).
XGM2 music_scene_clear2    "music/12 - Scene Clear Theme 2 (1).vgm"
// (17/10) "15 - It's Technodrome" es para una cinematica que todavia no
// existe: por ahora solo suena en el sound test.
XGM2 music_techno_cutscene "music/15 - It's Technodrome.vgm"

// Tema de TODOS los jefes. Arranca con la pelea y se mantiene hasta el final:
// el VGM trae punto de loop, asi que con XGM2_setLoopNumber(-1) repite solo.
// (09/10) Version final del artista: "06 - Fight! (Boss Theme).vgm", 40,4 s
// con loop de 31,1 s, FM+PSG puro (sin bloque PCM). Reemplaza a la "Fight con
// samples (Prueba Gus)", que pitcheaba la bateria 82 veces entre 16
// frecuencias y en XGM2 sonaba rota (ver el comentario de arriba).
XGM2 music_boss      "music/06 - Fight!.vgm"
// (17/10) Temas propios de la sala final (level9_1.c): Krang y Shredder ya no
// usan music_boss. FM+PSG puro (sin bloques PCM). Medidos:
//   17 - VS. Krank     41,5 s, loop 38,3 s
//   18 - VS. Shredder  51,8 s, loop 37,2 s
XGM2 music_vs_krang    "music/17 - VS. Krank.vgm"
XGM2 music_vs_shredder "music/18 - VS. Shredder.vgm"
// (20/10) Pistas extra del sound test, se desbloquean con A B A C A B B en la
// pantalla de cantidad de jugadores (scenes.c, soundExtra).
XGM2 music_shogun   "music/20 - Shogun.vgm"
XGM2 music_winter   "music/21 - Winter's Here.vgm"

// (30/10) Los dos temas del final (src/ending.c), SIN punto de loop: suenan UNA vez
// (XGM2_setLoopNumber(0) ANTES del play) y la escena dura lo que el vgm:
//   21 - Epilogue       38,40 s  (1693440 muestras @ 44100 = 2304 cuadros NTSC / 1920 PAL)
//   22 - Turtle Boogie  79,45 s  (3503668 muestras @ 44100 = 4767 cuadros NTSC / 3973 PAL)
XGM2 music_epilogue      "music/21 - Epilogue.vgm"
XGM2 music_turtle_boogie "music/22 - Turtle Boogie.vgm"

// Jingle de nivel completado (4s, SIN punto de loop -> hay que reproducirlo con
// XGM2_setLoopNumber(0) ANTES del play, si no el driver lo repite). Declarado y
// listo para usar; todavia no esta enganchado a ninguna escena.
XGM2 music_scene_clear "music/09 - Scene Clear Theme 1.vgm"

// (14/10) Jingle del logo animado de KONAMI (2,5 s, SIN punto de loop: va con
// XGM2_setLoopNumber(0) antes del play). Lo toca src/konami_logo.c.
XGM2 music_konami "music/19 - Konami Logo.vgm"

// --- Efectos de Sonido ---
XGM2 golpe         "golpe.vgm"

// --- Voice over ---
// Grito del arranque del nivel ("Attack!!"). El WAV de origen es mono 8-bit a
// 11025 Hz; rescomp lo reconvierte a 8-bit SIGNED, lo resamplea al rate por
// defecto de XGM2 (13.3 kHz) y ajusta el tamano a un multiplo de 256 bytes.
// Se dispara con XGM2_playPCMEx sobre un canal PCM libre (CH2), asi la musica
// del nivel (CH1) sigue sonando por debajo. Sintaxis: WAV name file driver
// [out_rate] -> out_rate se omite = 13300 (hay que reproducirlo a rate normal).
WAV attack_vo "audio/6650/attack.wav" XGM2 6650
// (12/10) VOZ DE LA INTRO, cantada encima de music_intro_arcade (ver
// introVo[] en intro_arcade.c): "Teenage Mutant Ninja Turtles" (sola y con
// coro), "Heroes in a half shell" y "Turtle power!". Se declaran las de
// intro_voice/gen/, que arma tools/gen_intro_voice.py desde las originales:
// volumen normalizado (venian con picos de 15/128 y no se escuchaban) y
// comprimidas en el tiempo al tempo del vgm sin cambiar el tono. Si se
// cambian las originales, volver a correr el script.
// (13/10) DESACTIVADAS (no ocupan ROM): descomentar junto con INTRO_VOICE 1.
// WAV intro_vo_tmnt   "music/intro_voice/gen/01_teenage_mutant.wav" XGM2
// WAV intro_vo_tmnt2  "music/intro_voice/gen/02_teenage_mutant.wav" XGM2
// WAV intro_vo_heroes "music/intro_voice/gen/03_heroes_in_a_half_shell.wav" XGM2
// WAV intro_vo_power  "music/intro_voice/gen/04_turtle_power.wav" XGM2
WAV scream_april "audio/6650/scream_april.wav" XGM2 6650
// (31/10) iron_ball.wav NUEVO (1,04 s, un golpe de metal con cola; el viejo era de 0,2 s): suena en
// cada pique de las bolas de la escalera del 1-1 y de las del ascensor del Technodrome (scenes.c,
// techno_hazards.c). El viejo queda como krang_drop_sfx: Krang lo seguia usando al caer del techo
// y no se le toca el sonido (krang.c).
//   iron_ball_sfx   1,04s  pico 94%  rms 22,4%  6.916 B
//   krang_drop_sfx  0,20s  pico 29%  rms 12,0%  1.330 B
WAV iron_ball_sfx "audio/6650/iron_ball.wav" XGM2 6650
WAV krang_drop_sfx "audio/6650/krang_drop.wav" XGM2 6650
// WAV attack_turtles "audio/attack_turtles.wav" XGM2
WAV hit_turtles "audio/6650/hit_turtles.wav" XGM2 6650
// (24/10) boss_hit sonaba muy bajo (pico 21,9%, rms 5,5%): ganancia x4,58
// hasta rms 25% con la rodilla suave tanh desde el 80% (toca el 0,3%).
//   boss_hit                0,167s  pico 94,1%  rms 25,0%
// El original queda como boss_hit_orig.wav.
WAV boss_hit "audio/6650/boss_hit.wav" XGM2 6650
// (01/10) Embestida de Rocksteady: el sonido de la CORRIDA hacia el logo de SEGA
// (musica_intro.vgm, no el golpe: es un tema XGM2 de FM y no se puede tocar
// encima de la musica del nivel). Renderizado a WAV con libgme (1.1 s, 8-bit 13300 Hz
// mono, normalizado) para tocarlo como PCM.
WAV rocksteady_charge_sfx "audio/6650/rocksteady_charge.wav" XGM2 6650
WAV foot_soldier_explode "audio/6650/foot_soldier_explode.wav" XGM2 6650
WAV drill_sfx "audio/6650/drill.wav" XGM2 6650
WAV electric_shock_sfx "audio/6650/electric_shock.wav" XGM2 6650
WAV capsule_door_sfx "audio/6650/capsule_door.wav" XGM2 6650
WAV say_your_p_sfx "audio/6650/say_your_p.wav" XGM2 6650
WAV shredder_laugh_sfx "audio/6650/shredder_laugh.wav" XGM2 6650

// (06/10) Sala final (9-1). Sacados del remake de PC (tmntarcadehd) y pasados
// a la convencion de la casa: MONO, 8-bit, 11025 Hz, puntas recortadas y
// normalizados a pico 96,9%.
//   krang_speech_vo    1,92 s  la cabeza de Krang al escaparse del cuerpo
//   shredder_death_vo  3,11 s  el grito de Shredder al caer
WAV krang_speech_vo    "audio/6650/krang_speech.wav" XGM2 6650
WAV shredder_death_vo  "audio/6650/shredder_death.wav" XGM2 6650

// Voice over de la cinematica de rescate (SCENE_CINEMATIC_FIRE, escena A,
// globos "Fire!!" / "Hang on, April") y del arranque de la 2da parte del
// nivel 1 (SCENE_LEVEL2, "April's Room") y del robot del latigo (final de
// la 1ra parte). Origen: WAV estereo 48000 Hz, convertidos a mono 11025 Hz
// 8-bit (misma convencion que el resto de esta seccion) antes de pasarlos
// por rescomp.
WAV fire_vo "audio/6650/fire.wav" XGM2 6650
WAV hang_on_april_vo "audio/6650/hang_on_april.wav" XGM2 6650
WAV robot_twip_sfx "audio/6650/robot_twip.wav" XGM2 6650
WAV help_me_april_vo "audio/6650/help_me_april.wav" XGM2 6650

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
// (16/10) Los dos grunidos REEMPLAZADOS por los del arcade ("leo (raph)
// attack.wav" y "mike (don) attack.wav"): los de arriba quedaron en
// audio/reemplazados/. Los nuevos salen de tools/gen_sfx_arcade.py.
WAV leo_raph_attack_vo  "audio/6650/gen/leo_raph_attack.wav" XGM2 6650
WAV mike_don_attack_vo  "audio/6650/gen/mike_don_attack.wav" XGM2 6650
WAV cowabunga_vo        "audio/6650/cowabunga.wav" XGM2 6650
WAV boss_scream_rocksteady_vo "audio/6650/boss_scream_rocksteady.wav" XGM2 6650
WAV lost_life_turtles_vo "audio/6650/lost_life_turtles.wav" XGM2 6650

// (24/09) Grito de Bebop, el jefe del 2-1. Llego como los otros (ESTEREO
// 8-bit 48000 Hz, 2,562 s) y se paso a la convencion de la casa: MONO, 8-bit,
// 11025 Hz, silencio de las puntas recortado y normalizado a pico 96,9%.
// Medido despues de convertir: 2,562 s  pico 96,9%  rms 19,6%  28.246 B.
// El original quedo como boss_scream_bebop_orig48k.wav.
WAV boss_scream_bebop_vo "audio/6650/boss_scream_bebop.wav" XGM2 6650

// --- Caida por la boca de tormenta (2-1, 20/09) ----------------------------
// "Duuuh, who put the light out". Llego como WAV ESTEREO 8-bit 48000 Hz de
// 1,308 s y se paso a la convencion de la casa: MONO, 8-bit, 11025 Hz,
// recortado el silencio de las puntas y normalizado a pico 96,9%.
// Medido despues de convertir:
//   who_put_the_light_out   1,301s  pico 96,9%  rms 21,4%  14.340 B
// El RMS cae justo en la banda de voz (20-24%), asi que se mezcla igual que
// el resto de los voice over (mismo criterio de mezcla).
// El original de 48k queda al lado como who_put_the_light_out_orig48k.wav.
WAV who_put_light_vo "audio/6650/who_put_the_light_out.wav" XGM2 6650

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
// (11/10) REEMPLAZADA por una toma nueva (a la vieja le faltaba una palabra).
// Llego MONO 8-bit 11025 Hz de 3,08 s pero baja (pico 44,5%, rms 4,9%):
// puntas recortadas (10 ms antes / 20 ms despues de la voz) y ganancia x4,76
// hasta rms 22% con la rodilla suave tanh desde el 80% (toca el 1,2%).
//   dinne_turtle            2,765s  pico 96,9%  rms 22,0%  30.479 B
// La toma nueva sin procesar queda como dinne_turtle_orig.wav. La tele mide
// la fase de Shredder con sizeof(), asi que se adapta sola al largo nuevo.
WAV dinne_turtle_vo "audio/6650/dinne_turtle.wav" XGM2 6650

// --- Voice over del 2-1 (11/10) ----------------------------------------------
// Los cuatro llegaron MONO 8-bit 11025 Hz pero bajos (pico 39-48%, rms 7-9%).
// Mismo tratamiento que dinne_turtle: puntas recortadas, ganancia hasta
// rms 22% y rodilla tanh desde el 80%. Los originales: *_orig.wav.
//   pizza_time              0,931s  pico 93,9%  rms 22,0%  10.267 B
//   youre_dead_shell_brain  2,404s  pico 96,9%  rms 22,0%  26.508 B  (Bebop)
//   any_prettier            1,363s  pico 90,6%  rms 22,0%  15.031 B  (tortuga)
//   shut_your_mouth         2,034s  pico 96,9%  rms 22,0%  22.421 B  (Bebop)
// Van en CH3 con prioridad 15 (como la tele): en CH2 los cortarian los golpes.
WAV pizza_time_vo      "audio/6650/pizza_time.wav" XGM2 6650
WAV youre_dead_vo      "audio/6650/youre_dead_shell_brain.wav" XGM2 6650
WAV any_prettier_vo    "audio/6650/any_prettier.wav" XGM2 6650
WAV shut_your_mouth_vo "audio/6650/shut_your_mouth.wav" XGM2 6650

// --- Baxter Stockman, jefe de la Scene 3 (30/09) ----------------------------
// "I'm invincible!" al aparecer el jefe. Llego ya MONO 8-bit 11025 Hz (2,0 s)
// pero bajo: pico 43%, rms 7,8%. Se recorto la cola de silencio y se le dio
// ganancia (x2,79) hasta rms 22% (la banda de voz), redondeando lo que pasaba
// del 80% con la rodilla suave (tanh, toca el 0,8% de las muestras), igual
// que dinne_turtle. Medido despues de convertir:
//   im_invinsible_baxter    1,947s  pico 96,6%  rms 22,0%  21.469 B
// El original queda al lado como im_invinsible_baxter_orig.wav.
// WAV im_invinsible_baxter_vo "audio/im_invinsible_baxter.wav" XGM2

// --- SFX del arcade (16/10) -------------------------------------------------
// Llegaron ESTEREO 8-bit 44100/48000 Hz con los nombres del rip ("1 up.wav",
// "hydrant.wav", ...; quedan tal cual en audio/). tools/gen_sfx_arcade.py los
// pasa a la convencion de la casa (MONO, 8-bit, 11025 Hz, puntas recortadas,
// pico 96,9%; los que quedaban por debajo de rms 18% suben hasta ahi con la
// rodilla tanh) y los deja en audio/gen/. Medidos despues de convertir:
//   hit_foot        0,302s  rms 30,9%   golpe a un foot soldier / rock soldier
//   mouser_hit      0,272s  rms 26,3%   golpe a un Mouser o al robot del latigo
//   shuriken_throw  0,135s  rms 18,0%   un foot tira shuriken o cuchillo
//   window_broken   0,736s  rms 22,3%   foot entrando por una ventana (2-1)
//   hydrant         1,355s  rms 17,9%   se rompe un hidrante (2-1)
//   car             1,829s  rms 33,5%   sale el auto del garage (4-1)
//   one_up          1,039s  rms 24,2%   vida extra
//   pause           0,640s  rms 22,1%   pausa
// Canales: los golpes en CH2 (como hit_turtles), el resto en CH3. CH1 sigue
// reservado a las voces.
WAV hit_foot_sfx       "audio/6650/gen/hit_foot.wav" XGM2 6650
WAV mouser_hit_sfx     "audio/6650/gen/mouser_hit.wav" XGM2 6650
// (23/10) Golpe a BAXTER (la mosca, Scene 3): baxter_hit.wav del artista, ya
// MONO 8-bit 11025 Hz; puntas recortadas y normalizado a pico 96,9% (llego
// con pico 58%). El original queda al lado como baxter_hit_orig.wav.
WAV baxter_hit_sfx     "audio/6650/baxter_hit.wav" XGM2 6650
WAV shuriken_throw_sfx "audio/6650/gen/shuriken_throw.wav" XGM2 6650
WAV window_broken_sfx  "audio/6650/gen/window_broken.wav" XGM2 6650
WAV hydrant_sfx        "audio/6650/gen/hydrant.wav" XGM2 6650
WAV car_sfx            "audio/6650/gen/car.wav" XGM2 6650
WAV one_up_sfx         "audio/6650/gen/one_up.wav" XGM2 6650
WAV pause_sfx          "audio/6650/gen/pause.wav" XGM2 6650
//   jump_attack     0,116s  rms 40,2%   patada voladora de la tortuga
WAV jump_attack_sfx    "audio/6650/gen/jump_attack.wav" XGM2 6650
//   special_attack  0,329s  rms 34,4%   el especial de la tortuga (reemplaza al
//                                       "Attack!!" de attack_turtles ahi)
WAV special_attack_sfx "audio/6650/gen/special_attack.wav" XGM2 6650
// (16/10) Robot del latigo, pizza y electrocucion de las tortugas:
//   rodney_lasso    0,440s  rms 29,3%   el robot tira el latigo
//   rodney_shoot    0,456s  rms 21,1%   el robot dispara el laser
//   rodney_stop     0,161s  rms 34,4%   el robot frena (derrape) y se da vuelta
//   get_pizza       0,620s  rms 27,4%   una tortuga agarra la pizza
//   shock_pain      0,693s  rms 28,3%   una tortuga electrocutada (CH1: es voz)
WAV rodney_lasso_sfx   "audio/6650/gen/rodney_lasso.wav" XGM2 6650
WAV rodney_shoot_sfx   "audio/6650/gen/rodney_shoot.wav" XGM2 6650
WAV rodney_stop_sfx    "audio/6650/gen/rodney_stop.wav" XGM2 6650
WAV get_pizza_sfx      "audio/6650/gen/get_pizza.wav" XGM2 6650
WAV shock_pain_vo      "audio/6650/gen/shock_pain.wav" XGM2 6650

// forzando ejecucion

// (18/10) Auto de los foot soldiers de la autopista (5-1) y ascensor del
// Technodrome (8-1). Llegaron MONO 8-bit 16000 Hz y bajos (pico 25% / 9%,
// rms 6% / 3%); tools/gen_sfx_arcade.py los pasa a la convencion de la casa
// en audio/gen/. Los originales quedan en audio/.
//   car_highway          0,960s  rms 24,0%   pasa el auto (CH3, una vez)
//   elevator_tecnodrome  8,015s  rms 32,9%   en LOOP (CH2) mientras baja el
//                                            ascensor; se corta al llegar
WAV car_highway_sfx         "audio/6650/gen/car_highway.wav" XGM2 6650
// (18/10) A MEDIA FRECUENCIA (6650 Hz, se toca con halfRate en level8_1.c):
// son 8 s en loop y a 13300 ocupaban 106 KB; con eso la ROM pasaba los 4 MB.
// (18/10) LOOP CORTO: 4 ciclos (1,617 s) de elevator_tecnodrome.wav con volumen
// parejo y el empalme fundido (tools/gen_elevator_loop.py), ya a 6650 Hz y con
// 42 x 256 muestras exactas (sin relleno de silencio en el loop). 10,5 KB en
// vez de 53,5 KB.
WAV elevator_tecnodrome_sfx "audio/gen/elevator_tecnodrome_loop.wav" XGM2 6650

// --- Tora, el jefe de Central Park (24/10) ----------------------------------
// "Time to put you on ice!" cuando entra a escena (con su globo). Llego MONO
// 8-bit 8000 Hz de 1,32 s (pico 73,4%, rms 10,3%). Convencion de la casa:
// puntas recortadas (10 ms antes / 20 ms despues de la voz), a 11025 Hz y
// ganancia x2,09 hasta rms 22% con la rodilla suave tanh desde el 80% (toca
// el 0,8%). Medido despues de convertir:
//   tora                    1,144s  pico 96,9%  rms 22,0%  12.613 B
// El original queda como tora_orig.wav.
WAV tora_vo "audio/6650/tora.wav" XGM2 6650
// (27/10) Cutscene de Splinter despues de la fabrica (SCENE_CUT_SPLINTER): "Thank you, my turtles!".
// WAV mono 8-bit a 8000 Hz (1,24 s): rescomp lo resamplea al rate de XGM2 como a los de 11025 Hz,
// la duracion no cambia. Ver cut_scenes.c.
WAV thank_you_vo "audio/6650/thank_you_my_turtles.wav" XGM2 6650
// (27/10) Baxter al aparecer: "Yippee-I-Ay!". WAV mono 8-bit a 8000 Hz (1,2 s).
WAV yippee_vo "audio/6650/yeepi_a_yey.wav" XGM2 6650
// (28/10) Cutscene despues de The Sewer (Rocksteady): "Come and get'em" y "shell-brain".
// WAV mono 8-bit a 8000 Hz (0,76 s y 0,80 s). Ver cut_scenes.c.
WAV come_get_vo   "audio/6650/come_and_get_them.wav" XGM2 6650
WAV shell_brain_vo "audio/6650/shell_brain.wav" XGM2 6650

// --- (30/10) Garage: la tortuga en la camioneta ------------------------------
// "Let's move it" (src/garage_van.c, junto con el globo lets_move_bubble).
// Llego MONO 8-bit 8000 Hz de 0,76 s, ya fuerte (rms 28%): se le bajo la
// ganancia a rms 22% como al resto de los VO (pico 79%), 6650 Hz.
//   lets_move_vo           0,760s  pico 79%  rms 22,0%  5.120 B
WAV lets_move_vo      "audio/6650/lets_move.wav" XGM2 6650
// (31/10) "I owe you one": April besa a la tortuga (cut_scenes.c, showCutKiss). Original 8 kHz,
// ganancia x0,794 hasta rms 22% (el nivel de los otros VO).
//   i_owe_you_vo           0,880s  pico 79%  rms 22,0%  5.852 B
WAV i_owe_you_vo      "audio/6650/i_owe_you_one.wav" XGM2 6650
