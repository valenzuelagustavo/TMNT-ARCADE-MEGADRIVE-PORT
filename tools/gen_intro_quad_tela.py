#!/usr/bin/env python3
# =============================================================================
# gen_intro_quad_tela.py  -  Cuadro de las 4 tortugas de la intro (escena D)
# =============================================================================
# (22/09) Gustavo trajo un rip nuevo con mejor uso de color:
#     res/images/intro_tmnt/genesis/Arcade---Teenage-Mutant-Ninja-Turtles---TELA-1.png
# y hay que pasarlo a algo que rescomp acepte. Dos problemas del PNG tal cual:
#
# 1. MIDE 252x224. rescomp exige ancho multiplo de 8 y lo rechaza ("error on
#    line 1"). Se agregan 2 px a cada lado repitiendo la columna del borde, asi
#    llega a 256 = la pantalla H32 de la intro, y la cruz blanca que separa los
#    cuadrantes (x = 124..127 en el original) queda CENTRADA en x = 128, igual
#    que el corte del intro_quad viejo.
#
# 2. EL INDICE 0 ES UN COLOR REAL: el magenta (220,0,237) del fondo de Raph,
#    10.085 px. En Megadrive el indice 0 de un tile es TRANSPARENTE y muestra el
#    color de backdrop, no el de la paleta. Se corren todos los indices +1 (el
#    PNG usa 15 colores, entra justo en 16) y el 0 queda sin usar.
#
# NO se escribe sobre intro_quad.png a proposito: ese archivo lo genera
# tools/gen_intro_assets.py, y si alguien lo vuelve a correr pisaria el arte
# nuevo sin avisar (la misma trampa que el foot soldier generado del 18/09).
#
# Uso:  python3 tools/gen_intro_quad_tela.py
# =============================================================================
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR  = os.path.join(ROOT, "res", "images", "intro_tmnt", "genesis")
SRC  = os.path.join(DIR, "Arcade---Teenage-Mutant-Ninja-Turtles---TELA-1.png")
DST  = os.path.join(DIR, "intro_quad_tela.png")

W_OUT, H_OUT = 256, 224


def main():
    im = Image.open(SRC)
    if im.mode != "P":
        sys.exit("El PNG tiene que ser indexado")
    a = np.asarray(im).astype(np.uint8)
    h, w = a.shape
    if h != H_OUT:
        sys.exit("Alto %d, se esperaba %d" % (h, H_OUT))

    used = sorted(set(a.flatten().tolist()))
    if len(used) > 15 or max(used) > 14:
        sys.exit("Usa %d colores (indices %s): con el corrimiento +1 no entra "
                 "en 16" % (len(used), used))

    # 1. padding centrado repitiendo los bordes
    pad = W_OUT - w
    if pad < 0 or pad % 2:
        sys.exit("Ancho %d: el padding tiene que ser par y positivo" % w)
    left = np.repeat(a[:, :1], pad // 2, axis=1)
    right = np.repeat(a[:, -1:], pad // 2, axis=1)
    b = np.concatenate([left, a, right], axis=1)

    # 2. indice 0 libre
    b = b + 1
    pal = im.getpalette()[:15 * 3]
    newpal = [0, 0, 0] + pal
    newpal += [0] * (768 - len(newpal))

    out = Image.fromarray(b, "P")
    out.putpalette(newpal)
    out.save(DST)
    print("%s  %dx%d  indices %d..%d" % (os.path.basename(DST), W_OUT, H_OUT,
                                         int(b.min()), int(b.max())))


if __name__ == "__main__":
    main()
