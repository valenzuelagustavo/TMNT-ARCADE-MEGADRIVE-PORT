# -*- coding: utf-8 -*-
# gen_techno_hazards.py (18/10) -- peligros del TECHNODROME (Scene 6-1)
#
# Fuente: res/sprites/Arcade - Teenage Mutant Ninja Turtles - Enemies_abelhas.png
# (415x406, plantilla de Gustavo). Viene en la MISMA paleta que el foot
# soldier blanco (foot_soldier_white_gen.png): todo va en PAL3, que en el
# Technodrome es la de los blancos hasta que entra Traag. De la hoja se usan:
#   - las COLUMNAS (8 de 32x190 en y 172: 4 plateadas, x 13/53/93/133, y 4
#     naranjas, x 193/234/273/313; la 0 de cada color lisa y la 1-3 con los
#     rayitos de electricidad). Cuelgan del techo y abajo tienen la cabeza
#     con el canon (mira a la IZQUIERDA). El cuerpo se repite cada 32 filas.
#     Se parte en:
#       techno_pillar_head.png  la cabeza, filas 158-189 (32x32), 8 frames
#       techno_pillar_seg.png   un tramo del cuerpo, x 8-23 y filas 126-157
#                               (16x32): apilados hacia arriba, empalman
#   - el DISPARO (los 4 eslabones de 2 filas en (373,203)): techno_laser.png
#     32x8, mirando a la izquierda;
#   - las BOLAS (2 de 31x31 en (373,311) y (373,371)): techno_ball.png, 2
#     frames de 32x32 (rodando).
# Y se dibujan dos sombras con colores de la misma paleta:
#       techno_laser_sh.png     raya negra de 30x2 (la sombra del disparo en
#                               el piso, como en el arcade)
#       techno_pillar_sh.png    elipse gris de 28x6 (la sombra de la cabeza)
# Las abejas (robots con helice) y los aros no se usan: son de la fabrica.
#
# Uso:  python3 tools/gen_techno_hazards.py   (desde la raiz del proyecto)
import os

import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "res", "sprites",
                   "Arcade - Teenage Mutant Ninja Turtles - Enemies_abelhas.png")
DST = os.path.join(ROOT, "res", "images", "lvl_8_techno")

PILLAR_X = (13, 53, 93, 133, 193, 234, 273, 313)
PILLAR_Y = 172
HEAD_Y0, HEAD_H = 158, 32
SEG_Y0, SEG_X0, SEG_W = 126, 8, 16
LASER = (372, 202, 32, 8)          # x, y, w, h
BALLS = ((373, 311), (373, 371))
GRAY, BLACK = 13, 15               # indices de la paleta (96,96,96) y negro


def save(arr, pal, name):
    im = Image.fromarray(arr.astype(np.uint8), "P")
    im.putpalette(pal)
    im.save(os.path.join(DST, name), transparency=0)
    print("%-22s %dx%d" % (name, im.size[0], im.size[1]))


def main():
    src = Image.open(SRC)
    assert src.mode == "P"
    pal = src.getpalette()[:48]
    a = np.array(src)

    head = np.zeros((HEAD_H, 32 * 8), np.uint8)
    seg = np.zeros((32, SEG_W * 8), np.uint8)
    for k, x0 in enumerate(PILLAR_X):
        p = a[PILLAR_Y:PILLAR_Y + 190, x0:x0 + 32]
        head[:, k * 32:(k + 1) * 32] = p[HEAD_Y0:HEAD_Y0 + HEAD_H]
        s = p[SEG_Y0:SEG_Y0 + 32, SEG_X0:SEG_X0 + SEG_W]
        # el tramo de mas arriba tiene que ser igual (periodo 32)
        assert (s == p[SEG_Y0 - 32:SEG_Y0, SEG_X0:SEG_X0 + SEG_W]).all(), "tramo %d" % k
        assert not p[:HEAD_Y0, :SEG_X0].any() and not p[:HEAD_Y0, SEG_X0 + SEG_W:].any()
        seg[:, k * SEG_W:(k + 1) * SEG_W] = s
    save(head, pal, "techno_pillar_head.png")
    save(seg, pal, "techno_pillar_seg.png")

    x, y, w, h = LASER
    las = a[y:y + h, x:x + w].copy()
    assert las.any()
    save(las, pal, "techno_laser.png")

    sh = np.zeros((8, 32), np.uint8)
    sh[6:8, 1:31] = BLACK
    save(sh, pal, "techno_laser_sh.png")

    psh = np.zeros((8, 32), np.uint8)
    for yy in range(8):
        for xx in range(32):
            if ((xx - 15.5) / 14.0) ** 2 + ((yy - 4.5) / 3.0) ** 2 <= 1.0:
                psh[yy, xx] = GRAY
    save(psh, pal, "techno_pillar_sh.png")

    ball = np.zeros((32, 64), np.uint8)
    for k, (bx, by) in enumerate(BALLS):
        ball[0:31, k * 32:k * 32 + 31] = a[by:by + 31, bx:bx + 31]
    save(ball, pal, "techno_ball.png")


if __name__ == "__main__":
    main()
