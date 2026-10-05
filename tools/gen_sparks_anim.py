# -*- coding: utf-8 -*-
"""
gen_sparks_anim.py - (24/09) Convierte las tiras viejas *_strip.png (4 frames
APILADOS y con los tiles reordenados "por columna") a *_anim.png: los mismos
4 frames, en orden de lectura normal y puestos EN FILA (1 animacion x 4
frames), para declararlos como recurso SPRITE en level1.res.

Por que: el reorden por columna de gen_sparks_frames.py solo es correcto si el
frame entra en UN solo sprite de hardware (4x4 tiles como maximo). Es el caso
de sparks (puertas, 4x4) y por eso esas se veian bien. spark_ascensor (5x3) y
sparks_2 (8x5) no entran: rescomp las parte en varios sprites de hardware y
ademas descarta los tiles vacios (rescomp real: spark_ascensor = 2 sprites /
13 tiles, sparks_2 = 3 sprites / 35 tiles, no 15 y 40). El juego streameaba
15 y 40 tiles en un orden que no coincidia con ese armado -> se veian rotas.

Ahora la tira ES el recurso SPRITE, asi que el orden y la cantidad de tiles de
cada frame los decide el propio rescomp, y el codigo streamea
frames[f]->tileset (el layout de sprites de hardware sale del frame 0 y es el
mismo en los 4: solo cambian indices de color, nunca la transparencia).

Los colores salen tal cual de las tiras actuales (ya reindexadas a la paleta
unica de PAL2 del 24/09). gen_sparks_frames.py queda como historia: su
rotacion de indices 5-8 corresponde a la paleta VIEJA.
"""
from PIL import Image
import os, sys

BASE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(__file__), '..', 'res', 'sprites')
TILE = 8

def unreorder(fr):
    """Inversa de gen_sparks_frames.reorder_for_sprite: el tile que quedo en
    la posicion raster k es el tile original (k // H, k % H)."""
    w, h = fr.size
    W, H = w // TILE, h // TILE
    out = Image.new('P', (w, h)); out.putpalette(fr.getpalette())
    for k in range(W * H):
        src = fr.crop(((k % W) * TILE, (k // W) * TILE,
                       (k % W + 1) * TILE, (k // W + 1) * TILE))
        out.paste(src, ((k // H) * TILE, (k % H) * TILE))
    return out

for name in ('sparks', 'sparks_2', 'spark_ascensor'):
    strip = Image.open(os.path.join(BASE, f'{name}_strip.png'))
    base = Image.open(os.path.join(BASE, f'{name}.png'))
    assert strip.mode == 'P' and base.mode == 'P'
    w, fh = base.size
    assert strip.size[0] == w and strip.size[1] % fh == 0, name
    nfr = strip.size[1] // fh
    # El frame 0 de la tira es el PNG base. Si coincide tal cual, la tira ya
    # esta en orden raster (caso de spark_ascensor_strip.png, que se
    # redibujo a mano con 3 frames); si no, viene de gen_sparks_frames.py y
    # hay que deshacerle el reorden por columna.
    raw0 = strip.crop((0, 0, w, fh))
    raster = list(raw0.getdata()) == list(base.getdata())
    if not raster:
        assert list(unreorder(raw0).getdata()) == list(base.getdata()), name
    anim = Image.new('P', (w * nfr, fh)); anim.putpalette(strip.getpalette())
    for f in range(nfr):
        fr = strip.crop((0, f * fh, w, (f + 1) * fh))
        anim.paste(fr if raster else unreorder(fr), (f * w, 0))
    anim.info['transparency'] = 0
    out = os.path.join(BASE, f'{name}_anim.png')
    anim.save(out)
    print('wrote', out, anim.size, nfr, 'frames', 'raster' if raster else 'des-reordenada')
