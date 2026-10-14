# -*- coding: utf-8 -*-
"""
gen_title_screens.py -- pantalla del titulo con la paleta nueva (09/10)

(14/10) Fuente nueva: el arte con "GENESIS" debajo del logo.
Fuente: res/images/intro_tmnt/Arcade - Teenage Mutant Ninja Turtles -
        Miscellaneous -TELA START _GENESIS_FINAL.png  (320x670, 16 colores)
    y   0..8    tira con la paleta
    y  15..238  pantalla 1: el final de la intro (banner, logo "TURTLES" con
                "GENESIS" debajo, TM, el texto de copyright y abajo el logo
                de Konami con "(C) KONAMI 1989"). Esta 1 px mas arriba que la
                pantalla 2 en el PNG: se toma desde la 15 para que las dos
                queden iguales.
    y 296..519  pantalla 2: el menu (1 TURTLE / 2 TURTLES / OPTIONS) con la
                cabecita de cursor
    (abajo, el banner y el logo sueltos: ya no se usan)

En este arte el logo se mete adentro del banner (y 36..43): los pixeles de
esas filas se reparten entre los dos. Lo que el logo tapa del banner se
rellena con el rojo del banner (y con el borde en las tres filas de abajo),
asi el banner cae entero antes de que entre el logo.

El celeste (indice 9) pasa al indice 0 (transparente) y el color 0 de la
paleta queda celeste: el fondo es el color de backdrop del VDP y no gasta
tiles. Todo comparte esta misma paleta (PAL0).

Salidas en res/images/intro_tmnt/genesis/:
    title_banner.png   320x48   solo el banner (filas de tile 0..5), BG_B
    title_logo.png     320x96   "TURTLES", "GENESIS" y el TM (filas de tile
                                4..15), BG_A: va por encima del banner
    title_rest.png     320x96   el resto de la pantalla 1 desde la fila de
                                tile 16: copyright, Konami y "(C) KONAMI 1989"
    title_menu_bg.png  320x224  la pantalla 2 SIN el menu ni el cursor (van
                                aparte: title_menu3/4 y title_cursor)
    title_cursor.png   24x16    la cabecita del cursor
    title_menu3.png    80x48    el menu tal cual el arte (1 TURTLE / 2 TURTLES
                                / OPTIONS), x 128..207, y 144..191
    title_menu4.png    80x64    el menu con el secreto de 4 jugadores: suma
                                "4 TURTLES" (el "2 TURTLES" con un 4 dibujado
                                en el mismo estilo). Arranca un renglon MAS
                                ARRIBA (y 128..191, renglones en 130, 147, 164
                                y 181): abajo esta el logo de Konami (y 198).

Uso:  python tools/gen_title_screens.py
"""
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SRC = os.path.join(ROOT, 'res', 'images', 'intro_tmnt',
                   'Arcade - Teenage Mutant Ninja Turtles - Miscellaneous -TELA START _GENESIS_FINAL.png')
OUT = os.path.join(ROOT, 'res', 'images', 'intro_tmnt', 'genesis')
BLUE = 9
RED, BORDER = 14, 1
GREENS = (2, 5, 8)

S1_Y, S2_Y = 15, 296
BANNER_Y = (0, 48)           # filas de tile 0..5
LOGO_Y = (32, 128)           # filas de tile 4..15
REST_Y = (128, 224)          # filas de tile 16..27
BANNER_ROWS = (13, 44)       # filas con banner
BANNER_X = (60, 256)         # el TM (x 258..) queda afuera
OVERLAP_Y = 36               # desde aca el logo se mete en el banner
BORDER_ROWS = 41             # desde aca, el borde de abajo del banner
EDGE = 4                     # px del borde del banner en cada fila
MENU_ROWS = (140, 192)       # filas de la pantalla 2 con el menu y el cursor
CURSOR_BOX = (104, 128, 142, 158)
MENU_X, MENU_W = 128, 80     # recorte del menu (tiles 16..25)
MENU_Y = 144                 # fila de tile 18
MENU4_Y = 128                # fila de tile 16 (el de 4 opciones)
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


def split_banner(s1):
    """-> (banner, mascara del banner en s1). El banner sale con los huecos
    que le hace el logo rellenos."""
    banner = np.zeros((BANNER_Y[1], 320), np.uint8)
    mask = np.zeros(s1.shape, bool)
    for y in range(*BANNER_ROWS):
        row = s1[y]
        xs = [x for x in range(*BANNER_X) if row[x] != 0]
        if not xs:
            continue
        x0, x1 = xs[0], xs[-1]
        fill = BORDER if y >= BORDER_ROWS else RED
        for x in range(x0, x1 + 1):
            c = row[x]
            logo = c in GREENS or c == 0
            if (y >= OVERLAP_Y and y < BORDER_ROWS and c == BORDER and
                    x0 + EDGE <= x <= x1 - EDGE):
                logo = True                     # contorno del logo sobre el rojo
            if logo:
                banner[y, x] = fill
            else:
                banner[y, x] = c
                mask[y, x] = True
    return banner, mask


def main():
    im = Image.open(SRC)
    a = np.array(im)
    pal = im.getpalette()[:48]
    pal[0:3] = pal[BLUE * 3:BLUE * 3 + 3]          # el 0 pasa a ser el celeste
    a = np.where(a == BLUE, 0, a)

    s1 = a[S1_Y:S1_Y + 224].copy()
    s2 = a[S2_Y:S2_Y + 224].copy()
    assert not s1[:1].any() and (s1[1:121] == s2[1:121]).all(), \
        'las pantallas 1 y 2 no coinciden arriba'

    banner, bmask = split_banner(s1)
    save('title_banner.png', banner, pal)

    logo = np.where(bmask, 0, s1)
    logo[:LOGO_Y[0]] = 0
    logo[LOGO_Y[1]:] = 0
    # todo lo que no es banner ni resto cae en el logo
    assert not np.where(bmask, 0, s1)[:LOGO_Y[0]].any(), 'pixeles sueltos arriba'
    save('title_logo.png', logo[LOGO_Y[0]:LOGO_Y[1]], pal)

    rest = s1[REST_Y[0]:REST_Y[1]]
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
    for name, top, rows in (
            ('title_menu3.png', MENU_Y, [items[0], items[1], items[2]]),
            ('title_menu4.png', MENU4_Y, [items[0], items[1], four, items[2]])):
        first = ITEM_TOPS[-1] - ITEM_STEP * (len(rows) - 1)   # termina en OPTIONS
        h = -(-(first - top + ITEM_STEP * (len(rows) - 1) + 8) // 8) * 8
        img = np.zeros((h, MENU_W), np.uint8)
        for i, it in enumerate(rows):
            y = first - top + ITEM_STEP * i
            img[y:y + 8] = it
        save(name, img, pal)


if __name__ == '__main__':
    main()
