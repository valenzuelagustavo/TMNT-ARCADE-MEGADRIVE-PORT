# Diario de desarrollo — TMNT: The Arcade Game (port para Sega Mega Drive)

**Desarrollo:** Gustavo Valenzuela · **Herramientas:** SGDK (C), Aseprite, VS Code + Genesis Code, emuladores Gens/BlastEm · **Sprites:** ripeos del arcade original por Napalm

> Este diario fue reconstruido retroactivamente el 19/07/2026 a partir del
> historial de git, las fechas de los assets del repositorio y las notas de
> las sesiones de trabajo. A partir de acá, la idea es mantenerlo al día
> agregando una entrada por sesión de trabajo.

---

## Febrero 2025 — Los primeros experimentos

Antes de que existiera el repo, el proyecto arrancó como una serie de
prototipos sueltos en SGDK. De esta época sobreviven los assets más viejos:
el logo de TMNT y la pantalla de selección de personaje (retratos en escala
de grises que se "encienden" al seleccionar, el cursor con forma de
tortuga, la sheet de caras del HUD), la música de selección convertida a
VGM desde el arcade, y las primeras pruebas de concepto del nivel 1:
`bg_test.png` y `firetest.png` — ya desde el principio la idea fue que la
Escena 1 (el departamento en llamas donde está atrapada April) tuviera el
fuego animado en primer plano como protagonista.

## Abril–Mayo 2026 — Se retoma: intro y arquitectura de escenas

Después de una pausa larga, el proyecto se retomó con dos frentes:

- **Intro estilo arcade:** el logo de SEGA no podía ser un logo estático —
  Rocksteady entra corriendo y lo choca (sprites de abril de 2026), con
  efecto de sonido de golpe y música de intro en VGM.
- **Máquina de estados de escenas** (`scenes.h`, mayo 2026): cada pantalla
  del juego es una función `showXxx()` que devuelve el `SceneId` siguiente,
  y `main.c` es un simple switch que va encadenando escenas. Esta decisión
  temprana resultó clave: agregar pantallas nuevas después fue siempre
  trivial.

También de esta época: la música del nivel 1 (`fire_v3.vgm`) y la
configuración del emulador Gens para probar.

## 25 de junio de 2026 — Nace el repositorio

Primer commit y README. El proyecto se formaliza: estructura estándar de
SGDK (`src/`, `res/`, `out/`), licencia de proyecto fan no comercial, y
créditos a Konami, a Stéphane Dallongeville (SGDK) y a la comunidad de
preservación.

## 26–27 de junio — Selección de personaje y módulo del jugador

- La selección de personaje pasa de ser una maqueta a funcionar de verdad:
  el sprite de la tortuga elegida se muestra en pantalla (commit del 26/06).
- **Refactor importante** (27/06): el código del jugador se separa en un
  módulo propio (`player.c/h`) con máquina de estados para las animaciones
  (IDLE, WALKING, ATTACKING, JUMPING, HURT, GRABBED). El diseño es
  multi-instancia desde el día uno — todas las funciones reciben un
  `Player*` — pensando en el modo de 2 jugadores.

## 30 de junio — Los assets definitivos

Día grande de arte: las cuatro tortugas quedan en spritesheets de grilla
13×13 tiles (frames de 104×104 px, 18 animaciones cada una: idle, patada,
combo de 3 golpes, salto, patada en salto, caminatas, especial, hits de
frente y de espalda, levantarse, agarrado) y el fondo completo del nivel 1
queda armado: **1376×224 px**, más ancho que cualquier plano que la Mega
Drive pueda dibujar. Ese ancho forzó una de las técnicas centrales del
proyecto (ver 15/07).

## 15 de julio — Fuente arcade y streaming del fondo

- **Fuente del arcade** ripeada y adaptada (ASCII 32..126) para el título
  del nivel: "SCENE 1 — FIRE! WE GOTTA GET APRIL OUT!!" apareciendo letra
  por letra (efecto typewriter con skip por START). Lección aprendida: la
  fuente se exporta con `TILESET ... NONE NONE` porque la deduplicación de
  rescomp rompe el mapeo 1:1 entre caracter ASCII y tile.
- **Streaming de columnas del fondo:** como el nivel (1376 px) no entra en
  ningún plano, el tileset completo (~495 tiles únicos) se carga una sola
  vez a VRAM y el plano BG_B funciona como ventana circular de 64 columnas:
  a medida que la cámara avanza se dibujan columnas nuevas por el borde
  derecho pisando las que ya salieron por el izquierdo. Como el beat-em-up
  nunca retrocede, solo hay que revelar hacia adelante. El tilemap va sin
  comprimir (`NONE`) para poder indexarlo directo desde ROM.

Además: selección de 1 o 2 jugadores, cámara con dead-zone, XGM2 como
driver de audio (permite control de volumen — la música del nivel saturaba
y se bajó al 40%).

## 16–17 de julio — Primeros enemigos

Primer foot soldier (sheet provisional de 7×8 tiles, solo idle y caminata)
con IA básica de patrulla/persecución/ataque, spawns por trigger de cámara
y el **flash blanco al recibir golpe**: en vez de parpadear la visibilidad
(que se notaba poco), el sprite cambia su atributo de paleta a una línea
PAL3 cargada toda en blanco — cero DMA por golpe. También se fijó el mapa
de paletas del nivel: PAL0 fondo, PAL1 tortugas (las 4 comparten paleta
unificada), PAL2 enemigos, PAL3 flash.

## 18 de julio — El fuego, el sheet nuevo del foot soldier y la VRAM

Sesión intensa. Entra el spritesheet definitivo del foot soldier (grilla
5×5 de 104×104, la misma que las tortugas: idle, walk, kick con salto,
uppercut, walk_up) y el fuego del primer plano. Tres batallas técnicas:

1. **El fuego por scroll no entraba en VRAM.** El plan original era el
   truco clásico: dibujar la tira de 8 frames (512 px = ancho exacto del
   plano circular) y correr el scroll de BG_A de a -64 px para animar sin
   costo. Pero al medir el asset real: ~400 tiles únicos de fuego + ~495
   del fondo + ~540 de sprites (2 tortugas + 4 foot soldiers de 104×104)
   = ~1550 tiles sobre ~1400 disponibles. No entraba ni en modo 1 jugador.
   **Solución final: animación por streaming de tiles** — un solo frame
   (64 tiles) vive en VRAM, el tilemap lo repite a lo ancho de la pantalla,
   y cada 8 frames de juego se pisa con el frame siguiente vía cola DMA
   (2 KB por paso, nada para el presupuesto de vblank). Bonus: todas las
   celdas de fuego quedan en fase y el scroll de BG_A queda libre para el
   HUD. La tira se reorganizó en vertical (`fire_strip.png`, 64×512) para
   que los 64 tiles de cada frame queden contiguos en ROM.
2. **El build que "no cambiaba nada":** los fuentes nuevos no estaban donde
   el makefile los tomaba y el emulador relanzaba la ROM vieja. Y al
   arreglarlo apareció un bug fantasma: rescomp (Java) lee los `.res` con
   charset Cp1252, y una "Í" mayúscula en un comentario UTF-8 (byte 0x8D,
   indefinido en Cp1252) tiraba `Input length = 1`. **Regla desde entonces:
   comentarios de los `.res` siempre en ASCII.**
3. **Presupuesto de sprites:** el default de `SPR_init()` (420 tiles) no
   alcanza para 2 tortugas + 4 foot soldiers grandes → `SPR_initEx(600)`.
   De acá salió también el tope de diseño: máximo 4 foot soldiers
   simultáneos en pantalla.

Con el sheet nuevo, la IA se completó: movimiento vertical para alinearse
en profundidad con el jugador, ataques kick (con lunge de ~30 px) y
uppercut elegidos al azar, walk_up cuando sube, flip según dirección real.

## 18–19 de julio — De demo técnica a juego jugable

Tanda grande de game feel, en fases:

- **Daño enemigo → tortuga (Fase 1):** hasta acá los enemigos "atacaban"
  sin colisión. Se implementó hitbox activa solo durante la ventana real
  del golpe (el lunge del kick, el tramo medio del uppercut), un golpe por
  swing, y la reacción del jugador: animación correcta según de dónde vino
  (HIT_1/HIT_2 alternados de frente, HIT_BEHIND por la espalda), knockback
  de ~20 px, 45 frames de invulnerabilidad con parpadeo, y esquive aéreo
  (saltando no te pegan).
- **Agresividad (Fase 2):** los enemigos se encimaban y atacaban sin pausa.
  Ahora cada uno tiene cooldown personal entre ataques (60-91 frames, con
  azar para que no sean un metrónomo), hay cupo global de 2 atacantes
  simultáneos (el resto rodea a distancia en un "anillo de espera" de
  ~72 px, el circling clásico del género), distancia de frenado de 36 px y
  separación de a pares para que no se apilen entre ellos.
- **Targeting en 2P (Fase 3):** el bug de que "ignoran al player 2" venía
  de elegir al jugador más cercano cada frame — el target oscilaba y en la
  práctica quedaban clavados en P1. Ahora cada enemigo tiene un target
  asignado al spawnear (al jugador con menos enemigos encima) y re-evalúa
  cada 32 frames con histéresis de 48 px.
- **Hitbox de las tortugas y combos:** pegarle a los enemigos era casi
  imposible por dos bugs: la ventana de golpe se medía desde el borde del
  frame (pegaba "arriba" del personaje, no adelante, justo donde los
  enemigos ya no se paran) y el combo B-B-B solo encadenaba si apretabas B
  en el frame exacto de fin de animación (ventana de 1/60 s). Solución:
  hitbox medida desde el centro con 64 px de alcance frontal (más que el
  rango de ataque enemigo: le pegás antes de que te pegue) y tolerancia
  simétrica en profundidad; y para el combo, buffer de input durante todo
  el swing + ventana de enlace de 20 frames. De paso apareció un regalo:
  la patada en salto no golpeaba (nunca se chequeaba) — ahora sí.
- **Cámara:** el scroll ahora arranca cuando el personaje pasa apenas la
  mitad de la pantalla (dead-zone 120), y en 2P la cámara —que la mueve el
  jugador más adelantado— se topea antes de dejar al rezagado fuera de
  pantalla; el adelantado choca contra el borde visible en vez de salirse.
- **Spawner por oleadas:** en vez de un enemigo por trigger, cada punto
  del nivel manda una oleada: 3 en el primero (2 de frente + 1 por la
  espalda) y 4 en el resto (2 y 2), con lanes de profundidad variadas para
  que no entren en fila india. Nacen fuera de pantalla por ambos flancos,
  ya persiguiendo. Si no hay cupo, quedan pendientes y gotean a medida que
  el jugador libera lugar. Total del nivel: 23 foot soldiers.
- **Pantalla de créditos SGDK:** se habilitó la escena reservada, bilingüe
  ES/EN con la fuente arcade: el juego fue creado con SGDK (obra de
  Stéphane Dallongeville), gracias a Napalm por el ripeo de los sprites,
  desarrollo de Gustavo Valenzuela.
- **Marcos del HUD:** `hud_1p`/`hud_2p` (72×32) en la franja superior de
  32 px que el fondo deja libre, dibujados en BG_A con prioridad alta (por
  encima de la acción, estilo arcade) y compartiendo la paleta de las
  tortugas para no gastar línea. Falta llenarlos: vidas, puntos y barra.

## 19 de julio (tarde) — Recalibración del salto y el especial

Sesión de game feel sobre el control de las tortugas:

- **Salto por fases:** la animación del salto ya no corre sola — se apaga la
  auto-animación del sprite (`SPR_setAutoAnimation`) y los frames se
  eligen a mano según la física: frame 0 durante la subida, loop del frame
  1 al anteúltimo en el ápice y la caída, y el último frame aparece recién
  ~2 frames antes de tocar el suelo (predicho con la velocidad actual).
- **Jump kick con dos variantes:** golpe solo = frame 0, vuelo normal;
  golpe + dirección en X = frame 1 y la tortuga viaja sola con ímpetu a
  4 px/frame (el doble del control aéreo normal) con la trayectoria
  comprometida — llega bastante más lejos.
- **Botón A remapeado al ESPECIAL:** antes disparaba ANIM_KICK (que queda
  reservada); ahora A y B+C ejecutan el especial, que mata foot soldiers
  de un solo golpe (aplica el HP completo como daño). Pendiente: cuando
  exista el sistema de HP, usar el especial debe restar vida al jugador,
  como en el arcade.
- **Saltito visual del especial:** el arte del ESPECIAL ya se lee como un
  salto en el lugar, así que mientras dura la animación el sprite se
  dibuja `PLAYER_SPECIAL_LIFT` (8 px, constante ajustable en `player.h`)
  más arriba en pantalla. Es un offset puramente de RENDER: se aplica solo
  en el `SPR_setPosition` final de `updatePlayer()`, la `p->y` lógica
  (lane de profundidad, hitbox, Y-sorting) no se toca para nada.

## 19 de julio (noche) — Movilidad en el aire y piso más amplio

Ajuste fino de fidelidad al arcade, a partir de revisar el original:

- **Salto con movimiento en Y:** en el arcade, saltando la tortuga se puede
  seguir reposicionando también en profundidad (arriba/abajo), no solo en
  X. Antes acá el salto solo dejaba mover X porque la física del salto
  usaba directamente `p->y` para simular el arco vertical (crecía/decrecía
  con `jumpVel`+gravedad), y tocarlo con input hubiera roto esa cuenta.
  **Refactor:** se separó la altura del salto a un campo nuevo, `jumpZ`
  (offset puramente VISUAL que se resta al dibujar), dejando `p->y` libre
  para representar siempre la lane real de profundidad — igual en el aire
  que caminando. Efecto colateral bueno: el Y-sorting (`SPR_setDepth`) y el
  alcance del jump kick (`playerAttackHits`) quedaron más simples y más
  correctos, porque ya no hace falta ningún caso especial para el estado
  de salto (antes usaban `groundY` como parche).
- **Piso más ancho:** los límites de la lane (`BOUND_LANE_TOP/BOTTOM`,
  antes 150/192) se ampliaron 1 tile (8px) en cada extremo → 142/200.
  Se replicó el mismo ajuste en `ENEMY_LANE_TOP/BOTTOM` (`enemy.h`) para
  no dejar franjas de la vereda sin cobertura de la IA enemiga.

## 19 de julio (noche, cont.) — Pared diagonal del final del nivel

Comparando contra el arcade original apareció un bug de colisión: al final
del nivel hay un hueco de escalera/fire escape dibujado en el fondo en
PERSPECTIVA (diagonal), pero el límite de movimiento era una línea vertical
recta. Resultado: en las lanes de atrás (más cerca del fondo) el personaje
podía caminar "sobre" la pared dibujada, quedando visualmente parado en el
aire encima de la estructura diagonal.

**Solución:** se midió el borde sólido real directamente sobre
`bg01_completa.png` (script en Python, detectando dónde el color de piso
deja de ser piso) — dio un punto de referencia en cada extremo de la lane:
X≈1308 en la lane del fondo (Y=142) y X≈1352 en la lane del frente (Y=200).
Con esos dos puntos se interpola linealmente (`levelEndWallX` en
`player.c`, `enemyMaxX` en `enemy.c`) para obtener el tope de X real según
la profundidad de cada personaje, en vez de un límite fijo. Se aplicó
tanto al jugador (reemplaza el límite derecho en los clamps de caminar,
patada en salto y knockback) como a los foot soldiers (persecución,
lunge del kick y separación de grupo) — nadie puede ya cruzar la pared,
en ninguna lane.

## 21 de julio de 2026 — Contenido del HUD: barra de vida, vidas y puntaje

Se llenó el marco del HUD con sus tres indicadores, estilo arcade, sin
tocar el tamaño del marco: todo entra en el `hud_1p.png`/`hud_2p.png`
original (72x32), en las 2 filas de tiles de interior útil:

- **Distribución compacta (como el arcade):** fila superior = "1UP" (pintado
  en el arte) + PUNTAJE alineado a la derecha; fila inferior = VIDAS a la
  izquierda + BARRA a la derecha. Nada pisa el fondo del nivel: todo queda
  en la franja negra superior. (Un primer intento agrandó el marco a 72x48,
  pero quedó demasiado alto; se volvió al 72x32 achicando la barra.)
- **Barra de vida (`hp_bar.png`, 11 frames de 32x8):** frame 0 = 10 barras,
  frame 10 = 0 barras. El arte original era 32x16; se recortó por script a
  32x8 (una fila de tiles) aprovechando que los segmentos son columnas
  uniformes, para que quepa junto al puntaje en las 2 filas del marco.
  Comparte la paleta de las tortugas (PAL1). Se dibuja como **TILES en BG_A**
  (prioridad alta, igual que el marco), NO como sprite: no gasta presupuesto
  de `SPR_initEx` ni pelea con el layering sprite/plano. Un frame (4x1 = 4
  tiles) vive en VRAM por jugador y, al recibir un golpe, se pisa con el
  frame siguiente vía DMA — la misma técnica de streaming que el fuego. En
  `.res` va `NONE NONE` para indexar cada frame directo desde ROM
  (`frame N -> tile N*4`).
- **Vida / vidas / puntaje en el jugador:** `Player` ganó `health` (0..10,
  arranca lleno), `lives` (arranca en 3) y `score`. Cada golpe de un foot
  soldier resta una barra (`damagePlayer`); al vaciarse se pierde una vida y
  la barra se recarga. Matar un foot soldier suma 1 al puntaje del jugador
  que lo remató (se detecta la transición a `ENEMY_STATE_DEAD` en el bucle
  de colisiones de `scenes.c`).
- **Vidas y puntaje como TEXTO** (fuente por defecto, `VDP_drawText`) sobre
  BG_A. Se dibujan en **PAL3** aprovechando que la paleta "flash" es blanco
  puro en todos sus índices → texto blanco sin gastar una línea propia. El
  HUD cachea lo último dibujado y solo reescribe VRAM cuando algo cambia.
- **Knockout al perder una vida:** cuando se agota la barra, la tortuga entra
  en `STATE_KO` y muestra el último frame de `ANIM_HIT_BEHIND_2` (la pose
  tirada) durante `PLAYER_KO_FRAMES` (~1.2s) antes de revivir. Al revivir se
  recarga la barra y arranca la invulnerabilidad de respawn.
- **Parpadeo sólo al revivir:** se separó la invulnerabilidad "lógica"
  (`invincible`, sin efecto visual) del parpadeo (`blinkTimer`). Un golpe
  normal ya NO hace parpadear al sprite (queda visible durante sus i-frames);
  el parpadeo clásico quedó reservado para el respawn tras perder una vida.
- **Game over:** al llegar a 0 vidas se muestra la pose de knockeado y recién
  ahí se corta el nivel (`isPlayerGameOver` devuelve el flag `gameOver`, que
  se activa al final del KO).

## 21 de julio (cont.) — Ajustes del KO, escena de Game Over y bug de scroll

Tres correcciones tras probar el HUD y la muerte:

- **Frame exacto del KO:** la pose de tortuga tirada es el frame 11 (la "12a")
  de `ANIM_HIT_BEHIND_2`. Antes se reproducía la animación entera (loop off) y,
  como los 12 frames a FAST 7 tardan ~84 frames pero el KO dura 70, la tortuga
  revivía ANTES de llegar a la pose. Ahora se salta DIRECTO al frame 11 con la
  auto-animación apagada (`SPR_setAutoAnimation(FALSE)` + `SPR_setAnimAndFrame`)
  y se congela ahí; al revivir se reactiva la auto-animación.
- **Escena de Game Over:** nueva `SCENE_GAME_OVER` (`showGameOver` en
  `scenes.c`, caso en `main.c`). Muestra "GAME OVER" en blanco sobre negro
  (fuente por defecto, blanco puesto en el índice 15 de PAL0), espera ~4s o
  START, y reinicia desde el logo de SEGA. El nivel ahora sale a esta escena
  en vez de ir directo a SEGA.
- **Bug de scroll heredado:** al reiniciar tras un game over, el logo TMNT del
  menú aparecía corrido a la derecha. Causa: `clearScene()` limpiaba los planos
  pero NO reseteaba el scroll, y el nivel deja BG_B en `-cameraX`. Se agregó el
  reset de scroll H/V de ambos planos en `clearScene()`.

## 22 de julio — Voz de arranque, globo de diálogo y puertas que escupen enemigos

Sesión larga con tres frentes: darle voz al inicio del nivel, y convertir las
puertas del fondo en puntos de spawn.

**Voice over + globo "Attack!!" al arrancar el nivel.**

- El grito va como sample **PCM del driver XGM2**: recurso `WAV attack_vo` en
  `audio.res` (rescomp lo resamplea a 13.3 kHz y lo alinea a 256 bytes), disparado
  con `XGM2_playPCMEx` en el canal PCM 2 con prioridad 15 — así suena por encima
  de la música del nivel (canal 1) sin que ésta lo pise.
- **Lección a los golpes:** al principio no se escuchaba nada. No era el canal ni
  el código: el WAV venía grabado bajísimo (pico al 22%, RMS ~4.5% de la escala).
  En el DAC de 8 bits y con la música arriba, un sample flojo es lisa y llanamente
  inaudible. Se normalizó con compresión + makeup a ~24% RMS y apareció. **Regla
  nueva:** preparar los WAV (normalizar/comprimir) antes de meterlos, y verificar
  la AMPLITUD, no sólo el formato.
- El **globo de diálogo** (`attack_bubble`, 64x32) comparte la paleta de las
  tortugas (PAL1): el PNG ya venía indexado sobre ella, así que se dibuja con
  `TILE_ATTR(PAL1,...)` sin gastar una línea de paleta (mismo truco que el HUD y
  la barra de vida). Va en posición FIJA de pantalla, independiente del jugador y
  la cámara, con ciclo aparecer → fijo → parpadeo → desaparecer, todo por tiempo.
- Se dispara TODO apenas arranca el nivel (sin esperar). De paso: el jugador ahora
  nace a 5 tiles del borde izquierdo y el primer foot soldier ya queda spawneado y
  visible pegado al borde derecho, entrando hacia el player.

**Sheet ampliado del foot soldier + muerte con explosión.**

- El spritesheet pasó de 5 a **8 animaciones** (grilla 5x8, frames 104x104):
  además de idle/walk/patada/uppercut/walk-up, ahora hay **explosión** (5),
  **golpe de frente / directo** (6) y **rotura de puerta** (7). rescomp detecta
  las filas solo: no hubo que tocar `enemies.res` (el grid ya era `13 13`).
- **Muerte con explosión:** al morir, el foot soldier reproduce `ANIM_EXPLODE`
  (una vez, sin loop) en vez de quedarse en idle; se saltea el flash blanco en el
  golpe fatal para que se vean los colores de la explosión. Tacha un ítem viejo
  del backlog.
- El **directo** se sumó a la rotación de ataques: al atacar, el enemigo elige al
  azar entre uppercut, patada con salto y directo (misma duración/hitbox que el
  uppercut).

**Puertas como spawn points.**

- `door_lvl_1` (40x80) se dibuja sobre cada uno de los 3 huecos de puerta abierta
  del fondo (centros de mundo **429, 718, 846**, medidos sobre `bg01_completa.png`;
  los otros dos huecos, más anchos, son los ascensores). Comparte la paleta del
  **FONDO** (PAL0) reindexada — mismo truco de siempre, cero líneas de paleta.
- **Trigger por cercanía:** cuando el player pasa cerca, la puerta queda "armada"
  (aunque después se aleje); en cuanto hay cupo de activos (respeta
  `MAX_ACTIVE_ENEMIES`), se remueve el sprite de la puerta y aparece un foot
  soldier que la ROMPE con `ANIM_BREAK_DOOR` arrancando desde el 2do frame (el 1ro
  es la puerta cerrada, que ya mostraba el sprite). Al terminar la animación pasa a
  ser un enemigo normal. Cada puerta dispara una sola vez.
- Nuevo estado `ENEMY_STATE_SPAWNING`: sin IA ni colisión mientras rompe la puerta.
- **VRAM:** los sprites de puerta se crean/sueltan según visibilidad (no gastar
  tiles con puertas fuera de pantalla). Peor caso en 2 jugadores medido en ~544 de
  600 tiles de sprite: entra sin tocar el presupuesto de `SPR_initEx`.

## Estado al 22/07/2026 y próximos pasos

**Jugable hoy:** intro SEGA → créditos SGDK → selección de jugadores y de
tortuga → título de la Escena 1 → nivel 1 completo con scroll, fuego
animado, voice over + globo de diálogo al arrancar, oleadas de foot soldiers
con IA de grupo (que además ahora entran rompiendo las puertas del nivel y
explotan al morir), daño bidireccional, combos, HUD con vida/vidas/puntaje y
modo 2 jugadores cooperativo.

**Backlog:** alcance y poder de ataque por tortuga · el ESPECIAL debería
restar vida al usarlo (como en el arcade) · animación de muerte de la
tortuga · agarre (`STATE_GRABBED`) · afinar a ojo en emulador la posición del
globo y el calce del frame de rotura de puerta con el hueco · PAL3 va a
reasignarse (el flash de golpe y también el texto del HUD deberán mudarse o
reemplazarse).

**Reglas de la casa aprendidas a los golpes:**

- Comentarios de `.res` en ASCII puro (Cp1252 en rescomp).
- Medir tiles únicos ANTES de elegir técnica gráfica: la VRAM (~1400
  tiles) se agota entre fondo, efectos y sprites grandes mucho antes de lo
  que parece.
- La fuente arcade solo cubre ASCII: textos de pantalla sin acentos.
- Tilemaps que se indexan desde ROM: compresión `NONE`; tiles que se
  streamean: además sin dedup (`NONE NONE`).

## 26 de julio — Scroll del fuego, la bola de hierro y ajustes del robot

Sesión de pulido sobre el nivel 1: hacer que el fuego por fin scrollee, un
obstáculo nuevo (la esfera de metal que baja por la escalera) y dos arreglos al
robot del látigo. (Entre medio de esta entrada y la anterior el proyecto ya
sumó el **robot del látigo** como mini-jefe del final —máquina de estados propia
con patrulla, láser a distancia y agarre con electrocución— y la **cutscene
final** `SCENE_ENDING`; quedan mencionados acá al pasar, su desarrollo no está
documentado en detalle en este diario.)

**El fuego ahora scrollea con el mundo (parallax por tile).**

- Hasta acá el fuego estaba clavado a la pantalla: `BG_A` tenía el scroll H fijo
  en 0. En el arcade el fuego es parte del mundo y se desplaza al avanzar. Pero
  `BG_A` también lleva el HUD (franja superior), así que no se puede scrollear el
  plano entero sin arrastrar el HUD.
- **Solución:** scroll horizontal **por tile** (`VDP_setScrollingMode(HSCROLL_TILE,
  VSCROLL_PLANE)`). Las filas del HUD (0-3) quedan en scroll 0 y las 8 filas de la
  banda del fuego se desplazan solas. Como el modo de scroll H es **global a los
  dos planos**, `bgUpdate()` ahora también alimenta la tabla completa de `BG_B`
  (28 filas al mismo `-cameraX`) en vez de un solo `VDP_setHorizontalScroll`.
- La celda del fuego (64px) se repite en todo el plano circular (512px = 8×64),
  así que el scroll envuelve sin costura. Parallax tuneable con `FIRE_SCROLL_NUM/
  DEN` (1/2 = deriva suave, 1/1 = anclado al mundo). `clearScene()` vuelve a
  `HSCROLL_PLANE` para no romper el scroll de las otras escenas.

**Bola de hierro: obstáculo que baja rebotando por la escalera.**

- Sprite nuevo `iron_ball` (32×32, 2 frames girando) que aparece cada ~6s en lo
  alto de la escalera del nivel y baja rebotando en diagonal hasta salir por
  abajo. Si toca a una tortuga le resta 1 barra (vía `damagePlayer`, con sus
  i-frames → un solo golpe por pasada); si toca a un foot soldier, lo aplasta.
- **Coordenadas** como el resto del motor: `x` de mundo (anclada al mundo,
  scrollea con la cámara), `y` = línea de contacto (misma escala que la lane/
  pies) que desciende, y `z` = altura del rebote (offset visual sobre un
  "escalón" en z=0, mismo concepto que el `jumpZ` del jugador). Colisión por
  profundidad (`|feetY - y|`) + X, ignorando `z`. Profundidad del Y-sorting = `y`.
- **La paleta fue el detalle fino.** El PNG venía indexado en grises genéricos
  (índices 11-15), pero en la paleta REAL de las tortugas (PAL1) esos slots son
  lavanda/rojo/magenta → la bola habría salido de colores. PAL1 sí tiene grises,
  pero en los índices 1/4/10/13. Se re-indexó el PNG a esos slots por cercanía de
  brillo (quedan 4 tonos en vez de 5) y se le puso la paleta de las tortugas.
- **Primera versión spawneaba en X random**; comparando contra el GIF del arcade
  se corrigió: la bola SIEMPRE baja por la escalera. Se midió sobre
  `bg01_completa.png` que la escalera ocupa mundo X ≈ 508-620, y trackeando la
  esfera en el GIF cuadro a cuadro se confirmó que nace arriba y rueda en
  **diagonal a la derecha**. Ahora spawnea en X de mundo fija (`IRON_BALL_STAIRS_X`),
  con deriva diagonal (`IRON_BALL_ROLL`) y sólo cuando el alto de la escalera está
  en pantalla. Cadencia por `IRON_BALL_PERIOD`.
- Toda la lógica vive en `scenes.c` (funciones `ironBall*` estáticas, misma casa
  que el fuego y el HUD). `SPR_initEx` subió a 620 por los 16 tiles de la bola.

**Robot del látigo: frame de electrocución por distancia + más velocidad.**

- **Bug del frame congelado:** al atrapar a la tortuga, el látigo electrificado no
  quedaba a la longitud correcta. El cálculo escalaba con la cantidad de frames
  del *throw* pero lo aplicaba a la anim de *electro* (otra cantidad de frames →
  índice fuera de rango) y medía la distancia con el borde del sprite en vez del
  centro. Se reemplazó por un helper `robotElectroFrame()` que usa el `throwFrame`
  con el que **realmente enganchó** (= la distancia exacta robot→player en ese
  instante) escalado al `numFrame` REAL de la anim de electro, y se recalcula en
  cada alternancia A↔B por si difieren.
- **Más velocidad:** movimiento `ROBOT_SPEED` 2→3 (patrulla + alineado en Y);
  animaciones auto (aparición, giro, caminata, windup, láser) bajando el `time`
  del sprite `robot_whip` de 8 a 6; látigo (lanzar/recoger, a mano)
  `ROBOT_THROW_TICKS` 5→3; y `ROBOT_LASER_FIRE_DELAY` 12→8 para que el rayo salga
  antes y calce dentro de la anim ya acelerada.

**Regla nueva de la casa:** un sprite que "comparte la paleta de X" (PAL1/PAL2/…)
tiene que estar indexado sobre los índices REALES de esa paleta, no sobre una
paleta de grises cualquiera. Antes de dibujarlo con `TILE_ATTR(PALx,...)` hay que
verificar los slots contra la paleta destino — no alcanza con que "sean grises".
## 31 de julio de 2026 - Phase A: foot soldier morado rediseñado (64x80), hojas de tortuga nuevas y sistema de agarre

- **Foot soldier morado rediseñado:** nueva hoja `foot_soldier_16colors.png` de
  448x1280 (7x16 celdas de 64x80). Nueva fila por animación, documentada en el
  `.res`: 0 idle (1f), 1 caminar (5f), 2 patada (4f), 3 uppercut (2f), 4
  caminar de frente (4f), 5 explosión (6f), 6 golpe frontal (2f), 7 romper
  puerta (4f), 8-10 golpe recibido 1-3 (1f cada una), 11 giro (2f, flip-aware),
  12 guardia (3f), 13 stance / postura de espera (3f), 14 GRAB (celdas
  transparentes, hay que animarla), 15 voltereta (7f). La fila 12 de la hoja
  vieja (shuriken) quedó descartada: el naranja sigue usando su hoja 104x104.
- **Hojas de tortuga nuevas (Leo/Mike):** 1248x2184 = 12x21 celdas (Raph/Don
  quedan con la hoja vieja 12x19 "corrida": se decidió NO compensarles offset
  por personaje). El enum `PlayerAnim` se reordenó para el layout nuevo:
  `IDLE2` (1) entra después de ~5s parado (`PLAYER_IDLE_ANIM_DELAY`), se
  reproduce una vez y vuelve a IDLE; `ANIM_HELD` (18) ahora tiene 4 frames
  (0-2 = loop del agarre, 3 = frame de recibir golpe mientras lo sostienen) y
  `ANIM_KO` (20) 4 frames. Para no romper las hojas viejas de Raph/Don,
  `setHeldFrame()` ajusta el frame al `numFrame` real de la anim.
- **Geometría por tipo de enemigo:** el `Enemy` ahora carga su ancho, alto y
  offset de pies en runtime (`w`/`h`/`footOffset`, morado 64/80/80, naranja
  104/104/96) en `initEnemySpawn()`. Todas las referencias viejas a
  `ENEMY_SPRITE_W` / `ENEMY_FOOT_OFFSET` (centros, X máx, Y de sprite,
  shurikens, hitbox) pasaron a `e->w` / `e->footOffset`, y el clamp de mundo
  izquierdo a `-(s16)e->w`.
- **Estados nuevos:** `ENEMY_STATE_TURN` (el giro de 16 ticks al cambiar de
  dirección mientras persigue, anim 11) y `ENEMY_STATE_GRAB` (agarra a la
  tortuga por la espalda). En idle el morado alterna IDLE/stance
  (`ENEMY_STANCE_SWITCH`) y si ya hay atacantes activos se pone en guardia
  (anim 12). Las oleadas que entran "por la espalda" spawnean con voltereta
  (anim 15, `initEnemySomersaultSpawn`, 56 ticks) en vez de caminando; las de
  puertas/ascensor no.
- **Sistema de agarre:** `playerFootGrab()` (agarre a mano del morado) y
  `playerWhipGrab()` (eléctrico del robot) comparten la liberación
  `playerReleaseGrab()` (idle + invencibilidad de hurt). El agarrado queda en
  `STATE_GRABBED` con loop manual de frames (0-1-2) y el morado se pega a su
  espalda con `ENEMY_GRAB_BACK_OFFSET`. Se rompe si la tortuga golpea el
  mashing, si golpean al soldado, o por timeout de `ENEMY_GRAB_MAX_TIME` (240).
  Decisiones de diseño: **el agarrado SÍ puede recibir daño** (el jugador 2 lo
  libera pegándole al soldado) pero el **doble agarre está bloqueado**
  (`playerIsGrabbed`), y Raph/Don solo reciben daño, nunca se les muestra el
  loop de agarre.
- **Build verde:** `make` completo con la hoja 8x10 del morado (0 warnings de
  enemigos, solo el warning benigno de `lto-wrapper`). `out/rom.bin` generado y
  checksumeado (786432 bytes).

**Siguiente paso (Phase B):** portar el comportamiento fiel del arcade desde
`arcade_reverse_eng/` y pasar el HUD de P1/P2 a spritesheets (4 anims de 1
frame, índice = personaje elegido).

## 31 de julio de 2026 - Phase B (1/2): HUD de P1/P2 como spritesheet (un frame por tortuga)

- **HUD a sprites:** se reemplazó el HUD dibujado como imagen de fondo por
  sprites de 72x32 por jugador. `hud_1p.png`/`hud_2p.png` = 72x128 (4 frames
  de 72x32 apilados), reordenados a **orden de personaje** (0=Leo, 1=Mike,
  2=Don, 3=Raph) con permutación [0,3,2,1] para que `SPR_setAnim(sprite,
  personajeSeleccionado)` funcione directo. En `level1.res`:
  `SPRITE hud_1p "/images/hud/hud_1p.png" 9 4 NONE 0` (9x4 tiles = 72x32 px,
  `NONE NONE` para que rescomp no reordene ni comprima — indexing directo).
  Paleta = PAL1 (la de las tortugas, verificada idéntica), prioridad alta.
- **scenes.c:** `#define HUD_TILE_W 9`, P1 en X=0 y P2 en
  `SCREEN_PIXEL_WIDTH - (HUD_TILE_W * 8)` = 248. `hudInit()` agrega los
  sprites con `SPR_addSprite` en Y=0 y el `SPR_setAnim` según
  `personajeSeleccionado`/`personaje2Seleccionado` (P2 solo si
  `cantidadJugadores == 2`). El VRAM de tiles libres (`hudVramFree`) ahora
  arranca justo después de los tiles del fuego (antes lo consumía el HUD de
  fondo); la base de tiles de P2 es `40 - HUD_TILE_W`. Se quitaron las
  referencias al HUD viejo por imagen.
- **main.c:** `SPR_initEx(620)` → `SPR_initEx(720)` (los 2 HUD suman 70 tiles
  al presupuesto de sprites; el viejo 620 no alcanzaba).
- **Build verde:** `hud_1p`/`hud_2p` rescompilan en 4 anims x 1 frame x 3
  VDP sprites / 35 tiles c/u (4736 B raw) y las paletas deduplican a
  `hud_1p_palette`. `out/rom.bin` = 786432 bytes. Commits: `a00c395`.

## 31 de julio de 2026 - Phase B (2/2): combos de 2-3 golpes del foot soldier morado (fiel al arcade)

- **Combos (fiel al arcade ATTACK S0/S1/S2):** el foot soldier morado ya no
  pega un solo golpe: al atacar entra a `ENEMY_STATE_ATTACK` con un COMBO de
  pasos (`comboLen`/`comboStep` nuevos en `Enemy`), cada uno con su propia
  animación, duración, alcance y ventana de hitbox (`ComboStep` + tablas
  `purpleComboPunch/Kick/Front` en `enemy.c`). Combos:
  - `purpleComboPunch` (arcade S0, elegido pegado): directo → directo →
    uppercut (3 golpes).
  - `purpleComboKick` (arcade S1/S2, elegido a media distancia): directo →
    patada con salto (lunge de 16 ticks, 3px/frame).
  - `purpleComboFront` (variante corta): doble directo.
  Al expirar el timer de un paso avanza al siguiente reseteando `attackHit`
  (cada golpe conecta UNA vez), y al terminar el combo vuelve a CHASE con el
  cooldown de siempre. El timer del estado es el del paso actual; los
  i-frames del jugador (45) hacen que una cadena completa rara vez conecte
  entera — como el knockback del arcade, el golpe 1 suele sacarte del
  alcance de los siguientes. El naranja no usa combos (`comboLen` = 0 → el
  camino simple de siempre queda intacto).
- **Fix del clamp de la pared diagonal (bug latente de Phase A):** todas las
  llamadas `enemyMaxX()` pasaban el valor `e->y` (o `gy`/`list[i].y`) donde la
  función espera un `const Enemy*` — el compilador avisaba
  `-Wint-conversion` y el bound del clamp se computaba leyendo basura de una
  dirección baja de ROM. Resultado: la pared diagonal del final (fire escape)
  NO limitaba a los enemigos, que podían caminar a través de ella. Se corrigió
  en las 8 llamadas (`enemyMaxX(e)`, `enemyMaxX(&list[i])`) y en el agarre se
  reordenó para setear `e->y = gy` antes de calcular el bound. Los warnings
  de `enemy.c` desaparecieron (quedan solo los `-Wchar-subscripts` de
  `scenes.c`).
- **Build verde:** `make` completo, `out/rom.bin` = 786432 bytes. Commit:
  `dda832c`.

**Siguiente paso (Phase B, resto):** con los combos hechos, evaluar con la
captura arcade (`arcade_reverse_eng/footsoldier_capture.txt`) si el morado
debe también tirar shurikens (`PROJ S0`, DMG=32) — hoy solo lo hace el
naranja — y el ritmo de estados del arcade (ciclo por timer de 24 frames).
