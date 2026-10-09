# -*- coding: utf-8 -*-
"""
gen_char_select.py - (08/10) Corta "select player_15 CORES.png" (arte de
Gustavo, res/images/hud/) en los tres recursos de la seleccion de personaje:

  char_select_bg.png     320x224  la pantalla entera (marcos 1UP..4UP con
                                  nombre y barra, "SELECT YOUR TURTLES", los 4
                                  retratos y la linea de KONAMI). IMAGE en PAL0.
  char_select_badge.png  128x128  los globos "nUP" que marcan la tortuga de
                                  cada jugador: 4 filas (jugador 1..4) x 4
                                  frames (tortuga: Leo, Mike, Don, Raph = el
                                  color del globo). Celdas de 32x32.
  char_select_head.png    48x64   la cabeza de cada marco, para el parpadeo:
                                  4 filas (tortuga) x 2 frames (ojos abiertos,
                                  cerrados). Celdas de 24x16.

Las tres usan la MISMA paleta de 15 colores del PNG fuente, asi que todo va
en PAL0. En la pantalla el negro es el indice 1 (opaco); el indice 0 queda
como transparente de los sprites.

Si Gustavo retoca el PNG fuente, se vuelve a correr y listo.
"""
from PIL import Image
import os, sys

HUD = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', 'res', 'images', 'hud')
SRC = os.path.join(HUD, 'select player_15 CORES.png')

# Donde esta cada cosa en el PNG fuente (medido sobre el arte).
SCREEN_Y = 15                                   # la pantalla son las filas 15..238
BADGE_X  = (51, 117, 181, 240)                  # columnas: Leo, Mike, Don, Raph
BADGE_Y  = (278, 318, 354, 390)                 # filas: 1UP, 2UP, 3UP, 4UP
BADGE_W, BADGE_H = 30, 29
HEAD_X   = ((41, 65), (109, 133), (172, 196), (231, 255))  # (abiertos, cerrados)
HEAD_Y   = 253
HEAD_W, HEAD_H = 21, 16

src = Image.open(SRC)
assert src.mode == 'P', 'el PNG fuente tiene que ser indexado'
pal = src.getpalette()[:48]

def new(w, h):
    im = Image.new('P', (w, h), 0)
    im.putpalette(pal)
    return im

def save(im, name):
    im.info['transparency'] = 0
    path = os.path.join(HUD, name)
    im.save(path, transparency=0)
    print('wrote', path, im.size)

bg = src.crop((0, SCREEN_Y, 320, SCREEN_Y + 224))
assert 0 not in set(bg.getdata()), 'la pantalla no deberia usar el indice 0'
save(bg, 'char_select_bg.png')

badge = new(4 * 32, 4 * 32)
for row, y in enumerate(BADGE_Y):
    for col, x in enumerate(BADGE_X):
        badge.paste(src.crop((x, y, x + BADGE_W, y + BADGE_H)), (col * 32, row * 32))
save(badge, 'char_select_badge.png')

head = new(2 * 24, 4 * 16)
for row, xs in enumerate(HEAD_X):
    for f, x in enumerate(xs):
        head.paste(src.crop((x, HEAD_Y, x + HEAD_W, HEAD_Y + HEAD_H)), (f * 24, row * 16))
save(head, 'char_select_head.png')
