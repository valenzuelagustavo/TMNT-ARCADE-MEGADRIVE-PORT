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
  3. (29/10) ANIMACION POR PALETA del fondo: los 5 cuadros fundo_01..05 solo cambian en la zona del
     tunel (las "venas" grises titilan: ~2900 px) y un brillo leve del agua. Los pixeles que siguen
     la MISMA secuencia de colores en los 5 cuadros forman una clase; las NCLASES mas grandes
     (tantas como indices de paleta libres) reciben un indice propio en el fondo, y el codigo les
     cambia el color en cada paso (PAL_setColor) -> src/cut_techno_pal.h. Lo que queda (~40%, el
     brillo disperso del agua, cientos de secuencias distintas) se queda fijo como en el cuadro 1.

Uso (desde la raiz del proyecto):  python tools/gen_cut_techno.py
"""
from collections import Counter
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


def merge_tiles(a, cols):
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


def md_word(c):
    r, g, b = (int(v) for v in c)
    return (b << 9) | (g << 5) | (r << 1)


# ---- fondo: unificar colores, clases de animacion por paleta, fusion de tiles ----
FRAMES = 5
fr = []
for i in range(1, FRAMES + 1):
    a, bpal = load('1600_1604_Technodrome fundo_0%d.png' % i)
    assert bpal == fr[0][1] if fr else True
    fr.append((a, bpal))
bgpal = fr[0][1]
mdc = md_colors(bgpal)
n0 = count_tiles(fr[0][0])
b = unify_colors(fr[0][0], bgpal)

# indices libres (duplicados de color ya unificados) -> uno por clase de animacion
free = [i for i in range(1, 16) if i not in set(np.unique(b).tolist())]
seqs = np.stack([mdc[a] for a, _ in fr])                 # (5, H, W, 3)
var = (seqs.max(axis=0) != seqs.min(axis=0)).any(axis=2)
keyimg = {}
cnt = Counter()
for y, x in zip(*np.nonzero(var)):
    k = tuple(map(tuple, seqs[:, y, x]))
    keyimg[(y, x)] = k
    cnt[k] += 1
classes = [k for k, _ in cnt.most_common(len(free))]
print('pixeles que cambian:', int(var.sum()), 'en', len(cnt), 'secuencias; indices libres:', free)
fakecols = mdc.copy()
pal_out = list(bgpal)
anim = []                                                 # (indice, [palabra por cuadro])
covered = 0
for ci, k in enumerate(classes):
    X = free[ci]
    for (y, x), kk in keyimg.items():
        if kk == k:
            b[y, x] = X
    covered += cnt[k]
    fakecols[X] = (100 + 10 * ci,) * 3                    # no se fusiona con nada que no sea igual
    pal_out[X * 3:X * 3 + 3] = [v << 5 for v in k[0]]     # color del cuadro 1
    anim.append((X, [md_word(c) for c in k]))
    print('  clase %d: indice %d, %d px, colores %s' % (ci, X, cnt[k], [tuple(int(v) for v in c) for c in k]))
print('animados por paleta: %d de %d px (%.0f%%)' % (covered, var.sum(), 100.0 * covered / var.sum()))
n1 = count_tiles(b)
c = merge_tiles(b, fakecols)
n2 = count_tiles(c)
print('%-22s tiles %d -> %d (colores/clases) -> %d (tiles parecidos)' % ('cut_techno_bg.png', n0, n1, n2))
save(c, pal_out, 'cut_techno_bg.png', True)
total = n2

# ---- primer plano ----
a, pal = load('1600_1604_Technodrome machine_01.png')
n0 = count_tiles(a)
b2 = unify_colors(a, pal)
n1 = count_tiles(b2)
c2 = merge_tiles(b2, md_colors(pal))
n2 = count_tiles(c2)
print('%-22s tiles %d -> %d (colores iguales) -> %d (tiles parecidos)' % ('cut_techno_fg.png', n0, n1, n2))
save(c2, pal, 'cut_techno_fg.png', False)
total += n2
print('total planos: %d tiles (+16 de sistema +54 de sprites)' % total)

# ---- cabecera para el codigo ----
with open('src/cut_techno_pal.h', 'w') as f:
    f.write('// GENERADO por tools/gen_cut_techno.py: animacion por paleta del fondo del Technodrome.\n')
    f.write('// Clase k: el indice TCH_PAL_IDX[k] de PAL0 toma el color TCH_PAL_ANIM[paso][k] en cada paso.\n')
    f.write('#ifndef CUT_TECHNO_PAL_H\n#define CUT_TECHNO_PAL_H\n')
    f.write('#define TCH_PAL_STEPS %d\n#define TCH_PAL_N     %d\n' % (FRAMES, len(anim)))
    f.write('static const u8 TCH_PAL_IDX[TCH_PAL_N] = { %s };\n' % ', '.join(str(x) for x, _ in anim))
    f.write('static const u16 TCH_PAL_ANIM[TCH_PAL_STEPS][TCH_PAL_N] = {\n')
    for st in range(FRAMES):
        f.write('    { %s },\n' % ', '.join('0x%04X' % w[st] for _, w in anim))
    f.write('};\n#endif\n')

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
