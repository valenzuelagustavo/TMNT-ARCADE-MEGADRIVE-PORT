#!/usr/bin/env python3
# =============================================================================
# stage_raw.py  (26/09) -- fondos de stage_bg.c en FORMATO ANCHO
# =============================================================================
# Un IMAGE de rescomp guarda el indice del tile en 11 bits: un fondo con mas
# de 2048 tiles unicos no entra. Este modulo arma, a partir de un png
# indexado, los dos BIN que lee sbgInitRaw() (ver SbgRaw en stage_bg.h):
#
#   <name>_tiles.bin  tiles 4bpp como SGDK (32 bytes por tile, big-endian)
#   <name>_map.bin    u16 big-endian por celda, fila por fila:
#                     bits 0-11 indice, bit 13 flip H, bit 14 flip V
#
# Deduplica con flips. El tile 0 es el vacio (todo indice 0 = transparente).
# La paleta la toma rescomp del mismo png (PALETTE).
#
# Lo usa tools/gen_level5_1.py (freeway).
# =============================================================================
import os, struct
from PIL import Image


def build(png, out_dir, name):
    im = Image.open(png)
    assert im.mode == 'P'
    W, H = im.size
    assert W % 8 == 0 and H % 8 == 0
    TW, TH = W // 8, H // 8
    px = im.load()

    def tile_at(tx, ty):
        return tuple(tuple(px[tx * 8 + x, ty * 8 + y] for x in range(8)) for y in range(8))

    def hflip(t): return tuple(r[::-1] for r in t)
    def vflip(t): return t[::-1]

    tiles = [tuple((0,) * 8 for _ in range(8))]
    index = {tiles[0]: (0, 0)}
    cells = []
    for ty in range(TH):
        for tx in range(TW):
            t = tile_at(tx, ty)
            hit = None
            for fl, v in ((0, t), (1, hflip(t)), (2, vflip(t)), (3, vflip(hflip(t)))):
                if v in index:
                    i, f0 = index[v]
                    hit = (i, f0 ^ fl)
                    break
            if hit is None:
                i = len(tiles)
                tiles.append(t)
                index[t] = (i, 0)
                hit = (i, 0)
            i, fl = hit
            assert i < 4096, 'mas de 4096 tiles unicos'
            cells.append(i | ((fl & 1) << 13) | ((fl >> 1) << 14))

    with open(os.path.join(out_dir, name + '_tiles.bin'), 'wb') as f:
        for t in tiles:
            for row in t:
                f.write(bytes(((row[k] << 4) | row[k + 1]) for k in range(0, 8, 2)))
    with open(os.path.join(out_dir, name + '_map.bin'), 'wb') as f:
        for c in cells:
            f.write(struct.pack('>H', c))

    # Peor caso de tiles distintos en 42 columnas (para el cache de stage_bg).
    cols = [set(cells[r * TW + c] & 0x0FFF for r in range(TH)) - {0} for c in range(TW)]
    worst = max(len(set().union(*cols[c:c + 42])) for c in range(TW - 41))
    print(f'{TW}x{TH} celdas, {len(tiles)} tiles, peor caso en 42 columnas: {worst}')
    return len(tiles), worst
