# -*- coding: utf-8 -*-
"""
gen_chamas.py - (26/09) Recorta de res/sprites/chamas-sheet.png (arte nuevo,
paleta = la del foot soldier naranja = PAL2 del 1-1) las dos
animaciones que se usan en el nivel 1-1:

  floor_fire_gen.png  FUEGO DEL PISO, decorativo. 9 frames de 56x32 en fila
                      (7x4 tiles). En la hoja estan en y 330..361, cada uno
                      arrancando en la X de FLOOR_X0 (el charco gris de abajo
                      calza en esa X en los 9: medido). Reemplaza a sparks_2.
                      La forma de la llama cambia de frame a frame, asi que
                      NO se streamea: es un SPRITE comun con auto-animacion.

  door_fire_gen.png   FUEGO EN EL HUECO DE LA PUERTA que rompe el foot
                      soldier. 7 frames de 40x80 en fila (5x10 tiles); el arte
                      son 33x79 (marco marron + interior negro + llamas + piso)
                      y va en la esquina de arriba a la izquierda. Mide EXACTO
                      lo que el hueco del fondo (33x79, x = centro-14,
                      y 49..127). Reemplaza a sparks (puertas). El interior
                      negro es opaco (indice 15), asi que los 50 tiles estan
                      llenos en los 7 frames: se puede seguir streameando al
                      bloque compartido como antes.

Los indices se copian tal cual (la hoja ya esta en la paleta de PAL2).

Uso:  python3 tools/gen_chamas.py
"""
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPR = os.path.join(ROOT, "res", "sprites")
SRC = os.path.join(SPR, "chamas-sheet.png")

FLOOR_X0 = [116, 175, 234, 294, 354, 412, 471, 528, 585]
FLOOR_Y0, FLOOR_H = 330, 32
FLOOR_CELL_W = 56

DOOR_X0 = [138 + 35 * i for i in range(7)]
DOOR_Y0, DOOR_W, DOOR_H = 378, 33, 79
DOOR_CELL_W, DOOR_CELL_H = 40, 80


def main():
    im = Image.open(SRC)
    if im.mode != "P":
        sys.exit("chamas-sheet.png tiene que ser indexado")
    a = np.asarray(im).astype(np.uint8)
    pal = im.getpalette()[:48] + [0] * (768 - 48)

    def save(arr, name):
        out = Image.fromarray(arr, "P")
        out.putpalette(pal)
        out.save(os.path.join(SPR, name), transparency=0)
        print("%s  %dx%d" % (name, arr.shape[1], arr.shape[0]))

    # --- fuego del piso ---
    floor = np.zeros((FLOOR_H, FLOOR_CELL_W * len(FLOOR_X0)), np.uint8)
    for i, x0 in enumerate(FLOOR_X0):
        cell = a[FLOOR_Y0:FLOOR_Y0 + FLOOR_H, x0:x0 + FLOOR_CELL_W].copy()
        # Solo el frame: que no se cuele el vecino por la derecha.
        cols = np.where(cell.any(0))[0]
        gap = np.where(np.diff(cols) > 1)[0]
        if len(gap):
            cell[:, cols[gap[0]] + 1:] = 0
        if a[FLOOR_Y0 + FLOOR_H:FLOOR_Y0 + FLOOR_H + 4, x0:x0 + 53].any():
            sys.exit("el fuego del piso %d sigue debajo de y=%d" % (i, FLOOR_Y0 + FLOOR_H))
        floor[:, i * FLOOR_CELL_W:(i + 1) * FLOOR_CELL_W] = cell
    save(floor, "floor_fire_gen.png")

    # --- fuego de la puerta ---
    door = np.zeros((DOOR_CELL_H, DOOR_CELL_W * len(DOOR_X0)), np.uint8)
    for i, x0 in enumerate(DOOR_X0):
        cell = a[DOOR_Y0:DOOR_Y0 + DOOR_H, x0:x0 + DOOR_W]
        if (cell == 0).any():
            sys.exit("el fuego de puerta %d tiene huecos transparentes" % i)
        door[0:DOOR_H, i * DOOR_CELL_W:i * DOOR_CELL_W + DOOR_W] = cell
    save(door, "door_fire_gen.png")


if __name__ == "__main__":
    main()
