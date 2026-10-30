# -*- coding: utf-8 -*-
"""
gen_van_overlay.py (30/10) -- el techo y el paragolpes de la Van estrellada
===========================================================================
Fuente: res/images/lvl_7_factory/VAN_VERDE FACTORY.png (105x224, indexada con
la paleta de las tortugas; solo trae los verdes: indices 2, 5 y 8). Se pone
como SPRITES encima del fondo de la fabrica (bg_factory, que es gris/amarillo
y no tiene verde), alineada con su esquina (0,0): lo comprobado superponiendo
el PNG sobre bg_factory.png coincide con la captura que paso Gustavo.

La imagen entera es casi vacia (80x113 px de dibujo en 105x224), asi que se
parte en las dos piezas que tiene y cada una se recorta a su caja:
    van_roof_gen.png     64x32  (8x4 tiles)  el techo, en el mundo (0, 0)
    van_bumper_gen.png   64x64  (8x8 tiles)  el paragolpes, en el mundo (16, 49)
Misma paleta que el original (PAL1 en el juego, la de las tortugas).
Uso: python3 tools/gen_van_overlay.py
"""
import os

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
DIR = os.path.join(HERE, '..', 'res', 'images', 'lvl_7_factory')
SRC = os.path.join(DIR, 'VAN_VERDE FACTORY.png')

# (archivo, x, y, ancho, alto) en el dibujo original = posicion en el mundo
PIECES = [
    ('van_roof_gen.png', 0, 0, 64, 32),
    ('van_bumper_gen.png', 16, 49, 64, 64),
]


def main():
    im = Image.open(SRC)
    assert im.mode == 'P', im.mode
    a = np.array(im)
    covered = np.zeros(a.shape, bool)
    for name, x, y, w, h in PIECES:
        piece = a[y:y + h, x:x + w]
        covered[y:y + h, x:x + w] = True
        out = Image.fromarray(piece, 'P')
        out.putpalette(im.getpalette())
        out.save(os.path.join(DIR, name), transparency=0)
        print('%-20s %dx%d  mundo (%d,%d)  %d px' % (name, w, h, x, y, (piece != 0).sum()))
    assert not ((a != 0) & ~covered).any(), 'quedan pixeles fuera de las piezas'


if __name__ == '__main__':
    main()
