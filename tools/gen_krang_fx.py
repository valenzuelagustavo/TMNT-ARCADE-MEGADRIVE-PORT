#!/usr/bin/env python3
# =============================================================================
# gen_krang_fx.py (13/10) -- efectos de KRANG en su paleta (PAL2)
# =============================================================================
# Salidas (las compila final_bosses.res):
#
#   krang_eyebeam_gen.png  80x32: el LASER de los ojos, dos frames de 40x32.
#       Un tramo diagonal a 45 grados (baja 1 px por cada px hacia adelante),
#       mirando a la DERECHA. Es la continuacion exacta del arranque del rayo
#       que ya trae el frame 2 de la anim de laser (krang_boss_gen.png): las
#       dos lineas finas (blanco 6) con su halo (celeste 9), patron
#       "B W B B W B" por fila. krang.c apila 3 tramos (32 filas cada uno)
#       desde los ojos hasta el piso. 40 de ancho para que la ultima fila del
#       tramo (que se corre 31 px) no se corte. Frame 1: solo los nucleos
#       blancos (titila).
#
#   krang_boom_gen.png  512x128: la explosion de BAXTER (baxter_boom_gen.png)
#       pasada a la paleta de Krang. Solo los cuartos de la IZQUIERDA (anim 0
#       arriba, anim 1 abajo), 8 frames de 64x64; krang.c pone los de la
#       derecha espejados (igual que fw_bomb_boom de la autopista).
#       blanco -> blanco celeste (6), amarillo claro -> amarillo (7),
#       salmon -> naranja (8), rosa oscuro -> rojo (12).
#
# Uso: python3 tools/gen_krang_fx.py
# =============================================================================
import os
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPR = os.path.join(ROOT, 'res', 'sprites')


def save_p(a, pal, path):
    im = Image.fromarray(a.astype(np.uint8), 'P')
    im.putpalette(pal + [0] * (768 - len(pal)))
    im.save(path, transparency=0)
    print('%s %dx%d' % (os.path.basename(path), a.shape[1], a.shape[0]))


def main():
    kb = Image.open(os.path.join(SPR, 'krang_boss_gen.png'))
    pal = kb.getpalette()[:48]

    # --- laser de los ojos ----------------------------------------------------
    B, W = 9, 6
    beam = np.zeros((32, 80), np.uint8)
    full = [B, W, B, B, W, B]
    core = [0, W, 0, 0, W, 0]
    for r in range(32):
        for i in range(6):
            beam[r, r + i] = full[i]
            if core[i]:
                beam[r, 40 + r + i] = core[i]
    save_p(beam, pal, os.path.join(SPR, 'krang_eyebeam_gen.png'))

    # --- explosion de Baxter en la paleta de Krang ----------------------------
    bx = np.array(Image.open(os.path.join(SPR, 'baxter_boom_gen.png')))
    bx = np.concatenate([bx[:64], bx[128:192]])      # cuartos de la izquierda
    lut = np.zeros(256, np.uint8)
    for src, dst in ((15, 6), (6, 7), (10, 8), (13, 12)):
        lut[src] = dst
    save_p(lut[bx], pal, os.path.join(SPR, 'krang_boom_gen.png'))


if __name__ == '__main__':
    main()
