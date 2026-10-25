#!/usr/bin/env python3
# =============================================================================
# gen_techno_tube.py - el TUBO de spray congelante del Technodrome (25/10)
# =============================================================================
# Entrada: res/sprites/Arcade - ... - Robot Enemies.png (hoja del arcade; sus
#          primeros sprites son el tubo que sale del piso). Usa la MISMA paleta
#          que el foot soldier blanco (PAL3, la del nivel): no hay que remapear.
# Salida:  res/images/lvl_8_techno/techno_tube.png   8 cuadros 16x56 (2x7 tiles)
#            0 disco en el piso      1 disco abierto      2/3/4 saliendo
#            5 entero                6 entero disparando  7 destruido
#          (todos alineados abajo, centro x=8: el borde de abajo es el piso)
#          res/images/lvl_8_techno/techno_spray.png  4 cuadros 64x64 (8x8 tiles)
#            0/1 la nube de spray (el 1 es el 0 espejado en vertical alrededor
#            de la boquilla, y se alternan para que "hierva"), 2/3 ralos (el
#            principio y el final del chorro). El borde derecho toca la boquilla
#            (x=63, y=31); la punta del tubo disparando esta en x=64, y=28.
# Uso:  python3 tools/gen_techno_tube.py
# =============================================================================
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, 'res', 'sprites', 'Arcade - Teenage Mutant Ninja Turtles - Enemies - Robot Enemies.png')
OUT = os.path.join(ROOT, 'res', 'images', 'lvl_8_techno')

# (x0, y0, x1, y1) de cada cuadro del tubo en la hoja
BOXES = [(22, 15, 35, 23), (82, 15, 95, 24), (141, 14, 155, 24), (21, 70, 35, 88),
         (81, 62, 95, 96), (141, 54, 155, 104), (263, 38, 277, 88), (322, 5, 335, 33)]
SPRAY_BOX = (199, 10, 262, 72)          # la nube (sin el tubo)
TW, TH = 16, 56


def main():
    im = Image.open(SRC)
    a = np.array(im)
    pal = im.getpalette()
    tube = np.zeros((TH, TW * len(BOXES)), np.uint8)
    for k, (x0, y0, x1, y1) in enumerate(BOXES):
        c = a[y0:y1 + 1, x0:x1 + 1]
        h, w = c.shape
        ox = k * TW + TW // 2 - w // 2
        tube[TH - h:TH, ox:ox + w] = c
    out = Image.fromarray(tube, 'P'); out.putpalette(pal)
    out.save(os.path.join(OUT, 'techno_tube.png'), transparency=0, optimize=False)

    x0, y0, x1, y1 = SPRAY_BOX
    cloud = np.zeros((64, 64), np.uint8)
    cloud[0:y1 - y0 + 1, 0:x1 - x0 + 1] = a[y0:y1 + 1, x0:x1 + 1]
    flip = np.zeros_like(cloud)
    for y in range(63):
        flip[62 - y] = cloud[y]
    rng = np.random.RandomState(7)
    def thin(c):
        t = c.copy(); t[rng.rand(*t.shape) < 0.55] = 0
        return t
    frames = [cloud, flip, thin(cloud), thin(flip)]
    sheet = np.concatenate(frames, 1)
    out = Image.fromarray(sheet, 'P'); out.putpalette(pal)
    out.save(os.path.join(OUT, 'techno_spray.png'), transparency=0, optimize=False)
    print('tube', tube.shape, 'spray', sheet.shape)


if __name__ == '__main__':
    main()
