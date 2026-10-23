# -*- coding: utf-8 -*-
# gen_level7_1_fgtop.py (18/10) -- columnas de la fabrica en la franja del HUD
#
# Las dos columnas de reticulado de la fabrica (bg_factory_fg.png, BG_A con
# prioridad alta) llegan hasta arriba del todo, pero las filas 0..3 de BG_A
# son del HUD (no scrollean): ahi la columna quedaba cortada. Igual que los
# caños de la cloaca (tools/gen_level3_1_bg.py), lo que cae en esas filas se
# muestra como sprites de 8x32 que scrollean con la camara (fgTop de
# stage_level): uno por columna de tile con algo dibujado, con los frames
# repetidos compartidos (las dos columnas son iguales).
#
# Salidas:
#   res/images/lvl_7_factory/factory_fg_top.png  tira de frames de 8x32
#   src/level7_1_fgtop.h                         X de mundo y frame de cada una
#
# Uso:  python3 tools/gen_level7_1_fgtop.py   (desde la raiz del proyecto)
import os
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
D = os.path.join(ROOT, "res", "images", "lvl_7_factory")
TOP = 32            # filas del HUD (STAGE_HUD_ROWS * 8)


def main():
    fg = Image.open(os.path.join(D, "bg_factory_fg.png"))
    assert fg.mode == "P"
    pal = fg.getpalette()[:48]
    W = fg.size[0]
    px = fg.load()
    cols = [c for c in range(W // 8)
            if any(px[c * 8 + x, y] for x in range(8) for y in range(TOP))]
    frames, frameOf = [], []
    for c in cols:
        key = tuple(px[c * 8 + x, y] for y in range(TOP) for x in range(8))
        if key not in frames:
            frames.append(key)
        frameOf.append(frames.index(key))
    top = Image.new("P", (8 * max(1, len(frames)), TOP), 0)
    top.putpalette(pal)
    for c, fi in zip(cols, frameOf):
        top.paste(fg.crop((c * 8, 0, c * 8 + 8, TOP)), (fi * 8, 0))
    top.save(os.path.join(D, "factory_fg_top.png"), transparency=0)
    with open(os.path.join(ROOT, "src", "level7_1_fgtop.h"), "w", newline="\r\n") as f:
        f.write("// GENERADO por tools/gen_level7_1_fgtop.py -- NO EDITAR A MANO\n")
        f.write("// Columnas de la fabrica en la franja del HUD (ver fgTop en stage_level.h).\n")
        f.write("#ifndef _LEVEL7_1_FGTOP_H_\n#define _LEVEL7_1_FGTOP_H_\n")
        f.write("#define LVL71_FGTOP_N %d\n" % len(cols))
        f.write("#define LVL71_FGTOP_FRAMES %d\n" % len(frames))
        f.write("static const s16 lvl71FgTopX[LVL71_FGTOP_N] = { %s };\n"
                % ", ".join(str(c * 8) for c in cols))
        f.write("static const u8 lvl71FgTopF[LVL71_FGTOP_N] = { %s };\n"
                % ", ".join(str(i) for i in frameOf))
        f.write("#endif\n")
    print("factory_fg_top: %d columnas, %d frames" % (len(cols), len(frames)))


if __name__ == "__main__":
    main()
