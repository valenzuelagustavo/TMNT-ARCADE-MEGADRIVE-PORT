#!/usr/bin/env python3
# =============================================================================
# gen_intro_quad_tela.py  -  Cuadro de las 4 tortugas de la intro (escena D)
# =============================================================================
# (22/09) Rip nuevo con mejor uso de color:
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
# (09/10) REVERSION a 320x224: "Arcade - Teenage Mutant Ninja Turtles - TELA 1
# 320X224.png" (en res/images/intro_tmnt/), con la paleta nueva del titulo y
# un marco negro de 4 px con la cruz en el medio. Ya mide 320 (la escena D
# pasa a 320 de ancho) y deja libre el indice 0: no hace falta padding ni
# corrimiento. Si el rip vuelve a traer el indice 0 ocupado, se corre +1.
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
SRC  = os.path.join(ROOT, "res", "images", "intro_tmnt",
                    "Arcade - Teenage Mutant Ninja Turtles - TELA 1 320X224.png")
DST  = os.path.join(DIR, "intro_quad_tela.png")

W_OUT, H_OUT = 320, 224


def main():
    im = Image.open(SRC)
    if im.mode != "P":
        sys.exit("El PNG tiene que ser indexado")
    a = np.asarray(im).astype(np.uint8)
    h, w = a.shape
    if h != H_OUT:
        sys.exit("Alto %d, se esperaba %d" % (h, H_OUT))

    used = sorted(set(a.flatten().tolist()))
    pal = im.getpalette()

    # 1. padding centrado repitiendo los bordes (si hiciera falta)
    pad = W_OUT - w
    if pad < 0 or pad % 2:
        sys.exit("Ancho %d: el padding tiene que ser par y positivo" % w)
    if pad:
        left = np.repeat(a[:, :1], pad // 2, axis=1)
        right = np.repeat(a[:, -1:], pad // 2, axis=1)
        b = np.concatenate([left, a, right], axis=1)
    else:
        b = a

    # 2. indice 0 libre (transparente en Megadrive)
    if 0 in used:
        if len(used) > 15 or max(used) > 14:
            sys.exit("Usa %d colores (indices %s): con el corrimiento +1 no "
                     "entra en 16" % (len(used), used))
        b = b + 1
        newpal = [0, 0, 0] + pal[:15 * 3]
    else:
        if max(used) > 15:
            sys.exit("Usa indices %s: no entra en 16" % used)
        newpal = [0, 0, 0] + pal[3:16 * 3]
    newpal += [0] * (768 - len(newpal))

    out = Image.fromarray(b, "P")
    out.putpalette(newpal)
    out.save(DST)
    print("%s  %dx%d  indices %d..%d" % (os.path.basename(DST), W_OUT, H_OUT,
                                         int(b.min()), int(b.max())))


if __name__ == "__main__":
    main()
