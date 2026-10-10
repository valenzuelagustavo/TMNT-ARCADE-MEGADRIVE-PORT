# -*- coding: utf-8 -*-
"""
gen_option_menu.py - (10/10) Arma los recursos de la pantalla de OPCIONES a
partir del arte de Gustavo "res/images/option_menu/TELA OPTION_15 CORES.png".

El PNG fuente (320x509) trae:
  filas   0..7    muestras de paleta (se ignoran)
  filas  20..243  la pantalla de EJEMPLO, con textos ("GAME OPTION", el menu
                  y los valores) y la flechita del cursor
  filas 285..508  la MISMA pantalla limpia, sin ningun texto

Salidas (en la misma carpeta del PNG fuente):
  option_bg.png     320x224  la pantalla limpia + el titulo "GAME OPTION"
                             copiado del ejemplo. Las cuatro cabecitas del
                             marco se BORRAN (quedan en negro): las dibuja el
                             juego con char_select_head, la misma hoja de la
                             seleccion de personaje, y parpadean.
  option_arrow.png    8x8    la flechita del cursor (sprite).
  option_font.png   760x8    fuente ASCII 32..126 (95 tiles de 8x8) con la
                             letra del ejemplo, compatible con VDP_loadFont.
                             Los glifos que aparecen en el ejemplo se copian
                             tal cual; los que faltan (digitos 1,2,4..9, F, Q,
                             W, Z y algo de puntuacion) estan dibujados abajo,
                             con el mismo trazo (2 px vertical, 1 px
                             horizontal, 7x7).

Todo usa la paleta de 15 colores del PNG fuente (PAL0 en el juego). Las
letras van en el indice 13 (blanco); el 0 es transparente.
"""
from PIL import Image
import numpy as np
import os, sys

DIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', 'res', 'images', 'option_menu')
SRC = os.path.join(DIR, 'TELA OPTION_15 CORES.png')

EXAMPLE_Y, CLEAN_Y = 20, 285
WHITE = 13

src = Image.open(SRC)
assert src.mode == 'P'
pal = src.getpalette()[:48]
a = np.array(src)
ex = a[EXAMPLE_Y:EXAMPLE_Y + 224].copy()
clean = a[CLEAN_Y:CLEAN_Y + 224].copy()

def save(arr, name):
    im = Image.fromarray(arr.astype(np.uint8), 'P')
    im.putpalette(pal)
    path = os.path.join(DIR, name)
    im.save(path, transparency=0)
    print('wrote', path, im.size)

# --- Fondo -----------------------------------------------------------------
bg = clean.copy()
# Titulo "GAME OPTION" (x 106..214, y 7..17 en el ejemplo).
bg[0:30, 96:224] = ex[0:30, 96:224]
# Cabecitas del marco: se borran (las pone el juego como sprites).
HEADS = ((45, 7), (73, 7), (227, 7), (255, 7))
leo = ex[7:23, 45:66]
mask = leo != 0
for x, y in HEADS:
    reg = bg[y:y + 16, x:x + 21]
    assert (reg[mask] == leo[mask]).all(), 'la cabecita no esta donde se esperaba'
    reg[mask] = 1
assert not (bg == 0).any()
save(bg, 'option_bg.png')

# --- Flechita --------------------------------------------------------------
arrow = np.zeros((8, 8), np.uint8)
sub = ex[48:56, 40:48]
arrow[sub == WHITE] = WHITE
save(arrow, 'option_arrow.png')

# --- Fuente ----------------------------------------------------------------
# (texto, x, y) de cada linea del ejemplo: la grilla es de 8 px.
LINES = [('GAME LEVEL', 60, 48), ('PLAYER REST', 60, 64), ('CONTINUE', 60, 80),
         ('CONTROL PAD', 60, 96), ('SOUND TEST', 60, 160), ('EXIT', 60, 176),
         ('NORMAL', 196, 48), ('03', 196, 64), ('TYPE A', 196, 96),
         ('A.ATTACK', 196, 112), ('B.JUMP', 196, 128), ('C.DASH', 196, 144),
         ('BGM', 196, 160)]
glyphs = {}
for text, x0, y0 in LINES:
    for k, ch in enumerate(text):
        if ch == ' ':
            continue
        g = (ex[y0:y0 + 8, x0 + 8 * k:x0 + 8 * k + 8] == WHITE)
        if ch in glyphs:
            assert (glyphs[ch] == g).all(), 'glifo inconsistente: ' + ch
        glyphs[ch] = g

DRAWN = {
    '1': ['..##...', '.###...', '..##...', '..##...', '..##...', '..##...', '.####..'],
    '2': ['.#####.', '##...##', '.....##', '..####.', '.##....', '##.....', '#######'],
    '4': ['...###.', '..####.', '.##.##.', '##..##.', '#######', '....##.', '....##.'],
    '5': ['#######', '##.....', '######.', '.....##', '.....##', '##...##', '.#####.'],
    '6': ['.#####.', '##.....', '##.....', '######.', '##...##', '##...##', '.#####.'],
    '7': ['#######', '##...##', '....##.', '...##..', '..##...', '..##...', '..##...'],
    '8': ['.#####.', '##...##', '##...##', '.#####.', '##...##', '##...##', '.#####.'],
    '9': ['.#####.', '##...##', '##...##', '.######', '.....##', '.....##', '.#####.'],
    'F': ['#######', '##.....', '##.....', '######.', '##.....', '##.....', '##.....'],
    'Q': ['.#####.', '##...##', '##...##', '##...##', '##.#.##', '##..##.', '.###.##'],
    'W': ['##...##', '##...##', '##...##', '##.#.##', '##.#.##', '#######', '.##.##.'],
    'Z': ['#######', '....##.', '...##..', '..##...', '.##....', '##.....', '#######'],
    '-': ['.......', '.......', '.......', '.#####.', '.......', '.......', '.......'],
    ':': ['.......', '..##...', '..##...', '.......', '..##...', '..##...', '.......'],
    '+': ['.......', '..##...', '..##...', '######.', '..##...', '..##...', '.......'],
    '/': ['.....##', '....##.', '...##..', '..##...', '.##....', '##.....', '.......'],
    '!': ['..##...', '..##...', '..##...', '..##...', '..##...', '.......', '..##...'],
    '?': ['.#####.', '##...##', '....##.', '...##..', '...##..', '.......', '...##..'],
    '(': ['...##..', '..##...', '.##....', '.##....', '.##....', '..##...', '...##..'],
    ')': ['..##...', '...##..', '....##.', '....##.', '....##.', '...##..', '..##...'],
}
for ch, rows in DRAWN.items():
    assert ch not in glyphs, 'ya existe en el ejemplo: ' + ch
    g = np.zeros((8, 8), bool)
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            g[y, x] = (c == '#')
    glyphs[ch] = g

font = np.zeros((8, 95 * 8), np.uint8)
for code in range(32, 127):
    ch = chr(code)
    if 'a' <= ch <= 'z':
        ch = ch.upper()          # minusculas = mayusculas, como title_font
    if ch in glyphs:
        font[:, (code - 32) * 8:(code - 31) * 8][glyphs[ch]] = WHITE
missing = [c for c in "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.-:+/!?()" if c not in glyphs]
assert not missing, missing
save(font, 'option_font.png')
