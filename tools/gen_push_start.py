#!/usr/bin/env python3
# =============================================================================
# gen_push_start.py -- cartel "PUSH START" del HUD (09/10)
# =============================================================================
# Reemplaza al "PULSE / START" del marco vacio del P2. Mismo estilo y tamano
# que el "INSERT COIN(S)" del HUD del arcade (medido en gameplayARCADE/
# tmnt.avi, frame 2061): letras de 11 px de alto con trazos de 2 px y sombra
# negra de 1 px a la derecha y abajo, 1 px entre letras. Las letras S, T y R
# estan copiadas del arcade; P, U, H y A no aparecen en el cartel y estan
# dibujadas con la misma construccion.
#
# SALIDA: res/images/hud/push_start.png  144x16 = 2 frames de 72x16 (una
# sola animacion, uno al lado del otro) sobre la paleta de las tortugas (PAL1):
#   frame 0  letras verdes (indice 8)
#   frame 1  letras blancas (indice 13)
# El texto ocupa x 0..63 e y 2..13 de cada frame.
# Uso:  python3 tools/gen_push_start.py
# =============================================================================
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
PAL_SRC = os.path.join(ROOT, 'res', 'sprites', 'players', 'leo_new_13x13.png')
OUT = os.path.join(ROOT, 'res', 'images', 'hud', 'push_start.png')

GLYPHS = {
    'P': ['#####.', '##..##', '##..##', '##..##', '##..##', '#####.',
          '##....', '##....', '##....', '##....', '##....'],
    'U': ['##..##', '##..##', '##..##', '##..##', '##..##', '##..##',
          '##..##', '##..##', '##..##', '######', '.####.'],
    'S': ['.###.', '##.##', '##.##', '##...', '##...', '.##..',          # arcade
          '..##.', '##.##', '##.##', '#####', '.###.'],
    'H': ['##..##', '##..##', '##..##', '##..##', '##..##', '######',
          '######', '##..##', '##..##', '##..##', '##..##'],
    'T': ['######', '..##..', '..##..', '..##..', '..##..', '..##..',      # arcade
          '..##..', '..##..', '..##..', '..##..', '..##..'],
    'A': ['.####.', '##..##', '##..##', '##..##', '##..##', '######',
          '######', '##..##', '##..##', '##..##', '##..##'],
    'R': ['#####.', '##..##', '##..##', '##..##', '##..##', '#####.',      # arcade
          '####..', '##.##.', '##.##.', '##..##', '##..##'],
}
TEXT = 'PUSH START'
SPACE = 3
W, H, TOP = 72, 16, 2
COLORS = (8, 13)          # verde, blanco
SHADOW = 1


def main():
    mask = np.zeros((H, W), bool)
    x = 0
    for ch in TEXT:
        if ch == ' ':
            x += SPACE
            continue
        g = GLYPHS[ch]
        for gy, row in enumerate(g):
            for gx, c in enumerate(row):
                if c == '#':
                    mask[TOP + gy, x + gx] = True
        x += len(g[0]) + 1
    print('ancho del texto: %d px (+1 de sombra)' % (x - 1))
    sh = np.zeros_like(mask)
    sh[:, 1:] |= mask[:, :-1]
    sh[1:, :] |= mask[:-1, :]
    sh[1:, 1:] |= mask[:-1, :-1]
    sh &= ~mask
    out = np.zeros((H, W * 2), np.uint8)
    for f, col in enumerate(COLORS):
        fr = out[:, f * W:(f + 1) * W]
        fr[sh] = SHADOW
        fr[mask] = col
    pal = Image.open(PAL_SRC).getpalette()[:48]
    im = Image.fromarray(out, 'P')
    im.putpalette(pal + [0] * (768 - len(pal)))
    im.save(OUT, transparency=0)
    print(os.path.relpath(OUT, ROOT), out.shape[1], 'x', out.shape[0])


if __name__ == '__main__':
    main()
