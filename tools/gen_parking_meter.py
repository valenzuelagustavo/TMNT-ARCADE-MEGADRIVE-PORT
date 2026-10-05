#!/usr/bin/env python3
# =============================================================================
# gen_parking_meter.py  -  Parquimetros del nivel 2-1 (25/09)
# =============================================================================
# Entrada: res/sprites/packing_meter.png (asi se llama el archivo original;
#          si se renombra a parking_meter.png tambien lo encuentra)
#          168x64 = 3 frames de 56x64:
#            0  firme en la vereda
#            1  recibe el golpe (se inclina)
#            2  arrancado, volando
#
# Salidas (las que compila rescomp, ver res/props_2_1.res):
#   res/sprites/parking_meter_stand_gen.png  16x64, solo el frame 0 recortado
#   res/sprites/parking_meter_fly_gen.png    112x64, frames 1 y 2
#
# POR QUE DOS SPRITES: SGDK reserva por sprite los tiles de su frame MAS CARO.
# Parado es un palo fino (2 columnas de tiles); volando ocupa casi toda la
# celda de 56x64. Con un solo recurso, cada parquimetro a la vista costaria lo
# del frame volando aunque este quieto, y en el 2-1 con dos tortugas la VRAM de
# sprites va justa. Asi, los que estan quietos cuestan poco y solo el que vuela
# paga el frame grande.
#
# PALETA: los colores del PNG son EXACTAMENTE colores de la paleta del fondo
# del 2-1 (lvl21_pal), pero en otros indices. Se reindexa por color exacto
# contra lvl21_pal, con el negro al indice 15 (el 0 de la paleta tambien es
# negro, pero es el transparente). Asi se dibuja con PAL0 sin gastar linea.
#
# Uso:  python3 tools/gen_parking_meter.py
# =============================================================================
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPR = os.path.join(ROOT, "res", "sprites")
PAL = os.path.join(ROOT, "res", "images", "lvl_2_scene", "genesis", "lvl21_pal.png")
DST_STAND = os.path.join(SPR, "parking_meter_stand_gen.png")
DST_FLY = os.path.join(SPR, "parking_meter_fly_gen.png")

CELL_W, CELL_H = 56, 64
STAND_X0, STAND_W = 18, 16      # recorte del frame 0: el palo queda en x=8


def find_source():
    for name in ("parking_meter.png", "packing_meter.png"):
        p = os.path.join(SPR, name)
        if os.path.exists(p):
            return p
    sys.exit("No encuentro parking_meter.png ni packing_meter.png en res/sprites")


def main():
    src = find_source()
    im = Image.open(src)
    if im.mode != "P":
        sys.exit("El parquimetro tiene que ser indexado")
    a = np.asarray(im).astype(np.uint8)
    if a.shape != (CELL_H, CELL_W * 3):
        sys.exit("Se esperaba 168x64 (3 frames de 56x64), es %dx%d"
                 % (a.shape[1], a.shape[0]))
    tr = im.info.get("transparency", 0)
    if isinstance(tr, bytes):
        tr = 0
    sp = im.getpalette()
    src_rgb = [tuple(sp[i * 3:i * 3 + 3]) for i in range(len(sp) // 3)]

    pim = Image.open(PAL)
    pp = pim.getpalette()
    dst_rgb = [tuple(pp[i * 3:i * 3 + 3]) for i in range(16)]

    lut = np.zeros(256, np.uint8)
    for i in sorted(set(a.flatten().tolist())):
        if i == tr:
            lut[i] = 0
            continue
        c = src_rgb[i]
        cand = [k for k in range(1, 16) if dst_rgb[k] == c]
        if not cand:
            # Por las dudas: cercania (no deberia pasar, son colores del fondo)
            d = [sum((x - y) ** 2 for x, y in zip(c, dst_rgb[k])) for k in range(1, 16)]
            cand = [d.index(min(d)) + 1]
            print("  aviso: color %s no esta en lvl21_pal, va al %d" % (c, cand[0]))
        lut[i] = cand[-1]            # negro -> 15, no 0 (el 0 es transparente)
    b = lut[a]

    flat = [c for rgb in dst_rgb for c in rgb] + [0] * (768 - 48)

    def save(arr, path):
        out = Image.fromarray(arr, "P")
        out.putpalette(flat)
        out.save(path, transparency=0)
        print("%s  %dx%d" % (os.path.basename(path), arr.shape[1], arr.shape[0]))

    stand = b[:, STAND_X0:STAND_X0 + STAND_W]
    if (b[:, :CELL_W] != 0).sum() != (stand != 0).sum():
        sys.exit("El frame 0 no entra en el recorte de %d px" % STAND_W)
    save(stand, DST_STAND)
    save(b[:, CELL_W:CELL_W * 3], DST_FLY)


if __name__ == "__main__":
    main()
