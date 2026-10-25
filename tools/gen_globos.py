#!/usr/bin/env python3
# =============================================================================
# gen_globos.py  -  Globos de dialogo del arcade (11/10)
# =============================================================================
# Entrada: res/images/globos/globos_arcade.png -- la hoja con TODOS los globos
#          del arcade (fondo magenta 255,0,255; 4 colores: relleno, borde,
#          texto y sombra del texto).
# Salida:  res/sprites/<nombre>_baloon.png, uno por globo pedido en GLOBOS.
#
# Mismo remapeo que los globos que ya estaban (help_baloon, light_out...): se
# dibujan con la paleta de las TORTUGAS (PAL1), asi que cada color va a un
# indice fijo de esa paleta:
#     relleno 11   borde 9   texto 8   sombra del texto 7   (0 = transparente)
# La paleta del PNG de salida se copia de help_baloon.png (la de PAL1).
#
# Para sumar un globo: agregar (nombre, x, y, w, h) a GLOBOS -- la caja del
# globo en la hoja -- y declararlo en el .res del nivel con "8 4 NONE 0"
# (64x32), "12 4" (96x32) o "16 4" (128x32).
# =============================================================================
import os
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHEET = os.path.join(ROOT, "res", "images", "globos", "globos_arcade.png")
SPR = os.path.join(ROOT, "res", "sprites")
PAL_REF = os.path.join(SPR, "help_baloon.png")

GLOBOS = [
    ("do_not_resist", 368, 88, 64, 32),   # el robot del latigo del 1-1
    ("pizza_time", 80, 128, 64, 32),      # la pizza de la cornisa (2-1)
    ("youre_dead", 80, 88, 64, 32),       # Bebop al llegar (2-1)
    ("any_prettier", 224, 208, 96, 32),   # la tortuga le contesta
    ("watch_mouth", 328, 208, 96, 32),    # Bebop cierra
    ("im_invincible", 224, 88, 64, 32),   # (23/10) Krang (9-1): al caer y al escaparse la cabeza
    ("cork_screw", 144, 288, 128, 32),    # (25/10) Leo al salir la capsula del taladro (garage): "Oh,no.it's that overgrown cork-screw again"
]

# (24/10) Globos ARMADOS: el arcade no tiene la frase. Se usa un globo de la
# hoja como plantilla (se le borra el texto) y se escribe con letras
# recortadas de otros globos de la misma hoja (la fuente del arcade: x-height
# 5, mayusculas 7, descendentes 2; 1 px entre letras, 3 de espacio).
# GLYPHS: letra -> (x0, top, x1) en la hoja: columnas x0..x1 y 9 filas desde
# 'top' (la fila de arriba de las mayusculas de esa linea).
GLYPHS = {
    "T": (154, 94, 158),     # "Tonight I dine"
    "i": (169, 94, 169),
    "e": (205, 94, 208),
    "o": (155, 104, 158),    # "on turtle soup"
    "n": (160, 104, 163),
    "t": (166, 104, 169),
    "u": (171, 104, 174),
    "p": (209, 104, 212),
    "m": (234, 100, 238),    # "I'm invincible!"
    "c": (262, 100, 265),
    "!": (282, 100, 282),
    "y": (395, 140, 399),    # "I owe you one"
}
SPACE_W = 3
# (nombre, plantilla (x, y, w, h), [(texto, x, top, alineacion)])
#   alineacion "l": x = columna de la primera letra; "r": x = la ultima.
COMPUESTOS = [
    # (24/10) Tora (Central Park) al entrar: "Time to put you on ice!".
    # Plantilla: "They aint getting any prettier!" (96x32, punta en x ~71).
    ("tora_ice", (224, 208, 96, 32),
     [("Time to put you", 5, 6, "l"), ("on ice!", 90, 16, "r")]),
]

COLMAP = {
    (123, 123, 214): 11,   # relleno
    (82, 231, 255): 9,     # borde
    (255, 239, 0): 8,      # texto
    (189, 173, 49): 7,     # sombra del texto
}


def text_w(txt):
    w = 0
    for ch in txt:
        w += SPACE_W if ch == " " else (GLYPHS[ch][2] - GLYPHS[ch][0] + 1)
        w += 1
    return w - 1


def compose(a, tpl, lines):
    x, y, w, h = tpl
    cell = a[y:y + h, x:x + w].copy()
    fill = np.array((123, 123, 214), np.uint8)
    for c in ((255, 239, 0), (189, 173, 49)):          # sin el texto de la plantilla
        cell[(cell == c).all(-1)] = fill
    for txt, lx, top, al in lines:
        cx = lx if al == "l" else lx - text_w(txt) + 1
        for ch in txt:
            if ch == " ":
                cx += SPACE_W + 1
                continue
            gx0, gtop, gx1 = GLYPHS[ch]
            g = a[gtop:gtop + 9, gx0:gx1 + 1]
            for yy in range(9):
                for xx in range(gx1 - gx0 + 1):
                    c = tuple(int(v) for v in g[yy, xx])
                    if c in ((255, 239, 0), (189, 173, 49)):
                        cell[top + yy, cx + xx] = c
            cx += gx1 - gx0 + 2
    return cell


def save_cell(cell, name, pal):
    h, w = cell.shape[:2]
    out = np.zeros((h, w), np.uint8)
    for yy in range(h):
        for xx in range(w):
            out[yy, xx] = COLMAP.get(tuple(int(v) for v in cell[yy, xx]), 0)
    im = Image.fromarray(out, "P")
    im.putpalette(pal)
    p = os.path.join(SPR, name + "_baloon.png")
    im.save(p, transparency=0)
    print(os.path.basename(p), w, h)


def main():
    a = np.asarray(Image.open(SHEET).convert("RGB"))
    pal = Image.open(PAL_REF).getpalette()
    for name, tpl, lines in COMPUESTOS:
        save_cell(compose(a, tpl, lines), name, pal)
    for name, x, y, w, h in GLOBOS:
        cell = a[y:y + h, x:x + w]
        out = np.zeros((h, w), np.uint8)
        for yy in range(h):
            for xx in range(w):
                c = tuple(int(v) for v in cell[yy, xx])
                out[yy, xx] = COLMAP.get(c, 0)
        im = Image.fromarray(out, "P")
        im.putpalette(pal)
        p = os.path.join(SPR, name + "_baloon.png")
        im.save(p, transparency=0)
        print(os.path.basename(p), w, h)


if __name__ == "__main__":
    main()
