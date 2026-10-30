#!/usr/bin/env python3
# =============================================================================
# add_electro_row.py - fila ANIM_ELECTRO en las hojas de las 4 tortugas (30/10)
# =============================================================================
# Entrada: res/sprites/turtle_electro.png (2 cuadros de 96x72, mismos indices de
#          color que las hojas; lo arma tools/gen_turtle_electro.py. Para poner
#          los definitivos basta reemplazarlo, con cuadros de 96x72).
# Salida:  agrega a res/sprites/players/{leo,mike,don,raph}_new_13x13.png la FILA
#          30 (ANIM_ELECTRO de res/player.h) con esos cuadros en celdas de
#          104x104 (centro x=52, borde de abajo del cuerpo en y=95, como el resto
#          de las hojas). Es la misma imagen para las cuatro.
# Idempotente: reconstruye siempre la fila 30 (exige las 30 anteriores:
# gen_players_new.py, add_demutant_row.py y add_frozen_rows.py van ANTES).
# Uso:  python3 tools/add_electro_row.py
# =============================================================================
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DIR = os.path.join(ROOT, 'res', 'sprites', 'players')
SRC = os.path.join(ROOT, 'res', 'sprites', 'turtle_electro.png')
CELL = 104
ROW = 30          # ANIM_ELECTRO
FEET_Y = 95
SRC_W, SRC_H = 96, 72


def main():
    src = np.array(Image.open(SRC))
    assert src.shape[0] == SRC_H and src.shape[1] % SRC_W == 0, src.shape
    n = src.shape[1] // SRC_W
    for name in ('leo', 'mike', 'don', 'raph'):
        path = os.path.join(DIR, name + '_new_13x13.png')
        im = Image.open(path)
        rows = im.height // CELL
        assert rows >= ROW, (name, rows)
        a = np.array(im)[:ROW * CELL]
        out = np.zeros((CELL * (ROW + 1), a.shape[1]), np.uint8)
        out[:a.shape[0]] = a
        for k in range(n):
            f = src[:, k * SRC_W:(k + 1) * SRC_W]
            y0 = ROW * CELL + FEET_Y - (SRC_H - 1)
            x0 = k * CELL + CELL // 2 - SRC_W // 2
            out[y0:y0 + SRC_H, x0:x0 + SRC_W] = f
        res = Image.fromarray(out, 'P')
        res.putpalette(im.getpalette())
        res.save(path, transparency=0, optimize=False)
        print(name, 'fila', ROW, '->', res.size)


if __name__ == '__main__':
    main()
