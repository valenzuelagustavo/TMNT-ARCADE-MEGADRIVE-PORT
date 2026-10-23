#!/usr/bin/env python3
# =============================================================================
# boss_shadow.py (22/10) -- sombra a los pies de los jefes (Bebop, Rocksteady)
# =============================================================================
# Los dos jefes vienen sin sombra y "flotaban" sobre el piso. En sus hojas
# generadas (gen_bebop_sheet.py, gen_rocksteady_sheet.py) los pies estan
# apoyados en el BORDE DE ABAJO de cada celda (la linea del piso), asi que la
# sombra es una elipse plana centrada en esa linea:
#
#   - color SHADOW_IDX: el gris oscuro (107,107,107) de la paleta que
#     comparten con April (ningun otro gris es mas oscuro);
#   - SOLO en pixeles transparentes: queda detras del personaje (los pies la
#     tapan en el medio y asoma a los costados);
#   - ancho = lo que ocupan las ultimas FEET_ROWS filas del dibujo (piernas, o
#     el cuerpo entero si esta tirado) + SHADOW_PAD por lado, hasta SHADOW_MAX;
#     alto SHADOW_H filas, pegada al borde de abajo;
#   - siempre en la linea del piso, aunque el frame tenga los pies en el aire
#     (la patada, el salto): la sombra marca donde esta parado de verdad.
#
# Uso (desde los generadores, antes de guardar):
#     import boss_shadow
#     grid = boss_shadow.add(grid, cell_w, cell_h)
# =============================================================================
import numpy as np

SHADOW_IDX = 2      # gris oscuro de la paleta compartida de los jefes
SHADOW_H = 5        # filas
SHADOW_PAD = 6      # px de mas a cada lado de los pies
SHADOW_MIN = 24     # ancho minimo (px)
SHADOW_MAX = 72     # ancho maximo (px)
FEET_ROWS = 10      # filas de abajo del dibujo que cuentan como "los pies"


def add(grid, cw, ch, idx=SHADOW_IDX):
    """Devuelve una copia de la grilla (indices, 0 = transparente) con la
    sombra en cada celda que tenga dibujo."""
    out = grid.copy()
    R, C = grid.shape[0] // ch, grid.shape[1] // cw
    for r in range(R):
        for c in range(C):
            cell = out[r * ch:(r + 1) * ch, c * cw:(c + 1) * cw]
            art = cell != 0
            ys = np.nonzero(art.any(1))[0]
            if len(ys) == 0:
                continue
            bot = ys[-1]
            feet = art[max(0, bot - FEET_ROWS + 1):bot + 1]
            xs = np.nonzero(feet.any(0))[0]
            x0, x1 = xs[0], xs[-1]
            w = int(np.clip(x1 - x0 + 1 + 2 * SHADOW_PAD, SHADOW_MIN, SHADOW_MAX))
            cx = (x0 + x1) / 2.0
            rx = w / 2.0
            ry = SHADOW_H / 2.0
            cy = ch - ry                       # centro: media altura arriba del borde
            for y in range(ch - SHADOW_H, ch):
                dy = (y + 0.5 - cy) / ry
                if abs(dy) >= 1:
                    continue
                hw = rx * np.sqrt(1 - dy * dy)
                a = int(np.floor(cx - hw + 0.5))
                b = int(np.ceil(cx + hw - 0.5))
                a, b = max(0, a), min(cw - 1, b)
                row = cell[y, a:b + 1]
                row[row == 0] = idx
    return out
