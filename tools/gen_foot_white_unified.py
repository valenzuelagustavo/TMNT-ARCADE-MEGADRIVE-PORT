#!/usr/bin/env python3
# =============================================================================
# gen_foot_white_unified.py (15/10) -- Foot Soldier BLANCO desde la hoja unica
# =============================================================================
# Fuente unica y definitiva (de Gustavo):
#   res/sprites/Arcade - Teenage Mutant Ninja Turtles - Enemies - Foot Soldier (White).png
#   407x1937, 16 colores, poses sueltas (5 columnas). Trae la espada, el
#   cuchillo, el cuchillo tirado (vuelo, impacto, pedazos) y la salida del agua.
#
# Reemplaza a gen_foot_white.py + gen_foot_knife.py (que partian del rip viejo
# foot_soldier_white_sword.png). Genera los MISMOS archivos, con el mismo
# tamano y cada pose en el mismo lugar de su celda, asi el codigo (anims,
# offsets, hitboxes, pies en la fila 103) no cambia:
#   foot_soldier_white_gen.png   espada   (8 cols x 10 filas de 104x104)
#   foot_soldier_knife_gen.png   cuchillo (8 cols x  8 filas de 104x104)
#   knife_proj_gen.png           cuchillo tirado (3 frames de 40x24)
#   water_splash_gen.png         salpicon de la cloaca (4 frames de 64x48)
# Todos con la paleta de la hoja nueva (PAL3 en el juego).
#
# COMO SE ARMO LA TABLA: cada frame de los .png viejos se busco en la hoja
# nueva por forma (IoU de la silueta = 1.000 en las 73 poses). La hoja nueva
# esta ESPEJADA respecto del rip viejo salvo la caida (fila 8) y la muerte
# (fila 9), por eso cada entrada dice si se espeja (flip).
# El salpicon de la hoja nueva es tierra/barro (marron), dibujado distinto:
# se toman de esas 4 poses solo el marron (5), el gris oscuro (14) y el gris
# (13) pegado a ellos en la mitad de abajo, sin el cuerpo del soldier.
#
# Uso: python3 tools/gen_foot_white_unified.py   (despues: gen_sewer_mouser.py,
#      que pasa los Mousers de la cloaca a esta misma paleta)
# =============================================================================
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SPR = os.path.join(ROOT, 'res', 'sprites')
SRC = os.path.join(SPR, 'Arcade - Teenage Mutant Ninja Turtles - Enemies - Foot Soldier (White).png')

OUT = {   # salida: (archivo, ancho de celda, alto de celda, columnas, filas)
    "SWORD": ('foot_soldier_white_gen.png', 104, 104, 8, 11),   # (15/10) +patada
    "KNIFE": ('foot_soldier_knife_gen.png', 104, 104, 8, 9),
    "PROJ":  ('knife_proj_gen.png',          40,  24, 3, 1),
}

# (salida, fila, col, x en la hoja, y en la hoja, ancho, alto, flip,
#  x en la celda, y en la celda)
POSES = [
    ("SWORD", 0, 0,   85,  983,  51,  57, 1,   34,  47),
    ("SWORD", 1, 0,  333, 1460,  36,  63, 1,   43,  40),
    ("SWORD", 1, 1,  251, 1460,  40,  63, 1,   45,  40),
    ("SWORD", 1, 2,  170, 1461,  41,  62, 1,   42,  41),
    ("SWORD", 1, 3,   90, 1460,  41,  63, 1,   43,  40),
    ("SWORD", 1, 4,   10, 1460,  41,  63, 1,   46,  40),
    ("SWORD", 2, 0,  335, 1541,  32,  61, 1,   26,  43),
    ("SWORD", 2, 1,  255, 1541,  32,  62, 1,   24,  42),
    ("SWORD", 2, 2,  177, 1542,  28,  60, 1,   21,  43),
    ("SWORD", 2, 3,  334, 1622,  33,  60, 1,   19,  43),
    ("SWORD", 2, 4,  255, 1621,  32,  62, 1,   21,  41),
    ("SWORD", 2, 5,  171, 1621,  40,  62, 1,   23,  41),
    ("SWORD", 2, 6,   94, 1621,  33,  62, 1,   30,  41),
    ("SWORD", 2, 7,   17, 1621,  28,  62, 1,   31,  41),
    ("SWORD", 3, 0,  310, 1142,  97,  59, 1,    7,  44),
    ("SWORD", 3, 1,  241, 1142,  52,  60, 1,    0,  43),
    ("SWORD", 3, 2,  167, 1143,  47,  57, 1,    5,  46),
    ("SWORD", 4, 0,  247, 1216,  47,  72, 1,   11,  31),
    ("SWORD", 4, 1,  167, 1216,  48,  72, 1,    9,  31),
    ("SWORD", 4, 2,  312, 1220,  77,  63, 1,   16,  40),
    ("SWORD", 5, 0,  167, 1216,  48,  72, 1,    8,  31),
    ("SWORD", 5, 1,   89, 1305,  43,  53, 1,   15,  50),
    ("SWORD", 5, 2,    5, 1303,  52,  57, 1,   14,  46),
    ("SWORD", 6, 0,  167,  981,  48,  62, 1,   21,  38),
    ("SWORD", 6, 1,  256, 1074,  29,  35, 1,   27,  44),
    ("SWORD", 6, 2,  175, 1076,  31,  32, 1,   28,  47),
    ("SWORD", 6, 3,   95, 1073,  32,  37, 1,   25,  43),
    ("SWORD", 6, 4,   15, 1077,  32,  29, 1,   25,  50),
    ("SWORD", 7, 0,    6, 1374,  49,  76, 1,    9,  19),
    ("SWORD", 7, 1,  326, 1067,  50,  49, 1,   21,  36),
    # (15/10) golpe recibido: los 4 primeros de la fila 11 de la hoja contando
    # desde 1 (y 817..885; la captura que paso Gustavo), SIN espejar (como la
    # muerte: espejados se veian girados), con la sombra centrada en x 46 y
    # los pies en la fila 101. (La fila 10, y 736,
    # es una patada voladora: no va aca.)
    ("SWORD", 8, 0,   12,  825,  38,  54, 0,   22,  48),
    ("SWORD", 8, 1,   83,  824,  55,  55, 0,   14,  47),
    ("SWORD", 8, 2,  170,  824,  42,  55, 0,   26,  47),
    ("SWORD", 8, 3,  253,  824,  36,  55, 0,   27,  47),
    ("SWORD", 9, 0,   14, 1865,  41,  54, 0,   35,  48),
    ("SWORD", 9, 1,  102, 1894,  60,  27, 0,   19,  77),
    ("SWORD", 9, 2,  206, 1893,  60,  28, 0,   19,  76),
    ("SWORD", 9, 3,  312, 1867,  57,  54, 0,   21,  50),
    ("KNIFE", 0, 0,  251,  342,  40,  59, 1,   34,  41),
    ("KNIFE", 1, 0,  173,  101,  36,  61, 1,   39,  39),
    ("KNIFE", 1, 1,   96,  101,  30,  62, 1,   32,  38),
    ("KNIFE", 1, 2,   20,  100,  21,  63, 1,   44,  37),
    ("KNIFE", 1, 3,  338,   20,  26,  63, 1,   38,  37),
    ("KNIFE", 1, 4,  254,   21,  34,  62, 1,   38,  38),
    ("KNIFE", 1, 5,  177,   20,  27,  63, 1,   42,  37),
    ("KNIFE", 1, 6,   98,   20,  26,  63, 1,   46,  37),
    ("KNIFE", 1, 7,   17,   21,  28,  62, 1,   44,  38),
    ("KNIFE", 2, 0,   15,  262,  32,  60, 1,   37,  40),
    ("KNIFE", 2, 1,  335,  181,  32,  62, 1,   42,  38),
    ("KNIFE", 2, 2,  253,  181,  35,  62, 1,   40,  38),
    ("KNIFE", 2, 3,  177,  181,  27,  62, 1,   43,  38),
    ("KNIFE", 2, 4,   97,  181,  28,  62, 1,   42,  38),
    ("KNIFE", 2, 5,   18,  181,  26,  61, 1,   48,  39),
    ("KNIFE", 2, 6,  337,  101,  27,  62, 1,   46,  38),
    ("KNIFE", 2, 7,  257,  102,  28,  60, 1,   40,  40),
    ("KNIFE", 3, 0,  248,  657,  46,  69, 1,   29,  31),
    ("KNIFE", 3, 1,  327,  658,  47,  67, 1,   32,  33),
    ("KNIFE", 3, 2,  161,  664,  60,  56, 1,   32,  44),
    ("KNIFE", 3, 3,   92,  665,  37,  54, 1,   36,  46),
    ("KNIFE", 3, 4,   17,  664,  27,  55, 1,   39,  45),
    ("KNIFE", 4, 0,   91,  582,  39,  60, 1,   34,  40),
    ("KNIFE", 4, 1,  171,  580,  40,  63, 1,   41,  37),
    ("KNIFE", 4, 2,  240,  582,  62,  60, 1,   37,  40),
    ("KNIFE", 5, 0,   10,  583,  42,  57, 1,   42,  43),
    ("KNIFE", 5, 1,   92,  502,  37,  60, 1,   36,  40),
    ("KNIFE", 5, 2,    3,  500,  55,  64, 1,   22,  36),
    # (15/10) golpe recibido: los mismos 4 que la espada.
    ("KNIFE", 6, 0,   12,  825,  38,  54, 0,   22,  48),
    ("KNIFE", 6, 1,   83,  824,  55,  55, 0,   14,  47),
    ("KNIFE", 6, 2,  170,  824,  42,  55, 0,   26,  47),
    ("KNIFE", 6, 3,  253,  824,  36,  55, 0,   27,  47),
    ("KNIFE", 7, 0,   14, 1865,  41,  54, 0,   35,  48),
    ("KNIFE", 7, 1,  102, 1894,  60,  27, 0,   19,  77),
    ("KNIFE", 7, 2,  206, 1893,  60,  28, 0,   19,  76),
    ("KNIFE", 7, 3,  312, 1867,  57,  54, 0,   21,  50),
    ("PROJ", 0, 0,  322,  591,  17,   2, 1,   11,  11),   # a mano: la linea
                                                            # sola calza en cualquier lado
    ("PROJ", 0, 1,  360,  585,  21,  14, 1,    9,   5),
    ("PROJ", 0, 2,  356,  625,  29,  14, 1,    5,   5),
]

# (15/10) PATADA VOLADORA (anim 10 de la espada, anim 8 del cuchillo), con las
# mismas 4 fases que la del morado: despegue (en el piso, con sombra), sube
# encogido, patada, baja. La hoja no trae la altura del salto: las poses en el
# aire se suben LIFT px y se les pone abajo la sombra del despegue (como el
# morado, que trae la altura y la sombra dibujadas en el frame).
#   (salida, fila, col, x, y, ancho, alto, lift)
KICKS = [
    ("SWORD", 10, 0,   85,  983,  51,  57,  0),   # despegue (espada atras)
    ("SWORD", 10, 1,  246,  987,  50,  50, 26),   # sube encogido
    ("SWORD", 10, 2,  322,  989,  58,  45, 32),   # patada
    ("SWORD", 10, 3,  167,  981,  48,  62, 18),   # baja
    ("KNIFE",  8, 0,  330,  743,  42,  57,  0),
    ("KNIFE",  8, 1,  168,  747,  46,  50, 26),
    ("KNIFE",  8, 2,   82,  749,  58,  45, 32),
    ("KNIFE",  8, 3,  252,  741,  38,  62, 18),
]
KICK_SHADOW_X = 46   # centro de la sombra en la celda (como el soldado parado)
KICK_FEET_Y   = 101  # fila de abajo de la sombra


# Salpicon: (frame, x, y, ancho, alto, flip, x en la celda, y en la celda)
SPLASH = [
    (0,  260, 1726,  22,  12, 1,   21,  36),
    (1,  334, 1715,  34,  33, 1,   15,  15),
    (2,    9, 1789,  45,  48, 1,    9,   0),
    (3,   86, 1805,  49,  48, 1,    7,   0),
]


def dilate1(m):
    """Dilatacion de 1 px en cruz (4 vecinos), sin scipy."""
    p = np.pad(m, 1)
    h, w = m.shape
    return (p[1:h + 1, 1:w + 1] | p[0:h, 1:w + 1] | p[2:h + 2, 1:w + 1]
            | p[1:h + 1, 0:w] | p[1:h + 1, 2:w + 2])


def put_kicks(a, out, key):
    """Patada voladora: ver KICKS."""
    shadow = None          # sombra del despegue: (ys, xs) en la celda
    torso_x = None         # centro del torso del despegue, en la celda
    for k, r, c, sx, sy, w, h, lift in KICKS:
        if k != key:
            continue
        piece = a[sy:sy + h, sx:sx + w][:, ::-1]      # la hoja mira al reves
        top = piece[: int(h * 0.6)] != 0
        tx = np.nonzero(top)[1].mean()
        if lift == 0:
            sh = piece == 14
            sh[: h - 6] = False
            dx = int(round(KICK_SHADOW_X - np.nonzero(sh)[1].mean()))
            dy = KICK_FEET_Y - (h - 1)
            # La sombra del despegue tiene huecos donde la tapan las botas:
            # en el aire se dibuja una elipse con su mismo tamano.
            sy_, sx_ = np.nonzero(sh)
            cy = (sy_.min() + sy_.max()) / 2.0
            cx = (sx_.min() + sx_.max()) / 2.0
            ry = max(1.0, (sy_.max() - sy_.min()) / 2.0 + 0.5)
            rx = (sx_.max() - sx_.min()) / 2.0 + 0.5
            ys, xs = [], []
            for y in range(sy_.min(), sy_.max() + 1):
                for x in range(sx_.min(), sx_.max() + 1):
                    if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0:
                        ys.append(y + dy)
                        xs.append(x + dx)
            shadow = (np.array(ys), np.array(xs))
            torso_x = tx + dx
        else:
            dx = int(round(torso_x - tx))
            dy = KICK_FEET_Y - 4 - lift - (h - 1)
        cell = out[r * 104:(r + 1) * 104, c * 104:(c + 1) * 104]
        if lift and shadow is not None:
            cell[shadow] = 14
        dst = cell[dy:dy + h, dx:dx + w]
        np.copyto(dst, piece, where=piece != 0)


def main():
    src = Image.open(SRC)
    a = np.array(src)
    pal = src.getpalette()[:48]
    pal = pal + [0] * (768 - len(pal))

    def save(arr, name):
        im = Image.fromarray(arr, 'P')
        im.putpalette(pal)
        im.save(os.path.join(SPR, name), transparency=0)
        print('->', name, arr.shape[1], 'x', arr.shape[0])

    for key, (name, cw, ch, cols, rows) in OUT.items():
        out = np.zeros((ch * rows, cw * cols), np.uint8)
        for k, r, c, sx, sy, w, h, fl, dx, dy in POSES:
            if k != key:
                continue
            piece = a[sy:sy + h, sx:sx + w]
            if fl:
                piece = piece[:, ::-1]
            dst = out[r * ch + dy:r * ch + dy + h, c * cw + dx:c * cw + dx + w]
            np.copyto(dst, piece, where=piece != 0)
        if key in ('SWORD', 'KNIFE'):
            put_kicks(a, out, key)
        save(out, name)

    out = np.zeros((48, 64 * 4), np.uint8)
    for f, sx, sy, w, h, fl, dx, dy in SPLASH:
        reg = a[sy:sy + h, sx:sx + w].copy()
        base = np.isin(reg, (5, 14))
        low = (np.arange(h)[:, None] >= h * 0.45) & np.ones((1, w), bool)
        grey = (reg == 13) & dilate1(base) & low
        reg[~(base | grey)] = 0
        if fl:
            reg = reg[:, ::-1]
        out[dy:dy + h, f * 64 + dx:f * 64 + dx + w] = reg
    save(out, 'water_splash_gen.png')


if __name__ == '__main__':
    main()
