#!/usr/bin/env python3
# =============================================================================
# gen_garage_sign_fly.py -- el cartel "20 MPH" del garage arrancado (14/10)
# =============================================================================
# Al golpearlo, el cartel sale disparado como el parquimetro del 2-1: un frame
# "recibe el golpe" (inclinado) y otro volando (mas inclinado), arrancado de
# raiz. Se arman ROTANDO el cartel parado (frame 0 de garage_sign20.png)
# alrededor del pie del poste, con la punta hacia la IZQUIERDA (como el arte
# del parquimetro: el motor lo espeja cuando sale volando a la derecha).
#
# SALIDA: res/images/lvl_4_garage/garage_sign20_fly.png
#   2 frames de FLY_W x FLY_H, misma paleta que el cartel (PAL0 del garage).
#   El pie del poste queda en (FOOT_X, FLY_H - 1) de cada celda.
# Uso:  python3 tools/gen_garage_sign_fly.py
# =============================================================================
import math
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
DIR = os.path.join(ROOT, 'res', 'images', 'lvl_4_garage')
SRC = os.path.join(DIR, 'garage_sign20.png')
OUT = os.path.join(DIR, 'garage_sign20_fly.png')

CELL_W = 48                  # celda del cartel parado
PIVOT = (13.5, 95.5)         # pie del poste en el frame 0
ANGLES = (22, 50)            # grados: recibe el golpe, volando
FLY_W, FLY_H = 96, 96
FOOT_X = 80                  # x del pie dentro de la celda nueva
ROOT_COL = 3                 # gris oscuro del poste: la punta arrancada


def scale2x(a):
    """Scale2x (EPX): agranda al doble sin inventar colores y redondeando
    las diagonales. Tres pasadas = x8, la base de RotSprite."""
    h, w = a.shape
    p = np.pad(a, 1, mode='edge')
    B, D, E, F, H = p[:-2, 1:-1], p[1:-1, :-2], p[1:-1, 1:-1], p[1:-1, 2:], p[2:, 1:-1]
    e0 = np.where((D == B) & (B != F) & (D != H), D, E)
    e1 = np.where((B == F) & (B != D) & (F != H), F, E)
    e2 = np.where((D == H) & (D != B) & (H != F), D, E)
    e3 = np.where((H == F) & (D != H) & (B != F), F, E)
    out = np.zeros((h * 2, w * 2), a.dtype)
    out[0::2, 0::2], out[0::2, 1::2] = e0, e1
    out[1::2, 0::2], out[1::2, 1::2] = e2, e3
    return out


SCALE = 8


def rotate(src, deg):
    """RotSprite: la imagen x8 con Scale2x y despues rotada al vecino mas
    cercano (no inventa colores). Punta a la izquierda."""
    big = scale2x(scale2x(scale2x(src)))
    h, w = src.shape
    out = np.zeros((FLY_H, FLY_W), np.uint8)
    t = math.radians(deg)
    c, s = math.cos(t), math.sin(t)
    px, py = PIVOT
    for y in range(FLY_H):
        for x in range(FLY_W):
            # punto de destino relativo al pie, girado al reves
            dx, dy = x + 0.5 - FOOT_X, y + 0.5 - FLY_H
            sx = px + dx * c - dy * s
            sy = py + dx * s + dy * c
            ix, iy = int(math.floor(sx * SCALE)), int(math.floor(sy * SCALE))
            if 0 <= ix < w * SCALE and 0 <= iy < h * SCALE:
                out[y, x] = big[iy, ix]
    return out


def main():
    im = Image.open(SRC)
    a = np.array(im)
    f0 = a[:, :CELL_W]
    frames = [rotate(f0, d) for d in ANGLES]
    for f, d in zip(frames, ANGLES):
        assert not f[:, 0].any() and not f[0].any(), 'no entra en la celda (%d)' % d
        # dos pixeles del poste arrancado al pie
        f[FLY_H - 1, FOOT_X:FOOT_X + 2] = ROOT_COL
    sheet = np.hstack(frames)
    o = Image.fromarray(sheet, 'P')
    o.putpalette(im.getpalette())
    o.save(OUT, transparency=0)
    print(os.path.relpath(OUT, ROOT), sheet.shape[1], 'x', sheet.shape[0])


if __name__ == '__main__':
    main()
