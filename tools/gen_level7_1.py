#!/usr/bin/env python3
# =============================================================================
# gen_level7_1.py  (26/09) -- limites caminables de la Scene 7 (fabrica)
# =============================================================================
# El fondo (bg_factory.png + bg_factory_fg.png) y la colision pintada
# (col_factory.bin, que en el proyecto de Ray se llama col_l6c.bin) vienen del
# proyecto del companero. Mismo formato que la del sewer (ver gen_level3_1.py):
#   u16 big-endian nCols + 6 bytes por columna
#   [n, top0/8, bot0/8, top1/8, bot1/8, transicion]
#
# La banda de arriba (cuando hay dos: columnas 61-106) es el techo de la
# plataforma "NFP-1" del fondo. (18/10, Gustavo) Ahora se puede SUBIR: el tope
# de esas columnas es el de la plataforma (64) y la cara (96..128) la maneja
# el escalon de tramo de stage_level (ledgeTop/Bot/X0/X1 en level7_1.c).
# Antes se usaba solo la banda del piso.
#
# Salida: src/level7_1_limits.c / .h con lvl71WalkTop[] (u8, px, por columna).
#
# Uso: python3 tools/gen_level7_1.py
# =============================================================================
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COL = os.path.join(ROOT, "res", "images", "lvl_7_factory", "col_factory.bin")
OUT_C = os.path.join(ROOT, "src", "level7_1_limits.c")
OUT_H = os.path.join(ROOT, "src", "level7_1_limits.h")


def main():
    d = open(COL, "rb").read()
    n = (d[0] << 8) | d[1]
    if len(d) != 2 + n * 6:
        sys.exit("col_factory.bin: tamano inesperado")
    tops = []
    for c in range(n):
        p = d[2 + c * 6:8 + c * 6]
        bands = [(p[1] * 8, p[2] * 8)]
        if p[0] >= 2:
            bands.append((p[3] * 8, p[4] * 8))
        tops.append(min(b[0] for b in bands))
    with open(OUT_H, "w") as f:
        f.write("// GENERADO por tools/gen_level7_1.py -- NO EDITAR A MANO\n")
        f.write("#ifndef _LEVEL7_1_LIMITS_H_\n#define _LEVEL7_1_LIMITS_H_\n")
        f.write("#include <genesis.h>\n")
        f.write("#define LVL71_COLS %d\n" % n)
        f.write("#define LVL71_TOP_MIN %d\n" % min(tops))
        f.write("extern const u8 lvl71WalkTop[LVL71_COLS];\n#endif\n")
    with open(OUT_C, "w") as f:
        f.write("// GENERADO por tools/gen_level7_1.py -- NO EDITAR A MANO\n")
        f.write('#include "level7_1_limits.h"\n\n')
        f.write("// Tope caminable (pies, px de mundo) por columna de 8 px.\n")
        f.write("const u8 lvl71WalkTop[LVL71_COLS] = {\n")
        for i in range(0, n, 16):
            f.write("    " + ", ".join("%3d" % t for t in tops[i:i + 16]) + ",\n")
        f.write("};\n")
    print("level7_1_limits: %d columnas, tope %d..%d" % (n, min(tops), max(tops)))


if __name__ == "__main__":
    main()
