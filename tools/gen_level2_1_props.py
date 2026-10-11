#!/usr/bin/env python3
# =============================================================================
# gen_level2_1_props.py  -  Utileria de la SCENE 2 (nivel 2-1), 11/10
# =============================================================================
# Arma los sprites de la calle que el video del arcade muestra encima del
# fondo: ventanas rotas, hidrantes, el cartel "Stunner" que se cae y la chica
# en skate. Las POSICIONES de mundo de cada cosa se midieron sobre
# gameplayARCADE/tmnt.avi (SCENE 2 arranca en 3:07) ubicando cada frame del
# video dentro del mapa del nivel; estan en src/street_2_1.c, no aca.
#
# Entradas (res/images/lvl_2_scene/):
#   broken_window_front.png  38x74   ventana del frente rota
#   broken_window_side.png   20x75   ventana de la pared en diagonal, rota
#                                    (marco naranja: el edificio "Robodyne")
#   511372.png               el rip del arcade; de su zona de sprites sale la
#                            segunda ventana lateral (marco gris, el edificio
#                            de la "K"), que no vino como PNG aparte
#   hydration.png            hidrante: quieto, chorro con la tapa, 4 de rocio
#   publicity_.png           cartel: colgado, cayendo, tirado en el piso
#   skategirl.png            la chica en skate: parada y agachada
#   Stage 2-_16_colors_v2.png  el fondo, para volver transparente lo que la
#                            ventana rota tiene IGUAL al fondo (ahorra tiles)
#   genesis/lvl21_pal.png    la paleta del fondo (PAL0)
#
# Salidas (res/sprites/, las compila rescomp desde res/props_2_1.res):
#   l21_win_front.png   40x80   1 frame
#   l21_win_side.png    24x80   2 animaciones: [0] Robodyne, [1] edificio K
#   l21_hydrant.png     32x48   quieto
#   l21_hydrant_spray.png 80x72 x4  rocio (el cuerpo siempre en la misma X)
#   l21_hydrant_jet.png 112x32  la tapa saliendo con el chorro
#   l21_bill_front.png  112x64  el cartel colgado
#   l21_bill_flat.png   128x32 x2  [0] cayendo, [1] tirado
#   l21_skategirl.png   56x64 x2   [0] parada, [1] agachada  (paleta PROPIA)
#   l21_pizza.png       40x48 x4   la caja de pizza (sale de res/sprites/pizza.png)
#
# Todo menos la chica va reindexado por color exacto contra lvl21_pal (se
# dibuja con PAL0, igual que los parquimetros); el negro va al 15 porque el 0
# es el transparente.
#
# Uso:  python3 tools/gen_level2_1_props.py
# =============================================================================
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMG = os.path.join(ROOT, "res", "images", "lvl_2_scene")
SPR = os.path.join(ROOT, "res", "sprites")
PAL = os.path.join(IMG, "genesis", "lvl21_pal.png")

# Donde va cada ventana en el mundo (esquina sup. izq. del PNG de la ventana).
# Tienen que coincidir con l21Windows[] de src/street_2_1.c.
WIN_FRONT_POS = [(238, 33)]
WIN_SIDE_A_POS = [(1624, 112), (1656, 144), (1688, 176)]
WIN_SIDE_B_POS = [(1752, 240), (1784, 272)]

pim = Image.open(PAL)
pp = pim.getpalette()
DST = [tuple(pp[i * 3:i * 3 + 3]) for i in range(16)]
FLAT = [c for rgb in DST for c in rgb] + [0] * (768 - 48)


def to_lvl21(rgb, opaque):
    """RGB (h,w,3) -> indices de lvl21_pal. Color exacto si esta; si no, el
    mas cercano (solo pasa con el arte que sale del rip del arcade)."""
    h, w = opaque.shape
    out = np.zeros((h, w), np.uint8)
    pal = np.array(DST[1:], int)
    for y in range(h):
        for x in range(w):
            if not opaque[y, x]:
                continue
            c = rgb[y, x].astype(int)
            d = ((pal - c) ** 2).sum(1)
            k = int(np.argmin(d)) + 1
            # negro -> 15 (el 0 tambien es negro pero es el transparente)
            if DST[k] == (0, 0, 0):
                k = 15
            out[y, x] = k
    return out


def load_asset(name):
    im = Image.open(os.path.join(IMG, name))
    if im.mode != "P":
        sys.exit("%s tiene que ser indexado" % name)
    a = np.asarray(im)
    tr = im.info.get("transparency", 0)
    if isinstance(tr, bytes):
        tr = 0
    rgb = np.asarray(im.convert("RGB"))
    return a, rgb, (a != tr)


def save(arr, name, flat=FLAT):
    p = os.path.join(SPR, name)
    out = Image.fromarray(arr.astype(np.uint8), "P")
    out.putpalette(flat)
    out.save(p, transparency=0)
    print("  %-24s %4dx%-3d" % (name, arr.shape[1], arr.shape[0]))


def tiles_used(cell):
    h, w = cell.shape
    n = 0
    for ty in range(0, h, 8):
        for tx in range(0, w, 8):
            if cell[ty:ty + 8, tx:tx + 8].any():
                n += 1
    return n


def cut(idx, x0, y0, w, h):
    """Recorte con relleno transparente si se sale de la imagen."""
    out = np.zeros((h, w), np.uint8)
    H, W = idx.shape
    for y in range(h):
        sy = y0 + y
        if sy < 0 or sy >= H:
            continue
        for x in range(w):
            sx = x0 + x
            if 0 <= sx < W:
                out[y, x] = idx[sy, sx]
    return out


STAGE = np.asarray(Image.open(os.path.join(IMG, "Stage 2-_16_colors_v2.png"))
                   .convert("RGB")).astype(int)


def window(idx, rgb, opaque, positions, cw, ch):
    """La ventana rota con transparente todo lo que es IGUAL al fondo en
    todas sus posiciones: el marco ya esta en el fondo, solo hace falta el
    vidrio roto. Menos tiles de sprite."""
    h, w = idx.shape
    same = np.ones((h, w), bool)
    for (x, y) in positions:
        same &= np.all(STAGE[y:y + h, x:x + w] == rgb.astype(int), axis=2)
    keep = opaque & ~same
    out = np.zeros((ch, cw), np.uint8)
    out[:h, :w] = np.where(keep, idx, 0)
    return out


WATER = (4, 5, 10, 13)      # indices de lvl21_pal que usa el agua


def clean(cell, speck_far=10):
    """(11/10) Limpia la "suciedad" de una celda del hidrante:
    - cualquier mancha suelta SIN colores de agua (bordes del cuerpo del frame
      vecino que se colaban en la celda, o restos del frame de arriba);
    - gotitas de 1-2 px que quedan a mas de speck_far px de todo lo demas.
    Lo que queda es el cuerpo, el chorro/rocio y las gotas pegadas a el."""
    import cv2
    m = (cell > 0).astype(np.uint8)
    n, lab, st, _ = cv2.connectedComponentsWithStats(m, connectivity=8)
    if n <= 1:
        return cell
    big = 1 + int(np.argmax(st[1:, 4]))
    out = cell.copy()
    for i in range(1, n):
        if i == big:
            continue
        cols = set(np.unique(cell[lab == i]).tolist())
        if not (cols & set(WATER)) and st[i][4] < 40:
            out[lab == i] = 0
    # gotitas aisladas lejos de todo
    m = (out > 0).astype(np.uint8)
    n, lab, st, _ = cv2.connectedComponentsWithStats(m, connectivity=8)
    for i in range(1, n):
        if st[i][4] > 2:
            continue
        others = (m > 0) & (lab != i)
        x, y, w, h = st[i][:4]
        y0, y1 = max(0, y - speck_far), y + h + speck_far
        x0, x1 = max(0, x - speck_far), x + w + speck_far
        if not others[y0:y1, x0:x1].any():
            out[lab == i] = 0
    return out


def main():
    print("gen_level2_1_props:")

    # --- Ventanas -----------------------------------------------------------
    a, rgb, op = load_asset("broken_window_front.png")
    wf = window(to_lvl21(rgb, op), rgb, op, WIN_FRONT_POS, 40, 80)
    save(wf, "l21_win_front.png")
    print("     tiles", tiles_used(wf))

    a, rgb, op = load_asset("broken_window_side.png")
    wa = window(to_lvl21(rgb, op), rgb, op, WIN_SIDE_A_POS, 24, 80)

    # Variante del edificio K: sale del rip (misma celda que la naranja, que
    # en el rip esta en x=1622 y en el PNG recortado empieza 2 px adentro).
    rip = Image.open(os.path.join(IMG, "511372.png")).convert("RGB")
    rr = np.asarray(rip)
    pink = rr[700, 10]
    vb = rr[639 + 2:639 + 77, 1647 + 2:1647 + 22]
    opb = ~np.all(vb == pink, axis=2)
    ib = to_lvl21(vb, opb)
    # rgb en la paleta del fondo, para comparar contra el STAGE de 16 colores
    rgbb = np.array([DST[v] for v in ib.flatten()]).reshape(ib.shape + (3,))
    wb = window(ib, rgbb, opb, WIN_SIDE_B_POS, 24, 80)
    save(np.vstack([wa, wb]), "l21_win_side.png")
    print("     tiles", tiles_used(wa), tiles_used(wb))

    # --- Hidrante -----------------------------------------------------------
    a, rgb, op = load_asset("hydration.png")
    hi = to_lvl21(rgb, op)
    body = np.isin(hi, [3, 12, 1])      # naranja, magenta, rojo oscuro
    # quieto: el borde izquierdo del naranja va a x=5 de la celda
    ys, xs = np.nonzero(body[0:50, 0:52])
    L0, T0 = xs.min(), ys.min()
    idle = cut(hi, L0 - 5, T0 - 2, 32, 48)
    idle[:, :] = np.where(cut(op.astype(np.uint8), L0 - 5, T0 - 2, 32, 48) > 0, idle, 0)
    idle = clean(idle)
    save(idle, "l21_hydrant.png")
    print("     tiles", tiles_used(idle), " cuerpo izq/arriba", L0, T0)
    # rocio: 4 frames en la fila de abajo; cada uno arranca en el borde
    # izquierdo de su cuerpo
    sub = body[52:120]
    cols = np.nonzero(sub.any(0))[0]
    starts = []
    for c in cols:
        rows = np.nonzero(sub[:, c])[0]
        if len(rows) > 20 and (not starts or c > starts[-1] + 30):
            starts.append(int(c))
    if len(starts) != 4:
        sys.exit("esperaba 4 frames de rocio, hay %s" % starts)
    top = 52 + int(np.nonzero(sub[:, starts[0]:starts[0] + 20].any(1))[0].min())
    frames = []
    bounds = starts + [hi.shape[1] + 5]
    for k in range(4):
        x0 = starts[k] - 5
        w = min(80, bounds[k + 1] - 5 - x0)
        cell = np.zeros((72, 80), np.uint8)
        c = cut(hi, x0, top - 20, w, 72)
        cell[:, :w] = c
        frames.append(clean(cell))
    spray = np.hstack(frames)
    save(spray, "l21_hydrant_spray.png")
    print("     tiles", [tiles_used(f) for f in frames], " arriba", top)
    # chorro con la tapa
    ys, xs = np.nonzero(op[0:50, 52:200])
    jx0, jy0 = 52 + xs.min(), ys.min()
    jet = clean(cut(hi, jx0, jy0 - 1, 112, 32))
    save(jet, "l21_hydrant_jet.png")
    print("     tiles", tiles_used(jet))

    # --- Cartel --------------------------------------------------------------
    a, rgb, op = load_asset("publicity_.png")
    pi = to_lvl21(rgb, op)
    cols = np.nonzero(op.any(0))[0]
    segs, s, p = [], cols[0], cols[0]
    for c in cols[1:]:
        if c > p + 1:
            segs.append((s, p)); s = c
        p = c
    segs.append((s, p))
    if len(segs) != 3:
        sys.exit("el cartel tenia que tener 3 frames: %s" % segs)
    bot = int(np.nonzero(op.any(1))[0].max())
    front = cut(pi, segs[0][0], bot - 63, 112, 64)
    save(front, "l21_bill_front.png")
    print("     tiles", tiles_used(front))
    flat = np.hstack([cut(pi, segs[1][0], bot - 31, 128, 32),
                      cut(pi, segs[2][0], bot - 31, 128, 32)])
    save(flat, "l21_bill_flat.png")
    print("     tiles", tiles_used(flat[:, :128]), tiles_used(flat[:, 128:]))

    # --- Pizza (11/10) ---------------------------------------------------------
    # pizza.png (res/sprites): 4 frames sueltos de la caja (cerrada, abriendo,
    # abierta con "ENERGY" subiendo). Ya viene en la paleta del fondo. Cada
    # frame se alinea por el borde IZQUIERDO de la caja, que queda en x=4 de
    # una celda de 40x48 (la imagen mide 47 de alto: 1 fila de relleno arriba).
    im = Image.open(os.path.join(SPR, "pizza.png"))
    pa = np.asarray(im)
    prgb = np.asarray(im.convert("RGB"))
    pidx = to_lvl21(prgb, pa != 0)
    cols = np.nonzero((pa != 0).any(0))[0]
    segs, s0, pv = [], cols[0], cols[0]
    for c in cols[1:]:
        if c > pv + 1:
            segs.append((s0, pv)); s0 = c
        pv = c
    segs.append((s0, pv))
    if len(segs) != 4:
        sys.exit("pizza.png: esperaba 4 frames, hay %d" % len(segs))
    cells = []
    for (a0, a1) in segs:
        cell = np.zeros((48, 40), np.uint8)
        w = a1 - a0 + 1
        cell[1:48, 4:4 + w] = pidx[:, a0:a1 + 1]
        cells.append(cell)
    pz = np.hstack(cells)
    save(pz, "l21_pizza.png")
    print("     tiles", [tiles_used(c) for c in cells])

    # --- Chica en skate (paleta propia) ---------------------------------------
    im = Image.open(os.path.join(IMG, "skategirl.png"))
    g = np.asarray(im)
    gp = im.getpalette()[:48]
    gflat = gp + [0] * (768 - len(gp))
    out = []
    for (fx0, fx1) in ((6, 57), (61, 108)):
        f = g[:, fx0:fx1 + 1]
        # alineadas por la punta izquierda de la tabla (fila de mas abajo)
        rows = np.nonzero(f.any(1))[0]
        r = rows.max()
        left = fx0 + int(np.nonzero(g[r - 5, fx0:fx1 + 1])[0].min())
        out.append(cut(g, left - 2, r - 63, 56, 64))
    save(np.hstack(out), "l21_skategirl.png", gflat)
    print("     tiles", tiles_used(out[0]), tiles_used(out[1]))


if __name__ == "__main__":
    main()
