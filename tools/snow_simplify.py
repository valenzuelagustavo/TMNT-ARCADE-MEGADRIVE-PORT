#!/usr/bin/env python3
# =============================================================================
# snow_simplify.py (21/10) -- simplificaciones del fondo de Central Park
# =============================================================================
# El arte del 3-1 repite sus motivos (piso, arboles con faroles, baranda,
# agua) con periodos que NO son multiplo de 8, asi que la VDP no reusa tiles:
# cada repeticion cae corrida en la grilla. Aca se rearman esos motivos para
# que se repitan alineados, sin tocar los objetos unicos. Lo usa
# tools/gen_level_snow.py. Todo trabaja sobre arrays (alto, ancho) de indices
# de la paleta.
#
#   simplify_trees  arboles + faroles (cada 74 px, identicos) y la baranda de
#                   arriba (barrotes cada 11 px) -> una UNIDAD de 72 px: se le
#                   sacan 2 columnas al follaje (las que menos se notan, lejos
#                   del farol) y los barrotes pasan a ir cada 12 px (6 por
#                   unidad, 1 px mas de follaje en cada hueco).
#   simplify_band   una franja (piso, agua) -> un tramo de P px (multiplo de 8)
#                   del propio arte repetido exacto. Con 'similar' > 0 solo se
#                   reemplazan los tiles que se parecen a lo que va (el agua
#                   entre postes); los demas (objetos) quedan como estaban.
# =============================================================================
import numpy as np

# --- Arboles, faroles y baranda de arriba -----------------------------------
# Medido sobre el arte (21/10):
#   - faroles cada 74 px EXACTOS desde x=59 (columnas 59..61), y 29..57; el
#     sello arboles+faroles se repite identico (100%).
#   - baranda de arriba: y 59.. con barrotes verticales cada 11 px. Debajo de
#     cada farol hay un barrote que la cruza entera (el poste del farol).
#   - dos clases de tramo: A (alambrado, barra gruesa abajo: la baranda llega
#     a y 86) y B (baranda baja, llega a y 87). Barrote de 7 px en A, 6 en B.
#   - postes grandes del alambrado (11 px) en los bordes de los tramos y a
#     mitad de los A: suben por la baranda y quedan como estaban.
LAMP0 = 59          # x del primer farol (columna izquierda)
LAMP_P0 = 74        # periodo original
UNIT_P = 72         # periodo nuevo
TREES_Y0 = 29       # primera fila de la cabeza de los faroles
RAIL_Y0 = 59        # primera fila de la baranda de arriba
RAIL_Y1 = {'A': 87, 'B': 88}   # primera fila DESPUES de la baranda
BAR_P0, BAR_P = 11, 12
PRE = 24            # el sello arranca 24 px antes del farol


def segments(im):
    """Clase de tramo por columna ('A' o 'B') y columnas de postes grandes."""
    W = im.shape[1]
    a = (im[78] == 5) & (im[81] == 2) & (im[82] == 2) & (im[79] == 4)
    sm = np.convolve(a.astype(int), np.ones(16), 'same') / 16 > 0.5
    seg = np.where(sm, 'A', 'B')
    dark = np.isin(im[95:125], [2, 3, 4]).sum(0)
    post = np.zeros(W, bool)
    for x in range(W):
        if dark[x] >= 28:
            post[max(0, x - 1):x + 2] = True
    return seg, post


def _rail_unit(rl, lampc, lamp_u, P0=LAMP_P0, P=UNIT_P):
    """Baranda (filas, P0) del sello -> (filas, P) con barrotes cada 12.

    Barrote = columna de un solo color en la baranda. Los barrotes comunes
    miden bw (6); el poste del farol (en A pasa por un hueco) es una tira de
    columnas lisas suelta, que se copia encima en la misma posicion relativa
    al farol."""
    isbar = [len(np.unique(rl[2:17, c])) == 1 for c in range(P0)]
    runs, c = [], 0
    while c < P0:
        if isbar[c]:
            d = c
            while d < P0 and isbar[d]:
                d += 1
            runs.append((c, d))
            c = d
        else:
            c += 1
    widths = [r[1] - r[0] for r in runs if r[0] > 0 and r[1] < P0]
    bw = max(set(widths), key=widths.count)
    full = [r for r in runs if r[1] - r[0] == bw]
    gen = max(full, key=lambda r: abs(r[0] - lampc))          # lejos del farol
    prev = max((r for r in full if r[0] <= lampc), key=lambda r: r[0])
    b0 = (lamp_u - (lampc - prev[0])) % BAR_P
    fills = [c for c in range(P0) if not isbar[c]]
    out = np.zeros((rl.shape[0], P), rl.dtype)
    for u in range(P):
        q = (u - b0) % BAR_P
        if q < bw:
            out[:, u] = rl[:, gen[0] + q]
        else:
            xo = u * P0 / P
            out[:, u] = rl[:, min(fills, key=lambda f: abs(f - xo))]
    # poste del farol (columnas lisas sueltas cerca del farol)
    infull = set(c for r in full for c in range(*r))
    for c in range(max(0, lampc - 2), min(P0, lampc + 5)):
        if isbar[c] and c not in infull:
            out[:, (lamp_u + c - lampc) % P] = rl[:, c]
    return out


def simplify_trees(im):
    """Devuelve (copia, columnas sacadas del follaje)."""
    P0, P = LAMP_P0, UNIT_P
    seg, post = segments(im)
    W = im.shape[1]
    lamps = list(range(LAMP0, W, P0))

    def seal_x(kind):
        for L in lamps:
            x0 = L - PRE
            if x0 >= 0 and x0 + P0 <= W and all(seg[x0:x0 + P0] == kind) and not post[x0:x0 + P0].any():
                return x0
        raise AssertionError('no hay un sello limpio en un tramo ' + kind)

    # Arboles: iguales en todos lados (se sacan 2 columnas del follaje)
    xs = seal_x('A')
    tr = im[TREES_Y0:RAIL_Y0, xs:xs + P0]
    lamp = set(range(PRE - 4, PRE + 7))
    cost = sorted(((tr[:, c - 1] != tr[:, c + 1]).sum() + 0.5 * (tr[:, c] != tr[:, c - 1]).sum(), c)
                  for c in range(1, P0 - 1) if c not in lamp)
    drop = []
    for _, c in cost:
        if all(abs(c - d) > 8 for d in drop):
            drop.append(c)
        if len(drop) == P0 - P:
            break
    trees = tr[:, [c for c in range(P0) if c not in drop]]
    lamp_u = PRE - sum(1 for d in drop if d < PRE)

    rails = {}
    for kind in ('A', 'B'):
        x0 = seal_x(kind)
        rl = im[RAIL_Y0:RAIL_Y1[kind], x0:x0 + P0]
        rails[kind] = _rail_unit(rl, PRE, lamp_u)

    out = im.copy()
    X0 = LAMP0 - lamp_u
    for x in range(W):
        u = (x - X0) % P
        out[TREES_Y0:RAIL_Y0, x] = trees[:, u]
        if post[x]:
            continue                    # poste grande: queda el original
        k = seg[x]
        out[RAIL_Y0:RAIL_Y1[k], x] = rails[k][:, u]
    return out, sorted(drop)


# --- Franjas que se repiten (piso, agua) ------------------------------------
def simplify_band(im, y0, y1, P, protect=frozenset(), keep_colors=None,
                  similar=0.0, slices=1):
    """Repite exacto un tramo de P px de las filas y0..y1-1.

    protect:     colores que marcan un OBJETO: su tile y uno de margen quedan.
    keep_colors: si se da, los colores de la textura (lo demas es objeto).
    similar:     > 0: un tile solo se reemplaza si coincide al menos en esa
                 fraccion de pixeles con lo que iria (si no, es un objeto).
    slices:      cuantos tramos candidatos (de lugares distintos) se usan; cada
                 tile toma el que mas se le parece (el agua con alambrado y la
                 de la baranda baja son dos texturas distintas).
    Devuelve (copia, [x de cada tramo], tiles que quedaron como estaban).
    """
    H, W = im.shape
    assert P % 8 == 0 and y0 % 8 == 0 and y1 % 8 == 0
    TR0, TR1, TC = y0 // 8, y1 // 8, W // 8
    obj = np.zeros((TR1, TC), bool)
    for r in range(TR0, TR1):
        for c in range(TC):
            cs = set(np.unique(im[r * 8:r * 8 + 8, c * 8:c * 8 + 8]).tolist())
            if (cs & set(protect)) or (keep_colors is not None and not cs <= set(keep_colors)):
                obj[r, c] = True
    keep = obj.copy()
    for r, c in zip(*np.nonzero(obj)):
        keep[max(TR0, r - 1):r + 2, max(0, c - 1):c + 2] = True
    band = im[y0:y1]

    # Tramos candidatos: los de mejor empalme, sin objetos, que no se pisen.
    cands = []
    for x0 in range(1, W - P - 8):
        if keep[TR0:TR1, x0 // 8:(x0 + P) // 8 + 1].any():
            continue
        m = ((band[:, x0 + P] == band[:, x0]).mean()
             + (band[:, x0 + P - 1] == band[:, x0 - 1]).mean()) / 2
        cands.append((m, x0))
    cands.sort(reverse=True)
    chosen = []
    for m, x0 in cands:
        if all(abs(x0 - c) >= P for c in chosen):
            chosen.append(x0)
        if len(chosen) == slices:
            break
    assert chosen, 'no hay un tramo sin objetos'
    # Para elegir entre texturas distintas, los tramos se prueban tambien en
    # su propia region: que el tramo i mire su zona.
    out = im.copy()
    kept = 0
    for r in range(TR0, TR1):
        for c in range(TC):
            if keep[r, c]:
                kept += 1
                continue
            orig = im[r * 8:r * 8 + 8, c * 8:c * 8 + 8]
            xs = np.arange(c * 8, c * 8 + 8)
            best, bt = -1.0, None
            for x0 in chosen:
                t = band[r * 8 - y0:r * 8 - y0 + 8, x0:x0 + P][:, xs % P]
                s = (t == orig).mean()
                if s > best:
                    best, bt = s, t
            if similar and best < similar:
                kept += 1
                continue
            out[r * 8:r * 8 + 8, c * 8:c * 8 + 8] = bt
    return out, chosen, kept


# --- Rio: agua con alambrado (A) y agua abierta (B) -------------------------
# Medido (21/10): en los tramos A (y 88..127) el agua con la malla se repite
# cada 128 px (57%, con variaciones); en los B, la parte de arriba (y 88..103,
# agua abierta) cada 256 casi exacta. La parte de abajo de los B (baranda baja
# con postes cada ~20,5 px y el cantero) queda como esta.
# (tramo, y0, y1, periodo, colores que marcan un objeto). La malla del
# alambrado tiene un ritmo propio de 14 px: 112 = 8 x 14 la deja entera en
# cada union (con 128 quedaba cortada). El verde oscuro (6) tambien es sombra
# del agua: en A solo se protege el cartel (marrones).
RIVER = [('A', 88, 128, 112, {9, 10, 11})]     # los B: simplify_river_b


def simplify_river(im):
    """Devuelve (copia, {tramo: x del corte}, tiles que quedaron)."""
    seg, post = segments(im)
    W = im.shape[1]
    TC = W // 8
    out = im.copy()
    cuts, kept = {}, 0
    for kind, y0, y1, P, protect in RIVER:
        TR0, TR1 = y0 // 8, y1 // 8
        # tile utilizable: todo del tramo, sin postes ni colores de objeto
        ok = np.zeros((TR1, TC), bool)
        for r in range(TR0, TR1):
            for c in range(TC):
                xs = slice(c * 8, c * 8 + 8)
                t = im[r * 8:r * 8 + 8, xs]
                ok[r, c] = (all(seg[xs] == kind) and not post[xs].any()
                            and not (set(np.unique(t).tolist()) & protect))
        bad = ~ok
        keep = bad.copy()
        for r, c in zip(*np.nonzero(bad[TR0:TR1])):
            r += TR0
            if any(seg[c * 8:c * 8 + 8] == kind):      # margen solo junto a objetos del tramo
                keep[max(TR0, r - 1):r + 2, max(0, c - 1):c + 2] = True
        band = im[y0:y1]
        best = None
        for x0 in range(1, W - P - 8):
            if keep[TR0:TR1, x0 // 8:(x0 + P) // 8 + 1].any():
                continue
            m = ((band[:, x0 + P] == band[:, x0]).mean()
                 + (band[:, x0 + P - 1] == band[:, x0 - 1]).mean()) / 2
            if best is None or m > best[0]:
                best = (m, x0)
        if best is None:
            continue
        x0 = best[1]
        cuts[kind] = x0
        sl = band[:, x0:x0 + P]
        for r in range(TR0, TR1):
            for c in range(TC):
                if keep[r, c] or not ok[r, c]:
                    kept += all(seg[c * 8:c * 8 + 8] == kind)
                    continue
                xs = np.arange(c * 8, c * 8 + 8)
                out[r * 8:r * 8 + 8, c * 8:c * 8 + 8] = sl[r * 8 - y0:r * 8 - y0 + 8][:, xs % P]
    return out, cuts, kept


# --- Rio en los tramos B: agua abierta + baranda baja -----------------------
# Medido (21/10): y 88..101 agua, 102..105 pasamanos liso, 106..135 barrotes
# de 6 px ('335442') cada 21 px con agua entre medio y la orilla abajo. Se
# toma un tramo de 72 px de agua SIN el cantero, se le borran los barrotes
# (cada columna de barrote toma la de agua de al lado) y se repite; encima se
# dibujan los barrotes cada 24 px (72 = 3 x 24). El cantero (blanco grande,
# verdes, ladrillos) y uno de margen quedan como estaban.
RB_Y0, RB_Y1 = 88, 136
RB_BARS_Y0 = 106
RB_P, RB_BAR_P = 72, 24     # 72 = 3 x 24 (entre poste y cantero no hay 96 px limpios)
RB_BAR = [3, 3, 5, 4, 4, 2]


def simplify_river_b(im):
    seg, post = segments(im)
    W = im.shape[1]
    TC = W // 8
    TR0, TR1 = RB_Y0 // 8, RB_Y1 // 8
    # columnas de barrote del original (patron en la fila 110)
    isbar = np.zeros(W, bool)
    for x in range(W - 6):
        if seg[x] == 'B' and list(im[110, x:x + 6]) == RB_BAR:
            isbar[x:x + 6] = True
    first_bar = next(x for x in range(W) if isbar[x])
    # tiles del cantero: mucho blanco (1) o verdes / ladrillos
    obj = np.zeros((TR1, TC), bool)
    use = np.zeros((TR1, TC), bool)
    for r in range(TR0, TR1):
        for c in range(TC):
            xs = slice(c * 8, c * 8 + 8)
            t = im[r * 8:r * 8 + 8, xs]
            if not all(seg[xs] == 'B') or post[xs].any():
                continue
            use[r, c] = True
            v = t.flatten().tolist()
            # la orilla (y 128..135) tiene blanco y marrones: no cuenta como objeto
            if r * 8 < 128 and (v.count(1) >= 16 or set(v) & {7, 8, 9}):
                obj[r, c] = True
    keep = obj.copy()
    for r, c in zip(*np.nonzero(obj)):
        keep[max(TR0, r - 1):r + 2, max(0, c - 1):c + 2] = True
    band = im[RB_Y0:RB_Y1]
    best = None
    for x0 in range(8, W - RB_P - 8):
        if not use[TR0:TR1, x0 // 8:(x0 + RB_P) // 8 + 1].all():
            continue
        if obj[TR0:TR1, x0 // 8:(x0 + RB_P) // 8 + 1].any():
            continue
        m = (band[:, x0 + RB_P] == band[:, x0]).mean()
        if best is None or m > best[0]:
            best = (m, x0)
    assert best, 'no hay agua limpia en los tramos B'
    x0 = best[1]
    sl = band[:, x0:x0 + RB_P].copy()
    # borrar los barrotes del tramo: cada columna toma la de agua mas cercana
    yb = RB_BARS_Y0 - RB_Y0
    for j in range(RB_P):
        if isbar[x0 + j]:
            d = 1
            while True:
                for k in (x0 + j - d, x0 + j + d):
                    if 0 <= k < W and not isbar[k]:
                        sl[yb:, j] = band[yb:, k]
                        break
                else:
                    d += 1
                    continue
                break
    bar_cols = band[:, first_bar:first_bar + 6]        # perfil de un barrote
    out = im.copy()
    for r in range(TR0, TR1):
        for c in range(TC):
            if not use[r, c] or keep[r, c]:
                continue
            for x in range(c * 8, c * 8 + 8):
                col = sl[r * 8 - RB_Y0:r * 8 - RB_Y0 + 8, x % RB_P].copy()
                q = (x - first_bar) % RB_BAR_P
                if q < 6:
                    ys = np.arange(r * 8, r * 8 + 8)
                    m = ys >= RB_BARS_Y0
                    col[m] = bar_cols[ys[m] - RB_Y0, q]
                out[r * 8:r * 8 + 8, x] = col
    return out, x0, int(keep[TR0:TR1].sum())
