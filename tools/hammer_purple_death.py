# -*- coding: utf-8 -*-
"""
hammer_purple_death.py -- (31/10) la MUERTE del foot del MARTILLO = la del purple
================================================================================
Igual que la lanza (tools/spear_purple_death.py): el morado del martillo
(foot_hammer_gen.png) caia en 4 frames, sin explosion, y despues parpadeaba
hasta desaparecer. Gustavo pidio que muera como el foot soldier purple
(foot_soldier_purple_13x13.png, fila 5 = ENEMY_ANIM_EXPLODE: 6 frames, sale
volando, cae y EXPLOTA). Las dos hojas comparten la paleta de enemigos.

  - celda purple 64x80, pies en el borde de abajo (y = 79), cuerpo en x = 32;
  - celda del martillo 104x96, pies en el borde de abajo (y = 95), cuerpo
    tirado centrado en x = 52 (la caida vieja ocupaba x 12..80).
Los pies coinciden con el borde, asi que no hace falta ningun footOffset.

Mismo reparto de tiempos que la lanza: el purple dura 8 ticks por frame y las
hojas de arma 6 (FW_TICKS), asi que la fila 5 (HAMMER_ANIM_EXPLODE) son 8
casillas de 6 ticks = 48 = ENEMY_EXPLODE_TIME.

Lo llama gen_foot_weapons.py al final; tambien se puede correr solo:
    python tools/hammer_purple_death.py
"""
import os
import numpy as np
from PIL import Image
import spear_purple_death as sp

HAMMER = os.path.join(sp.SPR, 'foot_hammer_gen.png')
HW, HH, HROW, HCX = 104, 96, 5, 52         # celda martillo, fila HAMMER_ANIM_EXPLODE


def apply(hammer_path=HAMMER, purple_path=sp.PURPLE):
    p = Image.open(purple_path)
    s = Image.open(hammer_path)
    assert p.mode == 'P' and s.mode == 'P'
    P = np.array(p)
    S = np.array(s)
    tab = sp.remap_table(p.getpalette(), s.getpalette())
    cols = S.shape[1] // HW
    assert len(sp.SEQ) <= cols
    dy = HH - sp.PH                   # los pies (borde de abajo) coinciden
    dx = HCX - sp.PCX
    S[HROW * HH:(HROW + 1) * HH, :] = 0
    for c, f in enumerate(sp.SEQ):
        src = P[sp.PROW * sp.PH:(sp.PROW + 1) * sp.PH, f * sp.PW:(f + 1) * sp.PW]
        ys, xs = np.nonzero(src)
        assert ys.min() + dy >= 0 and ys.max() + dy < HH, 'no entra en alto'
        assert xs.min() + dx >= 0 and xs.max() + dx < HW, 'no entra en ancho'
        cell = S[HROW * HH:(HROW + 1) * HH, c * HW:(c + 1) * HW]
        cell[ys + dy, xs + dx] = tab[src[ys, xs]]
    out = Image.fromarray(S, 'P')
    out.putpalette(s.getpalette())
    kw = {'transparency': s.info['transparency']} if 'transparency' in s.info else {}
    if kw:
        out.info['transparency'] = kw['transparency']
    out.save(hammer_path, **kw)
    print('foot_hammer_gen.png: fila %d = muerte del purple %s' % (HROW, sp.SEQ))


if __name__ == '__main__':
    apply()
