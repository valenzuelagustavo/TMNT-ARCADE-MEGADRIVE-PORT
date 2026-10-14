#!/usr/bin/env python3
# =============================================================================
# gen_konami_logo.py -- logo animado de KONAMI (14/10)
# =============================================================================
# FUENTE: res/images/konami_logo/konami_logo_frames.png, la hoja de capturas
# que paso Gustavo: 41 cuadros de 256x224 (pantalla H32) sobre fondo blanco,
# en orden de lectura. Lo que muestran:
#    0       negro
#    1..6    una estrella cruza de izquierda a derecha en y = 23 dejando una
#            linea fina rosa (40 px por cuadro)
#    7..26   la linea se hace gruesa (14 px) y baja 10 px por cuadro; arriba
#            de ella va apareciendo el logo DIBUJADO EN AZUL (solo contornos)
#   27       el logo azul entero
#   28..31   el logo se pinta (dorado arriba, rojo abajo, KONAMI plateado):
#            es la MISMA imagen con otra paleta, y el degrade de cada pieza
#            gira (ciclo de paleta) hasta quedar quieto
#   32..38   el fondo pasa de negro a gris claro
#   39..40   todo se oscurece hasta negro
#
# Como el azul y el color son la misma imagen con otra paleta, cada pixel del
# logo es un INDICE: se arma con los colores que tiene en los cuadros 27..31.
#
# SALIDAS
#   res/images/konami_logo/konami_logo_a.png   256x224, BG_A: la pieza de
#       arriba (dorada), PAL1
#   res/images/konami_logo/konami_logo_b.png   256x224, BG_B: la pieza de
#       abajo (roja), PAL2, y KONAMI, PAL0
#       (las dos de 64 colores: el indice / 16 es la linea de paleta del tile)
#   res/images/konami_logo/konami_star.png     4 frames de 16x16 (PAL3)
#   res/images/konami_logo/konami_trail.png    32x8: la linea fina (PAL3)
#   res/images/konami_logo/konami_band.png     32x16: la linea gruesa (PAL3)
#   src/konami_logo_pal.h                      paletas y ciclos (formato VDP)
#
# Uso:  python3 tools/gen_konami_logo.py
# =============================================================================
import os
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
DIR = os.path.join(ROOT, 'res', 'images', 'konami_logo')
SRC = os.path.join(DIR, 'konami_logo_frames.png')
HDR = os.path.join(ROOT, 'src', 'konami_logo_pal.h')
W, H = 256, 224
TEXT_Y = 170                 # KONAMI y la (R): de aca para abajo

# Ciclos de color (orden del degrade). Cada pieza tiene 10 indices que giran.
# Medidos: en el cuadro 30 el degrade esta en su lugar (corrimiento 0); en el
# 29 corrido 1 (arriba) y 2 (abajo); en el 28, 6 y 7.
CYC_TOP = [(176, 80, 0), (192, 104, 0), (200, 120, 0), (208, 136, 0),
           (208, 152, 0), (216, 160, 0), (224, 176, 0), (232, 192, 0),
           (240, 208, 0), (248, 224, 0)]
CYC_BOT = [(112, 8, 16), (136, 0, 8), (152, 0, 8), (168, 0, 0), (184, 0, 0),
           (200, 0, 0), (216, 0, 0), (232, 64, 0), (240, 96, 0), (248, 128, 0)]
# KONAMI: 11 colores para 10 indices (el (104,104,128) no queda en ningun
# pixel con el ciclo quieto). En el cuadro 30 corrido 5, en el 31 quieto (0)
# y del 32 en adelante quieto en 7.
CYC_TXT = [(40, 40, 40), (40, 40, 64), (64, 64, 88), (80, 80, 104),
           (88, 88, 112), (104, 104, 128), (120, 120, 144), (128, 128, 152),
           (144, 144, 168), (160, 160, 184), (176, 176, 200)]
# Colores fijos (los biseles) de la pieza de arriba; lo demas es de abajo.
TOP_STILL = [(168, 88, 8), (200, 120, 0), (216, 136, 0), (240, 152, 0)]
CHECKS = {'top': (CYC_TOP, {1: 6, 2: 1, 3: 0}),        # cuadro 28/29/30
          'bot': (CYC_BOT, {1: 7, 2: 2, 3: 0}),
          'txt': (CYC_TXT, {3: 5})}


def vdp(c):
    r, g, b = c
    return ((b >> 5) << 9) | ((g >> 5) << 5) | ((r >> 5) << 1)


def frames():
    a = np.array(Image.open(SRC).convert('RGBA')).astype(int)
    m = (a[..., 3] > 0) & ~((a[..., :3] > 245).all(2))
    lab, _ = ndimage.label(m)
    boxes = []
    for sl in ndimage.find_objects(lab):
        if sl[0].stop - sl[0].start >= 200 and sl[1].stop - sl[1].start >= 250:
            boxes.append((sl[0].start, sl[1].start))
    boxes.sort(key=lambda b: (round(b[0] / 229), b[1]))
    assert len(boxes) == 41, len(boxes)
    rgb = a[..., :3]
    return [rgb[y:y + H, x:x + W] for y, x in boxes]


def classes(F):
    """Cada pixel del logo -> (grupo, clave). La clave son sus colores en los
    cuadros 27..31."""
    st = np.stack([F[k] for k in (27, 28, 29, 30, 31)], 0)
    fin = F[31]
    groups = {'top': {}, 'bot': {}, 'txt': {}}
    pix = {}
    for y, x in zip(*np.nonzero((fin != 0).any(2))):
        key = tuple(tuple(int(v) for v in st[i, y, x]) for i in range(5))
        if y >= TEXT_Y:
            g = 'txt'
        elif key[4] in CYC_TOP or key[4] in TOP_STILL:
            g = 'top'
        else:
            g = 'bot'
        groups[g].setdefault(key, 0)
        groups[g][key] += 1
        pix[(y, x)] = (g, key)
    return groups, pix


def layout(g, keys):
    """-> lista de claves en el orden de los indices 1..n: primero las que
    giran (en el orden del ciclo), despues las fijas."""
    cyc = CHECKS[g][0]
    moving = [k for k in keys if k[3] != k[4] or k[1] != k[4]]
    still = [k for k in keys if k not in moving]
    if g == 'txt':
        moving = [k for k in keys if k[4] in cyc and k[3] != k[4]]
        still = [k for k in keys if k not in moving]
    moving.sort(key=lambda k: cyc.index(k[4]))
    for k in moving:                                  # el ciclo cierra
        i = cyc.index(k[4])
        for col, sh in CHECKS[g][1].items():
            assert k[col] == cyc[(i + sh) % len(cyc)], (g, k, col)
    out = moving + sorted(still, key=lambda k: k[4])
    assert len(out) <= 15, (g, len(out))
    return out, [cyc.index(k[4]) for k in moving]


def save64(path, arr, pal64):
    im = Image.fromarray(arr.astype(np.uint8), 'P')
    flat = []
    for c in pal64:
        flat += list(c)
    im.putpalette(flat + [0] * (768 - len(flat)))
    im.save(path, transparency=0)
    print('%-44s %dx%d' % (os.path.relpath(path, ROOT), arr.shape[1], arr.shape[0]))


def fx_sprites(F, fxpal):
    def idx(c):
        c = tuple(int(v) for v in c)
        if c == (0, 0, 0):
            return 0
        if c not in fxpal:
            fxpal.append(c)
        return fxpal.index(c) + 1

    def conv(block):
        return np.vectorize(lambda r, g, b: idx((r, g, b)))(block[..., 0], block[..., 1], block[..., 2])

    # linea gruesa: filas 48..63 del cuadro 10 (14 filas de color + 2 negras)
    band = conv(F[10][48:64, 0:32])
    # linea fina: cuadro 6, filas 19..26
    trail = conv(F[6][19:27, 0:32])
    # estrella: cuadros 1..4 (en el 5 ya es otra vez una de las anteriores),
    # 16x16 centrada en la columna mas alta de la cabeza
    stars = []
    for k in (1, 2, 3, 5):
        f = F[k]
        rows = f[8:40]
        lit = (rows != 0).any(2)
        h = lit.sum(0)
        cx = int(np.argmax(h))
        x0 = max(0, cx - 8)
        blk = f[15:31, x0:x0 + 16]
        stars.append(conv(blk))
    return band, trail, np.hstack(stars)


def main():
    F = frames()
    groups, pix = classes(F)
    lay = {g: layout(g, groups[g].keys()) for g in groups}
    line = {'txt': 0, 'top': 1, 'bot': 2}
    A = np.zeros((H, W), np.uint8)
    B = np.zeros((H, W), np.uint8)
    pal64 = [(0, 0, 0)] * 64
    for g, (keys, _) in lay.items():
        base = line[g] * 16
        for i, k in enumerate(keys):
            pal64[base + 1 + i] = k[4]
    for (y, x), (g, k) in pix.items():
        v = line[g] * 16 + 1 + lay[g][0].index(k)
        (A if g == 'top' else B)[y, x] = v
    # cada tile de B tiene que ser de una sola linea de paleta
    for ty in range(H // 8):
        for tx in range(W // 8):
            t = B[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8]
            assert len(set((t[t != 0] // 16).tolist())) <= 1, (tx, ty)
    save64(os.path.join(DIR, 'konami_logo_a.png'), A, pal64)
    save64(os.path.join(DIR, 'konami_logo_b.png'), B, pal64)

    fxpal = []
    band, trail, stars = fx_sprites(F, fxpal)
    assert len(fxpal) <= 14, len(fxpal)      # el 15 queda para el fondo
    fx16 = [(0, 0, 0)] + fxpal + [(0, 0, 0)] * (15 - len(fxpal))
    for name, arr in (('konami_band.png', band), ('konami_trail.png', trail),
                      ('konami_star.png', stars)):
        p = os.path.join(DIR, name)
        save64(p, arr, fx16)

    # --- paletas en formato VDP ---
    def arr16(cols):
        return ', '.join('0x%03X' % vdp(c) for c in cols)

    out = ['// GENERADO POR tools/gen_konami_logo.py -- NO EDITAR A MANO',
           '#ifndef _KONAMI_LOGO_PAL_H_', '#define _KONAMI_LOGO_PAL_H_', '',
           '// Linea de paleta: 0 = KONAMI, 1 = pieza de arriba, 2 = pieza de',
           '// abajo, 3 = linea, estrella y (indice 15) el color del fondo.',
           '// Los indices 1..KL_*_MOVE de cada linea giran: indice i toma el',
           '// color ciclo[(base[i] + corrimiento) % largo].']
    names = {'txt': 'TXT', 'top': 'TOP', 'bot': 'BOT'}
    for g in ('txt', 'top', 'bot'):
        keys, bases = lay[g]
        cyc = CHECKS[g][0]
        N = names[g]
        blue = [(0, 0, 0)] + [k[0] for k in keys] + [(0, 0, 0)] * (15 - len(keys))
        final = [(0, 0, 0)] + [k[4] for k in keys] + [(0, 0, 0)] * (15 - len(keys))
        out += ['', '#define KL_%s_MOVE  %d' % (N, len(bases)),
                '#define KL_%s_CYCLE %d' % (N, len(cyc)),
                '// en azul (el logo dibujado, cuadro 27)',
                'static const u16 klPal%sBlue[16] = { %s };' % (N.title(), arr16(blue)),
                '// pintado y quieto (cuadro 31)',
                'static const u16 klPal%sColor[16] = { %s };' % (N.title(), arr16(final)),
                'static const u16 klCyc%s[%d] = { %s };' % (N.title(), len(cyc), arr16(cyc)),
                'static const u8 klBase%s[%d] = { %s };' % (N.title(), len(bases),
                                                         ', '.join(str(b) for b in bases))]
    out += ['', '// linea y estrella (el 15 es el fondo, lo pone el codigo)',
            'static const u16 klPalFx[16] = { %s };' % arr16(fx16), '', '#endif', '']
    open(HDR, 'w').write('\n'.join(out))
    print(os.path.relpath(HDR, ROOT))
    for g in lay:
        print(g, 'indices', len(lay[g][0]), 'giran', len(lay[g][1]))
    print('fx colores', len(fxpal))


if __name__ == '__main__':
    main()
