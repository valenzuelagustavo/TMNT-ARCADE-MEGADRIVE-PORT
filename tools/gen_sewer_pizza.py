# -*- coding: utf-8 -*-
"""
gen_sewer_pizza.py (03/11) -- la caja de PIZZA del Sewer, en la paleta del foot blanco (PAL3)
==============================================================================================
Fuente: res/images/lvl_5_freeway/fw_pizza.png (160x48 = 4 cuadros de 40x48: cerrada, abriendo, abierta,
abierta con "ENERGY"; grises y amarillo). El Sewer no tiene PAL0 para eso (marrones y violetas), asi que va
en PAL3 (res/sprites/sewer_mouser.png trae esa paleta): blanco 11, plata 12, gris 13, negro 15, amarillo 7.

Salida: res/images/lvl_3_sewer/sewer_pizza_pal3.png (160x48, indexado) y _to_delete/_pizza_preview.png.
Uso (desde la raiz):  python tools/gen_sewer_pizza.py
"""
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SRC = os.path.join(ROOT, 'res', 'images', 'lvl_5_freeway', 'fw_pizza.png')
PAL = os.path.join(ROOT, 'res', 'sprites', 'sewer_mouser.png')
OUT = os.path.join(ROOT, 'res', 'images', 'lvl_3_sewer', 'sewer_pizza_pal3.png')

# indice de fw_pizza -> indice de PAL3 (0 = transparente)
MAP = {0: 0, 2: 15, 8: 11, 9: 12, 10: 12, 12: 13, 13: 7, 15: 13}


def main():
    src = Image.open(SRC)
    a = np.array(src)
    out = np.zeros_like(a)
    for k, v in MAP.items():
        out[a == k] = v
    im = Image.fromarray(out, 'P')
    pal = Image.open(PAL).getpalette()[:768]
    im.putpalette(pal)
    im.save(OUT)
    print('escrito', OUT, im.size)
    d = os.path.join(ROOT, '_to_delete')
    os.makedirs(d, exist_ok=True)
    rgba = im.convert('RGBA')
    arr = np.array(rgba); arr[:, :, 3] = np.where(out == 0, 0, 255).astype(np.uint8)
    bg = Image.new('RGBA', im.size, (140, 115, 82, 255))
    bg.alpha_composite(Image.fromarray(arr))
    bg.resize((im.size[0] * 4, im.size[1] * 4), Image.NEAREST).save(os.path.join(d, '_pizza_preview.png'))


if __name__ == '__main__':
    main()
