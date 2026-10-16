#!/usr/bin/env python3
# =============================================================================
# gen_level5_actors.py (12/10) -- los actores de la autopista (Scene 5)
# =============================================================================
# Arma, desde los rips que dejo Gustavo en res/images/lvl_5_freeway/, las hojas
# que compila rescomp para los "actores" del nivel (level5_1.c), y pasa la capa
# lejana a la paleta del fondo para liberar PAL3:
#
#   foot_car.png (3 autos de 192x102: rojo, azul, amarillo, con los foot
#   soldiers arriba; paleta propia de 16) -> fw_car_l.png / fw_car_r.png:
#       las dos MITADES (96x104) de cada auto, una fila por color (anim 0
#       rojo, 1 azul, 2 amarillo). Un frame de 24x13 tiles necesitaria 24
#       sprites de hardware (tope de SGDK: 16). Paleta del auto -> PAL3.
#   foot_motocycle.png (116x61) -> fw_moto.png 120x64, en la paleta del
#       auto (PAL3; los azules y lilas van al mas cercano).
#   foot_with_bomb.png (RGBA) -> en la paleta del morado (PAL2):
#       fw_thrower.png  celdas de 64x104, pies abajo y centrados:
#           anim 0 camina con el misil arriba (2 frames), anim 1 lo tira,
#           anim 2 camina con la goma arriba (2 frames), anim 3 la tira,
#           anim 4 con las manos vacias (hasta pasar a ser un morado comun).
#       fw_missile.png  64x32  el misil volando
#       fw_tire.png     64x56  la goma
#       fw_bomb_boom.png 512x128 la explosion de la bomba (la de Baxter, 1/2)
#       (las patinetas de la misma hoja son de la Scene 6).
#   l21_pizza.png -> fw_pizza.png en la paleta de la ruta (PAL0).
#   bg_freeway_far.png -> bg_freeway_far_p0.png: el skyline con los indices
#       de la paleta de la ruta (PAL0). Asi PAL3 queda para el auto, las motos
#       y los foot soldiers blancos (nunca a la vez: ver SP_WAIT en
#       level5_1.c).
#
# Correr desde la raiz del proyecto:  python3 tools/gen_level5_actors.py
# =============================================================================
import os
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, 'res', 'images', 'lvl_5_freeway')
SPR = os.path.join(ROOT, 'res', 'sprites')


def pal_of(im, n=16):
    p = im.getpalette()[:n * 3]
    return [tuple(p[i * 3:i * 3 + 3]) for i in range(n)]


def nearest(c, pal, first=1):
    return min(range(first, len(pal)), key=lambda j: sum((u - v) ** 2 for u, v in zip(pal[j], c)))


def remap_rgba(rgba, pal, drop=(), force=None):
    """RGBA -> indices de pal (0 = transparente). Colores en drop -> 0;
    force = {color: indice} para los que no tienen par razonable."""
    h, w = rgba.shape[:2]
    out = np.zeros((h, w), np.uint8)
    cache = dict(force or {})
    for y in range(h):
        for x in range(w):
            r, g, b, a = rgba[y, x]
            if a == 0 or (r, g, b) in drop:
                continue
            c = (int(r), int(g), int(b))
            if c not in cache:
                cache[c] = nearest(c, pal)
            out[y, x] = cache[c]
    return out


def save_p(arr, pal, path):
    im = Image.fromarray(arr.astype(np.uint8), 'P')
    flat = []
    for c in pal:
        flat += list(c)
    im.putpalette(flat + [0] * (768 - len(flat)))
    im.save(path, transparency=0)


def place(cell_w, cell_h, sub, center_x=None):
    """Pega sub (indices) en una celda con los pies abajo y centrados."""
    out = np.zeros((cell_h, cell_w), np.uint8)
    h, w = sub.shape
    feet = sub[max(0, h - 8):]
    xs = np.nonzero((feet != 0).any(axis=0))[0]
    fx = (int(xs.min()) + int(xs.max())) // 2 if len(xs) else w // 2
    cx = cell_w // 2 if center_x is None else center_x
    x0 = cx - fx
    x0 = max(0, min(cell_w - w, x0))
    out[cell_h - h:, x0:x0 + w] = sub
    return out


def crop(arr):
    ys, xs = np.nonzero(arr)
    return arr[ys.min():ys.max() + 1, xs.min():xs.max() + 1]


def main():
    # --- Autos ---------------------------------------------------------------
    car = Image.open(os.path.join(D, 'foot_car.png'))
    cpal = pal_of(car)
    a = np.array(car)
    rows = np.nonzero((a != 0).any(1))[0]
    segs, s, p = [], rows[0], rows[0]
    for r in rows[1:]:
        if r != p + 1:
            segs.append((s, p))
            s = r
        p = r
    segs.append((s, p))
    assert len(segs) == 3, segs
    L = np.zeros((104 * 3, 96), np.uint8)
    R = np.zeros((104 * 3, 96), np.uint8)
    for k, (y0, y1) in enumerate(segs):
        c = crop(a[y0:y1 + 1])
        h, w = c.shape
        assert w <= 192 and h <= 104, (w, h)
        cell = np.zeros((104, 192), np.uint8)
        cell[104 - h:, (192 - w) // 2:(192 - w) // 2 + w] = c
        L[k * 104:(k + 1) * 104] = cell[:, :96]
        R[k * 104:(k + 1) * 104] = cell[:, 96:]
    save_p(L, cpal, os.path.join(D, 'fw_car_l.png'))
    save_p(R, cpal, os.path.join(D, 'fw_car_r.png'))

    # --- Moto (paleta del auto) ---------------------------------------------
    moto = Image.open(os.path.join(D, 'foot_motocycle.png')).convert('RGBA')
    mi = np.array(Image.open(os.path.join(D, 'foot_motocycle.png')))
    mr = np.array(moto)
    mr[mi == 0, 3] = 0
    # El rojo es la sombra del rip; el negro (el auto no tiene: su indice 0 es
    # el transparente) va al gris oscuro del auto.
    m = crop(remap_rgba(mr, cpal, drop={(255, 0, 0)},
                        force={(0, 0, 0): cpal.index((74, 72, 74))}))
    save_p(place(120, 64, m), cpal, os.path.join(D, 'fw_moto.png'))

    # --- Los que tiran el misil / la goma (paleta del morado) ----------------
    fpal = pal_of(Image.open(os.path.join(SPR, 'foot_soldier_purple_13x13.png')))
    src = np.array(Image.open(os.path.join(D, 'foot_with_bomb.png')).convert('RGBA'))
    # El rojo puro marca la sombra en el rip: no se dibuja.
    idx = remap_rgba(src, fpal, drop={(255, 0, 0)})
    lab, n = ndimage.label(ndimage.binary_dilation(idx != 0, iterations=2))
    objs = []
    for sl in ndimage.find_objects(lab):
        y, x = sl[0].start, sl[1].start
        objs.append((y, x, sl))
    def obj_at(x, y):
        for (oy, ox, sl) in objs:
            if abs(ox - x) < 12 and abs(oy - y) < 12:
                sub = idx[sl].copy()
                sub[lab[sl] != lab[oy + 2 if False else sl[0].start:sl[0].stop, sl[1].start:sl[1].stop].max()] = 0
                return crop(sub)
        raise SystemExit('no encuentro el frame en (%d,%d)' % (x, y))
    # (x, y) de cada frame en el rip (esquina de la caja, medida)
    MISSILE_CARRY = [(189, 8), (270, 11)]
    MISSILE_THROW = (350, 11)
    TIRE_CARRY = [(188, 111), (271, 113)]
    TIRE_THROW = (353, 114)
    CW, CH = 64, 104
    PLAIN = (35, 8)            # el morado con las manos vacias (ya la tiro)
    sheet = np.zeros((CH * 5, CW * 2), np.uint8)
    rowsdef = [MISSILE_CARRY, [MISSILE_THROW, MISSILE_THROW],
               TIRE_CARRY, [TIRE_THROW, TIRE_THROW], [PLAIN, PLAIN]]
    for r, frames in enumerate(rowsdef):
        for c, (x, y) in enumerate(frames):
            sub = obj_at(x, y)
            assert sub.shape[0] <= CH and sub.shape[1] <= CW, sub.shape
            sheet[r * CH:(r + 1) * CH, c * CW:(c + 1) * CW] = place(CW, CH, sub)
    save_p(sheet, fpal, os.path.join(D, 'fw_thrower.png'))
    mis = obj_at(111, 38)
    out = np.zeros((32, 64), np.uint8)
    h, w = mis.shape
    out[(32 - h) // 2:(32 - h) // 2 + h, (64 - w) // 2:(64 - w) // 2 + w] = mis
    save_p(out, fpal, os.path.join(D, 'fw_missile.png'))
    tire = obj_at(113, 137)
    out = np.zeros((56, 64), np.uint8)
    h, w = tire.shape
    out[56 - h:, (64 - w) // 2:(64 - w) // 2 + w] = tire
    save_p(out, fpal, os.path.join(D, 'fw_tire.png'))

    # --- (13/10) Explosion de la bomba: la de Baxter, en PAL2 -----------------
    # Filas 0 y 2 de baxter_boom_gen.png (los cuartos de la IZQUIERDA de los
    # 8 frames de 128x128: anim 0 arriba, anim 1 abajo); level5_1.c pone los
    # de la derecha espejados. Los 4 colores del fuego pasan a sus parecidos del morado:
    # blanco -> crema, amarillo claro -> amarillo, salmon -> naranja,
    # rosa oscuro -> naranja rojizo.
    bx = np.array(Image.open(os.path.join(SPR, 'baxter_boom_gen.png')))
    bx = np.concatenate([bx[:64], bx[128:192]])      # cuartos de la izquierda
    lut = np.zeros(256, np.uint8)
    for src, dst in ((15, 4), (6, 5), (10, 7), (13, 9)):
        lut[src] = dst
    save_p(lut[bx], fpal, os.path.join(D, 'fw_bomb_boom.png'))

    # --- Pizza y capa lejana en la paleta de la ruta (PAL0) ------------------
    road = Image.open(os.path.join(D, 'bg_freeway.png'))
    rpal = pal_of(road)
    pz = Image.open(os.path.join(SPR, 'l21_pizza.png'))
    pzr = np.array(pz.convert('RGBA'))
    pzr[np.array(pz) == 0, 3] = 0
    save_p(remap_rgba(pzr, rpal), rpal, os.path.join(D, 'fw_pizza.png'))
    far = Image.open(os.path.join(D, 'bg_freeway_far.png'))
    fa = np.array(far)
    fpal_far = pal_of(far)
    lut = [0] + [nearest(fpal_far[i], rpal) for i in range(1, 16)]
    save_p(np.array(lut, np.uint8)[fa], rpal, os.path.join(D, 'bg_freeway_far_p0.png'))
    print('far lut', lut)
    print('listo: fw_car_l/r, fw_moto, fw_thrower, fw_missile, fw_tire, fw_bomb_boom, fw_pizza, bg_freeway_far_p0')


if __name__ == '__main__':
    main()
