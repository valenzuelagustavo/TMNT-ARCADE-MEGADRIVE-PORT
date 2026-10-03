#!/usr/bin/env python3
# =============================================================================
# gen_bebop_sheet.py  -  Jefe del 2-1: grilla uniforme + frames del disparo
# =============================================================================
# Entrada (la dibuja Gustavo, NO se toca):
#     res/sprites/Arcade - Teenage Mutant Ninja Turtles - Bosses - Bebop.png
#         536x735, 7 filas de frames sueltos. En la fila del disparo, a la
#         derecha, vienen los 5 aros del proyectil (x >= RINGS_X).
#     (03/10) Reemplaza a Bebop_Boss.png + bebop_shot.png: mismos dibujos,
#     con la PALETA COMPARTIDA con Rocksteady y April (ver
#     tools/gen_rocksteady_sheet.py y tools/gen_april_sheet.py).
#
# Salidas (las que compila rescomp):
#     res/sprites/bebop_boss_gen.png   grilla uniforme de 7 filas x 6 frames
#     res/sprites/bebop_shot_gen.png   5 frames, el disparo formandose
#
# POR QUE HAY QUE REARMARLO: rescomp exige una GRILLA (todas las celdas del
# mismo tamano, una fila por animacion). El sheet de Gustavo tiene los frames
# pegados uno al lado del otro con el ancho de cada dibujo, asi que hay que
# recortarlos y re-pegarlos centrados.
#
# EL ANCLA SON LOS PIES, no el centro del dibujo. Si se centrara cada frame por
# su bounding box, el cuerpo se correria solo cada vez que el arma se estira o
# se recoge (el arma mide medio Bebop). Se usa entonces:
#     X = centro del contenido de las ULTIMAS FEET_ROWS filas del frame (las
#         piernas, que es lo que se queda quieto)
#     Y = borde inferior de la celda (la linea de pies)
# La unica fila donde eso no aplica es la de golpes, cuando esta TIRADO en el
# piso: ahi el "pie" es todo el cuerpo acostado, y centrarlo es lo correcto.
#
# Uso:  python3 tools/gen_bebop_sheet.py
# =============================================================================
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPR = os.path.join(ROOT, "res", "sprites")

SRC = os.path.join(SPR, "Arcade - Teenage Mutant Ninja Turtles - Bosses - Bebop.png")
DST = os.path.join(SPR, "bebop_boss_gen.png")
DST_SHOT = os.path.join(SPR, "bebop_shot_gen.png")
SHOT_ROW = 5            # fila del disparo: los aros vienen a su derecha
RINGS_X = 460           # desde esta X de la fila, son aros y no Bebop

FEET_ROWS = 14          # filas de abajo que se toman como "las piernas"
PAD = 8                 # la celda se redondea a multiplo de 8 (tiles)

# Cuantos frames tiene cada fila, para validar contra lo que detecta el script
# (asi un sheet re-exportado con una fila de mas no pasa de largo).
EXPECTED = [3, 3, 6, 4, 5, 5, 6]
ROW_NAMES = ["idle", "vitoreo", "walk", "embestida", "uppercut",
             "disparo", "golpes"]


def runs(mask):
    out, start = [], None
    for i, v in enumerate(mask):
        if v and start is None:
            start = i
        if not v and start is not None:
            out.append((start, i - 1))
            start = None
    if start is not None:
        out.append((start, len(mask) - 1))
    return out


def split_rings(a):
    """Separa los aros del disparo (fila SHOT_ROW, x >= RINGS_X): devuelve
    (sheet sin los aros, recorte con los aros en fila)."""
    art = a != 0
    rows = runs(art.any(axis=1))
    if len(rows) <= SHOT_ROW:
        sys.exit("No esta la fila del disparo")
    r0, r1 = rows[SHOT_ROW]
    rings = a[r0:r1 + 1, RINGS_X:].copy()
    if not (rings != 0).any():
        sys.exit("No se encontraron los aros del disparo")
    b = a.copy()
    b[r0:r1 + 1, RINGS_X:] = 0
    return b, rings


def cut_frames(a):
    """Devuelve [[(sub, xcenter_rel, )...] por fila]: cada frame recortado a su
    bounding box, con la X del centro de las piernas dentro del recorte."""
    art = a != 0
    rows = runs(art.any(axis=1))
    if len(rows) != len(EXPECTED):
        sys.exit("Se detectaron %d filas, se esperaban %d" % (len(rows), len(EXPECTED)))

    out = []
    for ri, (r0, r1) in enumerate(rows):
        band = art[r0:r1 + 1]
        cols = runs(band.any(axis=0))
        if len(cols) != EXPECTED[ri]:
            sys.exit("Fila %d (%s): %d frames, se esperaban %d"
                     % (ri, ROW_NAMES[ri], len(cols), EXPECTED[ri]))
        frames = []
        for c0, c1 in cols:
            sub = a[r0:r1 + 1, c0:c1 + 1]
            ys = np.nonzero((sub != 0).any(axis=1))[0]
            sub = sub[ys.min():ys.max() + 1]          # recorte vertical propio
            feet = sub[max(0, sub.shape[0] - FEET_ROWS):]
            xs = np.nonzero((feet != 0).any(axis=0))[0]
            cx = (int(xs.min()) + int(xs.max())) // 2
            frames.append((sub, cx))
        out.append(frames)
    return out


def build_grid(frames):
    # La celda tiene que cubrir, respecto del ancla: lo que sobresale a la
    # izquierda, lo que sobresale a la derecha y el alto del frame mas alto.
    left = max(f[1] for row in frames for f in row)
    right = max(f[0].shape[1] - 1 - f[1] for row in frames for f in row)
    half = max(left, right) + 1
    cw = ((2 * half + PAD - 1) // PAD) * PAD
    ch = max(f[0].shape[0] for row in frames for f in row)
    ch = ((ch + PAD - 1) // PAD) * PAD
    cols = max(len(row) for row in frames)
    print("celda %dx%d px (%dx%d tiles), grilla %d filas x %d frames"
          % (cw, ch, cw // 8, ch // 8, len(frames), cols))

    out = np.zeros((ch * len(frames), cw * cols), np.uint8)
    for ri, row in enumerate(frames):
        for ci, (sub, cx) in enumerate(row):
            h, w = sub.shape
            x0 = ci * cw + cw // 2 - cx
            y0 = ri * ch + ch - h                    # pegado abajo: los pies
            if x0 < ci * cw or x0 + w > (ci + 1) * cw:
                sys.exit("Fila %d frame %d no entra en la celda" % (ri, ci))
            out[y0:y0 + h, x0:x0 + w] = np.where(sub != 0, sub,
                                                 out[y0:y0 + h, x0:x0 + w])
    return out, cw, ch


def build_shot(a):
    """(03/10) Los aros del disparo como en el ARCADE: cada aro es un proyectil
    aparte que crece mientras vuela (en el video: 10, 16 y 24 px de alto, uno
    cada ~7 frames). Frame k = SOLO el aro k (de menor a mayor), centrado en
    una celda chica de 16x40 (2x5 tiles). bebop.c tira tres aros por disparo.
    (Antes cada frame acumulaba los aros anteriores en una celda de 72x40.)"""
    art = a != 0
    cols = runs(art.any(axis=0))
    if len(cols) < 2:
        sys.exit("El disparo tiene %d aros" % len(cols))
    w = ((max(c1 - c0 + 1 for c0, c1 in cols) + PAD - 1) // PAD) * PAD
    hs = []
    for c0, c1 in cols:
        ys = np.nonzero(art[:, c0:c1 + 1].any(axis=1))[0]
        hs.append((int(ys.min()), int(ys.max())))
    h = ((max(y1 - y0 + 1 for y0, y1 in hs) + PAD - 1) // PAD) * PAD
    print("disparo: %d aros, celda %dx%d px (%dx%d tiles)"
          % (len(cols), w, h, w // 8, h // 8))
    out = np.zeros((h, w * len(cols)), np.uint8)
    for i, ((c0, c1), (y0, y1)) in enumerate(zip(cols, hs)):
        piece = a[y0:y1 + 1, c0:c1 + 1]
        ph, pw = piece.shape
        ox = i * w + (w - pw) // 2
        oy = (h - ph) // 2
        out[oy:oy + ph, ox:ox + pw] = piece
    return out


def remap(arr, src_pal, dst_pal):
    """Reindexa por cercania de color contra la paleta del jefe: el disparo
    comparte PAL3 con el, asi que no puede traer indices propios."""
    lut = np.zeros(256, np.uint8)
    tgt = np.array(dst_pal[1:], dtype=np.int32)
    for i in range(16):
        if i == 0:
            continue
        d = ((tgt - np.array(src_pal[i], dtype=np.int32)) ** 2).sum(axis=1)
        lut[i] = int(d.argmin()) + 1
    return lut[arr]


def save(arr, path, src_im):
    img = Image.fromarray(arr, "P")
    img.putpalette(src_im.getpalette())
    img.save(path, transparency=0)
    print("%s  %dx%d" % (os.path.basename(path), arr.shape[1], arr.shape[0]))


def main():
    im = Image.open(SRC)
    if im.mode != "P":
        sys.exit("El sheet de Bebop tiene que ser indexado")
    a = np.asarray(im).astype(np.uint8)
    body, rings = split_rings(a)
    grid, cw, ch = build_grid(cut_frames(body))
    save(grid, DST, im)
    # Los aros ya vienen con la paleta del jefe (mismo PNG): no hay remapeo.
    save(build_shot(rings), DST_SHOT, im)


if __name__ == "__main__":
    main()
