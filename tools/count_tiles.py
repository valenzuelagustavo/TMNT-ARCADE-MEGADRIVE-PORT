# -*- coding: utf-8 -*-
"""
count_tiles.py - Cuenta tiles unicos (con deduplicacion por flips, igual que
rescomp con dedup ALL) de uno o mas PNG. Regla de la casa: MEDIR los tiles
antes de elegir tecnica grafica, porque la VRAM (~1400-1600 tiles segun el
tamano de plano) se agota mucho antes de lo que parece.

Uso:  python3 tools/count_tiles.py res/images/**/*.png
"""
from PIL import Image
import sys, os
def uniq(path, flip=True):
    im=Image.open(path)
    if im.mode!='P': im=im.convert('P', palette=Image.ADAPTIVE, colors=16)
    a=im.tobytes(); w,h=im.size
    px=im.load()
    raw=(w//8)*(h//8); s=set()
    for ty in range(h//8):
        for tx in range(w//8):
            t=tuple(px[tx*8+x, ty*8+y] for y in range(8) for x in range(8))
            if flip:
                th=tuple(t[y*8+(7-x)] for y in range(8) for x in range(8))
                tv=tuple(t[(7-y)*8+x] for y in range(8) for x in range(8))
                tvh=tuple(t[(7-y)*8+(7-x)] for y in range(8) for x in range(8))
                t=min(t,th,tv,tvh)
            s.add(t)
    return raw, len(s), w, h
for p in sys.argv[1:]:
    raw,u,w,h=uniq(p)
    print('%-52s %4dx%-4d raw=%5d uniq=%4d' % (os.path.basename(p), w,h,raw,u))
