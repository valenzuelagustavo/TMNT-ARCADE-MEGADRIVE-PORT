# -*- coding: utf-8 -*-
# fix_player_sheet_rows.py (18/10) -- ajusta las filas agregadas A MANO al
# final de las hojas nuevas de Leo y Don (leo_new_13x13.png, don_new_13x13.png)
#
# Las hojas son una grilla de celdas de 104x104 (10 columnas, una fila por
# PlayerAnim) con la sombra/los pies en la fila 95 de la celda. Gustavo agrego
# en Aseprite:
#   fila 25  ANIM_MOUSER_BITE  un Mouser le muerde el brazo (3 frames)
#   fila 26  ANIM_BURN         quemado por el lanzallamas de Granitor (2 frames)
# pero el alto del PNG no quedo en un multiplo de 104 (Leo 2837, Don 3146) y
# rescomp necesita la grilla exacta (SPRITE ... 13 13). Este script:
#   1. guarda la hoja tal cual llego en res/sprites/players/orig/ (si no esta);
#   2. la corta/rellena a 27 filas (2808 px);
#   3. reubica los frames de las filas listadas en MOVE: los pies en la fila
#      95 y el centro de los pies en FEET_CX (donde los tiene el idle en
#      pantalla: estas filas no tienen desplazamiento en player_offsets.c);
#   4. los pixeles con indice >= 16 (Aseprite guardo la hoja con 256 colores y
#      el negro de las cabezas quemadas y de la mordida de Don quedo en el 255)
#      pasan al color mas parecido de los 16 (el negro opaco, indice 1);
#   5. se borran los pixeles sueltos (grupos de 1-2) de esas filas;
#   6. (opcional, head) rellena con negro opaco la cabeza quemada si viniera
#      pintada con el indice 0 (negro, pero TRANSPARENTE en la consola). Con la
#      hoja actual de Don ya no hace falta.
#
# Uso:  python3 tools/fix_player_sheet_rows.py   (desde la raiz del proyecto)
#       Lee siempre la copia de orig/, asi que se puede correr mas de una vez.
import os
import shutil

import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
D = os.path.join(ROOT, "res", "sprites", "players")
CELL = 104
ROWS = 27
FEET_Y = 95                 # ultima fila de los pies en la celda
FEET_CX = 41                # centro de los pies del idle en pantalla (Leo y Don)
SKIN = (2, 5, 8)            # verdes de la piel

SHEETS = {
    # nombre: (alto original, filas a reubicar, cabeza a rellenar)
    "leo": (2837, (26,), None),
    "don": (3146, (26,), None),   # head: (y0, y1, x0, x1) de la cabeza en el frame 0
}


def feet(cell):
    ys, xs = np.nonzero(np.isin(cell, SKIN))
    low = ys >= ys.max() - 10
    return int(round(xs[low].mean())), int(ys.max())


def main():
    for name, (h0, move, head) in SHEETS.items():
        path = os.path.join(D, "%s_new_13x13.png" % name)
        orig = os.path.join(D, "orig", "%s_new_13x13_%dpx.png" % (name, h0))
        if not os.path.exists(orig):
            src = Image.open(path)
            assert src.size[1] == h0, "%s: alto %d, se esperaba %d" % (name, src.size[1], h0)
            shutil.copyfile(path, orig)
        src = Image.open(orig)
        a = np.array(src)
        out = np.zeros((ROWS * CELL, a.shape[1]), np.uint8)
        n = min(a.shape[0], ROWS * CELL)
        out[:n] = a[:n]
        pal = src.getpalette()
        hi = out >= 16
        if hi.any():
            for v in (int(x) for x in np.unique(out[hi])):
                c = pal[v * 3:v * 3 + 3]
                j = min(range(1, 16), key=lambda i: sum((x - y) ** 2 for x, y in zip(c, pal[i * 3:i * 3 + 3])))
                print("%s: indice %d (%d px) -> %d" % (name, v, int((out == v).sum()), j))
                out[out == v] = j
        assert not a[ROWS * CELL:].any(), "%s: hay dibujo debajo de la fila %d" % (name, ROWS)
        for r in move:
            for c in range(a.shape[1] // CELL):
                y, x = r * CELL, c * CELL
                cell = out[y:y + CELL, x:x + CELL].copy()
                if not cell.any():
                    continue
                if head is not None:
                    # la cabeza quemada: lo que encierra el contorno pasa a negro opaco
                    fx, _ = feet(cell)
                    dx0 = fx - feet(a[y:y + CELL, 0:CELL])[0]   # contra el frame 0 ORIGINAL
                    y0, y1, x0, x1 = head
                    x0, x1 = x0 + dx0, x1 + dx0
                    m = np.zeros(cell.shape, bool)
                    m[y0:y1, x0:x1] = cell[y0:y1, x0:x1] != 0
                    fl = ndimage.binary_fill_holes(
                        ndimage.binary_closing(m, structure=np.ones((5, 5))))
                    add = fl & (cell == 0)
                    add[:, :x0] = False
                    add[:, x1:] = False
                    cell[add] = 1
                # basura del recorte: grupos de 1-2 pixeles sueltos (en Don
                # quedaba la punta de un bo pegada al borde de la celda)
                lab, nl = ndimage.label(cell != 0, structure=np.ones((3, 3)))
                for i in range(1, nl + 1):
                    if (lab == i).sum() < 3:
                        cell[lab == i] = 0
                fx, fy = feet(cell)
                ddx, ddy = FEET_CX - fx, FEET_Y - fy
                ys, xs = np.nonzero(cell)
                assert xs.min() + ddx >= 0 and xs.max() + ddx < CELL and ys.max() + ddy < CELL, \
                    "%s fila %d frame %d no entra en la celda" % (name, r, c)
                moved = np.zeros_like(cell)
                moved[ys + ddy, xs + ddx] = cell[ys, xs]
                out[y:y + CELL, x:x + CELL] = moved
                print("%s fila %d frame %d: dx %+d dy %+d" % (name, r, c, ddx, ddy))
        img = Image.fromarray(out, "P")
        img.putpalette(pal[:48])
        img.save(path, transparency=0)
        print("%s: %dx%d" % (name, img.size[0], img.size[1]))


if __name__ == "__main__":
    main()
