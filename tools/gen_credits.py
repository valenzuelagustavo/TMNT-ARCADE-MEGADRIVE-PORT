# -*- coding: utf-8 -*-
# Genera el arte de la pantalla de CREDITOS del final (18/10):
#
#   res/images/credits/credits_turtle_0..3.png  160x112 cada uno: los cuadros
#       de Leo, Mike, Don y Raph que pasan de derecha a izquierda, UNO A LA
#       VEZ. Salen de credits_turtles.png (320x224, los cuatro en cuadricula,
#       la imagen que paso Gustavo): se recorta el interior de cada cuadro
#       (154x106) y se le pone un borde negro de 3 px.
#   res/images/credits/credits_konami.png    72x24, el logo de KONAMI (el de
#       abajo de la pantalla de seleccion, char_select_bg.png) para el
#       "PRODUCED BY".
#
# Las dos fuentes usan la MISMA paleta (la de la seleccion de personaje):
# va toda en PAL1. El indice 0 queda transparente (en el logo, el negro del
# fondo -indice 1- pasa a 0 para que se vea la ciudad detras); el negro del
# borde de los cuadros es el indice 1 (opaco).
#
# Uso:  python3 tools/gen_credits.py      (desde la raiz del proyecto)
import os
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "res", "images", "hud", "char_select_bg.png")
TURTLES = os.path.join(ROOT, "res", "images", "credits", "credits_turtles.png")
DST = os.path.join(ROOT, "res", "images", "credits")

# Interior de cada cuadro en credits_turtles.png (x, y), 154x106.
# Orden de salida: Leo, Mike, Don, Raph.
CELLS = ((4, 4), (4, 114), (162, 4), (162, 114))
CELL_W, CELL_H = 154, 106
BORDER = 3                       # -> 160x112 (20x14 tiles)
BLACK = 1                        # indice del negro opaco
LOGO = (64, 198, 136, 222)       # logo de KONAMI (72x24)


def main():
    src = Image.open(SRC)
    assert src.mode == "P"
    pal = src.getpalette()[:48]
    os.makedirs(DST, exist_ok=True)

    tur = Image.open(TURTLES)
    assert tur.mode == "P"
    for k, (x, y) in enumerate(CELLS):
        cell = Image.new("P", (CELL_W + 2 * BORDER, CELL_H + 2 * BORDER), BLACK)
        cell.putpalette(pal)
        cell.paste(tur.crop((x, y, x + CELL_W, y + CELL_H)), (BORDER, BORDER))
        cell.save(os.path.join(DST, "credits_turtle_%d.png" % k), transparency=0)

    logo = src.crop(LOGO)
    px = logo.load()
    for y in range(logo.size[1]):
        for x in range(logo.size[0]):
            if px[x, y] == 1:
                px[x, y] = 0
    logo.putpalette(pal)
    logo.save(os.path.join(DST, "credits_konami.png"), transparency=0)
    print("ok")


if __name__ == "__main__":
    main()
