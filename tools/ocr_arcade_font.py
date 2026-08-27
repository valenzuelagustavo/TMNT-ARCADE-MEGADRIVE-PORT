# -*- coding: utf-8 -*-
"""
ocr_arcade_font.py - Transcribe EXACTO el texto de una imagen que fue escrita
con la fuente arcade del proyecto (title_font, 95 tiles de 8x8 en orden ASCII
32..126). No es OCR estadistico: compara cada celda de 8x8 contra los 95 glifos
y devuelve el que coincide 100%.

Se uso para pasar res/images/profiles/*_data.png y *_profile info.png a strings
de C sin depender de leerlos a ojo (la W y la M de esta fuente son casi iguales).

Uso:  python3 tools/ocr_arcade_font.py res/images/profiles/*.png
"""
import sys, os
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, 'res', 'images', 'font', 'font_tmnt_arcade.png')
THR  = 40

fa = np.array(Image.open(FONT).convert('L')) > THR
GLYPHS = {chr(32 + i): fa[:, i * 8:(i + 1) * 8] for i in range(95)}
BLANK  = GLYPHS[' ']

def decode(path):
    a = np.array(Image.open(path).convert('L')) > THR
    rows = np.nonzero(a.any(axis=1))[0]
    if not len(rows): return []
    # agrupar filas contiguas con tinta = renglones
    lines, cur = [], [rows[0]]
    for r in rows[1:]:
        if r <= cur[-1] + 1: cur.append(r)
        else: lines.append((cur[0], cur[-1])); cur = [r]
    lines.append((cur[0], cur[-1]))

    # x de arranque de la grilla: el primer pixel con tinta de toda la imagen
    x0 = int(np.nonzero(a.any(axis=0))[0].min())

    out = []
    for (y0, y1) in lines:
        band = a[y0:y0 + 8]
        text, x = '', x0
        while x + 8 <= a.shape[1]:
            cell = band[:, x:x + 8]
            if cell.shape != (8, 8): break
            best, score = None, -1
            for ch, g in GLYPHS.items():
                m = (g == cell).mean()
                if m > score: best, score = ch, m
            text += best if score == 1.0 else '?'
            x += 8
        out.append(text.rstrip())
    return out

for p in sys.argv[1:]:
    print('--- %s' % os.path.basename(p))
    for l in decode(p):
        print('    "%s"' % l)
