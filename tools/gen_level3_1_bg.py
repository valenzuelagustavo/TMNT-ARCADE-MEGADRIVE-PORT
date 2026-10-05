#!/usr/bin/env python3
# =============================================================================
# gen_level3_1_bg.py  (28/09, reescrito el 29/09) -- fondo y caños del sewer
# =============================================================================
# FUENTES (exportadas de Aseprite, 1252x288, 16 colores, indice 0 = magenta):
#   res/images/lvl_3_sewer/bg_sewer.png      el fondo COMPLETO (sin recortar)
#   res/images/lvl_3_sewer/bg_sewer_fg.png   los caños que pasan por delante
# (son exactamente las dos capas de "Arcade - ... Backgrounds - Stage 3.png").
#
# CAMARA VERTICAL (29/09). La pantalla tiene 224 de alto y el fondo 288: con
# el recorte de Ray (filas 32..255) la camara no podia subir. Ahora se usan
# las filas 0..255 (256 = 32 filas de tile = el alto del plano del VDP, asi no
# hace falta streamear en vertical) y stage_level sube la camara hasta 32 px
# (camYMin = -32) para mostrar la parte de arriba. Las Y de MUNDO no cambian:
# la fila 32 del fondo sigue siendo y = 0 (vereda 96..160, canal 190..216).
# Las 32 filas de abajo del todo (mas agua) quedan afuera, como antes.
#
# SALIDAS (res/images/lvl_3_sewer/ y src/):
#   bg_sewer_md.png        1248x256, el fondo listo para el Mega Drive (paleta
#                          con el agua animada, ver abajo). Lo lee rescomp
#                          (PALETTE).
#   bg_sewer_tiles.bin     Mas de 2048 tiles unicos: no entra en el indice de
#   bg_sewer_map.bin       11 bits de un IMAGE -> formato ancho de stage_bg.c
#                          (tools/stage_raw.py). El peor caso en 42 columnas
#                          es el minimo de bgSlots de level3_1.c.
#   bg_sewer_fg_md.png     1248x256, los caños, alineados con el fondo.
#   sewer_fg_top.png       Lo de los caños que puede caer DEBAJO DEL HUD (las
#   src/level3_1_fgtop.h   filas 0..63 del fondo: con la camara arriba del todo
#                          el HUD tapa las filas 0..31 y abajo del todo las
#                          32..63): una tira de frames de 8x64, uno por columna
#                          DISTINTA (las bajadas rectas se repiten), y por cada
#                          columna de tile con algo dibujado su X de mundo y
#                          su frame. stage_level los muestra como sprites
#                          (fgTop) con los tiles cargados UNA vez por frame
#                          (no uno por sprite): el HUD va en el plano WINDOW,
#                          que en esa franja reemplaza a BG_A.
#
# PALETTE / AGUA ANIMADA. Indice 0 en NEGRO (asi el color de fondo del VDP,
# PAL0[0], no es magenta). El agua usa el violeta (3) y dos grises (4 y 5) que
# el arcade ROTA; pero 4 y 5 tambien son de los caños y las escaleras, asi que
# el agua recibe indices propios, WATER_A (copia de 4) y WATER_B (copia de 5),
# y level3_1.c intercambia esos dos colores en la CRAM. Los lugares salen de:
#   - el 1, un negro repetido del 0: en el fondo sus pixeles pasan al 0 (en
#     BG_B el 0 muestra el color de fondo del VDP = PAL0[0] = negro) y en los
#     caños al 2 (4a0000, el mas oscuro que queda);
#   - el 14 (7b4a39), que en la VDP es el MISMO color que el 7 (635231: los
#     dos dan 3,2,1 en 3 bits por canal): sus pixeles pasan al 7.
# Solo se remapean los 4/5 del agua (filas >= WATER_ROW0).
#
# Uso: python3 tools/gen_level3_1_bg.py
# =============================================================================
import os
import sys
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stage_raw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = os.path.join(ROOT, 'res', 'images', 'lvl_3_sewer')
SRC_BG = os.path.join(D, 'bg_sewer.png')
SRC_FG = os.path.join(D, 'bg_sewer_fg.png')

W, H = 1248, 256      # 156 x 32 tiles
WATER_ROW0 = 202      # de aca para abajo, los 4/5 son del agua (el borde incluido)
WATER_A, WATER_B = 1, 14
TOP = 64              # filas del fondo que pueden quedar debajo del HUD


def main():
    sbg = Image.open(SRC_BG)
    sfg = Image.open(SRC_FG)
    assert sbg.mode == 'P' and sfg.mode == 'P', 'las fuentes tienen que ser indexadas'
    pal = list(sbg.getpalette()[:48])
    pal[0:3] = [0, 0, 0]
    pal[WATER_A * 3:WATER_A * 3 + 3] = pal[4 * 3:4 * 3 + 3]
    pal[WATER_B * 3:WATER_B * 3 + 3] = pal[5 * 3:5 * 3 + 3]

    bg = sbg.crop((0, 0, W, H))
    px = bg.load()
    for y in range(H):
        for x in range(W):
            v = px[x, y]
            assert v != 0, 'el fondo no deberia usar el indice 0'
            if v == 1:
                v = 0
            elif v == 14:
                v = 7
            elif y >= WATER_ROW0 and v == 4:
                v = WATER_A
            elif y >= WATER_ROW0 and v == 5:
                v = WATER_B
            px[x, y] = v
    bg.putpalette(pal)
    bg.save(os.path.join(D, 'bg_sewer_md.png'))

    fg = sfg.crop((0, 0, W, H))
    fq = fg.load()
    for y in range(H):
        for x in range(W):
            if fq[x, y] == 1:
                fq[x, y] = 2
            elif fq[x, y] == 14:
                fq[x, y] = 7
    fg.putpalette(pal)
    fg.save(os.path.join(D, 'bg_sewer_fg_md.png'), transparency=0)

    cols = [c for c in range(W // 8)
            if any(fq[c * 8 + x, y] for x in range(8) for y in range(TOP))]
    frames, frameOf = [], []
    for c in cols:
        key = tuple(fq[c * 8 + x, y] for y in range(TOP) for x in range(8))
        if key not in frames:
            frames.append(key)
        frameOf.append(frames.index(key))
    top = Image.new('P', (8 * max(1, len(frames)), TOP), 0)
    top.putpalette(pal)
    for c, fi in zip(cols, frameOf):
        top.paste(fg.crop((c * 8, 0, c * 8 + 8, TOP)), (fi * 8, 0))
    top.save(os.path.join(D, 'sewer_fg_top.png'), transparency=0)
    with open(os.path.join(ROOT, 'src', 'level3_1_fgtop.h'), 'w') as f:
        f.write('// GENERADO por tools/gen_level3_1_bg.py -- NO EDITAR A MANO\n')
        f.write('// Caños que pueden quedar debajo del HUD (ver fgTop en stage_level.h).\n')
        f.write('#ifndef _LEVEL3_1_FGTOP_H_\n#define _LEVEL3_1_FGTOP_H_\n')
        f.write('#define LVL31_FGTOP_N %d\n' % len(cols))
        f.write('#define LVL31_FGTOP_FRAMES %d\n' % len(frames))
        f.write('static const s16 lvl31FgTopX[LVL31_FGTOP_N] = { %s };\n'
                % ', '.join(str(c * 8) for c in cols))
        f.write('static const u8 lvl31FgTopF[LVL31_FGTOP_N] = { %s };\n'
                % ', '.join(str(i) for i in frameOf))
        f.write('#endif\n')
    fgt = set()
    for ty in range(H // 8):
        for tx in range(W // 8):
            t = tuple(fq[tx * 8 + x, ty * 8 + y] for y in range(8) for x in range(8))
            if any(t):
                fgt.add(t)
    print('caños: %d tiles (sin contar flips), %d columnas debajo del HUD (%d distintas)'
          % (len(fgt), len(cols), len(frames)))

    n, worst = stage_raw.build(os.path.join(D, 'bg_sewer_md.png'), D, 'bg_sewer')
    print('bg_sewer: %d tiles, peor caso %d (bgSlots de level3_1.c tiene que ser >=)'
          % (n, worst))


if __name__ == '__main__':
    main()
