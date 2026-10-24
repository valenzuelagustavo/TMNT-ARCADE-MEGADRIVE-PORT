# -*- coding: utf-8 -*-
"""
gen_snow_props.py (23/10) -- utileria de Central Park (3-1)
==========================================================
"tampa bueiro_lixeira.png" (del artista, en la paleta del nivel) trae tres
dibujos sueltos: el TACHO de basura, la BOCA de alcantarilla abierta y la
TAPA. Se recortan y se centran en celdas de tiles enteros:

    snow_trash.png  48x56  el tacho, apoyado en el borde de abajo
    snow_hole.png   40x24  la boca abierta (queda despues de que sale el soldier)
    snow_lid.png    32x24  la tapa cerrada (antes de que salga)

Misma paleta que el fondo (bg_snow_md.png, PAL0); el indice 0 es transparente.
Uso:  python3 tools/gen_snow_props.py
"""
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = os.path.join(ROOT, 'res', 'images', 'lvl_central_park')
SRC = 'tampa bueiro_lixeira.png'
SWATCH_ROWS = 10          # la tira de colores de arriba del PNG

# (nombre, x0, x1 de la columna del dibujo en la hoja, celda w, h, apoyado abajo)
ITEMS = [
    ('snow_trash.png', 0, 54, 48, 56, True),
    ('snow_hole.png', 55, 96, 40, 24, False),
    ('snow_lid.png', 97, 139, 32, 24, False),
]


def main():
    src = Image.open(os.path.join(D, SRC))
    a = np.array(src)
    a[:SWATCH_ROWS] = 0
    pal = Image.open(os.path.join(D, 'bg_snow_md.png')).getpalette()[:48]
    for name, x0, x1, cw, ch, bottom in ITEMS:
        part = a[:, x0:x1]
        ys, xs = np.nonzero(part)
        part = part[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
        h, w = part.shape
        assert w <= cw and h <= ch, (name, w, h)
        out = np.zeros((ch, cw), np.uint8)
        ox = (cw - w) // 2
        oy = (ch - h) if bottom else (ch - h) // 2
        out[oy:oy + h, ox:ox + w] = part
        im = Image.fromarray(out, 'P')
        im.putpalette(pal)
        im.save(os.path.join(D, name), transparency=0)
        print('%-15s %dx%d (dibujo %dx%d)' % (name, cw, ch, w, h))


if __name__ == '__main__':
    main()
