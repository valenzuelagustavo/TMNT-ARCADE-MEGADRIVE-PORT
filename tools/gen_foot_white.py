#!/usr/bin/env python3
# ============================================================================
# gen_foot_white.py -- Limpia la "sombra" del rip del Foot Soldier blanco
# ============================================================================
# QUE PROBLEMA RESUELVE
#   El sheet que vino del rip (res/sprites/foot_soldier_white_sword.png) trae,
#   abajo de las botas, un parche PLANO del piso del arcade pintado con el
#   indice 2 de la paleta = (219,36,0), un rojo fuerte. El foot soldier morado
#   trae el mismo parche pero en MARRON (indice 4 de PAL2 = 146,109,73), que se
#   disimula con la vereda; el rojo del blanco cantaria como un charco.
#
#   PAL3 (la que comparten el naranja y el blanco) NO tiene marron y NO tiene
#   un slot libre: los 15 colores estan todos en uso entre los dos sheets. El
#   color mas cercano al marron del morado es el indice 10 = (146,109,146):
#   mismo R y G, y practicamente la misma luminancia (124 contra 116), asi que
#   abajo de los pies lee como una sombra calida en vez de un charco rojo.
#
# COMO LO DETECTA
#   NO por color ni por una franja fija de Y: por COMPONENTES CONECTADAS. El
#   parche del piso no llega al borde de la celda (en el frame 0 del walk vive
#   en y=99..102 y el arte termina en 102), asi que flotar desde abajo no
#   alcanza. Lo que se hace es separar cada mancha roja conectada de la celda y
#   repintar SOLO las que terminan a menos de GROUND_SLACK px del pixel mas
#   bajo del arte de ESE frame. La faja y los detalles rojos del cuerpo son
#   otras manchas, y quedan varias decenas de px mas arriba.
#
# ENTRADA   res/sprites/foot_soldier_white_sword.png   (el original, no se toca)
# SALIDA    res/sprites/foot_soldier_white_gen.png     (GENERADO, es el que
#                                                       declara enemies.res)
#
# Correr desde la raiz del proyecto:  python3 tools/gen_foot_white.py
# ============================================================================

import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SPR  = os.path.join(ROOT, "res", "sprites")

SRC  = os.path.join(SPR, "foot_soldier_white_sword.png")
DST  = os.path.join(SPR, "foot_soldier_white_gen.png")
REF  = os.path.join(SPR, "foot_soldier_orange.png")  # de donde sale el indice 0

CELL        = 104   # celda del sheet (8 columnas x 10 filas)
GROUND_IDX  = 2     # rojo (219,36,0): con lo que vino pintado el piso
SHADOW_IDX  = 10    # (146,109,146): lo mas parecido al marron del morado
                    # que hay en PAL3 sin tocar la paleta del naranja
GROUND_SLACK = 4    # px de tolerancia contra el pixel mas bajo del frame


def ground_mask(cell):
    """Mascara de las manchas GROUND_IDX que son piso y no arte del cuerpo."""
    h, w = cell.shape
    opaque = cell != 0
    if not opaque.any():
        return np.zeros_like(opaque)
    art_bottom = np.nonzero(opaque.any(axis=1))[0][-1]

    is_g = cell == GROUND_IDX
    seen = np.zeros_like(is_g)
    out  = np.zeros_like(is_g)

    for sy in range(h):
        for sx in range(w):
            if not is_g[sy, sx] or seen[sy, sx]:
                continue
            # Una mancha entera (4-conexa)
            comp, stack = [], [(sy, sx)]
            seen[sy, sx] = True
            while stack:
                y, x = stack.pop()
                comp.append((y, x))
                for ny, nx in ((y-1, x), (y+1, x), (y, x-1), (y, x+1)):
                    if 0 <= ny < h and 0 <= nx < w and is_g[ny, nx] and not seen[ny, nx]:
                        seen[ny, nx] = True
                        stack.append((ny, nx))
            # Es piso si LLEGA hasta abajo de todo del arte de este frame
            if max(y for y, _ in comp) >= art_bottom - GROUND_SLACK:
                for y, x in comp:
                    out[y, x] = True
    return out


def main():
    im = Image.open(SRC)
    assert im.mode == "P", "el sheet tiene que ser indexado (modo P)"
    a = np.array(im)
    h, w = a.shape
    rows, cols = h // CELL, w // CELL

    total = 0
    for r in range(rows):
        for c in range(cols):
            sl = (slice(r * CELL, (r + 1) * CELL), slice(c * CELL, (c + 1) * CELL))
            cell = a[sl]
            m = ground_mask(cell)
            if m.any():
                cell[m] = SHADOW_IDX
                a[sl] = cell
                total += int(m.sum())

    # El indice 0 es el transparente y nunca se dibuja, pero initEnemySpawn
    # recarga la linea de paleta ENTERA desde el PNG del sheet: si el blanco
    # trajera un indice 0 distinto al del naranja, cada spawn le cambiaria ese
    # slot a PAL3. Se iguala al del naranja para que las dos paletas queden
    # byte a byte identicas y el orden de spawn deje de importar.
    pal = list(im.getpalette())
    ref = Image.open(REF).getpalette()
    pal[0:3] = ref[0:3]

    out = Image.fromarray(a, mode="P")
    out.putpalette(pal)
    t = im.info.get("transparency", 0)
    out.save(DST, transparency=t)

    print("%d pixeles del parche del piso repintados" % total)
    print("  de  idx %2d %s  (rojo del rip)"   % (GROUND_IDX, tuple(pal[GROUND_IDX*3:GROUND_IDX*3+3])))
    print("  a   idx %2d %s  (sombra)"         % (SHADOW_IDX, tuple(pal[SHADOW_IDX*3:SHADOW_IDX*3+3])))
    print("salida: %s" % os.path.relpath(DST, ROOT))


if __name__ == "__main__":
    main()
