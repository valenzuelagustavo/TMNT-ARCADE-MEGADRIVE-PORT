# -*- coding: utf-8 -*-
"""
gen_hud4_shared.py -- HUD de 3-4 jugadores con el marco COMPARTIDO (06/10)

Los cuatro marcos del HUD (hud_1p..hud_4p, 4 filas de color cada uno) son el
mismo contorno gris; lo unico que cambia es el cartel "1UP".."4UP" y su color.
Como sprites separados costaban 4 x 35 tiles de VRAM de sprites. Este script
los parte en:

    res/images/hud/hud4_outline.png   72x32: el contorno (los pixeles que son
                                      iguales en las 16 variantes, menos el
                                      recuadro del cartel)
    res/images/hud/hud4_label.png     celdas de 24x16 (x 8..31, y 0..15 del
                                      marco): fila = jugador (1UP..4UP),
                                      columna = fila de color del marco

El contorno se carga UNA vez y los cuatro sprites apuntan a esos tiles
(hudInit). Superpuestos, contorno + cartel dan exactamente el marco original.

Uso:  python tools/gen_hud4_shared.py
"""
import os
import numpy as np
from PIL import Image

HUD = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'res', 'images', 'hud')
LX0, LY0, LW, LH = 8, 0, 24, 16


def main():
    src = [Image.open(os.path.join(HUD, 'hud_%dp.png' % i)) for i in (1, 2, 3, 4)]
    pal = src[0].getpalette()[:48]
    var = [[np.array(im)[r * 32:(r + 1) * 32, :72] for r in range(4)] for im in src]
    st = np.stack([v for p in var for v in p])
    same = (st == st[0]).all(axis=0)
    diff = ~same
    ys, xs = np.nonzero(diff)
    assert xs.min() >= LX0 and xs.max() < LX0 + LW and ys.max() < LY0 + LH, 'el cartel no entra'
    # El recuadro del cartel va ENTERO en el cartel (tambien lo que es igual
    # en todos): asi en esas lineas no se superponen contorno y cartel, y los
    # cuatro marcos no pasan los 320 px de sprites por linea del VDP.
    box = np.zeros_like(same)
    box[LY0:LY0 + LH, LX0:LX0 + LW] = True
    outline = np.where(same & ~box, st[0], 0).astype(np.uint8)

    labels = np.zeros((LH * 4, LW * 4), np.uint8)
    for p in range(4):
        for r in range(4):
            cell = var[p][r][LY0:LY0 + LH, LX0:LX0 + LW]
            labels[p * LH:(p + 1) * LH, r * LW:(r + 1) * LW] = cell

    for name, arr in (('hud4_outline.png', outline), ('hud4_label.png', labels)):
        im = Image.fromarray(arr, 'P')
        im.putpalette(pal + [0] * (768 - len(pal)))
        im.save(os.path.join(HUD, name), transparency=0)
        print(name, arr.shape[1], 'x', arr.shape[0])


if __name__ == '__main__':
    main()
