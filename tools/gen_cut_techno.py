#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_cut_techno.py (29/10) - cutscene antes del Technodrome (SCENE_CUT_TECHNODROME)
=================================================================================
Fuentes (res/images/lvl_8_techno/1600_1604/):
  1600_1604_Technodrome fundo_01.png    fondo (BG_B, PAL0)
  1600_1604_Technodrome machine_01.png  primer plano: el Technodrome (BG_A, PAL1, indice 0 transparente)
  1600_1604_turtles_04.png              Leo + globo "There it is" (sprites, PAL2)
Salidas (res/images/lvl_8_techno/):
  cut_techno_bg.png      fondo; paleta[0] = negro (es el color del backdrop)
  cut_techno_fg.png      primer plano
  cut_techno_leo.png     48x40 en (194, 184)
  cut_techno_bubble.png  64x32 en (178, 152)
Ahorro de tiles (la VRAM de la escena queda justa: 891 + 467 tiles de los dos planos mas 62 de
sprites contra ~1424 disponibles):
  1. los indices de paleta que dan el MISMO color de MegaDrive (3 bits por canal) se unifican;
  2. los tiles que difieren en <= MAXPX pixeles y como mucho 1 nivel de color de la MegaDrive
     (invisible) se reemplazan por el parecido ya existente (tambien espejado), asi rescomp
     los deduplica.
Uso (desde la raiz del proyecto):  python tools/gen_cut_techno.py
"""
import numpy as np
from PIL import Image

S = 'res/images/lvl_8_techno/1600_1604/'
D = 'res/images/lvl_8_techno/'
MAXLEVEL = 1
MAXPX = 4


def md_colors(pal):
    return np.array([[v >> 5 for v in pal[i:i + 3]] for i in range(0, 48, 3)], dtype=np.int16)


def load(name):
    im = Image.open(S + name)
    assert im.mode == 'P'
    return np.array(im), im.getpalette()[:48]


def unify_colors(a, pal):
    cols = [tuple(c) for c in md_colors(pal)]
    first = {}
    remap = np.arange(16, dtype=np.uint8)
    for i, c in enumerate(cols):
        if i == 0:
            continue
        if c in first and c != cols[0]:
            remap[i] = first[c]
        else:
            first[c] = i
    return remap[a]


def variants(t):
    return (t, t[:, ::-1], t[::-1], t[::-1, ::-1])


def merge_tiles(a, pal):
    cols = md_colors(pal)
    H, W = a.shape
    kept_i = []           # tiles de indices (8,8)
    kept_c = []           # colores (8,8,3)
    out = a.copy()
    stack_c = np.zeros((0, 8, 8, 3), np.int16)
    stack_z = np.zeros((0, 8, 8), bool)
    for y in range(0, H, 8):
        for x in range(0, W, 8):
            t = a[y:y + 8, x:x + 8]
            tc = cols[t]
            tz = (t == 0)
            found = None
            if len(kept_i):
                for vi in range(4):
                    # variante vi de TODOS los tiles guardados
                    if vi == 0:
                        kc, kz = stack_c, stack_z
                    elif vi == 1:
                        kc, kz = stack_c[:, :, ::-1], stack_z[:, :, ::-1]
                    elif vi == 2:
                        kc, kz = stack_c[:, ::-1], stack_z[:, ::-1]
                    else:
                        kc, kz = stack_c[:, ::-1, ::-1], stack_z[:, ::-1, ::-1]
                    d = np.abs(kc - tc[None]).max(axis=3)
                    ok = ((kz == tz[None]).all(axis=(1, 2)) & (d.max(axis=(1, 2)) <= MAXLEVEL)
                          & ((d != 0).sum(axis=(1, 2)) <= MAXPX))
                    idx = np.nonzero(ok)[0]
                    if len(idx):
                        found = variants(kept_i[idx[0]])[vi]
                        break
            if found is not None:
                out[y:y + 8, x:x + 8] = found
            else:
                kept_i.append(t.copy())
                stack_c = np.concatenate([stack_c, tc[None]], axis=0)
                stack_z = np.concatenate([stack_z, tz[None]], axis=0)
    return out


def count_tiles(a):
    H, W = a.shape
    s = set()
    for y in range(0, H, 8):
        for x in range(0, W, 8):
            t = a[y:y + 8, x:x + 8]
            s.add(min(v.tobytes() for v in variants(t)))
    return len(s)


def save(a, pal, name, black0=False):
    p = list(pal) + [0] * (768 - len(pal))
    if black0:
        p[0:3] = [0, 0, 0]
    o = Image.fromarray(a.astype(np.uint8), 'P')
    o.putpalette(p)
    o.save(D + name, transparency=0)


total = 0
for src, dst, black0 in (('1600_1604_Technodrome fundo_01.png', 'cut_techno_bg.png', True),
                         ('1600_1604_Technodrome machine_01.png', 'cut_techno_fg.png', False)):
    a, pal = load(src)
    n0 = count_tiles(a)
    b = unify_colors(a, pal)
    n1 = count_tiles(b)
    c = merge_tiles(b, pal)
    n2 = count_tiles(c)
    changed = int((c != b).sum())
    print('%-22s tiles %d -> %d (colores iguales) -> %d (tiles parecidos, %d px tocados)' % (dst, n0, n1, n2, changed))
    save(c, pal, dst, black0)
    total += n2
print('total planos: %d tiles (+16 de sistema +62 de sprites)' % total)

# Leo y el globo (turtles_04 los trae juntos; el globo no toca a Leo)
t, tpal = load('1600_1604_turtles_04.png')
assert t.shape == (224, 320)
LX, LY, LW, LH = 194, 184, 48, 40
BX, BY, BW, BH = 178, 152, 64, 32
leo = t[LY:LY + LH, LX:LX + LW]
bub = t[BY:BY + BH, BX:BX + BW]
mask = np.zeros_like(t, bool)
mask[LY:LY + LH, LX:LX + LW] = True
mask[BY:BY + BH, BX:BX + BW] = True
assert not (t[~mask] != 0).any(), 'hay pixeles fuera de Leo y el globo'
save(leo, tpal, 'cut_techno_leo.png')
save(bub, tpal, 'cut_techno_bubble.png')
print('leo', LX, LY, 'globo', BX, BY)
