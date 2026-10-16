#!/usr/bin/env python3
# =============================================================================
# gen_sewer_mouser.py (13/10) -- los Mousers de los huecos de la cloaca
# =============================================================================
# En la cloaca del arcade salen Mousers de los huecos de la pared mientras
# pelean los foot soldiers (no solo con Baxter). Durante el nivel PAL3 es la
# paleta del foot BLANCO (cuchillo), asi que la rata y su explosion se pasan
# a esa paleta (color por color al mas parecido; el Mouser del arcade es
# blanco y gris azulado, la paleta del blanco tiene esos tonos):
#   res/sprites/sewer_mouser.png       <- res/sprites/baxter_rat.png
#   res/sprites/sewer_mouser_boom.png  <- res/sprites/rat_boom.png
# Con Baxter (que carga su propia PAL3) se siguen usando las originales.
#
# Uso: python3 tools/gen_sewer_mouser.py
# =============================================================================
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SPR = os.path.join(ROOT, 'res', 'sprites')


def pal_of(im):
    p = im.getpalette()[:48]
    return [tuple(p[i * 3:i * 3 + 3]) for i in range(16)]


def main():
    white = pal_of(Image.open(os.path.join(SPR, 'foot_soldier_white_gen.png')))
    # (16/10) sewer_mouser.png lo hace ahora gen_robot_enemies.py (el Mouser
    # azul de la hoja nueva, ya en esta paleta): aca queda solo la explosion.
    for src, dst in (('rat_boom.png', 'sewer_mouser_boom.png'),):
        im = Image.open(os.path.join(SPR, src))
        sp = pal_of(im)
        a = np.array(im)
        lut = np.zeros(256, np.uint8)
        for i in set(a.flatten().tolist()):
            if i == 0:
                continue
            c = sp[i]
            lut[i] = min(range(1, 16),
                         key=lambda j: sum((u - v) ** 2 for u, v in zip(white[j], c)))
            print(dst, i, c, '->', lut[i], white[lut[i]])
        out = Image.fromarray(lut[a], 'P')
        flat = [v for c in white for v in c]
        out.putpalette(flat + [0] * (768 - len(flat)))
        out.save(os.path.join(SPR, dst), transparency=0)


if __name__ == '__main__':
    main()
