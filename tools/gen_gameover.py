# -*- coding: utf-8 -*-
# Genera el arte del CONTINUE grande y del cartel de GAME OVER (02/11).
#
#   res/images/gameover/gameover_sign.png    112x32, UN solo cuadro del cartel.
#       Sale de res/sprites/gameover.png (560x32 = 5 variantes de 112x32 con la
#       MISMA forma y distintos colores). Cada pixel pertenece a una "clase"
#       (la tupla de indices que tiene en las 5 variantes); a cada clase se le
#       da un indice fijo y el efecto se logra rotando la PALETA (5 juegos de
#       colores). Un sprite admite 15 colores opacos y hay 16 clases coloreadas:
#       se funden las dos mas parecidas (el costo se imprime).
#   src/gameover_pal.h                       las 5 paletas (formato Mega Drive).
#   res/images/gameover/continue_digits.png  400x56: los digitos 0-9 en rojo
#       (40x56 c/u, estilo 7 segmentos del arcade), 10 cuadros de una fila.
#   res/images/gameover/continue_label.png   64x8: "CONTINUE" con la fuente del HUD.
#
# El digito y el rotulo usan la paleta de las tortugas (PAL1: el rojo es el
# indice 14), asi no hace falta una linea de paleta propia.
#
# Uso:  python3 tools/gen_gameover.py      (desde la raiz del proyecto)
import os
import collections
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "res", "sprites", "gameover.png")
TURTLE = os.path.join(ROOT, "res", "sprites", "players", "leo_new_13x13.png")
FONT = os.path.join(ROOT, "res", "images", "font", "font_tmnt_arcade_2.png")
DST = os.path.join(ROOT, "res", "images", "gameover")
PAL_H = os.path.join(ROOT, "src", "gameover_pal.h")

FRAMES = 5
FW, FH = 112, 32
RED = 14                          # rojo de PAL1 (ce0000)
DW, DH, DT = 40, 56, 6            # digito: ancho, alto, grosor del trazo


def lvl(v):                       # 0..255 -> nivel Mega Drive 0..7 (como rescomp)
    return v >> 5


def md(rgb):
    r, g, b = (lvl(c) for c in rgb)
    return (b << 9) | (g << 5) | (r << 1)


def dist(a, b):
    return sum((lvl(x) - lvl(y)) ** 2 for x, y in zip(a, b))


def sign():
    im = Image.open(SRC)
    assert im.mode == "P" and im.size == (FW * FRAMES, FH)
    pal = im.getpalette()
    rgb = lambda i: tuple(pal[3 * i:3 * i + 3])
    px = im.load()
    cls = collections.Counter()
    for y in range(FH):
        for x in range(FW):
            cls[tuple(px[x + f * FW, y] for f in range(FRAMES))] += 1
    transp = (0,) * FRAMES
    black = (1,) * FRAMES
    assert transp in cls and black in cls
    colored = [c for c in cls if c not in (transp, black)]
    # Se funden pares hasta que queden 14 (indices 2..15; el 1 es el negro).
    members = {c: [c] for c in colored}
    while len(colored) > 14:
        best = None
        for i in range(len(colored)):
            for j in range(i + 1, len(colored)):
                a, b = colored[i], colored[j]
                cost = sum(dist(rgb(a[f]), rgb(b[f])) for f in range(FRAMES))
                if best is None or cost < best[0]:
                    best = (cost, a, b)
        cost, a, b = best
        keep, drop = (a, b) if cls[a] >= cls[b] else (b, a)
        print("funde %s (%d px) en %s (%d px), costo %d" % (drop, cls[drop], keep, cls[keep], cost))
        members[keep] += members.pop(drop)
        cls[keep] += cls[drop]
        colored.remove(drop)
    colored.sort(key=lambda c: -cls[c])
    index = {transp: 0, black: 1}
    for n, c in enumerate(colored):
        for m in members[c]:
            index[m] = 2 + n
    out = Image.new("P", (FW, FH), 0)
    o = out.load()
    for y in range(FH):
        for x in range(FW):
            o[x, y] = index[tuple(px[x + f * FW, y] for f in range(FRAMES))]
    # Paleta del PNG = cuadro 0 (solo para verlo; el juego usa gameover_pal.h).
    p = [0] * 768
    frames = []
    for f in range(FRAMES):
        row = [md(rgb(1))]                       # indice 1: negro
        for c in colored:
            row.append(md(rgb(c[f])))
        assert len(row) == 15
        frames.append(row)
    for i, v in enumerate(frames[0]):
        p[3 * (i + 1) + 0] = ((v >> 1) & 7) << 5
        p[3 * (i + 1) + 1] = ((v >> 5) & 7) << 5
        p[3 * (i + 1) + 2] = ((v >> 9) & 7) << 5
    out.putpalette(p)
    out.save(os.path.join(DST, "gameover_sign.png"))
    with open(PAL_H, "w") as h:
        h.write("// GENERADO por tools/gen_gameover.py -- no editar a mano.\n")
        h.write("// Paletas (colores 1..15) del cartel GAME OVER, una por variante.\n")
        h.write("// El indice 1 es el negro del cartel; el 0 es transparente.\n")
        h.write("#ifndef _GAMEOVER_PAL_H_\n#define _GAMEOVER_PAL_H_\n\n")
        h.write("#define GO_PAL_FRAMES %d\n\n" % FRAMES)
        h.write("static const u16 goPalFrames[GO_PAL_FRAMES][15] = {\n")
        for row in frames:
            h.write("    { " + ", ".join("0x%04X" % v for v in row) + " },\n")
        h.write("};\n\n#endif\n")


SEG = {  # a=arriba b=sup.der c=inf.der d=abajo e=inf.izq f=sup.izq g=medio
    0: "abcdef", 1: "bc", 2: "abged", 3: "abgcd", 4: "fgbc",
    5: "afgcd", 6: "afgedc", 7: "abc", 8: "abcdefg", 9: "abcdfg",
}


def digits(pal):
    mid = (DH - DT) // 2
    rect = {
        "a": (0, 0, DW, DT), "d": (0, DH - DT, DW, DT), "g": (0, mid, DW, DT),
        "f": (0, 0, DT, mid + DT), "e": (0, mid, DT, DH - mid),
        "b": (DW - DT, 0, DT, mid + DT), "c": (DW - DT, mid, DT, DH - mid),
    }
    im = Image.new("P", (DW * 10, DH), 0)
    o = im.load()
    for n in range(10):
        if n == 1:   # el 1 va centrado (se muestra solo, no pegado a otro digito)
            cx = (DW - DT) // 2
            boxes = [(cx, 0, DT, DH)]
        else:
            boxes = [rect[s] for s in SEG[n]]
        for (x0, y0, w, h) in boxes:
            for y in range(y0, y0 + h):
                for x in range(x0, x0 + w):
                    o[n * DW + x, y] = RED
    im.putpalette(pal)
    im.save(os.path.join(DST, "continue_digits.png"))


def label(pal):
    font = Image.open(FONT)
    assert font.mode == "P" and font.size == (760, 8)
    fp = font.load()
    text = "CONTINUE"
    im = Image.new("P", (8 * len(text), 8), 0)
    o = im.load()
    for k, ch in enumerate(text):
        gx = (ord(ch) - 32) * 8
        for y in range(8):
            for x in range(8):
                o[k * 8 + x, y] = fp[gx + x, y]
    im.putpalette(pal)
    im.save(os.path.join(DST, "continue_label.png"))


def main():
    os.makedirs(DST, exist_ok=True)
    pal = Image.open(TURTLE).getpalette()[:48]
    pal = pal + [0] * (768 - len(pal))
    sign()
    digits(pal)
    label(pal)
    print("ok")


main()
