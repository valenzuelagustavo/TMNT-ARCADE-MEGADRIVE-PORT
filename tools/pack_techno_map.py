# -*- coding: utf-8 -*-
# pack_techno_map.py (18/10) -- el mapa del Technodrome POR BLOQUES
#
# techno_map.bin (584x352 celdas u16 = 411 KB) pasaba la ROM de los 4 MB: la
# Mega Drive solo ve 4 MB de cartucho sin mapper (en Gens/Kega/consola lo de
# arriba de 0x400000 se leia como basura). El 83% del mapa es el mismo tile de
# relleno, asi que se guarda en bloques de 8x8 celdas: los bloques UNICOS
# (64 u16 cada uno) y un indice u8 por bloque (73x44). ~29 KB.
#
# Entrada: res/images/lvl_8_techno/techno_map.bin (de gen_level8_1.py)
# Salida:  techno_blocks.bin (bloques, u16 big endian) y techno_bidx.bin (u8)
# Uso:     python3 tools/pack_techno_map.py  (gen_level8_1.py ya lo llama)
import os
import struct

W, H, B = 584, 352, 8
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "images", "lvl_8_techno")


def main():
    raw = open(os.path.join(D, "techno_map.bin"), "rb").read()
    mp = struct.unpack(">%dH" % (W * H), raw)
    assert W % B == 0 and H % B == 0
    blocks, index, idx = [], {}, []
    for br in range(H // B):
        for bc in range(W // B):
            blk = tuple(mp[(br * B + y) * W + bc * B + x] for y in range(B) for x in range(B))
            if blk not in index:
                index[blk] = len(blocks)
                blocks.append(blk)
            idx.append(index[blk])
    assert len(blocks) <= 256
    with open(os.path.join(D, "techno_blocks.bin"), "wb") as f:
        for blk in blocks:
            f.write(struct.pack(">64H", *blk))
    with open(os.path.join(D, "techno_bidx.bin"), "wb") as f:
        f.write(bytes(idx))
    print("techno: %d bloques unicos de 8x8, %d bytes (antes %d)"
          % (len(blocks), len(blocks) * 128 + len(idx), len(raw)))


if __name__ == "__main__":
    main()
