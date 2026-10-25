#!/usr/bin/env python3
# =============================================================================
# add_demutant_row.py - fila ANIM_DEMUTANT en las hojas de las 4 tortugas (25/10)
# =============================================================================
# Entrada: res/sprites/turtle_demutant.png  (6 cuadros de 42x64: la tortuga
#          vuelve a ser una tortuga comun; el arte usa los mismos indices de
#          color que las hojas: 2, 4, 5 y 8).
# Salida:  agrega a res/sprites/players/{leo,mike,don,raph}_new_13x13.png una
#          FILA nueva (la 27, ANIM_DEMUTANT de res/player.h) con esos cuadros
#          centrados en celdas de 104x104 (centro x=52, pies en y=95, como el
#          resto de las hojas). Es la misma imagen para las cuatro.
# Es idempotente: si la hoja ya tiene 28 filas, no hace nada. gen_players_new.py
# regenera las hojas SIN esta fila: despues de correrlo, correr este script.
# Uso:  python3 tools/add_demutant_row.py
# =============================================================================
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DIR = os.path.join(ROOT, 'res', 'sprites', 'players')
SRC = os.path.join(ROOT, 'res', 'sprites', 'turtle_demutant.png')
CELL = 104
ROW = 27          # ANIM_DEMUTANT
FEET_Y = 95       # ultima fila de pixeles del cuerpo dentro de la celda
SRC_W = 42


def main():
    src = np.array(Image.open(SRC))
    h, w = src.shape
    n = w // SRC_W
    for name in ('leo', 'mike', 'don', 'raph'):
        path = os.path.join(DIR, name + '_new_13x13.png')
        im = Image.open(path)
        rows = im.height // CELL
        if rows > ROW:
            print(name, 'ya tiene la fila', ROW)
            continue
        assert rows == ROW, (name, rows)
        a = np.array(im)
        out = np.zeros((CELL * (ROW + 1), a.shape[1]), np.uint8)
        out[:a.shape[0]] = a
        for k in range(n):
            f = src[:, k * SRC_W:(k + 1) * SRC_W]
            y0 = ROW * CELL + FEET_Y - (h - 1)
            x0 = k * CELL + CELL // 2 - SRC_W // 2
            out[y0:y0 + h, x0:x0 + SRC_W] = f
        res = Image.fromarray(out, 'P')
        res.putpalette(im.getpalette())
        res.save(path, transparency=0, optimize=False)
        print(name, 'fila', ROW, '->', res.size)


if __name__ == '__main__':
    main()
