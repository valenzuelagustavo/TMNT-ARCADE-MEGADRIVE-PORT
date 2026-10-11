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
]

COLMAP = {
    (123, 123, 214): 11,   # relleno
    (82, 231, 255): 9,     # borde
    (255, 239, 0): 8,      # texto
    (189, 173, 49): 7,     # sombra del texto
}


def main():
    a = np.asarray(Image.open(SHEET).convert("RGB"))
    pal = Image.open(PAL_REF).getpalette()
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
