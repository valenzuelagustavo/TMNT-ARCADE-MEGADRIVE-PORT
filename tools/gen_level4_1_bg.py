#!/usr/bin/env python3
# =============================================================================
# gen_level4_1_bg.py (01/10) -- fondo nuevo del garage + regiones variables
# =============================================================================
# FUENTES (exportadas de Aseprite, res/images/lvl_4_garage/):
#   bg_garage_new.png   1282x240, 15 colores, indice 0 sin usar. Mismo encuadre
#                       que el bg_garage.png de Ray (las Y de mundo no cambian)
#                       mas 16 filas de piso abajo. Tiene VACIOS el lugar del
#                       auto (entre el camion verde y la camioneta azul) y el
#                       hueco del ascensor.
#   truck_door.png      (31/10) las puertas de la camioneta TCRI abiertas
#                       (123x81, indices 1..5 = los mismos de la paleta del fondo)
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
#   1 DOOR  la puerta del ascensor. Variantes 1..N = la puerta abriendose por
#           la mitad de a 8 px (arriba sube, abajo baja; 12/10, como el
#           arcade); la N es el hueco sin puerta.
#   2 TRUCK (31/10) las puertas traseras de la camioneta TCRI (la gris, entre
#           el auto azul y el cajon "PIPE PAINT"): de ahi bajan los dos
#           morados de la oleada de x~415. Variante 0 = cerradas (el fondo);
#           1..N = se abren: las dos hojas giran sobre sus bisagras (se
#           achican hasta quedar de canto y salen del otro lado) hasta la
#           ultima, que es truck_door.png tal cual (puertas abiertas del
#           arcade, 123x81). Los cuadros del medio se arman aca: hoja cerrada
#           del fondo comprimida en X, el interior de truck_door.png detras.
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
#   garage_boom.png         la explosion del TNT en la paleta del fondo (01/10)
#   garage_pizza.png        la caja de pizza del 2-1 en la paleta del fondo (12/10)
# =============================================================================
import itertools, os, struct
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = os.path.join(ROOT, 'res', 'images', 'lvl_4_garage')
W, H = 1288, 240

# Ubicaciones (esquina sup. izq., en px del fondo) medidas contra la imagen
# de ubicaciones por coincidencia de pixeles.
CAR_POS   = (693, 29)
COVER_POS = (642, 0)
DOOR_POS  = (1067, 38)
DOOR_STEP = 8       # (12/10) px por paso de cada hoja (multiplo de 8: reusa tiles)
DOOR_SPLIT = 40     # (12/10) primera fila de la hoja de abajo (barra amarilla)

# (31/10) Puertas de la camioneta TCRI. TRUCK_POS = donde cae la esquina
# sup. izq. de truck_door.png (medido contra el fondo: la linea clara de arriba
# del panel esta en y=30, las bisagras en x=337 y x=417, la juntura en x=377).
TRUCK_POS     = (316, 30)
TRUCK_HINGE_L = 337
TRUCK_HINGE_R = 417
TRUCK_LEAF_Y0 = 31      # filas de las hojas: [Y0, Y1); mas abajo quedan las luces
TRUCK_LEAF_Y1 = 92      # traseras y el paragolpes, que no se mueven
# Cuadros del medio: ('closed', w) = la hoja cerrada comprimida a w px de ancho
# desde la bisagra; ('inside', w) = ya paso de canto, se ve el lado de adentro
# (el de truck_door.png) comprimido a w px del otro lado de la bisagra.
TRUCK_FRAMES = [('closed', 32), ('closed', 14), ('inside', 9)]

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


def truck_variants(main_im, tdoor):
    """Cuadros de la apertura de las puertas de la camioneta (ver TRUCK_*)."""
    ox, oy = TRUCK_POS
    dw, dh = tdoor.size
    dp = tdoor.load()
    mp = main_im.load()
    mid = (TRUCK_HINGE_L + TRUCK_HINGE_R) // 2
    lw = mid - TRUCK_HINGE_L                     # ancho de cada hoja cerrada
    lo, hi = TRUCK_HINGE_L - ox, TRUCK_HINGE_R - ox   # interior en truck_door

    def with_interior():
        v = main_im.copy()
        vp = v.load()
        for y in range(dh):
            for x in range(lo, hi):
                if dp[x, y]:
                    vp[ox + x, oy + y] = dp[x, y]
        return v

    out = []
    for kind, w in TRUCK_FRAMES:
        v = with_interior()
        vp = v.load()
        if kind == 'closed':
            for dx in range(w):
                sx = (dx * lw) // w
                for y in range(TRUCK_LEAF_Y0, TRUCK_LEAF_Y1):
                    vp[TRUCK_HINGE_L + dx, y] = mp[TRUCK_HINGE_L + sx, y]
                    vp[TRUCK_HINGE_R - dx, y] = mp[TRUCK_HINGE_R - sx, y]
        else:
            lw_open = lo                          # la hoja abierta: lo columnas
            for dx in range(w):
                sx = (dx * lw_open) // w          # 0 = pegada a la bisagra
                for y in range(dh):
                    a = dp[lo - 1 - sx, y]
                    if a:
                        vp[TRUCK_HINGE_L - 1 - dx, oy + y] = a
                    b = dp[hi + 1 + sx, y]
                    if b:
                        vp[TRUCK_HINGE_R + 1 + dx, oy + y] = b
        out.append(v)
    v = main_im.copy()                            # la ultima: truck_door.png entera
    vp = v.load()
    for y in range(dh):
        for x in range(dw):
            if dp[x, y]:
                vp[ox + x, oy + y] = dp[x, y]
    out.append(v)
    return out


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
    # (01/10) La explosion del barril es la del TNT del foot soldier
    # (res/sprites/explosion.png, 7 frames de 64x64) remapeada a la paleta del
    # fondo: asi va en PAL0 y no depende de PAL2, que en la pelea de jefes la
    # pisa Rocksteady (el segundo barril esta en la arena).
    # Mapeo a mano por TONO: por distancia el crema caia en el gris claro
    # (5) y la bola de fuego quedaba gris. Crema y amarillo -> amarillo (11),
    # naranja -> naranja (14), marron -> rojo oscuro (12), rojo -> rojo (13).
    BOOM_MAP = {4: 11, 5: 11, 7: 14, 8: 12, 10: 13}
    src_boom = Image.open(os.path.join(ROOT, 'res', 'sprites', 'explosion.png'))
    boom = Image.new('P', src_boom.size, 0)
    boom.putpalette(pal)
    sp, bp = src_boom.load(), boom.load()
    for y in range(src_boom.size[1]):
        for x in range(src_boom.size[0]):
            if sp[x, y]:
                bp[x, y] = BOOM_MAP[sp[x, y]]
    boom.save(os.path.join(D, 'garage_boom.png'), transparency=0)
    # (12/10) La caja de pizza ("Pizza time!" antes del ascensor): la misma
    # hoja del 2-1 (res/sprites/l21_pizza.png, 4 frames de 40x48) pasada a la
    # paleta del fondo del garage, para ir en PAL0.
    l21 = Image.open(os.path.join(ROOT, 'res', 'sprites', 'l21_pizza.png'))
    l21rgba = l21.convert('RGBA')
    lp, la = l21.load(), l21rgba.load()
    for y in range(l21.size[1]):
        for x in range(l21.size[0]):
            if lp[x, y] == 0:
                la[x, y] = (0, 0, 0, 0)
    tmp = os.path.join(D, '_pizza_tmp.png')
    l21rgba.save(tmp)
    remap(tmp, pal).save(os.path.join(D, 'garage_pizza.png'), transparency=0)
    os.remove(tmp)
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
    # (16/10) Paleta para la VDP. rescomp pasa cada canal a 3 bits CORTANDO
    # (v >> 5), y con los grises calidos del arcade eso hacia dos cosas mal:
    # el 1 (74,74,74) y el 2 (90,90,82) caian en el MISMO gris (68) -- el piso
    # perdia todo el granulado y quedaba mas oscuro -- y el 4 (132,132,123)
    # salia amarillento. Estos van elegidos a mano (comparados en pantalla
    # contra la hoja del arcade) y escritos de forma que el corte de rescomp
    # de el nivel buscado (36*n -> n). Solo cambia la PALETTE: los indices de
    # los tiles y de los props siguen iguales.
    #   2 piso      (90,90,82)   -> 3,3,3 (antes 2,2,2 = igual que el 1)
    #   3           (107,107,99) -> 4,4,4 (antes 3,3,3; el 2 paso a ocupar ese)
    #   4           (132,132,123)-> 4,4,3 (antes 4,4,3; queda igual)
    #   7 azul      (0,99,156)   -> 0,3,5 (antes 0,3,4)
    #   12 rojo osc (166,54,30)  -> 5,2,1 (antes 5,1,0)
    MD_PAL_FIX = {2: (3, 3, 3), 3: (4, 4, 4), 4: (4, 4, 3), 7: (0, 3, 5), 12: (5, 2, 1)}
    mdpal = list(main_im.getpalette()[:48])
    for i, lv in MD_PAL_FIX.items():
        mdpal[i * 3:i * 3 + 3] = [36 * v for v in lv]
    main_im.putpalette(mdpal)
    main_im.save(os.path.join(D, 'bg_garage_md.png'))

    door = gen['elevator_door']
    dw, dh = door.size
    regions = []
    # CAR: el rectangulo de celdas que toca el auto
    c0, r0 = CAR_POS[0] // 8, CAR_POS[1] // 8
    c1 = (CAR_POS[0] + gen['car'].size[0] - 1) // 8
    r1 = (CAR_POS[1] + gen['car'].size[1] - 1) // 8
    regions.append(('CAR', c0, r0, c1 - c0 + 1, r1 - r0 + 1, [empty]))
    # DOOR (12/10): como en el arcade, la puerta se abre POR LA MITAD: la hoja
    # de arriba (filas 0..DOOR_SPLIT-1) sube y se mete en el marco, la de abajo
    # (la barra amarilla y la reja de abajo) baja y se mete en el piso, de a
    # DOOR_STEP px cada una. Las dos se recortan al rectangulo de la puerta.
    c0, r0 = DOOR_POS[0] // 8, DOOR_POS[1] // 8
    c1 = (DOOR_POS[0] + dw - 1) // 8
    r1 = (DOOR_POS[1] + dh - 1) // 8
    vars_ = []
    k = DOOR_STEP
    while k < max(DOOR_SPLIT, dh - DOOR_SPLIT):
        v = empty.copy()
        if k < DOOR_SPLIT:
            paste(v, door.crop((0, k, dw, DOOR_SPLIT)), DOOR_POS)
        if k < dh - DOOR_SPLIT:
            paste(v, door.crop((0, DOOR_SPLIT, dw, dh - k)),
                  (DOOR_POS[0], DOOR_POS[1] + DOOR_SPLIT + k))
        vars_.append(v)
        k += DOOR_STEP
    vars_.append(empty)
    regions.append(('DOOR', c0, r0, c1 - c0 + 1, r1 - r0 + 1, vars_))

    # TRUCK (31/10): las puertas de la camioneta TCRI, ver truck_variants()
    tdoor = Image.open(os.path.join(D, 'truck_door.png'))
    assert tdoor.mode == 'P' and tdoor.size == (123, 81)
    assert max(tdoor.tobytes()) <= 5 and tdoor.getpalette()[:18] == pal[:18]
    c0, r0 = TRUCK_POS[0] // 8, TRUCK_POS[1] // 8
    c1 = (TRUCK_POS[0] + tdoor.size[0] - 1) // 8
    r1 = (TRUCK_POS[1] + tdoor.size[1] - 1) // 8
    regions.append(('TRUCK', c0, r0, c1 - c0 + 1, r1 - r0 + 1,
                    truck_variants(main_im, tdoor)))

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

    # peor caso del cache en 42 columnas. (31/10) EXACTO: cada region tiene UNA
    # sola variante puesta a la vez (sbgSetVariant suelta la vieja antes de
    # pedir la nueva), asi que se prueban las combinaciones de variantes de las
    # regiones que tocan la ventana. Antes se sumaban los tiles de TODAS las
    # variantes juntas (656 en vez de 607 con el auto y el ascensor) y la
    # holgura quedaba por accidente; level4_1.c suma ahora su margen a mano.
    def col_tiles(c, v):
        s = set()
        for r in range(TH):
            cell = cells[r * TW + c]
            for name, c0, r0, w, h, n, o in reg_info:
                if v and c0 <= c < c0 + w and r0 <= r < r0 + h:
                    cell = var_cells[o + (v - 1) * w * h + (r - r0) * w + (c - c0)]
            s.add(cell & 0x0FFF)
        return s

    def reg_of(c):
        for i, (name, c0, r0, w, h, n, o) in enumerate(reg_info):
            if c0 <= c < c0 + w:
                return i
        return -1

    cache = {}

    def colset(c, asg):
        i = reg_of(c)
        v = asg.get(i, 0) if i >= 0 else 0
        if (c, v) not in cache:
            cache[(c, v)] = col_tiles(c, v)
        return cache[(c, v)]

    worst = 0
    for c in range(TW - 41):
        touch = sorted(set(reg_of(cc) for cc in range(c, c + 42)) - {-1})
        for combo in itertools.product(*[range(reg_info[i][5] + 1) for i in touch]):
            asg = dict(zip(touch, combo))
            worst = max(worst, len(set().union(*[colset(cc, asg) for cc in range(c, c + 42)])))

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
        f.write('#define LVL41_TRUCK_X %d\n#define LVL41_TRUCK_Y %d\n' % TRUCK_POS)
        f.write('#endif\n')
    print('%d tiles, peor caso en 42 columnas: %d, variantes: %s'
          % (len(tiles), worst, [(r[0], r[5]) for r in reg_info]))


if __name__ == '__main__':
    main()
