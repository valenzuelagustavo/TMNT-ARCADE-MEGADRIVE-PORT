#!/usr/bin/env python3
# =============================================================================
# gen_sewer_rat.py (18/10) -- las ratas decorativas de la cloaca (Scene 3)
# =============================================================================
# Fuente: res/images/lvl_3_sewer/rat.png (RGBA, 96x32 = celdas de 32x16):
#   fila 0  [0] quieta (1 frame)
#   fila 1  [1] corriendo (3 frames)
# Mira a la IZQUIERDA (level3_1.c la espeja cuando corre a la derecha).
# Ningun color de la rata existe en las paletas del nivel, asi que se pasa a
# la del FONDO (PAL0, pal_sewer = la paleta de bg_sewer_md.png) por el color
# mas cercano, sin usar el 0 (transparente) ni el 1 / 14 (el agua, que cicla).
# Salida: res/images/lvl_3_sewer/sewer_rat.png, indexada con esa paleta.
#
# Uso: python3 tools/gen_sewer_rat.py   (necesita Pillow)
# =============================================================================
import os
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = os.path.join(ROOT, 'res', 'images', 'lvl_3_sewer')
SKIP = {0, 1, 14}


def main():
    bg = Image.open(os.path.join(D, 'bg_sewer_md.png'))
    pal = bg.getpalette()[:48]
    P = [tuple(pal[i * 3:i * 3 + 3]) for i in range(16)]
    src = Image.open(os.path.join(D, 'rat.png')).convert('RGBA')
    out = Image.new('P', src.size, 0)
    out.putpalette(pal + [0] * (768 - len(pal)))
    cache = {}
    w, h = src.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = src.getpixel((x, y))
            if a < 128:
                continue
            c = (r, g, b)
            if c not in cache:
                cache[c] = min((i for i in range(16) if i not in SKIP),
                               key=lambda i: sum((u - v) ** 2 for u, v in zip(c, P[i])))
            out.putpixel((x, y), cache[c])
    out.save(os.path.join(D, 'sewer_rat.png'), transparency=0)
    for c, i in sorted(cache.items(), key=lambda t: t[1]):
        print('%-16s -> %2d %s' % (c, i, P[i]))


if __name__ == '__main__':
    main()
