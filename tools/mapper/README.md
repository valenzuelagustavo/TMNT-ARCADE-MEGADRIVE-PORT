# Mapper SSF (bank switch) — rama `mapper`

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

1. Una vez (y cada vez que se actualice SGDK): `tools\mapper\build_sgdk_lib_ssf.bat`
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
3. Al volver a main: compilar normal con un `clean` antes (los objetos de
   `out\` del build con mapper no sirven sin el flag).

## Donde corre

Emuladores que soporten el mapper SSF con la cabecera "SEGA SSF" (por ejemplo
Genesis Plus GX y BlastEm; Gens viejo probablemente no) y la Mega EverDrive.
Mientras la ROM no pase de 4 MB, la ROM con mapper corre igual en cualquier
lado: los bancos no cambian. Un cartucho fisico necesita una placa con mapper.
