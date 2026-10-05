# -*- coding: utf-8 -*-
"""
gen_sparks_frames.py - Genera las tiras de 4 frames de sparks.png,
sparks_2.png y spark_ascensor.png para animarlas por STREAMING de tiles en
vez de por rotacion de paleta.

Por que: las tres (chispas de puerta, ascensor y piso) fingian su animacion
rotando los indices 5-8 de PAL2 en CRAM cada pocos ticks (sparksPalAnim en
scenes.c). El problema: el fuego de primer plano
(fire_strip.png) usa ESOS MISMOS indices para su propio dibujo (verificado
con PIL: fire_strip usa {5,6,8}, exactamente el mismo conjunto que las
chispas) -- no hay forma de rotar solo las chispas sin que el fuego de fondo
tiemble tambien, y no hay una 5ta linea de paleta libre en el nivel (las 4
ya estan repartidas: PAL0 fondo, PAL1 tortugas, PAL2 foot soldier + fuego +
chispas + el robot final, PAL3 foot soldier naranja).

La solucion: como sparksPalAnim son 4 filas que son ROTACIONES una de la
otra (fila f = rotar la fila 0 en f lugares), el mismo efecto visual se
puede lograr SIN tocar CRAM: en vez de rotar a que color apunta cada indice,
rotamos que indice tiene cada PIXEL. Pixel con indice original i (5..8) pasa
a tener, en el frame f, el indice 5 + ((i - 5 + f) % 4). Con la paleta fija
en los valores de "frame 0" de siempre, el resultado es IDENTICO pixel a
pixel al que ya se veia -- solo que ahora son 4 frames de tile REALES
(como fire_strip.png/smoke_lvl1.png), streameados con VDP_loadTileData en
vez de con PAL_setColors.

BUG encontrado en el primer intento (en las capturas: las chispas y el
decorado de piso se veian "en bloques", no como fuego): un recurso TILESET
(NONE NONE) exporta los tiles en orden de LECTURA normal (fila por fila),
pero un recurso SPRITE los exporta en el orden que espera el HARDWARE de
sprites de la Genesis: por COLUMNAS (de arriba a abajo, despues la columna
siguiente). fire_tiles/smoke_tiles no sufren esto porque son fondo: nosotros
mismos plantamos cada tile en el tilemap con VDP_fillTileMapRectInc, así que
el orden no importa. Pero acá streameamos los tiles DIRECTO al bloque de
VRAM que lee un SPRITE (via SPR_addSpriteEx apuntando a un indice fijo), y
un TILESET comun no los deja en el orden que ese sprite necesita -- de ahi
el aspecto "roto".
Confirmado compilando un caso de prueba (grilla 3x2 con un indice de color
distinto por tile) como SPRITE y como TILESET con el rescomp real: el
TILESET salio en orden 0,1,2,3,4,5 (por fila) y el SPRITE en 0,3,1,4,2,5
(por columna) -- exactamente la permutacion que este script aplica ahora
antes de armar la tira.
"""
from PIL import Image
import os

BASE = os.path.join(os.path.dirname(__file__), '..', 'res', 'sprites')
FRAMES = 4
ROT_IDX = (5, 6, 7, 8)   # los mismos que rotaba sparksPalAnim
TILE = 8

def remap_frame(im, f):
    """Copia im con cada pixel de indice i en ROT_IDX movido a
    5 + ((i-5+f) % 4)."""
    out = im.copy()
    px_in = im.load()
    px_out = out.load()
    w, h = im.size
    lut = {i: 5 + ((i - 5 + f) % 4) for i in ROT_IDX}
    for y in range(h):
        for x in range(w):
            v = px_in[x, y]
            if v in lut:
                px_out[x, y] = lut[v]
    return out

def reorder_for_sprite(im):
    """Reordena los tiles 8x8 de una imagen WxH tiles (fila por fila, como
    los lee un TILESET) al orden COLUMNA por columna que espera un SPRITE
    de hardware (ver nota de arriba). out_raster[k] = in_colmajor[k]:
    para cada k, el tile de salida en (k % W, k // W) es el tile de
    entrada en (k // H, k % H)."""
    w, h = im.size
    W, H = w // TILE, h // TILE
    out = Image.new('P', (w, h))
    out.putpalette(im.getpalette())
    for k in range(W * H):
        oc, orow = k % W, k // W
        c, r = k // H, k % H
        block = im.crop((c * TILE, r * TILE, (c + 1) * TILE, (r + 1) * TILE))
        out.paste(block, (oc * TILE, orow * TILE))
    return out

def gen(name):
    src_path = os.path.join(BASE, f'{name}.png')
    im = Image.open(src_path)
    assert im.mode == 'P', f'{name}.png debe ser PNG indexado'
    w, h = im.size
    assert w % TILE == 0 and h % TILE == 0, f'{name}.png no es multiplo de 8px'
    used = set(im.getdata())
    stray = used - set(range(16))
    assert not stray, f'{name}.png usa indices fuera de 0-15: {stray}'

    strip = Image.new('P', (w, h * FRAMES))
    strip.putpalette(im.getpalette())
    for f in range(FRAMES):
        frame = reorder_for_sprite(remap_frame(im, f))
        strip.paste(frame, (0, f * h))

    out_path = os.path.join(BASE, f'{name}_strip.png')
    strip.save(out_path)
    print('wrote', out_path, strip.size)

for name in ('sparks', 'sparks_2', 'spark_ascensor'):
    gen(name)
