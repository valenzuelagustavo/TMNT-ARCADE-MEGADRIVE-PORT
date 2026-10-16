#!/usr/bin/env python3
# =============================================================================
# gen_idle_missile.py (14/10) -- el misil que cae si una tortuga se queda quieta
# =============================================================================
# Modo ARCADE (idle_missile.c): el mismo misil de la cloaca
# (res/sprites/sewer_missil.png, 2 frames de 64x32 mirando a la DERECHA, en
# la paleta de los foot soldiers = PAL2), girado para que caiga de PUNTA:
#   res/sprites/idle_missile.png   2 frames de 32x64, la punta ABAJO
#
# Uso: python3 tools/gen_idle_missile.py
# =============================================================================
import os
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SPR = os.path.join(ROOT, 'res', 'sprites')


def main():
    src = Image.open(os.path.join(SPR, 'sewer_missil.png'))
    assert src.mode == 'P'
    fw, fh = 64, 32
    n = src.width // fw
    out = Image.new('P', (fh * n, fw), 0)
    out.putpalette(src.getpalette())
    for k in range(n):
        f = src.crop((k * fw, 0, (k + 1) * fw, fh))
        out.paste(f.transpose(Image.Transpose.ROTATE_270), (k * fh, 0))   # derecha -> abajo
    out.save(os.path.join(SPR, 'idle_missile.png'), transparency=0)
    print('res/sprites/idle_missile.png', out.size)


if __name__ == '__main__':
    main()
