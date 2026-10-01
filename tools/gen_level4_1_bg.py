#!/usr/bin/env python3
# =============================================================================
# gen_level4_1_bg.py (01/10) -- fondo nuevo del garage + regiones variables
# =============================================================================
# FUENTES (las exporta Gustavo, res/images/lvl_4_garage/):
#   bg_garage_new.png   1282x240, 15 colores, indice 0 sin usar. Mismo encuadre
#                       que el bg_garage.png de Ray (las Y de mundo no cambian)
#                       mas 16 filas de piso abajo. Tiene VACIOS el lugar del
#                       auto (entre el camion verde y la camioneta azul) y el
#                       hueco del ascensor.
#   car.png             el auto (172x100) que sale del estacionamiento
#   sprite_cover_car.png la cola del camion verde que tapa al auto (81x120)
#   elevator_door.png   la puerta del ascensor (80x82)
#   (ubicaciones medidas sobre bg_garage_new_assets_ubication.png, ver abajo)
#
# El auto ESTACIONADO y la puerta CERRADA se pintan EN EL FONDO: como sprites
# serian 286 + 165 tiles (auto + tapa) y 110 (puerta) de VRAM de sprites,
# justo en las zonas donde pelean los soldiers y los dos jefes. En el fondo
# solo cuestan los tiles que se ven (cache de stage_bg).
#
# REGIONES VARIABLES: rectangulos de celdas del fondo con versiones alternas
# (stage_bg: sbgSetVariant). La variante 0 es la del mapa principal.
#   0 CAR   el lugar del auto. Variante 1 = vacio (cuando el auto arranca, el
#           sprite del auto lo reemplaza).
#   1 DOOR  la puerta del ascensor. Variantes 1..N = la persiana subiendo de a
#           8 px (la parte de arriba se mete en el marco); la N es el hueco
#           sin puerta.
#
# Ademas remapea los props (carteles, conos, barriles, el auto, la puerta) a
# los indices de la paleta del fondo: vienen de la misma hoja del arcade pero
# exportados con otros valores (73 en vez de 74, etc.; en la VDP son el mismo
# color) y unos pocos sin par exacto van al mas cercano. Salen *_gen.png.
#
# SALIDAS (res/images/lvl_4_garage/):
#   bg_garage_md.png        1288x240 (pad de 6 px), lo lee rescomp (PALETTE)
#   bg_garage_tiles.bin / bg_garage_map.bin   formato ancho de stage_bg
#   bg_garage_var.bin       mapas de las variantes, una tras otra
#   src/level4_1_bg.h       las regiones (posicion, tamano, variantes)
#   *_gen.png               los props en la paleta del fondo
# =============================================================================
import os, struct
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = os.path.join(ROOT, 'res', 'images', 'lvl_4_garage')
W, H = 1288, 240

# Ubicaciones (esquina sup. izq., en px del fondo) medidas contra la imagen
# de ubicaciones por coincidencia de pixeles.
CAR_POS   = (693, 29)
COVER_POS = (642, 0)
DOOR_POS  = (1067, 38)
DOOR_STEP = 8

PROPS = ['20_mph_signal', 'oneway_signal', 'cono', 'barrel_explosive',
         'car', 'elevator_door', 'sprite_cover_car']


def q(c): return tuple(v >> 5 for v in c)


def remap(src, bgpal):
    """Prop -> imagen P con los indices del fondo (0 = transparente)."""
    im = Image.open(src).convert('RGBA')
    out = Image.new('P', im.size, 0)
    out.putpalette(bgpal)
    pal = [tuple(bgpal[i * 3:i * 3 + 3]) for i in range(16)]
    cache = {}
    ip, op = im.load(), out.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            r, g, b, a = ip[x, y]
            if a == 0:
                continue
            c = (r, g, b)
            if c not in cache:
                m = [j for j in range(1, 16) if q(pal[j]) == q(c)]
                cache[c] = m[0] if m else min(
                    range(1, 16), key=lambda j: sum((u - v) ** 2 for u, v in zip(pal[j], c)))
            op[x, y] = cache[c]
    return out


def paste(dst, src, pos):
    sp, dp = src.load(), dst.load()
    for y in range(src.size[1]):
        for x in range(src.size[0]):
            if sp[x, y]:
                dp[pos[0] + x, pos[1] + y] = sp[x, y]


def main():
    src = Image.open(os.path.join(D, 'bg_garage_new.png'))
    assert src.mode == 'P'
    pal = list(src.getpalette()[:48])
    pal[0:3] = [0, 0, 0]
    empty = Image.new('P', (W, H), 0)
    empty.putpalette(pal)
    empty.paste(src, (0, 0))
    ep = empty.load()
    for x in range(src.size[0], W):            # pad: repetir la ultima columna
        for y in range(H):
            ep[x, y] = ep[src.size[0] - 1, y]

    gen = {}
    for p in PROPS:
        g = remap(os.path.join(D, p + '.png'), pal)
        g.save(os.path.join(D, p + '_gen.png'), transparency=0)
        gen[p] = g

    # --- Sprites de los props, en celdas multiplo de 8 (01/10) ---
    # (nombre, prop, ancho de frame de la fuente, celda destino w x h, offset
    #  del arte dentro de la celda). El arte se apoya ABAJO de la celda: asi
    #  el borde inferior de la celda es la base (los pies) del prop.
    sheets = [
        ('garage_sign20', '20_mph_signal',    45, 48, 96, 0, 1),
        ('garage_oneway', 'oneway_signal',    51, 56, 96, 0, 0),
        ('garage_cone',   'cono',             24, 24, 32, 0, 7),
        ('garage_barrel', 'barrel_explosive', 32, 32, 64, 0, 0),
    ]
    for out, p, fw, cw, ch, ox, oy in sheets:
        g = gen[p]
        n = g.size[0] // fw
        sh = Image.new('P', (cw * n, ch), 0)
        sh.putpalette(pal)
        for i in range(n):
            sh.paste(g.crop((i * fw, 0, (i + 1) * fw, g.size[1])), (i * cw + ox, oy))
        sh.save(os.path.join(D, out + '.png'), transparency=0)
    # El auto: 172x100 -> 176x104, en DOS mitades de 88x104 (un frame de
    # 22x13 tiles necesitaria 24 sprites de hardware; el tope de SGDK es 16).
    car = Image.new('P', (176, 104), 0)
    car.putpalette(pal)
    car.paste(gen['car'], (0, 0))
    for k, nm in enumerate(('garage_car_l', 'garage_car_r')):
        half = car.crop((k * 88, 0, k * 88 + 88, 104))
        half.putpalette(pal)
        half.save(os.path.join(D, nm + '.png'), transparency=0)

    main_im = empty.copy()
    paste(main_im, gen['car'], CAR_POS)
    paste(main_im, gen['sprite_cover_car'], COVER_POS)
    paste(main_im, gen['elevator_door'], DOOR_POS)
    main_im.save(os.path.join(D, 'bg_garage_md.png'))

    door = gen['elevator_door']
    dw, dh = door.size
    regions = []
    # CAR: el rectangulo de celdas que toca el auto
    c0, r0 = CAR_POS[0] // 8, CAR_POS[1] // 8
    c1 = (CAR_POS[0] + gen['car'].size[0] - 1) // 8
    r1 = (CAR_POS[1] + gen['car'].size[1] - 1) // 8
    regions.append(('CAR', c0, r0, c1 - c0 + 1, r1 - r0 + 1, [empty]))
    # DOOR: la persiana sube de a DOOR_STEP px
    c0, r0 = DOOR_POS[0] // 8, DOOR_POS[1] // 8
    c1 = (DOOR_POS[0] + dw - 1) // 8
    r1 = (DOOR_POS[1] + dh - 1) // 8
    vars_ = []
    k = DOOR_STEP
    while k < dh:
        v = empty.copy()
        paste(v, door.crop((0, k, dw, dh)), DOOR_POS)
        vars_.append(v)
        k += DOOR_STEP
    vars_.append(empty)
    regions.append(('DOOR', c0, r0, c1 - c0 + 1, r1 - r0 + 1, vars_))

    # --- tileset comun + mapas (formato ancho de stage_raw) ---
    tiles = [tuple((0,) * 8 for _ in range(8))]
    index = {tiles[0]: (0, 0)}

    def cell(px, tx, ty):
        t = tuple(tuple(px[tx * 8 + x, ty * 8 + y] for x in range(8)) for y in range(8))
        for fl, v in ((0, t), (1, tuple(r[::-1] for r in t)), (2, t[::-1]),
                      (3, tuple(r[::-1] for r in t[::-1]))):
            if v in index:
                i, f0 = index[v]
                fl ^= f0
                return i | ((fl & 1) << 13) | ((fl >> 1) << 14)
        tiles.append(t)
        index[t] = (len(tiles) - 1, 0)
        return len(tiles) - 1

    mp = main_im.load()
    TW, TH = W // 8, H // 8
    cells = [cell(mp, tx, ty) for ty in range(TH) for tx in range(TW)]
    var_cells = []
    reg_info = []
    off = 0
    for name, c0, r0, w, h, vs in regions:
        reg_info.append((name, c0, r0, w, h, len(vs), off))
        for v in vs:
            vp = v.load()
            for ty in range(r0, r0 + h):
                for tx in range(c0, c0 + w):
                    var_cells.append(cell(vp, tx, ty))
            off += w * h
    assert len(tiles) <= 3072, len(tiles)

    with open(os.path.join(D, 'bg_garage_tiles.bin'), 'wb') as f:
        for t in tiles:
            for row in t:
                f.write(bytes(((row[k] << 4) | row[k + 1]) for k in range(0, 8, 2)))
    with open(os.path.join(D, 'bg_garage_map.bin'), 'wb') as f:
        for c in cells:
            f.write(struct.pack('>H', c))
    with open(os.path.join(D, 'bg_garage_var.bin'), 'wb') as f:
        for c in var_cells:
            f.write(struct.pack('>H', c))

    # peor caso del cache en 42 columnas (con cualquier variante puesta)
    colsets = [set(cells[r * TW + c] & 0x0FFF for r in range(TH)) for c in range(TW)]
    for name, c0, r0, w, h, n, o in reg_info:
        for vi in range(n):
            for cc in range(w):
                for rr in range(h):
                    colsets[c0 + cc].add(var_cells[o + vi * w * h + rr * w + cc] & 0x0FFF)
    worst = max(len(set().union(*colsets[c:c + 42])) for c in range(TW - 41))

    with open(os.path.join(ROOT, 'src', 'level4_1_bg.h'), 'w') as f:
        f.write('// GENERADO por tools/gen_level4_1_bg.py -- NO EDITAR A MANO\n')
        f.write('#ifndef _LEVEL4_1_BG_H_\n#define _LEVEL4_1_BG_H_\n')
        f.write('#define LVL41_BG_W %d\n#define LVL41_BG_ROWS %d\n' % (TW, TH))
        f.write('#define LVL41_BG_WORST %d\n' % worst)
        for i, (name, c0, r0, w, h, n, o) in enumerate(reg_info):
            f.write('#define LVL41_REG_%s %d\n' % (name, i))
            f.write('#define LVL41_REG_%s_C0 %d\n#define LVL41_REG_%s_R0 %d\n' % (name, c0, name, r0))
            f.write('#define LVL41_REG_%s_W %d\n#define LVL41_REG_%s_H %d\n' % (name, w, name, h))
            f.write('#define LVL41_REG_%s_NVAR %d\n#define LVL41_REG_%s_OFF %d\n' % (name, n, name, o))
        f.write('#define LVL41_CAR_X %d\n#define LVL41_CAR_Y %d\n' % CAR_POS)
        f.write('#define LVL41_DOOR_X %d\n#define LVL41_DOOR_Y %d\n' % DOOR_POS)
        f.write('#endif\n')
    print('%d tiles, peor caso en 42 columnas: %d, variantes: %s'
          % (len(tiles), worst, [(r[0], r[5]) for r in reg_info]))


if __name__ == '__main__':
    main()
