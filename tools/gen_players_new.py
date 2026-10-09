#!/usr/bin/env python3
# =============================================================================
# gen_players_new.py -- hojas nuevas de Leo y Don para el port (09/10)
# =============================================================================
# FUENTES (rips "de catalogo", 16 colores indexados, indice 0 transparente):
#   res/sprites/players/50886_leo.png   940x1425
#   res/sprites/players/50888_don.png  1030x1640
# Las dos comparten la paleta nueva de las tortugas (la misma del arte de la
# pantalla de titulo): pasa a ser PAL1 en todo el juego.
#
# Los frames estan en una grilla de 10 columnas, leidos de izquierda a derecha
# y de arriba a abajo, y cada animacion empieza donde hay un rotulo. Pasos
# (los mismos de Pruebas_TMNT_Control/tools/extract_leo_new.py):
#   1. se borran los rotulos: componentes conexas hechas SOLO del color del
#      texto, dentro de la franja de los rotulos;
#   2. los CUERPOS van a la celda de su centroide y cada trozo suelto (arma
#      gris, brillo, sombra) va con el cuerpo que toca o el mas cercano;
#   3. cada frame se copia a una celda de 104x104 (13x13 tiles) con el centro
#      de la celda en x=52 y la sombra terminando en y=95 (pies en ~96,
#      PLAYER_FOOT_OFFSET).
# Las filas del final sin rotulo (electrocutado, mareado, boca de tormenta) se
# cortan por los huecos entre frames; un hueco del doble de ancho deja un
# frame VACIO (la boca de tormenta lo usa: ver mhLayout en player.c).
#
# SALIDA: res/sprites/players/leo_new_13x13.png y don_new_13x13.png, una FILA
# por animacion en el ORDEN DE PlayerAnim (res/player.h), asi player.c usa las
# mismas constantes ANIM_* con las hojas viejas y con las nuevas. rescomp
# descarta los frames vacios al final de cada fila.
#
# Uso:  python3 tools/gen_players_new.py [--catalog DIR]
#       --catalog escribe ademas la hoja en el orden del rip (para revisar).
# =============================================================================
import os
import sys
import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DIR = os.path.join(ROOT, 'res', 'sprites', 'players')

FRAME = 104
FEET_Y = 99            # piso de la celda: la sombra termina en y=95
LOOSE_COLORS = [4, 10, 13]   # sombra, arma gris y brillo blanco
SHADOW_COLOR = 4
COLS = 10

# -----------------------------------------------------------------------------
# Rips. (nombre, primer frame, cantidad): indice = fila*10 + columna.
# -----------------------------------------------------------------------------
LEO = dict(
    src='50886_leo.png', out='leo_new_13x13.png',
    cell_w=94, cell_h=84, base0=115, rows=13,
    text_color=9, text_max_y=1060, sign_x=840, label_band=None,
    anims=[
        ('IDLE',           0,  6),
        ('KICK',           6,  4),
        ('THROW',         10,  6),
        ('ATTACK_1',      16,  5),
        ('ATTACK_2',      21,  6),
        ('ATTACK_3',      27,  4),
        ('ATTACK_4',      31,  4),
        ('JUMP',          35, 10),
        ('JUMP_ATTACK_1', 45,  3),
        ('JUMP_ATTACK_2', 48,  7),
        ('JUMP_ATTACK_3', 55,  3),
        ('WALK_FRONT',    58,  8),
        ('WALK_BACK',     66,  8),
        ('HIT_1',         74,  5),
        ('HIT_2',         79,  5),
        ('HIT_3',         84, 11),
        ('HIT_BEHIND_1',  95,  3),
        ('HIT_BEHIND_2',  98, 14),
        ('HIT_BEHIND_3', 112, 10),
        ('HELD',         122,  4),
    ],
    extras=[
        ('EXTRA_1', 1130, 1216),   # electrocutado (latigo del robot)
        ('EXTRA_2', 1240, 1328),   # mareado (estrellas): knockeado
        ('EXTRA_3', 1330, 1425),   # boca de tormenta: cae, vacio, sale
    ],
)

DON = dict(
    src='50888_don.png', out='don_new_13x13.png',
    cell_w=103, cell_h=100, base0=91, rows=13,
    # rotulos violetas (12, el mismo del panuelo): solo en la franja que
    # queda entre una fila y la siguiente
    text_color=12, text_max_y=1300, sign_x=None, label_band=(10, 32),
    strip=(0, 8, 0, 136),          # la tira con la paleta, arriba a la izquierda
    anims=[
        ('IDLE',           0,  5),
        ('KICK',           5,  4),
        ('THROW',          9,  7),
        ('ATTACK_1',      16,  4),
        ('ATTACK_2',      20,  7),
        ('ATTACK_3',      27,  5),
        ('ATTACK_4',      32,  4),
        ('JUMP',          36, 10),
        ('JUMP_ATTACK_1', 46,  3),
        ('JUMP_ATTACK_3', 49,  3),
        ('JUMP_ATTACK_2', 52,  7),
        ('WALK_FRONT',    59,  8),
        ('WALK_BACK',     67,  8),
        ('HIT_1',         75,  6),
        ('HIT_2',         81,  4),
        ('HIT_3',         85, 10),
        ('HIT_BEHIND_1',  95,  3),
        ('HIT_BEHIND_2',  98, 14),
        ('HIT_BEHIND_3', 112, 10),
        ('HELD',         122,  4),
    ],
    extras=[
        ('EXTRA_1', 1342, 1412),
        ('EXTRA_2', 1457, 1526),
        ('EXTRA_3', 1548, 1630),
    ],
)

EXTRA_GAP = 6          # columnas vacias que separan dos frames de las extras

# -----------------------------------------------------------------------------
# Hoja del PORT: (fila del catalogo, frames) por cada PlayerAnim, en orden.
# frames = None -> la fila entera; lista -> esos frames (None = frame vacio).
# -----------------------------------------------------------------------------
def port_layout(ch):
    hit3 = dict(LEO=11, DON=10)[ch]
    up1 = HIT3_UP[ch]
    up2 = HB3_UP[ch]
    return [
        ('IDLE',         'IDLE',          None),
        ('IDLE2',        'IDLE',          None),
        ('KICK',         'KICK',          None),
        ('ATTACK_1',     'ATTACK_1',      None),
        ('ATTACK_2',     'ATTACK_2',      None),
        ('ATTACK_3',     'ATTACK_3',      None),
        ('JUMP',         'JUMP',          None),
        ('JUMP_KICK',    'JUMP_ATTACK_1', None),
        ('WALK_FRONT',   'WALK_FRONT',    None),
        ('WALK_BACK',    'WALK_BACK',     None),
        ('SPECIAL',      'JUMP_ATTACK_2', None),
        ('HIT_1',        'HIT_1',         None),
        ('HIT_2',        'HIT_2',         None),
        ('HIT_3',        'HIT_3',         list(range(up1))),
        ('GET_UP_1',     'HIT_3',         list(range(up1, hit3))),
        ('HIT_BEHIND_1', 'HIT_BEHIND_1',  None),
        ('HIT_BEHIND_2', 'HIT_BEHIND_3',  list(range(up2))),
        ('GET_UP_2',     'HIT_BEHIND_3',  list(range(up2, 10))),
        ('HELD',         'HELD',          None),
        ('WHIP_SHOCK',   'EXTRA_1',       None),
        ('KO',           'EXTRA_2',       None),
        ('MANHOLE',      'EXTRA_3',       None),
        ('ATTACK_4',     'ATTACK_4',      None),
        ('JUMP_KICK_DIR', 'JUMP_ATTACK_3', None),
    ]

# Primer frame de la levantada al final de HIT_3 (volteo de frente) y de
# HIT_BEHIND_3 (cae de boca). Lo anterior termina con la tortuga en el piso.
# HIT_BEHIND_2 del rip (14 frames: cae, rueda y se levanta) queda sin uso.
HIT3_UP = dict(LEO=9, DON=8)
HB3_UP = dict(LEO=9, DON=8)


def extract(cfg):
    """-> (paleta, {nombre: [frame 104x104 o None]}) en el orden del rip."""
    im = Image.open(os.path.join(DIR, cfg['src']))
    assert im.mode == 'P'
    a = np.array(im)
    if cfg.get('strip'):
        y0, y1, x0, x1 = cfg['strip']
        a[y0:y1, x0:x1] = 0
    cw, chh, base0, rows = cfg['cell_w'], cfg['cell_h'], cfg['base0'], cfg['rows']
    nz = a != 0
    lab, _ = ndimage.label(nz, structure=np.ones((3, 3)))

    # 1. rotulos fuera
    spr = nz.copy()
    removed = 0
    for i, sl in enumerate(ndimage.find_objects(lab), 1):
        m = lab[sl] == i
        if not np.all(a[sl][m] == cfg['text_color']):
            continue
        y0 = sl[0].start
        if cfg['label_band'] is not None:
            lo, hi = cfg['label_band']
            rel = (y0 - (base0 - 4)) % chh       # desde la ultima linea de sombra
            if y0 >= cfg['text_max_y'] or not (lo <= rel <= hi):
                continue
        else:
            if not (y0 < cfg['text_max_y'] or (cfg['sign_x'] and sl[1].start > cfg['sign_x'])):
                continue
        spr[sl] &= ~m
        removed += 1

    def cell_of(cy, cx):
        r = int((cy - (base0 - chh)) // chh)
        c = int(cx // cw)
        if 0 <= r < rows and 0 <= c < COLS:
            return r * COLS + c
        return -1

    # 2. cuerpos a la celda de su centroide; sueltos con su cuerpo
    body = spr & ~np.isin(a, LOOSE_COLORS)
    blab, nb = ndimage.label(body, structure=np.ones((3, 3)))
    bcell = np.full(nb + 1, -1, np.int32)
    wide = []
    for i, sl in enumerate(ndimage.find_objects(blab), 1):
        ys, xs = np.nonzero(blab[sl] == i)
        if len(ys) < 4:
            continue
        cy = ys.mean() + sl[0].start
        cx = xs.mean() + sl[1].start
        bcell[i] = cell_of(cy, cx)
        if sl[1].stop - sl[1].start > cw * 1.35:
            wide.append((bcell[i], sl[1].start, sl[1].stop))
    _, (iy, ix) = ndimage.distance_transform_edt(blab == 0, return_indices=True)
    owner = np.full(a.shape, -1, np.int32)
    owner[body] = bcell[blab[body]]
    shadow = spr & (a == SHADOW_COLOR)
    loose = spr & ~body & ~shadow
    slab, _ = ndimage.label(shadow, structure=np.ones((3, 3)))
    llab, nl = ndimage.label(loose, structure=np.ones((3, 3)))
    llab[shadow] = slab[shadow] + nl
    for i, sl in enumerate(ndimage.find_objects(llab), 1):
        if sl is None:
            continue
        sl2 = (slice(max(sl[0].start - 1, 0), sl[0].stop + 1),
               slice(max(sl[1].start - 1, 0), sl[1].stop + 1))
        m = llab[sl2] == i
        ring = ndimage.binary_dilation(m, structure=np.ones((3, 3))) & ~m
        touch = blab[sl2][ring]
        touch = touch[touch > 0]
        if np.all(a[sl2][m] == SHADOW_COLOR):
            ys, xs = np.nonzero(m)
            k = cell_of(ys.mean() + sl2[0].start, xs.mean() + sl2[1].start)
            if k >= 0:
                owner[sl2][m] = k
                continue
        if len(touch):
            lbl = np.bincount(touch).argmax()
        else:
            ys, xs = np.nonzero(m)
            ys = ys + sl2[0].start
            xs = xs + sl2[1].start
            d = (iy[ys, xs] - ys) ** 2 + (ix[ys, xs] - xs) ** 2
            j = d.argmin()
            lbl = blab[iy[ys[j], xs[j]], ix[ys[j], xs[j]]]
        owner[sl2][m] = bcell[lbl]
    ys, xs = np.nonzero(spr)
    own = owner[ys, xs]
    cells = {}
    for k in np.unique(own):
        if k >= 0:
            sel = own == k
            cells[int(k)] = (ys[sel], xs[sel])

    out = {}
    clipped = 0
    for name, first, n in cfg['anims']:
        frames = []
        for k in range(n):
            idx = first + k
            r, c = divmod(idx, COLS)
            fr = np.zeros((FRAME, FRAME), np.uint8)
            if idx in cells:
                ys, xs = cells[idx]
                fy = ys + (FEET_Y - (base0 + r * chh))
                fx = xs + (FRAME // 2 - (c * cw + cw // 2))
                ok = (fy >= 0) & (fy < FRAME) & (fx >= 0) & (fx < FRAME)
                clipped += int((~ok).sum())
                fr[fy[ok], fx[ok]] = a[ys[ok], xs[ok]]
            frames.append(fr)
        out[name] = frames

    # 3. filas sin rotulo: cortadas por los huecos; piso comun de la banda
    for name, y0, y1 in cfg['extras']:
        band = spr[y0:y1].copy()
        colsx = np.nonzero(band.any(axis=0))[0]
        groups, start, last = [], None, None
        for c in colsx:
            if start is None:
                start = last = c
            elif c - last > EXTRA_GAP:
                groups.append((start, last))
                start = last = c
            else:
                last = c
        if start is not None:
            groups.append((start, last))
        bottom = max(int(np.nonzero(band[:, g0:g1 + 1].any(axis=1))[0].max()) for g0, g1 in groups) + y0
        centers = [(g0 + g1) // 2 for g0, g1 in groups]
        frames = []
        for gi, (g0, g1) in enumerate(groups):
            if gi > 0 and centers[gi] - centers[gi - 1] > 1.6 * cw:
                frames.append(None)                  # hueco = frame vacio
            sub = band[:, g0:g1 + 1]
            ys, xs = np.nonzero(sub)
            ys = ys + y0
            xs = xs + g0
            fr = np.zeros((FRAME, FRAME), np.uint8)
            fy = ys + (FEET_Y - 4 - bottom)
            fx = xs + (FRAME // 2 - centers[gi])
            ok = (fy >= 0) & (fy < FRAME) & (fx >= 0) & (fx < FRAME)
            clipped += int((~ok).sum())
            fr[fy[ok], fx[ok]] = a[ys[ok], xs[ok]]
            frames.append(fr)
        out[name] = frames
    print('%-15s rotulos borrados %d, pixeles fuera del frame %d' % (cfg['src'], removed, clipped))
    for k, x0, x1 in wide:
        print('   OJO: cuerpo muy ancho en el frame %d (x %d..%d)' % (k, x0, x1))
    return im.getpalette()[:48], out


def save_sheet(path, pal, rows):
    maxf = max(len(fr) for _, fr in rows)
    out = np.zeros((FRAME * len(rows), FRAME * maxf), np.uint8)
    for r, (_, frames) in enumerate(rows):
        for c, fr in enumerate(frames):
            if fr is not None:
                out[r * FRAME:(r + 1) * FRAME, c * FRAME:(c + 1) * FRAME] = fr
    o = Image.fromarray(out, 'P')
    o.putpalette(pal + [0] * (768 - len(pal)))
    o.save(path, transparency=0)
    return o.size


def main():
    catalog = None
    if '--catalog' in sys.argv:
        catalog = sys.argv[sys.argv.index('--catalog') + 1]
    pal0 = None
    for ch, cfg in (('LEO', LEO), ('DON', DON)):
        pal, cat = extract(cfg)
        if pal0 is None:
            pal0 = pal
        assert pal == pal0, 'las dos hojas tienen que compartir la paleta'
        if catalog:
            names = [n for n, _, _ in cfg['anims']] + [n for n, _, _ in cfg['extras']]
            save_sheet(os.path.join(catalog, ch.lower() + '_catalog.png'), pal,
                       [(n, cat[n]) for n in names])
        rows = []
        for slot, src, sel in port_layout(ch):
            frames = cat[src] if sel is None else [cat[src][i] for i in sel]
            rows.append((slot, frames))
        w, h = save_sheet(os.path.join(DIR, cfg['out']), pal, rows)
        print('%s  %dx%d' % (cfg['out'], w, h))
        for i, (slot, frames) in enumerate(rows):
            print('  %2d %-14s %2d' % (i, slot, len(frames)))


if __name__ == '__main__':
    main()
