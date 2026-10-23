# -*- coding: utf-8 -*-
# gen_factory_intro.py (18/10) -- los Mousers llevandose a Splinter (5-1)
#
# Fuente: res/images/lvl_7_factory/mousers_splinter.png (204x58, 3 frames de
# 68x58, mirando a la DERECHA, paleta propia de 16 colores con el 0
# transparente). Salida: mousers_splinter_gen.png, 3 celdas de 72x64 (9x8
# tiles, lo que pide un SPRITE de rescomp), con el arte centrado en X y
# apoyado ABAJO de la celda (el borde de abajo = los pies). Misma paleta.
#
# Uso:  python3 tools/gen_factory_intro.py   (desde la raiz del proyecto)
import os
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
D = os.path.join(ROOT, "res", "images", "lvl_7_factory")
SRC_W, SRC_H, N = 68, 58, 3
CELL_W, CELL_H = 72, 64


def main():
    src = Image.open(os.path.join(D, "mousers_splinter.png"))
    assert src.mode == "P"
    out = Image.new("P", (CELL_W * N, CELL_H), 0)
    out.putpalette(src.getpalette()[:48])
    ox = (CELL_W - SRC_W) // 2
    oy = CELL_H - SRC_H
    for k in range(N):
        out.paste(src.crop((k * SRC_W, 0, (k + 1) * SRC_W, SRC_H)), (k * CELL_W + ox, oy))
    out.save(os.path.join(D, "mousers_splinter_gen.png"), transparency=0)
    print("ok")


if __name__ == "__main__":
    main()
