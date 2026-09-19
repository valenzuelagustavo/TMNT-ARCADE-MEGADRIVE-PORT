# -*- coding: utf-8 -*-
"""
preview_roof.py - Render offline de la cinematica del rescate de April
(SCENE_CINEMATIC_FIRE) para revisar encuadre y tiempos SIN compilar la ROM.

Para que no se desincronice del codigo, NO copia las constantes: las LEE de
src/cinematic_fire.c (los #define numericos y las tablas `static const s16`).
Si tocas una constante en el C, este script la toma sola.

Salida: _tmp_intro/preview_roof.png  (hoja de contacto con los cuadros clave)
        _tmp_intro/roof_frames/*.png (los mismos cuadros sueltos, a 1x)

Uso:  python3 tools/preview_roof.py
"""
import os
import re
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN  = os.path.join(ROOT, 'res', 'images', 'roof_april_scene', 'genesis')
SRC  = os.path.join(ROOT, 'src', 'cinematic_fire.c')
PLH  = os.path.join(ROOT, 'res', 'player.h')
CHARS = os.path.join(ROOT, 'res', 'sprites')
OUT  = os.path.join(ROOT, '_tmp_intro')
FRM  = os.path.join(OUT, 'roof_frames')
os.makedirs(FRM, exist_ok=True)

# --------------------------------------------------------------------------
# Leer las constantes del C
# --------------------------------------------------------------------------
code = open(SRC, encoding='utf-8').read()
# player.h aporta PLAYER_SPRITE_W / PLAYER_FOOT_OFFSET y el enum PlayerAnim,
# que la escena C usa tal cual (dibuja con las hojas del juego).
playerh = open(PLH, encoding='utf-8').read()

K = {}
for name, val in re.findall(r'^\s*(ANIM_[A-Z_0-9]+)\s*=\s*(\d+)', playerh, re.M):
    K[name] = int(val)
for name, expr in re.findall(r'^#define\s+([A-Z_0-9]+)\s+(.+)$', playerh + '\n' + code, re.M):
    expr = expr.replace('TRUE', '1').replace('FALSE', '0')
    expr = expr.split('//')[0].strip()
    if expr.startswith('('):        # expresiones que usan otros #define
        expr = expr
    try:
        K[name] = int(eval(expr, {'__builtins__': {}}, dict(K)))
    except Exception:
        pass                        # macros no numericas (TILE_ATTR, etc.)

A = {}
for name, body in re.findall(
        r'static const (?:s16|u8)\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{([^}]*)\}', code):
    A[name] = [int(v) for v in re.findall(r'-?\d+', body)]

SCR_W_AB, SCR_W_C, SCR_H = 256, 320, K['ROOF_SCREEN_H']


def sheet(name, cw, ch):
    """Devuelve la lista de celdas de una hoja generada, en RGBA."""
    im = Image.open(os.path.join(GEN, name)).convert('RGBA')
    # el indice 0 de estas hojas es transparencia: PIL ya lo respeta al
    # convertir a RGBA porque el PNG trae el chunk tRNS
    cells = []
    for r in range(im.height // ch):
        for c in range(im.width // cw):
            cells.append(im.crop((c * cw, r * ch, (c + 1) * cw, (r + 1) * ch)))
    return cells


# Hojas del juego para la escena C. Son de 1248x2184: se abren una vez y se
# recortan bajo demanda (indice 0=Leo 1=Mike 2=Don 3=Raph, igual que el C).
_GAME_FILES = ['leo', 'mike', 'don', 'raph']
_game = [Image.open(os.path.join(CHARS, '%s_anim_13x13.png' % t)).convert('RGBA')
         for t in _GAME_FILES]
_GAME_CELL = 104


def gamecell(turtle, anim, frame):
    im = _game[turtle]
    return im.crop((frame * _GAME_CELL, anim * _GAME_CELL,
                    (frame + 1) * _GAME_CELL, (anim + 1) * _GAME_CELL))


bg_a = Image.open(os.path.join(GEN, 'roof_bg_a.png')).convert('RGB')
bg_b = Image.open(os.path.join(GEN, 'roof_bg_b.png')).convert('RGB')
bg_c = Image.open(os.path.join(GEN, 'roof_bg_c.png')).convert('RGB')
climb  = sheet('roof_climb.png', 56, 64)         # [fila*2 + frame]
climbf = sheet('roof_climb_far.png', 40, 48)    # [fila*2 + frame]
tiny  = sheet('roof_tiny.png',  48, 64)      # [fila]
far   = sheet('roof_far.png',   72, 88)      # [fila]
near  = sheet('roof_near.png', 104, 120)     # [fila]
spl   = sheet('roof_splinter.png', 48, 64)   # [frame]
bal   = sheet('roof_baloon.png',  64, 32)    # [frame]


def lerp(a, b, t, T):
    return a + (b - a) * t // T


def accel(a, b, t, T):
    return a + (b - a) * t * t // (T * T)


def blit(dst, cell, x, y):
    tmp = Image.new('RGBA', dst.size, (0, 0, 0, 0))
    tmp.paste(cell, (x, y))
    dst.alpha_composite(tmp)


def frame_a(f):
    """Un cuadro de la escena A (numero de frame del analisis, 0..T_A_TOTAL)."""
    im = bg_a.convert('RGBA')

    # Splinter, congelado en su 2da pose
    blit(im, spl[K['SPL_FRAME']], K['SPL_X'], K['SPL_Y'])

    # Globo de dialogo
    b = None
    if K['T_BAL_FIRE_IN'] <= f < K['T_BAL_FIRE_OUT']:
        b = 0
    elif K['T_BAL_HANG_IN'] <= f < K['T_BAL_HANG_OUT']:
        b = 1
    if b is not None:
        blit(im, bal[b], K['BAL_X'], K['BAL_Y'])

    # Grupo saltando: UN solo arco (mismo calculo que climbSetPose en el C)
    if f < K['T_CLIMB_END']:
        if f < K['T_CLIMB_START']:
            x, y, pose, big = K['CLIMB_FROM_X'], K['CLIMB_FROM_Y'], 0, True
        else:
            lt = f - K['T_CLIMB_START']
            T  = K['T_CLIMB_TIME']
            x = lerp(K['CLIMB_FROM_X'], K['CLIMB_TO_X'], lt, T)
            y = lerp(K['CLIMB_FROM_Y'], K['CLIMB_TO_Y'], lt, T)
            y -= K['CLIMB_APEX'] * 4 * lt * (T - lt) // (T * T)
            pose = 0 if lt == 0 else 1
            big = (lt * 100) < (T * K['CLIMB_SWAP_PCT'])
        cells, cw, ch, ox, oy = ((climb,  K['CLIMB_W'],     K['CLIMB_H'],
                                  A['climbOffX'],    A['climbOffY'])
                                 if big else
                                 (climbf, K['CLIMB_FAR_W'], K['CLIMB_FAR_H'],
                                  A['climbFarOffX'], A['climbFarOffY']))
        # Leo (0) va adelante: se dibuja ultimo
        for i in (3, 2, 1, 0):
            blit(im, cells[i * 2 + pose],
                 x + ox[i] - cw // 2, y + oy[i] - ch)
    return im.convert('RGB')


def frame_b(turtle, t):
    """Un cuadro de la pasada `turtle` de la escena B (t en 0..T_PASS)."""
    im = bg_b.convert('RGBA')
    d = A['passDrift'][turtle]
    if t < K['T_PASS_TINY']:
        lt = t
        cell, w, h = tiny[turtle], K['TINY_W'], K['TINY_H']
        cx = accel(K['PASS_VP_X'], K['PASS_TINY_END_X'] + d, lt, K['T_PASS_TINY'])
        cy = accel(K['PASS_VP_Y'], K['PASS_TINY_END_Y'],     lt, K['T_PASS_TINY'])
    elif t < K['T_PASS_TINY'] + K['T_PASS_FAR']:
        lt = t - K['T_PASS_TINY']
        cell, w, h = far[turtle], K['FAR_W'], K['FAR_H']
        cx = accel(K['PASS_FAR_X'] + d, K['PASS_FAR_END_X'] + d, lt, K['T_PASS_FAR'])
        cy = accel(K['PASS_FAR_Y'],     K['PASS_FAR_END_Y'],     lt, K['T_PASS_FAR'])
    else:
        lt = t - K['T_PASS_TINY'] - K['T_PASS_FAR']
        cell, w, h = near[turtle], K['NEAR_W'], K['NEAR_H']
        cx = accel(K['PASS_NEAR_X'] + d, K['PASS_END_X'] + d, lt, K['T_PASS_NEAR'])
        cy = accel(K['PASS_NEAR_Y'],     K['PASS_END_Y'],     lt, K['T_PASS_NEAR'])
    blit(im, cell, cx - w // 2, cy - h // 2)
    return im.convert('RGB')


def frame_c(f):
    """Un cuadro de la escena C (hojas del juego, mismo reloj que el C)."""
    im = bg_c.convert('RGBA')
    draw = []
    for i in range(K['C_ARRIVALS']):
        e = f - A['arriveStart'][i]
        if e < 0:
            continue
        down = A['arriveDown'][i]
        end_fall = K['T_C_FALL']
        end_down = end_fall + (K['T_C_DOWN'] if down else 0)
        end_getup = end_down + (K['T_C_GETUP'] if down else 0)
        end_walk = end_getup + K['T_C_WALK']
        if e >= end_walk:
            continue

        t = A['arriveChar'][i]
        if e < end_fall:
            T = K['T_C_FALL']
            fx = lerp(K['C_FALL_FROM_X'], K['C_LAND_X'], e, T)
            fy = (lerp(K['C_FALL_FROM_Y'], K['C_LAND_Y'], e, T)
                  + accel(K['C_FALL_FROM_Y'], K['C_LAND_Y'], e, T)) // 2
            cell = gamecell(t, K['ANIM_JUMP'], K['C_JUMP_LAND_FRAME'])
        elif e < end_down:
            fx, fy = K['C_LAND_X'], K['C_LAND_Y']
            cell = gamecell(t, K['ANIM_HIT_BEHIND_2'], K['C_DOWN_FRAME'])
        elif e < end_getup:
            fx, fy = K['C_LAND_X'], K['C_LAND_Y']
            fr = min((e - end_down) // K['C_GETUP_ANIM_PERIOD'],
                     K['C_GETUP_FRAMES'] - 1)
            cell = gamecell(t, K['ANIM_GET_UP_2'], fr)
        else:
            lt = e - end_getup
            T = K['T_C_WALK']
            fx = lerp(K['C_LAND_X'], K['C_DOOR_X'], lt, T)
            fy = lerp(K['C_LAND_Y'], K['C_DOOR_Y'], lt, T)
            fy += A['arriveBow'][i] * K['C_WALK_BOW'] * 4 * lt * (T - lt) // (T * T)
            cell = gamecell(t, K['ANIM_WALK_FRONT'],
                            (lt // K['C_WALK_ANIM_PERIOD']) % K['C_WALK_FRAMES'])

        if K['C_WALK_FLIP']:
            cell = cell.transpose(Image.FLIP_LEFT_RIGHT)
        draw.append((fy, cell,
                     fx - K['PLAYER_SPRITE_W'] // 2, fy - K['PLAYER_FOOT_OFFSET']))

    for _, cell, x, y in sorted(draw, key=lambda e: e[0]):
        blit(im, cell, x, y)     # la que esta mas abajo tapa a las de atras
    return im.convert('RGB')


def frame_wipe(bg, step):
    """El wipe de entrada: el fondo revelado hasta el paso `step` de T_WIPE."""
    cols, rows = bg.width // 8, bg.height // 8
    w = cols * step // K['T_WIPE'] * 8
    h = rows * step // K['T_WIPE'] * 8
    im = Image.new('RGB', bg.size, (0, 0, 0))
    if w and h:
        im.paste(bg.crop((0, 0, w, h)), (0, 0))
    return im


# --------------------------------------------------------------------------
# Cuadros clave
# --------------------------------------------------------------------------
shots = []
shots.append(('A f000  arranque',                     frame_a(0)))
shots.append(('A f020  globo FIRE',                   frame_a(20)))
shots.append(('A f050  pausa',                        frame_a(50)))
shots.append(('A f062  globo HANG ON + impulso',      frame_a(62)))
shots.append(('A f080  vertice del salto',            frame_a(80)))
shots.append(('A f096  llegando al humo',             frame_a(96)))
shots.append(('A f115  solo Splinter',                frame_a(115)))
shots.append(('wipe 3/%d escena B' % K['T_WIPE'],     frame_wipe(bg_b, 3)))
shots.append(('B leo   punto de fuga (tiny)',         frame_b(0, 2)))
shots.append(('B leo   media distancia (far)',        frame_b(0, K['T_PASS_TINY'] + 3)))
shots.append(('B leo   encima de camara (near)',      frame_b(0, K['T_PASS_TINY'] + K['T_PASS_FAR'] + 3)))
shots.append(('B raph  saliendo de cuadro',           frame_b(3, K['T_PASS'] - 2)))
shots.append(('wipe 5/%d escena C' % K['T_WIPE'],     frame_wipe(bg_c, 5)))
shots.append(('C f012  Leo entra desde el humo',      frame_c(12)))
shots.append(('C f018  Leo por aterrizar',            frame_c(18)))
shots.append(('C f036  Leo camina, Don entrando',     frame_c(36)))
shots.append(('C f052  Don camina, Leo llegando',     frame_c(52)))
shots.append(('C f078  Mike cae encima de Raph',      frame_c(78)))
shots.append(('C f086  Mike tirado, Raph se va',      frame_c(86)))
shots.append(('C f096  Mike se levanta',              frame_c(96)))
shots.append(('C f120  Mike camino al vano',          frame_c(120)))

for i, (name, im) in enumerate(shots):
    im.save(os.path.join(FRM, '%02d_%s.png'
                         % (i, re.sub(r'[^a-z0-9]+', '_', name.lower()))))

# --------------------------------------------------------------------------
# Hoja de contacto (4 columnas, 2x)
# --------------------------------------------------------------------------
SC, PAD, COLS = 2, 8, 4
cw = SCR_W_C * SC
ch = SCR_H * SC + 14
rows = (len(shots) + COLS - 1) // COLS
sheet_im = Image.new('RGB', (COLS * (cw + PAD) + PAD,
                             rows * (ch + PAD) + PAD), (24, 24, 24))
try:
    from PIL import ImageDraw
    d = ImageDraw.Draw(sheet_im)
except Exception:
    d = None

for i, (name, im) in enumerate(shots):
    c, r = i % COLS, i // COLS
    x = PAD + c * (cw + PAD)
    y = PAD + r * (ch + PAD)
    big = im.resize((im.width * SC, im.height * SC), Image.NEAREST)
    sheet_im.paste(big, (x + (cw - big.width) // 2, y + 14))
    if d:
        d.text((x + 2, y + 2), name, fill=(255, 220, 120))

path = os.path.join(OUT, 'preview_roof.png')
sheet_im.save(path)
print('Constantes leidas del C: %d #define, %d tablas' % (len(K), len(A)))
print('Hoja de contacto -> %s' % path)
print('Cuadros sueltos  -> %s' % FRM)
