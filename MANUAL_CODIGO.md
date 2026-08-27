# Manual didáctico del código — TMNT Arcade (Mega Drive / SGDK)

> Port en C del *TMNT: The Arcade Game* (Konami 1989) a la Sega Mega Drive,
> usando SGDK. Este manual explica **qué hace cada función y por qué está
> hecha así**, módulo por módulo. Está escrito para alguien que conoce C pero
> quizá no los detalles de la consola: las secciones 2 y 4 dan las bases.

**Índice**

1. [Panorama del proyecto](#1-panorama-del-proyecto)
2. [La Mega Drive en 10 minutos](#2-la-mega-drive-en-10-minutos)
3. [Convenciones de coordenadas y sprites](#3-convenciones-de-coordenadas-y-sprites)
4. [`main.c` — el bucle de escenas](#4-mainc--el-bucle-de-escenas)
5. [`res/scenes.h` — el enum de escenas](#5-resscenesh--el-enum-de-escenas)
6. [`src/scenes.c` — helpers compartidos](#6-srcscenesc--helpers-compartidos)
7. [`src/scenes.c` — fondo, fuego y humo](#7-srcscenesc--fondo-fuego-y-humo)
8. [`src/scenes.c` — bola de hierro](#8-srcscenesc--bola-de-hierro)
9. [`src/scenes.c` — HUD y continues](#9-srcscenesc--hud-y-continues)
10. [`src/scenes.c` — las escenas](#10-srcscenesc--las-escenas)
11. [`res/player.h` / `res/player.c`](#11-resplayerh--resplayerc)
12. [`src/enemy.h` / `src/enemy.c`](#12-srcenemyh--srcenemyc)
13. [`src/robot.h` / `src/robot.c`](#13-srcroboth--srcrobotc)
14. [`src/rocksteady.h` / `src/rocksteady.c`](#14-srcrocksteadyh--srcrocksteadyc)
15. [Recursos (`.res`) y paletas](#15-recursos-res-y-paletas)
16. [Decisiones de diseño y trampas conocidas](#16-decisiones-de-diseño-y-trampas-conocidas)
17. [Compilar y probar](#17-compilar-y-probar)

---

## 1. Panorama del proyecto

```
src/
  main.c          Bucle principal: máquina de estados de escenas
  scenes.c        TODO lo demás de las escenas (~3800 líneas): helpers, fondo
                  streaming, fuego/humo, bola de hierro, HUD, continues y cada
                  pantalla del juego
  enemy.c/.h      Foot soldiers (morado "regular" y naranja "kiter")
  robot.c/.h      Mini-jefe del látigo (final del nivel 1)
  rocksteady.c/.h Jefe final del nivel 2
  boot/           Stubs de arranque SGDK — NO modificar

res/
  player.h/.c     El jugador (¡viven en res/, ver nota abajo!)
  scenes.h        Enum SceneId + prototipos show*()
  *.res           Definiciones de recursos → rescomp genera *.h aquí mismo
  sprites/, images/, audio/, music/
```

> **Nota:** `player.h`, `player.c` y `scenes.h` están en `res/`, no en `src/`.
> Es una peculiaridad histórica del proyecto: rescomp genera los headers de
> recursos dentro de `res/` y desde ahí se resuelven los includes. No moverlos
> sin revisar las rutas de include.

El juego hoy: intro SEGA → intro arcade (título TMNT con scroll) → selección
de jugadores/tortugas → Nivel 1 (callejones de NY: oleadas, puertas, ascensores,
bola de hierro y el mini-jefe robot) → Nivel 2 (pasillo en llamas + Rocksteady
con cápsula-taladro) → cutscene final (Shredder se lleva a April) → ending /
game over. Soporta 1–2 jugadores simultáneos con HUD propio por jugador.

---

## 2. La Mega Drive en 10 minutos

Lo mínimo para entender el código:

- **Dos planos de fondo (BG_A y BG_B)**, cada uno un tilemap de tiles de 8×8.
  BG_A suele ir *encima* de BG_B; cada tile decide su prioridad (encima o
  debajo de los sprites).
- **Sprites**: hardware dibuja hasta ~80 sprites por línea desde la SAT
  (tabla de sprites). SGDK los administra con `Sprite*` (`SPR_addSprite`,
  `SPR_setPosition`...). La posición es la **esquina superior izquierda** del frame.
- **VRAM (~64 KB)** guarda tiles de fondo, tilemaps, sprites y la fuente. Todo
  presupuesto: si te pasás, se pisan datos. Por eso el proyecto cuenta tiles
  constantemente (ver §16).
- **Paletas**: 4 líneas de 16 colores (PAL0..PAL3), 64 entradas CRAM en total.
  Los índices son **globales**: PAL2 color 5 = entrada CRAM 32+5=37.
- **DMA**: copias masivas VRAM↔ROM. `DMA_QUEUE` encola transferencias que
  SGDK ejecuta durante el VBlank (inicio del refresco vertical) — es la forma
  segura de actualizar VRAM sin parpadear.
- **Scroll**: horizontal por plano o **por tile** (`HSCROLL_TILE`: tabla de
  scroll por fila de tiles). Ojo: el modo es *global a ambos planos*, no por plano.
- **VBlank**: el juego hace toda la lógica y termina con
  `SYS_doVBlankProcess()`, donde SGDK aplica scrolls, DMA encolados y paletas.

### Audio

Se usa el driver **XGM2** (`XGM2_play`, `XGM2_setFMVolume`,
`XGM2_setPSGVolume`) porque permite regular volumen de música en tiempo real;
el driver viejo XGM no. Las voces ("attack!", risa de Shredder) son WAV
reproducidos por PCM (`XGM2_playPCMEx(..., SOUND_PCM_CH2, 15, ...)`) con
prioridad 15 (máxima) para que no se corten.

---

## 3. Convenciones de coordenadas y sprites

Estas reglas aplican a TODO el código y explican la mitad de los cálculos:

| Concepto | Regla |
|---|---|
| X de mundo | Píxeles absolutos del nivel. Nivel 1: 0..1376. Nivel 2: 0..440 |
| X de pantalla | `worldX - cameraX`. La cámara solo avanza a la derecha (salvo nivel 2) |
| Y = pies | Para TODA entidad, `y` es la línea de los pies (lane), no el centro |
| Dibujo | `drawY = y - footOffset - jumpZ` (footOffset = distancia tope-frame→pies) |
| Profundidad | `SPR_setDepth(spr, -y)`: menor valor = dibujado adelante (convención SGDK) |
| jumpZ | Altura de salto puramente visual; colisiones usan siempre `y` |
| x ancla | Tortugas/soldados/jefe: borde IZQUIERDO del frame (centro = x + w/2). **Única excepción: robot** (x = centro del cuerpo) |

Lanes jugables: Y entre 142 (fondo visual) y 200 (frente) en ambos niveles.
Al final de cada nivel hay una pared diagonal: el X máximo permitido se
interpola linealmente entre `LEVEL_END_WALL_X_TOP` (1308 en Y=142) y
`..._BOTTOM` (1352 en Y=200).

---

## 4. `main.c` — el bucle de escenas

```c
SPR_initEx(752);
while (TRUE) {
    switch (sceneId) {
        case SCENE_SEGA: sceneId = showSegaIntro(); break;
        ...
    }
    SYS_doVBlankProcess();
}
```

- **Por qué `SPR_initEx(752)`**: el default de SGDK reserva 420 tiles de VRAM
  para sprites. Con 2 tortugas + 4 soldados de 104×104 (~540 tiles peor caso)
  más frames de HUD, retratos y la bola de hierro, quedaba corto. 752 cubre el
  peor caso. Cada escena puede re-inicializar el motor con otro presupuesto y
  debe restaurar 752 al salir.
- **Por qué una máquina de estados**: cada `showXxx()` corre una pantalla
  completa (setup → loop → limpieza) y devuelve el siguiente `SceneId`.
  Agregar pantallas no toca nada existente; el flujo entero del juego es
  legible en un solo `switch`.

---

## 5. `res/scenes.h` — el enum de escenas

Define `SceneId` (orden = orden del juego):

```
SEGA → KONAMI → SGDK → CREDITS → INTRO_ARCADE → VRAM_CLEAR →
PLAYER_SELECT → OPTIONS → CHAR_SELECT → CINEMATIC_FIRE(stub) →
LEVEL1_TITLE → LEVEL1 → LEVEL2 → ENDING → GAME_OVER(→ SEGA)
```

y los prototipos `SceneId showXxx(void)` más:

- **`clearSceneEx(bool keepAudio)`** — limpieza común entre escenas (§6).
- **`clearScene()`** — macro = `clearSceneEx(FALSE)`.

---

## 6. `src/scenes.c` — helpers compartidos

### Estado global del juego

```c
u16 personajeSeleccionado   = 0;  // tortuga de P1 (0=Leo 1=Mike 2=Don 3=Raph)
u16 personaje2Seleccionado  = 3;  // tortuga de P2 (default Raph)
u16 cantidadJugadores       = 1;
static u16 continuesLeft    = 3;  // pool COMPARTIDO entre ambos jugadores
extern u16 vidasIniciales;        // definida en player.c, editable en OPTIONS
```

### `void clearSceneEx(bool keepAudio)`

**Qué hace:** fundido a negro (20 frames), frena la música salvo `keepAudio`,
`SPR_reset()` + `SPR_update()` (vacía la SAT), limpia planos A/B, resetea el
scroll de ambos planos y devuelve el modo de scroll a `HSCROLL_PLANE`.

**Por qué así:**
- `SPR_reset` + `SPR_update` juntos: si no se hace un update tras el reset, la
  SAT vieja sigue en VRAM y los sprites de la escena anterior aparecen un
  frame durante el setup de la nueva.
- Resetear scroll y modo de scroll evita glitches clásicos: el nivel deja el
  modo en `HSCROLL_TILE` y scroll desplazado; sin esto, el logo de TMNT salía
  corrido al reiniciar tras game over.
- `keepAudio=TRUE` lo usa el ending para que la música de la cutscene siga
  sonando sobre la imagen final.

### `void playMusicVol(const u8* track, u8 vol)`

Setea volumen FM+PSG con `XGM2_setFMVolume/setPSGVolume` y reproduce con
`XGM2_play`. Centraliza el control de volumen: la música del arcade saturaba
el chip FM a full. Volúmenes actuales: `VOL_MUSIC_* = 90`, ending 80,
SFX 100.

### `bool justPressedJoy(u16 joy, u16 prev, u16 button)`

Detección de flanco: `(actual & button) && !(prev & button)`. Cada escena
guarda el estado del pad del frame anterior y consulta esta función; así
"mantener START" no repite la acción. Es la alternativa manual a eventos JOY,
para no depender de callbacks.

### `void charMove(u16* self, const u16* other, s16 dir)`

Mueve la selección de personaje/tortuga en dirección `dir` **saltándose el
slot del otro jugador**. Se usa en selección de personaje y en el continue:
garantiza que P1 y P2 nunca jueguen con la misma tortuga.

---

## 7. `src/scenes.c` — fondo, fuego y humo

### Fondo del nivel 1: streaming circular

El fondo mide 1376 px = 172 tiles: **no entra en un plano de 64 columnas**
(junto con todo lo demás de VRAM). Solución: plano B de 64×32 usado como
**ventana circular** que revela columnas nuevas conforme la cámara avanza.

Constantes clave: `LEVEL1_PIXEL_WIDTH 1376`, `SCREEN_PIXEL_WIDTH 320`
(=40 columnas visibles), `CAM_MAX_X 1056` (=1376−320), `BG_PLANE_W 64`.

#### `static void bgDrawColumn(u16 srcCol)`
Escribe la columna `srcCol` del tilemap original (en ROM) en la posición
circular `destCol = srcCol & 63` del plano, fila por fila con
`VDP_setTileMapXY`. El atributo base (paleta/prioridad) viene de `bgBaseAttr`.

#### `static void bgInit(VdpPlane plane, const TileSet* ts, const u16* map, u16 w, u16 h, u16 baseAttr)`
Carga el TileSet completo en `TILE_USER_INDEX`, guarda punteros al mapa ROM y
dibuja las primeras `min(64, w)` columnas. Solo se llama una vez por nivel.

#### `static void bgUpdate(s16 cameraX)`
Calcula la última columna visible (`cameraX>>3 + 41`, clampeada al ancho) y
mientras `bgLastCol < needCol` va revelando columnas nuevas con
`bgDrawColumn`. Después llena `bgScrollTbl[28]` con `-cameraX` y lo baja con
`VDP_setHorizontalScrollTile(BG_B, ...)`.

**Por qué scroll por tile y tabla completa:** el fuego fuerza el modo global
`HSCROLL_TILE`; ese modo afecta a AMBOS planos, así que BG_B también necesita
su tabla de 28 filas de tiles actualizada cada frame. `SCROLL_TILE_ROWS 28`
porque 28 filas de 8 px = 224 px de pantalla.

#### `static void levelFadeIn(pal0, pal1, pal2, pal3)`
Arma un array combinado de 64 colores con las cuatro paletas, **fuerza la
CRAM a negro primero** (helpers como `initEnemySpawn` ya cargaron paletas a
mitad del setup, y sin este paso se verían "saltar") y hace el fundido de
entrada. Toda escena termina su setup con esto.

### El fuego (técnica compartida con el humo del nivel 2)

Un solo cuadro de fuego 64×64 vive en VRAM sobre BG_A (prioridad ALTA, encima
de todo); el tilemap lo repite 8 veces a lo ancho del plano. Cada 8 ticks se
**sobreescriben los mismos 64 tiles** vía `DMA_QUEUE` desde la tira de 8
frames en ROM.

**Por qué así:** la idea original (scroll de una tira grande) fue descartada
por presupuesto de VRAM. Sobreescribir tiles es barato (misma dirección de
siempre) y el parallax a media velocidad se logra scrolleando solo las filas
del fuego a `-cameraX/2`.

#### `static void fireInit(u16 vramInd)`
Carga el frame 0 (64 tiles), construye el tilemap repetido con prioridad TRUE,
pasa el sistema a `HSCROLL_TILE` y pone a cero la tabla de scroll de BG_A.
`vramInd` es la primera tile libre después del fondo (calculada por quien llama).

#### `static void fireUpdate(s16 cameraX)`
Cada `FIRE_FRAME_INTERVAL` ticks avanza de frame y encola el DMA del frame
siguiente sobre los mismos tiles; luego actualiza el hscroll de sus filas
(`FIRE_Y_TILE 20` .. 27) a media velocidad.

### Humo del nivel 2: `smokeInit` / `smokeUpdate`

Misma técnica del fuego, tres diferencias deliberadas:
1. **Prioridad BAJA** (detrás de sprites): el humo es ambiente, no obstáculo.
2. **Parallax aún más lento** (un cuarto) para dar profundidad al techo.
3. **Excepción por columnas:** las columnas 35..49 del bandón van con
   prioridad ALTA. Motivo: la cápsula del taladro es un sprite de prioridad
   baja; la prioridad en la Mega Drive se resuelve **tile a tile del plano**,
   así que esas columnas altas hacen que la cápsula se vea DETRÁS del humo
   justo ahí, integrándola al fondo. Truco barato, cero sprites extra.

---

## 8. `src/scenes.c` — bola de hierro

Bola que cae de una escalera de incendios (worldX ≈ 535) cada 180 frames,
rebota y rueda hacia la derecha hasta salir por abajo. Daña a todos por igual.

- **`ironBallInit`** — agrega el sprite oculto y arma el timer de spawn.
- **`ironBallHits(cx, cfy)`** — overlap de caja 26×22 contra el centro de la
  entidad. Función separada para poder testearla contra jugador y enemigos.
- **`ironBallUpdate(cameraX, ...)`** — física mínima: cae a 2 px/frame,
  rebota cuando z≤0 (`vz=BOUNCE`), avanza 1 px/frame, sale al llegar a
  `EXIT_Y 236`. Solo aparece si la escalera está en pantalla (margen 40 px):
  spawnea cosas fuera de cámara desperdicia sprites y confunde. Golpea
  jugadores con `damagePlayer` (respeta i-frames) y aplasta soldados con
  `damageEnemy(ENEMY_HP)` (muerte instantánea, como en el arcade).
- **`ironBallEnd`** — oculta y desactiva; se llama al entrar al outro para no
  matarte durante la cinemática.

---

## 9. `src/scenes.c` — HUD y continues

El HUD es **sprites + algunos tiles**, no imagen de fondo: así se superpone
bien y se actualiza por partes.

### Constantes
`HUD_TILE_W 9`, P1 en X=40 / columna base 5, P2 en X=208 (=320−72−40) /
columna 26; retratos en los bordes (0 y 288). `hud_1p.png`/`hud_2p.png` traen
4 animaciones **en orden de personaje** (Leo/Mike/Don/Raph) para que
`SPR_setAnim(hudSpr, personajeSeleccionado)` funcione directo.

### `static void hudInit(void)`
Agrega frames de HUD (PAL1, prioridad alta) y retratos de P1/P2 según los
personajes elegidos. Se llama al empezar cada nivel.

### Barra de vida (tiles, no sprite)
`hpBarInit(barVram, baseCol)` carga el frame lleno y escribe 4×1 tiles en la
fila 2 del área del HUD; `hpBarSetFrame(barVram, frame)` sobreescribe esos
tiles vía `DMA_QUEUE` con el frame N de la tira en ROM (recursos `NONE NONE`,
tiles contiguos → offset `N*4*8` longs). Frame mostrado = `10 - health`.

### `HudPlayer` + `uintToDec` + `hudPlayerInit/hudPlayerUpdate`
`HudPlayer` cachea la última vida/lives/score dibujados (inicializados en −1
para forzar primer render). `uintToDec` convierte a decimal a mano y devuelve
el largo (SGDK trae utilidades similares, pero así el formato es exacto).
`hudPlayerUpdate` redibuja **solo lo que cambió**: vidas ("xN"), score
(alineado a derecha en 4 columnas) y barra. Redibujar texto cada frame sería
desperdicio de CPU y provocaría flicker.

### Sistema de continues (`ContPlayer`, `contDrawText`, `revivePlayer`, `continuePoll`)
Cuando un jugador muere (sin vidas), entra en COUNTING: cuenta regresiva de 9
segundos dibujada centrada (`contDrawText` borra o escribe el mensaje según
corresponda). Al presionar START **consume un continue del pool compartido**
(`continuesLeft`) y pasa a SELECTING: se elige tortuga con la cruceta usando
`charMove` (el retrato cambia en vivo). START confirma → `revivePlayer`:

- libera el sprite viejo y llama `initPlayer` en el mismo lugar,
- vida llena y `lives = vidasIniciales` (set completo nuevo),
- i-frames largos + blink para reaparecer sin morir en cadena.

Si el tiempo llega a cero, `continuePoll` devuelve TRUE y la escena va a game
over. En el nivel 2 el polling se desactiva durante la cutscene de Shredder y
mientras el jefe está en el piso (knock-down/death): revivir ahí rompería la
cinemática o el ritmo del duelo.

---

## 10. `src/scenes.c` — las escenas

Todas siguen el patrón **setup → loop propio → limpieza → `return SCENE_...`**.

### `showSegaIntro()` (línea 867)
Rocksteady corre e impacta contra el logo SEGA (máquina de estados local
0..3: correr, impacto, pausa, fade), con `music_sega` y SFX del golpe.
Primera impresión = fidelidad al arcade.

### `showKonamiIntro()` (919)
Stub de 1 línea → `SCENE_SGDK`. Reservado para el logo Konami futuro.

### `showArcadeIntro()` — intro arcade (`src/intro_arcade.c`)
Reescrita a partir del análisis frame a frame de la intro original del arcade.
Vive en su propio módulo (`src/intro_arcade.c`), no en `scenes.c`; el prototipo
sigue declarado en `res/scenes.h` y `main.c` no cambió.

**Línea de tiempo (940 ticks NTSC = los 940 frames del video original, 1:1).**
Las constantes `INTRO_T_*` del módulo son, literalmente, los tiempos del arcade:

| Escena | Ticks | Qué pasa |
|--------|-------|----------|
| A | 200 | Skyline nocturno con la luna, quieto |
| B | 307 | Dolly vertical hasta el callejón (smoothstep sobre 1528 px, con la banda de lluvia estirada) |
| — | 5 | Flash blanco y la tapa de la alcantarilla sale volando |
| C | 87 | Las 4 tortugas saltan afuera dentro del haz de luz |
| D | 152 | Los 4 retratos crecen desde su esquina en cuadrantes |
| E | 122 | Corte a celeste (15) + el banner "TEENAGE MUTANT NINJA" CAE desde arriba (25, con rebote) + fijo (82) |
| F | 118 | Entra "TURTLES" (wipe) + copyright de Konami |

**Lo que hay que entender del módulo:**
- **Banda de lluvia estirable** (`RAIN_*`): entre las filas de tile 87 y 126 la
  tira trae un bloque de 8 filas (64 px) PERFECTAMENTE tileable — es `fondo_b`
  horneado adentro, repetido 4 veces y media (verificado tile a tile: 95..102,
  103..110 y 111..118 son idénticas a 87..94). Para alargar ese tramo NO se baja
  la velocidad: `dollyMapRow()` inserta `RAIN_EXTRA_LOOPS` copias más del bloque
  entre la fila virtual y la real, y `INTRO_T_DOLLY` se recalcula solo para
  mantener los mismos px/tick. La sensación de velocidad no cambia, la cámara
  simplemente pasa más tiempo adentro de la lluvia. Cero tiles extra en VRAM y
  sin costura (el punto de inserción respeta la fase del bloque).
- **Streaming VERTICAL** (`dollyInit`/`dollyUpdate`/`dollyDrawRow`): la tira del
  dolly mide 256×1496 px = 32×187 tiles y no entra en ningún plano. Es la misma
  técnica que el fondo del nivel 1 pero girada 90°: los 1001 tiles únicos van a
  VRAM una vez, BG_B es una ventana circular de 32 filas (256 px) y se dibujan
  filas nuevas por abajo pisando las que salieron por arriba. Quedan 4 filas de
  colchón (256−224) y se escribe siempre 2 filas por debajo del borde visible.
- **Haz de luz gratis** (`beamDraw`): `intro_luz` tiene las filas 0..24 idénticas
  (el cuerpo) y 25..27 de base redondeada → crecer el haz es dibujar más filas
  del cuerpo. 11 tiles únicos, en BG_A (arriba del fondo, debajo de los sprites).
- **Cuadrantes de la escena D**: el hardware no escala. El rectángulo que crece
  se rellena con el tile SÓLIDO que la propia imagen ya tiene en la esquina de
  cada cuadrante (se lee del tilemap en runtime) y al completarse se vuelca el
  cuadro real encima.
- **Celeste de las escenas E/F**: es el color de *backdrop* del VDP
  (`INTRO_SKY_PAL_INDEX`, lo reporta el generador), no un tile.
- **Caída del banner**: se dibuja en BG_A y baja con el SCROLL VERTICAL del
  plano (pixel a pixel; redibujar el tilemap solo permitiría saltos de 8 px).
  Con `scrollA = S` el banner queda en pantalla a `y = BANNER_ROW*8 - S`, así
  que la caída es S de 64 → 0 con desplazamiento cuadrático (gravedad) más un
  rebote de `BANNER_BOUNCE` px. Al aterrizar se vuelca a BG_B y BG_A se limpia
  para el wipe del logo.
- **Salida**: `INTRO_T_FADE_OUT` (60 ticks) + `INTRO_T_BLACK_HOLD` (24) antes de
  devolver `SCENE_VRAM_CLEAR`.
- **256 px de ancho** (`VDP_setScreenWidth256`): todo el arte fuente está hecho
  a esa medida (los 4 retratos son 124×110 → 2×2 = 248×220 ≈ 256×224).
- **Presupuesto**: plano 64×32 → userTileMaxIndex ≈ 1612. A/B/C usa 1012 tiles de
  usuario + `SPR_initEx(416)`. D (378) y E/F (293) recargan VRAM desde cero en
  cada corte duro, tapadas por el negro.
- **Restaura todo al salir**: 320 px, plano 32×32, `SPR_initEx(752)`. Olvidar
  cualquiera rompe la escena siguiente. Devuelve `SCENE_VRAM_CLEAR`.

**Assets**: los generan `tools/gen_intro_assets.py` (PNG indexados con paletas
compartidas, en `res/images/intro_tmnt/genesis/`) a partir del arte de
`res/images/intro_tmnt/` y `res/images/intro_tmnt/assets/`. `tools/preview_intro.py`
renderiza la intro completa offline con las mismas constantes que el C, para
revisar encuadres y tiempos sin compilar.

### `showProfiles()` — perfiles de las tortugas (`src/scene_profiles.c`)
Modo ATRACTO. `showPlayerSelect` lleva un contador de inactividad: si pasan
`PLAYER_SELECT_IDLE_SECS` (30 s) sin que se toque un boton en NINGUNO de los dos
joysticks, devuelve `SCENE_PROFILES`. Esta escena muestra el perfil de UNA
tortuga sorteada (nunca la misma dos veces seguidas) y vuelve a
`SCENE_PLAYER_SELECT`, asi que cada timeout saca otra.

Secuencia: el retrato **entra desde el borde derecho**, cruza el medio y frena a
la izquierda (ease-out cuadratica) → aparecen los datos a su derecha, linea por
linea → la descripcion se escribe letra a letra abajo de los dos (misma tecnica
que los creditos) → se queda ~4 s → vuelve. **Cualquier boton corta en cualquier
momento.** Suena `music_profiles`.

Detalles que importan:
- **El retrato es un SPRITE, no un IMAGE**: tiene que deslizarse, y un sprite se
  mueve pixel a pixel sin tocar el tilemap. Son 64x128 px = 8x16 tiles = 128
  tiles de VRAM, holgado dentro de `SPR_initEx(420)`.
- **Cada retrato trae su propia paleta** (los 4 no entran juntos en 16 colores).
  Como hay uno solo en pantalla, se carga la del sorteado en PAL0 y la fuente
  arcade en PAL2 (con `PAL_setPalette`, que escribe UNA linea — `PAL_setColors`
  escribiria las 64 entradas del PNG y pisaria las otras paletas).
- **Los textos son texto**, dibujados con `title_font`, no imagenes. Salieron de
  `res/images/profiles/*_data.png` y `*_profile info.png` transcriptos con
  `tools/ocr_arcade_font.py` (compara cada celda de 8x8 contra los 95 glifos y
  solo acepta coincidencias del 100%). Hizo falta porque la W y la M de esta
  fuente son casi identicas a ojo: "WILD BOY OF THE BUNCH" se lee "MILD".
- El sorteo arranca deterministico en un arranque en frio (`random()` todavia no
  fue llamada); a partir de la segunda vez rota sola.

### `showVramClear()` (1099)
Limpieza hardware real: apaga display, `SPR_end` (libera TODA la VRAM de
sprites), vacía cola DMA y hace un fill de VRAM completo. Existe porque la
intro dejó la VRAM sucia y la dirección de la SAT depende del tamaño de plano
anterior (0xAC00 con planos de 64 vs 0xF400 con 32). Normalizar scroll y CRAM
acá garantiza que los menús empiecen en estado conocido.

### `showPlayerSelect()` (1144)
Logo grande (necesita `SPR_initEx(420)` + plano ancho), menú con cursor
(selector_turtle): cantidad de jugadores y opciones. Dos gotchas resueltos:
- `title_font_pal` tiene **64 entradas**: cargarla con `PAL_setColors` desde
  PAL2 se pasa del final de la CRAM y wrappea. Se usa `PAL_setPalette(PAL2,…)`
  (16 colores justos).
- Transferencias `CPU` (no RQUEUE) para evitar condiciones de carrera con
  rescomp/DMA al pintar textos sobre imágenes recién cargadas.

### `showOptions()` (1283)
Submenú: VIDAS iniciales (3/5/7 → escribe `vidasIniciales`) y SOUND TEST
(tabla de tracks). Detalle fino: `sndPlaying` es una variable local porque
`XGM2_isPlaying()` tarda ~1 frame en reflejar un `XGM2_play` recién hecho.
SALIR con START sobre la fila o B en cualquier lado. Restaura fuente y
presupuesto de sprites antes de salir.

### `showCharSelect()` (1405)
Grilla de personajes sobre `characters_greyscale`. Mapeos no triviales:
- `faceRow`: la grilla dibuja {Leo, Mike arriba; Don, Raph abajo} pero el
  orden lógico de selección es 0=Leo,1=Mike,2=Don,3=Raph → tabla de conversión.
- `hudAnimForChar`: mismo mapeo para animar el HUD.
- 2P: dos cursores independientes + `charMove` impide elegir la misma tortuga.
- Al confirmar ambos: `playerPersistReset()` (vidas/score limpios) y
  `continuesLeft = 3`.

### `showFireCinematic()` (1586)
Stub → `SCENE_LEVEL1_TITLE`.

### `drawTextTypewriter(...)`
Texto letra a letra con delay; los espacios consumen tiempo sin dibujar
(para mantener ritmo constante). Devuelve TRUE si START lo salteó — todas las
escenas de texto lo respetan.

### `showSGDKIntro()` (1628) y `showCredits()` (1697)
Créditos del port (bilingüe) y pantalla de créditos con logo + esqueleto
bailando (`skeleton_music`). La música `music_credits` se reproduce UNA vez
(`XGM2_setLoopNumber(0)`) y al salir se restaura loop infinito (−1): si no,
el loop queda configurado para todas las canciones siguientes. Ambas restauran
la fuente blanca default.

### `showLevel1Title()` (1754)
Cartel "SCENE 1 — FIRE! WE GOTTA GET APRIL OUT!!". Espera START y además
espera que lo suelten (para no saltear el nivel).

### `showLevel1()` (1806) — el nivel completo

**Setup (en este orden por dependencias de VRAM):**
1. `bgInit(BG_B, fondo)` → fondo del nivel.
2. Limpia BG_A y `fireInit(hudVramFree)` → el fuego ocupa las tiles libres
   después del fondo.
3. `hudInit` + barras de HP + fuentes de score.
4. Sistemas: `resetEnemyAI(cantidadJugadores)`, `shurikenInit`, `robotInit`,
   `ironBallInit`, arrays de puertas/chispas/ascensores.
5. Jugadores: `initPlayer` P1 (40,182) y P2 (160,182), límite derecho 216
   (antes del primer lock de zona).
6. `playMusicVol(music_level1)`, `bgUpdate(0)` y `levelFadeIn(...)`.

**Intro:** burbuja de "!ATTACK!" (constantes BUBBLE_*: sólida 2 s, luego
parpadeo) + voz `attack_vo` por PCM canal 2 prioridad 15; el primer soldado
aparece caminando desde fuera de pantalla en estado CHASE.

**Loop principal (pasos comentados 1..8c):**
1. `updatePlayer` de P1/P2.
2. **Cámara**: dead-zones asimétricas — el jugador líder empuja al pasar 120 px
   del centro-derecha, el rezagado tira de la cámara si queda a menos de
   80 px detrás (margen 8, velocidad máx 4 px/frame, NUNCA retrocede:
   convención beat-em-up). Locks por zona (abajo). Si la cámara sigue en 0 a
   los ~6 s aparece el cartel HURRY UP (256,40).
3. Notifica límites a los jugadores (`setPlayerRightBound` con la pared diagonal).
4. Burbuja de ataque (fases sólido→parpadeo→off).
5. **Puertas** (centros 429/718/846): se crea/libera el sprite según cercanía
   a cámara; se arman a <110 px; al pasar el jugador cerca, abren y sueltano
   enemigos si hay capacidad (`MAX_ACTIVE_ENEMIES`).
6. **Ascensores** (centros 972/1100): cuando AMBOS jugadores están dentro del
   rango central [40,280], animación de apertura (32 ticks), desaparecen y
   spawnean 2 enemigos. Requiere a los dos: mecánica cooperativa del arcade.
7. **Zonas de combate** (máquina `combatZone` 0..9) — locks de cámara:
   | Zona | Lock | Spawns |
   |---|---|---|
   | ZONE1 | 150 | voltereta desde la izquierda (y160), patada desde la derecha (y160), patada derecha naranja (y166) |
   | ZONE2 | 300 | voltereta izquierda (y145), caminando derecha (y160) |
   | ZONE3 | 614 | voltereta izquierda (y162), caminando derecha NARANJA (y150) |
   | ZONE4 | 880 | gate de los ascensores (elevPhase) |
   | ZONE5 | 1056 | camina naranja izquierda (y160) + `robotSpawn(1256)` |
8. Combate:
   - `separateEnemies` (empuje suave 1 px por eje entre pares cercanos).
   - `updateEnemy` de cada slot.
   - `robotUpdate` (mini-jefe).
   - `shurikenUpdate`.
   - Colisiones jugador→enemigo: ventana activa del ataque + solape
     `[-ATK_BACK, +atkReach]` con tolerancia Y ±20. El especial mata de un
     golpe (`ENEMY_HP` de daño), kill suma +1 punto y reproduce `explode_sfx`;
     con jumpkick suena `hit_turtles`. Daño al robot: normal −1, especial −3
     (+5 puntos al matarlo).
   - Colisiones enemigo→jugador: `enemyTryHitPlayerBox` + halfWidth de cuerpo.
   - **Los shurikens se pueden destruir atacándolos** — se chequea ANTES del
     golpe al enemigo para que un ataque rompa el proyectil y no pegue a la vez.
   - `ironBallUpdate`, rotación de paleta de chispas (`PAL_setColors` sobre los
     índices 5..8 de PAL2 = animación de paleta gratis, sin frames extra),
     reposición de `sparks2`, HUD, y `continuePoll` por jugador (break si
     alguien pierde definitivamente).
9. **Victoria**: robot GONE && cero enemigos && sin spawns pendientes → voz
   `scream_april` y **outro**: todos miran arriba 1 s, caminan hasta la puerta
   final (1243; P2 desfasado un ancho de sprite para no superponerse), fade →
   `playerPersistSave()` → `SCENE_LEVEL2`.
10. Derrota → `SCENE_GAME_OVER`.
11. Limpieza: libera proyectiles/chispas/HURRY, restaura atributos de texto y
    `bgScrollTbl`.

### Nivel 2 — sistemas previos a `showLevel2`

- **Fondo** (`bg_test`, 440 px = 55 columnas): SÍ entra completo en el plano
  circular de 64 → `bgInit2` lo dibuja ENTERO una vez (tile por tile porque
  rescomp deduplicó y los índices no son secuenciales) y `bgUpdate2` solo
  alimenta la tabla de scroll. La cámara del nivel es **bidireccional**
  (0..120): el único nivel donde se puede caminar hacia atrás.
- **Cápsula del taladro**: sprite `taladro_capsula` anclado a PANTALLA
  (172,51), oculto hasta el stage 1, con depth máximo (detrás de todos) y
  prioridad baja. Anim [0] = 7 frames de emergencia, reproducidos a mano con
  `SPR_setAnimAndFrame` mientras `capsuleShake()` sacude ±8 px con fórmula
  determinista (sin random: el shake debe ser reproducible y suave). Anim [1]
  = puerta abierta, congelada el resto del nivel. Está elevada 4 tiles para
  que Rocksteady aparezca alineado con la puerta.
- **Paleta flash del jefe**: al spawn se precomputan `bossPal` y `flashPal`
  (canales RGB doblados con saturación, índice 0 transparente e índice 1
  blanco para el texto del HUD). Flashea con umbrales 20/10 de HP y ticks
  8/3 — más rápido cuanto más herido. Precomputar evita aritmética por frame.
- **Cutscene de Shredder**: geometría dura-codeada (April worldX=160, lane
  148, `footOffset` 58, depth −128; balanceo con onda triangular
  `((t>>3)&7)-2`). Shredder entra, la toma, la levanta y sale con ella. La
  máquina de estados vive en `cutScene` 1..5 dentro del loop del nivel.

### `showLevel2()` (2931) — pasillo en llamas + Rocksteady

- Arranca con `SPR_initEx(768)`: las tiles de usuario terminan en 666 → región
  de sprites `[672..1439]` (tope físico 773); el pico real por frame es ~700.
  Devuelve 752 al salir.
- **Fases**: 0 = oleada A con cámara clavada en 0; al limpiar, fase 1 = sala
  libre (ida y vuelta); al llegar a `LEVEL2_CAM_MAX_X` (120), fase 2 = cámara
  bloqueada, entra la **oleada B** (dos naranjas de frente + un morado por la
  espalda con voltereta) y arranca la secuencia del jefe (`bossStage` 0..3:
  pausa → cápsula emerge con shake → tapa abre → jefe camina → pelea).
- El jefe es golpeable desde la intro (spawn en `ROCKSTEADY_TALADRO_X`=340).
- El shake de pantalla SOLO se aplica a la cámara de display y a la cápsula —
  jamás a la cámara de gameplay ni al HUD (si no, las colisiones "tiemblan").
- Victoria: `boss.state == ROCKSTEADY_GONE` → cutscene Shredder → fade →
  `SCENE_ENDING`.
- Limpieza: balas/shurikens liberados, texto restaurado, `SPR_initEx(752)`.

### `showEnding()` (3707)
Imagen compuesta en DOS planos (`bg_b_final` en BG_B/PAL0 + `bg_a_final` en
BG_A/PAL1, cuyo índice 0 es transparente para que se vea B debajo). Antes,
`SPR_end` libera TODA la VRAM de sprites: la imagen necesita ~1000 tiles, más
de lo que deja ningún presupuesto de sprites. Ambas paletas se meten en un
array de 32 y se funden juntas: así nunca se ve BG_B "solo". Aguanta 3 s
(START adelanta) con la risa de Shredder a los ~0,5 s, restaura el loop de
música, vuelve `SPR_initEx(600)` y reinicia el juego (`SCENE_SEGA`).

### `showGameOver()` (3769)
"GAME OVER" centrado con la fuente default (blanco puesto a mano en el índice
15 de PAL0, porque `clearScene` dejó la CRAM en negro), 4 segundos, START
adelanta (y espera que lo suelten). Reinicia desde el logo SEGA.

---

## 11. `res/player.h` / `res/player.c`

Multi-jugador desde el diseño: **todas** las funciones reciben `Player*`.
P1 y P2 son instancias totalmente independientes.

### Constantes destacadas (player.h)

| Grupo | Valores | Por qué |
|---|---|---|
| Movimiento | SPEED 2 · lane 142..200 | velocidad clásica de beat'em-up |
| Salto | fuerza 13, gravedad 1, hang 4 ticks en el apex (~84 px) | apex flotante = sensación arcade |
| Jumpkick | soft speed 4 · strong lleva fracción Q16 9/16 extra (~16 px) | el fuerte "viaja" más lejos |
| Combo buffer | COMBO_LINK_WINDOW 10 | encadenar apretando ANTES de terminar el golpe (input buffering, feel moderno) |
| Alcance por arma | Leo 56 · Mike 40 · Don 60 · Raph 38 | katana>bō>nunchaku>sai |
| Herida | invincible 45 SIN blink | blink solo al reaparecer; el arcade tampoco parpadea en hurt |
| KO | 70 frames → revive con 90 de invincible + blink | margen para reacomodarse |
| Knock-down | KD_HOLD 35 · invulnerable 110 · frame KO 11 | secuencia caer→levantar con i-frames |
| Agarre látigo | escapar con 90 de mash (pasos de 18 = 5 botones) | mash = tensión |
| Idle | cambia pose tras 300 frames (~5 s) | detalle de personalidad |
| Vida | MAX_HEALTH 10 · vidasIniciales (default 3) | configurable en OPTIONS |

Anims (filas de la sheet): IDLE, IDLE2, KICK, ATTACK_1..3, JUMP, JUMP_KICK,
WALK_FRONT/BACK, SPECIAL, HIT_1..3, GET_UP_1, HIT_BEHIND_1..2, GET_UP_2,
HELD (18, frame 3 = golpe estando agarrado), WHIP_SHOCK (19). Leo y Mike
tienen sheets rediseñadas de 21 filas; Raph y Don conservan las viejas
(índices corridos aceptados).

### `Player` (struct)
Posición mundo (x borde izq., y pies), `jumpZ/jumpVel`, estado, timers,
`health/lives/score/gameOver`, `comboBuffered/comboBufferTimer`, flags de
ataque (`isSpecialAttack/isJumpKicking`), `hurtToggle` (alterna HIT_1/HIT_2),
`grabTimer/grabType/grabbedByFoot`, `invincibleTimer/blinkTimer`,
`rightBoundX`, `sprite`, `charId`, `dir`, `idleTimer`, `airFrame/airTimer`.

### Funciones (player.c)

- **`initPlayer(Player*, u8 ch, u16 joyId, s16 x, s16 y)`** — elige sheet y
  alcance por personaje (Leo 56 · Mike 40 · Don 60 · Raph 38), resetea todo el
  struct y restaura persistencia del joystick (`persistSlot`).
- **`updatePlayer`** — el corazón (~500 líneas). Por estado:
  - *IDLE/WALKING*: input, flip, clamp de lane y pared diagonal, anims de
    caminar adelante/atrás, pose idle alternativa tras el delay, detección de
    especial (A, o B+C) y arranque de combo.
  - *JUMPING*: física propia (`jumpZ -= vel; vel -= GRAVITY`), hang en el apex
    solo si no hay input horizontal, control aéreo, jumpkick disparado una vez
    por salto con commit de dirección, anim manual en 3 fases (subir, loop en
    apex, anticipación de aterrizaje cuando `jumpZ <= vel<<1`).
  - *ATTACKING*: consume el buffer de combo (encadena ATTACK_2/3 si apretaste
    dentro de la ventana), linger breve al terminar.
  - *HURT*: knockback 10×2 px + duración mínima.
  - *KO*: desliza y al terminar revive (vida llena) o marca gameOver.
  - *KNOCKED_DOWN*: secuencia en fases (caer→piso→levantar) con invulnerabilidad.
  - *GRABBED*: mash reduce `grabTimer`; con soldado reproduce HELD manual
    (loop 0..2) y congela el frame 3 cuando pega.
- **`renderPlayer`** — posiciona con `drawY = y − FOOT_OFFSET − jumpZ − lift`
  (lift 8 px visual durante el especial) y depth `−y`.
- **Ataques**: `isPlayerAttackActive` (anim en curso; el jumpkick vale todo el
  vuelo), `playerAttackHitsBox` (solape `[-ATK_BACK,+reach]` vs cuerpo, tolerancia
  Y simétrica), `playerAttackIsSpecial`, `isPlayerJumping/Kicking`.
- **Daño**: `playerCanBeHit` (regla: saltando = inmune — eludís con el aire;
  agarrado = SÍ te pegan), `playerEnterKO`, `playerTakeHit` (elige HIT_BEHIND
  si te pegan de espaldas según `side != dir`, alterna HIT_1/HIT_2, corta combos).
- **Barras**: `playerHitBars(dmg)` genérico y `...Knockdown` (daño + derribo).
- **Agarres**: `playerReleaseGrab` (idle + i-frames), `playerWhipGrab`
  (WHIP_SHOCK con fallback), `playerFootGrab` (control manual), `playerIsGrabbed`,
  `playerElectroDrain` (drena 1 barra/seg mientras el robot electrocuta).
- **Persistencia**: slots por JOY id (`s_persistLives/s_persistScore`),
  `playerPersistSave` (al completar el nivel) y `playerPersistReset` (nueva
  partida). Así P2 conserva sus vidas/score entre niveles aunque muera.
- **Cutscene**: `playerCutsceneStand/WalkTo/Watch` — IA scriptada para el outro
  y la cutscene de Shredder.

---

## 12. `src/enemy.h` / `src/enemy.c`

Dos tipos que comparten pool y cerebro grupal:
- **Morado** (64×80): melee, flanquea hacia tu espalda y agarra; ataca con
  **combos de 2–3 golpes**.
- **Naranja** (104×104, PAL3): kitea a distancia lanzando shurikens; en melee
  patea/mete uppercut.

### Constantes clave
Pool `MAX_ENEMIES 8`, máx **activos 4**, máx **atacando 2** (los demás
hostigan a ~72 px). Aggro 200 / deaggro 400 px (entran corriendo de lejos y
solo abandonan bien lejos). Cooldown de ataque 60+rand31 frames; tras recibir
golpe, 30 de cooldown (evita "stunlock infinito"). Retargeting cada 32 frames
con histéresis de 48 px (no cambia de objetivo por 1 px de diferencia).
Separación entre enemigos: 32×12 px. Shurikens: pool 4, speed 3, daño 1,
rango 30..180, salen a 16 px del cuerpo. Grab: rango 44, se pega a tu espalda
(+42), se escapa mash-eando o a los 240 frames.

### Funciones

- **`resetEnemyAI(twoPlayers)`** — apaga el pool y reparte objetivos parejo
  entre los jugadores presentes.
- **Spawns**: `initEnemySpawn` (base: sheet/paleta/dimensiones POR TIPO — el
  struct guarda geometría runtime porque conviven tipos de distinto tamaño),
  variantes de entrada: `door` (sale rompiendo la puerta, frame 1 en adelante),
  `elevator` (frames 3-4, ya afuera), `kick` (entra pateando), `somersault`
  (voltereta desde fuera de pantalla, speed 3, 56 ticks). La paleta se
  recarga en cada spawn porque los slots del pool se reciclan entre tipos.
- **Combos del morado**: tablas `ComboStep purpleComboPunch/Kick/Front`
  (punch = frontal,frontal,uppercut; kick = frontal,patada-con-embestida).
  Cada paso define anim, duración, ventana de impacto y avance. Así el jugador
  aprende ritmos (como en el arcade) en vez de golpes sueltos aleatorios.
- **`updateEnemy`** — la máquina grande:
  - DEAD cuenta explosión y libera slot; SPAWNING ejecuta la entrada elegida.
  - Targeting con histéresis; TURN gira con animación propia (no snap).
  - CHASE decide por tipo: morado intenta agarre si ya te flanqueó (chance
    1/4 y con cooldown); naranja dispara si estás a 30..180 px, si no melee.
    Morado normal: uppercut de cerca, patada/frontal al azar.
  - MOVEMENT: el naranja mantiene distancia; el morado persigue un punto
    DETRÁS de ti (`MORADO_BACK_STANDOFF`) y retrocede mientras tiene cooldown
    (hostiga en círculos sin suicidarse).
  - ATTACK: recorre pasos de combo (embestidas incluidas) y al terminar paga
    cooldown; el shuriken sale en el tick 16 de su anim.
  - GRAB: se pega a tu espalda, te inmoviliza (`playerFootGrab`), con timer
    de seguridad para no tragarte eternamente.
  - Idle: alterna guardia/stance cada 120 ticks; camina "hacia arriba"
    (anim walkUp) cuando corresponde.
- **`damageEnemy(e,dmg)`** — si estaba agarrándote, te suelta (rescate);
  corta ataques, aplica cooldown post-hit, y con HP ≤0 entra en explosión.
  Sin knockback: el arcade no empuja a los soldiers.
- **`separateEnemies`** — empuje mutuo 1 px por eje: evita apilamientos feos.
- **`enemyTryHitPlayerBox`** — ventana de impacto por paso de combo (o patada
  con embestida), caja `[-8,+34]` vs medio cuerpo del jugador, tolY 16; marca
  `attackHit` para pegar UNA vez por ataque.
- **Shurikens**: pool con `shurikenSpawn/Update/ReleaseAll`; se destruyen con
  ataques (chequeo en scenes.c ANTES del hit de cuerpo) y dañan con `damagePlayer`.

---

## 13. `src/robot.h` / `src/robot.c` — mini-jefe del látigo

Frame ENORME y NO cuadrado: 184×80; el cuerpo vive a la izquierda (centro
~x28) y el látigo se estira a la derecha. **Por eso `r->x` es el CENTRO del
cuerpo** (única entidad así) y `robotRender` compensa la X al espejar:
`fl = x − (flip ? (184−28) : 28)`. HP 7; especial le saca 3.

Máquina de estados (`RobotState`): INACTIVE → APPEAR (sale del suelo, inmune)
→ WALK (patrulla 1100..1250, arranca con anim [3] y a los 24 ticks pasa a
WALK_LONG [12]) → TURN (gira y se alinea en Y a velocidad 3) → decide:
- **LÁSER** si estás lejos (>120 centro-a-centro): sub-sprite `whip_waves`
  (anim 4) viaja a 6 px/frame y pega 4 barras. Sale en el tick 8 de la anim.
- **LÁTIGO** si estás en rango (50..120 creciendo 8/frame): WINDUP obligatorio
  (telegraph), THROW manual (3 ticks/frame), si te alcanza → GRAB y
  electrocución alternando anims A/B cada 8 ticks y drenando 1 barra/segundo.
  Te sueltas con mash (90 total, pasos de 18).

Tras cualquier ataque: cooldown 45. HURT 14 ticks; DEAD explota (anim 11) y
libera sprite+láser → GONE (condición de victoria junto con cero enemigos).

**Por qué telegraphs tan marcados**: es un mini-boss; el windup largo y el
láser visible son el "juego de leer patrones" del arcade.

---

## 14. `src/rocksteady.h` / `src/rocksteady.c` — jefe final

104×104, PAL3 (su paleta se carga al aparecer; índice 1 blanco para el HUD).
HP 48. Aquí **x = borde izquierdo** (como los soldados) — a diferencia del
robot — porque el frame sí es cuadrado y el espejado no necesita compensación.

**Fases:**
1. **Sin arma**: APPROACH/estampida [2] (speed 6, overshoot 18 tras impactar,
   1 daño por carga) y patada [3]. **Solo** pega la patada como CONTRAATAQUE
   después de 2 golpes seguidos tuyos (`counterPending`): castiga el spam.
   Cada 10 golpes CAE (knock-down, máx 60 en el piso). Fase 2 al perder 24 HP
   o al 4º knock-down.
2. **Con arma**: DRAW [5], AIM_WALK [7] (se alinea a |dy|≤6 y dispara a
   ≤130 px), SHOOT [9] con balas en los frames 3/5/7 (máx 3 por ráfaga); las
   balas van DIAGONAL ARRIBA si estás saltando (te lee, como el arcade). Si te
   arrimas, patada con arma [8].

**Comportamientos extra:**
- **Anger**: 3 golpes recibidos en fase 2 → +1 px/frame en todo (2→3, 6→7).
- **Retreat**: se va al fondo y suelta 3-4 ráfagas alternando tiro recto y
  alto, moviéndose en Y entre ráfagas (anti-camping con estilo).
- **Corner charge**: corre a la esquina (155), guarda el arma y carga 3 veces.
- **Anti-camping general**: 120 frames sin que lo alcances → embiste.
- Armadura durante patadas (no interrumpe flinch) y contador `comboHits` para
  el counter.
- EMERGE: aparece en la puerta elevada (y≈170) y BAJA caminando al lane 180;
  si lo interrumpen de un golpe estando arriba del lane, vuelve a EMERGE-mode
  para bajar (las patadas tuyas no conectan por tolY si se queda arriba).

**Muerte**: anim [4] hasta el último frame → `ROCKSTEADY_GONE` → scenes.c
dispara la cutscene de Shredder. Balas: pool de 9, mismas convenciones que
shurikens (`rocksteadyBulletCheckHitPlayer` devuelve el X de impacto para
efectos).

---

## 15. Recursos (`.res`) y paletas

Reglas duras aprendidas a fuerza de errores:

1. **Comentarios ASCII puro** en `.res`: rescomp los lee como Cp1252 y un
   UTF-8 suelto (`Í`) revienta el build.
2. **`NONE NONE`** para recursos indexados desde ROM por cálculo (fuego, HP
   bar, humo): sin compresión NI dedup, si no los offsets calculados no coinciden.
3. **Nunca dejar filas transparentes enteras en una spritesheet**: rescomp
   SALTA las filas vacías y todos los índices posteriores se corren (ya comió
   una animación entera una vez). Un pixel opaco basta como placeholder.
4. **Tiras horizontales ≠ tiles contiguos por frame**: tile (r,c) del frame N
   está en `r*COLS + N*TILE_W + c`. Solo las tiras verticales permiten un DMA
   único por frame.
5. Fuente arcade: solo ASCII 32..126 (sin acentos).
6. WAVs normalizados (~24% RMS) o no se escuchan sobre la música.

**Mapa de paletas Nivel 1:** PAL0 fondo · PAL1 tortugas+HUD+barra · PAL2
soldados+fuego+robot+chispas · PAL3 soldado naranja+texto HUD (blanco en
PAL3[1]).

**Nivel 2:** igual, con PAL0 compartiendo la cápsula, PAL1 sumando humo y
April, y PAL3 siendo REEMPLAZADA por la paleta del jefe al spawn (forzando
blanco en el índice 1).

---

## 16. Decisiones de diseño y trampas conocidas

- **Presupuestos de sprites por escena**: 752 (default), 420 (intros/menús,
  liberan tiles), 768 (nivel 2, región de sprites [672..1439]), 600 (tras el
  ending, que hizo `SPR_end`). Toda escena debe restaurar el suyo al salir.
- **`HSCROLL_TILE` es global**: activarlo por el fuego obliga a alimentar la
  tabla de scroll de AMBOS planos cada frame (`SCROLL_TILE_ROWS 28`).
- **Prioridad por tile** como herramienta de composición (columnas de humo
  altas para tapar la cápsula).
- **Depth `−y`** en todas las entidades para el Y-sort correcto.
- **i-frames y ventanas**: 45 tras hurt (sin blink), 90 al reaparecer (con
  blink), buffer de combo de 10 frames — el "feel" vive en estos números.
- **Chequeos en orden**: romper shuriken/bala ANTES del golpe de cuerpo; daño
  al jugador respeta estado (salto inmune, agarrado vulnerable).
- **Cámara**: dead-zone líder/rezagada, velocidad máxima, nunca hacia atrás
  (salvo sala libre del nivel 2), locks por zona para dirigir el ritmo de
  oleadas.
- ⚠️ **Bug corregido (histórico)**: en `initPlayer` el `atkReach` se
  sobreescribía con el de Leo después de asignarlo por personaje → las 4
  tortugas pegaban con alcance 56. Se eliminó la línea; cada tortuga usa su
  alcance de arma.

---

## 17. Compilar y probar

No hay Makefile local; se usa el makefile.gen de SGDK:

```powershell
& "$env:GDK\bin\make.exe" -f "$env:GDK\makefile.gen"
```

- `GDK` apunta a la instalación de SGDK (aquí `C:\SGDK`); `make` no está en PATH.
- rescomp regenera los headers de `res/*.h` — no editarlos a mano.
- Probar `out/rom.bin` en BlastEm/Gens.

Más contexto histórico y decisiones sesión a sesión: **DEVLOG.md**.
Datos medidos del ROM original del arcade: **arcade_reverse_eng/**.
