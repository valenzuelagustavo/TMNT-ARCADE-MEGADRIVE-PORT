#!/usr/bin/env python3
# ============================================================================
# gen_foot_knife.py -- Arma la hoja del foot soldier BLANCO DEL CUCHILLO (10/10)
# ============================================================================
# QUE PROBLEMA RESUELVE
#   res/sprites/foot_soldier_white_sword.png (el rip) trae DOS enemigos:
#     - arriba, 10 filas en grilla de 104x104: el blanco de la ESPADA (lo arma
#       tools/gen_foot_white.py, sin cambios);
#     - abajo (y >= 1040) el blanco del CUCHILLO, SIN grilla: cada pose esta
#       suelta, con los pies a alturas distintas y columnas cada ~80 px.
#   Este script recorta las poses del cuchillo (cajas medidas abajo, POSES),
#   las ordena en animaciones y las pega en una grilla de 104x104 con los PIES
#   en la misma fila que la espada (fila 103), centradas por la sombra del
#   piso. Asi el codigo usa las mismas medidas de frame que el de la espada.
#
#   Las animaciones salen del video del arcade (gameplayARCADE/tmnt.avi,
#   1-1, cuadros 2150-3010): se compararon las siluetas del video contra cada
#   pose de la hoja (ver claude/foot-blanco-cuchillo-oleadas-10-10.md).
#
# ENTRADA   res/sprites/foot_soldier_white_sword.png  (el rip, no se toca)
# SALIDAS   res/sprites/foot_soldier_knife_gen.png    (832 x 104*filas)
#           res/sprites/knife_proj_gen.png            (cuchillo tirado:
#                                                      3 frames de 40x24)
#
# Correr desde la raiz del proyecto:  python3 tools/gen_foot_knife.py
# ============================================================================

import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SPR  = os.path.join(ROOT, "res", "sprites")

SRC   = os.path.join(SPR, "foot_soldier_white_sword.png")
WHITE = os.path.join(SPR, "foot_soldier_white_gen.png")   # paleta (indice 0 igualado)
DST   = os.path.join(SPR, "foot_soldier_knife_gen.png")
PROJ  = os.path.join(SPR, "knife_proj_gen.png")

CELL       = 104
FOOT_ROW   = 103     # ultima fila con arte (igual que la espada)
CENTER_X   = 52
GROUND_IDX = 2       # rojo del rip (la sombra del piso)
SHADOW_IDX = 10      # lo que usa la espada para la sombra (gen_foot_white.py)

# Cajas (x, y, w, h) de cada pose del cuchillo en el rip, por nombre "fila.col"
# de la disposicion del rip (filas de abajo hacia arriba no: de arriba abajo,
# contando desde y = 1040).
POSES = {
    "0.0": (29, 1075, 34, 71), "0.1": (105, 1076, 42, 70), "0.2": (189, 1075, 35, 71),
    "0.3": (269, 1075, 34, 71), "0.4": (348, 1076, 36, 70),
    "1.0": (29, 1156, 35, 70), "1.1": (108, 1157, 36, 68), "1.2": (184, 1156, 44, 69),
    "1.3": (267, 1156, 38, 70), "1.4": (352, 1155, 29, 71),
    "2.0": (26, 1236, 40, 70), "2.1": (105, 1236, 43, 70), "2.2": (189, 1236, 35, 70),
    "2.3": (268, 1236, 36, 70), "2.4": (349, 1236, 34, 69),
    "3.4": (346, 1317, 40, 68),
    "4.1": (102, 1397, 48, 67),
    "5.0": (27, 1487, 43, 64), "5.1": (111, 1486, 35, 66),
    "6.3": (266, 1565, 45, 68), "6.4": (337, 1563, 63, 72),
    "8.1": (93, 1645, 70, 68), "8.2": (184, 1643, 48, 71), "8.3": (265, 1645, 47, 68),
    "8.4": (343, 1646, 50, 65),
    "9.0": (21, 1721, 55, 75), "9.1": (101, 1720, 54, 77), "9.2": (174, 1727, 68, 64),
    "9.3": (266, 1728, 45, 62), "9.4": (351, 1727, 35, 63),
}
# Proyectil: el cuchillo en vuelo, el chispazo y los pedazos al romperse.
PROJ_POSES = [(56, 1654, 25, 10), (14, 1648, 29, 22), (10, 1688, 37, 22)]
PROJ_W, PROJ_H = 40, 24

# Filas de la hoja generada = animaciones (KNIFE_ANIM_* en enemy.h).
ANIMS = [
    ["4.1"],                                                   # 0 quieto / guardia
    ["1.2", "1.3", "1.4", "0.0", "0.1", "0.2", "0.3", "0.4"],  # 1 camina (adelante y atras)
    ["3.4", "2.0", "2.1", "2.2", "2.3", "2.4", "1.0", "1.1"],  # 2 camina arriba/abajo
    ["9.1", "9.0", "9.2", "9.3", "9.4"],                       # 3 tira el cuchillo
    ["8.3", "8.2", "8.1"],                                     # 4 punalada
    ["8.4", "6.3", "6.4"],                                     # 5 patada
    ["5.0", "5.1"],                                            # 6 golpe recibido
    "SWORD_DEATH",                                             # 7 muerte (la de la espada)
]
SWORD_DEATH_ROW = 9   # fila de la muerte en la hoja de la espada (4 frames)


def ground_fix(cell):
    """Las manchas GROUND_IDX que tocan el piso pasan a SHADOW_IDX (igual que
    gen_foot_white.py: los detalles rojos del cuerpo quedan mas arriba)."""
    opaque = cell != 0
    if not opaque.any():
        return cell
    bottom = np.nonzero(opaque.any(axis=1))[0][-1]
    is_g = cell == GROUND_IDX
    h, w = cell.shape
    seen = np.zeros_like(is_g)
    for sy in range(h):
        for sx in range(w):
            if not is_g[sy, sx] or seen[sy, sx]:
                continue
            comp, stack = [], [(sy, sx)]
            seen[sy, sx] = True
            while stack:
                y, x = stack.pop()
                comp.append((y, x))
                for ny, nx in ((y-1, x), (y+1, x), (y, x-1), (y, x+1)):
                    if 0 <= ny < h and 0 <= nx < w and is_g[ny, nx] and not seen[ny, nx]:
                        seen[ny, nx] = True
                        stack.append((ny, nx))
            if max(y for y, _ in comp) >= bottom - 4:
                for y, x in comp:
                    cell[y, x] = SHADOW_IDX
    return cell


def feet_center(crop):
    """Centro de los pies: el de la sombra del piso (filas de abajo); si no
    hay sombra, el del arte de las ultimas 6 filas."""
    h = crop.shape[0]
    low = crop[max(0, h - 6):]
    ys, xs = np.nonzero(low == GROUND_IDX)
    if len(xs) == 0:
        ys, xs = np.nonzero(low != 0)
    return int(round((xs.min() + xs.max()) / 2.0))


def main():
    im = Image.open(SRC)
    assert im.mode == "P", "el rip tiene que ser indexado (modo P)"
    a = np.array(im)
    sword = np.array(Image.open(WHITE))

    rows = len(ANIMS)
    out = np.zeros((rows * CELL, 8 * CELL), np.uint8)
    for r, anim in enumerate(ANIMS):
        if anim == "SWORD_DEATH":
            out[r * CELL:(r + 1) * CELL, :] = sword[SWORD_DEATH_ROW * CELL:(SWORD_DEATH_ROW + 1) * CELL, :]
            continue
        for c, name in enumerate(anim):
            x, y, w, h = POSES[name]
            crop = a[y:y + h, x:x + w].copy()
            fx = feet_center(crop)
            crop = ground_fix(crop)
            ox = CENTER_X - fx
            oy = FOOT_ROW + 1 - h
            assert 0 <= ox and ox + w <= CELL and 0 <= oy, (name, ox, oy)
            cell = out[r * CELL:(r + 1) * CELL, c * CELL:(c + 1) * CELL]
            m = crop != 0
            cell[oy:oy + h, ox:ox + w][m] = crop[m]

    pal = Image.open(WHITE).getpalette()
    img = Image.fromarray(out, mode="P"); img.putpalette(pal)
    img.save(DST, transparency=0)

    proj = np.zeros((PROJ_H, PROJ_W * len(PROJ_POSES)), np.uint8)
    for k, (x, y, w, h) in enumerate(PROJ_POSES):
        crop = a[y:y + h, x:x + w]
        ox = k * PROJ_W + (PROJ_W - w) // 2
        oy = (PROJ_H - h) // 2
        m = crop != 0
        proj[oy:oy + h, ox:ox + w][m] = crop[m]
    pimg = Image.fromarray(proj, mode="P"); pimg.putpalette(pal)
    pimg.save(PROJ, transparency=0)

    print("salida: %s (%dx%d, %d anims)" % (os.path.relpath(DST, ROOT), out.shape[1], out.shape[0], rows))
    print("salida: %s (%d frames de %dx%d)" % (os.path.relpath(PROJ, ROOT), len(PROJ_POSES), PROJ_W, PROJ_H))


if __name__ == "__main__":
    main()
