# -*- coding: utf-8 -*-
"""
gen_sewer_poles.py (03/11) -- el POSTE con pinchos de la trampa del Sewer, a la paleta del foot blanco (PAL3)
================================================================================================
Fuente: res/images/lvl_3_sewer/spiked_pole_start__01583C.png (16x192, RGBA, 4 colores: negro y tres
grises; las ultimas 16 filas estan vacias: el poste termina en la fila 175 con la punta).

Salida:
  res/images/lvl_3_sewer/sewer_pole_pal3.png   16x176 indexado con la paleta del foot blanco (PAL3 durante
                                               el nivel: sprites/sewer_mouser.png la trae), sin las 16 filas
                                               vacias de abajo (2x22 tiles). El 0 es transparente.
  (opcional)  python tools/gen_sewer_poles.py preview  -> _to_delete/_poles_preview.png: los 6 postes
              sobre el fondo, en el lugar del arcade, para compararlo con la captura.

(03/11, 2) Primera version en PAL0 (la del fondo): salia gris azulado, el escenario no tiene blancos.
La paleta del foot blanco (PAL3) si: 11 blanco (255), 12 plata (192), 13 gris (96), 14 gris oscuro (64), 15 negro.
  gris claro 213 -> 11   gris medio 164 -> 12   gris oscuro 106 -> 13   negro 0 -> 15
"""
import os
import sys
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SEW = os.path.join(ROOT, 'res', 'images', 'lvl_3_sewer')
SRC = os.path.join(SEW, 'spiked_pole_start__01583C.png')
OUT = os.path.join(SEW, 'sewer_pole_pal3.png')
PAL = os.path.join(ROOT, 'res', 'sprites', 'sewer_mouser.png')   # la paleta del foot blanco
BG = os.path.join(SEW, 'bg_sewer_md.png')

MAP = {213: 11, 164: 12, 106: 13, 0: 15}
H = 176

# Colocacion de la captura del arcade (ver level3_1.c, SW_POLE_*): centros X de mundo y fila del
# fondo de la punta.
POLE_CX = [746, 757, 769, 780, 791, 805]
POLE_BOT_ROW = [134, 144, 154, 163, 173, 185]


def build():
    a = np.array(Image.open(SRC).convert('RGBA'))[:H]
    out = np.zeros((H, 16), np.uint8)
    for y in range(H):
        for x in range(16):
            r, g, b, al = a[y, x]
            if al == 0:
                continue
            out[y, x] = MAP[int(r)]
    im = Image.fromarray(out, 'P')
    pal = Image.open(PAL).getpalette()[:768]
    im.putpalette(pal)
    im.save(OUT)
    print('escrito', OUT, im.size)
    return im


def preview(im):
    bg = Image.open(BG).convert('RGBA')
    px = np.array(im)
    arr = np.array(im.convert('RGBA'))
    arr[:, :, 3] = np.where(px == 0, 0, 255).astype(np.uint8)
    pole = Image.fromarray(arr)
    for cx, bot in zip(POLE_CX, POLE_BOT_ROW):
        bg.alpha_composite(pole, (cx - 8, bot - H))
    d = os.path.join(ROOT, '_to_delete')
    os.makedirs(d, exist_ok=True)
    out = os.path.join(d, '_poles_preview.png')
    bg.crop((541, 17, 541 + 312, 17 + 224)).resize((936, 672), Image.NEAREST).save(out)
    print('vista previa', out)


if __name__ == '__main__':
    im = build()
    if 'preview' in sys.argv[1:]:
        preview(im)
