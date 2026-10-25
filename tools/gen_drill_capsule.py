#!/usr/bin/env python3
# =============================================================================
# gen_drill_capsule.py - la CAPSULA DEL TALADRO saliendo del piso (25/10)
# =============================================================================
# Entrada: tools/drill_capsule_src/f00.png .. f16.png -- 17 capturas del arcade
#          (320x224) de la capsula subiendo y girando. Las capturas tienen el
#          HUD tapando todo lo de arriba de y 34 y la camara tiembla +-2 px.
# Salida:  res/images/lvl_4_garage/garage_capsule.png
#          17 cuadros de CELL_W x CELL_H en una fila (una sola animacion), con
#          el piso (la base de los escombros) en el borde de abajo y el centro
#          de la capsula en x CELL_W/2. Sprite de 4 bits, indice 0 transparente.
# Como se arma:
#   1. Se registra cada captura contra la ultima (corre la camara).
#   2. El fondo se saca restando un "piso limpio" (cuadro 0 con la punta
#      tapada) y se queda con el objeto mas grande (capsula + escombros).
#   3. La capsula SUBE: se mide cuanto en cada cuadro (anillo con tiras y ojo
#      de buey, que coinciden entre cuadros) -> OFFY (px respecto del cuadro 6).
#   4. Lo que el HUD tapa en un cuadro (y < 34) se completa con las filas del
#      cuadro anterior mas cercano donde esas filas si se veian.
#   5. Colores: grises del arcade -> grises de la Mega Drive (3 bits).
# =============================================================================
import os
import numpy as np
from PIL import Image
from scipy import ndimage as ndi

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(HERE, "drill_capsule_src")
OUT = os.path.join(ROOT, "res", "images", "lvl_4_garage", "garage_capsule.png")

N = 17
HUD_Y = 34          # el HUD del arcade tapa y < 34
GROUND_Y = 110      # borde de abajo de los escombros (captura)
X0, X1 = 118, 222   # columnas (captura): 104 px, centro de la capsula en 170
CELL_W, CELL_H = X1 - X0, 184
# cuanto subio la capsula en cada cuadro respecto del 6 (px de captura).
OFFY = {4: 18, 5: 11, 6: 0, 7: -9, 8: -19, 9: -30.5, 10: -40.5, 11: -42.5,
        12: -53.5, 13: -63.5, 14: -76.5, 15: -84.5, 16: -87.5}

A = [np.array(Image.open(os.path.join(SRC, "f%02d.png" % k)).convert("RGB")).astype(int)
     for k in range(N)]
REGS = [(30, 100, 40, 100), (240, 300, 40, 110)]


def reg_off(k):
    best = None
    for dy in range(-4, 5):
        for dx in range(-4, 5):
            e = 0
            for x0, x1, y0, y1 in REGS:
                e += np.abs(A[16][y0:y1, x0:x1] - A[k][y0 + dy:y1 + dy, x0 + dx:x1 + dx]).sum()
            if best is None or e < best[0]:
                best = (e, dx, dy)
    return best[1:]


def shifted(k):
    dx, dy = reg_off(k)
    h, w = A[k].shape[:2]
    sy = np.clip(np.arange(h) + dy, 0, h - 1)
    sx = np.clip(np.arange(w) + dx, 0, w - 1)
    return A[k][sy][:, sx]


R = [shifted(k) for k in range(N)]
plate = R[0].copy()
# sin la punta ni las grietas: el piso es liso, se interpola fila por fila
for _y in range(72, 108):
    _a = R[0][_y, 136:140].mean(0)
    _b = R[0][_y, 192:196].mean(0)
    for _x in range(140, 192):
        _t = (_x - 139) / 53.0
        plate[_y, _x] = (_a * (1 - _t) + _b * _t).round()


def mask(k, thr=24, strong=60):
    d = np.abs(R[k] - plate).sum(2)
    # lo debil (>thr) solo a 3 px de algo fuerte: el parche del piso limpio no
    # es perfecto y dejaba un rectangulo gris pegado a la punta
    m = (d > thr) & ndi.binary_dilation(d > strong, structure=np.ones((3, 3)), iterations=3)
    m[:HUD_Y] = False
    m[:, :X0 - 8] = False
    m[:, X1 + 8:] = False
    m[GROUND_Y:] = False
    m = ndi.binary_closing(m, structure=np.ones((3, 3)), iterations=2)
    m = ndi.binary_fill_holes(m)
    lab, n = ndi.label(m)
    if n == 0:
        return m
    sizes = ndi.sum(m, lab, range(1, n + 1))
    return lab == (1 + int(np.argmax(sizes)))


MASK = [mask(k) for k in range(N)]

# gris del arcade -> nivel de 3 bits de la Mega Drive (x36)
def md_level(rgb):
    r, g, b = rgb
    if r > 200 and g > 170 and b < 170 and r - b > 40:        # remaches
        return (6, 5, 4)
    v = (r + g + b) / 3.0
    levels = [0, 36, 73, 109, 146, 182, 219, 255]
    li = min(range(8), key=lambda i: abs(levels[i] - v))
    if v < 70:
        li = 1
    return (li, li, li)


def build_frame(k):
    cell = np.zeros((CELL_H, CELL_W, 4), np.uint8)
    for j in range(CELL_H):
        y = j + GROUND_Y - CELL_H
        src = None
        if HUD_Y <= y < GROUND_Y:
            src = (k, y)
        elif y < HUD_Y and k in OFFY and k >= 5:
            r = y - OFFY[k]
            for jj in range(k - 1, 3, -1):
                yy = int(round(r + OFFY[jj]))
                if 36 <= yy < 98:
                    src = (jj, yy)
                    break
        if src is None:
            continue
        kk, yy = src
        m = MASK[kk][yy, X0:X1]
        cell[j, :, :3] = R[kk][yy, X0:X1]
        cell[j, :, 3] = np.where(m, 255, 0)
    return cell


def main():
    frames = [build_frame(k) for k in range(N)]
    # colores
    pal = {}
    out = np.zeros((CELL_H, CELL_W * N), np.uint8)
    for k, cell in enumerate(frames):
        for y in range(CELL_H):
            for x in range(CELL_W):
                if cell[y, x, 3] == 0:
                    continue
                lv = md_level(tuple(int(v) for v in cell[y, x, :3]))
                if lv not in pal:
                    pal[lv] = len(pal) + 1
                out[y, k * CELL_W + x] = pal[lv]
    assert len(pal) <= 15, pal
    palette = [0] * 48
    for lv, i in pal.items():
        palette[i * 3:i * 3 + 3] = [min(255, c * 36 + (1 if c == 7 else 0) * 0 + (c * 36 // 36 if False else 0)) for c in lv]
    # los niveles 0..7 -> 0,36,73,109,146,182,219,255
    lut = [0, 36, 73, 109, 146, 182, 219, 255]
    for lv, i in pal.items():
        palette[i * 3:i * 3 + 3] = [lut[c] for c in lv]
    im = Image.fromarray(out, "P")
    im.putpalette(palette + [0] * (768 - 48))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    im.save(OUT, transparency=0)
    print(OUT, im.size, "colores:", len(pal))


if __name__ == "__main__":
    main()
