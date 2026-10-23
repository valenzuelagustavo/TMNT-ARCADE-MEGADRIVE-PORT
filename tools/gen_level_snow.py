#!/usr/bin/env python3
# =============================================================================
# gen_level_snow.py -- Scene 3-1: Central Park con nieve, en CAPAS (parallax)
# =============================================================================
# (22/10) Version por capas. El artista paso el fondo con las capas separadas
# (res/images/lvl_central_park/, todas 2048x224, la misma paleta, indice 0
# transparente). De atras hacia adelante:
#
#   skiline city.png     los edificios (y 0..59), se repite cada 448 px
#   arbusto.png          arboles + faroles (y 21..80), cada 74 px
#   grade arbusto.png    la baranda de arriba (y 50..80), barrotes cada 11
#   rio escuro.png       el agua (y 81..125), cada 128 px exactos
#                        (rio claro.png: la variante clara, RIVER_SRC)
#   grade frente.png     alambrado + baranda baja (y 64..125), ciclo de 791
#   chão.png             el piso (y 124..223)
#   itens separados.png  carteles, canteros y circulos de hielo
#   modelo rio escuro.png  el compuesto (referencia; no se usa)
#
# PARALLAX: el Mega Drive tiene dos planos y scroll horizontal por fila de 8
# px. Las capas se reparten asi:
#
#   BG_B, banda LEJOS  filas 0..9   (y 0..79):  cielo + arboles + baranda de
#                      arriba, a FAR_NUM/FAR_DEN de la camara (1/4). Van juntos:
#                      las puntas nevadas y las cabezas de los faroles se meten
#                      en la franja del cielo sin caer en un borde de 8 px.
#   BG_B, banda RIO    filas 10..15 (y 80..127): el agua, a RIVER_NUM/DEN (1/2).
#                      La linea 80 es el borde de la baranda (un solo color):
#                      va con el rio sin que se note.
#   BG_A, FRENTE       el resto, a la velocidad de la camara: alambrado, piso y
#                      objetos. Se streamea con stage_bg (cache); lo vacio es
#                      transparente y deja ver BG_B.
#
# Las bandas se arman al ancho que recorren (la camara las mueve menos) y se
# cargan ENTERAS en VRAM (no pasan por el cache): ver BANDS en level_snow_bg.h.
#
# Simplificaciones para que los motivos repetidos reusen tiles (alineados a la
# grilla de 8 px):
#   - arboles + faroles: sello de 74 px -> unidad de 72 (se sacan 2 columnas
#     del follaje, lejos del farol);
#   - baranda de arriba: barrotes cada 12 (un px mas de hueco);
#   - piso: un tramo de FLOOR_PERIOD px del propio piso, repetido;
#   - el rio ya se repite cada 128 exactos y el cielo cada 448: quedan igual.
#
# NIVEL MAS LARGO: se insertan EXTRA_CYCLES copias de un ciclo del frente
# (alambrado con cartel + baranda baja con cantero, CYCLE = x 776..1567, 792
# px) en INSERT_X. El piso se rearma despues sobre el ancho nuevo.
#
# OBSTACULOS: los objetos del piso con colores que no son de nieve (los
# canteros) no se pisan: tope de los pies por columna (lvlSnowWalkTop).
#
# SALIDAS:
#   res/images/lvl_central_park/bg_snow_md.png      el FRENTE (PALETTE de rescomp)
#   res/images/lvl_central_park/bg_snow_tiles.bin   frente, formato ancho de stage_bg
#   res/images/lvl_central_park/bg_snow_map.bin
#   res/images/lvl_central_park/bg_snow_bands_tiles.bin  tiles de las bandas (4bpp)
#   res/images/lvl_central_park/bg_snow_band0_map.bin    banda lejos (u16 BE, como stage_raw)
#   res/images/lvl_central_park/bg_snow_band1_map.bin    banda rio
#   res/images/lvl_central_park/snow_flakes.bin     los 8 tiles de los copos
#   res/images/lvl_central_park/snow_flake_spr.png  celda 8x8 para el SPRITE de los copos
#   res/images/lvl_central_park/bg_snow_preview.png el compuesto, para mirar
#   src/level_snow_bg.h                             medidas, bandas, obstaculos
#
# Uso: python3 tools/gen_level_snow.py
# =============================================================================
import os, struct, io, contextlib
import numpy as np
from PIL import Image
import stage_raw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = os.path.join(ROOT, 'res', 'images', 'lvl_central_park')
H = 224

SKY_SRC, SKY_P = 'skiline city.png', 448
TREES_SRC, TREES_P0, TREES_P = 'arbusto.png', 74, 72
LAMP0 = 57                      # x del primer farol (arbusto.png)
RAIL_SRC, RAIL_P0, RAIL_P = 'grade arbusto.png', 11, 12
RAIL_BAR = [3, 5, 4, 4, 4, 2]   # el barrote, fila 60
RIVER_SRC, RIVER_P = 'rio escuro.png', 128
FENCE_SRC = 'grade frente.png'
FLOOR_SRC, FLOOR_Y0 = 'chão.png', 124
ITEMS_SRC = 'itens separados.png'

# Bandas de BG_B: (filas de tile, numerador, denominador de la velocidad)
FAR_ROWS, FAR_NUM, FAR_DEN = (0, 10), 1, 4
RIVER_ROWS, RIVER_NUM, RIVER_DEN = (10, 16), 1, 2
RAIL_EDGE_Y, RAIL_EDGE_COLOR = 80, 2   # linea 80: borde de la baranda (va con el rio)

FLOOR_PERIOD = 120
# (22/10) LARGO DEL NIVEL DE NES: el ripeo del Central Park (TMNT II, nivel 4)
# mide 2649 px a 256 de ancho -> 2649 * 320 / 256 = 3311 en nuestras
# pantallas. Se insertan 2 ciclos (3632 px) y se recorta a CROP_W.
EXTRA_CYCLES = 2
CYCLE = (776, 1568)
INSERT_X = 1568
CROP_W = 3312                  # 0 = sin recorte

# (22/10) OBJETOS NUEVOS del artista (misma paleta; las 8 filas de arriba de
# cada PNG son la muestra de colores y no se usan):
#   banco.png             el banco con nieve: se PINTA en el frente (y queda
#                         como obstaculo, como los canteros).
#   placa one way.png     el cartel "ONE WAY" de 8 cuadros (una vuelta): es un
#                         SPRITE que gira al golpearlo, como en el garage. Sale
#                         snow_oneway.png en celdas de SIGN_CELL (el poste en
#                         SIGN_POLE_X de la celda; la base es el borde de abajo).
SWATCH_ROWS = 8
BENCH_SRC = 'banco.png'
# Bancos: a la derecha de los canteros 1 y 3 (x del borde izquierdo del
# dibujo, y de las patas = la base). Los canteros terminan en x 684 y 2280.
BENCHES = [(700, 152), (2296, 152)]
SIGN_SRC = 'placa one way.png'
SIGN_CELL = (56, 96)
SIGN_POLE_X = 24
# Carteles: x del borde izquierdo de la celda, y de la base (el pie del poste).
# Entre canteros y bancos, con lugar para pasar por delante y por detras.
# (22/10) UNO SOLO en todo el nivel, al principio y mas abajo en la pantalla
# (base 200, la mitad de adelante de la franja caminable).
SIGNS = [(210, 200)]

# (22/10) CARTELES DE PIZZA HUT (misma paleta): se PINTAN en el frente,
# parados contra el alambrado (las patas en y PIZZA_BASE, detras de la franja
# caminable). Posiciones del mapa de NES (x 880 y 1908 de 2649) pasadas a
# 3312; el blanco corrido a la derecha para no tapar el cartel LAP CLAP del
# alambrado (x 2380..2424).
PIZZA_BASE = 128
PIZZA = [('pizza hut gold  com neve.png', 1100), ('pizza hut white com neve.png', 2440)]

# (22/10) EL TRACTOR (trator.png: 2 cuadros, su PROPIA paleta -los indices 6-11
# son sus amarillos-, va en PAL3). Sale snow_trator.png: 2 celdas de TRAC_CELL
# alineadas por la rueda de adelante (TRAC_WHEEL_X) y apoyadas abajo. Mira a la
# izquierda (avanza hacia alla). rescomp no admite sprites de 32 tiles de
# ancho: va en DOS MITADES de 128 px (snow_trator_l / _r), como el auto de la
# autopista, que el nivel mueve juntas.
TRACTOR_SRC = 'trator.png'
TRAC_CELL = (256, 112)
TRAC_WHEEL_X = 100

# Franja caminable: el piso arranca en 124; los pies desde WALK_Y_MIN.
WALK_Y_MIN = 132
OBST_MARGIN = 4
OBST_Y0 = 126                  # objetos del piso: de aca para abajo
GROUND = {15, 1, 5, 9, 10, 11}  # colores de la nieve / el piso

# Topes de src/stage_bg.c (el frente) y del presupuesto de las bandas
SBG_MAX_TILES, SBG_MAX_SLOTS = 3072, 896
BAND_MAX_TILES = 512

# --- Copos: 8x8, (x, y, color). Sobre el cielo y el rio el blanco se lee;
# sobre el piso casi no (como debe ser).
WHITE, ICE, GREY = 1, 15, 5
FLAKES = [
    [(3, 3, WHITE)],
    [(5, 1, WHITE)],
    [(1, 5, ICE)],
    [(2, 2, WHITE), (3, 2, WHITE), (2, 3, WHITE), (3, 3, ICE)],
    [(5, 4, WHITE), (6, 4, ICE), (5, 5, ICE)],
    [(3, 1, ICE), (2, 2, ICE), (3, 2, WHITE), (4, 2, ICE), (3, 3, ICE)],
    [(6, 6, GREY), (6, 5, WHITE)],
    [(1, 1, WHITE), (5, 6, ICE)],
]


def load(name):
    im = Image.open(os.path.join(D, name))
    assert im.mode == 'P' and im.size[1] == H, name
    return np.array(im), im.getpalette()[:48]


def load_any(name):
    im = Image.open(os.path.join(D, name))
    assert im.mode == 'P', name
    return np.array(im), im.getpalette()[:48]


def sign_sheet(pal):
    """Los 8 cuadros del cartel, alineados por el poste en celdas SIGN_CELL."""
    a, _ = load_any(SIGN_SRC)
    a = a.copy()
    a[:SWATCH_ROWS] = 0
    cols = np.nonzero((a != 0).any(0))[0]
    runs, s0, p = [], cols[0], cols[0]
    for c in cols[1:]:
        if c != p + 1:
            runs.append((s0, p))
            s0 = c
        p = c
    runs.append((s0, p))
    assert len(runs) == 8, 'el cartel tiene %d cuadros' % len(runs)
    cw, ch = SIGN_CELL
    ys = np.nonzero((a != 0).any(1))[0]
    y0, y1 = ys[0], ys[-1]
    assert y1 - y0 + 1 <= ch
    out = np.zeros((ch, cw * 8), np.uint8)
    for k, (x0, x1) in enumerate(runs):
        f = a[y0:y1 + 1, x0:x1 + 1]
        pole = np.nonzero((f[-8:] != 0).any(0))[0][0]        # el poste (abajo)
        ox = k * cw + SIGN_POLE_X - pole
        assert ox >= k * cw and ox + f.shape[1] <= (k + 1) * cw, 'cuadro %d no entra' % k
        oy = ch - f.shape[0]
        out[oy:, ox:ox + f.shape[1]] = f
    im = Image.fromarray(out, 'P')
    im.putpalette(pal)
    im.save(os.path.join(D, 'snow_oneway.png'), transparency=0)


def runs_of(mask):
    idx = np.nonzero(mask)[0]
    out, s0, p = [], idx[0], idx[0]
    for c in idx[1:]:
        if c != p + 1:
            out.append((s0, p))
            s0 = c
        p = c
    out.append((s0, p))
    return out


def crop_art(a):
    ys = np.nonzero((a != 0).any(1))[0]
    xs = np.nonzero((a != 0).any(0))[0]
    return a[ys[0]:ys[-1] + 1, xs[0]:xs[-1] + 1]


def tractor_sheet():
    """Los 2 cuadros del tractor alineados por la rueda de adelante."""
    im = Image.open(os.path.join(D, TRACTOR_SRC))
    assert im.mode == 'P'
    a = np.array(im).copy()
    a[:SWATCH_ROWS] = 0
    rows = runs_of((a != 0).any(1))
    assert len(rows) == 1, 'el tractor tiene %d bloques de filas' % len(rows)
    y0, y1 = rows[0]
    fr = [a[y0:y1 + 1, x0:x1 + 1] for x0, x1 in runs_of((a != 0).any(0))]
    assert len(fr) == 2, 'el tractor tiene %d cuadros' % len(fr)
    cw, ch = TRAC_CELL
    out = np.zeros((ch, cw * len(fr)), np.uint8)
    for k, f in enumerate(fr):
        wheel = np.nonzero(((f[-20:] == 2).sum(0)) >= 6)[0][0]   # rueda (negro)
        ox = TRAC_WHEEL_X - wheel
        assert ox >= 0 and ox + f.shape[1] <= cw and f.shape[0] <= ch, 'cuadro %d no entra' % k
        out[ch - f.shape[0]:, k * cw + ox:k * cw + ox + f.shape[1]] = f
    half = cw // 2
    for side, h0 in (('l', 0), ('r', half)):
        part = np.hstack([out[:, k * cw + h0:k * cw + h0 + half] for k in range(len(fr))])
        o = Image.fromarray(np.ascontiguousarray(part), 'P')
        o.putpalette(im.getpalette()[:48])
        o.save(os.path.join(D, 'snow_trator_%s.png' % side), transparency=0)
    return [(ox, f.shape[1]) for f in fr]


def over(*layers):
    out = np.zeros_like(layers[0])
    for l in layers:
        out = np.where(l != 0, l, out)
    return out


def repeat(a, P, W, x0=0):
    """Repite a[:, x0:x0+P] hasta W columnas."""
    u = a[:, x0:x0 + P]
    return np.hstack([u] * (W // P + 1))[:, :W]


def trees72(arb, W):
    """Sello de 74 px (arranca 24 px antes de un farol) -> unidad de 72."""
    pre = 24
    x0 = LAMP0 - pre + TREES_P0
    seal = arb[:, x0:x0 + TREES_P0]
    t = seal[21:81]
    lamp = set(range(pre - 4, pre + 7))
    cost = sorted(((t[:, c - 1] != t[:, c + 1]).sum() + 0.5 * (t[:, c] != t[:, c - 1]).sum(), c)
                  for c in range(1, TREES_P0 - 1) if c not in lamp)
    drop = []
    for _, c in cost:
        if all(abs(c - d) > 8 for d in drop):
            drop.append(c)
        if len(drop) == TREES_P0 - TREES_P:
            break
    unit = seal[:, [c for c in range(TREES_P0) if c not in drop]]
    lamp_u = pre - sum(1 for d in drop if d < pre)
    # que el primer farol quede donde estaba (x = LAMP0)
    shift = (LAMP0 - lamp_u) % TREES_P
    return repeat(np.roll(unit, shift, axis=1), TREES_P, W), sorted(drop)


def rail12(rail, W):
    """Barrotes cada 11 -> cada 12: a cada periodo se le suma 1 px de hueco."""
    row = rail[60]
    ph = next(x for x in range(RAIL_P0) if list(row[x:x + 6]) == RAIL_BAR)
    unit = np.concatenate([rail[:, ph:ph + RAIL_P0], rail[:, ph + 6:ph + 7]], axis=1)
    return repeat(np.roll(unit, ph, axis=1), RAIL_P, W)


def ext_cycle(a):
    if not EXTRA_CYCLES:
        return a
    c0, c1 = CYCLE
    assert (c1 - c0) % 8 == 0 and INSERT_X % 8 == 0
    return np.hstack([a[:, :INSERT_X]] + [a[:, c0:c1]] * EXTRA_CYCLES + [a[:, INSERT_X:]])


def floor_periodic(fl):
    """El piso (solo, sin objetos): tramo de FLOOR_PERIOD px que mejor empalma."""
    W = fl.shape[1]
    P = FLOOR_PERIOD
    band = fl[FLOOR_Y0:]
    best = None
    for x0 in range(1, W - P - 8):
        m = ((band[:, x0 + P] == band[:, x0]).mean()
             + (band[:, x0 + P - 1] == band[:, x0 - 1]).mean())
        if best is None or m > best[0]:
            best = (m, x0)
    out = fl.copy()
    # el tramo se ancla para que la columna x tome la (x mod P) del tramo
    out[FLOOR_Y0:] = repeat(np.roll(band[:, best[1]:best[1] + P], best[1] % P, axis=1), P, W)
    return out, best[1]


def tiles_and_maps(bands):
    """Tileset compartido (tile 0 vacio, dedup con flips) + un mapa por banda
    (u16: indice en bits 0-12, flip H bit 13, V bit 14, como stage_raw)."""
    tiles = [bytes(64)]
    index = {tiles[0]: (0, 0)}
    maps = []
    for b in bands:
        hh, ww = b.shape
        cells = []
        for ty in range(hh // 8):
            for tx in range(ww // 8):
                t = b[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8]
                hit = None
                for fl, v in ((0, t), (1, t[:, ::-1]), (2, t[::-1]), (3, t[::-1, ::-1])):
                    k = np.ascontiguousarray(v).tobytes()
                    if k in index:
                        i, f0 = index[k]
                        hit = (i, f0 ^ fl)
                        break
                if hit is None:
                    k = np.ascontiguousarray(t).tobytes()
                    index[k] = (len(tiles), 0)
                    hit = (len(tiles), 0)
                    tiles.append(k)
                i, fl = hit
                cells.append(i | ((fl & 1) << 13) | ((fl >> 1) << 14))
        maps.append(cells)
    return tiles, maps


def write_tiles(path, tiles):
    with open(path, 'wb') as f:
        for k in tiles:
            t = np.frombuffer(k, np.uint8).reshape(8, 8)
            for row in t:
                f.write(bytes(((int(row[c]) << 4) | int(row[c + 1])) for c in range(0, 8, 2)))


def main():
    sky, pal = load(SKY_SRC)
    arb, _ = load(TREES_SRC)
    rail, _ = load(RAIL_SRC)
    river, _ = load(RIVER_SRC)
    fence, _ = load(FENCE_SRC)
    floor, _ = load(FLOOR_SRC)
    items, _ = load(ITEMS_SRC)
    pal = list(pal)
    pal[0:3] = [0, 0, 0]

    # --- FRENTE (BG_A) ---
    fence, floor, items = ext_cycle(fence), ext_cycle(floor), ext_cycle(items)
    if CROP_W:
        assert CROP_W % 8 == 0 and CROP_W <= fence.shape[1]
        fence, floor, items = fence[:, :CROP_W], floor[:, :CROP_W], items[:, :CROP_W]
    items = items.copy()
    bench, _ = load_any(BENCH_SRC)
    bench[:SWATCH_ROWS] = 0
    ys = np.nonzero((bench != 0).any(1))[0]
    xs = np.nonzero((bench != 0).any(0))[0]
    bench = bench[ys[0]:ys[-1] + 1, xs[0]:xs[-1] + 1]
    for bx, by in BENCHES:
        h, w = bench.shape
        dst = items[by - h + 1:by + 1, bx:bx + w]
        dst[bench != 0] = bench[bench != 0]
    for name, px in PIZZA:
        art = crop_art(load_any(name)[0])
        h, w = art.shape
        dst = items[PIZZA_BASE - h + 1:PIZZA_BASE + 1, px:px + w]
        dst[art != 0] = art[art != 0]
    floor, fx0 = floor_periodic(floor)
    front = over(fence, floor, items)
    W = front.shape[1]
    assert W % 8 == 0
    cam_max = W - 320
    fim = Image.fromarray(front, 'P')
    fim.putpalette(pal)
    fpath = os.path.join(D, 'bg_snow_md.png')
    fim.save(fpath, transparency=0)
    with contextlib.redirect_stdout(io.StringIO()):
        n_front, worst = stage_raw.build(fpath, D, 'bg_snow', max_tiles=8192)
    assert n_front <= SBG_MAX_TILES, 'el frente tiene %d tiles: SBG_MAX_TILES' % n_front
    assert worst + 8 <= SBG_MAX_SLOTS, 'frente: peor caso %d, SBG_MAX_SLOTS' % worst

    # --- BANDAS (BG_B) ---
    def band_w(num, den):
        return ((320 + cam_max * num // den) // 8 + 2) * 8
    wf = band_w(FAR_NUM, FAR_DEN)
    trees, drop = trees72(arb, wf)
    far = over(repeat(sky, SKY_P, wf), trees, rail12(rail, wf))
    far = far[FAR_ROWS[0] * 8:FAR_ROWS[1] * 8]
    wr = band_w(RIVER_NUM, RIVER_DEN)
    riv = repeat(river, RIVER_P, wr)[RIVER_ROWS[0] * 8:RIVER_ROWS[1] * 8].copy()
    riv[RAIL_EDGE_Y - RIVER_ROWS[0] * 8] = RAIL_EDGE_COLOR
    btiles, bmaps = tiles_and_maps([far, riv])
    assert len(btiles) <= BAND_MAX_TILES, 'bandas: %d tiles' % len(btiles)
    # (22/10) Las bandas pasan por un CACHE (StageBands.slots): peor caso de
    # tiles distintos en pantalla recorriendo toda la camara.
    bcols = []
    for (num, den), m, w, rows in (((FAR_NUM, FAR_DEN), bmaps[0], wf // 8, FAR_ROWS[1] - FAR_ROWS[0]),
                                   ((RIVER_NUM, RIVER_DEN), bmaps[1], wr // 8, RIVER_ROWS[1] - RIVER_ROWS[0])):
        bcols.append(([set(m[r * w + c] & 0x1FFF for r in range(rows)) - {0} for c in range(w)], num, den, w))
    band_worst = 0
    for cx in range(0, cam_max + 1, 2):
        seen = set()
        for cols, num, den, w in bcols:
            l = (cx * num // den) >> 3
            for c in range(l, l + 42):
                seen |= cols[c % w]
        band_worst = max(band_worst, len(seen))
    write_tiles(os.path.join(D, 'bg_snow_bands_tiles.bin'), btiles)
    for k, m in enumerate(bmaps):
        with open(os.path.join(D, 'bg_snow_band%d_map.bin' % k), 'wb') as f:
            f.write(struct.pack('>%dH' % len(m), *m))

    # --- Preview del compuesto (camara en 0) ---
    prev = front.copy()
    b = np.zeros_like(front)
    # (las bandas se repiten para cubrir el ancho: en el juego van mas lento)
    b[:far.shape[0]] = repeat(far, far.shape[1], W)
    rr = RIVER_ROWS[0] * 8
    b[rr:rr + riv.shape[0]] = repeat(riv, riv.shape[1], W)
    prev = np.where(front != 0, front, b)
    pim = Image.fromarray(prev, 'P')
    pim.putpalette(pal)
    pim.save(os.path.join(D, 'bg_snow_preview.png'))

    # --- Cartel ONE WAY y tractor (sprites) ---
    sign_sheet(pal)
    tractor_sheet()

    # --- Copos ---
    with open(os.path.join(D, 'snow_flakes.bin'), 'wb') as f:
        for fl in FLAKES:
            t = [[0] * 8 for _ in range(8)]
            for x, y, c in fl:
                t[y][x] = c
            for row in t:
                f.write(bytes(((row[k] << 4) | row[k + 1]) for k in range(0, 8, 2)))
    cell = Image.new('P', (8, 8), 0)
    cell.putpalette(pal)
    cell.putpixel((3, 3), WHITE)      # la celda del SPRITE (los tiles reales: snow_flakes)
    cell.save(os.path.join(D, 'snow_flake_spr.png'), transparency=0)

    # --- Obstaculos (canteros) ---
    TW = W // 8
    walk = [WALK_Y_MIN] * TW
    obst = []
    for c in range(TW):
        bot = 0
        for x in range(c * 8, c * 8 + 8):
            ys = [y for y in range(OBST_Y0, H) if items[y, x] != 0 and items[y, x] not in GROUND]
            if ys:
                bot = max(bot, max(ys))
        if bot:
            walk[c] = max(WALK_Y_MIN, bot + OBST_MARGIN)
            obst.append(c)
    spans, i = [], 0
    while i < len(obst):
        j = i
        while j + 1 < len(obst) and obst[j + 1] == obst[j] + 1:
            j += 1
        spans.append((obst[i] * 8, obst[j] * 8 + 7, max(walk[obst[i]:obst[j] + 1])))
        i = j + 1

    hp = os.path.join(ROOT, 'src', 'level_snow_bg.h')
    with open(hp, 'w', newline='\n') as f:
        f.write('// GENERADO por tools/gen_level_snow.py -- no editar a mano\n')
        f.write('#ifndef _LEVEL_SNOW_BG_H_\n#define _LEVEL_SNOW_BG_H_\n\n')
        f.write('#define LVLSNOW_W          %d  // ancho del nivel (px)\n' % W)
        f.write('#define LVLSNOW_BG_W       %d   // frente: celdas\n' % TW)
        f.write('#define LVLSNOW_BG_ROWS    %d\n' % (H // 8))
        f.write('#define LVLSNOW_BG_TILES   %d  // frente: tiles unicos\n' % n_front)
        f.write('#define LVLSNOW_BG_WORST   %d   // frente: peor caso en 42 columnas\n' % worst)
        f.write('#define LVLSNOW_FLAKES     %d     // tiles de copos (snow_flakes)\n' % len(FLAKES))
        f.write('#define LVLSNOW_WALK_Y_MIN %d   // tope de los pies fuera de los obstaculos\n\n' % WALK_Y_MIN)
        f.write('// Bandas de parallax en BG_B (tiles: bg_snow_bands_tiles, mapas u16\n')
        f.write('// con el indice en los bits 0-12 y los flips en 13/14, como stage_raw).\n')
        f.write('#define LVLSNOW_BAND_TILES %d\n' % len(btiles))
        f.write('#define LVLSNOW_BAND_WORST %d   // con cache: peor caso en pantalla\n' % band_worst)
        f.write('#define LVLSNOW_FAR_ROW0   %d\n#define LVLSNOW_FAR_ROWS   %d\n' % (FAR_ROWS[0], FAR_ROWS[1] - FAR_ROWS[0]))
        f.write('#define LVLSNOW_FAR_W      %d   // celdas\n' % (wf // 8))
        f.write('#define LVLSNOW_FAR_NUM    %d\n#define LVLSNOW_FAR_DEN    %d\n' % (FAR_NUM, FAR_DEN))
        f.write('#define LVLSNOW_RIV_ROW0   %d\n#define LVLSNOW_RIV_ROWS   %d\n' % (RIVER_ROWS[0], RIVER_ROWS[1] - RIVER_ROWS[0]))
        f.write('#define LVLSNOW_RIV_W      %d   // celdas\n' % (wr // 8))
        f.write('#define LVLSNOW_RIV_NUM    %d\n#define LVLSNOW_RIV_DEN    %d\n\n' % (RIVER_NUM, RIVER_DEN))
        f.write('// Carteles ONE WAY (sprite snow_oneway): x de la celda, y de la base.\n')
        f.write('#define LVLSNOW_SIGN_W     %d\n#define LVLSNOW_SIGN_H     %d\n' % SIGN_CELL)
        f.write('#define LVLSNOW_SIGN_POLE  %d   // poste (borde izq.) dentro de la celda\n' % SIGN_POLE_X)
        f.write('#define LVLSNOW_N_SIGNS    %d\n' % len(SIGNS))
        f.write('static const s16 lvlSnowSigns[LVLSNOW_N_SIGNS][2] = {\n')
        for x, b in SIGNS:
            f.write('    { %d, %d },\n' % (x, b))
        f.write('};\n\n')
        f.write('// Tractor (sprite snow_trator, PAL3): celda y rueda de adelante.\n')
        f.write('#define LVLSNOW_TRAC_W     %d\n#define LVLSNOW_TRAC_H     %d\n' % TRAC_CELL)
        f.write('#define LVLSNOW_TRAC_WHEEL %d\n\n' % TRAC_WHEEL_X)
        f.write('// Tope caminable (pies, y de mundo) por columna de 8 px: canteros y bancos.\n')
        f.write('static const u8 lvlSnowWalkTop[LVLSNOW_BG_W] = {\n')
        for k in range(0, TW, 16):
            f.write('    ' + ', '.join('%d' % v for v in walk[k:k + 16]) + ',\n')
        f.write('};\n\n#endif\n')

    print('nivel %d px; frente: %d tiles, peor caso %d (piso desde x=%d cada %d)'
          % (W, n_front, worst, fx0, FLOOR_PERIOD))
    print('bandas: lejos %d px a %d/%d, rio %d px a %d/%d; %d tiles fijos (arboles: columnas sacadas %s)'
          % (wf, FAR_NUM, FAR_DEN, wr, RIVER_NUM, RIVER_DEN, len(btiles), drop))
    print('obstaculos (x0, x1, tope de los pies): %s' % spans)
    print('bancos en %s, carteles en %s, pizza hut en %s'
          % ([b[0] for b in BENCHES], [s_[0] for s_ in SIGNS], [p_[1] for p_ in PIZZA]))
    print('VRAM de fondo: %d (frente, cache) + %d (bandas, cache; enteras serian %d) = %d'
          % (worst + 8, band_worst + 8, len(btiles), worst + 8 + band_worst + 8))


if __name__ == '__main__':
    main()
