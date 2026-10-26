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
#   "golpeado". El arte se REMAPEA a la paleta de las tortugas (PAL1, la de la
#   hoja de Leo): el HUD y los fades comparten esa linea, no hay una libre.
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

# Mapa manual de colores de Slash -> indices de PAL1 (la de las tortugas):
#  Leo: 1 negro, 2 verde oscuro, 3 oliva, 4 gris oscuro, 5 verde, 6 marron,
#  7 amarillo, 8 verde claro, 9 azul, 10 gris, 11 azul violeta, 12 violeta,
#  13 blanco, 14 rojo, 15 naranja
# Se completa solo (color mas cercano con peso al brillo); MANUAL pisa casos.
MANUAL = {2: 2, 3: 3, 4: 6, 6: 12, 7: 2, 9: 6, 10: 5, 13: 12}


def pal_of(im):
    p = im.getpalette()[:48]
    return [tuple(p[i * 3:i * 3 + 3]) for i in range(16)]


def build_map(sp, dp):
    m = np.zeros(256, np.uint8)
    for i in range(1, 16):
        if i in MANUAL:
            m[i] = MANUAL[i]
            continue
        s = np.array(sp[i], float)
        best, bd = 1, 1e18
        for j in range(1, 16):
            d_ = np.array(dp[j], float)
            d = ((s - d_) ** 2).sum() + 2.0 * (s.sum() / 3 - d_.sum() / 3) ** 2
            if d < bd:
                best, bd = j, d
        m[i] = best
    return m


def main():
    prev = sys.argv[sys.argv.index('--preview') + 1] if '--preview' in sys.argv else None
    sim = Image.open(SRC)
    src = np.array(sim)
    cmap = build_map(pal_of(sim), pal_of(Image.open(LEO)))
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
    lp = Image.open(LEO).getpalette()
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
