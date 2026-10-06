// =============================================================================
// enemies.res  Sprites de enemigos
// =============================================================================
// Foot Soldier (morado): sheet de 832x1440 = grilla 13x18 de frames de 64x80px
// (8x10 tiles). El arte mira a la DERECHA (enemy.c aplica HFlip cuando dir == -1).
// Animaciones (filas, en orden de Aseprite):
//
//   [0]  idle quieto (1f)
//   [1]  walk (5f)
//   [2]  kick patada con salto (4f)
//   [3]  uppercut (2f)
//   [4]  walk up (4f)
//   [5]  explode muerte (6f)
//   [6]  punch front directo (2f)
//   [7]  break door (4f)
//   [8]  hit 1 (1f)
//   [9]  hit 2 (1f)
//   [10] hit 3 (1f)
//   [11] giro (2f: arranca mirando a la derecha, termina mirando a la izquierda)
//   [12] guard espera (3f: el frame 2 se mantiene mas tiempo; f1/f3 = entrada/salida)
//   [13] stance, otra postura de espera (3f)
//   [14] grab agarre por la espalda (ATENCION: si esta fila queda 100%
//        transparente, rescomp la elimina y todos los indices siguientes
//        se corren UNO: 14 pasa a ser la voltereta y 15 queda fuera de rango.
//        Hoy tiene un punto placeholder de 4px hasta dibujar la pose).
//   [15] voltereta (7f, avanza mas en X que el walk)
//   [16] TIRAR DINAMITA (13f, 18/09). Se asoma por la escalera (f0-f2), se
//        planta (f3-f8), tira el cartucho arriba y adelante (f9-f10) y se
//        recompone (f11-f12). El TNT se spawnea en el frame 10.
//   [17] salir por la ALCANTARILLA (6f, 18/09): levanta la tapa y sale.
//        Declarada pero TODAVIA SIN USAR (falta la tapa voladora).
//
// IMPORTANTE: el ultimo parametro es el TIEMPO DE FRAME en 1/60s. Si se omite,
// rescomp usa 0 = SIN animacion automatica (el sprite queda clavado en el
// primer frame). Con 8: kick = 4x8 = 32 y punch = 2x8 = 16  deben coincidir
// con ENEMY_KICK_TIME / ENEMY_PUNCH_TIME de enemy.h.
// =============================================================================

// (19/09) SE DECLARA EL SHEET ORIGINAL, DIRECTO. Antes habia un PNG
// intermedio (foot_soldier_purple_gen.png) que generaba tools/gen_foot_purple.py
// y era lo que compilaba; el sheet original no se tocaba. Salio mal: arreglo
// un frame del walk, recompilo y seguia saliendo roto, porque el intermedio
// habia quedado viejo. Se elimino el paso: ahora lo que el edita es lo que
// compila.
//
// !! NO DEJAR LA FILA 14 (el agarre por la espalda) 100% TRANSPARENTE !!
// rescomp BORRA las filas vacias, y si esa se va, las de abajo se corren una:
// ENEMY_ANIM_VOLTERETA (15) terminaria reproduciendo la dinamita. Como todavia
// no esta dibujada, el sheet lleva un punto de 2x2 px (indice 9) en x 20..21,
// y 59..60 del frame 0 de esa fila. No se ve nunca: esa animacion no se
// reproduce. El dia que se dibuje el agarre, el arte lo tapa y listo.
// Para chequearlo: en out/symbol.txt tienen que estar foot_soldier_animation0
// hasta foot_soldier_animation17, SIN huecos.
// (24/09) SHEET NUEVO: foot_soldier_purple_13x13.png. Misma grilla (832x1440,
// 13x18 frames de 64x80) pero repintado sobre la PALETA UNICA de enemigos, la
// que tambien usan el naranja y el fuego. tools/gen_enemy_palette.py le dejo el
// indice 0 libre (transparente) y los 15 colores reales en 1..15.
SPRITE foot_soldier "sprites/foot_soldier_purple_13x13.png" 8 10 FAST 8

// Foot Soldier Naranja: sheet de 416x936 = grilla 4x9 de frames de 104x104px.
// Misma grilla que las tortugas/regular. Usa PAL2 (la paleta unica de enemigos).
// Animaciones (filas):
//   [0] Idle (1f) | [1] Walk (4f) | [2] Walk up (4f) | [3] Shuriken throw (3f)
//   [4] Punch front (2f) | [5] Uppercut (3f) | [6] Explode (4f)
//   [7] Hit received (1f) | [8] Jump kick (4f)
// El shuriken se spawnea en el frame 1 de la anim [3] (timer == 16).
// (24/09) SHEET NUEVO: Foot_Soldier_Orange_new.png, misma grilla que el viejo.
// Comparte la paleta EXACTA del morado, asi que ya no gasta una linea propia:
// se dibuja con PAL2 y PAL3 queda para el blanco (ver scenes.c).
SPRITE foot_soldier_orange "sprites/Foot_Soldier_Orange_new.png" 13 13 FAST 8

// Foot Soldier BLANCO (espada larga): sheet de 832x1040 = grilla 8x10 de
// frames de 104x104px. Misma grilla que el naranja y las tortugas.
// Comparte PAL3 con el naranja: los indices 1..15 de las dos paletas son
// IDENTICOS (verificado pixel a pixel), solo difiere el indice 0, que es el
// transparente y nunca se dibuja. Por eso NO lleva PALETTE propia -- no hay
// una 5ta linea de paleta libre en el nivel.
// Animaciones (filas):
//   [0] Idle (1f) | [1] Walk (5f) | [2] Walk up (8f)
//   [3] Espadazo LARGO (3f) | [4] Espadazo medio (3f) | [5] Espadazo medio, variante (3f)
//   [6] Salto (5f: despegue + giro tipo bolita) | [7] Espadazo cayendo desde el aire (2f)
//   [8] Golpe recibido (2f) | [9] Muerte: cae y explota (4f)
// El arte mira a la DERECHA -> HFlip cuando dir == -1, igual que los otros dos.
//
// OJO: el PNG que se declara es foot_soldier_white_gen.png, GENERADO por
// tools/gen_foot_white.py a partir de foot_soldier_white_sword.png (el rip
// original, que NO se toca). El generador repinta el parche del
// piso que traia el rip abajo de las botas: venia con el indice 2 (rojo
// 219,36,0) y quedaba como un charco; pasa al indice 10 (146,109,146), lo mas
// parecido al marron de la sombra del morado que hay en PAL3. Si se
// actualiza el sheet, hay que volver a correr el generador.
SPRITE foot_soldier_white "sprites/foot_soldier_white_gen.png" 13 13 FAST 8

// Shuriken: proyectil del foot soldier naranja. 16x16px = 2x2 tiles.
// Misma paleta que el naranja (PAL3). Se crea/destruye en runtime.
// (27/09) TRES frames que giran en loop (time 4 = 15 fps): la tira
// shuriken_anim.png la arma tools/gen_shuriken.py con shuriken1..3.png.
SPRITE shuriken_sprite "sprites/shuriken_anim.png" 2 2 FAST 4

// Foot Soldier AMARILLO, el del boomerang (29/09): frames de 64x80 (8x10
// tiles, la grilla del morado, pies en el borde de abajo). Mismo orden de
// indices que la paleta unica de enemigos: se dibuja con PAL2 y con la paleta
// del MORADO (initEnemySpawn no carga la de este PNG).
// Animaciones (filas):
//   [0] Idle (1f) | [1] Lanzar el boomerang (9f; sale en el frame 6)
//   [2] Guardia (1f) | [3] Caminar (5f) | [4] Caminar hacia arriba (4f)
//   [5] Muerte (6f)
//   [6] [7] [8] Golpe recibido (1f cada una, se alternan en cada golpe)
// Va a 6 ticks por frame (ver YELLOW_TICKS en enemy.h).
SPRITE foot_soldier_yellow "sprites/Foot_Soldier_Yellow_boomerang.png" 8 10 FAST 6

// El boomerang: 32x32 = 4x4 tiles, PAL2. [0] girando (8f, en loop)
// [1] le pega a una tortuga (2f) | [2] roto por un golpe de tortuga (3f).
SPRITE boomerang_sprite "sprites/boomerang.png" 4 4 FAST 4

// --- Foot soldiers con ARMA (06/10): FUSIL, MARTILLO y LANZA ----------------
// Hojas PROVISORIAS: las arma tools/gen_foot_weapons.py desde foot_gun.png,
// foot_hammer.png y foot_spear.png (frames sueltos, mirando a la izquierda).
// Si se toca alguna de esas tres, hay que volver a correr el script. Cuando
// lleguen las definitivas en grilla se declaran directo aca.
// Grilla pareja, arte mirando a la DERECHA, cuerpo centrado y pies en el
// borde de abajo. Paleta unica de enemigos (PAL2). Filas: GUN_ANIM_*,
// HAMMER_ANIM_* y SPEAR_ANIM_* de enemy.h.
//   foot_gun     120x72  [0] idle [1] walk 7f [2] walk up 8f [3] rafaga 6f
//                        [4] culatazo 5f [5] golpe [6] cae 3f [7] burla 6f
//   foot_hammer  104x96  [0] idle [1] walk 8f [2] walk up 8f
//                        [3] martillazo 6f [4] golpe 3f [5] cae 4f
//   foot_spear   136x104 [0] idle [1] walk 8f [2] walk up 8f [3] estocada 5f
//                        [4] tira la lanza 6f [5] golpe [6] cae 4f
SPRITE foot_gun "sprites/foot_gun_gen.png" 15 9 FAST 6
SPRITE foot_hammer "sprites/foot_hammer_gen.png" 13 12 FAST 6
SPRITE foot_spear "sprites/foot_spear_gen.png" 17 13 FAST 6
// Proyectiles: la bala del fusil ([0] en vuelo, [1] chispas en el piso, 6f)
// en celdas de 16x24, y la lanza tirada (96x8).
SPRITE foot_gun_fx "sprites/foot_gun_fx.png" 2 3 FAST 4
SPRITE foot_spear_fx "sprites/foot_spear_fx.png" 12 1 FAST 0

// --- Dinamita del foot soldier morado (18/09) --------------------------------
// TNT: el cartucho que tira con la anim [16]. 192x24 = 8 frames de 24x24px
// (3x3 tiles) girando sobre si mismo. Frame time 3 (1/20 s): a 8 frames da una
// vuelta cada 24/60 s, que es lo que se ve en el arcade.
//
// EXPLOSION: 448x64 = 7 frames de 64x64px (8x8 tiles). La secuencia es la del
// arcade: destello de 4 puntas -> bola chica -> anillo -> bola blanca grande ->
// hongo de fuego (x2) -> restos que se dispersan. Frame time 6 (1/10 s) = 42
// frames en total, 0,7 s.
//
// LAS DOS USAN LA PALETA DEL MORADO (PAL2): verificado indice por indice, las
// tres paletas son identicas. Por eso no llevan PALETTE propia.
// Se crean y se liberan en runtime (una sola vez por partida, en la escalera
// del 1-1), asi que su VRAM no esta reservada todo el nivel.
SPRITE tnt_sprite "sprites/tnt.png" 3 3 FAST 3
SPRITE explosion_sprite "sprites/explosion.png" 8 8 FAST 6

// --- TAPA de la alcantarilla (19/09) ---------------------------------------
// 32x24 = UN solo frame (4x3 tiles). Es el proyectil que tira el morado que
// sale por la boca de tormenta del 2-1 (anim 17 del sheet). Tambien se usa
// QUIETA como "tapa cerrada" sobre cada boca de tormenta mientras el soldier
// todavia no salio: son 12 tiles contra los 80 que costaria tener ahi un
// foot_soldier congelado en el frame 0, y el dibujo es practicamente el mismo.
//
// Tampoco lleva PALETTE propia: comparte la del morado (PAL2), igual que el
// tnt y la explosion. Verificado indice por indice contra
// foot_soldier_16colors_purple.png (usa los indices 0, 1, 4, 10 y 14).
//
// Frame time 0 = sin animacion: el sprite no gira. Si algun dia se quiere que
// de vueltas en el aire hay que traer un PNG con varios frames y subir el
// primer numero.
SPRITE lid_sprite "sprites/tapa_voladora.png" 4 3 FAST 0

// --- BEBOP: jefe del 2-1 (24/09) --------------------------------------------
// El sheet que compila es bebop_boss_gen.png, GENERADO por
// tools/gen_bebop_sheet.py a partir del rip original (03/10: "Arcade -
// Teenage Mutant Ninja Turtles - Bosses - Bebop.png", con la PALETA
// COMPARTIDA con Rocksteady y April; antes Bebop_Boss.png), con
// los frames sueltos y de distinto ancho). El generador los re-pega en una
// grilla de 112x120 px (14x15 tiles) anclados por los PIES: el borde inferior
// de la celda es la linea de pies y la X sale del centro de las PIERNAS, no
// del bounding box -- si no, el cuerpo se corre solo cada vez que estira el
// arma. Si se actualiza el rip, hay que volver a correr el generador.
//
// Animaciones (filas):
//   [0] idle (3f) | [1] vitoreo, estira los brazos (3f) | [2] camina (6f)
//   [3] embestida (4f) | [4] uppercut (5f: 0-2 arranque, 3-4 el golpe)
//   [5] dispara (5f: 0-1 parado, 2-4 agachado)
//   [6] golpes (6f: 0-1 recibe, 2 tirado, 3-5 se levanta)
// time 8, pero casi todas se manejan a mano desde bebop.c (SPR_setFrame).
// Paleta PROPIA: en el 2-1 PAL3 esta libre (no hay foot soldier blanco), asi
// que el jefe se la queda entera. (03/10) Es la paleta compartida de los
// jefes: en el garage Rocksteady tambien se dibuja en PAL3.
SPRITE bebop_boss "sprites/bebop_boss_gen.png" 14 15 FAST 8

// El disparo: (03/10, como el arcade) cada aro es un proyectil aparte que
// crece mientras vuela: 5 frames, frame k = el aro k solo, en una celda de
// 16x40 (2x5 tiles). bebop_shot_gen.png sale del mismo generador (los aros
// vienen en el mismo rip, a la derecha de la fila del disparo), con la paleta
// del jefe, asi que NO lleva PALETTE propia: se dibuja con PAL3.
SPRITE bebop_shot_spr "sprites/bebop_shot_gen.png" 2 5 FAST 0
