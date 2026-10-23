# -*- coding: utf-8 -*-
# gen_factory_fx.py (18/10) -- arte nuevo de THE FACTORY (Scene 5-1)
#
# Salidas (res/images/lvl_7_factory/ salvo la ultima):
#   factory_capsule_gen.png  la CAPSULA DEL TALADRO del 1-2
#       (res/sprites/taladro_capsula_v2.png, 864x240: fila 0 = 8 frames del
#       taladro + la capsula cerrada, fila 1 = puerta cerrada/abierta) pasada
#       a la paleta de las TORTUGAS (va en PAL1: blanca como en el arcade;
#       con la del fondo se perdia contra la pared gris).
#   factory_bee_gen.png      las ABEJAS (robots con helice) de la plantilla
#       "...Enemies_abelhas.png" (paleta del foot BLANCO: van en PAL3).
#       Celdas de 32x32, una fila por anim: 0 azul mirando a la izquierda,
#       1 azul de frente, 2 azul a la derecha, 3-5 lo mismo en NARANJA (el
#       parpadeo del golpe). 3 frames por anim = las 3 filas de la hoja (la
#       helice gira). Cada pose con la panza en la fila 30 de la celda y
#       centrada como el cuerpo de la fila sin helice.
#   factory_bee_shot_gen.png los tiros de las abejas (las 3 rayitas rosas de
#       la plantilla): celdas 16x24, frame 0 "/" (abajo a la izquierda),
#       1 "|" (abajo), 2 "\" (abajo a la derecha). PAL3.
#   factory_laser_gen.png    el LASER de los emisores "NFP-1": un tramo de
#       8x32 de una linea vertical BLANCA que titila: frame 0 de 6 px (columnas
#       1-6, el ancho de la captura del arcade de Gustavo) y frame 1 de 4 px
#       (2-5). Paleta de las tortugas (PAL1): 13 = blanco.
#   ../../sprites/granitor_boom_gen.png  la EXPLOSION de Krang
#       (krang_boom_gen.png, cuartos izquierdos 64x64, 8 frames) pasada a la
#       paleta de Granitor (PAL3 con el jefe): blanco, amarillos y naranja.
#
# Uso:  python3 tools/gen_factory_fx.py   (desde la raiz del proyecto)
import os

import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
R = os.path.join(ROOT, "res")
D = os.path.join(R, "images", "lvl_7_factory")
SHEET = os.path.join(R, "sprites", "Arcade - Teenage Mutant Ninja Turtles - Enemies_abelhas.png")


def lv(p):
    return [tuple(v >> 5 for v in p[i * 3:i * 3 + 3]) for i in range(16)]


def nearest(c, pal):
    return min(range(1, 16), key=lambda j: sum((a - b) ** 2 for a, b in zip(c, pal[j])))


def save(arr, pal, path):
    im = Image.fromarray(arr.astype(np.uint8), "P")
    im.putpalette(pal[:48])
    im.save(path, transparency=0)
    print("%-28s %dx%d" % (os.path.basename(path), im.size[0], im.size[1]))


def capsule():
    src = Image.open(os.path.join(R, "sprites", "taladro_capsula_v2.png"))
    # (18/10) En la paleta de las TORTUGAS (PAL1): con la del fondo los grises
    # de la capsula eran los mismos de la pared y no se distinguia; en el
    # arcade es BLANCA, y PAL1 tiene blanco.
    pl = Image.open(os.path.join(R, "sprites", "players", "leo_new_13x13.png"))
    sp, bp = lv(src.getpalette()), lv(pl.getpalette())
    # el gris mas claro de la capsula va al blanco
    bp_w = list(bp)
    remap = [0] + [nearest(sp[i], bp_w) for i in range(1, 16)]
    grays = (1, 4, 10, 13)              # negro, (2,2,2), (4,4,4), blanco
    for i in range(1, 16):
        r, g, b = sp[i]
        if max(r, g, b) - min(r, g, b) <= 1:          # grisaceo: solo a grises
            remap[i] = min(grays, key=lambda j: sum((x - y) ** 2 for x, y in zip(sp[i], bp[j])))
        if sp[i] == (6, 6, 6):
            remap[i] = 13
        if sp[i] == (3, 2, 1):             # marron oscuro: gris oscuro (no oliva)
            remap[i] = 4
        if sp[i] == (4, 2, 0):             # marron: el marron de PAL1
            remap[i] = 6
    a = np.array(src)
    save(np.array(remap, np.uint8)[a], pl.getpalette(), os.path.join(D, "factory_capsule_gen.png"))


def bees(a, pal):
    # columnas de cada pose (azul) y filas de la hoja; naranja = x + 180
    cols = ((10, 50), (70, 110), (130, 170))
    rows = ((8, 46), (68, 106), (128, 166))
    out = np.zeros((32 * 6, 32 * 3), np.uint8)
    for color in range(2):
        dx = 180 * color
        for c, (x0, x1) in enumerate(cols):
            # centro del cuerpo: el de la fila sin helice (la del medio)
            ref = a[rows[1][0]:rows[1][1], x0 + dx:x1 + dx]
            ys, xs = np.nonzero(ref)
            cx = x0 + dx + (xs.min() + xs.max()) // 2
            for r, (y0, y1) in enumerate(rows):
                p = a[y0:y1, x0 + dx:x1 + dx]
                ys, xs = np.nonzero(p)
                bottom = y0 + ys.max()
                cell = np.zeros((32, 32), np.uint8)
                for yy, xx in zip(ys, xs):
                    ty = 30 - (bottom - (y0 + yy))
                    tx = 16 + (x0 + dx + xx - cx)
                    if 0 <= ty < 32 and 0 <= tx < 32:
                        cell[ty, tx] = p[yy, xx]
                out[(color * 3 + c) * 32:(color * 3 + c + 1) * 32, r * 32:(r + 1) * 32] = cell
    save(out, pal, os.path.join(D, "factory_bee_gen.png"))


def shots(a, pal):
    boxes = ((361, 139, 16, 16), (385, 136, 7, 21), (403, 137, 12, 20))
    out = np.zeros((24, 48), np.uint8)
    for k, (x, y, w, h) in enumerate(boxes):
        p = a[y:y + h, x:x + w]
        ox = k * 16 + (16 - w) // 2
        oy = 24 - h
        out[oy:oy + h, ox:ox + w] = p
    save(out, pal, os.path.join(D, "factory_bee_shot_gen.png"))


def laser():
    pal = Image.open(os.path.join(R, "sprites", "players", "leo_new_13x13.png")).getpalette()
    WHITE = 13
    out = np.zeros((32, 16), np.uint8)
    out[:, 1:7] = WHITE                 # 6 px
    out[:, 8 + 2:8 + 6] = WHITE         # 4 px
    save(out, pal, os.path.join(D, "factory_laser_gen.png"))


def boom():
    src = Image.open(os.path.join(R, "sprites", "krang_boom_gen.png"))
    gp = Image.open(os.path.join(R, "sprites", "granitor_boss.png")).getpalette()
    remap = np.zeros(256, np.uint8)
    for s, d in ((6, 3), (7, 11), (8, 15), (12, 4)):   # blanco, amarillo, naranja, rojo
        remap[s] = d
    a = np.array(src)
    assert set(np.unique(a)) <= {0, 6, 7, 8, 12}
    save(remap[a], gp, os.path.join(R, "sprites", "granitor_boom_gen.png"))


def main():
    capsule()
    sh = Image.open(SHEET)
    a, pal = np.array(sh), sh.getpalette()
    bees(a, pal)
    shots(a, pal)
    laser()
    boom()


if __name__ == "__main__":
    main()
