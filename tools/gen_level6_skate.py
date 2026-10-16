#!/usr/bin/env python3
# =============================================================================
# gen_level6_skate.py (12/10) -- la Scene 6 en PATINETA, como el arcade
# =============================================================================
# Arma lo que compila rescomp para el nivel de las patinetas (level6_1.c):
#
#   PALETAS. PAL0 = la ruta, PAL1 = las tortugas, PAL2 = los foot soldiers,
#   PAL3 = el helicoptero (su paleta propia) / los blancos (nunca a la vez).
#   Para liberar PAL3 la capa lejana (cielo + skyline) pasa a PAL0: la ruta
#   usa 13 de sus 16 indices (los 9, 13 y 14 estan sin usar, repetidos en
#   gris); ahi van 3 celestes del cielo.
#     bg_skate_p.png        la ruta con esos 3 celestes en la paleta (los
#                           pixeles no cambian)
#     bg_skate_far_p0.png   el cielo con los indices de esa paleta
#
#   EL HELICOPTERO (FootSoldier_helicopter.png de Gustavo, en lvl_5_freeway:
#   3 frames de 148x164, el rotor girando). El cuerpo es el mismo en los 3
#   (el 1 esta corrido 4 px, el 2 tiene el fogonazo), asi que va en DOS
#   sprites dentro del cuadro de 168x168:
#     sk_cop_body.png   112x120  el cuerpo, 1 frame (9 sprites de hardware,
#                                116 tiles) -- se sube UNA vez
#     sk_cop_rotor.png  168x80 x2  el rotor en sus 2 posiciones (5 sprites,
#                                54 tiles) -- los 2 frames quedan cargados
#   Nada se vuelve a subir por DMA mientras vuelan (antes eran 3 franjas de
#   56x168 con 3 frames: ~10 KB por cambio de rotor y 20 sprites).
#     sk_copter_purple.png  16x1: la misma paleta con los rosas pasados a
#                           violeta (los helicopteros violetas del arcade)
#
#   LO QUE TIRA (sacado del video del arcade, cuadros 640-1000 del tramo, el
#   color mayoritario de 117 / 50 / 43 apariciones; en la paleta del
#   helicoptero):
#     sk_bullet.png   16x16  la bala rosa que baja en diagonal
#     sk_flame.png    16x24 x2  la llamita que deja en el piso
#
#   LAS PATINETAS (en PAL1, la paleta de las tortugas, que es fija):
#     sk_board_turtle.png  64x24  la de las tortugas (turtle_skateboard.png,
#                                 sacada del video)
#     sk_board_foot.png    64x24  la de los foot soldiers (foot_with_bomb.png)
#   y la SOMBRA del helicoptero en la ruta (PAL0): sk_shadow.png 48x24.
#
# Correr desde la raiz del proyecto:  python3 tools/gen_level6_skate.py
# =============================================================================
import os
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, 'res', 'images', 'lvl_6_skate')
D5 = os.path.join(ROOT, 'res', 'images', 'lvl_5_freeway')
SPR = os.path.join(ROOT, 'res', 'sprites')

SKY = [(0, 165, 255), (0, 140, 230), (0, 99, 140)]   # -> indices 9, 13, 14
SKY_IDX = [9, 13, 14]


def pal_of(im, n=16):
    p = im.getpalette()[:n * 3]
    return [tuple(p[i * 3:i * 3 + 3]) for i in range(n)]


def nearest(c, pal, cand):
    return min(cand, key=lambda j: sum((u - v) ** 2 for u, v in zip(pal[j], c)))


def save_p(arr, pal, path):
    im = Image.fromarray(arr.astype(np.uint8), 'P')
    flat = [v for c in pal for v in c]
    im.putpalette(flat + [0] * (768 - len(flat)))
    im.save(path, transparency=0)


def remap_rgba(rgba, pal, cand, force=None):
    h, w = rgba.shape[:2]
    out = np.zeros((h, w), np.uint8)
    cache = dict(force or {})
    for y in range(h):
        for x in range(w):
            r, g, b, a = rgba[y, x]
            if a == 0:
                continue
            c = (int(r), int(g), int(b))
            if c not in cache:
                cache[c] = nearest(c, pal, cand)
            out[y, x] = cache[c]
    return out


ROTOR_IDX = [15, 2]             # colores del rotor (blanco y gris)
COP_BODY = (8, 48, 112, 120)    # x, y, w, h dentro del cuadro de 168x168
COP_ROTOR = (-16, 0, 168, 80)   # (13/10) arranca 16 px antes del cuadro


def erode(m, r):
    out = m.copy()
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            out &= np.roll(np.roll(m, dy, 0), dx, 1)
    return out


def dilate(m, r):
    out = m.copy()
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            out |= np.roll(np.roll(m, dy, 0), dx, 1)
    return out


def crop(arr):
    ys, xs = np.nonzero(arr)
    return arr[ys.min():ys.max() + 1, xs.min():xs.max() + 1]


def place_bottom(arr, w, h):
    out = np.zeros((h, w), np.uint8)
    ah, aw = arr.shape
    out[h - ah:, (w - aw) // 2:(w - aw) // 2 + aw] = arr
    return out


# Video (RGB, sin alfa) -> RGBA con el negro del borde intacto
BULLET = [
    "....pppp.",
]


def main():
    # --- Ruta y cielo -----------------------------------------------------
    road = Image.open(os.path.join(D, 'bg_skate.png'))
    ra = np.array(road)
    for i in SKY_IDX:
        assert not (ra == i).any(), 'el indice %d de la ruta esta en uso' % i
    rpal = pal_of(road)
    for i, c in zip(SKY_IDX, SKY):
        rpal[i] = c
    save_p(ra, rpal, os.path.join(D, 'bg_skate_p.png'))
    far = Image.open(os.path.join(D, 'bg_skate_far.png'))
    fa = np.array(far)
    fpal = pal_of(far)
    cand = [1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12] + SKY_IDX     # sin el 0 (transparente) ni el 15 repetido
    lut = [0] + [nearest(fpal[i], rpal, cand) for i in range(1, 16)]
    save_p(np.array(lut, np.uint8)[fa], rpal, os.path.join(D, 'bg_skate_far_p0.png'))
    print('cielo:', lut)

    # --- Helicoptero ---------------------------------------------------------
    cop = Image.open(os.path.join(D5, 'FootSoldier_helicopter.png'))
    cpal = pal_of(cop)
    ca = np.array(cop)
    FW, FH = 148, 164
    S = 168
    L = 16                       # margen a la izquierda del cuadro (ver abajo)
    # (13/10) La hoja NO es una grilla pareja de 148: el rotor del frame 1
    # arranca 12 px antes de su celda (ese pedazo de arco quedaba pegado a la
    # derecha del frame 0 y le faltaba al 1: "se corta y aparece del otro
    # lado"). Cortes: frame 0 = x 0..135, frame 1 = 136..283, frame 2 = 284..
    # Los cuadros se arman en un lienzo con L px de margen a la izquierda.
    CUT = [0, FW - 12, 2 * FW - 12, 3 * FW]
    fr = []
    for f in range(3):
        a = np.zeros((S, L + S), np.uint8)
        x0 = L + 4 + (CUT[f] - f * FW)
        a[S - FH:, x0:x0 + (CUT[f + 1] - CUT[f])] = ca[:, CUT[f]:CUT[f + 1]]
        fr.append(a)
    # El frame 1 es el 0 corrido (-4, +3) con el rotor en otra posicion; el
    # 2 es el 0 con el fogonazo del arma. El cuerpo es siempre el mismo.
    f1 = np.roll(np.roll(fr[1], 3, 0), -4, 1)
    f0 = fr[0]
    up = (np.arange(S)[:, None] < 80)                # el rotor no baja de ahi
    core = (f0 != 0) & ~np.isin(f0, ROTOR_IDX)
    bm = dilate(core, 2) | ~up                       # el cuerpo + 2 px
    body = np.where(bm, f0, 0)
    rot = [np.where(~bm & up & np.isin(x, ROTOR_IDX), x, 0) for x in (f0, f1)]
    BX, BY, BW, BH = COP_BODY
    RX, RY, RW, RH = COP_ROTOR
    assert not body[:, :L + BX].any() and not body[:, L + BX + BW:].any()
    save_p(body[BY:BY + BH, L + BX:L + BX + BW], cpal, os.path.join(D, 'sk_cop_body.png'))
    rr = np.zeros((RH, RW * 2), np.uint8)
    for k in range(2):
        assert not rot[k][:, :L + RX].any() and not rot[k][:, L + RX + RW:].any()
        rr[:, k * RW:(k + 1) * RW] = rot[k][RY:RY + RH, L + RX:L + RX + RW]
    save_p(rr, cpal, os.path.join(D, 'sk_cop_rotor.png'))
    # violeta: los rosas con R y B cambiados
    ppal = []
    for (r, g, b) in cpal:
        ppal.append((b, g, r) if (r > 120 and r > b + 40 and g < r) else (r, g, b))
    save_p(np.zeros((1, 16), np.uint8) + np.arange(16, dtype=np.uint8), ppal,
           os.path.join(D, 'sk_copter_purple.png'))

    # --- Bala y llama (rips del video, RGBA) ---------------------------------
    ccand = list(range(2, 16))
    for src, dst, (cw, ch), nfr in (('rip_bullet.png', 'sk_bullet.png', (16, 16), 1),
                                    ('rip_flame.png', 'sk_flame.png', (16, 24), 2)):
        im = np.array(Image.open(os.path.join(D, src)).convert('RGBA'))
        idx = remap_rgba(im, cpal, ccand, force={(0, 0, 0): 1})
        # rip_flame.png trae los 2 frames uno al lado del otro, separados
        lab, n = ndimage.label(idx != 0)
        objs = sorted(ndimage.find_objects(lab), key=lambda s: s[1].start)
        out = np.zeros((ch, cw * nfr), np.uint8)
        for k in range(nfr):
            sub = idx[objs[k]].copy()
            out[:, k * cw:(k + 1) * cw] = place_bottom(sub, cw, ch)
        save_p(out, cpal, os.path.join(D, dst))

    # --- Patinetas (PAL1) ----------------------------------------------------
    tpal = pal_of(Image.open(os.path.join(SPR, 'players', 'leo_new_13x13.png')))
    tcand = list(range(2, 16))
    tb = np.array(Image.open(os.path.join(D, 'turtle_skateboard.png')).convert('RGBA'))
    tbi = np.array(Image.open(os.path.join(D, 'turtle_skateboard.png')))
    tb[tbi == 0, 3] = 0
    idx = remap_rgba(tb, tpal, tcand, force={(0, 0, 0): 1})
    save_p(place_bottom(crop(idx) if idx.any() else idx, 64, 24), tpal,
           os.path.join(D, 'sk_board_turtle.png'))
    fb = np.array(Image.open(os.path.join(D5, 'foot_with_bomb.png')).convert('RGBA'))
    sub = fb[135:170, 25:95]
    m = sub[..., 3] > 0
    lab, n = ndimage.label(m)
    big = 1 + int(np.argmax(ndimage.sum(m, lab, range(1, n + 1))))
    sub[lab != big, 3] = 0
    sub[(sub[..., 0] == 255) & (sub[..., 1] == 0) & (sub[..., 2] == 0), 3] = 0   # sombra roja del rip
    idx = remap_rgba(sub, tpal, tcand, force={(0, 0, 0): 1})
    save_p(place_bottom(crop(idx), 64, 24), tpal, os.path.join(D, 'sk_board_foot.png'))

    # --- Sombra del helicoptero (PAL0, gris oscuro de la ruta) ---------------
    dark = rpal.index((99, 99, 99))
    sh = np.zeros((24, 48), np.uint8)
    sh[9:15, 2:46] = dark          # las aspas/el cuerpo
    sh[4:20, 18:30] = dark          # la cabina
    sh[10:14, 30:46] = dark         # la cola
    sh[6:18, 40:44] = dark
    save_p(sh, rpal, os.path.join(D, 'sk_shadow.png'))
    print('listo')


if __name__ == '__main__':
    main()
