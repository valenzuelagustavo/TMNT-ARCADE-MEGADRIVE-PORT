# -*- coding: utf-8 -*-
"""
gen_roof_assets.py - Genera los assets de la cinematica del rescate de April
(SCENE_CINEMATIC_FIRE) para la MegaDrive a partir del arte suelto de
res/images/roof_april_scene/Assets/.

Salida: res/images/roof_april_scene/genesis/roof_*.png (PNG indexados, <=16
colores, indice 0 SIEMPRE reservado a transparencia, paletas COMPARTIDAS entre
los assets que conviven en pantalla para no gastar lineas de paleta de mas).

Mapa de paletas que produce (ver cinematic_fire.c):
    Escena A : PAL0 roof_bg_a | PAL1 roof_climb | PAL2 roof_splinter | PAL3 roof_baloon
    Escena B : PAL0 roof_bg_b | PAL1 roof_far + roof_near (MISMA paleta)
    Escena C : PAL0 roof_bg_c | PAL1 roof_far + roof_near (MISMA paleta)

Uso:  python3 tools/gen_roof_assets.py
"""
import os
from PIL import Image

ROOT   = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC    = os.path.join(ROOT, 'res', 'images', 'roof_april_scene')
ASSETS = os.path.join(SRC, 'Assets')
OUT    = os.path.join(SRC, 'genesis')
os.makedirs(OUT, exist_ok=True)

# Orden de las tortugas = el del resto del juego (personajeSeleccionado)
TURTLES = ['leo', 'mike', 'don', 'raph']

# --------------------------------------------------------------------------
# Utilidades de color: la MegaDrive tiene 3 bits por componente (0,2,4..14).
# Cuantizamos a esa rejilla para que dos fuentes distintas que "ven" el mismo
# color caigan en la MISMA entrada de paleta.
# --------------------------------------------------------------------------
def md_quant(rgb):
    return tuple((c >> 5) * 36 for c in rgb[:3])


def load_rgba(name):
    """Abre un PNG de Assets/ respetando su indice transparente."""
    return Image.open(os.path.join(ASSETS, name)).convert('RGBA')


def collect_colors(images, reserve0=(0, 0, 0)):
    """Paleta compartida: indice 0 reservado (transparencia) + colores usados."""
    pal = [reserve0]
    for im in images:
        px = im.load()
        w, h = im.size
        for y in range(h):
            for x in range(w):
                r, g, b, a = px[x, y]
                if a < 128:
                    continue
                c = md_quant((r, g, b))
                if c not in pal[1:]:      # pal[0] esta reservado
                    pal.append(c)
    return pal


def build_indexed(rgba, palette, transparent=True):
    """RGBA -> modo P usando 'palette' (lista de RGB ya cuantizados).

    OJO: el indice 0 es SIEMPRE transparencia en la MegaDrive. Nunca puede ser
    destino de un pixel opaco aunque el color coincida (el negro del contorno
    tiene que vivir en un indice >= 1 o los contornos se vuelven agujeros)."""
    w, h = rgba.size
    out = Image.new('P', (w, h), 0)
    px_in, px_out = rgba.load(), out.load()
    lut = {c: i for i, c in enumerate(palette) if i > 0}
    for y in range(h):
        for x in range(w):
            r, g, b, a = px_in[x, y]
            if transparent and a < 128:
                px_out[x, y] = 0
                continue
            c = md_quant((r, g, b))
            i = lut.get(c)
            if i is None:                  # color no listado: al mas cercano
                i = min(range(1, len(palette)),
                        key=lambda k: sum((palette[k][j] - c[j]) ** 2 for j in range(3)))
                lut[c] = i
            px_out[x, y] = i
    flat = []
    for c in palette:
        flat += list(c)
    flat += [0, 0, 0] * (256 - len(palette))
    out.putpalette(flat)
    if transparent:
        # Declarar el indice 0 como transparente en el propio PNG. rescomp no
        # lo necesita (en la MegaDrive el indice 0 es transparente por
        # hardware), pero sin esto el arte se abre con fondo negro opaco en
        # Aseprite y en tools/preview_roof.py.
        out.info['transparency'] = 0
    return out


def unique_tiles(im):
    """Tiles 8x8 unicos (lo que realmente va a gastar en VRAM tras el dedup)."""
    px, (w, h) = im.load(), im.size
    seen = set()
    for ty in range(h // 8):
        for tx in range(w // 8):
            seen.add(bytes(px[tx * 8 + x, ty * 8 + y]
                           for y in range(8) for x in range(8)))
    return len(seen)


def save(name, im, note=''):
    path = os.path.join(OUT, name)
    if 'transparency' in im.info:
        im.save(path, transparency=0)
    else:
        im.save(path)
    w, h = im.size
    print('  %-20s %3dx%-3d  %2d col  %2dx%-2d tiles  unicos=%4d  %s'
          % (name, w, h, len(set(im.tobytes())), w // 8, h // 8,
             unique_tiles(im), note))


def bbox_opaque(rgba):
    """Caja del contenido opaco (None si esta todo transparente)."""
    px, (w, h) = rgba.load(), rgba.size
    x0, y0, x1, y1 = w, h, -1, -1
    for y in range(h):
        for x in range(w):
            if px[x, y][3] >= 128:
                if x < x0: x0 = x
                if y < y0: y0 = y
                if x > x1: x1 = x
                if y > y1: y1 = y
    return None if x1 < 0 else (x0, y0, x1, y1)


def paste_centered(src, cell_w, cell_h):
    """Pega el CONTENIDO de src centrado en una celda cell_w x cell_h.

    Centrar por la caja del contenido es lo que hace que el swap chico->grande
    de la escena B no pegue un salto: las dos versiones son la MISMA pose a dos
    tamanos, asi que si las dos quedan centradas en su celda alcanza con dibujar
    las dos celdas con el mismo centro de pantalla."""
    out = Image.new('RGBA', (cell_w, cell_h), (0, 0, 0, 0))
    bb = bbox_opaque(src)
    if bb is None:
        return out
    x0, y0, x1, y1 = bb
    crop = src.crop((x0, y0, x1 + 1, y1 + 1))
    out.paste(crop, ((cell_w - crop.width) // 2, (cell_h - crop.height) // 2))
    return out


# ==========================================================================
# 1) FONDOS - uno por escena, cada uno con su propia paleta (PAL0)
# ==========================================================================
# Los tres son opacos de punta a punta y NO usan el indice 0, asi que se
# reindexan tal cual: el indice 0 queda libre y el color de backdrop del VDP
# (negro) es lo que se ve durante los cortes y el wipe.
print('Fondos (PAL0 de cada escena):')
for out_name, src_name in (('roof_bg_a.png', 'april_building_a.png'),
                           ('roof_bg_b.png', 'sky_buildings.png'),
                           ('roof_bg_c.png', 'april_building_b.png')):
    rgba = load_rgba(src_name)
    pal  = collect_colors([rgba])
    assert len(pal) <= 16, '%s no entra en 16 colores (%d)' % (src_name, len(pal))
    save(out_name, build_indexed(rgba, pal, transparent=False), src_name)

# ==========================================================================
# 2) ESCENA A - grupo trepando: hoja de las 4 tortugas "de espaldas"
# ==========================================================================
# leo/mike/don/raph.png traen 2 poses de 56x64 lado a lado (salto y aterrizaje
# con sombra). Se copian TAL CUAL, sin recentrar: la diferencia de altura entre
# las dos poses es justamente lo que da la sensacion de salto al alternarlas.
# Hoja resultante: 112x256 = 2 frames x 4 filas (0=Leo 1=Mike 2=Don 3=Raph).
CLIMB_W, CLIMB_H = 56, 64
print('Escena A - hoja del grupo trepando (PAL1):')
climb_src = [load_rgba(t + '.png') for t in TURTLES]
climb_pal = collect_colors(climb_src)
assert len(climb_pal) <= 16, 'paleta unificada chica: %d colores' % len(climb_pal)
climb = Image.new('RGBA', (CLIMB_W * 2, CLIMB_H * len(TURTLES)), (0, 0, 0, 0))
for row, src in enumerate(climb_src):
    climb.paste(src, (0, row * CLIMB_H))
save('roof_climb.png', build_indexed(climb, climb_pal),
     '%d colores compartidos' % (len(climb_pal) - 1))

# Segundo tamano del grupo: los dos ultimos saltos ya son sobre la fachada, mas
# lejos de camara, y con un solo tamano las tortugas quedaban de 5 pisos de
# alto sobre el edificio. Se reduce la celda ENTERA a 2/3 (no el contenido
# recortado) para que las dos poses conserven su alineacion relativa y el salto
# no tiemble; queda alineada abajo, asi la linea de piso sigue siendo el borde
# inferior de la celda, igual que en roof_climb.
CLIMB_FAR_W, CLIMB_FAR_H = 40, 48
_cw, _ch = CLIMB_W * 2 // 3, CLIMB_H * 2 // 3        # 37x42
climb_far = Image.new('RGBA', (CLIMB_FAR_W * 2, CLIMB_FAR_H * len(TURTLES)),
                      (0, 0, 0, 0))
for row, src in enumerate(climb_src):
    for fr in range(2):
        cell = src.crop((fr * CLIMB_W, 0, (fr + 1) * CLIMB_W, CLIMB_H))
        small = cell.resize((_cw, _ch), Image.NEAREST)
        climb_far.paste(small, (fr * CLIMB_FAR_W + (CLIMB_FAR_W - _cw) // 2,
                                row * CLIMB_FAR_H + (CLIMB_FAR_H - _ch)))
save('roof_climb_far.png', build_indexed(climb_far, climb_pal),
     'misma paleta que roof_climb')

# ==========================================================================
# 3) ESCENAS B/C - pasada de las tortugas: hojas "lejos" y "cerca"
# ==========================================================================
# *_scale.png trae la MISMA pose a dos tamanos (izquierda chica, derecha
# grande): es el truco de "zoom" del arcade, que no escala por hardware sino
# que sustituye el sprite. Se parten en dos hojas separadas para no gastar
# VRAM de sprite del tamano grande mientras la tortuga esta lejos.
#   roof_far.png   72x88  = 9x11 tiles =  99 tiles/frame
#   roof_near.png 104x120 = 13x15 tiles = 195 tiles/frame
# Las DOS comparten paleta (salen del mismo PNG) -> una sola linea, PAL1.
FAR_W,  FAR_H  =  72,  88
NEAR_W, NEAR_H = 104, 120
print('Escenas B/C - hojas lejos/cerca (PAL1 compartida):')
scale_src = [load_rgba(t + '_scale.png') for t in TURTLES]
scale_pal = collect_colors(scale_src)
assert len(scale_pal) <= 16, 'paleta unificada grande: %d colores' % len(scale_pal)

far  = Image.new('RGBA', (FAR_W,  FAR_H  * len(TURTLES)), (0, 0, 0, 0))
near = Image.new('RGBA', (NEAR_W, NEAR_H * len(TURTLES)), (0, 0, 0, 0))
for row, src in enumerate(scale_src):
    half = src.width // 2
    far.paste(paste_centered(src.crop((0, 0, half, src.height)), FAR_W, FAR_H),
              (0, row * FAR_H))
    near.paste(paste_centered(src.crop((half, 0, src.width, src.height)), NEAR_W, NEAR_H),
               (0, row * NEAR_H))
save('roof_far.png',  build_indexed(far,  scale_pal),
     '%d colores compartidos' % (len(scale_pal) - 1))
save('roof_near.png', build_indexed(near, scale_pal), 'misma paleta que roof_far')

# Tercer escalon de tamano. El arte suelto trae solo DOS (lejos y cerca), y con
# dos el salto se nota: en el punto de fuga de la escena B la tortuga ya entra
# enorme. roof_tiny es roof_far reducido a 2/3 por vecino mas cercano (que es
# como se redibujaria a mano en pixel art: sin colores nuevos), y da la cadena
# tiny -> far -> near, que es la que vende el acercamiento.
TINY_W, TINY_H = 48, 64
TINY_NUM, TINY_DEN = 2, 3
tiny = Image.new('RGBA', (TINY_W, TINY_H * len(TURTLES)), (0, 0, 0, 0))
for row, src in enumerate(scale_src):
    half = src.crop((0, 0, src.width // 2, src.height))
    bb = bbox_opaque(half)
    crop = half.crop((bb[0], bb[1], bb[2] + 1, bb[3] + 1))
    small = crop.resize((max(1, crop.width  * TINY_NUM // TINY_DEN),
                         max(1, crop.height * TINY_NUM // TINY_DEN)),
                        Image.NEAREST)
    assert small.width <= TINY_W and small.height <= TINY_H, \
        'roof_tiny: %s no entra en la celda' % TURTLES[row]
    tiny.paste(small, ((TINY_W - small.width) // 2,
                       row * TINY_H + (TINY_H - small.height) // 2))
save('roof_tiny.png', build_indexed(tiny, scale_pal), 'misma paleta que roof_far')

# ==========================================================================
# 4) ESCENA A - Splinter (PAL2) y globos de dialogo (PAL3)
# ==========================================================================
# Splinter.png son 2 poses de 44x64 (idle de respiracion). 44 px no es multiplo
# de 8: se pasa a celdas de 48x64 pegando cada mitad con el MISMO offset (+2),
# asi las dos poses conservan su alineacion relativa y el baston no salta.
SPL_W, SPL_H, SPL_PAD = 48, 64, 2
print('Escena A - Splinter (PAL2) y globos (PAL3):')
spl_src = load_rgba('Splinter.png')
spl_pal = collect_colors([spl_src])
spl = Image.new('RGBA', (SPL_W * 2, SPL_H), (0, 0, 0, 0))
for i in range(2):
    half = spl_src.crop((i * 44, 0, (i + 1) * 44, SPL_H))
    spl.paste(half, (i * SPL_W + SPL_PAD, 0))
save('roof_splinter.png', build_indexed(spl, spl_pal),
     '2 poses de 48x64 (idle)')

# Los dos globos comparten paleta y viven en la misma hoja: son 2 frames de la
# misma animacion, se muestran/ocultan sin animar.
bal_src = [load_rgba('baloon_fire.png'), load_rgba('baloon_hang_on.png')]
bal_pal = collect_colors(bal_src)
bal = Image.new('RGBA', (64 * 2, 32), (0, 0, 0, 0))
for i, src in enumerate(bal_src):
    bal.paste(src, (i * 64, 0))
save('roof_baloon.png', build_indexed(bal, bal_pal),
     'frame 0 = FIRE!!, frame 1 = HANG ON APRIL')

print('\nListo. Recorda que rescomp deduplica tiles: los "unicos" de arriba son'
      '\nlo que cada asset va a ocupar de verdad en VRAM.')
