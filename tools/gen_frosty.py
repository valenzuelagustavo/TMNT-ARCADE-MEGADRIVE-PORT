#!/usr/bin/env python3
# gen_frosty.py (27/10) - recorta y ordena frosty-sheet.png (el muneco de nieve
# robot de Central Park) en las hojas que usa src/frosty.c.
#
# Fuente: res/images/lvl_central_park/frosty-sheet.png (628x230, una paleta de 16;
# el indice 0 es transparente). Salida (misma paleta, mismos indices):
#   frosty_snow.png    muneco: [0 en el piso, 1 saltando]
#   frosty_break.png   se rompe el disfraz: [0, 1, 2]
#   frosty_bot.png     robot: [0 piso, 1-2 saltando, 3 golpeado, 4 muerto,
#                              5 sin nariz, 6 regenerando]
#   frosty_missile.png la nariz: [0 sin fuego, 1 con fuego] (mira a la derecha)
#   frosty_shadow.png  la sombra (40x24)
#   frosty_boom.png    la explosion de Baxter (baxter_boom_gen.png) pasada a
#                      la paleta de Frosty (usa los indices 12 y 14, libres)
# Cada hoja es una sola fila. Convencion: la SOMBRA (el "lane") queda a FEET px
# del borde de arriba de la celda; el cuerpo apoyado en el piso queda con su
# borde de abajo a GB px por debajo del lane; el que salta lleva la altura que
# traia el arte (cuerpo vs. centro de su sombra). Imprime las constantes para
# src/frosty_art.h (que este script tambien escribe).
import numpy as np
from PIL import Image
import os

SRC = 'res/images/lvl_central_park/frosty-sheet.png'
OUT = 'res/images/lvl_central_park/'
BOOM_SRC = 'res/sprites/baxter_boom_gen.png'
GB = 6          # el cuerpo en el piso: su borde de abajo, GB px bajo el lane

im = Image.open(SRC)
A = np.array(im)
pal = im.getpalette()


def bbox(r):
    x0, y0, x1, y1 = r
    sub = A[y0:y1, x0:x1] != 0
    ys, xs = np.where(sub)
    return (x0 + xs.min(), y0 + ys.min(), x0 + xs.max() + 1, y0 + ys.max() + 1)


def tile8(v):
    return (v + 7) // 8 * 8


# nombre -> (rect, tipo, sombra)  tipo: 'g' piso, 'a' aire (con sombra), 'l' alinear
# el borde izquierdo (los de la rotura)
SNOW = [
    ('ground', (20, 23, 84, 115), 'g', None),
    ('jump',   (101, 0, 165, 90), 'a', (112, 96, 154, 117)),
]
BREAK = [
    ('b1', (178, 26, 262, 122), 'l', None),
    ('b2', (266, 13, 352, 122), 'l', None),
    ('b3', (368, 24, 432, 115), 'g', None),
]
BOT = [
    ('ground', (441, 30, 494, 113), 'g', None),
    ('jumpA',  (514, 0, 566, 91), 'a', (511, 98, 552, 118)),
    ('jumpB',  (580, 0, 628, 90), 'a', (577, 97, 618, 117)),
    ('hit',    (519, 146, 581, 232), 'g', None),
    ('dead',   (406, 185, 484, 226), 'g', None),
    ('nonose', (154, 134, 209, 220), 'g', None),
    ('regen',  (218, 134, 273, 220), 'g', None),
]


def layout(frames, W, anchor_left=None):
    """Devuelve (placements, FEET, H). placements: (nombre, bbox, dx, dy) donde
    (dx, dy) = donde cae el borde sup-izq del bbox dentro de la celda (FEET aun
    sin fijar: dy relativo al lane)."""
    pl = []
    for name, rect, kind, sh in frames:
        b = bbox(rect)
        bw = b[2] - b[0]
        if kind == 'a':
            sb = bbox(sh)
            lane = (sb[1] + sb[3]) / 2.0
            bottom_rel = b[3] - lane          # negativo: por encima del lane
        else:
            bottom_rel = GB
        top_rel = bottom_rel - (b[3] - b[1])
        if kind == 'l':
            dx = int(W // 2 - 32)
            dx = dx - 0                        # el borde izq. del escombro
        else:
            dx = int(round(W / 2.0 - bw / 2.0))
        pl.append((name, b, dx, top_rel))
    above = max(-t for _, _, _, t in pl)          # px sobre el lane
    below = max(t + (b[3] - b[1]) for _, b, _, t in pl)
    H = tile8(int(np.ceil(above + below)))
    FEET = int(np.ceil(above))
    # que no se pase por abajo
    while FEET + below > H:
        H += 8
    return pl, FEET, H


def build(name, frames, W):
    pl, FEET, H = layout(frames, W)
    sheet = np.zeros((H, W * len(frames)), np.uint8)
    for i, (nm, b, dx, top_rel) in enumerate(pl):
        sub = A[b[1]:b[3], b[0]:b[2]]
        y0 = int(round(FEET + top_rel))
        # sin el sombreado de otros: solo lo del rect (ya recortado a bbox)
        # (los rects de las del aire excluyen su sombra)
        for yy in range(sub.shape[0]):
            for xx in range(sub.shape[1]):
                if sub[yy, xx]:
                    sheet[y0 + yy, i * W + dx + xx] = sub[yy, xx]
    o = Image.fromarray(sheet, 'P')
    o.putpalette(pal)
    o.save(OUT + name + '.png', transparency=0)
    print(name, 'W', W, 'H', H, 'FEET', FEET, 'frames', len(frames))
    return W, H, FEET


dims = {}
dims['snow'] = build('frosty_snow', SNOW, 64)
dims['break'] = build('frosty_break', BREAK, 104)
dims['bot'] = build('frosty_bot', BOT, 80)

# --- misil: la punta (borde derecho) alineada ---
MIS = [((20, 154, 62, 178)), ((66, 152, 130, 180))]
bb = [bbox(r) for r in MIS]
mw, mh = 64, 24
sheet = np.zeros((mh, mw * 2), np.uint8)
for i, b in enumerate(bb):
    sub = A[b[1]:b[3], b[0]:b[2]]
    x0 = 60 - sub.shape[1]
    y0 = (mh - sub.shape[0]) // 2
    sheet[y0:y0 + sub.shape[0], i * mw + x0:i * mw + x0 + sub.shape[1]] = sub
o = Image.fromarray(sheet, 'P'); o.putpalette(pal); o.save(OUT + 'frosty_missile.png', transparency=0)
print('missile', bb)

# --- sombra 40x24 ---
sb = bbox((112, 96, 154, 117))
sh = A[sb[1]:sb[3], sb[0]:sb[0] + 40]
S = np.zeros((24, 40), np.uint8)
S[2:2 + sh.shape[0], 0:sh.shape[1]] = sh
o = Image.fromarray(S, 'P'); o.putpalette(pal); o.save(OUT + 'frosty_shadow.png', transparency=0)

# --- explosion de Baxter con la paleta de Frosty ---
bi = Image.open(BOOM_SRC)
B = np.array(bi)
remap = {6: 12, 10: 13, 13: 14, 15: 6}   # crema, durazno, malva, blanco
R = np.zeros_like(B)
for k, v in remap.items():
    R[B == k] = v
assert set(np.unique(B)) <= set(remap) | {0}, np.unique(B)
o = Image.fromarray(R, 'P'); o.putpalette(pal); o.save(OUT + 'frosty_boom.png', transparency=0)

# --- header con las medidas ---
with open('src/frosty_art.h', 'w') as f:
    f.write('// frosty_art.h - generado por tools/gen_frosty.py (27/10); no editar a mano.\n')
    f.write('// W/H: celda en px; FEET: y del lane (centro de la sombra) dentro de la celda.\n')
    f.write('#ifndef _FROSTY_ART_H_\n#define _FROSTY_ART_H_\n')
    for k, (w, h, ft) in dims.items():
        K = k.upper()
        f.write('#define FA_%s_W %d\n#define FA_%s_H %d\n#define FA_%s_FEET %d\n' % (K, w, K, h, K, ft))
    f.write('#define FA_GB %d\n#endif\n' % GB)
print(open('src/frosty_art.h').read())
