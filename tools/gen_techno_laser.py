# -*- coding: utf-8 -*-
"""
gen_techno_laser.py (30/10) -- la barra LASER de la puerta del Technodrome
==========================================================================
Despues de que la puerta sale volando, en el hueco aparecen tres barras
amarillas que titilan; si la tortuga las toca se electrocuta (src/techno_lasers.c).
Medidas de las capturas del arcade (tools/electro_src/arcade_laser_0.png): cada
barra es un tubo de 4 px de ancho y 96 de alto (borde amarillo, centro claro)
con una tapita amarilla abajo; las tres van corridas 12 px a la derecha y 12
abajo (siguen la perspectiva del hueco).

Se dibuja con los colores de PAL1 (la paleta de las tortugas, la de
res/sprites/players/leo_new_13x13.png): 7 = amarillo, 13 = blanco. Asi no hace
falta cargar una paleta nueva: el sprite se crea con PAL1.

Salida: res/images/lvl_8_techno/techno_laser_bar.png, 2 cuadros de 8x104 (una
celda de 1x13 tiles cada uno, la barra en las columnas 2..5):
    cuadro 0  borde amarillo, centro blanco (como el arcade)
    cuadro 1  todo amarillo (el destello)
Uso: python3 tools/gen_techno_laser.py
"""
import os

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
SHEET = os.path.join(ROOT, 'res', 'sprites', 'players', 'leo_new_13x13.png')
OUT = os.path.join(ROOT, 'res', 'images', 'lvl_8_techno', 'techno_laser_bar.png')
YEL, WHT = 7, 13
CW, CH = 8, 104
BODY = 96          # alto del tubo (la tapita va en la fila 96)


def frame(core):
    f = np.zeros((CH, CW), np.uint8)
    f[0:BODY, 2] = YEL                 # borde izquierdo
    f[1:BODY, 5] = YEL                 # borde derecho (arranca una fila mas abajo)
    f[1:BODY, 3:5] = core              # centro
    f[BODY, 3:5] = YEL                 # tapita
    return f


def main():
    sheet = Image.open(SHEET)
    pal = sheet.getpalette()
    assert tuple(pal[YEL * 3:YEL * 3 + 3]) == (255, 206, 0), pal[YEL * 3:YEL * 3 + 3]
    assert tuple(pal[WHT * 3:WHT * 3 + 3]) == (255, 255, 255)
    out = np.concatenate([frame(WHT), frame(YEL)], axis=1)
    im = Image.fromarray(out, 'P')
    im.putpalette(pal)
    im.save(OUT, transparency=0)
    print('laser bar ->', out.shape[::-1])


if __name__ == '__main__':
    main()
