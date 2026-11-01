# -*- coding: utf-8 -*-
"""
gen_sewer_gator.py (01/11) -- el CAIMAN del arcade (sin usar en el juego) recoloreado para el Sewer
===============================================================================================
Fuente: dos cuadros de 128x32 en grises (res/images/lvl_3_sewer/gator_src_0.png boca abierta,
gator_src_1.png boca cerrada) sacados del codigo del arcade: el caiman nadando, su mancha negra
con ondas (sombra/agua) y el bultito de la cola a la derecha.

Los grises pasan a indices de la paleta de las TORTUGAS (PAL1, res/sprites/players/*_new_13x13.png):
las cuatro tortugas comparten la misma paleta (cada una usa sus propios indices para la vincha), asi
que los colores no cambian segun quien juega. Verdes de la piel: 2 (0,119,0), 5 (87,146,0), 8
(119,206,0); 3 oliva (87,87,0); 1 negro; 13 blanco; 11 violeta oscuro (72,0,145, el agua del Sewer).
(Ojo: con Slash, PAL1 se reemplaza por su paleta -> el caiman cambiaria de colores.)

Variante PAL0 (paleta del NIVEL, la del escenario bg_sewer_md.png; no cambia con Slash):
  PAL0 no tiene negro, verde ni blanco. Cuerpo ocre/ladrillo (12,7,9,8,13,11,10), sombra y contorno en
  granate (2), ondas de la mancha en gris (4), dientes naranja claro (10). Se evitan los indices 1 y 14 porque la animacion del agua los cicla.

Salida:
  res/images/lvl_3_sewer/sewer_gator.png      256x32 (los 2 cuadros), indexado con la paleta de las tortugas
  res/images/lvl_3_sewer/sewer_gator_pal0.png 256x32, indexado con la paleta del escenario (PAL0)
  _to_delete/_gator_preview*.png              vistas previas sobre el fondo del Sewer (no van al juego)

Uso (desde la raiz del proyecto):  python tools/gen_sewer_gator.py [pal0]
"""
import sys
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SEW = os.path.join(ROOT, 'res', 'images', 'lvl_3_sewer')
PAL_SRC = os.path.join(ROOT, 'res', 'sprites', 'players', 'leo_new_13x13.png')

# gris del original -> indice (cuerpo). Modo turtle = PAL1 de las tortugas; modo pal0 = paleta del nivel
CFG = {
    'turtle': dict(BODY={90: 3, 123: 2, 139: 2, 156: 5, 180: 5, 213: 8, 222: 8},
                   BLACK=1, WAKE_LINE=11, WHITE=13),
    # variante C (elegida): ocre/ladrillo, dientes en naranja claro (10)
    'pal0':   dict(BODY={90: 12, 123: 7, 139: 9, 156: 8, 180: 13, 213: 11, 222: 10},
                   BLACK=2, WAKE_LINE=4, WHITE=10),
}
BODY = BLACK = WAKE_LINE = WHITE = None   # se fijan en main() segun el modo
TAIL_X = 100              # desde aca (x) el bultito de la cola: es cuerpo
WAKE_Y = 21               # desde aca (y) y a la derecha del cuerpo (x >= WAKE_X): ondas de la mancha
WAKE_X = 69
TEETH = (0, 20, 16, 19)   # (x0, x1, y0, y1) de la boca abierta (cuadro 0): dientes claros


def recolor(path, frame):
    a = np.array(Image.open(path).convert('RGBA'))
    h, w, _ = a.shape
    out = np.zeros((h, w), np.uint8)
    for y in range(h):
        for x in range(w):
            r, g, b, al = a[y, x]
            if al == 0:
                continue
            if r == 0:
                out[y, x] = BLACK
                continue
            if r == 90:
                wake = (y >= WAKE_Y or WAKE_X <= x < TAIL_X)
                out[y, x] = WAKE_LINE if wake else BODY[90]
                continue
            out[y, x] = BODY[int(r)]
            if frame == 0 and TEETH[0] <= x <= TEETH[1] and TEETH[2] <= y <= TEETH[3] and r in (180, 213):
                out[y, x] = WHITE
    return out


def main():
    global BODY, BLACK, WAKE_LINE, WHITE
    mode = 'pal0' if 'pal0' in sys.argv[1:] else 'turtle'
    BODY, BLACK, WAKE_LINE, WHITE = (CFG[mode][k] for k in ('BODY', 'BLACK', 'WAKE_LINE', 'WHITE'))
    suffix = '_pal0' if mode == 'pal0' else ''
    pal_im = Image.open(os.path.join(SEW, 'bg_sewer_md.png') if mode == 'pal0' else PAL_SRC)
    pal = pal_im.getpalette()[:768]
    sheet = np.zeros((32, 256), np.uint8)
    for f in range(2):
        sheet[:, f * 128:(f + 1) * 128] = recolor(os.path.join(SEW, 'gator_src_%d.png' % f), f)
    im = Image.fromarray(sheet, 'P')
    im.putpalette(pal)
    im.info['transparency'] = 0
    im.save(os.path.join(SEW, 'sewer_gator%s.png' % suffix), transparency=0)

    # vista previa: sobre el fondo del Sewer, con el recorte de 3 bits por canal de rescomp
    def q(c):
        return tuple((v >> 5) * 36 for v in c)
    bg = Image.open(os.path.join(SEW, 'bg_sewer_md.png'))
    bp = bg.getpalette()[:48]
    bgrgb = np.array([q(bp[i * 3:i * 3 + 3]) for i in range(16)], np.uint8)[np.array(bg)]
    prev = Image.fromarray(bgrgb[:, :640], 'RGB')
    gp = np.array([q(pal[i * 3:i * 3 + 3]) for i in range(16)], np.uint8)
    for f, x0 in ((0, 40), (1, 300)):
        cell = sheet[:, f * 128:(f + 1) * 128]
        rgb = gp[cell]
        layer = Image.fromarray(rgb, 'RGB')
        mask = Image.fromarray(((cell != 0) * 255).astype(np.uint8), 'L')
        prev.paste(layer, (x0, 222), mask)
    prev = prev.resize((prev.width * 2, prev.height * 2), Image.NEAREST)
    os.makedirs(os.path.join(ROOT, '_to_delete'), exist_ok=True)
    prev.save(os.path.join(ROOT, '_to_delete', '_gator_preview%s.png' % suffix))
    print('sewer_gator%s.png 256x32 listo; vista previa en _to_delete/_gator_preview%s.png' % (suffix, suffix))


if __name__ == '__main__':
    main()
