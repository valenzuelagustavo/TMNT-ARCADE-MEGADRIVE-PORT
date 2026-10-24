# -*- coding: utf-8 -*-
"""
gen_snow_boss.py (23/10) -- el jefe de Central Park (TORA) y su utileria
=======================================================================
Arte del artista (res/images/lvl_central_park/, la MISMA paleta en los tres:
va en PAL3):

  Tora_Boss.png  filas: 0 camina (4), 1 espera la esfera / la lanza (2),
                 2 golpe cuerpo a cuerpo (2), 3 recibe un golpe (1),
                 4 muere (3). Mira a la DERECHA.
  ice_ball.png   la esfera de hielo (0), se rompe (1-2) y su sombra.
  DISCO.png      la esfera que flota al final del nivel (3) y su sombra
                 (la fila de arriba es la muestra de colores).

Los cuadros vienen sueltos: se separan por los huecos y se pegan en grillas
parejas. Tora se ancla por el TORSO (mediana de los pixeles de la franja del
medio del cuerpo) en x 52 de la celda y por los pies en el borde de abajo.

Salidas (res/images/lvl_central_park/):
  snow_tora.png       celdas 104x104, una fila por animacion
  snow_iceball.png    80x72: 0 esfera, 1-2 se rompe (centrada)
  snow_iceshadow.png  56x24: la sombra (la que avisa donde cae)
  snow_disco.png      64x64: 3 cuadros
  snow_disco_sh.png   56x16: su sombra

Uso:  python3 tools/gen_snow_boss.py
"""
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = os.path.join(ROOT, 'res', 'images', 'lvl_central_park')


def segment(a, y_from=0, gap=3, minpx=30):
    """Filas de cuadros: lista de filas, cada una lista de (x0, x1, y0, y1)."""
    m = a != 0
    m[:y_from] = False
    rows = m.any(1)
    bands, y, H = [], 0, a.shape[0]
    while y < H:
        if rows[y]:
            y0 = y
            while y < H and rows[y]:
                y += 1
            if bands and y0 - bands[-1][1] <= gap:
                bands[-1] = (bands[-1][0], y)
            else:
                bands.append((y0, y))
        y += 1
    out = []
    for y0, y1 in bands:
        cols = m[y0:y1].any(0)
        segs, x, W = [], 0, a.shape[1]
        while x < W:
            if cols[x]:
                x0 = x
                while x < W and cols[x]:
                    x += 1
                if segs and x0 - segs[-1][1] <= gap:
                    segs[-1] = (segs[-1][0], x)
                else:
                    segs.append((x0, x))
            x += 1
        fr = []
        for x0, x1 in segs:
            sub = m[y0:y1, x0:x1]
            if sub.sum() < minpx:
                continue
            ys = np.nonzero(sub.any(1))[0]
            fr.append((x0, x1, y0 + ys.min(), y0 + ys.max() + 1))
        out.append(fr)
    return out


def save(grid, pal, name):
    im = Image.fromarray(grid, 'P')
    im.putpalette(pal)
    im.save(os.path.join(D, name), transparency=0)
    print('%-20s %dx%d' % (name, im.size[0], im.size[1]))


def paste(cell, part, ax, ay):
    """Pega 'part' (indices, 0 transparente) con su punto (0,0) en (ax, ay)."""
    h, w = part.shape
    ch, cw = cell.shape
    assert 0 <= ax and ax + w <= cw and 0 <= ay and ay + h <= ch, (ax, ay, w, h, cw, ch)
    dst = cell[ay:ay + h, ax:ax + w]
    dst[part != 0] = part[part != 0]


def tora():
    src = Image.open(os.path.join(D, 'Tora_Boss.png'))
    a = np.array(src)
    rows = segment(a)
    assert [len(r) for r in rows] == [4, 2, 2, 1, 3], [len(r) for r in rows]
    CW, CH, AX = 104, 104, 52
    grid = np.zeros((CH * len(rows), CW * 4), np.uint8)
    for r, row in enumerate(rows):
        for c, (x0, x1, y0, y1) in enumerate(row):
            part = a[y0:y1, x0:x1]
            h = y1 - y0
            band = (part[int(h * 0.35):int(h * 0.65)] != 0)
            xs = np.nonzero(band)[1]
            ax = int(np.median(xs)) if len(xs) else (x1 - x0) // 2
            cell = grid[r * CH:(r + 1) * CH, c * CW:(c + 1) * CW]
            paste(cell, part, AX - ax, CH - h)
    save(grid, src.getpalette()[:48], 'snow_tora.png')


def soft_shadow(W, H, rx, ry):
    sh = np.zeros((H, W), np.uint8)
    cx, cy = (W - 1) / 2.0, (H - 1) / 2.0
    for y in range(H):
        for x in range(W):
            d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
            if d < 0.45:
                sh[y, x] = 2
            elif d < 0.72:
                sh[y, x] = 3 if (d > 0.6 and (x + y) & 1) else 2 if d < 0.6 else 3
            elif d < 0.92:
                sh[y, x] = 4 if (d > 0.84 and (x + y) & 1) else 3
            elif d < 1.0:
                sh[y, x] = 4
            elif d < 1.15 and (x + y) & 1:
                sh[y, x] = 4
    return sh


def iceball():
    src = Image.open(os.path.join(D, 'ice_ball.png'))
    a = np.array(src)
    fr = segment(a)[0]
    assert len(fr) == 4, len(fr)
    CW, CH = 80, 72
    grid = np.zeros((CH, CW * 3), np.uint8)
    for c, (x0, x1, y0, y1) in enumerate(fr[:3]):
        part = a[y0:y1, x0:x1]
        h, w = part.shape
        paste(grid[:, c * CW:(c + 1) * CW], part, (CW - w) // 2, (CH - h) // 2)
    save(grid, src.getpalette()[:48], 'snow_iceball.png')
    # (23/10) La sombra del arte (fr[3]) esta escalada x3 y tiene bordes en
    # escalon: se dibuja una elipse suave del mismo tamano, con degrade de
    # grises de la paleta (2 oscuro, 3 medio, 4 claro) y el ultimo anillo
    # tramado contra lo transparente (no hay transparencia parcial).
    save(soft_shadow(56, 24, 27.5, 9.0), src.getpalette()[:48], 'snow_iceshadow.png')


def disco():
    src = Image.open(os.path.join(D, 'DISCO.png'))
    a = np.array(src)
    rows = segment(a, y_from=12)       # la fila 0-8 es la muestra de colores
    assert [len(r) for r in rows] == [3, 3], [len(r) for r in rows]
    grid = np.zeros((64, 64 * 3), np.uint8)
    for c, (x0, x1, y0, y1) in enumerate(rows[0]):
        part = a[y0:y1, x0:x1]
        h, w = part.shape
        paste(grid[:, c * 64:(c + 1) * 64], part, (64 - w) // 2, 64 - h)
    save(grid, src.getpalette()[:48], 'snow_disco.png')
    x0, x1, y0, y1 = rows[1][0]
    part = a[y0:y1, x0:x1]
    sh = np.zeros((16, 56), np.uint8)
    paste(sh, part, (56 - part.shape[1]) // 2, 16 - part.shape[0])
    save(sh, src.getpalette()[:48], 'snow_disco_sh.png')


if __name__ == '__main__':
    tora()
    iceball()
    disco()
