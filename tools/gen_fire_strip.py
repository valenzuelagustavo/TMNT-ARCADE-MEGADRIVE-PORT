#!/usr/bin/env python3
# =============================================================================
# gen_fire_strip.py  -  Fuego de primer plano (PAL2) a partir del sheet nuevo
# =============================================================================
# Entrada:  res/sprites/CHAMAS 7 CORES.png  (24/09, Gustavo)
#     702x322. Arriba a la izquierda hay una tira de muestras de la paleta
#     (filas 0..7) y debajo 8 frames de una BANDA ancha de fuego, en 4 filas x
#     2 columnas de ~345x63 px. Los 7 colores del fuego son un subconjunto de
#     la paleta de los foot soldiers (ver gen_enemy_palette.py).
#
# Salida:   res/sprites/fire_strip_new.png  (64x512 = 8 frames de 64x64)
#     Que es el formato que ya consume el juego: el plano BG_A repite UNA celda
#     de 64x64 (8x8 tiles) a lo ancho, y scenes.c pisa esos 64 tiles por DMA
#     con el frame siguiente cada 8 frames de juego. La celda es lo unico que
#     vive en VRAM, asi que el ancho NO se puede agrandar: 64 tiles es todo el
#     presupuesto que tiene el fuego en el nivel 1.
#
# EL CORTE: como la celda se repite cada 64 px, la columna 63 queda pegada a la
# columna 0 de la copia siguiente. De las ~280 ventanas posibles de 64 px se
# elige la que MENOS costura deja, sumando el error de ese empalme en los 8
# frames (asi los 8 comparten la misma fase y la animacion no "salta"), con la
# altura de la llama como peso: lo que se nota es el contorno, no el relleno.
#
# Uso:  python3 tools/gen_fire_strip.py
# =============================================================================
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPR = os.path.join(ROOT, "res", "sprites")
SRC = os.path.join(SPR, "CHAMAS 7 CORES.png")
DST = os.path.join(SPR, "fire_strip_new.png")
PURPLE = os.path.join(SPR, "foot_soldier_purple_13x13.png")

CELL = 64          # celda de 8x8 tiles
FRAMES = 8

# Las 4 bandas del sheet (primera y ultima fila de cada una, medidas sobre el
# PNG) y las dos columnas. Se recortan por contenido, no a ojo.
ROW_BANDS = [(19, 81), (87, 151), (154, 216), (236, 297)]
COL_BANDS = [(0, 344), (355, 664)]


def main():
    im = Image.open(SRC)
    if im.mode != "P":
        sys.exit("El sheet del fuego tiene que ser indexado")
    a = np.asarray(im).astype(np.uint8)
    pal = im.getpalette()
    src_rgb = [tuple(pal[i * 3:i * 3 + 3]) for i in range(16)]

    # Paleta destino = la de los foot soldiers ya acomodada (indice 0 libre).
    pim = Image.open(PURPLE)
    ppal = pim.getpalette()
    dst_rgb = [tuple(ppal[i * 3:i * 3 + 3]) for i in range(16)]

    # Mapa de indices: blanco (fondo del sheet) -> 0 transparente, el resto por
    # color EXACTO (los 7 del fuego estan en la paleta de los soldiers).
    lut = np.zeros(256, np.uint8)
    for i in range(16):
        c = src_rgb[i]
        if c == (255, 255, 255):
            lut[i] = 0
            continue
        if c in dst_rgb[1:]:
            lut[i] = dst_rgb.index(c)
        else:                       # por las dudas: cercania
            d = [sum((x - y) ** 2 for x, y in zip(c, t)) for t in dst_rgb[1:]]
            lut[i] = d.index(min(d)) + 1

    # Recorte de los 8 frames, en orden de lectura (fila, columna).
    bands = []
    for r0, r1 in ROW_BANDS:
        for c0, c1 in COL_BANDS:
            b = lut[a[r0:r1 + 1, c0:c1 + 1]]
            if b.shape[0] > CELL:
                b = b[b.shape[0] - CELL:]          # ninguna banda llega a 64
            bands.append(b)
    if len(bands) != FRAMES:
        sys.exit("Se esperaban %d frames, salieron %d" % (FRAMES, len(bands)))

    width = min(b.shape[1] for b in bands)
    bands = [b[:, :width] for b in bands]

    # --- Elegir la ventana de 64 px con menos costura -----------------------
    # Costo = diferencia entre la columna que cierra la celda (x+63) y la que
    # la abre (x), sumada en los 8 frames. Se compara "hay pixel o no" (el
    # contorno de la llama) y ademas el indice, que es un degrade de brillo.
    best, bestcost = 0, None
    for x in range(width - CELL + 1):
        cost = 0.0
        for b in bands:
            left = b[:, x].astype(np.int16)
            right = b[:, x + CELL - 1].astype(np.int16)
            cost += float(np.abs((left > 0).astype(np.int16) -
                                 (right > 0).astype(np.int16)).sum()) * 4.0
            cost += float(np.abs(left - right).sum()) * 0.25
        if bestcost is None or cost < bestcost:
            best, bestcost = x, cost
    print("ventana elegida x=%d..%d de %d px (costo %.0f)"
          % (best, best + CELL - 1, width, bestcost))

    out = np.zeros((CELL * FRAMES, CELL), np.uint8)
    for i, b in enumerate(bands):
        cell = b[:, best:best + CELL]
        # Pegado ABAJO de la celda: el fuego crece desde el piso y arriba queda
        # transparente, igual que el fire_strip viejo.
        out[i * CELL + (CELL - cell.shape[0]): (i + 1) * CELL] = cell

    img = Image.fromarray(out, "P")
    flat = [c for rgb in dst_rgb for c in rgb] + [0] * (768 - 48)
    img.putpalette(flat)
    img.save(DST, transparency=0)
    print("%s  %dx%d" % (os.path.basename(DST), CELL, CELL * FRAMES))


if __name__ == "__main__":
    main()
