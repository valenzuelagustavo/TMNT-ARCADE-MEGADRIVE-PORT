# -*- coding: utf-8 -*-
"""
spear_purple_death.py -- (29/10) la MUERTE del foot de la LANZA = la del purple
================================================================================
El morado de la lanza (foot_spear_gen.png) usaba la caida del martillo (4
frames, sin explosion) y despues parpadeaba hasta desaparecer. Gustavo pidio
que muera igual que el foot soldier purple (foot_soldier_purple_13x13.png,
fila 5 = ENEMY_ANIM_EXPLODE: 6 frames, sale volando, cae y EXPLOTA).
Las dos hojas comparten la paleta de enemigos, asi que los pixeles se copian
tal cual (si algun indice difiere se remapea por color RGB).

  - celda purple 64x80, pies en el borde de abajo (y = 79), cuerpo en x = 32;
  - celda de la lanza 136x104, pies en el borde de abajo (y = 103), cuerpo en
    x = 68.
  Los pies coinciden con el borde, asi que el cuerpo tirado entra sin bajar el
  sprite (no hace falta ningun footOffset especial como en el fusil).

El purple dura 8 ticks por frame (6 frames = 48 = ENEMY_EXPLODE_TIME) y las
hojas de arma 6 ticks por frame (FW_TICKS): la fila 6 (SPEAR_ANIM_EXPLODE) se
arma con SEQ, repartiendo los frames para que los cortes caigan en los mismos
instantes (8 casillas x 6 = 48 ticks).

Lo llama gen_foot_weapons.py al final; tambien se puede correr solo:
    python tools/spear_purple_death.py
"""
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SPR = os.path.join(ROOT, 'res', 'sprites')
PURPLE = os.path.join(SPR, 'foot_soldier_purple_13x13.png')
SPEAR = os.path.join(SPR, 'foot_spear_gen.png')

PW, PH, PROW, PCX = 64, 80, 5, 32          # celda purple, fila ENEMY_ANIM_EXPLODE
SW, SH, SROW, SCX = 136, 104, 6, 68        # celda lanza, fila SPEAR_ANIM_EXPLODE
NFRAMES = 6
TICKS_SRC, TICKS_DST = 8, 6                # tiempo por frame: purple / hojas de arma
# casilla k (6 ticks) -> frame del purple que se ve en su instante medio
SEQ = [min(NFRAMES - 1, (k * TICKS_DST + TICKS_DST // 2) // TICKS_SRC)
       for k in range(NFRAMES * TICKS_SRC // TICKS_DST)]


def remap_table(p_pal, s_pal):
    """indice de la hoja purple -> indice de la hoja de la lanza (por color)."""
    p = np.array(p_pal[:768], np.uint8).reshape(-1, 3)
    s = np.array(s_pal[:768], np.uint8).reshape(-1, 3)
    tab = np.arange(256, dtype=np.uint8)
    for i in range(1, 16):
        if not (p[i] == s[i]).all():
            d = ((s[:16].astype(int) - p[i].astype(int)) ** 2).sum(axis=1)
            d[0] = 1 << 30
            tab[i] = int(d.argmin())
    return tab


def apply(spear_path=SPEAR, purple_path=PURPLE):
    p = Image.open(purple_path)
    s = Image.open(spear_path)
    assert p.mode == 'P' and s.mode == 'P'
    P = np.array(p)
    S = np.array(s)
    tab = remap_table(p.getpalette(), s.getpalette())
    cols = S.shape[1] // SW
    assert len(SEQ) <= cols
    dy = SH - PH                      # los pies (borde de abajo) coinciden
    dx = SCX - PCX
    S[SROW * SH:(SROW + 1) * SH, :] = 0
    for c, f in enumerate(SEQ):
        src = P[PROW * PH:(PROW + 1) * PH, f * PW:(f + 1) * PW]
        ys, xs = np.nonzero(src)
        assert ys.min() + dy >= 0 and ys.max() + dy < SH, 'no entra en alto'
        assert xs.min() + dx >= 0 and xs.max() + dx < SW, 'no entra en ancho'
        cell = S[SROW * SH:(SROW + 1) * SH, c * SW:(c + 1) * SW]
        cell[ys + dy, xs + dx] = tab[src[ys, xs]]
    out = Image.fromarray(S, 'P')
    out.putpalette(s.getpalette())
    kw = {'transparency': s.info['transparency']} if 'transparency' in s.info else {}
    if kw:
        out.info['transparency'] = kw['transparency']
    out.save(spear_path, **kw)
    print('foot_spear_gen.png: fila %d = muerte del purple %s' % (SROW, SEQ))


if __name__ == '__main__':
    apply()
