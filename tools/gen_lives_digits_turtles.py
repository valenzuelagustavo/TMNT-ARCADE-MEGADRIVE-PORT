#!/usr/bin/env python3
# =============================================================================
# gen_lives_digits_turtles.py  (26/09) -- digitos de VIDAS del HUD, uno por
# tortuga, con el color de su bandana.
# =============================================================================
# Arte del proyecto del companero (res/images/hud/src/number_hud_lifes_*.png):
# 10 digitos de 8x16 en fila (80x16), contorno negro + un color. Los cuatro
# colores (azul Leo, naranja Mike, violeta Don, rojo Raph) YA existen en la
# paleta unificada de las tortugas (PAL1), igual que el negro opaco (indice 1),
# asi que se reindexan por color exacto y se dibujan con PAL1 como antes.
#
# Salida: res/images/hud/lives_digits_turtles.png, 8x640 = 4 tortugas x 10
# digitos apilados, en el orden de charIndex (0 Leo, 1 Mike, 2 Don, 3 Raph).
# Digito d de la tortuga t -> tiles [(t*10 + d) * 2 .. +1]  (TILESET NONE NONE).
# =============================================================================
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "res", "images", "hud", "src")
OUT = os.path.join(ROOT, "res", "images", "hud", "lives_digits_turtles.png")
PALSRC = os.path.join(ROOT, "res", "sprites", "leo_anim_13x13.png")
ORDER = ["leo", "dmichelangello", "don", "rafa"]   # = charIndex 0..3


def main():
    pim = Image.open(PALSRC)
    pp = pim.getpalette()
    pal = [tuple(pp[i * 3:i * 3 + 3]) for i in range(16)]
    out = np.zeros((16 * 10 * len(ORDER), 8), np.uint8)
    for t, name in enumerate(ORDER):
        im = Image.open(os.path.join(SRC, "number_hud_lifes_%s.png" % name))
        a = np.asarray(im)
        sp = im.getpalette()
        tr = im.info.get("transparency", 0)
        lut = np.zeros(256, np.uint8)
        for i in set(a.flatten().tolist()):
            if i == tr:
                continue
            c = tuple(sp[i * 3:i * 3 + 3])
            cand = [k for k in range(1, 16) if pal[k] == c]
            if not cand:
                sys.exit("%s: color %s no esta en PAL1" % (name, c))
            lut[i] = cand[0]
        b = lut[a]
        for d in range(10):
            out[(t * 10 + d) * 16:(t * 10 + d + 1) * 16, :] = b[:, d * 8:(d + 1) * 8]
    img = Image.fromarray(out, "P")
    img.putpalette([c for rgb in pal for c in rgb] + [0] * (768 - 48))
    img.save(OUT, transparency=0)
    print("lives_digits_turtles.png  %dx%d" % (out.shape[1], out.shape[0]))


if __name__ == "__main__":
    main()
