#!/usr/bin/env python3
# gen_cut_sewer.py (28/10) - cutscene despues de The Sewer (SCENE_CUT_SHREDDER_APRIL):
# Shredder se lleva a April; Rocksteady (con Bebop) los provoca: "Come and get'em, shell-brain".
# Fuentes (res/images/lvl_3_sewer/):
#   1432_1459_shredder_bg.png     fondo (IMAGE, PAL0)
#   1432_1459_april_rapted.png    April atada (SPRITE, PAL1) -> recorte alineado a tiles
#   bebop_rocksteady_N.png        28 cuadros del primer plano (320x224, indice 0 transparente).
#       Solo hay 4 distintos: 1/2/4 boca cerrada + globo; 3 boca abierta + globo; 5 boca
#       abierta sin globo; 6..28 boca cerrada sin globo (el 28 es el primer plano fijo).
# Salidas (todas con la paleta del primer plano, PAL2):
#   cut_april_spr.png     80x128 en (120, 80)
#   cut_rock_mouth.png    128x64: cuadro 0 boca cerrada, 1 abierta; en (222, 160)
#   cut_rock_bubble.png   96x32 el globo, en (192, 96) (solo los pixeles que el globo cambia)
# El primer plano fijo (cuadro 28) se dibuja directo en BG_A (cutscenes.res).
import numpy as np
from PIL import Image

D = 'res/images/lvl_3_sewer/'


def load(n):
    im = Image.open(D + n)
    return im, np.array(im)


def save(a, pal, n):
    o = Image.fromarray(a.astype(np.uint8), 'P')
    o.putpalette(pal)
    o.save(D + n, transparency=0)


imF, f28 = load('bebop_rocksteady_28.png')
_, f1 = load('bebop_rocksteady_1.png')
_, f3 = load('bebop_rocksteady_3.png')
_, f5 = load('bebop_rocksteady_5.png')
pal = imF.getpalette()
for n in (1, 3, 5):
    assert Image.open(D + 'bebop_rocksteady_%d.png' % n).getpalette() == pal, n

# April
imA, ap = load('1432_1459_april_rapted.png')
AX, AY, AW, AH = 120, 80, 80, 128
ys, xs = np.where(ap != 0)
assert xs.min() >= AX and xs.max() < AX + AW and ys.min() >= AY and ys.max() < AY + AH
save(ap[AY:AY + AH, AX:AX + AW], imA.getpalette(), 'cut_april_spr.png')
print('april', AX, AY, AW // 8, AH // 8)

# boca (64x64): cerrada = cuadro 28, abierta = cuadro 5 (el 3 es igual abajo)
MX, MY, MW, MH = 222, 160, 64, 64
closed = f28[MY:MY + MH, MX:MX + MW]
opened = f5[MY:MY + MH, MX:MX + MW]
assert (f3[MY:MY + MH, MX:MX + MW] == opened).all()
assert (closed != 0).all() and (opened != 0).all()
save(np.hstack([closed, opened]), pal, 'cut_rock_mouth.png')
print('mouth', MX, MY)

# globo (96x32): lo que el cuadro 1 cambia respecto del 28
BX, BY, BW, BH = 192, 96, 96, 32
d = f1 != f28
ys, xs = np.where(d)
assert xs.min() == BX and ys.min() == BY and xs.max() == BX + BW - 1 and ys.max() == BY + BH - 1
bub = np.where(d, f1, 0)[BY:BY + BH, BX:BX + BW]
save(bub, pal, 'cut_rock_bubble.png')
print('bubble', BX, BY)
