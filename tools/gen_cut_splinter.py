#!/usr/bin/env python3
# gen_cut_splinter.py (27/10) - cutscene de Splinter tras la fabrica (SCENE_CUT_SPLINTER).
# Entrada (res/images/lvl_7_factory/Cutscene_Factory/): 1587_fundo.png (fondo amarillo con el globo)
# y 1587_1..13.png (Splinter hablando, indice 0 = transparente; misma paleta).
# Salida:
#   splinter_bg.png     fondo + todo lo que NO cambia entre los 13 cuadros (el cuerpo)
#   splinter_mouth.png  64x40, 13 cuadros: solo los pixeles que cambian (cabeza/boca),
#                       en (168,104) de la pantalla
# Verifica que fondo + cuadro k reproduce exactamente cada 1587_k sobre el fundo.
import numpy as np
from PIL import Image

D = 'res/images/lvl_7_factory/Cutscene_Factory/'
N = 13
X0, Y0, X1, Y1 = 168, 104, 232, 144        # region alineada a tiles que cubre a V

fr = np.stack([np.array(Image.open(D + '1587_%d.png' % i)) for i in range(1, N + 1)])
bgim = Image.open(D + '1587_fundo.png')
bg = np.array(bgim)
pal = bgim.getpalette()

V = (fr != fr[0]).any(0)                    # pixeles que cambian en algun cuadro
ys, xs = np.where(V)
assert xs.min() >= X0 and xs.max() < X1 and ys.min() >= Y0 and ys.max() < Y1, (xs.min(), ys.min(), xs.max(), ys.max())

body = np.where(fr[0] != 0, fr[0], bg)
body[V] = bg[V]
o = Image.fromarray(body.astype(np.uint8), 'P'); o.putpalette(pal); o.save(D + 'splinter_bg.png')

W, H = X1 - X0, Y1 - Y0
sheet = np.zeros((H, W * N), np.uint8)
for i in range(N):
    sheet[:, i * W:(i + 1) * W] = np.where(V[Y0:Y1, X0:X1], fr[i][Y0:Y1, X0:X1], 0)
s = Image.fromarray(sheet, 'P'); s.putpalette(pal); s.save(D + 'splinter_mouth.png', transparency=0)

for i in range(N):
    comp = body.copy()
    reg = sheet[:, i * W:(i + 1) * W]
    sub = comp[Y0:Y1, X0:X1]
    sub[reg != 0] = reg[reg != 0]
    assert (comp == np.where(fr[i] != 0, fr[i], bg)).all(), i
tiles = {body[y:y + 8, x:x + 8].tobytes() for y in range(0, 224, 8) for x in range(0, 320, 8)}
print('ok: %d px cambian, %d tiles unicos en el fondo' % (V.sum(), len(tiles)))
