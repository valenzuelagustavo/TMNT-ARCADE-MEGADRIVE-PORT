# -*- coding: utf-8 -*-
"""
gen_final_bosses.py -- arma las hojas de KRANG (su cuerpo androide) y de
SHREDDER para la sala final (06/10)

Fuentes:
    res/sprites/krang_boss.png      frames sueltos, mirando a la DERECHA
    res/sprites/shredder_boss.png   idem, con casco arriba y sin casco abajo

Las hojas no son una grilla: cada fila del ripeo es una franja horizontal y
los frames de la franja se separan por columnas vacias. Este script:
  1. recorta cada frame (franja + rango de columnas);
  2. lo ancla por la SOMBRA: X = centro de las ultimas filas del frame (la
     sombra va debajo del cuerpo), Y = base de la franja (los pies);
  3. lo pega en una grilla pareja, una fila por animacion, con el ancla en el
     centro de la celda y los pies en el borde de abajo.
La paleta de cada hoja se conserva tal cual (Krang y Shredder usan PAL2 en la
sala, uno despues del otro).

Salidas (las compila final_bosses.res):
    krang_boss_gen.png     krang_fist_gen.png   krang_head_gen.png
    krang_bolt_gen.png     krang_zap_gen.png
    shredder_boss_gen.png  shredder_beam_a_gen.png  shredder_beam_b_gen.png
    shredder_fx_gen.png    shredder_helmet_gen.png

Las FILAS de cada hoja tienen que coincidir con KRANG_ANIM_* (krang.h) y
SHRED_ANIM_* (shredder_boss.h).

Uso:  python tools/gen_final_bosses.py
"""
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SPR = os.path.join(ROOT, 'res', 'sprites')


def load(name):
    im = Image.open(os.path.join(SPR, name))
    return np.array(im), im.getpalette()[:48]


def runs(v):
    r, s = [], None
    for i, x in enumerate(v):
        if x and s is None:
            s = i
        if not x and s is not None:
            r.append((s, i))
            s = None
    if s is not None:
        r.append((s, len(v)))
    return r


def crop(a, x0, x1, y0, y1):
    """Frame recortado (sin bordes vacios) y su ancla (ax, ay) relativa al
    recorte: ax = centro de la sombra, ay = base de la franja (y1)."""
    sub = a[y0:y1, x0:x1]
    yy, xx = np.nonzero(sub)
    t, b, l, r = yy.min(), yy.max() + 1, xx.min(), xx.max() + 1
    px = sub[t:b, l:r].copy()
    m = px != 0
    rows = np.nonzero(m.any(axis=1))[0]
    low = m[rows[-3]:, :] if len(rows) >= 3 else m
    cols = np.nonzero(low.any(axis=0))[0]
    ax = (cols.min() + cols.max() + 1) / 2.0
    ay = (y1 - y0) - t
    return px, ax, ay


def blob(a, x0, x1, y0, y1, base):
    """Frame sin sombra (la materializacion): ancla X en el centro de masa,
    pies en la base 'base' de la franja."""
    sub = a[y0:y1, x0:x1]
    yy, xx = np.nonzero(sub)
    t, b, l, r = yy.min(), yy.max() + 1, xx.min(), xx.max() + 1
    px = sub[t:b, l:r].copy()
    ax = float(np.mean(xx - l)) + 0.5
    ay = (base - y0) - t
    return px, ax, ay


def build(name, pal, rows, W=None, H=None):
    half = up = 0
    for label, frames in rows:
        for px, ax, ay in frames:
            half = max(half, ax, px.shape[1] - ax)
            up = max(up, ay)
    W = W or int(-(-int(np.ceil(half * 2)) // 8) * 8)
    H = H or int(-(-int(up) // 8) * 8)
    cols = max(len(f) for _, f in rows)
    sheet = np.zeros((H * len(rows), W * cols), np.uint8)
    for r, (label, frames) in enumerate(rows):
        for c, (px, ax, ay) in enumerate(frames):
            ox = int(round(W / 2 - ax))
            oy = H - int(ay)
            h, w = px.shape
            sx0, sx1 = max(0, -ox), min(w, W - ox)
            sy0, sy1 = max(0, -oy), min(h, H - oy)
            if sx1 <= sx0 or sy1 <= sy0:
                continue
            dst = sheet[r * H + oy + sy0: r * H + oy + sy1, c * W + ox + sx0: c * W + ox + sx1]
            src = px[sy0:sy1, sx0:sx1]
            dst[src != 0] = src[src != 0]
    save(name, sheet, pal)
    print('%-26s celda %dx%d (%dx%d tiles), %d filas x %d' %
          (name, W, H, W // 8, H // 8, len(rows), cols))
    for r, (label, frames) in enumerate(rows):
        print('   [%d] %-10s %d frames' % (r, label, len(frames)))
    return W, H


def save(name, sheet, pal):
    out = Image.fromarray(sheet, 'P')
    out.putpalette(list(pal) + [0] * (768 - len(pal)))
    out.save(os.path.join(SPR, name), transparency=0)


def place(cells, cw, ch, name, pal, anchor='center'):
    """Hoja chica: cells = filas de listas de pixeles, centrados en la celda
    (o pegados al borde derecho con anchor='right')."""
    cols = max(len(r) for r in cells)
    sheet = np.zeros((ch * len(cells), cw * cols), np.uint8)
    for r, row in enumerate(cells):
        for c, px in enumerate(row):
            h, w = px.shape
            w2, h2 = min(w, cw), min(h, ch)
            px = px[:h2, w - w2:] if anchor == 'right' else px[:h2, :w2]
            ox = cw - w2 if anchor == 'right' else (cw - w2) // 2
            oy = (ch - h2) // 2
            dst = sheet[r * ch + oy: r * ch + oy + h2, c * cw + ox: c * cw + ox + w2]
            dst[px != 0] = px[px != 0]
    save(name, sheet, pal)
    print('%-26s celda %dx%d, %d filas' % (name, cw, ch, len(cells)))


def trim(px):
    yy, xx = np.nonzero(px)
    return px[yy.min():yy.max() + 1, xx.min():xx.max() + 1]


def remap(px, src_pal, dst_pal):
    """Pasa pixeles de una paleta a otra por el color mas parecido (sin el 0)."""
    sp = np.array(src_pal).reshape(-1, 3).astype(int)
    dp = np.array(dst_pal).reshape(-1, 3).astype(int)
    lut = np.zeros(256, np.uint8)
    for i in range(1, 16):
        d = ((dp[1:] - sp[i]) ** 2).sum(axis=1)
        lut[i] = 1 + int(np.argmin(d))
    return np.where(px != 0, lut[px], 0).astype(np.uint8)


# ===========================================================================
# KRANG
# ===========================================================================
def krang(shred_a, shred_pal):
    a, pal = load('krang_boss.png')
    # Franjas (y0, y1) y columnas de cada frame en el ripeo.
    B0 = (7, 145)
    B1 = (168, 291)
    B2 = (296, 430)          # incluye las chispas de la antena (y 296..306)
    B3 = (433, 556)
    B4 = (571, 694)
    B5 = (696, 824)
    B6 = (826, 953)
    # Una raya verde suelta debajo del ultimo paso de la caminata (marca del
    # ripeo): el verde (indice 14) en las filas de los pies se borra.
    a = a.copy()
    for y0, y1 in (B1, B2, B3, B4, B5, B6):
        foot = a[y1 - 6:y1]
        foot[foot == 14] = 0
    f = lambda b, x0, x1: crop(a, x0, x1, b[0], b[1])
    rows = [
        ('idle',  [f(B0, 23, 85), f(B0, 87, 160)]),
        ('walk',  [f(B1, x0, x1) for x0, x1 in
                   ((2, 53), (68, 108), (110, 166), (168, 210), (212, 276), (278, 347), (349, 413))]),
        ('kick',  [f(B2, 2, 71), f(B2, 73, 149), f(B2, 151, 261)]),
        ('laser', [f(B2, 265, 316), f(B2, 318, 369), f(B2, 371, 423)]),
        ('arm',   [f(B3, 68, 124), f(B3, 126, 198), f(B3, 200, 299), f(B3, 301, 376)]),
        ('taunt', [f(B0, 162, 224), f(B0, 226, 296)]),
        ('hurt',  [f(B5, x, x + 91) for x in (2, 95, 188, 281)]),
        ('death', [f(B4, 2, 75), f(B4, 77, 152)] + [f(B6, x, x + 68) for x in (2, 72, 142, 212, 282, 352)]),
    ]
    build('krang_boss_gen.png', pal, rows)

    # Puno cohete: cinco piezas apiladas a la derecha de la fila B3. El puno
    # mira a la derecha; la llama queda atras. Pegado al borde derecho.
    fists = [trim(a[y0:y1, 378:438]) for y0, y1 in
             ((464, 476), (478, 492), (493, 518), (519, 538), (542, 557))]
    place([fists], 64, 24, 'krang_fist_gen.png', pal, anchor='right')

    # Cabeza (el cerebro de Krang que escapa) y su sombra.
    heads = [trim(a[y0:y1, x0:x1]) for (x0, x1, y0, y1) in
             ((154, 187, 605, 634), (188, 220, 605, 634), (154, 186, 635, 664),
              (188, 218, 635, 664), (154, 186, 665, 694))]
    shadow = trim(a[665:694, 192:213])
    place([heads, [shadow]], 32, 32, 'krang_head_gen.png', pal)

    # Rayo que cae del techo: el relampago recto de la hoja de Shredder,
    # parado y pasado a la paleta de Krang. Dos frames (normal y espejado)
    # para que titile.
    bolt = remap(trim(shred_a[735:752, 1:130]), shred_pal, pal)
    v = np.rot90(bolt, 1)
    place([[v, v[::-1, ::-1]]], 16, 128, 'krang_bolt_gen.png', pal)
    # Chispazo contra el piso: los dos pedacitos chicos del relampago.
    z1 = remap(trim(shred_a[735:752, 132:165]), shred_pal, pal)
    z2 = remap(trim(shred_a[735:752, 166:199]), shred_pal, pal)
    place([[z1, z2]], 32, 16, 'krang_zap_gen.png', pal)


# ===========================================================================
# SHREDDER
# ===========================================================================
def shredder(a, pal):
    f = lambda b, x0, x1: crop(a, x0, x1, b[0], b[1])
    R0 = (2, 80)              # materializacion + quieto + destello amarillo
    R1 = (88, 166)            # camina
    R2 = (171, 250)           # camina hacia el fondo
    R3 = (257, 306)           # se agacha (defensa)
    RA = (320, 396)
    RB = (401, 493)
    RC = (498, 572)
    RD = (577, 645)
    RR = (648, 729)           # rayo (los tres frames de Shredder)
    HB = (1087, 1160)         # golpeado (con casco)
    G = (1877, 1954)          # fantasma (palido)
    # sin casco
    BI = (1164, 1241)
    BW = (1245, 1324)
    BU = (1328, 1405)
    BC = (1411, 1460)
    BA = (1465, 1540)
    BB = (1546, 1638)
    BCc = (1643, 1717)
    BD = (1720, 1789)
    BX = (1797, 1871)
    appear = [blob(a, x0, x1, R0[0], R0[1], R0[1]) for x0, x1 in
              ((3, 9), (16, 28), (33, 51), (56, 80))] + \
             [f(R0, x0, x1) for x0, x1 in ((85, 127), (131, 178), (185, 237))]
    rows = [
        ('idle',     [f(R0, 244, 299)]),
        ('walk',     [f(R1, x0, x1) for x0, x1 in
                      ((2, 54), (64, 124), (133, 196), (201, 255), (271, 326), (337, 391))]),
        ('walk_up',  [f(R2, x0, x1) for x0, x1 in
                      ((3, 52), (63, 115), (125, 180), (190, 243), (256, 315), (319, 370))]),
        ('block',    [f(R3, 3, 70)]),
        ('atk1',     [f(RA, x0, x1) for x0, x1 in ((65, 115), (125, 174), (182, 270), (278, 372))]),
        ('atk2',     [f(RB, x0, x1) for x0, x1 in ((66, 106), (119, 178), (183, 245), (250, 298))]),
        ('atk3',     [f(RC, x0, x1) for x0, x1 in ((4, 67), (76, 131), (138, 198), (206, 249))]),
        ('atk4',     [f(RD, x0, x1) for x0, x1 in ((5, 69), (78, 142), (149, 203), (210, 272))]),
        ('ray',      [f(RR, 1, 56), f(RR, 131, 186), f(RR, 229, 284)]),
        ('hurt',     [f(HB, 6, 46), f(HB, 51, 105)]),
        ('appear',   appear),
        ('flash',    [f(R0, 305, 360)]),
        ('ghost',    [f(G, 1, 56), f(G, 64, 104), f(G, 115, 169)]),
        ('b_idle',   [f(BI, 2, 57)]),
        ('b_walk',   [f(BW, x0, x1) for x0, x1 in
                      ((2, 54), (61, 121), (125, 188), (192, 246), (255, 310), (316, 370))]),
        ('b_walkup', [f(BU, x0, x1) for x0, x1 in
                      ((2, 51), (64, 116), (119, 174), (178, 232), (241, 300), (303, 354))]),
        ('b_block',  [f(BC, 5, 72)]),
        ('b_atk1',   [f(BA, x0, x1) for x0, x1 in ((2, 52), (60, 109), (118, 206), (209, 303))]),
        ('b_atk2',   [f(BB, x0, x1) for x0, x1 in ((9, 49), (55, 114), (119, 181), (190, 238))]),
        ('b_atk3',   [f(BCc, x0, x1) for x0, x1 in ((3, 66), (75, 130), (142, 202), (206, 249))]),
        ('b_atk4',   [f(BD, x0, x1) for x0, x1 in ((1, 65), (73, 137), (142, 196), (198, 260))]),
        ('b_hurt',   [f(BX, 2, 42), f(BX, 49, 103)]),
        ('b_death',  [f(BX, 109, 171), f(BX, 185, 248), f(BX, 259, 323)]),
    ]
    build('shredder_boss_gen.png', pal, rows)

    # Efectos del rayo: chispitas que crecen en la espada (fila 0), la
    # llamarada (fila 1) y el resplandor de la punta (fila 2).
    sparks = [trim(a[RR[0]:RR[1], x0:x1]) for x0, x1 in
              ((74, 77), (78, 83), (84, 91), (93, 100), (101, 109), (111, 120))]
    flames = [trim(a[RR[0]:RR[1], x0:x1]) for x0, x1 in ((192, 203), (203, 214), (214, 225))]
    glow = [trim(a[RR[0]:RR[1], 298:331])]
    place([sparks, flames, glow], 32, 32, 'shredder_fx_gen.png', pal)

    # Casco que sale volando (el pedacito del final de la fila de muerte).
    place([[trim(a[BX[0]:BX[1], 336:349])]], 16, 16, 'shredder_helmet_gen.png', pal)

    # Rayo de la espada: siete frames que crecen y tres del tridente entero.
    # Origen = la bola de la izquierda (centro de las primeras columnas). Se
    # parte en dos mitades de 136 px (un sprite no puede pasar de 32 tiles).
    beams = []
    for (y0, y1), spans in (((771, 900), ((9, 49), (53, 124), (127, 231), (235, 374),
                                         (378, 549), (554, 754), (763, 997))),
                            ((920, 1066), ((11, 276), (281, 550), (555, 823)))):
        for x0, x1 in spans:
            sub = a[y0:y1, x0:x1]
            yy, xx = np.nonzero(sub)
            l = xx.min()
            first = yy[xx <= l + 5]
            oy = int(round(float(np.mean(first))))
            beams.append((sub, l, oy))
    BW_, BH_ = 272, 152
    full = np.zeros((BH_, BW_ * len(beams)), np.uint8)
    for i, (sub, l, oy) in enumerate(beams):
        h, w = sub.shape
        for y in range(h):
            ty = BH_ // 2 + (y - oy)
            if ty < 0 or ty >= BH_:
                continue
            for x in range(l, w):
                tx = x - l
                if tx >= BW_ or sub[y, x] == 0:
                    continue
                full[ty, i * BW_ + tx] = sub[y, x]
    half_a = np.concatenate([full[:, i * BW_: i * BW_ + 136] for i in range(len(beams))], axis=1)
    half_b = np.concatenate([full[:, i * BW_ + 136: (i + 1) * BW_] for i in range(len(beams))], axis=1)
    save('shredder_beam_a_gen.png', half_a, pal)
    save('shredder_beam_b_gen.png', half_b, pal)
    print('%-26s celda 136x%d, %d frames (x2 mitades)' % ('shredder_beam_*_gen.png', BH_, len(beams)))


def main():
    sa, spal = load('shredder_boss.png')
    shredder(sa, spal)
    krang(sa, spal)


if __name__ == '__main__':
    main()
