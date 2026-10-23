#!/usr/bin/env python3
# =============================================================================
# gen_players_new.py -- hojas nuevas de Leo, Don (09/10), Mike y Raph (20/10)
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

# (20/10) MIKE: rip "50887_mike.png" (820x1640) SIN rotulos, grilla pareja de
# 10 columnas de 82 px y filas de 95 (la sombra de la fila 0 termina en y 94).
# Animaciones identificadas a ojo contra la hoja de Leo (mismo orden del rip,
# con menos frames): ver el doc del proyecto mike-hoja-nueva-20-10.
#   0-3 idle | 4-7 patada | 8-14 llave (7) | 15-19 / 20-25 / 26-29 / 30-33
#   golpes 1..4 | 34-43 salto | 44-46 / 47-53 / 54-56 golpes en el aire 1..3 |
#   57-64 / 65-72 caminatas | 73-77 golpe 1 | 78-82 golpe 2 | 83-93 volteo de
#   frente (11) | 94-96 golpe de atras | 97-110 cae de boca (14: Mike no trae
#   el "Hit Behind 3" de 10 de Leo y Don; se usa este) | 111-114 agarrado.
# Filas sueltas de abajo: electrocutado (con el esqueleto), mareado, boca de
# tormenta, MORDIDA del Mouser (3, la tortuga sacudiendo el brazo) y QUEMADO
# del lanzallamas (2).
MIKE = dict(
    src='50887_mike.png', out='mike_new_13x13.png',
    cell_w=82, cell_h=95, base0=98, rows=12,
    text_color=None, text_max_y=0, sign_x=None, label_band=None,
    extra_gap=16,                  # la boca de tormenta tiene frames en 2 pedazos
    anims=[
        ('IDLE',           0,  4),
        ('KICK',           4,  4),
        ('THROW',          8,  7),
        ('ATTACK_1',      15,  5),
        ('ATTACK_2',      20,  6),
        ('ATTACK_3',      26,  4),
        ('ATTACK_4',      30,  4),
        ('JUMP',          34, 10),
        ('JUMP_ATTACK_1', 44,  3),
        ('JUMP_ATTACK_2', 47,  7),
        ('JUMP_ATTACK_3', 54,  3),
        ('WALK_FRONT',    57,  8),
        ('WALK_BACK',     65,  8),
        ('HIT_1',         73,  5),
        ('HIT_2',         78,  5),
        ('HIT_3',         83, 11),
        ('HIT_BEHIND_1',  94,  3),
        ('HIT_BEHIND_3',  97, 14),
        ('HELD',         111,  4),
    ],
    extras=[
        ('EXTRA_1', 1170, 1250),   # electrocutado (3: el ultimo, el esqueleto)
        ('EXTRA_2', 1280, 1360),   # mareado
        ('EXTRA_3', 1368, 1464),   # boca de tormenta (con el frame vacio)
        ('BITE',    1490, 1560),   # mordida del Mouser
        ('BURN',    1570, 1640),   # quemado del lanzallamas
    ],
    # filas extra de la hoja del port (despues de las 25 de PlayerAnim):
    tail=[('MOUSER_BITE', 'BITE'), ('BURN', 'BURN')],
)

# (20/10) RAPH: rip "50923_RAPH.png" (850x1540) CON rotulos rojos (indice 14,
# el mismo del panuelo: solo se borran en la franja de arriba de cada fila y
# hasta y 1050), grilla de 10 columnas de 85 px y filas de 77 (la sombra de la
# fila 0 termina en y 90). Arriba a la izquierda trae la tira de la paleta y a
# la derecha de la fila 12 la firma "ripped by napalm": se borran.
# Mismo orden que Don (golpes en el aire 1, 3 y despues el especial de 7).
RAPH = dict(
    src='50923_RAPH.png', out='raph_new_13x13.png',
    cell_w=85, cell_h=77, base0=94, rows=13,
    text_color=14, text_max_y=1050, sign_x=None, label_band=(0, 20),   # 20: la 'ac' de 'Attack' (frame 25) arranca en 19
    strip=(0, 8, 0, 130),
    blank=[(955, 1005, 770, 850)],  # firma del ripper
    anims=[
        ('IDLE',           0,  3),
        ('KICK',           3,  4),
        ('THROW',          7,  6),
        ('ATTACK_1',      13,  3),
        ('ATTACK_2',      16,  6),
        ('ATTACK_3',      22,  3),
        ('ATTACK_4',      25,  8),
        # 33 = parado antes de saltar (el rip trae 11; la vieja y las otras
        # tortugas usan los 10 desde que levanta las sai): queda sin uso
        ('JUMP',          34, 10),
        # (20/10) Gustavo: el PRIMER "Jump Attack" del rip (44-46) es la
        # patada voladora con direccion y de media altura -> JUMP_KICK_DIR
        # (aca 'JUMP_ATTACK_3'); el SEGUNDO (47-49) es la de mas abajo ->
        # JUMP_KICK ('JUMP_ATTACK_1'). Ver PLAYER_RAPH_LOW_KICK_Z en player.h.
        ('JUMP_ATTACK_3', 44,  3),
        ('JUMP_ATTACK_1', 47,  3),
        ('JUMP_ATTACK_2', 50,  7),
        ('WALK_FRONT',    57,  8),
        ('WALK_BACK',     65,  8),
        ('HIT_1',         73,  6),
        ('HIT_2',         79,  4),
        ('HIT_3',         83, 10),
        ('HIT_BEHIND_1',  93,  3),
        ('HIT_BEHIND_2',  96, 14),
        ('HIT_BEHIND_3', 110, 10),
        ('HELD',         120,  4),
    ],
    extras=[
        ('EXTRA_1', 1050, 1122),   # electrocutado (3: el ultimo, el esqueleto)
        ('EXTRA_2', 1168, 1232),   # mareado
        ('EXTRA_3', 1258, 1340),   # boca de tormenta (con el frame vacio)
        ('BITE',    1378, 1446),   # mordida del Mouser
        ('BURN',    1462, 1530),   # quemado del lanzallamas
    ],
    tail=[('MOUSER_BITE', 'BITE'), ('BURN', 'BURN')],
)

EXTRA_GAP = 6          # columnas vacias que separan dos frames de las extras

# -----------------------------------------------------------------------------
# Hoja del PORT: (fila del catalogo, frames) por cada PlayerAnim, en orden.
# frames = None -> la fila entera; lista -> esos frames (None = frame vacio).
# -----------------------------------------------------------------------------
def port_layout(ch):
    hit3 = dict(LEO=11, DON=10, MIKE=11, RAPH=10)[ch]
    hb3 = dict(LEO=10, DON=10, MIKE=14, RAPH=10)[ch]
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
        ('GET_UP_2',     'HIT_BEHIND_3',  list(range(up2, hb3))),
        ('HELD',         'HELD',          None),
        ('WHIP_SHOCK',   'EXTRA_1',       None),
        ('KO',           'EXTRA_2',       None),
        ('MANHOLE',      'EXTRA_3',       None),
        ('ATTACK_4',     'ATTACK_4',      None),
        ('JUMP_KICK_DIR', 'JUMP_ATTACK_3', None),
        # (09/10) La llave: agarra al foot soldier y lo tira por encima del
        # hombro (gameplayARCADE/tmnt.avi, frames 2106..2129 y 4206..4229).
        ('THROW',        'THROW',         None),
    ]

# Primer frame de la levantada al final de HIT_3 (volteo de frente) y de
# HIT_BEHIND_3 (cae de boca). Lo anterior termina con la tortuga en el piso.
# HIT_BEHIND_2 del rip (14 frames: cae, rueda y se levanta) queda sin uso.
HIT3_UP = dict(LEO=9, DON=8, MIKE=8, RAPH=8)    # Mike: 83..90 en el aire y el piso, 91..93 se levanta
HB3_UP = dict(LEO=9, DON=8, MIKE=11, RAPH=8)    # Mike: 97..107 cae y rueda, 108..110 se levanta


def extract(cfg):
    """-> (paleta, {nombre: [frame 104x104 o None]}) en el orden del rip."""
    im = Image.open(os.path.join(DIR, cfg['src']))
    assert im.mode == 'P'
    a = np.array(im)
    if cfg.get('strip'):
        y0, y1, x0, x1 = cfg['strip']
        a[y0:y1, x0:x1] = 0
    for y0, y1, x0, x1 in cfg.get('blank', []):
        a[y0:y1, x0:x1] = 0
    cw, chh, base0, rows = cfg['cell_w'], cfg['cell_h'], cfg['base0'], cfg['rows']
    nz = a != 0
    lab, _ = ndimage.label(nz, structure=np.ones((3, 3)))

    # 1. rotulos fuera
    spr = nz.copy()
    removed = 0
    for i, sl in enumerate(ndimage.find_objects(lab), 1):
        if cfg['text_color'] is None:
            break                                    # rip sin rotulos (Mike)
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
            elif c - last > cfg.get('extra_gap', EXTRA_GAP):
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


# -----------------------------------------------------------------------------
# DESPLAZAMIENTO POR FRAME (09/10)
# -----------------------------------------------------------------------------
# El rip centra cada frame en su celda por el dibujo entero: cuando el arma se
# estira (el bo de Don, la estela de la katana) el CUERPO queda corrido hacia
# atras y, al animar, la tortuga se mueve de un lado a otro. Para cada frame se
# busca un ancla del cuerpo y se calcula cuanto hay que correr el frame en X
# para que el ancla quede donde esta en el idle de Leo:
#   - en el piso: el centro de la SOMBRA (la elipse gris debajo de los pies);
#   - en el aire (o sin sombra): el centro de los VERDES (piel), sin el arma.
# El arte no se toca: el motor corre el sprite al dibujarlo (player.c,
# playerFrameDX) y el alcance de los golpes ya lleva sumado el desplazamiento
# (gen_player_hitbox.py lee offsets.json).
SKIN = [2, 5, 8]
AIR_SLOTS = ('JUMP', 'JUMP_KICK', 'JUMP_KICK_DIR', 'SPECIAL')
NO_SHIFT = ('MANHOLE',)          # cada frame va donde esta el agujero
# (09/10) SALTO medido en el video del arcade (gameplayARCADE/tmnt.avi, el
# salto en el lugar de los frames 2044..2107): por cada pose de la anim JUMP,
# donde queda el cuerpo respecto de la del despegue (f0). Se mide con los
# verdes de la piel (5 y 8):
#   JUMP_ARC_DX  centro en X de la piel, relativo al de f0 (px, mirando a la
#                derecha)
#   JUMP_ARC_DY  cuanto por ENCIMA de la altura del salto queda el punto mas
#                bajo de la piel (px); f0 y f9 apoyan los pies (0)
# El rip apoya cada frame en el piso de su celda y lo centra por el dibujo, asi
# que se pierden esas posiciones: con esto se recuperan (playerFrameDXTab y
# playerJumpDYTab).
JUMP_ARC_DX = [0.0, 3.4, -1.1, -1.5, -4.1, -2.1, 1.5, 2.4, 3.5, 3.3]
JUMP_ARC_DY = [0.0, 10.7, 10.3, 13.5, 11.6, 15.0, 17.8, 28.0, 14.0, 0.0]
SKIN_LIGHT = [5, 8]
MAX_DX = 40
OFF_FRAMES = 12                  # columnas de la tabla (las hojas viejas llegan a 12)


def shadow_center(fr):
    sh = (fr == SHADOW_COLOR)
    sh[:80] = False
    lab, n = ndimage.label(sh, structure=np.ones((3, 3)))
    if n == 0:
        return None
    sizes = ndimage.sum(sh, lab, range(1, n + 1))
    k = int(np.argmax(sizes)) + 1
    ys, xs = np.nonzero(lab == k)
    if sizes[k - 1] < 15 or xs.max() - xs.min() < 12 or ys.max() - ys.min() > 10:
        return None
    return (xs.min() + xs.max()) / 2.0


def skin_center(fr):
    ys, xs = np.nonzero(np.isin(fr, SKIN))
    return xs.mean() if len(xs) else None


def idle_ref(rows):
    idle = rows[0][1][0]
    return shadow_center(idle), skin_center(idle)


def skin_light(fr):
    ys, xs = np.nonzero(np.isin(fr, SKIN_LIGHT))
    return (xs.mean(), int(ys.max())) if len(xs) else (None, None)


def jump_offsets(rows):
    """(dx, dy) de cada frame de JUMP, como en el arcade. dx relativo al idle
    de la tortuga por la piel; dy = lo que hay que subir el frame."""
    idle_cx, idle_bot = skin_light(rows[0][1][0])
    frames = [fr for slot, fr in rows if slot == 'JUMP'][0]
    dxs, dys = [], []
    for i, fr in enumerate(frames):
        cx, bot = skin_light(fr)
        dxs.append(int(round(idle_cx + JUMP_ARC_DX[i] - cx)))
        dys.append(int(round(JUMP_ARC_DY[i] - (idle_bot - bot))))
    return dxs, dys


def frame_offsets(rows, ref):
    """-> lista (por fila) de listas de dx (px, mirando a la derecha). 'ref'
    = anclas del idle de LEO: todas las tortugas quedan con el cuerpo en el
    mismo lugar respecto de x (ahi esta calibrada la hurtbox, PLAYER_HURT_*)."""
    ref_s, ref_g = ref
    out = []
    for slot, frames in rows:
        dxs = []
        for fr in frames:
            dx = 0
            if fr is not None and fr.any() and slot not in NO_SHIFT:
                c = None if slot in AIR_SLOTS else shadow_center(fr)
                if c is not None:
                    dx = ref_s - c
                else:
                    g = skin_center(fr)
                    dx = (ref_g - g) if g is not None else 0
            dxs.append(int(max(-MAX_DX, min(MAX_DX, round(dx)))))
        if slot == 'JUMP':
            # relativo a la tortuga misma, mas el corrimiento de su idle
            jdx, _ = jump_offsets(rows)
            dxs = [d + out[0][0] for d in jdx]
        out.append(dxs)
    return out


def write_offsets(offs, jdy):
    """src/player_offsets.c/.h y res/sprites/players/offsets.json"""
    import json
    with open(os.path.join(DIR, 'offsets.json'), 'w') as f:
        json.dump(offs, f)
    names = [slot for slot, _, _ in port_layout('LEO')]
    src = os.path.join(ROOT, 'src')
    with open(os.path.join(src, 'player_offsets.h'), 'w') as f:
        f.write(OFF_H % dict(anims=len(names), frames=OFF_FRAMES))
    with open(os.path.join(src, 'player_offsets.c'), 'w') as f:
        f.write('// GENERADO POR tools/gen_players_new.py -- NO EDITAR A MANO\n')
        f.write('#include "player_offsets.h"\n\n')
        f.write('const s8 playerFrameDXTab[PDX_CHARS][PDX_ANIMS][PDX_FRAMES] = {\n')
        for ch in ('leo', 'mike', 'don', 'raph'):
            f.write('  {   // %s\n' % ch)
            rows = offs.get(ch)
            for i, name in enumerate(names):
                vals = rows[i] if rows else []
                vals = (vals + [0] * OFF_FRAMES)[:OFF_FRAMES]
                f.write('    { %s },  // %s\n' % (', '.join('%3d' % v for v in vals), name))
            f.write('  },\n')
        f.write('};\n\n')
        f.write('const s8 playerJumpDYTab[PDX_CHARS][PDX_FRAMES] = {\n')
        for ch in ('leo', 'mike', 'don', 'raph'):
            vals = (jdy.get(ch, []) + [0] * OFF_FRAMES)[:OFF_FRAMES]
            f.write('    { %s },  // %s\n' % (', '.join('%3d' % v for v in vals), ch))
        f.write('};\n')


OFF_H = """// GENERADO POR tools/gen_players_new.py -- NO EDITAR A MANO
#ifndef _PLAYER_OFFSETS_H_
#define _PLAYER_OFFSETS_H_

#include <genesis.h>

// Desplazamiento en X de cada frame de las tortugas (px, mirando a la
// DERECHA; espejado se resta). Deja el cuerpo quieto aunque el rip haya
// corrido el dibujo para que entre el arma. Personaje: 0=Leo 1=Mike 2=Don
// 3=Raph.
#define PDX_CHARS   4
#define PDX_ANIMS   %(anims)d
#define PDX_FRAMES  %(frames)d

extern const s8 playerFrameDXTab[PDX_CHARS][PDX_ANIMS][PDX_FRAMES];
// Cuanto SUBIR cada frame de ANIM_JUMP (px) para que la vuelta en el aire quede
// a la altura que tiene en el arcade (el rip apoya todos en el piso).
extern const s8 playerJumpDYTab[PDX_CHARS][PDX_FRAMES];

#endif
"""


def shift_to(fr, target_x):
    """Corre el frame en X para que el centro de su sombra quede en target_x
    (filas de la hoja del port que no tienen dx en player_offsets)."""
    c = shadow_center(fr)
    if c is None:
        c = skin_center(fr)
    if c is None:
        return fr
    d = int(round(target_x - c))
    out = np.zeros_like(fr)
    if d >= 0:
        out[:, d:] = fr[:, :FRAME - d]
    else:
        out[:, :FRAME + d] = fr[:, -d:]
    return out


def main():
    """(20/10) Las hojas de Leo y Don se ARMARON con este script pero despues
    Gustavo les agrego filas a mano (mordida y quemado, ver
    fix_player_sheet_rows.py): ya NO se reescriben salvo con --rebuild-leo-don.
    Siempre se leen sus rips para los offsets (las filas 0-24 son las mismas).
    Mike se arma entero, con sus 27 filas."""
    catalog = None
    if '--catalog' in sys.argv:
        catalog = sys.argv[sys.argv.index('--catalog') + 1]
    rebuild_ld = '--rebuild-leo-don' in sys.argv
    pal0 = None
    offs = {}
    jdy = {}
    ref = None
    for ch, cfg in (('LEO', LEO), ('DON', DON), ('MIKE', MIKE), ('RAPH', RAPH)):
        pal, cat = extract(cfg)
        if pal0 is None:
            pal0 = pal
        assert pal == pal0, 'las hojas tienen que compartir la paleta'
        if catalog:
            names = [n for n, _, _ in cfg['anims']] + [n for n, _, _ in cfg['extras']]
            save_sheet(os.path.join(catalog, ch.lower() + '_catalog.png'), pal,
                       [(n, cat[n]) for n in names])
        rows = []
        for slot, src, sel in port_layout(ch):
            frames = cat[src] if sel is None else [cat[src][i] for i in sel]
            rows.append((slot, frames))
        if ref is None:
            ref = idle_ref(rows)                  # Leo va primero
        sheet_rows = list(rows)
        for slot, src in cfg.get('tail', []):
            sheet_rows.append((slot, [shift_to(fr, ref[0]) if fr is not None else None
                                      for fr in cat[src]]))
        if ch in ('MIKE', 'RAPH') or rebuild_ld:
            w, h = save_sheet(os.path.join(DIR, cfg['out']), pal, sheet_rows)
            print('%s  %dx%d' % (cfg['out'], w, h))
        offs[ch.lower()] = frame_offsets(rows, ref)
        jdy[ch.lower()] = jump_offsets(rows)[1]
        print('  JUMP dy %s' % jdy[ch.lower()])
        for i, (slot, frames) in enumerate(rows):
            print('  %2d %-14s %2d  dx %s' % (i, slot, len(frames), offs[ch.lower()][i]))
    write_offsets(offs, jdy)


if __name__ == '__main__':
    main()
