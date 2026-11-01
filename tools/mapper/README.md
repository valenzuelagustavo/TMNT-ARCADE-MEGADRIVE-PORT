# Ediciones FULL / LITE y mapper SSF (bank switch)

**Una sola base de codigo (rama `main`), dos ediciones** (01/11; antes eran dos ramas, `main` y `mapper`):

| Edicion | Script | Cabecera | Para quien |
| --- | --- | --- | --- |
| **LITE** (sin mapper, hasta 4 MB) | `tools\mapper\build_rom_plain.bat` | `SEGA MEGA DRIVE`, fin 0x3FFFFF | flash carts viejos (Everdrive MD v2.6 y anteriores, chinos de 4 MB), cartuchos sin mapper, y cualquier emulador. **Descarga recomendada mientras entre en 4 MB.** |
| **FULL** (mapper SSF, puede pasar de 4 MB) | `tools\mapper\build_rom_ssf.bat` | `SEGA SSF`, fin 0x7FFFFF | emuladores con SSF, Mega EverDrive PRO y similares |

La edicion sale del flag de SGDK `ENABLE_BANK_SWITCH` (los scripts lo ponen) y `src/build_config.h` la
expone al codigo como `TMNT_FULL` / `TMNT_LITE` (y `TMNT_EDITION`). Hoy el contenido es el mismo en las
dos. Cuando la ROM pase de 4 MB, lo "extra" que no entre se envuelve en `#if TMNT_FULL ... #endif` y sus
recursos van en `.res` propios que LITE no incluye. LITE muestra `LITE` en el menu de titulo (para los
reportes de bugs). Cada script termina con `check_edition.py`, que comprueba la cabecera (y que LITE
entre en 4 MB) y deja `out\tmnt_megadrive_port_<version>_full.bin` / `_lite.bin`: esa copia es la que se publica.

## Mapper SSF (bank switch)

Para que la ROM pueda pasar de 4 MB se usa el mapper oficial de Sega (el de
Super Street Fighter II) que trae SGDK (`ENABLE_BANK_SWITCH`).

## Como reparte la ROM SGDK

- Los primeros **3 MB** (0x000000-0x2FFFFF) se ven siempre igual: ahi van el
  codigo y todo lo que se lee DIRECTO de la ROM.
- Las regiones 6 y 7 (0x300000-0x3FFFFF) son las **ventanas** que SGDK cambia
  para leer los datos "FAR" (todo lo que este despues de los 3 MB). Las
  funciones de SGDK (motor de sprites, `VDP_loadTileSet`, `VDP_drawImageEx`,
  `VDP_setTileMapEx`...) lo hacen solas con `FAR_SAFE`.

Lo que NO puede ir mas alla de los 3 MB (va en archivos `.res` con `NEAR`):

- **Audio** (`audio.res`): el Z80 lee la musica y los samples de la ROM
  mientras suenan; si SGDK cambia una ventana en el medio, se escucha basura.
- **Lo que el codigo lee crudo**: fondos streameados (`stage_bg`), tiles que
  se suben cuadro por cuadro (fuego, humo, barra de vida, Splinter, secciones
  del 2-1, capsulas, chispas), mapas leidos celda por celda (2-1, intro,
  ascensor, sala final).
- **Sprites sin comprimir que se suben con DMA en cola**: la cola se ejecuta
  en el VBlank y para entonces la ventana puede apuntar a otro banco. Por eso
  en los archivos FAR todos los sprites van comprimidos (`FAST`): se
  descomprimen en RAM en el momento.

| Archivo | Reparto | Por que |
| --- | --- | --- |
| audio, level1, level2, level2_1, level3_1, level4_1, level5_1, level6_1, level7_1, level8_1, level9_1, level_snow, props_2_1, intro_tmnt, roof_april | NEAR | lectura directa / Z80 |
| chars, enemies, final_bosses, granitor_res, traag_res, baxter_res, profiles, intro, menus, credits, cutscenes, idle_missile_res, roof_april_far, level_snow_far, level4_1_far, level8_1_far | FAR | solo funciones de SGDK, sprites comprimidos |

Sprites que quedan SIN comprimir a proposito (el codigo sube sus tiles a mano
o con `DMA_QUEUE`): `factory_capsule`, `taladro_capsula`, `sparks`,
`spark_ascensor`, `fw_moto`, `sk_cop_body`, `sk_cop_rotor`, `snow_flake_spr`.

## Reglas para lo nuevo

- Un recurso que el codigo lea por puntero (`->tiles`, `->tilemap->tilemap`,
  un `BIN`) o que suene → a un `.res` NEAR.
- Un sprite o una imagen que solo se cargan con funciones de SGDK → puede ir
  a un `.res` FAR (por ejemplo `levelX_far.res`, como `roof_april_far.res`).
  Si es un sprite, comprimido (`FAST`).
- Nada de `VDP_loadTileSet(..., DMA_QUEUE)` con datos FAR sin comprimir.
- Lo NEAR tiene que entrar en 3 MB: `check_mapper.py` lo controla en cada
  build.

## Compilar

(LITE: solo `tools\mapper\build_rom_plain.bat`, no necesita nada mas.)

1. Una vez (FULL) (y cada vez que se actualice SGDK): `tools\mapper\build_sgdk_lib_ssf.bat`
   arma `C:\SGDK\lib_ssf\libmd.a`. La libreria normal no se toca.
   Necesita que `C:\SGDK\inc\config.h` acepte el flag desde afuera:
   ```c
   #ifndef ENABLE_BANK_SWITCH
   #define ENABLE_BANK_SWITCH      0
   #endif
   ```
   (cambio hecho el 23/10; sin el flag queda en 0 como siempre).
2. La ROM: `tools\mapper\build_rom_ssf.bat` (hace `clean`, compila con
   `-DENABLE_BANK_SWITCH=1` y corre `check_mapper.py`). La cabecera sale como
   `SEGA SSF` (src/boot/rom_head.c).
3. Para cambiar de edicion no hace falta cambiar de rama: los dos scripts hacen `clean`
   antes (los objetos de `out\` de una edicion no sirven en la otra).

## Donde corre

Emuladores que soporten el mapper SSF con la cabecera "SEGA SSF" (por ejemplo
Genesis Plus GX y BlastEm; Gens viejo probablemente no) y la Mega EverDrive.
Un cartucho fisico necesita una placa con mapper.

**Ojo (01/11, reporte de la comunidad con la 0.4.0):** la idea de que "mientras
la ROM no pase de 4 MB, la ROM con mapper corre igual en cualquier lado" NO se
cumplio en hardware real:

| Flash cart | Con mapper (`build_rom_ssf.bat`) | Sin mapper (`build_rom_plain.bat`) |
| --- | --- | --- |
| Mega EverDrive PRO (firmware ultimo) | arranca | arranca |
| Everdrive MD v2.6 | NO arranca | arranca |
| Everdrive chino antiguo (solo hasta 4 MB), MD1 PAL | NO arranca | (esperado: arranca, sin confirmar) |

El v2.6 dice soportar SSF2 pero en la practica es parcial (la cabecera
`SEGA SSF` y/o las escrituras a 0xA130F1-FF al arrancar). Como la ROM mide menos
de 4 MB (3,87 MB), el mapper no hace falta: **mientras entre en 4 MB, publicar
la ROM sin mapper** (`tools\mapper\build_rom_plain.bat`, cabecera `SEGA MEGA
DRIVE`, fin de ROM 0x3FFFFF) como descarga principal. El mapper solo vale la
pena si la ROM pasa de 4 MB, y ahi habra que ver como lo resolvio Mr. Todd X
(un Shinobi beta de 5 MB que si anda con SSF2 en el Everdrive MD viejo).
