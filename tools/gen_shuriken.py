#!/usr/bin/env python3
# =============================================================================
# gen_shuriken.py  (27/09) -- shuriken del foot soldier naranja, 3 frames
# =============================================================================
# El shuriken esta dibujado en tres PNG sueltos de 16x16 (shuriken1..3.png,
# misma paleta que el viejo shuriken.png). Este script los pega en una tira
# horizontal de 48x16 (res/sprites/shuriken_anim.png) para el SPRITE de
# rescomp: una sola animacion de 3 frames que gira en loop.
#
# Uso: python3 tools/gen_shuriken.py
# =============================================================================
import os
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = os.path.join(ROOT, 'res', 'sprites')

frames = [Image.open(os.path.join(D, 'shuriken%d.png' % i)) for i in (1, 2, 3)]
pal = frames[0].getpalette()
for f in frames:
    assert f.mode == 'P' and f.size == (16, 16), 'cada frame: 16x16 indexado'
    assert f.getpalette()[:48] == pal[:48], 'los tres frames tienen que compartir paleta'

strip = Image.new('P', (16 * len(frames), 16), 0)
strip.putpalette(pal)
for i, f in enumerate(frames):
    strip.paste(f, (16 * i, 0))
strip.save(os.path.join(D, 'shuriken_anim.png'))
print('shuriken_anim.png: %d frames' % len(frames))
