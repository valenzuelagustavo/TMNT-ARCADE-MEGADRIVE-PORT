# -*- coding: utf-8 -*-
"""
gen_garage_van.py (30/10) -- la camioneta de las tortugas del garage (nivel 4)
==============================================================================
Fuente: res/images/lvl_4_garage/TURTLE_VAN.png (486x164, indexada, 16 colores:
el 0 es transparente, 1..15 los colores de la camioneta). Trae DOS dibujos de
240x164: la puerta cerrada (x 2..241) y abierta (x 244..483). Arriba a la
izquierda tiene la tira de muestras de la paleta (filas 0..7): se descarta.

En el juego la camioneta es un dibujo de BG_A (240x144 px = 30x18 tiles) que
se desliza con el scroll del plano (src/garage_van.c): como sprite serian 48
sprites de hardware y el limite de tiles por linea (HUD + tortugas) la
cortaria. Los tiles viven en VRAM de sprites reservada con un sprite oculto.

La puerta es corrediza: el panel (con las dos cajas) se corre 66 px hacia
atras (a la izquierda) y deja ver el interior. Entre el dibujo cerrado y el
abierto se arman 2 cuadros intermedios (corrimiento 22 y 44 px) con el mismo
panel del dibujo cerrado sobre el interior del abierto. Estados:
    0 cerrada   1 corrida 22 px   2 corrida 44 px   3 abierta (el dibujo del usuario)

Salida (res/images/lvl_4_garage/):
    van_tiles.bin    tiles unicos (con flips H/V), 32 bytes c/u; el 0 es vacio
    van_map.bin      4 estados x (18 filas x 30 columnas) u16 (flips | indice;
                     0 = celda vacia); se le suma la base de VRAM y la paleta
    van_pal_gen.png  16x1 con la paleta (para el PALETTE de rescomp)
y src/level4_1_van.h con las medidas.
Uso: python3 tools/gen_garage_van.py
"""
import os

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
DIR = os.path.join(ROOT, 'res', 'images', 'lvl_4_garage')
SRC = os.path.join(DIR, 'TURTLE_VAN.png')
HDR = os.path.join(ROOT, 'src', 'level4_1_van.h')

IMG_W = 240
CLOSED_X, OPEN_X = 2, 244
SWATCH_ROWS = 8
CROP_Y0 = 20            # primera fila con dibujo (el alerón)
ROWS_T = 18             # 144 px: hasta la fila 163
COLS_T = 30
PANEL = (67, 133, 85, 143)       # x0, x1, y0, y1 del panel de la puerta (dibujo cerrado)
SHIFTS = (0, 22, 44, 66)         # corrimiento del panel de cada estado
WORLD_Y0 = CROP_Y0 - 2           # y de mundo de la fila 0 del dibujo (la camioneta parada
                                 # queda con la fila 0 de TURTLE_VAN.png en y = -2)


def states(a):
    C = a[:, CLOSED_X:CLOSED_X + IMG_W].copy()
    O = a[:, OPEN_X:OPEN_X + IMG_W].copy()
    C[:SWATCH_ROWS] = 0
    O[:SWATCH_ROWS] = 0
    x0, x1, y0, y1 = PANEL
    out = []
    for sh in SHIFTS:
        if sh == 0:
            out.append(C)
            continue
        if sh == SHIFTS[-1]:
            out.append(O)                      # el dibujo del usuario tal cual
            continue
        B = C.copy()
        B[y0:y1 + 1, x0:x1 + 1] = O[y0:y1 + 1, x0:x1 + 1]    # el interior
        B[y0:y1 + 1, x0] = O[y0:y1 + 1, x0 + 1]
        B[y0:y1 + 1, x0 - sh:x1 + 1 - sh] = C[y0:y1 + 1, x0:x1 + 1]   # el panel corrido
        out.append(B)
    return out


def canon(t):
    """tile 8x8 -> (clave, bytes canonicos, flipH, flipV) para deduplicar con flips."""
    best = None
    for fv in (0, 1):
        for fh in (0, 1):
            v = t
            if fh:
                v = v[:, ::-1]
            if fv:
                v = v[::-1, :]
            b = v.tobytes()
            if best is None or b < best[0]:
                best = (b, v, fh, fv)
    return best


def main():
    im = Image.open(SRC)
    assert im.mode == 'P', im.mode
    pal = im.getpalette()[:48]
    a = np.array(im)
    sts = states(a)

    tiles = [np.zeros((8, 8), np.uint8)]          # 0 = vacio
    index = {tiles[0].tobytes(): 0}
    maps = []
    for B in sts:
        crop = B[CROP_Y0:CROP_Y0 + ROWS_T * 8, :COLS_T * 8]
        assert crop.shape == (ROWS_T * 8, COLS_T * 8)
        m = np.zeros((ROWS_T, COLS_T), np.uint16)
        for ty in range(ROWS_T):
            for tx in range(COLS_T):
                t = crop[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8]
                if not t.any():
                    continue
                key, v, fh, fv = canon(t)
                # 'v' es la forma canonica; la celda muestra t = flip(v) con los mismos flips
                if key not in index:
                    index[key] = len(tiles)
                    tiles.append(v.copy())
                m[ty, tx] = index[key] | (0x0800 if fh else 0) | (0x1000 if fv else 0)
        maps.append(m)
    n = len(tiles)
    assert n < 1000, n

    # tiles 4bpp
    tb = bytearray()
    for t in tiles:
        for r in range(8):
            for x in range(0, 8, 2):
                tb.append(int(t[r, x]) << 4 | int(t[r, x + 1]))
    with open(os.path.join(DIR, 'van_tiles.bin'), 'wb') as f:
        f.write(tb)
    mb = bytearray()
    for m in maps:
        for v in m.flatten():
            mb += int(v).to_bytes(2, 'big')
    with open(os.path.join(DIR, 'van_map.bin'), 'wb') as f:
        f.write(mb)

    # paleta: 16x1, pixel i = color i
    p = Image.fromarray(np.arange(16, dtype=np.uint8).reshape(1, 16), 'P')
    p.putpalette(pal + [0] * (768 - len(pal)))
    p.save(os.path.join(DIR, 'van_pal_gen.png'))

    # rectangulo donde los estados difieren del 0 (lo que hay que reescribir al mover la puerta)
    diff = np.zeros((ROWS_T, COLS_T), bool)
    for m in maps[1:]:
        diff |= (m != maps[0])
    ys, xs = np.where(diff)
    dc0, dc1, dr0, dr1 = int(xs.min()), int(xs.max()), int(ys.min()), int(ys.max())

    with open(HDR, 'w') as f:
        f.write('// GENERADO por tools/gen_garage_van.py -- NO EDITAR A MANO\n')
        f.write('#ifndef _LEVEL4_1_VAN_H_\n#define _LEVEL4_1_VAN_H_\n')
        f.write('#define VAN_TILES %d\n' % n)
        f.write('#define VAN_COLS %d\n#define VAN_ROWS %d\n' % (COLS_T, ROWS_T))
        f.write('#define VAN_STATES %d\n' % len(maps))
        f.write('#define VAN_WORLD_Y0 %d\n' % WORLD_Y0)
        f.write('#define VAN_DOOR_C0 %d\n#define VAN_DOOR_R0 %d\n#define VAN_DOOR_W %d\n#define VAN_DOOR_H %d\n'
                % (dc0, dr0, dc1 - dc0 + 1, dr1 - dr0 + 1))
        f.write('#endif\n')
    print('tiles unicos: %d (%d bytes), mapas: %d x %d celdas, puerta: cols %d..%d filas %d..%d'
          % (n, len(tb), len(maps), ROWS_T * COLS_T, dc0, dc1, dr0, dr1))
    for k, m in enumerate(maps):
        print('estado', k, 'celdas con tile', int((m != 0).sum()))


if __name__ == '__main__':
    main()
