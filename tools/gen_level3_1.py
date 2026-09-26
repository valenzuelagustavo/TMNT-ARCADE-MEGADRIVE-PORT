#!/usr/bin/env python3
# =============================================================================
# gen_level3_1.py  (26/09) -- limites caminables de la Scene 3 (sewer)
# =============================================================================
# El fondo (bg_sewer.png + bg_sewer_fg.png) y la colision pintada
# (col_sewer.bin) vienen del proyecto del companero (Ray Project,
# res/images/lvl_4_sewer/). Su colision tiene, por columna de 8 px, hasta dos
# bandas caminables [top..bot] (la vereda y el canal) y un flag de
# "transicion" que permite pasar caminando de una a la otra. En todo el nivel
# el flag esta prendido, asi que para nosotros es UNA sola franja por columna:
# desde el tope de la banda de arriba hasta el piso de abajo.
#
# Formato de col_sewer.bin: u16 big-endian nCols + 6 bytes por columna
#   [n, top0/8, bot0/8, top1/8, bot1/8, transicion]
#
# Salida: src/level3_1_limits.c / .h con lvl31WalkTop[] (u8, px, por columna).
# El piso (el borde de abajo de la franja) es LVL31_WALK_Y_MAX en level3_1.c.
#
# Uso: python3 tools/gen_level3_1.py
# =============================================================================
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COL = os.path.join(ROOT, "res", "images", "lvl_3_sewer", "col_sewer.bin")
OUT_C = os.path.join(ROOT, "src", "level3_1_limits.c")
OUT_H = os.path.join(ROOT, "src", "level3_1_limits.h")


def main():
    d = open(COL, "rb").read()
    n = (d[0] << 8) | d[1]
    if len(d) != 2 + n * 6:
        sys.exit("col_sewer.bin: tamano inesperado")
    tops = []
    for c in range(n):
        p = d[2 + c * 6:8 + c * 6]
        bands = [(p[1] * 8, p[2] * 8)]
        if p[0] >= 2:
            bands.append((p[3] * 8, p[4] * 8))
        tops.append(min(b[0] for b in bands))
    with open(OUT_H, "w") as f:
        f.write("// GENERADO por tools/gen_level3_1.py -- NO EDITAR A MANO\n")
        f.write("#ifndef _LEVEL3_1_LIMITS_H_\n#define _LEVEL3_1_LIMITS_H_\n")
        f.write("#include <genesis.h>\n")
        f.write("#define LVL31_COLS %d\n" % n)
        f.write("extern const u8 lvl31WalkTop[LVL31_COLS];\n#endif\n")
    with open(OUT_C, "w") as f:
        f.write("// GENERADO por tools/gen_level3_1.py -- NO EDITAR A MANO\n")
        f.write('#include "level3_1_limits.h"\n\n')
        f.write("// Tope caminable (pies, px de mundo) por columna de 8 px.\n")
        f.write("const u8 lvl31WalkTop[LVL31_COLS] = {\n")
        for i in range(0, n, 16):
            f.write("    " + ", ".join("%3d" % t for t in tops[i:i + 16]) + ",\n")
        f.write("};\n")
    print("level3_1_limits: %d columnas, tope %d..%d" % (n, min(tops), max(tops)))


if __name__ == "__main__":
    main()
