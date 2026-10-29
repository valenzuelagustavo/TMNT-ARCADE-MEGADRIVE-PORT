#!/usr/bin/env python3
# =============================================================================
# gen_foot_ledge_jump.py (28/10) - fila "salto del escalon" de todos los foot soldiers
# =============================================================================
# Fuente: res/sprites/foot_soldier_ledge_jump_src.png = la fila que dibujo el artista para el
# MORADO (4 cuadros de 64 px de ancho): 0 parado, 1 inicio del salto, 2 en el aire (sube y baja),
# 3 aterrizaje. En el sheet original venia pegada debajo de la grilla de 80 px (1440..1570), asi que
# rescomp no la podia tomar: aca se la acomoda en una celda comun y se la agrega como ULTIMA fila
# de cada hoja (anim = cantidad de filas de antes: ver ENEMY_LEDGE_ANIM_* en enemy.h).
#
# Para los demas soldiers se recolorea el morado con la paleta de cada uno (traje: indices 1,2,3
# del morado -> los del naranja / blanco / amarillo; el resto, al color mas cercano de su paleta) y
# se centra sobre el cuerpo de su cuadro de idle, apoyando los pies en el piso de esa hoja.
# Es un PROVISORIO "adaptando los colores": cuando el artista dibuje la fila propia de un tipo,
# se pega a mano en su hoja y se saca de TYPES.
#
# IDEMPOTENTE: corta cada hoja a sus filas originales (BASE) y agrega la fila.
# Si se regenera una hoja con su propio script (gen_foot_weapons.py: gun/hammer/spear, etc.),
# correr este DESPUES.
# Uso: python tools/gen_foot_ledge_jump.py
# =============================================================================
import os
import numpy as np
from PIL import Image

SPR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'res', 'sprites')

# nombre: (archivo, ancho de celda, alto de celda, filas originales, indices del traje (para
#          centrar), mapa del traje morado (1,2,3) -> indices de esa paleta o None = igual)
TYPES = {
    'purple': ('foot_soldier_purple_13x13.png', 64, 80, 18, (1, 2, 3), None),
    'yellow': ('Foot_Soldier_Yellow_boomerang.png', 64, 80, 9, (6, 7, 8), {1: 8, 2: 7, 3: 6}),
    'orange': ('Foot_Soldier_Orange_new.png', 104, 104, 9, (7, 9, 10), {1: 10, 2: 9, 3: 7}),
    'gun':    ('foot_gun_gen.png', 120, 72, 8, (7, 9, 10), {1: 10, 2: 9, 3: 7}),
    'white':  ('foot_soldier_white_gen.png', 104, 104, 11, (11, 3), {1: 13, 2: 12, 3: 11}),
    'knife':  ('foot_soldier_knife_gen.png', 104, 104, 9, (11, 3), {1: 13, 2: 12, 3: 11}),
    'hammer': ('foot_hammer_gen.png', 104, 96, 6, (2, 3), None),
    'spear':  ('foot_spear_gen.png', 136, 104, 7, (2, 3), None),
}
SRC_CELL_W = 64
SRC_FEET = 94          # fila de los pies (parado) en la fuente (130 px de alto)
PURPLE_SUIT = (1, 2, 3)


def med_x(cell, idxs):
    ys, xs = np.where(np.isin(cell, idxs))
    return float(np.median(xs))


def main():
    src_im = Image.open(os.path.join(SPR, 'foot_soldier_ledge_jump_src.png'))
    src = np.array(src_im)
    sp = np.array(src_im.getpalette()).reshape(-1, 3)[:16]
    pur = np.array(Image.open(os.path.join(SPR, TYPES['purple'][0])))
    mp = med_x(pur[0:80, 0:64], PURPLE_SUIT)
    for name, (fn, W, H, base, suit, cmap) in TYPES.items():
        path = os.path.join(SPR, fn)
        im = Image.open(path)
        a = np.array(im)
        assert a.shape[1] % W == 0, (name, a.shape)
        assert a.shape[0] in (base * H, (base + 1) * H) or (name == 'purple' and a.shape[0] >= base * H), (name, a.shape)
        a = a[:base * H]
        for r in range(base):
            assert (a[r * H:(r + 1) * H] != 0).any(), (name, 'fila vacia', r)
        tp = np.array(im.getpalette()).reshape(-1, 3)[:16]
        idle = a[0:H, 0:W]
        ys, xs = np.where(idle != 0)
        feet = int(ys.max())                    # fila de los pies (borde de abajo del idle)
        dx = int(round(med_x(idle, suit) - mp))
        # tabla de colores: indice del morado -> indice de esta paleta
        lut = np.zeros(256, np.uint8)
        for i in range(1, 16):
            if cmap and i in cmap:
                lut[i] = cmap[i]
            elif cmap is None:
                lut[i] = i
            else:
                d = ((tp[1:] - sp[i]) ** 2).sum(1)
                lut[i] = 1 + int(np.argmin(d))
        row = np.zeros((H, a.shape[1]), np.uint8)
        crop = 0
        for f in range(4):
            sub = src[:, f * SRC_CELL_W:(f + 1) * SRC_CELL_W]
            ys2, xs2 = np.where(sub != 0)
            for y, x in zip(ys2, xs2):
                ty = int(y - SRC_FEET + feet)
                tx = int(x + f * 0 + dx)
                if ty < 0 or ty >= H or tx < 0 or tx >= W:
                    crop += 1
                    continue
                row[ty, f * W + tx] = lut[sub[y, x]]
        out = np.vstack([a, row])
        o = Image.fromarray(out, 'P')
        o.putpalette(im.getpalette())
        o.save(path, transparency=0)
        print('%-7s celda %dx%d  pies %d  dx %+d  fila nueva = anim %d  (px recortados: %d)' % (name, W, H, feet, dx, base, crop))


if __name__ == '__main__':
    main()
