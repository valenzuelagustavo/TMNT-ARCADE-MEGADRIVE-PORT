# -*- coding: utf-8 -*-
# gen_factory_splinter.py (18/10) -- Splinter atado en la pared (jefe de 5-1)
#
# Fuente: res/images/lvl_7_factory/splinter_tide.png (152x75, 4 frames de
# 38x75, paleta propia; la soga es la columna 33-35 de cada celda):
#   frame 0      quieto
#   frames 1..3  intenta romper las ataduras
# Se arma cada frame en una celda de 40x112 (5x14 tiles) que va con el borde
# de arriba en y=0 de la pantalla: el arte con ROPE_TOP filas libres arriba y
# la SOGA estirada hacia arriba hasta la fila 0, siguiendo el patron de 3
# filas (bb1 / bc1 / bc1) de cada frame. Despues la celda se parte en dos:
#   splinter_tide_ts.png    filas 32-111 (5x10 tiles) de los 4 frames, uno
#       abajo del otro (40x320): TILESET que va en BG_A (prioridad baja) como
#       "overlay" del primer plano. Va en un PLANO y no como sprite porque en
#       la pelea, con Granitor (144 px) y las tortugas en las mismas lineas,
#       el limite de pixeles de sprite por linea se comia media figura.
#   splinter_rope.png       filas 0-31, columna de tiles 4 (x 32-39): el
#       tramo de soga que cae en la franja del HUD (BG_A ahi no scrollea),
#       como SPRITE de 1x4 tiles, 4 frames (sincronizados con el resto).
#
#   - se borran los pixeles sueltos (grupos de 1-2 pixeles, basura del rip);
#   - los colores se pasan a la paleta de las TORTUGAS (leo_new_13x13.png,
#     PAL1): en la fabrica PAL0 es el fondo, PAL2 los foot soldiers y PAL3
#     Granitor (que ademas parpadea), asi que Splinter no tiene paleta propia.
#     Cada color va al mas parecido de esa paleta.
#
# Uso:  python3 tools/gen_factory_splinter.py   (desde la raiz del proyecto)
import os
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
D = os.path.join(ROOT, "res", "images", "lvl_7_factory")
PLAYER = os.path.join(ROOT, "res", "sprites", "players", "leo_new_13x13.png")
SRC_W, SRC_H, N = 38, 75, 4
CELL_W, CELL_H = 40, 112
OX = 2                       # columna de la celda donde va la col 0 del arte
ROPE_X = 33                  # columnas 33..35 del arte: la soga
ROPE_TOP = 31                # filas para estirar la soga (cabeza en y~32, como el arcade)
TOP_ROWS = 32                # filas que van en el sprite de la soga
ROPE_TX = 4                  # columna de tiles de la soga (x 32-39 de la celda)


def nearest(c, pal):
    return min(range(1, 16), key=lambda j: sum((a - b) ** 2 for a, b in zip(c, pal[j])))


def lv(p):
    return [tuple(v >> 5 for v in p[i * 3:i * 3 + 3]) for i in range(16)]


def main():
    src = Image.open(os.path.join(D, "splinter_tide.png"))
    assert src.mode == "P"
    spal = lv(src.getpalette()[:48])
    pp = Image.open(PLAYER).getpalette()[:48]
    dpal = lv(pp)
    remap = [0] + [nearest(spal[i], dpal) for i in range(1, 16)]

    out = Image.new("P", (CELL_W * N, CELL_H), 0)
    out.putpalette(pp)
    q = out.load()
    for k in range(N):
        cell = src.crop((k * SRC_W, 0, (k + 1) * SRC_W, SRC_H))
        px = cell.load()
        # pixeles sueltos: grupos (8-vecinos) de menos de 3 pixeles
        seen = set()
        for y in range(SRC_H):
            for x in range(SRC_W):
                if not px[x, y] or (x, y) in seen:
                    continue
                grp, st = [], [(x, y)]
                seen.add((x, y))
                while st:
                    a, b = st.pop()
                    grp.append((a, b))
                    for dx in (-1, 0, 1):
                        for dy in (-1, 0, 1):
                            u, v = a + dx, b + dy
                            if 0 <= u < SRC_W and 0 <= v < SRC_H and px[u, v] and (u, v) not in seen:
                                seen.add((u, v))
                                st.append((u, v))
                if len(grp) < 3:
                    for a, b in grp:
                        px[a, b] = 0
        bx = k * CELL_W + OX
        for y in range(SRC_H):
            for x in range(SRC_W):
                if px[x, y]:
                    q[bx + x, ROPE_TOP + y] = remap[px[x, y]]
        # soga: las 3 primeras filas del arte dan el patron (periodo 3)
        pat = [[px[ROPE_X + i, r] for i in range(3)] for r in range(3)]
        for y in range(ROPE_TOP):
            r = (y - ROPE_TOP) % 3
            for i in range(3):
                if pat[r][i]:
                    q[bx + ROPE_X + i, y] = remap[pat[r][i]]
    # todo lo que quede arriba de TOP_ROWS fuera de la columna de la soga se pierde
    for k in range(N):
        for y in range(TOP_ROWS):
            for x in range(CELL_W):
                if q[k * CELL_W + x, y] and not (ROPE_TX * 8 <= x < ROPE_TX * 8 + 8):
                    raise SystemExit("frame %d: pixel en (%d,%d) fuera de la soga" % (k, x, y))
    ts = Image.new("P", (CELL_W, (CELL_H - TOP_ROWS) * N), 0)
    ts.putpalette(pp)
    rope = Image.new("P", (8 * N, TOP_ROWS), 0)
    rope.putpalette(pp)
    for k in range(N):
        ts.paste(out.crop((k * CELL_W, TOP_ROWS, (k + 1) * CELL_W, CELL_H)), (0, k * (CELL_H - TOP_ROWS)))
        x0 = k * CELL_W + ROPE_TX * 8
        rope.paste(out.crop((x0, 0, x0 + 8, TOP_ROWS)), (k * 8, 0))
    ts.save(os.path.join(D, "splinter_tide_ts.png"), transparency=0)
    rope.save(os.path.join(D, "splinter_rope.png"), transparency=0)
    print("ok")


if __name__ == "__main__":
    main()
