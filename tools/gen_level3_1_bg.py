#!/usr/bin/env python3
# =============================================================================
# gen_level3_1_bg.py  (28/09) -- fondo y primer plano de la Scene 3 (sewer)
# =============================================================================
# Fuente: res/images/lvl_3_sewer/
#   "Arcade - Teenage Mutant Ninja Turtles - Backgrounds - Stage 3.png"
# (1252x620, 16 colores, el indice 0 = magenta = transparente). La hoja trae
# DOS capas apiladas:
#   - filas  12..299: el fondo (288 de alto)
#   - filas 312..599: el primer plano, los caños que pasan por delante
#
# Salidas (todas en res/images/lvl_3_sewer/):
#   bg_sewer.png         1248x224. Filas 44..267 de la hoja (= filas 32..255
#                        del fondo): el MISMO recorte que tenia el fondo de Ray,
#                        asi las Y de mundo de siempre siguen valiendo (vereda
#                        96..160, canal 182..216). El ancho baja de 1252 a 1248
#                        (156 columnas de tile). Solo lo lee rescomp (PALETTE).
#   bg_sewer_tiles.bin   El fondo tiene 2252 tiles unicos (el agua nueva es
#   bg_sewer_map.bin     mucho mas rica que la de Ray): NO entra en el indice
#                        de 11 bits de un IMAGE -> formato ancho de stage_bg.c
#                        (ver tools/stage_raw.py).
#   bg_sewer_fg.png      1248x224, los caños. (28/09, 2da pasada) Van ALINEADOS
#                        con el fondo como estan en la hoja: misma X y el mismo
#                        recorte vertical (filas 344..567 = 312 + 32). Asi los
#                        caños horizontales quedan contra la pared y los codos
#                        de las bajadas justo en el borde del escalon. La
#                        ubicacion de Ray (127 px a la izquierda y 30 px mas
#                        abajo) los dejaba cruzando la vereda. Para moverlos:
#                        FG_DX / FG_ROW0.
#   sewer_fg_top.png     (28/09) Lo del primer plano que cae en las filas 0-3
#   src/level3_1_fgtop.h (la franja del HUD, que en BG_A no scrollea): una
#                        tira de frames de 8x32, uno por columna de tile con
#                        algo dibujado, y la X de mundo de cada uno. stage_level
#                        los muestra como sprites (ver fgTop en stage_level.h).
#                        Sin esto las bajadas de los caños arrancaban recien
#                        debajo del HUD y parecian mas bajas que el nivel.
#
# La paleta es la de la hoja con el indice 0 en NEGRO (el fondo no lo usa;
# asi el color de fondo del VDP, PAL0[0], no es magenta). En el primer plano
# el indice 0 es el transparente.
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
SRC = os.path.join(D, 'Arcade - Teenage Mutant Ninja Turtles - Backgrounds - Stage 3.png')

W, H = 1248, 224
BG_ROW0 = 44          # hoja: fila 12 (arranque del fondo) + 32 de recorte
FG_ROW0 = 344         # hoja: fila 312 (arranque del primer plano) + 32, como el fondo
FG_DX = 0             # misma X que en la hoja


def paletted(img, pal):
    out = Image.new('P', img.size, 0)
    out.putpalette(pal)
    out.putdata(list(img.getdata()))
    return out


def main():
    src = Image.open(SRC)
    assert src.mode == 'P', 'la hoja tiene que ser indexada'
    pal = list(src.getpalette()[:48])
    pal[0:3] = [0, 0, 0]

    bg = paletted(src.crop((0, BG_ROW0, W, BG_ROW0 + H)), pal)
    assert 0 not in set(bg.getdata()), 'el fondo no deberia usar el indice 0'
    bg.save(os.path.join(D, 'bg_sewer.png'))

    # crop() fuera de la hoja rellena con 0 (transparente).
    fg = paletted(src.crop((FG_DX, FG_ROW0, FG_DX + W, FG_ROW0 + H)), pal)
    fg.save(os.path.join(D, 'bg_sewer_fg.png'), transparency=0)

    # Tramos del primer plano en la franja del HUD (filas de tile 0-3).
    TOP = 32
    cols = [c for c in range(W // 8)
            if any(fg.getpixel((c * 8 + x, y)) for x in range(8) for y in range(TOP))]
    top = Image.new('P', (8 * max(1, len(cols)), TOP), 0)
    top.putpalette(pal)
    for i, c in enumerate(cols):
        top.paste(fg.crop((c * 8, 0, c * 8 + 8, TOP)), (i * 8, 0))
    top.save(os.path.join(D, 'sewer_fg_top.png'), transparency=0)
    with open(os.path.join(ROOT, 'src', 'level3_1_fgtop.h'), 'w') as f:
        f.write('// GENERADO por tools/gen_level3_1_bg.py -- NO EDITAR A MANO\n')
        f.write('// Tramos del primer plano en la franja del HUD (ver fgTop en stage_level.h).\n')
        f.write('#ifndef _LEVEL3_1_FGTOP_H_\n#define _LEVEL3_1_FGTOP_H_\n')
        f.write('#define LVL31_FGTOP_N %d\n' % len(cols))
        f.write('static const s16 lvl31FgTopX[LVL31_FGTOP_N] = { %s };\n'
                % ', '.join(str(c * 8) for c in cols))
        f.write('#endif\n')
    print('sewer_fg_top: %d columnas en la franja del HUD' % len(cols))

    n, worst = stage_raw.build(os.path.join(D, 'bg_sewer.png'), D, 'bg_sewer')
    print('bg_sewer: %d tiles, peor caso %d (bgSlots de level3_1.c tiene que ser >=)'
          % (n, worst))


if __name__ == '__main__':
    main()
