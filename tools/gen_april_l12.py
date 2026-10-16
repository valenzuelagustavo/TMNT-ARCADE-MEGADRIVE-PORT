#!/usr/bin/env python3
# =============================================================================
# gen_april_l12.py (15/10) -- April atada de la sala del 1-2 en PAL2
# =============================================================================
# Fuente (de Gustavo): res/sprites/APRIL PALETA CHAMAS.png (162x105), April
# con la paleta del fuego y los foot soldiers (morado + naranja), que en el 1-2
# es PAL2. Antes usaba la hoja vieja en PAL1 (tortugas) y salia con colores
# cambiados, porque PAL1 cambia segun la tortuga elegida.
#
# Salida: res/sprites/april_l12.png, 64x64 = 2 frames de 32x64 (el mismo
# formato y el mismo lugar de la figura en cada celda que la hoja vieja, asi
# APRIL_FOOT_OFFSET y el balanceo de scenes.c no cambian).
#
# Uso: python3 tools/gen_april_l12.py
# =============================================================================
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SPR = os.path.join(ROOT, 'res', 'sprites')
SRC = os.path.join(SPR, 'APRIL PALETA CHAMAS.png')
DST = os.path.join(SPR, 'april_l12.png')

# (x, y) de la esquina de cada celda de 32x64 en la fuente: la figura queda
# en x 2..30 / 4..29 de la celda y los pies en la fila 63, como antes.
CELLS = [(26, 22), (61, 22)]


def main():
    src = Image.open(SRC)
    a = np.array(src)
    out = np.zeros((64, 64), np.uint8)
    for i, (x, y) in enumerate(CELLS):
        out[:, i * 32:(i + 1) * 32] = a[y:y + 64, x:x + 32]
    # nada de la fuente puede quedar afuera de las celdas
    assert (a != 0).sum() == (out != 0).sum(), 'la figura no entra en las celdas'
    im = Image.fromarray(out, 'P')
    im.putpalette(src.getpalette())
    im.save(DST, transparency=0)
    print('->', os.path.relpath(DST, ROOT))


if __name__ == '__main__':
    main()
