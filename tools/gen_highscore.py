#!/usr/bin/env python3
# =============================================================================
# gen_highscore.py -- graficos de la carga de iniciales y del "BEST 10 PLAYERS"
# =============================================================================
# (20/10) Como en el arcade: al terminar la partida (game over o despues de los
# creditos) cada jugador que entra en el top 10 pone 3 letras en una pantalla
# con SU retrato del selector a color y los otros tres en negro (la silueta, sin
# el fondo plano de color), y despues se ve la tabla con los retratos pasando
# por detras de derecha a izquierda, cada uno en un solo tono.
#
# FUENTES
#   res/images/hud/select player_15 CORES.png   retratos del selector (64x128
#       en x = 13 + 77*t, y = 81; fondo plano de color por tortuga)
#   res/images/option_menu/option_font.png      letras 7x7 (ASCII 32..126)
#
# SALIDA
#   res/images/hiscore/hs_portraits.png  SPRITE 8x16 tiles, 4 frames (Leo,
#       Mike, Don, Raph) SIN el fondo plano (indice 0), paleta del selector.
#   src/hiscore_gfx.c / .h  (GENERADOS) tiles de la fuente alta (8x16 con
#       sombra, 5 colores), del marco, de los degrades y las paletas.
#
# Uso: python3 tools/gen_highscore.py
# =============================================================================
import os
import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
IMG = os.path.join(ROOT, 'res', 'images')
SRC = os.path.join(ROOT, 'src')

# Niveles reales del DAC de la Mega Drive (para cuantizar los degrades).
DAC = [0, 52, 87, 116, 144, 172, 206, 255]


def md(rgb):
    """(r,g,b) 0..255 -> palabra de color de la VDP (0x0BGR, pares)."""
    def lv(v):
        return min(range(8), key=lambda i: abs(DAC[i] - v))
    r, g, b = rgb
    return (lv(b) << 9) | (lv(g) << 5) | (lv(r) << 1)


# -----------------------------------------------------------------------------
# PALETA DE TEXTO (PAL0 de las dos pantallas): 1..7 degrade, 8..15 texto.
# -----------------------------------------------------------------------------
C_WHITE, C_ORANGE, C_CYAN, C_DBLUE, C_YELLOW, C_DRED, C_MAGENTA, C_LAV = range(8, 16)
TEXT_RGB = {
    C_WHITE:   (255, 255, 255),
    C_ORANGE:  (222, 134, 0),
    C_CYAN:    (0, 190, 255),
    C_DBLUE:   (0, 0, 213),
    C_YELLOW:  (255, 210, 0),
    C_DRED:    (150, 0, 0),
    C_MAGENTA: (255, 0, 255),
    C_LAV:     (239, 222, 255),
}
# Variantes de la fuente: (relleno, sombra). El orden es el de HsColor (.h).
VARIANTS = [
    ('WHITE',   C_WHITE,   C_ORANGE),
    ('CYAN',    C_CYAN,    C_DBLUE),
    ('YELLOW',  C_YELLOW,  C_DRED),
    ('MAGENTA', C_MAGENTA, C_DRED),
    ('LAV',     C_LAV,     C_MAGENTA),
]
GLYPHS = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.!?-*&"


def font_tiles():
    f = np.array(Image.open(os.path.join(IMG, 'option_menu', 'option_font.png')))
    out = []
    for name, fill, shadow in VARIANTS:
        for ch in GLYPHS:
            k = ord(ch) - 32
            g = f[:, k * 8:(k + 1) * 8] != 0
            tall = np.repeat(g[:7], 2, axis=0)          # 7 filas -> 14
            cell = np.zeros((16, 8), np.uint8)
            sh = np.zeros((16, 8), bool)
            sh[1:15, 1:8] = tall[:, :7]
            cell[sh] = shadow
            cell[:14][tall] = fill
            out.append(cell[:8])
            out.append(cell[8:])
    return out


# -----------------------------------------------------------------------------
# DEGRADES (medidos en las capturas del arcade, franjas de 8 px). Cada fila de
# pixeles elige entre los dos niveles de la VDP mas cercanos (tramado por
# fila), asi el escalon de 3 bits se nota menos.
# -----------------------------------------------------------------------------
def grad_entry(y):          # azul: verde fijo 133, azul 115 -> 213 -> 115
    top = [115, 134, 149, 173, 190, 213]
    if y < 40:
        b = top[y // 8] + (top[y // 8 + 1] - top[y // 8]) * (y % 8) / 8
    elif y < 184:
        b = 213
    else:
        yy = 223 - y
        b = top[yy // 8] + (top[yy // 8 + 1] - top[yy // 8]) * (yy % 8) / 8
    return (0, 144, b)          # 133 del arcade -> nivel exacto 144 (sin tramar)


def grad_board(y):          # verde: 56 -> 120 -> 56
    top = [56, 73, 83, 93, 101, 111, 120]
    yy = y if y < 112 else 223 - y
    if yy < 48:
        g = top[yy // 8] + (top[yy // 8 + 1] - top[yy // 8]) * (yy % 8) / 8
    else:
        g = 120
    return (0, g, 0)


def dac_split(v):
    """valor 0..255 -> (nivel bajo, nivel alto, fraccion hacia el alto)"""
    for i in range(7):
        if DAC[i] <= v <= DAC[i + 1]:
            return i, i + 1, (v - DAC[i]) / (DAC[i + 1] - DAC[i])
    return 7, 7, 0.0


def gradient(fn):
    """-> (28 tiles, colores VDP de los indices 1..7)"""
    rows = []
    for y in range(224):
        r, g, b = fn(y)
        chans = []
        for v in (r, g, b):
            lo, hi, fr = dac_split(v)
            # tramado ordenado por fila (Bayer 1D de 4)
            th = [0.125, 0.625, 0.375, 0.875][y % 4]
            chans.append(hi if fr > th else lo)
        rows.append(tuple(chans))
    colors = sorted(set(rows), key=lambda c: (c[1], c[2]))
    assert len(colors) <= 7, colors
    pal = [(c[2] << 9) | (c[1] << 5) | (c[0] << 1) for c in colors]
    pal += [pal[-1]] * (7 - len(pal))
    tiles = []
    for t in range(28):
        cell = np.zeros((8, 8), np.uint8)
        for k in range(8):
            cell[k, :] = 1 + colors.index(rows[t * 8 + k])
        tiles.append(cell)
    return tiles, pal


# -----------------------------------------------------------------------------
# MARCO del panel (blanco, 2 px, esquinas redondeadas): 8 tiles
#   0 sup-izq  1 sup  2 sup-der  3 izq  4 der  5 inf-izq  6 inf  7 inf-der
# -----------------------------------------------------------------------------
def frame_tiles():
    W = C_WHITE
    t = [np.zeros((8, 8), np.uint8) for _ in range(8)]
    for x in range(8):
        t[1][3:5, x] = W
        t[6][3:5, x] = W
    for y in range(8):
        t[3][y, 3:5] = W
        t[4][y, 3:5] = W
    # esquinas: linea a 3..4 con el codo redondeado
    c = t[0]
    c[3:5, 5:8] = W
    c[5:8, 3:5] = W
    c[4, 4] = W
    for src, dst in ((0, 2), (0, 5), (0, 7)):
        a = t[src].copy()
        if dst in (2, 7):
            a = a[:, ::-1]
        if dst in (5, 7):
            a = a[::-1, :]
        t[dst] = a
    return t


# -----------------------------------------------------------------------------
# RETRATOS
# -----------------------------------------------------------------------------
PANEL_BG = [9, 15, 11, 12]          # color del fondo plano de cada retrato
# Leo y Don llevan el panuelo del mismo color que su fondo: los huecos
# encerrados se borran solo desde esta fila (Mike: todos).
POCKET_Y = [70, 0, 60, 60]


def portraits():
    im = Image.open(os.path.join(IMG, 'hud', 'select player_15 CORES.png'))
    a = np.array(im)
    pal = im.getpalette()[:48]
    out = np.zeros((128, 64 * 4), np.uint8)
    for t in range(4):
        x0 = 13 + 77 * t
        p = a[81:209, x0:x0 + 64].copy()
        bgm = p == PANEL_BG[t]
        # solo el fondo conectado con los bordes (el mismo color puede estar
        # adentro de la tortuga: el panuelo de Leo es azul)
        lab, n = ndimage.label(bgm)
        border = set(np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]]))) - {0}
        for k, sl in enumerate(ndimage.find_objects(lab), 1):
            # huecos de fondo encerrados (entre el arma y el cuerpo): por
            # debajo de POCKET_Y[t] son fondo; arriba es el panuelo
            if k in border or sl[0].start >= POCKET_Y[t]:
                p[lab == k] = 0
        out[:, t * 64:(t + 1) * 64] = p
    o = Image.fromarray(out, 'P')
    o.putpalette(pal + [0] * (768 - len(pal)))
    d = os.path.join(IMG, 'hiscore')
    os.makedirs(d, exist_ok=True)
    o.save(os.path.join(d, 'hs_portraits.png'), transparency=0)
    return pal


def tints(pal):
    """Paletas de un solo tono para la tabla (Leo azul, Mike amarillo, Don
    violeta, Raph rojo): cada color va a su luminancia sobre el tono."""
    hues = [(40, 70, 255), (255, 230, 40), (200, 40, 255), (255, 60, 40)]
    out = []
    for h in hues:
        row = [0]
        for i in range(1, 16):
            r, g, b = pal[i * 3:i * 3 + 3]
            lum = (0.3 * r + 0.59 * g + 0.11 * b) / 255.0
            k = 0.12 + 0.75 * lum
            row.append(md((h[0] * k, h[1] * k, h[2] * k)))
        out.append(row)
    return out


def words(cell):
    w = []
    for y in range(8):
        v = 0
        for x in range(8):
            v = (v << 4) | int(cell[y, x])
        w.append(v)
    return w


def emit_tiles(f, name, tiles):
    f.write('const u32 %s[%d] = {\n' % (name, len(tiles) * 8))
    for c in tiles:
        f.write('    ' + ', '.join('0x%08X' % v for v in words(c)) + ',\n')
    f.write('};\n\n')


def emit_pal(f, name, pal):
    f.write('const u16 %s[16] = { %s };\n' % (name, ', '.join('0x%03X' % v for v in pal)))


def main():
    pal = portraits()
    ft = font_tiles()
    ge, pe = gradient(grad_entry)
    gb, pb = gradient(grad_board)
    fr = frame_tiles()
    text = [md(TEXT_RGB[i]) for i in range(8, 16)]
    tt = tints(pal)
    with open(os.path.join(SRC, 'hiscore_gfx.h'), 'w', newline='\r\n') as f:
        f.write('// GENERADO POR tools/gen_highscore.py -- NO EDITAR A MANO\n')
        f.write('#ifndef _HISCORE_GFX_H_\n#define _HISCORE_GFX_H_\n\n#include <genesis.h>\n\n')
        f.write('#define HS_GLYPHS     %d\n' % len(GLYPHS))
        f.write('#define HS_GLYPH_STR  "%s"\n' % GLYPHS)
        f.write('// Fuente alta: [variante][glifo][arriba, abajo]\n')
        f.write('enum { %s, HS_COLORS };\n' % ', '.join('HS_%s' % v[0] for v in VARIANTS))
        f.write('#define HS_FONT_TILES (HS_COLORS * HS_GLYPHS * 2)\n')
        f.write('extern const u32 hsFontTiles[];\n')
        f.write('#define HS_FRAME_TILES 8\nextern const u32 hsFrameTiles[];\n')
        f.write('extern const u32 hsGradEntryTiles[];   // 28 tiles, uno por fila de pantalla\n')
        f.write('extern const u32 hsGradBoardTiles[];\n')
        f.write('extern const u16 hsPalEntry[16];       // PAL0: degrade + texto\n')
        f.write('extern const u16 hsPalBoard[16];\n')
        f.write('extern const u16 hsPalTint[4][16];     // retratos de la tabla\n')
        f.write('extern const u16 hsPalBlack[16];       // siluetas\n')
        f.write('\n#endif\n')
    with open(os.path.join(SRC, 'hiscore_gfx.c'), 'w', newline='\r\n') as f:
        f.write('// GENERADO POR tools/gen_highscore.py -- NO EDITAR A MANO\n')
        f.write('#include "hiscore_gfx.h"\n\n')
        emit_tiles(f, 'hsFontTiles', ft)
        emit_tiles(f, 'hsFrameTiles', fr)
        emit_tiles(f, 'hsGradEntryTiles', ge)
        emit_tiles(f, 'hsGradBoardTiles', gb)
        emit_pal(f, 'hsPalEntry', [0] + pe + text)
        emit_pal(f, 'hsPalBoard', [0] + pb + text)
        f.write('const u16 hsPalTint[4][16] = {\n')
        for row in tt:
            f.write('    { %s },\n' % ', '.join('0x%03X' % v for v in row))
        f.write('};\n')
        emit_pal(f, 'hsPalBlack', [0] * 16)
    print('fuente %d tiles, degrades %d/%d colores' % (len(ft), len(set(pe)), len(set(pb))))


if __name__ == '__main__':
    main()
