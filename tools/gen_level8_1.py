#!/usr/bin/env python3
# =============================================================================
# gen_level8_1.py  (27/09) -- mapa de la Scene 8 (Technodrome)
# =============================================================================
# El mapa viene del proyecto del companero (Ray Project, lvl_7_techno): una L
# gigante de 584x352 tiles (4672x2816 px): pasillo arriba, un pozo en diagonal
# a 45 grados y la sala de abajo. 2266 tiles unicos (mas de los 2048 que admite
# un IMAGE): ya viene como BIN crudo (indice u16 por celda, sin flips, 0 =
# vacio/transparente), que es justo el formato ancho de stage_bg.c.
#
# Lo unico que se toca: el ASCENSOR esta DIBUJADO en el mapa, en su posicion
# de arranque (1408,128), identico a techno_elevator.png. En el juego el
# ascensor es un objeto que baja (se dibuja aparte, en BG_A), asi que en el
# mapa se lo reemplaza por lo que hay debajo: el pozo. El pozo se repite cada
# 32 tiles en diagonal (medido: 98 % de celdas iguales), asi que cada pixel
# tapado por el ascensor se toma de (x+256, y+256). Los tiles nuevos que
# salen de esa mezcla se agregan al final del tileset. Con el mismo periodo
# se tapan los huecos (tiles en blanco) que el mapa traia dentro del pozo.
#
# Entrada:  res/images/lvl_8_techno/techno_src_map.bin, techno_src_tiles.bin,
#           techno_elevator.png
# Salida:   res/images/lvl_8_techno/techno_map.bin, techno_tiles.bin
#
# Uso: python3 tools/gen_level8_1.py
# =============================================================================
import os, struct
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = os.path.join(ROOT, 'res', 'images', 'lvl_8_techno')
W, H = 584, 352
EX, EY = 1408, 128          # ascensor en el mapa (px)
SHIFT = 256                 # periodo diagonal del pozo (px)

mp = list(struct.unpack('>%dH' % (W * H), open(os.path.join(D, 'techno_src_map.bin'), 'rb').read()))
raw = open(os.path.join(D, 'techno_src_tiles.bin'), 'rb').read()
tiles = []
for i in range(len(raw) // 32):
    d = raw[i * 32:(i + 1) * 32]
    t = []
    for y in range(8):
        row = []
        for b in d[y * 4:y * 4 + 4]:
            row += [b >> 4, b & 15]
        t.append(tuple(row))
    tiles.append(tuple(t))
index = {t: i for i, t in enumerate(tiles)}

elev = Image.open(os.path.join(D, 'techno_elevator.png'))
ep = elev.load()
EW, EH = elev.size


def px(x, y):
    c, r = x >> 3, y >> 3
    if c >= W or r >= H:
        return 0
    return tiles[mp[r * W + c]][y & 7][x & 7]


changed = 0
for r in range(EY >> 3, (EY + EH) >> 3):
    for c in range(EX >> 3, (EX + EW) >> 3):
        old = tiles[mp[r * W + c]]
        new = []
        touched = False
        for y in range(8):
            row = list(old[y])
            for x in range(8):
                wx, wy = c * 8 + x, r * 8 + y
                if ep[wx - EX, wy - EY] != 0:
                    row[x] = px(wx + SHIFT, wy + SHIFT)
                    touched = True
            new.append(tuple(row))
        if not touched:
            continue
        new = tuple(new)
        if new not in index:
            index[new] = len(tiles)
            tiles.append(new)
        mp[r * W + c] = index[new]
        changed += 1

# Huecos: el mapa de Ray tiene, dentro del pozo, bloques de un tile en blanco
# (todo transparente) que en pantalla quedaban como cuadrados negros. Se
# rellenan con la misma celda un periodo (32 tiles) mas abajo o mas arriba en
# la diagonal. La puerta del pasillo (blanco rodeado de pared, sin pozo en la
# diagonal) no se toca.
blank = set(i for i, t in enumerate(tiles) if all(v == 0 for row in t for v in row))


def isblank(c, r):
    if c < 0 or r < 0 or c >= W or r >= H:
        return True
    return mp[r * W + c] in blank


holes = 0
for r in range(H):
    for c in range(W):
        if mp[r * W + c] not in blank or mp[r * W + c] == 0:
            continue
        if isblank(c + 32, r + 32) or isblank(c - 32, r - 32):
            continue
        mp[r * W + c] = mp[(r + 32) * W + c + 32]
        holes += 1
print('techno: %d huecos del pozo rellenados' % holes)

# Y los bordes de esos huecos: tiles del pozo con pixeles transparentes. Pixel
# por pixel, cada transparente se toma del mismo lugar un periodo mas abajo
# (o mas arriba). Solo dentro del pozo (las dos celdas de la diagonal con
# dibujo): los bordes de verdad del pozo quedan igual, porque la copia en
# diagonal cae sobre el mismo borde.
fixed = 0
for r in range(H):
    for c in range(W):
        v = mp[r * W + c]
        if v in blank:
            continue
        if isblank(c + 32, r + 32) or isblank(c - 32, r - 32):
            continue
        old_t = tiles[v]
        if all(p != 0 for row in old_t for p in row):
            continue
        down = tiles[mp[(r + 32) * W + c + 32]]
        up = tiles[mp[(r - 32) * W + c - 32]]
        new = tuple(tuple(old_t[y][x] or down[y][x] or up[y][x] for x in range(8))
                    for y in range(8))
        if new == old_t:
            continue
        if new not in index:
            index[new] = len(tiles)
            tiles.append(new)
        mp[r * W + c] = index[new]
        fixed += 1
print('techno: %d tiles del pozo con pixeles transparentes rellenados' % fixed)

assert len(tiles) < 4096
with open(os.path.join(D, 'techno_tiles.bin'), 'wb') as f:
    for t in tiles:
        for row in t:
            f.write(bytes(((row[k] << 4) | row[k + 1]) for k in range(0, 8, 2)))
with open(os.path.join(D, 'techno_map.bin'), 'wb') as f:
    f.write(struct.pack('>%dH' % (W * H), *mp))
print('techno: %d celdas cambiadas, %d tiles (%d nuevos)' % (changed, len(tiles), len(tiles) - len(raw) // 32))
