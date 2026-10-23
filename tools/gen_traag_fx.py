#!/usr/bin/env python3
# =============================================================================
# gen_traag_fx.py -- (21/10) retoques del arte de Traag (jefe del Technodrome)
# =============================================================================
# 1. traag_boss.png: borra pixeles sueltos (1 a 3 px, a 10 px o mas del
#    cuerpo) que quedaron del recorte. Con el sprite espejado aparecian del
#    otro lado del cuerpo (culatazo, disparo, caminata).
#    El original queda en res/sprites/orig/traag_boss_antes_limpieza.png.
# (La explosion del misil usa baxter_boom tal cual, con su paleta en PAL2:
# ya no hace falta la copia en la paleta de Traag.)
# Uso: python3 tools/gen_traag_fx.py
# =============================================================================
import os
import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
SPR = os.path.join(os.path.dirname(HERE), 'res', 'sprites')
CELL = 144


def clean_boss():
    src = os.path.join(SPR, 'orig', 'traag_boss_antes_limpieza.png')
    im = Image.open(src)
    a = np.array(im)
    removed = 0
    for r in range(a.shape[0] // CELL):
        for c in range(a.shape[1] // CELL):
            cell = a[r * CELL:(r + 1) * CELL, c * CELL:(c + 1) * CELL]
            lab, n = ndimage.label(cell != 0, structure=np.ones((3, 3)))
            if n <= 1:
                continue
            sizes = ndimage.sum(cell != 0, lab, range(1, n + 1))
            big = int(np.argmax(sizes)) + 1
            dist = ndimage.distance_transform_edt(lab != big)
            for k in range(1, n + 1):
                m = lab == k
                if k != big and sizes[k - 1] <= 3 and dist[m].min() >= 10:
                    cell[m] = 0
                    removed += 1
    o = Image.fromarray(a, 'P')
    o.putpalette(im.getpalette())
    o.save(os.path.join(SPR, 'traag_boss.png'), transparency=0)
    print('traag_boss: %d pixeles sueltos borrados' % removed)
    return im.getpalette()[:48]


if __name__ == '__main__':
    clean_boss()
