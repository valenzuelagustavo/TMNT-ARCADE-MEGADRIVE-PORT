#!/usr/bin/env python3
# =============================================================================
# gen_final_12.py (16/10) -- final del 1-2: Shredder se lleva a April
# =============================================================================
# Arte de Gustavo en res/images/lvl_1_scene/:
#   background.png  320x224  el edificio con la sombra de Shredder (BG_B, PAL0).
#                            Se usa tal cual (rescomp lo lee directo).
#   april.png       320x239  Shredder con April, va ENCIMA (BG_A, PAL1, indice
#                            0 transparente). Trae 15 filas vacias de mas
#                            arriba: se recortan para que mida 320x224 y
#                            calce con el fondo (la figura queda donde estaba
#                            en el final viejo).
#   turtles.png     336x98   las 4 tortugas en la pared (PAL2, SPRITES).
#
# Salidas:
#   april_final_gen.png   320x224
#   final_turtles_gen.png 256x64: 4 frames de 64x64, frame = tortuga en el
#                         orden del juego (0 Leo, 1 Mike, 2 Don, 3 Raph). Cada
#                         una pegada arriba a la izquierda de su celda; la
#                         escena la pone en TURTLE_POS (ver scenes.c).
#
# Uso: python3 tools/gen_final_12.py
# =============================================================================
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
DIR = os.path.join(ROOT, 'res', 'images', 'lvl_1_scene')

APRIL_TOP = 15   # filas vacias de mas arriba de april.png

# Caja de cada tortuga en turtles.png (x0, y0, x1, y1), en el orden del juego.
# La posicion en pantalla (medida en la captura de Gustavo) va en scenes.c.
TURTLES = [
    (104, 28, 151, 91),   # 0 Leo   -> pantalla (227, 39)
    ( 15, 29,  76, 91),   # 1 Mike  -> pantalla ( 55, 71)
    (268, 28, 311, 90),   # 2 Don   -> pantalla (153, 13)
    (183, 30, 234, 82),   # 3 Raph  -> pantalla ( 83, 12)
]


def save(arr, pal, name):
    im = Image.fromarray(arr, 'P')
    im.putpalette(pal)
    im.save(os.path.join(DIR, name), transparency=0)
    print('->', name, arr.shape[1], 'x', arr.shape[0])


def main():
    src = Image.open(os.path.join(DIR, 'april.png'))
    a = np.array(src)
    assert not a[:APRIL_TOP].any(), 'april.png: las filas de arriba no estan vacias'
    save(a[APRIL_TOP:APRIL_TOP + 224].copy(), src.getpalette(), 'april_final_gen.png')

    src = Image.open(os.path.join(DIR, 'turtles.png'))
    t = np.array(src)
    out = np.zeros((64, 64 * 4), np.uint8)
    for i, (x0, y0, x1, y1) in enumerate(TURTLES):
        piece = t[y0:y1 + 1, x0:x1 + 1]
        assert piece.shape[0] <= 64 and piece.shape[1] <= 64
        out[:piece.shape[0], i * 64:i * 64 + piece.shape[1]] = piece
    # nada de la hoja puede quedar afuera de las cajas
    assert (t != 0).sum() == (out != 0).sum(), 'turtles.png: hay pixeles fuera de las cajas'
    save(out, src.getpalette(), 'final_turtles_gen.png')


if __name__ == '__main__':
    main()
