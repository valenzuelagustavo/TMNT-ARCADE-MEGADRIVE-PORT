#!/usr/bin/env python3
# =============================================================================
# add_frozen_rows.py - filas ANIM_FROZEN / ANIM_FROZEN_BREAK en las 4 hojas (25/10)
# =============================================================================
# Entrada: res/sprites/turtle_frosen.png (256x128, celdas de 64x64):
#            fila 0 = 4 cuadros del bloque de hielo (la tortuga congelada),
#            fila 1 = 2 cuadros del hielo ROMPIENDOSE.
#          Tiene su PROPIA paleta (azules/cian).
# Salida:  filas 28 (4 cuadros) y 29 (2 cuadros) de res/sprites/players/
#          {leo,mike,don,raph}_new_13x13.png, celdas de 104x104, centro x=52,
#          pies (borde de abajo del hielo) en y=95. Los pixeles se copian CON
#          LOS INDICES ORIGINALES del hielo (sin remapear): al congelarse, la
#          tortuga pasa a PAL2 y player.c carga ahi la paleta del hielo
#          (pal_frozen, playerSetFreezePalette). En el Technodrome PAL2 es la de
#          los foot soldiers PURPURA/lanza, y en la zona de los tubos (x 1040+)
#          no salen soldiers (level8_1.c), asi que esta libre.
# (26/10) Antes (25/10) se remapeaba a los colores de PAL1; ahora el hielo
# conserva su arte.
# Idempotente: reconstruye siempre las filas 28-29 (exige la 27, demutant).
# gen_players_new.py y add_demutant_row.py van ANTES.
# Uso:  python3 tools/add_frozen_rows.py [--preview carpeta]
# =============================================================================
import os
import sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DIR = os.path.join(ROOT, 'res', 'sprites', 'players')
SRC = os.path.join(ROOT, 'res', 'sprites', 'turtle_frosen.png')
CELL = 104
ROW_FROZEN = 28
ROW_BREAK = 29
FEET_Y = 95
SRC_CELL = 64


def main():
    src = np.array(Image.open(SRC))
    for name in ('leo', 'mike', 'don', 'raph'):
        path = os.path.join(DIR, name + '_new_13x13.png')
        im = Image.open(path)
        rows = im.height // CELL
        assert rows >= ROW_FROZEN, (name, rows)
        a = np.array(im)[:ROW_FROZEN * CELL]
        out = np.zeros((CELL * (ROW_BREAK + 1), a.shape[1]), np.uint8)
        out[:a.shape[0]] = a
        for srow, drow, n in ((0, ROW_FROZEN, 4), (1, ROW_BREAK, 2)):
            for k in range(n):
                f = src[srow * SRC_CELL:(srow + 1) * SRC_CELL, k * SRC_CELL:(k + 1) * SRC_CELL]
                y0 = drow * CELL + FEET_Y - (SRC_CELL - 1)
                x0 = k * CELL + CELL // 2 - SRC_CELL // 2
                out[y0:y0 + SRC_CELL, x0:x0 + SRC_CELL] = f
        res = Image.fromarray(out, 'P')
        res.putpalette(im.getpalette())
        res.save(path, transparency=0, optimize=False)
        print(name, 'filas', ROW_FROZEN, ROW_BREAK, '->', res.size)


if __name__ == '__main__':
    main()
