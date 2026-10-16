#!/usr/bin/env python3
# =============================================================================
# gen_sewer_holes.py (13/10) -- la pared de la cloaca que se RAJA
# =============================================================================
# En el arcade los huecos de la pared por donde salen los Mousers no estan
# desde el principio: la pared se raja cuando llegan. El fondo del port
# (bg_sewer_md.png, sacado de la hoja del arcade) ya los trae rotos, asi que
# se tapan con un SPRITE (PAL0, la paleta del fondo) hasta que se abren:
#   frame 0  la pared sana: los pixeles del hueco se reemplazan por los de
#            un pedazo de pared sano (el mejor de la misma fila de ladrillos)
#   frame 1  una rajadura chica (el 30 % del hueco, desde el centro)
#   frame 2  mas grande (el 65 %)
# y despues se suelta el sprite: queda el hueco del fondo.
#   res/images/lvl_3_sewer/sewer_hole_N.png  (N = 0..3), 3 frames en fila
# Las cajas (x, y en el fondo, ancho, alto) estan en BOXES y en level3_1.c
# (swPatch: misma posicion, y de mundo = y del fondo - 32).
#
# Uso: python3 tools/gen_sewer_holes.py   (necesita numpy, scipy, Pillow)
# =============================================================================
import os
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = os.path.join(ROOT, 'res', 'images', 'lvl_3_sewer')
BOXES = [(424, 90, 48, 40),     # hueco 0: el bajo, al lado del caño redondo
         (508, 61, 48, 32),     # hueco 1: alto
         (669, 61, 48, 32),     # hueco 2: alto, antes de las rejas
         (1022, 61, 48, 32)]    # hueco 4: alto, sala de Baxter
STAGES = (0.30, 0.65)


def main():
    im = Image.open(os.path.join(D, 'bg_sewer_md.png'))
    a = np.array(im)
    pal = im.getpalette()
    lum = np.array([sum(pal[i * 3:i * 3 + 3]) / 3 for i in range(256)])
    for n, (x0, y0, w, h) in enumerate(BOXES):
        t = a[y0:y0 + h, x0:x0 + w]
        # El hueco: lo oscuro grueso (la apertura saca las juntas finas).
        dark = lum[t] < 50
        op = ndimage.binary_opening(dark, np.ones((3, 3)))
        lab, cnt = ndimage.label(op)
        sizes = ndimage.sum(op, lab, range(1, cnt + 1))
        hole = lab == 1 + int(np.argmax(sizes))
        m = ndimage.binary_dilation(hole, iterations=3)
        # Donante: pared de la misma altura que mejor calza fuera del hueco
        # y sin agujeros adentro.
        best = None
        for dx in range(-200, 201):
            xs = x0 + dx
            if abs(dx) < w or xs < 0 or xs + w > a.shape[1]:
                continue
            d = a[y0:y0 + h, xs:xs + w]
            sc = (d != t)[~m].mean() + 3 * ndimage.binary_opening(
                lum[d] < 50, np.ones((3, 3)))[m].mean()
            if best is None or sc < best[0]:
                best = (sc, dx)
        d = a[y0:y0 + h, x0 + best[1]:x0 + best[1] + w]
        intact = np.where(m, d, t)
        # Rajaduras: los pixeles del hueco por distancia al centro del hueco.
        ys, xs = np.nonzero(m)
        cy, cx = ys.mean(), xs.mean()
        dist = np.hypot(*np.mgrid[0:h, 0:w] - np.array([cy, cx])[:, None, None])
        lim = [np.quantile(dist[m], s) for s in STAGES]
        frames = [intact] + [np.where(m & (dist <= L), t, intact) for L in lim]
        sheet = np.concatenate(frames, axis=1).astype(np.uint8)
        # El indice 0 es transparente en un sprite: el fondo no lo usa en la
        # pared, pero por las dudas se pasa al negro mas parecido.
        sheet[sheet == 0] = int(np.argmin([lum[i] if i else 999 for i in range(16)]))
        out = Image.fromarray(sheet, 'P')
        out.putpalette(pal)
        out.save(os.path.join(D, 'sewer_hole_%d.png' % n), transparency=0)
        print('sewer_hole_%d' % n, (x0, y0, w, h), 'donante dx', best[1])


if __name__ == '__main__':
    main()
