# -*- coding: utf-8 -*-
"""
gen_techno_door.py (29/10) -- la PUERTA del Technodrome por donde entra Traag
==============================================================================
En el arcade el jefe del Technodrome entra a la sala GOLPEANDO una puerta del
fondo (pared derecha): la puerta sale volando hacia la izquierda y el jefe
queda parado en el hueco. Siete capturas del arcade (320x224) estan en
tools/techno_door_src/arcade_00..06.png:
    00 la puerta cerrada    01..04 la puerta volando (siempre la misma imagen,
    64 px mas a la izquierda en cada captura)    05 y 06 el jefe en el hueco.

El mapa del nivel (techno_map.bin) tenia ahi un RECTANGULO gris liso donde iba
la puerta. Este script:

  1. ALINEA las capturas con el mapa: con la camara en su tope (4352, 2592) un
     pixel del arcade (ax, ay) cae en la pantalla del juego en (ax+10, ay-17)
     (medido por correlacion en la pared: 98 % de pixeles iguales).
  2. Arma el HUECO ABIERTO de la puerta (sin el jefe): las capturas 05 y 06
     muestran el hueco con el jefe en dos posturas; de cada pixel se toma la
     captura donde no lo tapa el jefe. Lo que tapa el cuerpo en las dos
     (~2400 px) se rellena continuando la columna hacia abajo.
     Lo pinta EN EL MAPA (techno_map/techno_tiles, sobre los tiles de esa zona)
     con la paleta del mapa (pal_techno, 16 colores): cada color del arcade
     pasa al indice mas cercano; si el mapa ya tenia un color parecido en ese
     pixel se lo deja (asi no cambian ni el resto de la pared ni los indices
     que laten con la animacion de paleta).
  3. Recorta la PUERTA CERRADA (captura 00) y la PUERTA VOLANDO (captura 01)
     como sprites con la misma paleta (techno_door_closed.png y
     techno_door_fly.png). El juego dibuja la cerrada encima del hueco y,
     cuando entra el jefe, la cambia por la que vuela.
  4. Vuelve a empaquetar el mapa por bloques (pack_techno_map.py).

Se puede correr solo (parte de techno_map.bin y techno_tiles.bin: si ya tenian
la puerta no pasa nada, es idempotente) y lo llama gen_level8_1.py al final.
    python tools/gen_techno_door.py
"""
import os
import struct
import sys

import numpy as np
from PIL import Image
from scipy import ndimage as ndi

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
D = os.path.join(ROOT, 'res', 'images', 'lvl_8_techno')
SRC = os.path.join(HERE, 'techno_door_src')
W, H = 584, 352

# arcade -> pantalla del juego, y la camara del juego en el tope de la sala
OX, OY = 10, -17
CAMX, CAMY = 4352, 2592
# zona del arcade que se repinta en el mapa (x0, y0, x1, y1; el HUD llega a y 39)
PATCH = (190, 40, 274, 186)
# sprites: celda (x0, y0, ancho, alto) en pixeles del arcade
CLOSED_CELL = (218, 40, 48, 144)
FLY_CELL = (150, 69, 64, 96)

# indices de pal_techno que laten (techPalStep, level8_1.c): no se usan para
# pared ni puerta, salvo el azul 12 en el interior del hueco
ANIMATED = (3, 4, 6, 9, 10, 12)
NA = [i for i in range(1, 16) if i not in ANIMATED]
LEO_KEEP_Y = 172  # de aca para abajo (arcade) Leo tapa el piso: no se toca el mapa

BOSS_COLORS = {(140, 66, 24), (206, 99, 90), (255, 148, 123), (255, 132, 107),
               (255, 198, 189), (57, 0, 74), (107, 82, 107), (140, 107, 140),
               (165, 132, 165), (189, 165, 189), (111, 105, 117), (86, 80, 99),
               (92, 86, 111), (67, 67, 74), (86, 74, 86), (0, 123, 99), (0, 181, 140)}
LEO_COLORS = {(165, 222, 0), (107, 165, 0), (0, 107, 0), (222, 165, 0), (115, 99, 0),
              (148, 123, 0), (0, 189, 255), (140, 132, 206), (173, 148, 214),
              (181, 156, 0), (214, 214, 214), (165, 165, 165), (206, 206, 206)}


def load(i):
    return np.array(Image.open(os.path.join(SRC, 'arcade_%02d.png' % i)).convert('RGB')).astype(int)


def color_mask(a, colors):
    m = np.zeros(a.shape[:2], bool)
    for c in colors:
        m |= (a == np.array(c)).all(axis=2)
    return m


def boss_mask(a):
    reg = np.zeros(a.shape[:2], bool)
    reg[38:190, 190:270] = True
    m = color_mask(a, BOSS_COLORS) & reg
    m = ndi.binary_fill_holes(ndi.binary_closing(m, iterations=2))
    black = (a == 0).all(axis=2)
    for _ in range(2):                      # el contorno negro del cuerpo
        m |= ndi.binary_dilation(m) & black & reg
    return m


def leo_mask(a):
    reg = np.zeros(a.shape[:2], bool)
    reg[120:224, 150:250] = True
    m = color_mask(a, LEO_COLORS) & reg
    m = ndi.binary_fill_holes(ndi.binary_closing(m, iterations=3))
    black = (a == 0).all(axis=2)
    for _ in range(3):
        m |= ndi.binary_dilation(m) & black & reg
    return ndi.binary_dilation(m, iterations=3)


def hueco_abierto(f):
    """El hueco de la puerta sin el jefe (coordenadas del arcade)."""
    f5, f6 = f[5], f[6]
    m5, m6 = boss_mask(f5), boss_mask(f6)
    leo = leo_mask(f[0])
    reg = np.zeros(m5.shape, bool)
    x0, y0, x1, y1 = PATCH
    reg[y0:y1, x0:x1] = True
    B = f6.copy()
    use5 = m6 & ~m5
    B[use5] = f5[use5]
    unres = m6 & m5                          # lo que tapa el cuerpo en las dos
    # lo que tapa el cuerpo: se continua la columna desde arriba
    for x in range(x0, x1):
        for y in np.nonzero(unres[:, x])[0]:
            yy = y
            while yy > y0 and unres[yy, x]:
                yy -= 1
            if not unres[yy, x]:
                B[y, x] = B[yy, x]
    # restos del cinturon del jefe (negro suelto) y la punta de Leo
    box = np.zeros(m5.shape, bool)
    box[126:152, 205:229] = True
    stray = box & (B == 0).all(axis=2)
    stray[:, 216:220] = False                # el marco negro de la puerta se queda
    bad = (stray | (leo & reg)) & reg
    if bad.any():
        B = _fill(B, bad)
    return B


def _fill(B, bad):
    """Rellena los pixeles `bad` con el conocido mas cercano."""
    idx = ndi.distance_transform_edt(bad, return_distances=False, return_indices=True)
    out = B.copy()
    out[bad] = B[idx[0][bad], idx[1][bad]]
    return out


def puerta_cerrada(f, B):
    """Mascara de la puerta cerrada (captura 00) y el hueco B corregido: fuera
    de la puerta el hueco es la pared de la captura 00, tal cual."""
    reg = np.zeros(B.shape[:2], bool)
    reg[38:190, 205:275] = True
    leo = leo_mask(f[0])
    d = (np.abs(f[0] - B).sum(axis=2) > 20) & reg & ~leo
    d[:, :218] = False                     # el borde izquierdo de la puerta
    lab, n = ndi.label(ndi.binary_dilation(d, iterations=2))
    main = lab == (int(np.argmax(ndi.sum(d, lab, range(1, n + 1)))) + 1)
    door = ndi.binary_fill_holes(ndi.binary_closing(d & main, iterations=2)) & main
    B2 = B.copy()
    fixreg = np.zeros(B.shape[:2], bool)
    fixreg[38:190, 190:275] = True
    fix = fixreg & ~door & ~leo
    B2[fix] = f[0][fix]
    return door, B2


def mask_volando(f):
    d = (np.abs(f[1] - f[5]).sum(axis=2) > 30)
    d[:40, :] = False
    lab, n = ndi.label(ndi.binary_dilation(d, iterations=2))
    k = int(np.argmax(ndi.sum(d, lab, range(1, n + 1)))) + 1
    comp = (lab == k) & d
    return ndi.binary_fill_holes(ndi.binary_closing(comp, iterations=1)) | comp


def paleta():
    im = Image.open(os.path.join(D, 'techno_pal.png'))
    p = im.getpalette()[:48]
    return np.array(p, int).reshape(16, 3), im.getpalette()


def cuantiza(c, P, allow12=True):
    """indice de pal_techno mas cercano a un color del arcade (RGB)."""
    r, g, b = c
    cand = list(NA)
    if allow12 and b > r + 40 and b >= g:
        cand.append(12)
    d = [((P[i] - np.array(c)) ** 2).sum() for i in cand]
    return cand[int(np.argmin(d))]


def sprite_png(f, mask, cell, P, pal768, name):
    x0, y0, w, h = cell
    out = np.zeros((h, w), np.uint8)
    for y in range(h):
        for x in range(w):
            ay, ax = y0 + y, x0 + x
            if 0 <= ay < 224 and 0 <= ax < 320 and mask[ay, ax]:
                out[y, x] = cuantiza(tuple(f[ay, ax]), P, allow12=False)
    ys, xs = np.nonzero(out)
    assert xs.min() >= 0 and ys.min() >= 0, name
    # la mascara no puede salirse de la celda
    assert mask.sum() == mask[y0:y0 + h, x0:x0 + w].sum(), name + ': la celda no alcanza'
    im = Image.fromarray(out, 'P')
    im.putpalette(pal768)
    im.save(os.path.join(D, name), transparency=0)
    print('%s: %dx%d, %d px, bbox x %d..%d y %d..%d (arcade)' %
          (name, w, h, (out != 0).sum(), x0 + xs.min(), x0 + xs.max(), y0 + ys.min(), y0 + ys.max()))


def parche_mapa(B, P, skip):
    Bfill = _fill(B, skip)
    mp = list(struct.unpack('>%dH' % (W * H), open(os.path.join(D, 'techno_map.bin'), 'rb').read()))
    raw = open(os.path.join(D, 'techno_tiles.bin'), 'rb').read()
    tiles = []
    for i in range(len(raw) // 32):
        t = []
        for y in range(8):
            row = []
            for b in raw[i * 32 + y * 4:i * 32 + y * 4 + 4]:
                row += [b >> 4, b & 15]
            t.append(row)
        tiles.append(t)
    index = {tuple(tuple(r) for r in t): i for i, t in enumerate(tiles)}
    x0, y0, x1, y1 = PATCH
    # pixeles de mundo que se repintan
    wx0, wx1 = CAMX + x0 + OX, CAMX + x1 + OX
    wy0, wy1 = CAMY + y0 + OY, CAMY + y1 + OY
    cols = range(wx0 // 8, (wx1 - 1) // 8 + 1)
    rows = range(wy0 // 8, (wy1 - 1) // 8 + 1)
    nuevos = cambiados = 0
    for r in rows:
        for c in cols:
            t = [list(row) for row in tiles[mp[r * W + c]]]
            hit = False
            for y in range(8):
                for x in range(8):
                    wx, wy = c * 8 + x, r * 8 + y
                    if not (wx0 <= wx < wx1 and wy0 <= wy < wy1):
                        continue
                    ax, ay = wx - CAMX - OX, wy - CAMY - OY
                    if skip[ay, ax]:
                        # ahi esta Leo en la captura: mas abajo de su cabeza
                        # se deja lo que tiene el mapa; arriba (restos del
                        # rectangulo viejo) se usa el pixel vecino
                        if ay >= LEO_KEEP_Y:
                            continue
                        col = tuple(Bfill[ay, ax])
                    else:
                        col = tuple(B[ay, ax])
                    cur = t[y][x]
                    if cur != 0 and np.abs(P[cur] - np.array(col)).sum() <= 40:
                        continue                      # el mapa ya tenia ese color
                    q = cuantiza(col, P)
                    if q != cur:
                        t[y][x] = q
                        hit = True
            if not hit:
                continue
            key = tuple(tuple(row) for row in t)
            if key not in index:
                index[key] = len(tiles)
                tiles.append(t)
                nuevos += 1
            if mp[r * W + c] != index[key]:
                mp[r * W + c] = index[key]
                cambiados += 1
    assert len(tiles) < 4096
    with open(os.path.join(D, 'techno_tiles.bin'), 'wb') as fo:
        for t in tiles:
            for row in t:
                fo.write(bytes(((row[k] << 4) | row[k + 1]) for k in range(0, 8, 2)))
    with open(os.path.join(D, 'techno_map.bin'), 'wb') as fo:
        fo.write(struct.pack('>%dH' % (W * H), *mp))
    print('techno puerta: %d celdas cambiadas, %d tiles nuevos (total %d)' % (cambiados, nuevos, len(tiles)))
    # la cache de BG_B tiene L8_BG_SLOTS (512) tiles: la pantalla del jefe
    # (ventana de 42x29 celdas) tiene que entrar con margen
    m2 = np.array(mp).reshape(H, W)
    win = m2[CAMY // 8:min(H, CAMY // 8 + 29), CAMX // 8:min(W, CAMX // 8 + 42)]
    print('techno puerta: tiles unicos en la pantalla del jefe: %d (cache 512)' % len(set(win.flatten().tolist())))
    assert len(set(win.flatten().tolist())) <= 400


def main():
    f = {i: load(i) for i in range(7)}
    P, pal768 = paleta()
    B = hueco_abierto(f)
    cerrada, B = puerta_cerrada(f, B)
    parche_mapa(B, P, leo_mask(f[0]))
    sprite_png(f[0], cerrada, CLOSED_CELL, P, pal768, 'techno_door_closed.png')
    sprite_png(f[1], mask_volando(f), FLY_CELL, P, pal768, 'techno_door_fly.png')
    sys.path.insert(0, HERE)
    import pack_techno_map
    pack_techno_map.main()


if __name__ == '__main__':
    main()
