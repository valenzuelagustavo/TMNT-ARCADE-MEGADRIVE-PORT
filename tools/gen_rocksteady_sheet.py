#!/usr/bin/env python3
# =============================================================================
# gen_rocksteady_sheet.py  -  Rocksteady (jefe) + sus balas desde el rip nuevo
# =============================================================================
# (03/10) Gustavo trajo un rip nuevo de Rocksteady que COMPARTE PALETA con
# Bebop y April (los tres juntos en el garage entran en una sola linea de
# paleta). Los dibujos son los mismos que los de la hoja anterior; cambian los
# colores y la disposicion (frames sueltos, no una grilla).
#
# Entrada (la dibuja Gustavo, NO se toca):
#   res/sprites/Arcade - Teenage Mutant Ninja Turtles - Bosses - Rocksteady.png
#       626x990, frames sueltos. A la derecha de la fila del disparo vienen
#       las balas (horizontal, diagonal e impacto).
#
# Referencias de disposicion (las hojas anteriores, NO se tocan):
#   res/sprites/rocksteady_boss.png    grilla 8x11 de 104x104 (13x13 tiles)
#   res/sprites/boss_bullet-new.png    3 frames de 16x16
#
# Salidas (las que compila rescomp):
#   res/sprites/rocksteady_boss_gen.png
#   res/sprites/boss_bullet_gen.png
#
# COMO: cada frame de la hoja de referencia se busca en el rip por su SILUETA
# (matchTemplate de las mascaras; los dibujos coinciden pixel a pixel salvo
# retoques chicos) y se copian los pixeles del rip a la MISMA posicion de la
# celda. Asi los anclajes (pies, centro) quedan exactamente donde estaban y
# no hay que tocar ningun offset de rocksteady.c. Si una silueta aparece
# repetida en el rip (el idle y el disparo repiten dibujos), se usan en el
# orden en que aparecen, de izquierda a derecha.
#
# Lo que el rip trae y la hoja no usa (el chorrito blanco a la derecha del
# idle) se ignora.
#
# Uso:  python3 tools/gen_rocksteady_sheet.py
# =============================================================================
import os
import sys

import cv2
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPR = os.path.join(ROOT, "res", "sprites")

SRC = os.path.join(SPR, "Arcade - Teenage Mutant Ninja Turtles - Bosses - Rocksteady.png")
REF = os.path.join(SPR, "rocksteady_boss.png")
DST = os.path.join(SPR, "rocksteady_boss_gen.png")
REF_BUL = os.path.join(SPR, "boss_bullet-new.png")
DST_BUL = os.path.join(SPR, "boss_bullet_gen.png")

CELL = 104
BUL = 16
BUL_X0 = 580            # las balas sueltas del rip estan de aca a la derecha
# Diferencia de silueta tolerada (pixeles) para dar un frame por encontrado.
TOL_ABS = 30
TOL_REL = 0.02


def load_p(path):
    im = Image.open(path)
    if im.mode != "P":
        sys.exit("%s tiene que ser indexado" % os.path.basename(path))
    return im, np.asarray(im).astype(np.uint8)


class Rip:
    def __init__(self, a):
        self.a = a
        self.mask = a != 0
        self.lab, self.n = ndimage.label(self.mask, structure=np.ones((3, 3)))
        self.sizes = np.bincount(self.lab.ravel())
        self.pad = 128
        self.padded = np.pad(self.mask.astype(np.float32), self.pad)
        self.used = set()

    def find(self, tmask, name):
        """Posicion (x, y) en el rip de la silueta 'tmask' (recorte justo)."""
        sc = cv2.matchTemplate(self.padded, tmask.astype(np.float32), cv2.TM_SQDIFF)
        best = float(sc.min())
        area = float(tmask.sum())
        tol = max(min(TOL_ABS, 0.1 * area), TOL_REL * area)
        if best > tol:
            sys.exit("%s: no aparece en el rip (diferencia %d px)" % (name, best))
        # todos los minimos que entran en la tolerancia, sin repetir vecinos
        ys, xs = np.nonzero(sc <= tol)
        order = np.argsort(sc[ys, xs])[:64]                # los mejores, alcanza
        cands = []
        for y, x in zip(ys[order], xs[order]):
            if all(abs(x - cx) > 4 or abs(y - cy) > 4 for cx, cy in cands):
                cands.append((int(x), int(y)))
        cands = [(x - self.pad, y - self.pad) for x, y in cands]
        cands.sort(key=lambda p: (p[0], p[1]))            # de izquierda a derecha
        for c in cands:
            if c not in self.used:
                self.used.add(c)
                return c
        return cands[0]                                     # repetido de mas

    def pixels(self, x, y, tmask):
        """Pixeles del rip que forman el dibujo en (x, y): las manchas (8-conexas)
        que caen al menos a la mitad dentro de la silueta de referencia."""
        h, w = tmask.shape
        out = np.zeros((h, w), np.uint8)
        y0, x0 = max(0, y), max(0, x)
        y1, x1 = min(self.a.shape[0], y + h), min(self.a.shape[1], x + w)
        lab = self.lab[y0:y1, x0:x1]
        tm = tmask[y0 - y:y1 - y, x0 - x:x1 - x]
        inside = np.bincount(lab[tm].ravel(), minlength=self.n + 1)
        keep = np.zeros(self.n + 1, bool)
        keep[1:] = inside[1:] * 2 >= self.sizes[1:]
        sel = keep[lab]
        out[y0 - y:y1 - y, x0 - x:x1 - x] = np.where(sel, self.a[y0:y1, x0:x1], 0)
        return out


def rebuild(rip, ref, cell, label):
    out = np.zeros_like(ref)
    rows, cols = ref.shape[0] // cell, ref.shape[1] // cell
    found = 0
    for r in range(rows):
        for c in range(cols):
            cy, cx = r * cell, c * cell
            m = ref[cy:cy + cell, cx:cx + cell] != 0
            if not m.any():
                continue
            ys, xs = np.nonzero(m)
            by0, by1, bx0, bx1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
            tm = m[by0:by1, bx0:bx1]
            x, y = rip.find(tm, "%s fila %d frame %d" % (label, r, c))
            px = rip.pixels(x, y, tm)
            dst = out[cy + by0:cy + by1, cx + bx0:cx + bx1]
            dst[:] = np.where(px != 0, px, dst)
            found += 1
    print("%s: %d frames" % (label, found))
    return out


def save(arr, path, pal_im):
    img = Image.fromarray(arr, "P")
    img.putpalette(pal_im.getpalette())
    img.save(path, transparency=0)
    print("%s  %dx%d" % (os.path.basename(path), arr.shape[1], arr.shape[0]))


def main():
    im, a = load_p(SRC)
    _, ref = load_p(REF)
    rip = Rip(a)
    save(rebuild(rip, ref, CELL, "rocksteady"), DST, im)
    # Las balas sueltas estan a la derecha de la fila del disparo; se buscan
    # solo ahi (el disparo horizontal tambien aparece pegado al arma).
    _, refb = load_p(REF_BUL)
    ab = a.copy()
    ab[:, :BUL_X0] = 0
    save(rebuild(Rip(ab), refb, BUL, "balas"), DST_BUL, im)


if __name__ == "__main__":
    main()
