#!/usr/bin/env python3
# =============================================================================
# gen_baxter_boom.py (30/09) -- explosion de la nave de Baxter en 4 cuartos
# =============================================================================
# FUENTE: res/sprites/baxter_boom.png (1008x144, la tira que vino del Ray
# Project; NO se toca). No es una grilla: son 8 explosiones de tamanos
# distintos una al lado de la otra (la mas grande mide 128x128). El .res viejo
# la declaraba como frames de 48x72 (6x9 tiles), asi que rescomp la cortaba en
# 21 pedazos de 48x72 y en dos filas: el juego mostraba solo tajadas de la
# fila de arriba -> "la explosion no sale completa".
#
# SALIDA: res/sprites/baxter_boom_gen.png, 512x256:
#   - cada explosion se centra en un lienzo de 128x128 y se parte en CUATRO
#     cuartos de 64x64 (8x8 tiles = 4 sprites de hardware de 32x32 cada uno);
#   - fila 0 = cuarto de arriba a la izquierda, 1 = arriba a la derecha,
#     2 = abajo a la izquierda, 3 = abajo a la derecha; columna = frame (8).
# baxter.c crea 4 Sprite con esta misma definicion, uno por fila (anim), y los
# junta alrededor del centro de la nave. Mismo indice de paleta que la fuente.
#
# Uso: python3 tools/gen_baxter_boom.py
# =============================================================================
import os
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SRC = os.path.join(ROOT, 'res', 'sprites', 'baxter_boom.png')
DST = os.path.join(ROOT, 'res', 'sprites', 'baxter_boom_gen.png')
C = 128           # lienzo de cada frame
Q = C // 2        # cuarto


def main():
    im = Image.open(SRC)
    assert im.mode == 'P'
    px = im.load()
    W, H = im.size
    cols = [any(px[x, y] for y in range(H)) for x in range(W)]
    runs, x = [], 0
    while x < W:
        if cols[x]:
            s = x
            while x < W and cols[x]:
                x += 1
            runs.append((s, x - 1))
        x += 1
    out = Image.new('P', (Q * len(runs), Q * 4), 0)
    out.putpalette(im.getpalette())
    for f, (a, b) in enumerate(runs):
        ys = [y for y in range(H) if any(px[xx, y] for xx in range(a, b + 1))]
        top, bot = min(ys), max(ys)
        w, h = b - a + 1, bot - top + 1
        assert w <= C and h <= C, (f, w, h)
        canvas = Image.new('P', (C, C), 0)
        canvas.paste(im.crop((a, top, b + 1, bot + 1)), ((C - w) // 2, (C - h) // 2))
        for q in range(4):
            qx, qy = (q & 1) * Q, (q >> 1) * Q
            out.paste(canvas.crop((qx, qy, qx + Q, qy + Q)), (f * Q, q * Q))
        print('frame %d: x %d..%d  %dx%d' % (f, a, b, w, h))
    out.save(DST, transparency=0)
    print('%s: %d frames' % (DST, len(runs)))


if __name__ == '__main__':
    main()
