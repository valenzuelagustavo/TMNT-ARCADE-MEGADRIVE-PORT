#!/usr/bin/env python3
# =============================================================================
# gen_robot_enemies.py (16/10) -- robot del latigo, Mousers y misil de la cloaca
# =============================================================================
# Fuente (de Gustavo), en la PALETA DEL FOOT BLANCO (la misma, indice por indice):
#   res/sprites/Arcade - Teenage Mutant Ninja Turtles - Enemies - Robot Enemies.png
#   415x1381. Trae: el tubo de gas del Technodrome (todavia sin usar), el misil
#   de la cloaca (2 frames con la llama), el salpicon del misil saliendo del
#   agua (5 frames), el robot del latigo completo, los Mousers AZULES (los de
#   los huecos de la cloaca) y los ROJOS (los de Baxter).
#
# Salidas (en res/sprites/):
#   robot_whip.png       robot del latigo, MISMO formato que antes (11x13 celdas
#                        de 184x80, cada pose en el mismo lugar de su celda). Las
#                        poses se buscaron en la hoja por silueta (IoU 1.000).
#                        Las que la hoja no trae (los largos del cable tenso de
#                        las filas 6/7/8, frames 1..3, y los frames 1..3 de la
#                        destruccion) salen de la hoja vieja (robot_whip_v1.png)
#                        pasadas a esta paleta.
#   whip_waves.png       laser del robot, idem (whip_waves_v1.png).
#   sewer_mouser.png     Mouser AZUL (paleta del blanco, PAL3 en la cloaca).
#   baxter_rat.png       Mouser ROJO de Baxter, pasado a la paleta de Baxter
#                        (con Baxter, PAL3 es la de baxter_boss).
#                        Las dos: celdas de 40x40, 7 columnas, 9 filas:
#                          0-1 de frente | 2-3 de costado | 4 hacia arriba |
#                          5 salto (7 frames) | 6 golpeado (7) | 7 muerto |
#                          8 SALE DEL HUECO (3 frames, nuevo)
#   sewer_missil.png     misil, 2 frames de 64x32 (paleta del blanco, PAL3; con
#                        Baxter no salen misiles).
#   sewer_mis_splash.png salpicon del misil al salir del agua, 5 frames de
#                        64x64 (PAL3).
#
# Uso: python3 tools/gen_robot_enemies.py
# =============================================================================
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SPR = os.path.join(ROOT, 'res', 'sprites')
SRC = os.path.join(SPR, 'Arcade - Teenage Mutant Ninja Turtles - Enemies - Robot Enemies.png')

# ---------------------------------------------------------------------------
# Robot y laser: (salida, fila, col, x, y, ancho, alto, flip, x celda, y celda)
POSES = [
    ("ROBOT",  0,  0,   16,  204,  25,  12, 0,   15,  66),
    ("ROBOT",  0,  1,   73,  196,  31,  27, 0,   12,  51),
    ("ROBOT",  0,  2,  131,  185,  35,  47, 0,   11,  33),
    ("ROBOT",  0,  3,  191,  177,  35,  64, 0,   11,  16),
    ("ROBOT",  0,  4,  253,  172,  32,  75, 0,   12,   4),
    ("ROBOT",  0,  5,  315,  171,  28,  77, 0,   13,   0),
    ("ROBOT",  0,  6,  317,  171,  26,  62, 0,   15,   4),
    ("ROBOT",  0,  7,   16,  250,  26,  59, 0,   16,   7),
    ("ROBOT",  0,  8,  136,  253,  26,  54, 0,   16,  12),
    ("ROBOT",  0,  9,  136,  253,  26,  54, 0,   16,  12),
    ("ROBOT",  1,  0,  196,  254,  26,  51, 0,   16,  15),
    ("ROBOT",  2,  0,  254,  254,  29,  52, 0,   16,  14),
    ("ROBOT",  2,  1,  314,  254,  29,  52, 0,    9,  14),
    ("ROBOT",  2,  2,  376,  254,  26,  51, 0,   13,  16),
    ("ROBOT",  3,  0,   10,  316,  37,  47, 0,    5,  20),
    ("ROBOT",  3,  1,   70,  316,  37,  47, 0,    5,  20),
    ("ROBOT",  3,  2,  130,  316,  38,  47, 0,    4,  20),
    ("ROBOT",  3,  3,  191,  316,  36,  47, 0,    6,  20),
    ("ROBOT",  3,  4,  252,  316,  34,  47, 0,    8,  20),
    ("ROBOT",  3,  5,  312,  316,  34,  47, 0,    8,  20),
    ("ROBOT",  4,  0,  373,  314,  32,  52, 0,   18,  15),
    ("ROBOT",  4,  1,    9,  374,  40,  52, 0,   26,  15),
    ("ROBOT",  4,  2,   68,  374,  41,  51, 0,   26,  17),
    ("ROBOT",  4,  3,  129,  374,  39,  51, 0,   29,  17),
    ("ROBOT",  4,  4,  191,  374,  36,  51, 0,   28,  17),
    ("ROBOT",  4,  5,  253,  374,  32,  51, 0,   20,  17),
    ("ROBOT",  4,  6,  310,  374,  38,  51, 0,    9,  18),
    ("ROBOT",  4,  7,  368,  374,  41,  51, 0,    2,  18),
    ("ROBOT",  4,  8,    7,  432,  44,  55, 0,   20,  14),
    ("ROBOT",  5,  0,   68,  434,  62,  52, 0,   18,  17),
    ("ROBOT",  5,  1,  144,  434,  70,  52, 0,   18,  17),
    ("ROBOT",  5,  2,  220,  434,  78,  52, 0,   18,  17),
    ("ROBOT",  5,  3,  306,  434,  86,  52, 0,   18,  17),
    ("ROBOT",  5,  4,   12,  494,  94,  52, 0,   18,  17),
    ("ROBOT",  5,  5,  148,  494, 102,  52, 0,   18,  17),
    ("ROBOT",  5,  6,  284,  494, 110,  52, 0,   18,  17),
    ("ROBOT",  5,  7,    0,  554, 118,  52, 0,   18,  17),
    ("ROBOT",  5,  8,  136,  554, 126,  52, 0,   18,  17),
    ("ROBOT",  5,  9,  272,  554, 134,  52, 0,   18,  17),
    ("ROBOT",  5, 10,   18,  614, 142,  52, 0,   18,  17),
    ("ROBOT",  6,  0,  245,  674,  48,  52, 0,   18,  17),
    ("ROBOT",  7,  0,  305,  674,  48,  52, 0,   18,  17),
    ("ROBOT",  8,  0,  365,  674,  48,  52, 0,   18,  17),
    ("ROBOT",  9,  0,   13,  734,  31,  51, 0,   20,  18),
    ("ROBOT",  9,  1,   71,  734,  35,  51, 0,   20,  17),
    ("ROBOT",  9,  2,  128,  734,  41,  51, 0,   20,  17),
    ("ROBOT",  9,  3,  186,  734,  45,  51, 0,   20,  17),
    ("ROBOT",  9,  4,    2,  794,  54,  51, 0,   20,  17),
    ("ROBOT",  9,  5,   69,  794,  60,  51, 0,   20,  17),
    ("ROBOT",  9,  6,  145,  794,  67,  51, 0,   20,  17),
    ("ROBOT",  9,  7,  224,  794,  70,  51, 0,   20,  17),
    ("ROBOT", 10,  0,   12,  851,  34,  57, 1,   17,  11),
    ("ROBOT", 10,  1,   70,  860,  37,  40, 1,   14,  27),
    ("ROBOT", 10,  2,  124,  862,  49,  36, 1,    3,  31),
    ("ROBOT", 11,  0,  181,  860,  55,  39, 1,    2,  39),
    ("ROBOT", 12,  0,  312,  316,  34,  47, 0,    8,  20),
    ("ROBOT", 12,  1,  312,  316,  34,  47, 0,    8,  20),
    ("LASER",  0,  0,  192,  446,  22,  15, 0,    0,   0),
    ("LASER",  1,  0,  313,  818,  32,   3, 0,    0,   6),
    ("LASER",  1,  1,  326,  818,  64,   3, 0,    0,   6),
    ("LASER",  1,  2,  311,  818,  96,   3, 0,    0,   6),
    ("LASER",  4,  0,  313,  818,  32,   3, 0,    0,   6),
    ("LASER",  4,  1,  326,  818,  64,   3, 0,    0,   6),
    ("LASER",  4,  2,  311,  818,  96,   3, 0,    0,   6),
]
# Frames que la hoja no trae: se toman de la hoja vieja y se pasan de paleta.
KEEP = [
    ("LASER",  2,  0),      # la chispa del frame 0 calza con otra pose de la
    ("LASER",  3,  0),      # hoja (blanca): se deja la vieja, como sus 1 y 2
    ("ROBOT",  6,  1),
    ("ROBOT",  6,  2),
    ("ROBOT",  6,  3),
    ("ROBOT",  7,  1),
    ("ROBOT",  7,  2),
    ("ROBOT",  7,  3),
    ("ROBOT",  8,  1),
    ("ROBOT",  8,  2),
    ("ROBOT",  8,  3),
    ("ROBOT", 11,  1),
    ("ROBOT", 11,  2),
    ("ROBOT", 11,  3),
    ("LASER",  0,  1),
    ("LASER",  0,  2),
    ("LASER",  2,  1),
    ("LASER",  2,  2),
    ("LASER",  3,  1),
    ("LASER",  3,  2),
]
ROBOT_OUT = {"ROBOT": ('robot_whip.png', 'robot_whip_v1.png', 184, 80),
             "LASER": ('whip_waves.png', 'whip_waves_v1.png', 96, 16)}

# ---------------------------------------------------------------------------
# Mousers. Posicion (fila, col) de cada pose en la grilla de 60 px de la hoja
# (azules desde y 920, rojos 240 px mas abajo), y como va: flip y anclaje.
#   'feet' : abajo del dibujo en la fila 38 y centrada en x 20 (como antes)
#   (x, y) : esquina en la celda (las que calzan exacto con la hoja vieja)
MOUSER_TOP = {'blue': 920, 'red': 1160}
SIDE = [(0, 3), (0, 4), (0, 5), (0, 6)]
SIDE2 = [(1, 6), (2, 0), (0, 5), (0, 6)]
MOUSER_ROWS = [
    # fila 0-1: de frente (2 frames)
    [((1, 4), 0, 'feet'), ((1, 5), 0, 'feet')],
    [((1, 4), 0, 'feet'), ((1, 5), 0, 'feet')],
    # fila 2-3: de costado
    [(p, 1, 'feet') for p in SIDE],
    [(p, 1, 'feet') for p in SIDE2],
    # fila 4: hacia arriba
    [((1, k), 0, 'feet') for k in range(4)],
    # fila 5: salto (7 frames iguales, como antes; se reparten a mano)
    [((2, 1), 0, (4, 1))] * 7,
    # fila 6: golpeado, da vueltas (mismo orden y lugar que la hoja vieja)
    [((3, 0), 0, (7, 5)), ((3, 1), 0, (12, 2)), ((3, 2), 0, (15, 5)),
     ((3, 3), 0, (7, 13)), ((3, 4), 0, (20, 0)), ((3, 5), 0, (5, 17)),
     ((2, 2), 0, (2, 8))],
    # fila 7: muerto, tirado
    [((2, 4), 0, (0, 24))],
    # fila 8: sale del hueco caminando (nuevo)
    [((0, 0), 0, 'feet'), ((0, 1), 0, 'feet'), ((0, 2), 0, 'feet')],
]

# Misil: frame 0 llama grande, frame 1 llama chica (la punta a la derecha).
MISSILE = [(363, 83, 413, 95), (307, 83, 350, 95)]
MIS_NOSE_X = (53, 53)       # punta en la celda de 64x32 (como el viejo)
MIS_CY = 16
# Salpicon: 5 frames en la grilla de 60 px desde y 110.
SPLASH_Y0 = 110


def pal_of(im):
    p = im.getpalette()[:48]
    return [tuple(p[i * 3:i * 3 + 3]) for i in range(16)]


def nearest_lut(src_pal, dst_pal):
    lut = np.zeros(256, np.uint8)
    for i in range(1, 16):
        c = src_pal[i]
        lut[i] = min(range(1, 16), key=lambda j: sum((u - v) ** 2 for u, v in zip(dst_pal[j], c)))
    return lut


def bbox(a):
    ys, xs = np.nonzero(a)
    return xs.min(), ys.min(), xs.max(), ys.max()


def save(arr, pal, name):
    im = Image.fromarray(arr, 'P')
    flat = [v for c in pal for v in c]
    im.putpalette(flat + [0] * (768 - len(flat)))
    im.save(os.path.join(SPR, name), transparency=0)
    print('->', name, arr.shape[1], 'x', arr.shape[0])


def piece_at(a, x0, y0, x1, y1):
    """Recorte de la hoja: solo la mancha que hay en esa caja."""
    return a[y0:y1 + 1, x0:x1 + 1].copy()


def mouser_piece(a, top, rc):
    """La pose (fila, col) de la grilla de Mousers, recortada a su dibujo."""
    r, k = rc
    cell = a[top + r * 60:top + r * 60 + 60, k * 60:k * 60 + 60]
    x0, y0, x1, y1 = bbox(cell != 0)
    return cell[y0:y1 + 1, x0:x1 + 1]


def fix_shadows(out, cell=40, red=10, gray=14, top=30):
    """(19/10) La hoja dibuja la SOMBRA de los Mousers con el indice 10, que en
    la paleta del blanco es ROJO (feedback: "Robot mousers have red shadow").
    En cada celda, las manchas de ese indice que arrancan de la fila 'top'
    para abajo son la sombra: pasan al gris oscuro 14 (el de las sombras de
    los foot blancos). Los rojos del cuerpo (ojos, boca) quedan."""
    from scipy import ndimage
    for cy in range(0, out.shape[0], cell):
        for cx in range(0, out.shape[1], cell):
            c = out[cy:cy + cell, cx:cx + cell]
            lab, n = ndimage.label(c == red, structure=np.ones((3, 3)))
            for i in range(1, n + 1):
                m = lab == i
                if np.nonzero(m)[0].min() >= top:
                    c[m] = gray


def main():
    src = Image.open(SRC)
    a = np.array(src)
    white = pal_of(src)

    # --- Robot y laser -----------------------------------------------------
    for key, (name, old, cw, ch) in ROBOT_OUT.items():
        oldim = Image.open(os.path.join(SPR, old))
        o = np.array(oldim)
        lut = nearest_lut(pal_of(oldim), white)
        out = np.zeros_like(o)
        for k, r, c, sx, sy, w, h, fl, dx, dy in POSES:
            if k != key:
                continue
            piece = a[sy:sy + h, sx:sx + w]
            if fl:
                piece = piece[:, ::-1]
            dst = out[r * ch + dy:r * ch + dy + h, c * cw + dx:c * cw + dx + w]
            np.copyto(dst, piece, where=piece != 0)
        for k, r, c in KEEP:
            if k != key:
                continue
            sl = (slice(r * ch, (r + 1) * ch), slice(c * cw, (c + 1) * cw))
            out[sl] = lut[o[sl]]
        save(out, white, name)

    # --- Mousers -----------------------------------------------------------
    baxter = pal_of(Image.open(os.path.join(SPR, 'baxter_boss.png')))
    to_baxter = nearest_lut(white, baxter)
    for color, name in (('blue', 'sewer_mouser.png'), ('red', 'baxter_rat.png')):
        out = np.zeros((40 * len(MOUSER_ROWS), 40 * 7), np.uint8)
        for r, row in enumerate(MOUSER_ROWS):
            for c, (rc, fl, anchor) in enumerate(row):
                p = mouser_piece(a, MOUSER_TOP[color], rc)
                if fl:
                    p = p[:, ::-1]
                h, w = p.shape
                if anchor == 'feet':
                    dx, dy = 20 - w // 2, 38 - (h - 1)
                else:
                    dx, dy = anchor
                dx = max(0, min(40 - w, dx))
                dy = max(0, min(40 - h, dy))
                dst = out[r * 40 + dy:r * 40 + dy + h, c * 40 + dx:c * 40 + dx + w]
                np.copyto(dst, p, where=p != 0)
        fix_shadows(out)
        if color == 'red':
            save(to_baxter[out], baxter, name)
        else:
            save(out, white, name)

    # --- Misil y salpicon --------------------------------------------------
    out = np.zeros((32, 64 * 2), np.uint8)
    for i, (x0, y0, x1, y1) in enumerate(MISSILE):
        p = piece_at(a, x0, y0, x1, y1)[:, ::-1]          # punta a la derecha
        h, w = p.shape
        dx = MIS_NOSE_X[i] - (w - 1)
        dy = MIS_CY - h // 2
        dst = out[dy:dy + h, i * 64 + dx:i * 64 + dx + w]
        np.copyto(dst, p, where=p != 0)
    save(out, white, 'sewer_missil.png')

    out = np.zeros((64, 64 * 5), np.uint8)
    for i in range(5):
        cell = a[SPLASH_Y0:SPLASH_Y0 + 60, i * 60:i * 60 + 60]
        x0, y0, x1, y1 = bbox(cell != 0)
        p = cell[y0:y1 + 1, x0:x1 + 1]
        h, w = p.shape
        dx, dy = 32 - w // 2, 63 - (h - 1) - 4        # el centro del agua abajo
        dst = out[dy:dy + h, i * 64 + dx:i * 64 + dx + w]
        np.copyto(dst, p, where=p != 0)
    save(out, white, 'sewer_mis_splash.png')


if __name__ == '__main__':
    main()
