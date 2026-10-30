# -*- coding: utf-8 -*-
"""
gen_turtle_electro.py (30/10) -- los cuadros de ELECTROCUCION de las tortugas
=============================================================================
Las lasers de la puerta del Technodrome electrocutan a la tortuga. En el
arcade son dos cuadros que se alternan (capturas en tools/electro_src/):
    arcade_laser_1.png   el esqueleto (rayos X) con un resplandor rosa
    arcade_laser_2.png   la silueta negra con el borde verde y una estrella
                         amarilla
mas arcade_laser_0.png (las barras sin la tortuga) para recortar la tortuga
del fondo. Es el sprite de LEONARDO (con las katanas); por ahora se usa el
mismo en las cuatro hojas, hasta que se rippeen los de cada una.

Los colores del arcade se pasan a los 16 de PAL1 (la paleta de las tortugas,
la de res/sprites/players/leo_new_13x13.png), que no tiene rosa:
    esqueleto  hueso blanco -> 13, lineas -> 14 (rojo), resplandor -> 12
               (violeta), borde -> 11 (violeta oscuro)
    silueta    negro -> 1, estrella -> 7 (amarillo), borde verde -> 2/5/8

VRAM: la hoja de las tortugas reserva en VRAM los tiles del cuadro MAS GRANDE
(maxNumTile, hoy 64) para cada jugador en todos los niveles. La estrella
amarilla entera llevaba la hoja a 81 tiles (+17 por jugador), asi que se le
recortan las puntas: solo queda lo que esta a STAR_REACH px del cuerpo (con 6
la hoja queda en 56 tiles; el valor no es monotono, medido con rescomp: 4->70,
5->57, 6->56, 7->67, 8->71). Los cuadros definitivos que se ripeen deberian
entrar en ese limite (rescomp informa maxNumTile en el .s).

Salida: res/sprites/turtle_electro.png (2 cuadros de 96x72, misma paleta que
las hojas). tools/add_electro_row.py los agrega a las hojas. Para poner los
cuadros definitivos basta reemplazar ese PNG (mismo formato) y volver a correr
add_electro_row.py.
Uso: python3 tools/gen_turtle_electro.py
"""
import os

import numpy as np
from PIL import Image
from scipy import ndimage as ndi

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
SRC = os.path.join(HERE, 'electro_src')
SHEET = os.path.join(ROOT, 'res', 'sprites', 'players', 'leo_new_13x13.png')
OUT = os.path.join(ROOT, 'res', 'sprites', 'turtle_electro.png')
CW, CH = 96, 72
STAR_YEL = 7        # la estrella amarilla del cuadro 2 (indice de PAL1)
STAR_REACH = 6      # px de la estrella que se conservan alrededor del cuerpo

# color del arcade -> indice de PAL1
MAP = {
    1: {(255, 255, 255): 13, (99, 0, 0): 14, (255, 198, 255): 12, (231, 132, 206): 12,
        (181, 0, 148): 11, (132, 0, 123): 11},
    2: {(0, 0, 0): 1, (222, 165, 0): 7, (255, 206, 0): 7, (0, 107, 0): 2, (107, 165, 0): 5,
        (165, 222, 0): 8, (115, 99, 0): 3, (148, 123, 0): 6, (0, 189, 255): 9,
        (140, 132, 206): 10, (173, 148, 214): 10, (165, 165, 165): 10, (214, 214, 214): 13,
        (107, 107, 107): 4},
}


def load(name):
    return np.array(Image.open(os.path.join(SRC, name)).convert('RGB')).astype(int)


def sprite_mask(f0, fk, table):
    d = (np.abs(fk - f0).sum(axis=2) > 25)
    d[:40, :] = False
    d[:, :200] = False                     # el jefe queda a la izquierda
    keep = np.zeros(d.shape, bool)
    for c in table:
        keep |= (fk == np.array(c)).all(axis=2)
    m = d & keep
    lab, n = ndi.label(ndi.binary_dilation(m, iterations=2))
    big = lab == (int(np.argmax(ndi.sum(m, lab, range(1, n + 1)))) + 1)
    return m & big


def grow(m, n):
    m = m.copy()
    for _ in range(n):
        p = np.pad(m, 1)
        m = m | p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:]
    return m


def trim_star(f):
    star = (f == STAR_YEL)
    body = (f != 0) & ~star
    f[star & ~grow(body, STAR_REACH)] = 0


def main():
    f0, f1, f2 = load('arcade_laser_0.png'), load('arcade_laser_1.png'), load('arcade_laser_2.png')
    masks = [sprite_mask(f0, f1, MAP[1]), sprite_mask(f0, f2, MAP[2])]
    ys = [np.nonzero(m)[0] for m in masks]
    xs = [np.nonzero(m)[1] for m in masks]
    x0 = min(x.min() for x in xs)
    x1 = max(x.max() for x in xs)
    y1 = max(y.max() for y in ys)
    cx = (x0 + x1) // 2
    sheet = Image.open(SHEET)
    out = np.zeros((CH, CW * 2), np.uint8)
    for k, (m, f, t) in enumerate(zip(masks, (f1, f2), (MAP[1], MAP[2]))):
        for y, x in zip(*np.nonzero(m)):
            ox = x - cx + CW // 2 + k * CW
            oy = y - y1 + CH - 1
            assert 0 <= oy < CH and k * CW <= ox < (k + 1) * CW, (x, y)
            out[oy, ox] = t[tuple(f[y, x])]
    trim_star(out[:, CW:])
    print('electro: bbox arcade x %d..%d, abajo y %d; %d + %d px' %
          (x0, x1, y1, (out[:, :CW] != 0).sum(), (out[:, CW:] != 0).sum()))
    im = Image.fromarray(out, 'P')
    im.putpalette(sheet.getpalette())
    im.save(OUT, transparency=0)


if __name__ == '__main__':
    main()
