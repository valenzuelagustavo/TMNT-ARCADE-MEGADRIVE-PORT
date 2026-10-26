#!/usr/bin/env python3
# =============================================================================
# gen_slash_sheet.py - hoja de SLASH en el formato de las tortugas (26/10)
# =============================================================================
# Entrada: res/sprites/slash_spritesheet.png (384x560, celdas de 96x80, 7 filas:
#   0 idle (3)  1 caminar (3)  2 cubrirse (1)  3 saltar (4, bola)  4 golpeado (1)
#   5 ataque 1 (3)  6 ataque 2 (3)). Tiene su propia paleta.
# Salida:  res/sprites/players/slash_new_13x13.png: celdas de 104x104 (centro
#   x=52, pies y=95), UNA FILA POR PlayerAnim (res/player.h) hasta ANIM_MANHOLE
#   (22 filas), para que todo el motor del jugador (golpes, KO, volteo...) siga
#   funcionando sin cambios. Lo que Slash no tiene se rellena con la pose
#   "golpeado". (26/10) El arte conserva SUS COLORES ORIGINALES: la hoja lleva
#   la paleta de Slash, con los colores REORDENADOS (asignacion optima, ver
#   assign) para que cada uno caiga en el indice de PAL1 cuyo color de las
#   tortugas se le parece: asi el HUD y lo demas que usa PAL1 (que esta dibujado
#   con los indices de las tortugas) queda lo mas parecido posible. En modo
#   Slash los niveles cargan esta paleta en PAL1 (playerSheetPalette).
# Tambien imprime el alcance de los golpes (px desde el centro, mirando a la
# derecha) para SLASH_REACH_* de res/player.c.
# Uso:  python3 tools/gen_slash_sheet.py [--preview carpeta]
# =============================================================================
import os
import sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, 'res', 'sprites', 'slash_spritesheet.png')
LEO = os.path.join(ROOT, 'res', 'sprites', 'players', 'leo_new_13x13.png')
OUT = os.path.join(ROOT, 'res', 'sprites', 'players', 'slash_new_13x13.png')
SC_W, SC_H = 96, 80       # celda de la hoja de origen
CELL = 104
FEET_Y = 95
# frames por fila de origen
SRC_FRAMES = {0: 3, 1: 3, 2: 1, 3: 4, 4: 1, 5: 3, 6: 3}

# (26/10) Ya no se remapea a mano: ver assign().


def pal_of(im):
    p = im.getpalette()[:48]
    return [tuple(p[i * 3:i * 3 + 3]) for i in range(16)]


def assign(sp, dp):
    """Permutacion indice-de-Slash -> indice-de-PAL1 (1..15 <-> 1..15) de costo
    minimo (distancia de color + brillo). Hungarian O(n^3), n = 15."""
    n = 15
    cost = np.zeros((n, n))
    for a in range(n):
        for b in range(n):
            s_ = np.array(sp[a + 1], float)
            d_ = np.array(dp[b + 1], float)
            cost[a, b] = ((s_ - d_) ** 2).sum() + 2.0 * (s_.sum() / 3 - d_.sum() / 3) ** 2
    # Hungarian (e-maxx)
    INF = 1e18
    u = [0.0] * (n + 1); v = [0.0] * (n + 1); p = [0] * (n + 1); way = [0] * (n + 1)
    for i in range(1, n + 1):
        p[0] = i; j0 = 0
        minv = [INF] * (n + 1); used = [False] * (n + 1)
        while True:
            used[j0] = True
            i0 = p[j0]; delta = INF; j1 = 0
            for j in range(1, n + 1):
                if not used[j]:
                    cur = cost[i0 - 1][j - 1] - u[i0] - v[j]
                    if cur < minv[j]:
                        minv[j] = cur; way[j] = j0
                    if minv[j] < delta:
                        delta = minv[j]; j1 = j
            for j in range(n + 1):
                if used[j]:
                    u[p[j]] += delta; v[j] -= delta
                else:
                    minv[j] -= delta
            j0 = j1
            if p[j0] == 0:
                break
        while True:
            j1 = way[j0]; p[j0] = p[j1]; j0 = j1
            if j0 == 0:
                break
    m = np.zeros(256, np.uint8)
    for j in range(1, n + 1):
        m[p[j]] = j
    return m


def main():
    prev = sys.argv[sys.argv.index('--preview') + 1] if '--preview' in sys.argv else None
    sim = Image.open(SRC)
    src = np.array(sim)
    spal = pal_of(sim)
    cmap = assign(spal, pal_of(Image.open(LEO)))
    print('mapa', {i: int(cmap[i]) for i in range(1, 16)})

    def cell(srow, k):
        c = src[srow * SC_H:(srow + 1) * SC_H, k * SC_W:(k + 1) * SC_W]
        out = np.zeros((CELL, CELL), np.uint8)
        y0 = FEET_Y - (SC_H - 1)
        x0 = (CELL - SC_W) // 2
        o = cmap[c]
        o[c == 0] = 0
        out[y0:y0 + SC_H, x0:x0 + SC_W] = o
        return out

    empty = np.zeros((CELL, CELL), np.uint8)
    idle = [cell(0, k) for k in range(3)]
    walk = [cell(1, k) for k in range(3)]
    guard = [cell(2, 0)]
    ball = [cell(3, k) for k in range(4)]
    hit = [cell(4, 0)]
    atk1 = [cell(5, k) for k in range(3)]
    atk2 = [cell(6, k) for k in range(3)]
    rows = [idle, idle, atk1, atk1, atk2, atk2, ball, ball, walk, walk, guard,
            hit, hit, hit, hit, hit, hit, hit,
            hit * 4, hit * 3, hit * 4,
            [hit[0], empty, hit[0]]]
    assert len(rows) == 22
    sheet = np.zeros((CELL * len(rows), CELL * 4), np.uint8)
    for r, fr in enumerate(rows):
        for k, f in enumerate(fr):
            sheet[r * CELL:(r + 1) * CELL, k * CELL:(k + 1) * CELL] = f
    # paleta de la hoja: colores de Slash en el indice que les asigno assign()
    newpal = [(0, 0, 0)] * 16
    for i in range(1, 16):
        newpal[int(cmap[i])] = spal[i]
    lp = [c for col in newpal for c in col] + [0] * (768 - 48)
    res = Image.fromarray(sheet, 'P')
    res.putpalette(lp)
    res.save(OUT, transparency=0, optimize=False)
    print('slash', res.size)
    # alcance de los golpes: pixel opaco mas a la derecha (offset desde el centro)
    for name, fr in (('ATTACK_1', atk1), ('ATTACK_2', atk2), ('JUMP', ball)):
        print(name, [int(np.nonzero(f.any(0))[0].max() - CELL // 2) for f in fr])
    if prev:
        os.makedirs(prev, exist_ok=True)
        rgb = np.array(res.convert('RGBA'))
        bg = Image.new('RGBA', res.size, (60, 60, 70, 255))
        bg.alpha_composite(Image.fromarray(rgb))
        bg.crop((0, 0, CELL * 4, CELL * 12)).save(os.path.join(prev, 'slash_rows.png'))


if __name__ == '__main__':
    main()
