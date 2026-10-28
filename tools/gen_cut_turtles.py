#!/usr/bin/env python3
# gen_cut_turtles.py (27/10) - sprite de la tortuga de la cutscene del choque (SCENE_CUT_VAN).
# LEO_TURTLES.png es un lienzo de 320x224 con Leo ya ubicado sobre cuscene_accident.png; aca se
# recorta la region alineada a tiles que lo cubre y se guarda como sprite (indice 0 = transparente,
# paleta propia: el fondo usa los 15 colores de PAL0). Salida: leo_turtles_spr.png (64x64) que va en
# (CUT_TURTLE_X, CUT_TURTLE_Y) de pantalla (ver src/cut_scenes.c).
# Cuando lleguen las otras tortugas: mismo script con su PNG y su posicion.
import numpy as np
from PIL import Image

D = 'res/images/lvl_6_skate/'
SRC = {'leo': 'LEO_TURTLES.png'}
for name, f in SRC.items():
    im = Image.open(D + f)
    a = np.array(im)
    ys, xs = np.where(a != 0)
    x0, y0 = xs.min() // 8 * 8, ys.min() // 8 * 8
    x1, y1 = (xs.max() // 8 + 1) * 8, (ys.max() // 8 + 1) * 8
    sub = a[y0:y1, x0:x1]
    o = Image.fromarray(sub.astype(np.uint8), 'P'); o.putpalette(im.getpalette())
    o.save(D + '%s_turtles_spr.png' % name, transparency=0)
    print(name, 'x', x0, 'y', y0, 'tiles', (x1 - x0) // 8, 'x', (y1 - y0) // 8)
