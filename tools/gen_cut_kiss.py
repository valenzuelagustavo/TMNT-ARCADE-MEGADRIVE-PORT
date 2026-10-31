#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_cut_kiss.py (30/10) - cutscene "April besa a la tortuga" despues del garage (SCENE_CUT_KISS)
================================================================================================
Fuentes (res/images/lvl_4_garage/1500_1523/): 24 cuadros de 320x224 en dos capas
  1500_1523_TURTLES_NN.png  la tortuga (Leo) sobre fondo salmon, con la silueta negra (indice 15)
                            de donde va April, el globo (cuadros 5..10) y los corazones
  1500_1523_APRIL_NN.png    April (indice 0 = transparente)
Las dos capas son planos de 40x28 tiles: TURTLES en BG_B (PAL0, indices de color sin tocar: la
BANDANA y las munequeras de Leo son los indices 10 (claro) y 11 (oscuro) y el codigo los repinta
segun la tortuga que juega) y APRIL en BG_A (PAL1). El globo NO va en el plano: sale como sprite
(PAL2, una copia de la paleta de la tortuga) para que no cambie de color con la bandana; son los
pixeles que el cuadro 5 tiene de mas respecto del 1 ("I owe you one", azul con borde violeta: el
globo de esta escena en el arcade) y se pone donde esta en los cuadros.

Salidas (res/images/lvl_4_garage/kiss/):
  kiss_tiles.bin    tiles unicos (4bpp) de las dos capas y todos los cuadros; el 0 es vacio
  kiss_map.bin      mapa inicial (cuadro 1): BG_B (40x28 u16) y BG_A (40x28 u16); cada u16 =
                    (paleta << 13) | indice local de tile (el codigo le suma el tile base)
  kiss_delta.bin    para los cuadros 2..24, en orden: n, n x (posicion en el mapa, u16) para BG_B y
                    despues lo mismo para BG_A (solo lo que cambia respecto del cuadro anterior)
  kiss_pal_t.png / kiss_pal_a.png   paletas de 16 (PAL0 = tortuga, PAL1 = April)
  kiss_bubble.png   globo 64x32 con la paleta de la tortuga (PAL2)
  src/cut_kiss.h    constantes (generado)

Uso (desde la raiz del proyecto):  python tools/gen_cut_kiss.py
"""
import os
import struct
import numpy as np
from PIL import Image

SRC = 'res/images/lvl_4_garage/1500_1523/1500_1523_%s_%02d.png'
OUT = 'res/images/lvl_4_garage/kiss/'
NF = 24
TW, TH = 40, 28
BUB_FIRST, BUB_LAST = 4, 9            # indices de cuadro (0-based) con el globo en el arcade


def load(kind, i):
    im = Image.open(SRC % (kind, i))
    assert im.mode == 'P' and im.size == (320, 224)
    return np.array(im), im.getpalette()[:48]


def main():
    os.makedirs(OUT, exist_ok=True)
    T, A = [], []
    palT = palA = None
    for i in range(1, NF + 1):
        t, pt = load('TURTLES', i)
        a, pa = load('APRIL', i)
        palT = palT or pt
        palA = palA or pa
        assert pt == palT and pa == palA
        T.append(t.copy())
        A.append(a.copy())

    # Globo del cuadro 5..10: se ubica y se quita de la capa (debajo esta lo del cuadro 1)
    d = (T[BUB_FIRST] != T[0])
    ys, xs = np.where(d)
    bx, by = int(xs.min()), int(ys.min())
    print('globo en los cuadros: caja', (bx, by, int(xs.max()), int(ys.max())))
    for i in range(BUB_FIRST, BUB_LAST + 1):
        dd = (T[i] != T[0])
        yy, xx = np.where(dd)
        fuera = ((xx < bx) | (xx > bx + 63) | (yy < by) | (yy > by + 31)).sum()
        assert fuera <= 2, (i, fuera)           # (el cuadro 6 trae un pixel suelto en x = 0)
        T[i] = T[0].copy()                      # (un pixel suelto del cuadro 6 tambien se va)
    # En los cuadros con globo la capa de April tiene un hueco (708 px de pelo y cara que el globo
    # tapaba en la captura): se rellena con el cuadro 1 (con el globo como sprite se ve el borde).
    for i in range(BUB_FIRST, BUB_LAST + 1):
        hole = (A[i] == 0) & (A[0] != 0)
        print('cuadro', i + 1, 'hueco de April rellenado:', int(hole.sum()), 'px')
        A[i][hole] = A[0][hole]
    # Ruido: un pixel en (256,112) de la capa de April cambia entre cuadros iguales
    for i in range(NF):
        A[i][112, 256] = A[0][112, 256]

    # --- tiles unicos ---
    tiles = [np.zeros((8, 8), np.uint8)]
    index = {tiles[0].tobytes(): 0}

    def tid(t):
        k = t.tobytes()
        if k not in index:
            index[k] = len(tiles)
            tiles.append(t.copy())
        return index[k]

    def tmap(img, pal):
        m = np.zeros((TH, TW), np.uint16)
        for ty in range(TH):
            for tx in range(TW):
                m[ty, tx] = (pal << 13) | tid(img[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8])
        return m

    MT = [tmap(T[i], 0) for i in range(NF)]
    MA = [tmap(A[i], 1) for i in range(NF)]
    n = len(tiles)
    assert n < 2048
    print('tiles unicos:', n)

    with open(OUT + 'kiss_tiles.bin', 'wb') as f:
        for t in tiles:
            for row in t:
                for x in range(0, 8, 2):
                    f.write(bytes([(int(row[x]) << 4) | int(row[x + 1])]))
    with open(OUT + 'kiss_map.bin', 'wb') as f:
        f.write(MT[0].astype('>u2').tobytes())
        f.write(MA[0].astype('>u2').tobytes())
    dsz = 0
    with open(OUT + 'kiss_delta.bin', 'wb') as f:
        for i in range(1, NF):
            for L in (MT, MA):
                ch = np.flatnonzero((L[i] != L[i - 1]).ravel())
                f.write(struct.pack('>H', len(ch)))
                for p in ch:
                    f.write(struct.pack('>HH', int(p), int(L[i].ravel()[p])))
                dsz += 2 + 4 * len(ch)
    print('delta bytes', dsz)

    # --- verificacion: reconstruir cada cuadro desde tiles + mapas + deltas ---
    raw = open(OUT + 'kiss_tiles.bin', 'rb').read()
    def tile_px(k):
        b = raw[k * 32:(k + 1) * 32]
        px = np.zeros((8, 8), np.uint8)
        for r in range(8):
            for c in range(4):
                v = b[r * 4 + c]
                px[r, c * 2] = v >> 4
                px[r, c * 2 + 1] = v & 15
        return px
    def render(m):
        out = np.zeros((224, 320), np.uint8)
        for ty in range(TH):
            for tx in range(TW):
                out[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8] = tile_px(int(m[ty, tx]) & 0x1FFF)
        return out
    bm = open(OUT + 'kiss_map.bin', 'rb').read()
    cT = np.frombuffer(bm[:2240], '>u2').astype(np.uint16).copy()
    cA = np.frombuffer(bm[2240:], '>u2').astype(np.uint16).copy()
    dl = open(OUT + 'kiss_delta.bin', 'rb').read()
    off = 0
    for i in range(NF):
        if i > 0:
            for cur in (cT, cA):
                cnt = struct.unpack_from('>H', dl, off)[0]
                off += 2
                for _ in range(cnt):
                    p, v = struct.unpack_from('>HH', dl, off)
                    off += 4
                    cur[p] = v
        assert (render(cT.reshape(TH, TW)) == T[i]).all(), ('T', i)
        assert (render(cA.reshape(TH, TW)) == A[i]).all(), ('A', i)
    assert off == len(dl)
    print('verificado: los 24 cuadros se reconstruyen pixel a pixel')

    # --- paletas ---
    for name, pal in (('kiss_pal_t.png', palT), ('kiss_pal_a.png', palA)):
        im = Image.new('P', (16, 1))
        im.putpalette(pal + [0] * (768 - 48))
        im.putdata(list(range(16)))
        im.save(OUT + name)

    # --- globo: los pixeles que el cuadro 5 tiene de mas respecto del 1 (azul con borde violeta y
    # texto amarillo: el globo de ESTA escena en el arcade). Sprite 64x32 con la paleta de la
    # tortuga (los indices tal cual), que va en PAL2: no cambia con la bandana. ---
    t5 = np.array(Image.open(SRC % ('TURTLES', BUB_FIRST + 1)))
    box = np.zeros((32, 64), np.uint8)
    for yy in range(32):
        for xx in range(64):
            if t5[by + yy, bx + xx] != T[0][by + yy, bx + xx]:
                box[yy, xx] = t5[by + yy, bx + xx]
    assert box.max() > 0 and 0 not in (box[box > 0]), 'indice 0 en el globo'
    im = Image.fromarray(box, 'P')
    im.putpalette(palT + [0] * (768 - 48))
    im.save(OUT + 'kiss_bubble.png', transparency=0)
    print('globo: indices usados', sorted(set(int(v) for v in box.ravel())), 'pixeles', int((box > 0).sum()))

    with open('src/cut_kiss.h', 'w', newline='\r\n') as f:
        f.write('// GENERADO por tools/gen_cut_kiss.py -- NO EDITAR A MANO\n')
        f.write('#ifndef _CUT_KISS_H_\n#define _CUT_KISS_H_\n')
        f.write('#define KISS_TILES %d\n#define KISS_COLS %d\n#define KISS_ROWS %d\n#define KISS_FRAMES %d\n' % (n, TW, TH, NF))
        f.write('#define KISS_BUB_X %d\n#define KISS_BUB_Y %d\n' % (bx, by))
        f.write('#define KISS_BUB_FIRST %d\n#define KISS_BUB_LAST %d\n' % (BUB_FIRST, BUB_LAST))
        f.write('#endif\n')

    # --- vista previa: cuadros 1, 5, 11, 15, 19, 24 compuestos ---
    prev = Image.new('RGB', (960, 448))
    for k, i in enumerate((0, 4, 10, 14, 18, 23)):
        rgb = np.zeros((224, 320, 3), np.uint8)
        pt = np.array(palT).reshape(16, 3)
        pa = np.array(palA).reshape(16, 3)
        rgb[:] = pt[T[i]]
        m = A[i] != 0
        rgb[m] = pa[A[i]][m]
        prev.paste(Image.fromarray(rgb), ((k % 3) * 320, (k // 3) * 224))
    prev.save(OUT + '_preview.png')


if __name__ == '__main__':
    main()
