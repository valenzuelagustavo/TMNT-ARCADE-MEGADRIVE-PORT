#!/usr/bin/env python3
# =============================================================================
# gen_april_sheet.py  -  April en grilla (y la version del 1-2 en PAL1)
# =============================================================================
# (03/10) Entrada (el arte original, NO se toca):
#   res/sprites/april.png   166x84, 4 frames sueltos en una fila:
#       [0..1] parada (el balanceo del 1-2)
#       [2..3] atada sentada en el piso (el garage)
#   Comparte PALETA con Bebop y Rocksteady (los tres en una linea de paleta).
#
#   res/sprites/april_wave.png  (12/10) April LIBERADA saludando con el brazo
#       en alto, como al final del garage del arcade. Recortada del video
#       (gameplayARCADE/tmnt.avi, cuadro 11700 del tramo del garage) y pasada
#       a los indices de la paleta compartida. 26x73.
#
# Salida (la que compila rescomp):
#   res/sprites/april_gen.png      64x320: grilla de 2x4 celdas de 32x80
#       (4x10 tiles; hasta el 12/10 eran 32x64, pero April saludando mide 73
#       de alto). Anims:
#         0 parada (2 frames)
#         1 atada (2 frames)
#         2 (12/10) se suelta: parada con las sogas cayendo (muslos, tobillos)
#         3 (12/10) saludando (april_wave.png, el mismo frame dos veces)
#       Paleta compartida con los jefes.
#
# EL 1-2 NO USA ESTA HOJA: ahi April esta en pantalla desde el principio y PAL3
# la usa el foot soldier blanco hasta que aparece Rocksteady, asi que se sigue
# dibujando en PAL1 (tortugas) con la hoja anterior, res/sprites/april_l12.png
# (la april.png de antes del 03/10, pintada a mano con esa paleta). Un remapeo
# automatico a la paleta de las tortugas dejaba la piel y los brillos grises.
#
# ANCLA: los pies. El borde de abajo del dibujo va a la ultima fila de la celda
# (y = 63, como la hoja anterior) y el centro de las ultimas FEET_ROWS filas a
# x = FEET_X (con eso los dos frames de la hoja anterior caian en x = 2 y 4,
# igual que ahora: APRIL_FOOT_OFFSET del 1-2 no cambia).
#
# Uso:  python3 tools/gen_april_sheet.py
# =============================================================================
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPR = os.path.join(ROOT, "res", "sprites")

SRC = os.path.join(SPR, "april.png")
DST = os.path.join(SPR, "april_gen.png")

CW, CH = 32, 80
WAVE = os.path.join(SPR, "april_wave.png")
# (12/10) Las sogas: filas 44..47 de la celda atada (la banda gris que le
# rodea el torso, indices 2/3/4), x 16..28. Al soltarse caen por las piernas.
ROPE_ROWS = (44 - 64 + 80, 48 - 64 + 80)   # en la celda de 80 (pies en 79)
ROPE_X = (16, 29)
ROPE_DROP = (62, 74)                       # Y (celda) donde se dibuja en cada frame
FEET_ROWS = 9
FEET_X = 15
ROWS = [[0, 1], [2, 3]]          # frames del rip por fila de la grilla


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


def main():
    im = Image.open(SRC)
    if im.mode != "P":
        sys.exit("april.png tiene que ser indexado")
    a = np.asarray(im).astype(np.uint8)
    cols = runs((a != 0).any(axis=0))
    if len(cols) != 4:
        sys.exit("april.png: %d frames, se esperaban 4" % len(cols))

    out = np.zeros((CH * len(ROWS), CW * 2), np.uint8)
    for r, row in enumerate(ROWS):
        for c, fi in enumerate(row):
            c0, c1 = cols[fi]
            sub = a[:, c0:c1 + 1]
            ys = np.nonzero((sub != 0).any(axis=1))[0]
            sub = sub[ys.min():ys.max() + 1]
            h, w = sub.shape
            feet = sub[max(0, h - FEET_ROWS):]
            xs = np.nonzero((feet != 0).any(axis=0))[0]
            fx = (int(xs.min()) + int(xs.max())) // 2
            x0 = c * CW + FEET_X - fx
            y0 = r * CH + CH - h
            if x0 < c * CW or x0 + w > (c + 1) * CW or h > CH:
                sys.exit("April frame %d no entra en la celda de %dx%d" % (fi, CW, CH))
            out[y0:y0 + h, x0:x0 + w] = sub
    # (12/10) Fila 2: se suelta. Parada (fila 0, frame 0) con la banda de
    # sogas de la celda atada (fila 1, frame 0) cayendo por las piernas.
    out = np.vstack([out, np.zeros((2 * CH, CW * 2), np.uint8)])
    stand = out[0:CH, 0:CW]
    rope = out[CH + ROPE_ROWS[0]:CH + ROPE_ROWS[1], ROPE_X[0]:ROPE_X[1]].copy()
    rx = FEET_X - (ROPE_X[1] - ROPE_X[0]) // 2
    for k, ry in enumerate(ROPE_DROP):
        cell = stand.copy()
        h = min(rope.shape[0], CH - ry)
        m = rope[:h] != 0
        cell[ry:ry + h, rx:rx + rope.shape[1]][m] = rope[:h][m]
        out[2 * CH:3 * CH, k * CW:(k + 1) * CW] = cell
    # Fila 3: saludando (pies abajo, centrados en FEET_X).
    wv = np.asarray(Image.open(WAVE)).astype(np.uint8)
    h, w = wv.shape
    feet = wv[h - FEET_ROWS:]
    xs = np.nonzero((feet != 0).any(axis=0))[0]
    fx = (int(xs.min()) + int(xs.max())) // 2
    for c in range(2):
        x0 = c * CW + FEET_X - fx
        if x0 < c * CW or x0 + w > (c + 1) * CW or h > CH:
            sys.exit("april_wave.png no entra en la celda de %dx%d" % (CW, CH))
        out[3 * CH + CH - h:4 * CH, x0:x0 + w] = wv
    img = Image.fromarray(out, "P")
    img.putpalette(im.getpalette())
    img.save(DST, transparency=0)
    print("%s  %dx%d" % (os.path.basename(DST), out.shape[1], out.shape[0]))


if __name__ == "__main__":
    main()
