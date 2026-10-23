#!/usr/bin/env python3
# =============================================================================
# gen_level_snow.py (21/10) -- Scene 3-1: Central Park con nieve (port de NES)
# =============================================================================
# PRUEBA del fondo que mando el artista para medir el scroll, el alto de la
# fase, como se mueven los sprites encima, los colores y la nieve cayendo.
#
# FUENTE (exportada de Aseprite, res/images/lvl_central_park/):
#   FASE CENTRAL PARK_TESTE CORES-sheet.png   2048x234, indexada, 15 colores
#                       (indices 1..15; el 0 es magenta y no se usa).
#   Franjas: cielo 0-32, arboles 32-80, cerca 80-88, rio helado 88-136,
#   piso nevado 136-234.
#
# 234 no es multiplo de 8: se RECORTAN las 2 ultimas filas del piso -> 232
# (29 filas de tile) y la camara vertical baja 8 px (camYMax). (21/10) Antes
# se completaba a 240 repitiendo filas abajo, pero la camara bajaba 16 px y
# se comia el cielo de arriba.
#
# El fondo tiene ~4700 tiles unicos: NO entra en un IMAGE (11 bits) ni en el
# formato ancho de 12 bits; va con indice de 13 bits (stage_raw max_tiles
# 8192, SBG_RAW_INDEX_MASK 0x1FFF) y stage_bg con SBG_MAX_TILES >= el total.
#
# NIEVE: un patron de 32x32 celdas (256x256 px) de copos sueltos que se repite
# en el plano BG_A (64x32) y baja con el scroll vertical del plano (ver aPat
# en stage_level.h). Los copos son SNOW_TILES tiles propios (extraTiles del
# nivel) con los colores del fondo: 1 blanco, 15 celeste claro, 5 gris.
#
# SALIDAS:
#   res/images/lvl_central_park/bg_snow_md.png      2048x232 (PALETTE de rescomp)
#   res/images/lvl_central_park/bg_snow_tiles.bin   formato ancho de stage_bg
#   res/images/lvl_central_park/bg_snow_map.bin
#   res/images/lvl_central_park/snow_flakes.bin     los tiles de los copos (4bpp)
#   src/level_snow_bg.h                             medidas + patron de la nieve
#
# Uso: python3 tools/gen_level_snow.py
# =============================================================================
import os, random
import numpy as np
from PIL import Image
import stage_raw
import snow_simplify

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = os.path.join(ROOT, 'res', 'images', 'lvl_central_park')
SRC = 'FASE CENTRAL PARK_TESTE CORES-sheet.png'
H_OUT = 232
# (21/10) Filas de arriba que se dejan vacias. Se probo con 32 (suponiendo que
# quedaban tapadas por el HUD) y desaparecieron el cielo y los edificios: el
# HUD en WINDOW es transparente fuera de los marcos y el fondo se ve detras.
# Queda en 0: se carga la imagen entera.
HIDE_ROWS = 0

# (21/10) PISO SIMPLIFICADO. El piso nevado del arte (y >= FLOOR_Y0) es un
# patron que se repite cada ~119 px: como 119 no es multiplo de 8, cada
# repeticion cae corrida en la grilla de tiles y la VDP la ve como dibujo
# nuevo (546 tiles distintos por pantalla). Aca se toma un tramo del piso del
# propio arte de FLOOR_PERIOD px (multiplo de 8) y se repite EXACTO a lo
# ancho; el corte se elige donde mejor empalma y donde no hay objetos.
# Los tiles con colores que no son de la nieve (contornos negros, verdes: el
# cantero, etc.) y uno de margen alrededor se dejan como estaban.
#   FLOOR_PERIOD 120: peor caso 849 (los circulos de hielo quedan regulares)
#   FLOOR_PERIOD 240: peor caso 1005 (casi igual al original)
#   FLOOR_PERIOD 0:   el arte tal cual (1116)
FLOOR_Y0 = 136
FLOOR_PERIOD = 120
# (21/10) Arriba del piso (ver tools/snow_simplify.py):
#   SIMPLIFY_TREES  arboles + faroles cada 72 px (eran 74) y los barrotes de
#                   la baranda de arriba cada 12 (eran 11). 282 -> 137 tiles.
#   SIMPLIFY_RIVER  el agua con alambrado repetida cada 112 px (la malla va
#                   cada 14) y el agua abierta de la baranda baja cada 72 con
#                   los barrotes cada 24 (eran 21). Cartel, canteros y postes
#                   grandes intactos.
SIMPLIFY_TREES = True
SIMPLIFY_RIVER = True

# (22/10) NIVEL MAS LARGO. El arte tiene un CICLO que se repite: un tramo de
# alambrado (con el cartel LAP CLAP) y uno de baranda baja (con el cantero);
# arrancan en x 1, 393, 780, 1184, 1571, 1975. Se insertan EXTRA_CYCLES copias
# del bloque CYCLE (un ciclo entero, 792 px: multiplo de 8, asi las copias
# reusan los mismos tiles) en INSERT_X, el mismo punto del ciclo siguiente
# (3-4 px antes de que empiece un alambrado): la costura cae en lugares
# equivalentes. Las capas que se repiten (arboles, agua, piso) se rearman
# despues sobre el ancho nuevo, y el cielo (se repite cada SKY_P px, 98,5%)
# se rearma parejo para que los edificios no salten en la union.
#   EXTRA_CYCLES 0 = el largo del arte (2048), 1 = 2840, 2 = 3632...
EXTRA_CYCLES = 1
CYCLE = (776, 1568)
INSERT_X = 1568
SKY_Y1 = 29          # filas del cielo (las cabezas de los faroles van debajo)
SKY_P = 448

# (22/10) FRANJA CAMINABLE. Los pies van de WALK_Y_MIN (el piso nevado arranca
# en 136, abajo del rio) para abajo. Los OBJETOS del piso (los canteros) no se
# pisan: en sus columnas el tope pasa a ser su borde de abajo + OBST_MARGIN, y
# desde el costado son pared (stage_level revierte el paso). El borde se mide
# en el dibujo: el pixel mas bajo con un color que no es de la nieve (GROUND)
# entre OBST_Y0 y OBST_Y1 (la orilla del rio llega hasta 135 y no cuenta).
WALK_Y_MIN = 144
OBST_MARGIN = 4
OBST_Y0, OBST_Y1 = 137, 200
GROUND = {15, 1, 5, 9, 10, 11}     # celeste, blanco, gris y los tres marrones


def simplify_floor(im):
    """im: array (H, W) de indices. Devuelve (copia simplificada, x del corte)."""
    H, W = im.shape
    P, Y0 = FLOOR_PERIOD, FLOOR_Y0
    assert P % 8 == 0 and Y0 % 8 == 0
    TR0, TR, TC = Y0 // 8, H // 8, W // 8
    obj = np.zeros((TR, TC), bool)
    for r in range(TR0, TR):
        for c in range(TC):
            if not set(np.unique(im[r * 8:r * 8 + 8, c * 8:c * 8 + 8]).tolist()) <= GROUND:
                obj[r, c] = True
    keep = obj.copy()
    for r, c in zip(*np.nonzero(obj)):
        keep[max(TR0, r - 1):r + 2, max(0, c - 1):c + 2] = True
    floor = im[Y0:]
    best = None
    for x0 in range(1, W - P - 8):
        if keep[TR0:, x0 // 8:(x0 + P) // 8 + 1].any():
            continue
        m = ((floor[:, x0 + P] == floor[:, x0]).mean()
             + (floor[:, x0 + P - 1] == floor[:, x0 - 1]).mean())
        if best is None or m > best[0]:
            best = (m, x0)
    assert best, 'no hay un tramo de piso sin objetos'
    x0 = best[1]
    sl = floor[:, x0:x0 + P]
    out = im.copy()
    for r in range(TR0, TR):
        for c in range(TC):
            if keep[r, c]:
                continue
            xs = np.arange(c * 8, c * 8 + 8)
            out[r * 8:r * 8 + 8, c * 8:c * 8 + 8] = sl[r * 8 - Y0:r * 8 - Y0 + 8][:, xs % P]
    return out, x0, int(keep[TR0:].sum())

# --- Copos: 8x8, (x, y, color) por pixel. Pocos pixeles: sobre el cielo y el
# rio celeste el blanco se lee; sobre el piso nevado casi no (como debe ser).
WHITE, ICE, GREY = 1, 15, 5
FLAKES = [
    [(3, 3, WHITE)],                                         # punto
    [(5, 1, WHITE)],                                         # punto (otro lugar)
    [(1, 5, ICE)],                                           # punto celeste
    [(2, 2, WHITE), (3, 2, WHITE), (2, 3, WHITE), (3, 3, ICE)],      # 2x2
    [(5, 4, WHITE), (6, 4, ICE), (5, 5, ICE)],               # 2x2 roto
    [(3, 1, ICE), (2, 2, ICE), (3, 2, WHITE), (4, 2, ICE), (3, 3, ICE)],  # cruz
    [(6, 6, GREY), (6, 5, WHITE)],                           # 1x2 con sombra
    [(1, 1, WHITE), (5, 6, ICE)],                            # dos sueltos
]
PAT_W, PAT_H = 32, 32          # celdas (divide al plano de 64x32)
PAT_DENSITY = 0.10             # fraccion de celdas con copo
SEED = 1987


def main():
    src = Image.open(os.path.join(D, SRC))
    assert src.mode == 'P', 'la fuente tiene que estar indexada'
    W, H = src.size
    assert W % 8 == 0 and H >= H_OUT
    pal = list(src.getpalette()[:48])
    pal[0:3] = [0, 0, 0]        # el 0 (magenta, sin usar) = negro de backdrop
    a = np.array(src)[:H_OUT]
    if EXTRA_CYCLES:
        c0, c1 = CYCLE
        assert (c1 - c0) % 8 == 0 and INSERT_X % 8 == 0
        block = a[:, c0:c1]
        a = np.hstack([a[:, :INSERT_X]] + [block] * EXTRA_CYCLES + [a[:, INSERT_X:]])
        sky = a[:SKY_Y1, :SKY_P].copy()
        for x in range(a.shape[1]):
            a[:SKY_Y1, x] = sky[:, x % SKY_P]
        print('nivel: %d -> %d px (%d ciclo(s) de %d px en x=%d)'
              % (W, a.shape[1], EXTRA_CYCLES, c1 - c0, INSERT_X))
        W = a.shape[1]
    out = Image.fromarray(np.ascontiguousarray(a), 'P')
    out.putpalette(pal)
    pad = H_OUT - H                 # negativo: filas recortadas abajo
    if SIMPLIFY_TREES or SIMPLIFY_RIVER:
        arr = np.array(out)
        if SIMPLIFY_TREES:
            arr, drop = snow_simplify.simplify_trees(arr)
            print('arboles: unidad de 72 px (columnas sacadas del follaje: %s)' % drop)
        if SIMPLIFY_RIVER:
            arr, cuts, _ = snow_simplify.simplify_river(arr)
            arr, xb, _ = snow_simplify.simplify_river_b(arr)
            print('rio: alambrado desde x=%s, agua abierta desde x=%d' % (cuts.get('A'), xb))
        out = Image.fromarray(arr, 'P')
        out.putpalette(pal)
    if FLOOR_PERIOD:
        arr, x0, kept = simplify_floor(np.array(out))
        out = Image.fromarray(arr, 'P')
        out.putpalette(pal)
        print('piso: tramo de %d px desde x=%d repetido; %d tiles de objetos intactos'
              % (FLOOR_PERIOD, x0, kept))
    used = set(out.crop((0, HIDE_ROWS, W, H_OUT)).getdata())
    assert 0 not in used, 'pixeles con indice 0 (transparente) en el fondo'
    if HIDE_ROWS:
        out.paste(0, (0, 0, W, HIDE_ROWS))
    md = os.path.join(D, 'bg_snow_md.png')
    out.save(md)

    n, worst = stage_raw.build(md, D, 'bg_snow', max_tiles=8192)
    # Topes de src/stage_bg.c (SBG_MAX_TILES / SBG_MAX_SLOTS; level_snow.c
    # pide worst + 8 slots). Pasarse NO avisa en la consola: las celdas sin
    # lugar salen negras.
    assert n <= 3072, 'el fondo tiene %d tiles: sube SBG_MAX_TILES en stage_bg.c' % n
    assert worst + 8 <= 896, 'peor caso %d: sube SBG_MAX_SLOTS en stage_bg.c' % worst

    # --- Tiles de los copos ---
    with open(os.path.join(D, 'snow_flakes.bin'), 'wb') as f:
        for fl in FLAKES:
            t = [[0] * 8 for _ in range(8)]
            for x, y, c in fl:
                t[y][x] = c
            for row in t:
                f.write(bytes(((row[k] << 4) | row[k + 1]) for k in range(0, 8, 2)))

    # --- Patron: copos sueltos, nunca dos pegados (ni en diagonal) ---
    rnd = random.Random(SEED)
    pat = [[0] * PAT_W for _ in range(PAT_H)]
    want = int(PAT_W * PAT_H * PAT_DENSITY)
    tries = 0
    while want and tries < 100000:
        tries += 1
        x, y = rnd.randrange(PAT_W), rnd.randrange(PAT_H)
        if any(pat[(y + dy) % PAT_H][(x + dx) % PAT_W]
               for dy in (-1, 0, 1) for dx in (-1, 0, 1)):
            continue
        pat[y][x] = 1 + rnd.randrange(len(FLAKES))
        want -= 1

    TW, TH = W // 8, H_OUT // 8

    # --- Tope caminable por columna de 8 px (los canteros) ---
    fin = np.array(out)
    walk = [WALK_Y_MIN] * TW
    obst = []
    for c in range(TW):
        b = 0
        for x in range(c * 8, c * 8 + 8):
            ys = [y for y in range(OBST_Y0, OBST_Y1) if fin[y, x] not in GROUND]
            if ys:
                b = max(b, max(ys))
        if b:
            walk[c] = max(WALK_Y_MIN, b + OBST_MARGIN)
            obst.append(c)
    # tramos de obstaculos (para el log)
    spans, i = [], 0
    while i < len(obst):
        j = i
        while j + 1 < len(obst) and obst[j + 1] == obst[j] + 1:
            j += 1
        spans.append((obst[i] * 8, obst[j] * 8 + 7, max(walk[obst[i]:obst[j] + 1])))
        i = j + 1
    print('obstaculos (x0, x1, tope de los pies): %s' % spans)
    hp = os.path.join(ROOT, 'src', 'level_snow_bg.h')
    with open(hp, 'w', newline='\n') as f:
        f.write('// GENERADO por tools/gen_level_snow.py -- no editar a mano\n')
        f.write('#ifndef _LEVEL_SNOW_BG_H_\n#define _LEVEL_SNOW_BG_H_\n\n')
        f.write('#define LVLSNOW_W         %d  // ancho del nivel (px)\n' % W)
        f.write('#define LVLSNOW_BG_W      %d    // celdas\n' % TW)
        f.write('#define LVLSNOW_BG_ROWS   %d\n' % TH)
        f.write('#define LVLSNOW_BG_TILES  %d  // tiles unicos (con el 0 vacio)\n' % n)
        f.write('#define LVLSNOW_BG_WORST  %d   // peor caso en 42 columnas\n' % worst)
        f.write('#define LVLSNOW_SRC_H     %d   // alto original del arte\n' % H)
        f.write('#define LVLSNOW_HIDE_ROWS %d    // filas de arriba vacias (bajo el HUD)\n' % HIDE_ROWS)
        f.write('#define LVLSNOW_FLAKES    %d     // tiles de copos (snow_flakes)\n\n' % len(FLAKES))
        f.write('#define LVLSNOW_WALK_Y_MIN %d   // tope de los pies fuera de los obstaculos\n' % WALK_Y_MIN)
        f.write('// Tope caminable (pies, y de mundo) por columna de 8 px: los canteros.\n')
        f.write('static const u8 lvlSnowWalkTop[LVLSNOW_BG_W] = {\n')
        for k in range(0, TW, 16):
            f.write('    ' + ', '.join('%d' % v for v in walk[k:k + 16]) + ',\n')
        f.write('};\n\n')
        f.write('#define LVLSNOW_PAT_W     %d\n#define LVLSNOW_PAT_H     %d\n' % (PAT_W, PAT_H))
        f.write('// Copo de cada celda del patron: 0 = nada, k = tile k-1 de snow_flakes.\n')
        f.write('static const u8 lvlSnowPat[LVLSNOW_PAT_W * LVLSNOW_PAT_H] = {\n')
        for row in pat:
            f.write('    ' + ','.join('%d' % v for v in row) + ',\n')
        f.write('};\n\n#endif\n')
    print('fondo %dx%d (%+d filas), %d tiles, peor caso %d; nieve %d copos en %dx%d'
          % (W, H_OUT, pad, n, worst, sum(1 for r in pat for v in r if v), PAT_W, PAT_H))


if __name__ == '__main__':
    main()
