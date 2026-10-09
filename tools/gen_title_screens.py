# -*- coding: utf-8 -*-
"""
gen_title_screens.py -- pantalla del titulo con la paleta nueva (09/10)

Fuente: res/images/intro_tmnt/Arcade - Teenage Mutant Ninja Turtles -
        Miscellaneous -TELA START.png  (320x670, 16 colores)
    y   0..8    tira con la paleta
    y  16..239  pantalla 1: el final de la intro (banner, logo, Konami y el
                texto de copyright completo)
    y 296..519  pantalla 2: el menu (1 TURTLE / 2 TURTLES / OPTIONS) con la
                cabecita de cursor
    y 541..565  el banner suelto
    y 577..648  el logo "TURTLES" suelto (se usa como mascara para separarlo)

El celeste (indice 9) pasa al indice 0 (transparente) y el color 0 de la
paleta queda celeste: el fondo es el color de backdrop del VDP y no gasta
tiles. Todo comparte esta misma paleta (PAL0).

Salidas en res/images/intro_tmnt/genesis/:
    title_banner.png   320x40   solo el banner (filas de tile 0..4)
    title_logo.png     320x80   solo "TURTLES" (filas de tile 6..15)
    title_rest.png     320x184  el resto de la pantalla 1 desde la fila de
                                tile 5: TM, logo de Konami y copyright
    title_menu_bg.png  320x224  la pantalla 2 SIN el menu ni el cursor (van
                                aparte: title_menu3/4 y title_cursor)
    title_cursor.png   24x16    la cabecita del cursor
    title_menu3.png    80x48    el menu tal cual el arte (1 TURTLE / 2 TURTLES
                                / OPTIONS), x 128..207, y 144..191
    title_menu4.png    80x64    el menu con el secreto de 4 jugadores: suma
                                "4 TURTLES" (el "2 TURTLES" con un 4 dibujado
                                en el mismo estilo) y OPTIONS baja 17 px

Uso:  python tools/gen_title_screens.py
"""
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SRC = os.path.join(ROOT, 'res', 'images', 'intro_tmnt',
                   'Arcade - Teenage Mutant Ninja Turtles - Miscellaneous -TELA START.png')
OUT = os.path.join(ROOT, 'res', 'images', 'intro_tmnt', 'genesis')
BLUE = 9

S1_Y, S2_Y = 16, 296
LOGO_PIECE = (577, 649)      # filas del logo suelto
LOGO_AT = (40, 49)           # donde cae el logo dentro de la pantalla (x, y)
MENU_ROWS = (140, 192)       # filas de la pantalla 2 con el menu y el cursor
CURSOR_BOX = (104, 128, 142, 158)
MENU_X, MENU_W = 128, 80     # recorte del menu (tiles 16..25)
MENU_Y = 144                 # fila de tile 18
ITEM_TOPS = (147, 164, 181)  # 1 TURTLE, 2 TURTLES, OPTIONS en el arte
ITEM_STEP = 17
WHITE = 13
# El arte no trae un 4: este va en el mismo estilo (7x7, trazo de 2 px).
GLYPH_4 = ['...###.',
           '..####.',
           '.##.##.',
           '##..##.',
           '#######',
           '....##.',
           '....##.']
DIGIT_X = 132                # columna del numero en cada opcion


def save(name, arr, pal):
    im = Image.fromarray(arr.astype(np.uint8), 'P')
    im.putpalette(pal + [0] * (768 - len(pal)))
    im.save(os.path.join(OUT, name), transparency=0)
    print('%-18s %dx%d' % (name, arr.shape[1], arr.shape[0]))


def main():
    im = Image.open(SRC)
    a = np.array(im)
    pal = im.getpalette()[:48]
    pal[0:3] = pal[BLUE * 3:BLUE * 3 + 3]          # el 0 pasa a ser el celeste
    a = np.where(a == BLUE, 0, a)

    s1 = a[S1_Y:S1_Y + 224].copy()
    s2 = a[S2_Y:S2_Y + 224].copy()

    # Mascara del logo "TURTLES" (la pieza suelta, ubicada en la pantalla).
    piece = a[LOGO_PIECE[0]:LOGO_PIECE[1]]
    ys, xs = np.nonzero(piece)
    piece = piece[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    lx, ly = LOGO_AT
    logo_mask = np.zeros(s1.shape, bool)
    logo_mask[ly:ly + piece.shape[0], lx:lx + piece.shape[1]] = piece != 0
    assert (s1[logo_mask] == piece[piece != 0]).all(), 'el logo no coincide'

    banner = np.zeros((40, 320), np.uint8)
    banner[:] = s1[0:40]
    assert not s1[0:40][logo_mask[0:40]].any()
    save('title_banner.png', banner, pal)

    logo = np.where(logo_mask, s1, 0)[48:128]
    assert not logo_mask[:48].any() and not logo_mask[128:].any()
    save('title_logo.png', logo, pal)

    rest = np.where(logo_mask, 0, s1)[40:224]
    save('title_rest.png', rest, pal)

    x0, x1, y0, y1 = CURSOR_BOX
    cur = s2[y0:y1, x0:x1].copy()
    save('title_cursor.png', cur, pal)

    menu = s2.copy()
    menu[MENU_ROWS[0]:MENU_ROWS[1], :] = 0
    save('title_menu_bg.png', menu, pal)

    # El menu, sacado del arte (pixeles blancos), en su lugar exacto.
    items = [s2[t:t + 8, MENU_X:MENU_X + MENU_W].copy() for t in ITEM_TOPS]
    four = items[1].copy()
    four[:, DIGIT_X - MENU_X - 1:DIGIT_X - MENU_X + 8] = 0
    for gy, row in enumerate(GLYPH_4):
        for gx, ch in enumerate(row):
            if ch == '#':
                four[gy, DIGIT_X - MENU_X + gx] = WHITE
    for name, rows in (('title_menu3.png', [items[0], items[1], items[2]]),
                       ('title_menu4.png', [items[0], items[1], four, items[2]])):
        h = -(-(ITEM_TOPS[0] - MENU_Y + ITEM_STEP * (len(rows) - 1) + 8) // 8) * 8
        img = np.zeros((h, MENU_W), np.uint8)
        for i, it in enumerate(rows):
            y = ITEM_TOPS[0] - MENU_Y + ITEM_STEP * i
            img[y:y + 8] = it
        save(name, img, pal)


if __name__ == '__main__':
    main()
