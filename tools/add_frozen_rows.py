#!/usr/bin/env python3
# =============================================================================
# add_frozen_rows.py - filas ANIM_FROZEN / ANIM_FROZEN_BREAK en las 4 hojas (25/10)
# =============================================================================
# Entrada: res/sprites/turtle_frosen.png (256x128, celdas de 64x64):
#            fila 0 = 4 cuadros del bloque de hielo (la tortuga congelada),
#            fila 1 = 2 cuadros del hielo ROMPIENDOSE.
#          Tiene su PROPIA paleta (azules/cian) y las tortugas viven en PAL1 (la
#          de las hojas, compartida por las cuatro): el Technodrome ya usa las
#          cuatro paletas (PAL0 mapa, PAL1 tortugas, PAL2 morados, PAL3
#          blancos/columnas) y no hay una libre para el hielo. Por eso el arte se
#          REMAPEA a los colores de PAL1 que sirven para hielo (azul 9, violeta
#          azulado 11, grises 4/10 y blanco 13) y va como filas de las hojas: no
#          hace falta cargar ninguna paleta ni sprite aparte, y la tortuga
#          congelada no suma VRAM (usa el buffer del sprite del jugador).
# Salida:  filas 28 (4 cuadros) y 29 (2 cuadros) de res/sprites/players/
#          {leo,mike,don,raph}_new_13x13.png, celdas de 104x104, centro x=52,
#          pies (borde de abajo del hielo) en y=95.
# Idempotente (si la hoja ya tiene 30 filas no hace nada). gen_players_new.py y
# add_demutant_row.py van ANTES: este script exige la fila 27 (demutant).
# Uso:  python3 tools/add_frozen_rows.py [--preview carpeta]
# =============================================================================
import os
import sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DIR = os.path.join(ROOT, 'res', 'sprites', 'players')
SRC = os.path.join(ROOT, 'res', 'sprites', 'turtle_frosen.png')
CELL = 104
ROW_FROZEN = 28
ROW_BREAK = 29
FEET_Y = 95
SRC_CELL = 64
# indices de PAL1 que se usan para el hielo
ICE_IDX = (4, 9, 10, 11, 13)


def build_map(src_pal, dst_pal):
    """indice de la fuente -> indice de PAL1 (por color mas cercano, con peso
    extra al brillo para que el hielo claro quede blanco y el oscuro, gris/azul)."""
    m = np.zeros(256, np.uint8)
    for i in range(1, 16):
        s = np.array(src_pal[i], float)
        best, bd = None, 1e18
        for j in ICE_IDX:
            d_ = np.array(dst_pal[j], float)
            d = ((s - d_) ** 2).sum() + 3.0 * (s.sum() / 3 - d_.sum() / 3) ** 2
            if d < bd:
                best, bd = j, d
        m[i] = best
    return m


def main():
    prev = sys.argv[sys.argv.index('--preview') + 1] if '--preview' in sys.argv else None
    sim = Image.open(SRC)
    sp = sim.getpalette()[:48]
    src_pal = [tuple(sp[i * 3:i * 3 + 3]) for i in range(16)]
    src = np.array(sim)
    for name in ('leo', 'mike', 'don', 'raph'):
        path = os.path.join(DIR, name + '_new_13x13.png')
        im = Image.open(path)
        rows = im.height // CELL
        if rows > ROW_BREAK:
            print(name, 'ya tiene las filas de hielo')
            continue
        assert rows == ROW_FROZEN, (name, rows)
        dp = im.getpalette()[:48]
        dst_pal = [tuple(dp[i * 3:i * 3 + 3]) for i in range(16)]
        cmap = build_map(src_pal, dst_pal)
        a = np.array(im)
        out = np.zeros((CELL * (ROW_BREAK + 1), a.shape[1]), np.uint8)
        out[:a.shape[0]] = a
        for srow, drow, n in ((0, ROW_FROZEN, 4), (1, ROW_BREAK, 2)):
            for k in range(n):
                f = src[srow * SRC_CELL:(srow + 1) * SRC_CELL, k * SRC_CELL:(k + 1) * SRC_CELL]
                f = cmap[f]
                f[src[srow * SRC_CELL:(srow + 1) * SRC_CELL, k * SRC_CELL:(k + 1) * SRC_CELL] == 0] = 0
                y0 = drow * CELL + FEET_Y - (SRC_CELL - 1)
                x0 = k * CELL + CELL // 2 - SRC_CELL // 2
                out[y0:y0 + SRC_CELL, x0:x0 + SRC_CELL] = f
        res = Image.fromarray(out, 'P')
        res.putpalette(im.getpalette())
        res.save(path, transparency=0, optimize=False)
        print(name, 'filas', ROW_FROZEN, ROW_BREAK, '->', res.size)
        if prev and name == 'leo':
            os.makedirs(prev, exist_ok=True)
            c = res.crop((0, ROW_FROZEN * CELL, 4 * CELL, (ROW_BREAK + 1) * CELL)).convert('RGBA')
            bg = Image.new('RGBA', c.size, (60, 60, 70, 255)); bg.alpha_composite(c)
            bg.resize((c.width * 2, c.height * 2), 0).save(os.path.join(prev, 'frozen_rows.png'))


if __name__ == '__main__':
    main()
