#!/usr/bin/env python3
# =============================================================================
# remap_pal1_new.py -- arte de PAL1 pasado a la paleta NUEVA de las tortugas
# =============================================================================
# (09/10) PAL1 pasa a ser la paleta de las hojas nuevas de Leo y Don
# (sprites/players/*_new_13x13.png). Los indices conservan su papel casi
# siempre (verdes, dorados, grises, azul de Leo, rojo), pero el 15 cambia: era
# el VIOLETA de Don y ahora es el NARANJA de Mike (el violeta de Don es el 12).
# Este script mueve el 15 donde hace falta y le pone la paleta nueva al PNG,
# asi se ve igual que en el juego:
#   - HUD (marcos, carteles, digitos, retrato): la parte de Don, 15 -> 12.
#   - HURRY: la flecha violeta, 15 -> 11 (la lila ya pasa al 12).
#   - Hoja vieja de Raph (modo de 4): el panuelo usaba el 15, 15 -> 14 (rojo).
# Correrlo una sola vez: si se vuelve a correr, el 15 ya no esta y no cambia
# nada.
# =============================================================================
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
RES = os.path.join(ROOT, 'res')
NEW = os.path.join(RES, 'sprites', 'players', 'leo_new_13x13.png')

# (archivo, (y0, y1, x0, x1) o None = todo, de, a)
JOBS = [
    ('images/hud/hud_1p.png',              (64, 96, 0, 72),   15, 12),
    ('images/hud/hud_2p.png',              (64, 96, 0, 72),   15, 12),
    ('images/hud/hud_3p.png',              (64, 96, 0, 72),   15, 12),
    ('images/hud/hud_4p.png',              (64, 96, 0, 72),   15, 12),
    ('images/hud/hud4_label.png',          (0, 64, 48, 72),   15, 12),
    ('images/hud/lives_digits_turtles.png', (320, 480, 0, 8), 15, 12),
    ('images/hud/frames_hud.png',          (64, 96, 0, 32),   15, 12),
    ('sprites/hurry_sheet.png',            None,              15, 11),
    ('sprites/raph_anim_13x13.png',        None,              15, 14),
]


def main():
    pal = Image.open(NEW).getpalette()[:48]
    for fn, box, src, dst in JOBS:
        path = os.path.join(RES, fn)
        im = Image.open(path)
        assert im.mode == 'P', fn
        a = np.array(im)
        sub = a if box is None else a[box[0]:box[1], box[2]:box[3]]
        n = int((sub == src).sum())
        sub[sub == src] = dst
        o = Image.fromarray(a, 'P')
        o.putpalette(pal + [0] * (768 - len(pal)))
        if 'transparency' in im.info:
            o.save(path, transparency=im.info['transparency'])
        else:
            o.save(path)
        left = int((a == src).sum())
        print('%-38s %5d px %d -> %d   (quedan %d con %d)' % (fn, n, src, dst, left, src))


if __name__ == '__main__':
    main()
