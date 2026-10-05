#!/usr/bin/env python3
# ============================================================================
# gen_level2_1_limits.py -- Limites caminables del nivel 2-1 (la calle)
# ============================================================================
# ENTRADA
#   res/images/lvl_2_scene/Stage 2-_LIMITES.png
#       El MISMO arte que "Stage 2-_16_colors_v2.png" con dos anotaciones
#       pintadas encima (16/09/2026):
#
#       * MAGENTA (255,0,128) = "LIMITE DE PARED". Es la franja que separa lo
#         caminable de lo que no: TODO lo que queda por ENCIMA del pixel
#         magenta mas bajo de esa columna es pared. NO esta pintada la pared
#         entera, solo la linea/franja del limite, asi que lo que importa de
#         cada columna es el BORDE INFERIOR del magenta.
#
#       * UNA CAJA AZUL OSCURA (41,66,99) con el texto "ACA ES UNA PLATAFORMA
#         QUE SE PUEDE PISAR / SE PUEDE SUBIR A ELLA SALTANDO HACIA ARRIBA /
#         SE PUEDE BAJAR CAMINANDO HACIA ABAJO". Esta puesta EXACTAMENTE sobre
#         la cornisa de los portones, y su rectangulo es la plataforma.
#
#   res/images/lvl_2_scene/Stage 2-_16_colors_v2.png
#       El arte sin anotar. De aca sale el limite DE ADELANTE: debajo de la
#       calle no hay nada dibujado (indice 0), asi que el ultimo pixel con
#       arte de cada columna es el borde de la vereda/cuneta.
#
# SALIDA
#   src/level2_1_limits.h / .c
#       lvl21WalkTop[] y lvl21WalkBot[]: para cada bloque de LVL21_LIM_STEP px
#       de X, el primer y el ultimo Y de PIES caminables. Una columna sin
#       calle queda con top > bot (imposible de satisfacer).
#       Mas las constantes de la plataforma.
#
# POR QUE UNA TABLA Y NO UN POLIGONO
#   La version anterior aproximaba la calle con tres piezas a mano (dos
#   rectangulos y una diagonal de 45 grados) y numeros de calibracion. La
#   tabla sale directo del dibujo, sigue los escalones reales de las fachadas
#   y cuesta 2 x 320 x 2 = 1280 bytes de ROM.
# ============================================================================

import os, sys
from PIL import Image
import numpy as np

HERE   = os.path.dirname(os.path.abspath(__file__))
ROOT   = os.path.dirname(HERE)
IMGDIR = os.path.join(ROOT, "res", "images", "lvl_2_scene")
LIMPNG = os.path.join(IMGDIR, "Stage 2-_LIMITES.png")
ARTPNG = os.path.join(IMGDIR, "Stage 2-_16_colors_v2.png")
SRCDIR = os.path.join(ROOT, "src")

STEP          = 8      # granularidad en X de la tabla (1 tile)
FRONT_MARGIN  = 5      # px que se le restan al ultimo pixel con arte (cuneta)
MAGENTA       = (255, 0, 128)
BOX_RGB       = (41, 66, 99)    # caja de texto azul de la plataforma
BOX_MIN_RUN   = 120             # px seguidos para considerar que es la caja

# Margenes de la propia plataforma: la caja dibujada es un cartel,
# no una medicion al pixel. Se le come un poco arriba y abajo para que los
# pies queden sobre la cornisa y no flotando en el aire ni metidos en la pared.
PLAT_INSET_TOP = 1
PLAT_INSET_BOT = 1

# TECHO DE LA FRANJA DE PIES, y el numero mas importante de este archivo.
#
# La camara de este nivel NUNCA SUBE de LVL21_CAM_Y_MIN (32): el plano solo se
# revela hacia abajo y hacia la derecha, asi que por encima de esa fila no hay
# nada dibujado. Un personaje mide PLAYER_FOOT_OFFSET (64) px por encima de
# sus pies, o sea que para que entre ENTERO en pantalla sus pies no pueden
# estar mas arriba de 32 + 64 = 96.
#
# La caja dibujada arranca en y=76: parado ahi, sobre el filo de arriba de
# la cornisa, a la tortuga se le va la cabeza fuera de la pantalla (probado en
# emulador el 16/09). Asi que la franja jugable de la cornisa se recorta a
# y >= 96, que cae justo sobre la moldura clara de abajo: la tortuga queda
# parada en el borde de la cornisa, entera y bien visible.
#
# Si alguna vez la camara pudiera subir mas (regenerando el fondo con
# CAM_Y_MIN mas chico), este numero baja y la cornisa se vuelve mas profunda.
PLAT_FEET_MIN = 96


def index_of(pal, rgb):
    for i in range(len(pal) // 3):
        if tuple(pal[i * 3:i * 3 + 3]) == rgb:
            return i
    return -1


def main():
    lim = Image.open(LIMPNG)
    art = Image.open(ARTPNG)
    if lim.mode != "P" or art.mode != "P":
        sys.exit("Los dos PNG tienen que ser indexados")
    palL = lim.getpalette()
    L = np.array(lim)
    A = np.array(art)
    if L.shape != A.shape:
        sys.exit("LIMITES y el arte tienen que medir lo mismo: %s vs %s"
                 % (L.shape, A.shape))

    # El PNG v2 mide 2560x722 pero el arte vive en las filas 0..639 (las de
    # abajo son sobra del export). Se recorta a filas completas de tiles.
    rows_used = int(np.nonzero((A != 0).any(axis=1))[0].max()) + 1
    H = ((rows_used + 7) // 8) * 8
    L, A = L[:H], A[:H]
    W = L.shape[1]
    print("mundo: %dx%d" % (W, H))

    iMag = index_of(palL, MAGENTA)
    iBox = index_of(palL, BOX_RGB)
    if iMag < 0:
        sys.exit("no encontre el magenta %s en la paleta de LIMITES" % (MAGENTA,))
    MAG = (L == iMag)
    ART = (A != 0)

    # ---------------------------------------------------------------- pared
    # Borde INFERIOR del magenta por columna. El texto blanco
    # escrito adentro de la franja deja agujeros, pero como se toma el pixel
    # magenta MAS BAJO de la columna no molesta.
    wall = np.full(W, -1, np.int32)
    for x in range(W):
        nz = np.nonzero(MAG[:, x])[0]
        if len(nz):
            wall[x] = nz.max()
    holes = int((wall < 0).sum())
    # Columnas sin magenta: se interpola linealmente entre las vecinas que si
    # tienen (en el PNG del 16/09 es un solo hueco de 9 px en la esquina).
    idx = np.nonzero(wall >= 0)[0]
    if len(idx) == 0:
        sys.exit("no hay magenta en la imagen")
    wall = np.interp(np.arange(W), idx, wall[idx]).astype(np.int32)
    print("columnas sin magenta (interpoladas):", holes)

    # -------------------------------------------------------------- adelante
    # Ultimo pixel con arte de la columna: debajo de la calle no hay nada
    # dibujado, asi que ese es el borde de la vereda.
    front = np.full(W, -1, np.int32)
    for x in range(W):
        nz = np.nonzero(ART[:, x])[0]
        if len(nz):
            front[x] = nz.max()

    top = wall + 1
    bot = front - FRONT_MARGIN

    # ------------------------------------------------------------ plataforma
    # La caja de texto azul: filas con una corrida larga de BOX_RGB.
    if iBox < 0:
        sys.exit("no encontre el azul de la caja %s" % (BOX_RGB,))
    BOX = (L == iBox)
    rows = []
    for y in range(H):
        row = BOX[y]
        best = cur = 0
        for v in row:
            cur = cur + 1 if v else 0
            if cur > best:
                best = cur
        if best >= BOX_MIN_RUN:
            rows.append(y)
    if not rows:
        sys.exit("no encontre la caja de la plataforma")
    y0, y1 = min(rows), max(rows)
    # El azul de la caja TAMBIEN esta en el arte (las ventanas), asi que no
    # alcanza con el bounding box de todo el color. La PRIMERA fila de la caja
    # no tiene texto encima, asi que ahi la caja es una corrida solida: se
    # busca la corrida contigua mas larga de esa fila.
    def longest_run(row):
        best = (0, 0, 0)
        s = None
        for x in range(len(row) + 1):
            if x < len(row) and row[x]:
                if s is None:
                    s = x
            elif s is not None:
                if x - s > best[0]:
                    best = (x - s, s, x - 1)
                s = None
        return best
    run = longest_run(BOX[y0])
    if run[0] < BOX_MIN_RUN:
        sys.exit("la caja de la plataforma quedo demasiado corta: %s" % (run,))
    x0, x1 = run[1], run[2]
    # Alto real de la caja, medido en su columna del medio.
    xm = (x0 + x1) // 2
    y1 = y0
    while y1 + 1 < H and BOX[y1 + 1, xm]:
        y1 += 1
    py0, py1 = y0 + PLAT_INSET_TOP, y1 - PLAT_INSET_BOT
    box0 = py0
    if py0 < PLAT_FEET_MIN:
        py0 = PLAT_FEET_MIN
    if py0 > py1:
        sys.exit("la cornisa quedo sin franja jugable: %d..%d" % (py0, py1))
    print("plataforma: x %d..%d  caja y %d..%d  franja de pies y %d..%d"
          % (x0, x1, box0, py1, py0, py1))

    # ----------------------------------------------------------- compactado
    cols_n = W // STEP
    wt = np.zeros(cols_n, np.int32)
    wb = np.zeros(cols_n, np.int32)
    blocked = 0
    for i in range(cols_n):
        s = slice(i * STEP, (i + 1) * STEP)
        t = int(top[s].max())          # el limite MAS restrictivo del bloque
        b = int(bot[s].min())
        if front[s].max() < 0 or t > b:
            t, b = 1, 0                # bloque sin calle
            blocked += 1
        wt[i], wb[i] = t, b
    print("bloques sin calle: %d / %d" % (blocked, cols_n))
    ok = [(int(wt[i]), int(wb[i])) for i in range(cols_n) if wt[i] <= wb[i]]
    print("ancho de calle: min %d  max %d"
          % (min(b - t for t, b in ok) + 1, max(b - t for t, b in ok) + 1))

    ymin = min(min(t for t, _ in ok), py0)
    ymax = max(b for _, b in ok)
    xmin = min(i for i in range(cols_n) if wt[i] <= wb[i]) * STEP
    xmax = (max(i for i in range(cols_n) if wt[i] <= wb[i]) + 1) * STEP - 1
    print("lane util: y %d..%d   x %d..%d" % (ymin, ymax, xmin, xmax))
    with open(os.path.join(SRCDIR, "level2_1_limits.h"), "w") as f:
        f.write(HEADER_H % dict(step=STEP, cols=cols_n, w=W, h=H,
                                px0=x0, px1=x1, py0=py0, py1=py1,
                                ymin=ymin, ymax=ymax, xmin=xmin, xmax=xmax))
    with open(os.path.join(SRCDIR, "level2_1_limits.c"), "w") as f:
        f.write('#include "level2_1_limits.h"\n\n')
        for name, tab in (("lvl21WalkTop", wt), ("lvl21WalkBot", wb)):
            f.write("const u16 %s[LVL21_LIM_COLS] = {\n" % name)
            for i in range(0, cols_n, 16):
                f.write("    " + ",".join("%d" % v for v in tab[i:i + 16]) + ",\n")
            f.write("};\n\n")
    print("listo")


HEADER_H = """// GENERADO POR tools/gen_level2_1_limits.py -- NO EDITAR A MANO
// Sale de res/images/lvl_2_scene/"Stage 2-_LIMITES.png" (el magenta pintado
// = limite de pared) + el arte v2 (el ultimo pixel dibujado de cada
// columna = borde de la vereda).
#ifndef _LEVEL2_1_LIMITS_H_
#define _LEVEL2_1_LIMITS_H_

#include <genesis.h>

#define LVL21_LIM_STEP   %(step)d     // px de X que cubre cada entrada
#define LVL21_LIM_COLS   %(cols)d
#define LVL21_WORLD_W    %(w)d
#define LVL21_WORLD_H    %(h)d

// Para cada bloque de LVL21_LIM_STEP px en X: primer y ultimo Y de PIES
// caminable. Un bloque sin calle tiene top > bot (no hay Y que lo cumpla).
extern const u16 lvl21WalkTop[LVL21_LIM_COLS];
extern const u16 lvl21WalkBot[LVL21_LIM_COLS];

// La cornisa sobre los portones: se sube saltando y se baja caminando hacia
// abajo (te dejas caer a la calle). Fuera de este rango de X no existe.
#define LVL21_PLAT_X0    %(px0)d
#define LVL21_PLAT_X1    %(px1)d
#define LVL21_PLAT_Y0    %(py0)d
#define LVL21_PLAT_Y1    %(py1)d

// Extremos de TODA el area util (plataforma incluida). Son los que se le
// pasan a setPlayerLane/setEnemyBounds; el recorte fino lo hace la tabla.
#define LVL21_WALK_Y_MIN %(ymin)d
#define LVL21_WALK_Y_MAX %(ymax)d
#define LVL21_WALK_X_MIN %(xmin)d
#define LVL21_WALK_X_MAX %(xmax)d

#endif
"""

if __name__ == "__main__":
    main()
