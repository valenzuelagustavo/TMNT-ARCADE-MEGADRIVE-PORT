#!/usr/bin/env python3
# =============================================================================
# gen_hud_select.py -- HUD del nivel con el arte de la seleccion (09/10)
# =============================================================================
# FUENTE: res/images/hud/select player_15 CORES.png (arte de Gustavo, misma
# paleta que las tortugas: PAL1). Arriba tiene los cuatro marcos 1UP..4UP de
# 64x32 (contorno blanco con sombra verde, cartel "nUP" en el color de cada
# tortuga, la cabecita, el nombre y la barra de vida).
#
# SALIDAS
#   res/images/hud/hud_1p.png .. hud_4p.png   72x128: 4 filas de 72x32, una
#       por tortuga en orden de personaje (0 Leo, 1 Mike, 2 Don, 3 Raph). Cada
#       fila = el contorno + el cartel "nUP" con los colores de esa tortuga.
#       SIN la cabecita (en partida no va) y SIN el nombre (ahi va el puntaje).
#       El marco queda en x 5..69 de la celda, asi el interior cae sobre la
#       grilla de tiles del HUD (scenes.c):
#         puntaje  fila 1, columnas 5..7 (3 digitos)
#         vidas    columna 3, filas 2-3 (donde estaba la cabecita)
#         barra    columnas 4..7, filas 2-3 (las barras caen donde estan en el
#                  arte)
#       tools/gen_hud4_shared.py parte estos cuatro en contorno + carteles para
#       el modo de 4 jugadores.
#   res/sprites/hp_bar.png   32x704: la barra de vida, 4 tortugas x 11 frames
#       de 32x16 (frame 0 = llena .. 10 = vacia). 10 barras de 2 px con 1 px de
#       separacion y fondo negro, con los colores del arte de cada tortuga.
#       Tortuga t, frame f -> tiles ((t * 11) + f) * 8.
#
# El contorno se arma con el marco de Leo: el blanco del arte (sin el nombre,
# las barras ni la cabecita) y la esquina de abajo a la izquierda, que la
# cabecita tapa, espejada de la de la derecha. El verde es la sombra del blanco
# corrida 1 px a la derecha y abajo, como en el arte.
#
# Uso:  python3 tools/gen_hud_select.py ; python3 tools/gen_hud4_shared.py
# =============================================================================
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
HUD = os.path.join(ROOT, 'res', 'images', 'hud')
SRC = os.path.join(HUD, 'select player_15 CORES.png')
BAROUT = os.path.join(ROOT, 'res', 'sprites', 'hp_bar.png')

BOX_X = (13, 90, 167, 244)     # marcos 1UP..4UP en el arte
BOX_Y = 19
BOX_W, BOX_H = 66, 32
WHITE, GREEN, BLACK = 13, 2, 1
CELL_W, CELL_H = 72, 32
OX = 5                         # x del marco dentro de la celda
LABEL = (3, 27, 0, 16)         # x0, x1, y0, y1 (relativo al marco): cartel nUP

# Colores de cada tortuga: (relleno, sombra) del cartel y las 10 barras.
LABEL_COL = {0: (9, 11), 1: (7, 15), 2: (12, 11), 3: (14, 15)}
BAR_COL = {
    0: [11, 11, 9, 9, 9, 9, 13, 13, 13, 13],
    1: [15, 15, 7, 7, 7, 7, 13, 13, 13, 13],
    2: [11, 11, 12, 12, 12, 12, 13, 13, 13, 13],
    3: [14, 14, 15, 15, 15, 15, 13, 13, 13, 13],
}
BAR_W, BAR_H, BAR_FRAMES = 32, 16, 11
BAR_X0, BAR_Y0, BAR_TALL = 1, 4, 10   # primera barra y alto (dentro del bloque)


def outline(a):
    box = a[BOX_Y:BOX_Y + BOX_H, BOX_X[0]:BOX_X[0] + BOX_W]
    white = box == WHITE
    white[3:29, 32:61] = False          # interior derecho: nombre y barras
    white[12:29, 3:32] = False          # interior izquierdo: cabecita
    # esquina de abajo a la izquierda (la tapa la cabecita): espejo de la derecha
    for y in range(12, BOX_H):
        for x in range(0, 32):
            white[y, x] = white[y, 62 - x] if 62 - x < BOX_W else False
    green = np.zeros_like(white)
    green[1:, 1:] = white[:-1, :-1]
    green &= ~white
    out = np.zeros(white.shape, np.uint8)
    out[green] = GREEN
    out[white] = WHITE
    # control: fuera de la zona reconstruida tiene que dar igual que el arte
    ref = np.where(np.isin(box, (WHITE, GREEN)), box, 0)
    chk = np.ones_like(white)
    chk[3:29, 32:61] = False
    chk[12:29, 3:32] = False
    chk[:, :32] = False
    chk[LABEL[2]:LABEL[3], LABEL[0]:LABEL[1]] = False
    bad = int((out[chk] != ref[chk]).sum())
    print('contorno: %d pixeles distintos del arte en la parte copiada' % bad)
    return out


def label(a, n, t):
    """Cartel del jugador n (0..3) con los colores de la tortuga t."""
    x0, x1, y0, y1 = LABEL
    src = a[BOX_Y + y0:BOX_Y + y1, BOX_X[n] + x0:BOX_X[n] + x1].copy()
    fill, shade = LABEL_COL[n]
    tf, ts = LABEL_COL[t]
    out = np.zeros(src.shape, np.uint8)
    out[src == WHITE] = WHITE
    out[src == GREEN] = GREEN
    out[src == fill] = tf
    out[src == shade] = ts
    return out


def save(path, arr, pal):
    im = Image.fromarray(arr, 'P')
    im.putpalette(pal + [0] * (768 - len(pal)))
    im.save(path, transparency=0)
    print('%-28s %dx%d' % (os.path.relpath(path, ROOT), arr.shape[1], arr.shape[0]))


def main():
    im = Image.open(SRC)
    assert im.mode == 'P'
    a = np.array(im)
    pal = im.getpalette()[:48]
    base = outline(a)
    x0, x1, y0, y1 = LABEL
    for n in range(4):
        sheet = np.zeros((CELL_H * 4, CELL_W), np.uint8)
        for t in range(4):
            cell = np.zeros((CELL_H, CELL_W), np.uint8)
            cell[:, OX:OX + BOX_W] = base[:, :CELL_W - OX]
            # el recuadro del cartel es entero del cartel (con los pedazos de
            # contorno que tiene el arte ahi): gen_hud4_shared lo separa asi
            cell[y0:y1, OX + x0:OX + x1] = label(a, n, t)
            sheet[t * CELL_H:(t + 1) * CELL_H] = cell
        save(os.path.join(HUD, 'hud_%dp.png' % (n + 1)), sheet, pal)

    bar = np.zeros((BAR_H * BAR_FRAMES * 4, BAR_W), np.uint8)
    for t in range(4):
        for f in range(BAR_FRAMES):
            lit = 10 - f
            if lit == 0:
                continue
            y = (t * BAR_FRAMES + f) * BAR_H
            fr = bar[y:y + BAR_H]
            fr[BAR_Y0 - 1:BAR_Y0 + BAR_TALL + 1, BAR_X0 - 1:BAR_X0 + 3 * lit] = BLACK
            for i in range(lit):
                bx = BAR_X0 + 3 * i
                fr[BAR_Y0:BAR_Y0 + BAR_TALL, bx:bx + 2] = BAR_COL[t][i]
    save(BAROUT, bar, pal)


if __name__ == '__main__':
    main()
