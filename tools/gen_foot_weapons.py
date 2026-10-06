# -*- coding: utf-8 -*-
"""
gen_foot_weapons.py -- arma las hojas de los foot soldiers con ARMA (06/10)

Fuentes (provisorias, hasta que lleguen las definitivas):
    res/sprites/foot_gun.png      el naranja del FUSIL
    res/sprites/foot_hammer.png   el morado del MARTILLO
    res/sprites/foot_spear.png    el morado de la LANZA

Esas hojas no son una grilla: los frames estan sueltos, mirando a la
IZQUIERDA y con el arma saliendo de la celda. Este script:
  1. separa los frames (componentes conexas, con los dos pegados del fusil y
     los tres del martillo partidos a mano por la columna mas vacia);
  2. los ancla por el CUERPO (mediana de los pixeles del traje) y los pies;
  3. los espeja para que miren a la DERECHA, como el resto de los enemigos;
  4. los pega en una grilla pareja, una fila por animacion, con el cuerpo
     centrado en la celda y los pies en el borde de abajo;
  5. les pone la paleta UNICA de enemigos (la del morado, PAL2): el orden de
     indices es el mismo, solo cambian un poco los colores.

Salidas (lo que compila enemies.res):
    res/sprites/foot_gun_gen.png     res/sprites/foot_gun_fx.png
    res/sprites/foot_hammer_gen.png
    res/sprites/foot_spear_gen.png   res/sprites/foot_spear_fx.png

Las FILAS de cada hoja tienen que coincidir con GUN_ANIM_* / HAMMER_ANIM_* /
SPEAR_ANIM_* de src/enemy.h. Cuando lleguen las hojas definitivas en grilla,
se declaran directo en enemies.res y este script deja de hacer falta.

Uso:  python tools/gen_foot_weapons.py
"""
import os
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SPR = os.path.join(ROOT, 'res', 'sprites')
PAL_REF = os.path.join(SPR, 'foot_soldier_purple_13x13.png')
SUIT = (1, 2, 3, 7, 9, 10, 11)   # indices del traje (morado / naranja)


def segment(path, gap=2):
    """Frames sueltos de la hoja: lista de (mask, x0, y0) en orden de lectura."""
    im = Image.open(path)
    a = np.array(im)
    mask = a != 0
    st = np.ones((2 * gap + 1, 2 * gap + 1), bool)
    lab, n = ndimage.label(ndimage.binary_dilation(mask, structure=st))
    comps = []
    for i, s in enumerate(ndimage.find_objects(lab)):
        ys, xs = s
        sub = mask[ys, xs] & (lab[ys, xs] == i + 1)
        if sub.sum() < 20:
            continue
        yy, xx = np.nonzero(sub)
        x0, y0 = xs.start + xx.min(), ys.start + yy.min()
        x1, y1 = xs.start + xx.max() + 1, ys.start + yy.max() + 1
        comps.append((x0, y0, x1, y1, sub[yy.min():yy.max() + 1, xx.min():xx.max() + 1]))
    comps.sort(key=lambda c: (c[3] // 40, c[0]))
    return a, comps


def split(comp, parts):
    """Parte un componente con 'parts' frames pegados por las columnas con
    menos pixeles cerca de cada division pareja."""
    x0, y0, x1, y1, m = comp
    w = m.shape[1]
    dens = m.sum(axis=0)
    cuts = [0]
    for k in range(1, parts):
        c = w * k // parts
        lo, hi = max(1, c - 14), min(w - 1, c + 14)
        cuts.append(lo + int(np.argmin(dens[lo:hi])))
    cuts.append(w)
    out = []
    for k in range(parts):
        sub = np.zeros_like(m)
        sub[:, cuts[k]:cuts[k + 1]] = m[:, cuts[k]:cuts[k + 1]]
        yy, xx = np.nonzero(sub)
        out.append((x0 + xx.min(), y0 + yy.min(), x0 + xx.max() + 1, y0 + yy.max() + 1,
                    sub[yy.min():yy.max() + 1, xx.min():xx.max() + 1]))
    return out


def frames_of(path, splits):
    """Diccionario clave -> (pixeles, ancla_x, ancla_y) ya ESPEJADO (mira a la
    derecha). Clave = indice del componente, o (indice, parte) si se partio."""
    a, comps = segment(path)
    res = {}
    for i, c in enumerate(comps):
        pieces = [(i, c)] if i not in splits else \
                 [((i, k), p) for k, p in enumerate(split(c, splits[i]))]
        for key, (x0, y0, x1, y1, m) in pieces:
            px = np.where(m, a[y0:y1, x0:x1], 0).astype(np.uint8)
            # ancla en X: la mediana de los pixeles del TRAJE (morados 1-3,
            # naranjas y rojos 7, 9-11), sin las ultimas filas (la sombra) ni
            # el arma (grises y dorados): asi el cuerpo no baila de un frame
            # a otro aunque el arma cambie de lado.
            body = np.isin(px, SUIT) & m
            body[-5:, :] = False
            yy, xx = np.nonzero(body)
            if len(xx) < 10:
                yy, xx = np.nonzero(m[-4:, :])
            ax = float(np.median(xx))
            px = px[:, ::-1]                      # espejo: mira a la derecha
            ax = (px.shape[1] - 1) - ax
            res[key] = (px, ax, px.shape[0])
    return res


def build(name, src, splits, rows, extra=None, fixed_w=None):
    fr = frames_of(os.path.join(SPR, src), splits)
    if extra:
        fr.update(extra)
    used = [k for r in rows for k in r[1]]
    half = 0
    up = 0
    for k in used:
        px, ax, ay = fr[k]
        half = max(half, ax + 1, px.shape[1] - ax)
        up = max(up, ay)
    W = fixed_w or int(-(-int(np.ceil(half * 2)) // 8) * 8)
    H = int(-(-up // 8) * 8)
    cols = max(len(r[1]) for r in rows)
    sheet = np.zeros((H * len(rows), W * cols), np.uint8)
    for r, (label, keys) in enumerate(rows):
        for c, k in enumerate(keys):
            px, ax, ay = fr[k]
            ox = int(round(W / 2 - ax))
            oy = H - ay
            h, w = px.shape
            # recorte si el arma se sale de la celda
            sx0 = max(0, -ox); sx1 = min(w, W - ox)
            sy0 = max(0, -oy)
            dst = sheet[r * H + oy + sy0: r * H + oy + h, c * W + ox + sx0: c * W + ox + sx1]
            srcp = px[sy0:h, sx0:sx1]
            dst[srcp != 0] = srcp[srcp != 0]
    out = Image.fromarray(sheet, 'P')
    out.putpalette(Image.open(PAL_REF).getpalette()[:48] + [0] * (768 - 48))
    out.save(os.path.join(SPR, name), transparency=0)
    print('%-22s celda %dx%d (%dx%d tiles), %d filas x %d' %
          (name, W, H, W // 8, H // 8, len(rows), cols))
    for r, (label, keys) in enumerate(rows):
        print('   [%d] %-10s %d frames' % (r, label, len(keys)))
    return fr


def fx_sheet(name, cells, cw, ch):
    """Hoja de proyectiles: cells = filas de listas de (pixeles) centrados."""
    cols = max(len(r) for r in cells)
    sheet = np.zeros((ch * len(cells), cw * cols), np.uint8)
    for r, row in enumerate(cells):
        for c, px in enumerate(row):
            h, w = px.shape
            ox, oy = (cw - w) // 2, (ch - h) // 2
            sheet[r * ch + oy: r * ch + oy + h, c * cw + ox: c * cw + ox + w] = px
    out = Image.fromarray(sheet, 'P')
    out.putpalette(Image.open(PAL_REF).getpalette()[:48] + [0] * (768 - 48))
    out.save(os.path.join(SPR, name), transparency=0)
    print('%-22s celda %dx%d, %d filas' % (name, cw, ch, len(cells)))


def main():
    # --- FUSIL (naranja) -----------------------------------------------------
    # Indices de foot_gun.png en orden de lectura (ver el docstring):
    #   0 golpeado  1 cayendo  2,3 apunta  4 (dos pegados) y 5,6 culatazo
    #   7 fusil al hombro  8 fusil parado (burla)  9-11 disparo con fogonazo
    #   12,13 apunta  14-20 camina  21-25,29-31 camina de espaldas
    #   26-28,32-34 chispas de la bala contra el piso
    gun = build('foot_gun_gen.png', 'foot_gun.png', {4: 2}, [
        ('idle',    [2]),
        ('walk',    [14, 15, 16, 17, 18, 19, 20]),
        ('walk_up', [21, 22, 23, 24, 25, 29, 30, 31]),
        ('shoot',   [12, 11, 10, 9, 10, 12]),
        ('butt',    [(4, 0), (4, 1), 5, 6, 7]),
        ('hit',     [0]),
        ('death',   [0, 1, 1]),
        ('taunt',   [8, 8, 2, 8, 8, 2]),
    ])
    # La bala: un punto naranja (lo que sale del cano en el frame 13). Las
    # chispas: el impacto contra el piso.
    bullet = np.zeros((3, 5), np.uint8)
    bullet[:, :] = 9
    bullet[1, 1:4] = 6
    bullet[0, 0] = bullet[0, 4] = bullet[2, 0] = bullet[2, 4] = 0
    sparks = [gun[k][0] for k in (26, 27, 28, 32, 33, 34)]
    fx_sheet('foot_gun_fx.png', [[bullet], sparks], 16, 24)

    # --- MARTILLO (morado) ---------------------------------------------------
    #   0-7 camina  8-15 camina de espaldas  16-19 y 20 (tres pegados) martillazo
    #   21 levanta  22-25 golpeado  26-36 cae / en el piso
    ham = build('foot_hammer_gen.png', 'foot_hammer.png', {20: 3}, [
        ('idle',    [25]),
        ('walk',    [0, 1, 2, 3, 4, 5, 6, 7]),
        ('walk_up', [8, 9, 10, 11, 12, 13, 14, 15]),
        ('attack',  [16, 17, 18, 19, 19, (20, 0)]),
        ('hit',     [22, 23, 24]),
        ('death',   [23, 31, 26, 26]),
    ])

    # --- LANZA (morada) -----------------------------------------------------
    #   0 golpeado  1 patada  2-5 lanzamiento  6-8 estocada  9-16 camina
    #   17-19,21-25 camina de espaldas  20 la lanza sola (el proyectil)
    # No tiene caida propia: usa la del martillo (misma paleta, sin arma).
    extra = {('h', k): ham[k] for k in (31, 26)}
    spr = build('foot_spear_gen.png', 'foot_spear.png', {}, [
        ('idle',    [9]),
        ('walk',    [9, 10, 11, 12, 13, 14, 15, 16]),
        ('walk_up', [17, 18, 19, 21, 22, 23, 24, 25]),
        ('thrust',  [8, 7, 6, 6, 7]),
        ('throw',   [2, 3, 4, 5, 5, 5]),
        ('hit',     [0]),
        ('death',   [0, ('h', 31), ('h', 26), ('h', 26)]),
    ], extra=extra)
    lanza = spr[20][0]
    fx_sheet('foot_spear_fx.png', [[lanza[:, :]]], 96, 8)


if __name__ == '__main__':
    main()
