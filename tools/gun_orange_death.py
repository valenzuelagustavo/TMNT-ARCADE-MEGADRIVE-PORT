# -*- coding: utf-8 -*-
"""
gun_orange_death.py -- (22/10) la MUERTE del naranja del fusil = la del naranja
================================================================================
El naranja del fusil (foot_gun_gen.png) tenia una caida de 3 frames sin
explosion: quedaba tirado y parpadeaba. Gustavo pidio que use la muerte del
naranja de los shurikens (Foot_Soldier_Orange_new.png, fila 6: sale volando,
cae, se prende y EXPLOTA). Las dos hojas tienen la MISMA paleta, asi que los
pixeles se copian tal cual.

  - celda naranja 104x104, pies en y = 96 (ENEMY_FOOT_OFFSET_ORANGE), cuerpo
    centrado en x = 52;
  - celda del fusil 120x72, pies en el borde de abajo, cuerpo en x = 60.
  El cuerpo tirado de la muerte baja hasta y = 103 (7 px debajo de los pies) y
  no entra en los 72 px: se pega GUN_DEATH_DROP (8) px mas arriba y el juego
  baja el sprite esos 8 px mientras muere (footOffset, ver enemy.c).

La fila 6 (GUN_ANIM_EXPLODE) queda con SEQ: el frame de la explosion va dos
veces para que se vea como en el naranja (8 ticks por frame alla, 6 aca).

Lo llama gen_foot_weapons.py al final; tambien se puede correr solo (no usa
scipy):  python tools/gun_orange_death.py
"""
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SPR = os.path.join(ROOT, 'res', 'sprites')
ORANGE = os.path.join(SPR, 'Foot_Soldier_Orange_new.png')
GUN = os.path.join(SPR, 'foot_gun_gen.png')

OW, OH, OROW, OFEET, OCX = 104, 104, 6, 96, 52
GW, GH, GROW, GCX = 120, 72, 6, 60
GUN_DEATH_DROP = 8
SEQ = [0, 1, 2, 3, 3]


def apply(gun_path=GUN, orange_path=ORANGE):
    o = Image.open(orange_path)
    g = Image.open(gun_path)
    assert o.mode == 'P' and g.mode == 'P'
    assert o.getpalette()[:48] == g.getpalette()[:48], 'las paletas no coinciden'
    O = np.array(o)
    G = np.array(g)
    cols = G.shape[1] // GW
    assert len(SEQ) <= cols
    dy = (GH - GUN_DEATH_DROP) - OFEET          # y naranja -> y fusil
    dx = GCX - OCX
    G[GROW * GH:(GROW + 1) * GH, :] = 0
    for c, f in enumerate(SEQ):
        src = O[OROW * OH:(OROW + 1) * OH, f * OW:(f + 1) * OW]
        ys, xs = np.nonzero(src)
        assert ys.min() + dy >= 0 and ys.max() + dy < GH, 'no entra en alto'
        assert xs.min() + dx >= 0 and xs.max() + dx < GW, 'no entra en ancho'
        cell = G[GROW * GH:(GROW + 1) * GH, c * GW:(c + 1) * GW]
        cell[ys + dy, xs + dx] = src[ys, xs]
    out = Image.fromarray(G, 'P')
    out.putpalette(g.getpalette())
    if 'transparency' in g.info:
        out.info['transparency'] = g.info['transparency']
    out.save(gun_path, **({'transparency': g.info['transparency']} if 'transparency' in g.info else {}))
    print('foot_gun_gen.png: fila %d = muerte del naranja %s' % (GROW, SEQ))


if __name__ == '__main__':
    apply()
