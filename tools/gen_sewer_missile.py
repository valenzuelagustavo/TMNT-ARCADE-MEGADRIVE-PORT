#!/usr/bin/env python3
# =============================================================================
# gen_sewer_missile.py  (29/09) -- misil y explosion del agua del sewer (3-1)
# =============================================================================
# Reusa el arte de Traag (res/sprites/traag_missil.png, 2 frames de 64x32, y
# traag_explosao.png, 7 frames de 32x32) pero en la cloaca PAL3 es de Baxter
# (y hasta que llega esta en negro). Se remapea cada color al mas parecido de
# la paleta de los FOOT SOLDIERS (PAL2, siempre cargada en el nivel), que es
# la que mejor los cubre (amarillos, naranjas, rojos, gris, crema): medido en
# la grilla de 3 bits por canal de la VDP, error 6215 contra 16748 de la de
# las tortugas y 44328 de la del fondo.
#
# Salidas: res/sprites/sewer_missil.png, res/sprites/sewer_explosao.png
# Uso: python3 tools/gen_sewer_missile.py
# =============================================================================
import os
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SPR = os.path.join(ROOT, 'res', 'sprites')
PAL_SRC = os.path.join(SPR, 'foot_soldier_purple_13x13.png')   # = foot_soldier (PAL2)


def md(c):
    return tuple(round(v * 7 / 255) for v in c)


def main():
    pal = list(Image.open(PAL_SRC).getpalette()[:48])
    P = [tuple(pal[i * 3:i * 3 + 3]) for i in range(16)]
    for src, dst in (('traag_missil.png', 'sewer_missil.png'),
                     ('traag_explosao.png', 'sewer_explosao.png')):
        im = Image.open(os.path.join(SPR, src))
        assert im.mode == 'P'
        sp = im.getpalette()
        remap = {0: 0}
        for i in range(1, 16):
            c = md(sp[i * 3:i * 3 + 3])
            remap[i] = min(range(1, 16),
                           key=lambda j: sum((a - b) ** 2 for a, b in zip(c, md(P[j]))))
        out = Image.new('P', im.size, 0)
        out.putpalette(pal)
        out.putdata([remap.get(v, 0) for v in im.getdata()])
        out.save(os.path.join(SPR, dst), transparency=0)
        print('%s -> %s %s' % (src, dst, im.size))


if __name__ == '__main__':
    main()
