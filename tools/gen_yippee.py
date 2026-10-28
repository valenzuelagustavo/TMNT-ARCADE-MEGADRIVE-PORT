#!/usr/bin/env python3
# =============================================================================
# gen_yippee.py  -  Globo de Baxter "Yippee-I-Ay!" (27/10)
# =============================================================================
# La captura que paso el equipo vino escalada ~3,26x, asi que no se puede
# usar directa: se redibuja el globo a su tamano real (59x29) con la fuente
# del arcade (x-height 5, mayusculas 7, descendentes 2, 1 px entre letras) y
# queda en un lienzo de 64x32 (se declara "8 4" en el .res).
# Indices de PAL1 como el resto de los globos: relleno 11, borde 9, texto 8.
# Salida: res/sprites/yippee_baloon.png
# =============================================================================
import os
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPR = os.path.join(ROOT, "res", "sprites")
PAL_REF = os.path.join(SPR, "help_baloon.png")

FILL, BORDER, TEXT = 11, 9, 8

G = {
    "Y": ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#..", ".....", "....."],
    "i": ["#", ".", "#", "#", "#", "#", "#", ".", "."],
    "p": [".....", ".....", "###.", "#..#", "#..#", "#..#", "###.", "#...", "#..."],
    "e": [".....", ".....", ".##.", "#..#", "####", "#...", ".###", ".....", "....."],
    "-": [".."] * 4 + ["##"] + [".."] * 4,
    "I": ["#"] * 7 + ["."] * 2,
    "A": ["..#..", ".#.#.", ".#.#.", "#...#", "#####", "#...#", "#...#", ".....", "....."],
    "y": [".....", ".....", "#..#", "#..#", "#..#", ".###", "...#", "#..#", ".##."],
    "!": ["#", "#", "#", "#", "#", ".", "#", ".", "."],
}
TEXTO = "Yippee-I-Ay!"
# el 'p' tiene 4 columnas y la altura de x-height arranca en la fila 2
G["p"] = [r[:4] if len(r) > 4 else r for r in G["p"]]
G["e"] = [r[:4] for r in G["e"]]
G["y"] = [r[:4] for r in G["y"]]

W, HB = 59, 26          # cuerpo
CX = 31                 # punta de la cola
CANVAS_W, CANVAS_H, OX = 64, 32, 2


def main():
    g = np.zeros((CANVAS_H, CANVAS_W), np.uint8)
    b = np.zeros((HB, W), np.uint8)
    for y in range(HB):
        for x in range(W):
            b[y, x] = FILL
    # borde con esquinas redondeadas
    for x in range(W):
        if 3 <= x <= W - 4:
            b[0, x] = BORDER
            b[HB - 1, x] = BORDER
    for y, xs in ((1, (2,)), (2, (1,))):
        for x in xs:
            b[y, x] = BORDER; b[y, W - 1 - x] = BORDER
            b[HB - 1 - y, x] = BORDER; b[HB - 1 - y, W - 1 - x] = BORDER
    for y in range(3, HB - 3):
        b[y, 0] = BORDER; b[y, W - 1] = BORDER
    # las esquinas del borde en x=0 ya cubren filas 3..HB-4; cierre en 0/1
    # cola hacia abajo
    for x in range(CX - 2, CX + 3):
        b[HB - 1, x] = FILL
    tail = [(HB, CX - 2, CX + 2), (HB + 1, CX - 1, CX + 1), (HB + 2, CX, CX)]
    tb = np.zeros((CANVAS_H - HB, W), np.uint8)
    for (r, x0, x1) in tail:
        tb[r - HB, x0] = BORDER; tb[r - HB, x1] = BORDER
        for x in range(x0 + 1, x1):
            tb[r - HB, x] = FILL
    tb[HB + 3 - HB, CX] = BORDER
    g[0:HB, OX:OX + W] = b
    g[HB:CANVAS_H, OX:OX + W] = np.where(tb > 0, tb, g[HB:CANVAS_H, OX:OX + W])
    # lo que queda fuera del borde (esquinas) es transparente: relleno desde
    # el borde del lienzo sin cruzar el borde del globo
    seen = np.zeros(g.shape, bool)
    st = [(0, 0), (0, CANVAS_W - 1), (CANVAS_H - 1, 0), (CANVAS_H - 1, CANVAS_W - 1)]
    while st:
        y, x = st.pop()
        if y < 0 or x < 0 or y >= CANVAS_H or x >= CANVAS_W or seen[y, x] or g[y, x] == BORDER:
            continue
        seen[y, x] = True
        st += [(y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)]
    g[seen] = 0
    # texto centrado
    tw = sum(len(G[c][0]) for c in TEXTO) + len(TEXTO) - 1
    cx = OX + (W - tw) // 2
    top = 1 + (HB - 2 - 9) // 2
    for c in TEXTO:
        gl = G[c]
        for yy, row in enumerate(gl):
            for xx, ch in enumerate(row):
                if ch == "#":
                    g[top + yy, cx + xx] = TEXT
        cx += len(gl[0]) + 1
    im = Image.fromarray(g, "P")
    im.putpalette(Image.open(PAL_REF).getpalette())
    p = os.path.join(SPR, "yippee_baloon.png")
    im.save(p, transparency=0)
    print(os.path.basename(p), CANVAS_W, CANVAS_H)
    pal = np.array([(255, 0, 255)] * 16, np.uint8)
    pal[FILL] = (123, 123, 214); pal[BORDER] = (82, 231, 255); pal[TEXT] = (255, 239, 0)
    Image.fromarray(pal[g]).resize((CANVAS_W * 8, CANVAS_H * 8), Image.NEAREST).save("/tmp/yippee_prev.png")


if __name__ == "__main__":
    main()
