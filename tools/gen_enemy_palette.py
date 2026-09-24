#!/usr/bin/env python3
# =============================================================================
# gen_enemy_palette.py  -  Paleta UNICA de enemigos (PAL2) del 24/09
# =============================================================================
# Gustavo trajo tres sheets nuevos que comparten una misma paleta de 15 colores:
#
#     res/sprites/foot_soldier_purple_13x13.png   (morado, 832x1440)
#     res/sprites/Foot_Soldier_Orange_new.png     (naranja, 416x936)
#     res/sprites/CHAMAS 7 CORES.png              (fuego, 8 frames de banda)
#
# Los tres venian con el BLANCO como color transparente en el ULTIMO indice
# (15 en los soldiers, 7 en el fuego). En Megadrive el indice 0 de un tile es
# el transparente, asi que habia que correr todo: este script deja
#
#     indice 0  = transparente (magenta, nunca se dibuja)
#     indice 1..15 = los 15 colores reales, en el mismo orden del artista
#
# y de paso reescribe TODOS los PNGs que comparten PAL2 (tnt, explosion, tapa,
# sparks, robot, shuriken) contra la paleta nueva, por CERCANIA DE COLOR. Eso
# es lo que hace que el script sea idempotente: un PNG ya remapeado vuelve a
# caer en los mismos indices, asi que correrlo dos veces no rompe nada.
#
# El fuego ademas cambia de formato: ver gen_fire_strip.py.
#
# Uso:  python3 tools/gen_enemy_palette.py
# =============================================================================
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPR = os.path.join(ROOT, "res", "sprites")

PURPLE = os.path.join(SPR, "foot_soldier_purple_13x13.png")
ORANGE = os.path.join(SPR, "Foot_Soldier_Orange_new.png")

# Color del indice 0. No se dibuja nunca (en Megadrive el 0 de un tile es
# transparente); magenta para que cualquier pixel que se escape cante.
TRANSPARENT_RGB = (255, 0, 255)

# PNGs que se dibujan con PAL2 y no llevan paleta propia. Se reindexan por
# cercania contra la paleta nueva. El shuriken entra aca porque el naranja se
# mudo de PAL3 a PAL2 (ahora comparte paleta con el morado).
PAL2_ASSETS = [
    "tnt.png", "explosion.png", "tapa_voladora.png",
    "sparks.png", "sparks_strip.png",
    "sparks_2.png", "sparks_2_strip.png",
    "spark_ascensor.png", "spark_ascensor_strip.png",
    "robot_whip.png", "whip_waves.png",
    "shuriken.png",
]


def load_indexed(path):
    im = Image.open(path)
    if im.mode != "P":
        sys.exit("%s no es indexado" % os.path.basename(path))
    return im, np.asarray(im).astype(np.uint8)


def palette_list(im, n=16):
    p = im.getpalette()
    return [tuple(p[i * 3:i * 3 + 3]) for i in range(n)]


def shift_sheet(path, pal_out=None):
    """Mueve el transparente (blanco, ultimo indice usado) al 0 y corre el
    resto +1. Si el PNG YA esta acomodado (indice 0 = TRANSPARENT_RGB) no lo
    toca. Devuelve la paleta resultante."""
    im, a = load_indexed(path)
    pal = palette_list(im)
    if pal[0] == TRANSPARENT_RGB:
        if im.info.get("transparency") != 0:        # que Aseprite lo vea
            im.save(path, transparency=0)
        return pal                                  # ya acomodado
    used = sorted(set(a.flatten().tolist()))
    white = [i for i in used if pal[i] == (255, 255, 255)]
    if len(white) != 1:
        sys.exit("%s: se esperaba UN solo blanco (el transparente), hay %d"
                 % (os.path.basename(path), len(white)))
    w = white[0]
    reales = [i for i in used if i != w]
    if len(reales) > 15:
        sys.exit("%s usa %d colores reales, no entran en 15"
                 % (os.path.basename(path), len(reales)))

    lut = np.zeros(256, np.uint8)
    newpal = [TRANSPARENT_RGB]
    for n, i in enumerate(reales, start=1):
        lut[i] = n
        newpal.append(pal[i])
    lut[w] = 0
    while len(newpal) < 16:
        newpal.append((0, 0, 0))

    out = Image.fromarray(lut[a], "P")
    flat = [c for rgb in newpal for c in rgb] + [0] * (768 - 48)
    out.putpalette(flat)
    out.save(path, transparency=0)
    print("%-34s reindexado: %d colores + transparente"
          % (os.path.basename(path), len(reales)))
    return newpal


def remap_to(path, pal):
    """Reindexa un PNG contra 'pal' por cercania de color. El transparente
    (indice 0 del origen, o el blanco si el PNG lo declara) va al 0."""
    im, a = load_indexed(path)
    src = palette_list(im)
    tr = im.info.get("transparency", 0)
    if isinstance(tr, bytes):
        tr = 0
    target = np.array(pal[1:], dtype=np.int32)      # 1..15, el 0 no compite

    lut = np.zeros(256, np.uint8)
    for i in range(16):
        if i == tr:
            lut[i] = 0
            continue
        d = ((target - np.array(src[i], dtype=np.int32)) ** 2).sum(axis=1)
        lut[i] = int(d.argmin()) + 1

    out = Image.fromarray(lut[a], "P")
    flat = [c for rgb in pal for c in rgb] + [0] * (768 - 48)
    out.putpalette(flat)
    out.info["transparency"] = 0
    out.save(path, transparency=0)
    print("%-34s remapeado a la paleta nueva" % os.path.basename(path))


# Fila 14 del morado (el agarre por la espalda) todavia no esta dibujada. Si
# queda 100% transparente rescomp BORRA la fila entera y todas las de abajo se
# corren una (ENEMY_ANIM_VOLTERETA pasaria a ser la dinamita). El sheet del
# 24/09 vino sin el punto placeholder que tenia el anterior, asi que se lo
# vuelve a poner: 2x2 px en el frame 0, donde estaba antes. No se ve nunca,
# porque esa animacion no se reproduce.
GRAB_ROW_Y = 14 * 80
GRAB_DOT = [(59, 20), (59, 21), (60, 20), (60, 21)]
GRAB_DOT_IDX = 2          # (160,0,160), el morado medio de la paleta nueva


def fix_grab_row(path):
    im, a = load_indexed(path)
    band = a[GRAB_ROW_Y:GRAB_ROW_Y + 80]
    if band.any():
        return
    a = a.copy()
    for y, x in GRAB_DOT:
        a[GRAB_ROW_Y + y, x] = GRAB_DOT_IDX
    out = Image.fromarray(a, "P")
    out.putpalette(im.getpalette())
    out.save(path, transparency=0)
    print("%-34s fila 14 (agarre) vacia -> punto placeholder"
          % os.path.basename(path))


def main():
    pal = shift_sheet(PURPLE)
    fix_grab_row(PURPLE)
    palo = shift_sheet(ORANGE)
    if pal[:16] != palo[:16]:
        sys.exit("El morado y el naranja NO quedaron con la misma paleta:\n"
                 "  %s\n  %s" % (pal, palo))
    print("Paleta compartida:")
    for i, c in enumerate(pal):
        print("   %2d %s%s" % (i, c, "   <- transparente" if i == 0 else ""))

    for name in PAL2_ASSETS:
        p = os.path.join(SPR, name)
        if os.path.exists(p):
            remap_to(p, pal)
        else:
            print("%-34s NO EXISTE, salteado" % name)


if __name__ == "__main__":
    main()
