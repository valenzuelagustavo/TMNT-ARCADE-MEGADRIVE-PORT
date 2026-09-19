# -*- coding: utf-8 -*-
"""
gen_intro_assets.py - Genera los assets de la intro arcade (SCENE_INTRO_ARCADE)
para la MegaDrive a partir del arte fuente de res/images/intro_tmnt/.

Salida: res/images/intro_tmnt/genesis/intro_*.png  (PNG indexados, <=16 colores,
paletas COMPARTIDAS entre los assets que conviven en pantalla, para no gastar
lineas de paleta de mas).

Uso:  python3 tools/gen_intro_assets.py
"""
import os
from PIL import Image

ROOT   = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC    = os.path.join(ROOT, 'res', 'images', 'intro_tmnt')
ASSETS = os.path.join(SRC, 'assets')
OUT    = os.path.join(SRC, 'genesis')
SPR    = os.path.join(ROOT, 'res', 'sprites')
os.makedirs(OUT, exist_ok=True)

# --------------------------------------------------------------------------
# Utilidades de color: la MegaDrive tiene 3 bits por componente (0,2,4..14).
# Cuantizamos a esa rejilla para que dos fuentes distintas que "ven" el mismo
# color terminen en la MISMA entrada de paleta.
# --------------------------------------------------------------------------
def md_quant(rgb):
    return tuple((c >> 5) * 36 for c in rgb[:3])

def load_rgba(path):
    return Image.open(path).convert('RGBA')

def build_indexed(rgba, palette, transparent=True):
    """Convierte una imagen RGBA a modo P usando 'palette' (lista de tuplas RGB
    ya cuantizadas). El indice 0 queda reservado a transparencia."""
    w, h = rgba.size
    out = Image.new('P', (w, h), 0)
    px_in  = rgba.load()
    px_out = out.load()
    # OJO: el indice 0 es SIEMPRE transparencia en la MegaDrive. Nunca puede ser
    # destino de un pixel opaco, aunque su color coincida (el negro del arte
    # tiene que vivir en un indice >= 1 o los contornos se volverian agujeros).
    lut = {c: i for i, c in enumerate(palette) if i > 0}
    for y in range(h):
        for x in range(w):
            r, g, b, a = px_in[x, y]
            if transparent and a < 128:
                px_out[x, y] = 0
                continue
            c = md_quant((r, g, b))
            i = lut.get(c)
            if i is None:                      # color no listado: al mas cercano
                i = min(range(1, len(palette)),
                        key=lambda k: sum((palette[k][j] - c[j]) ** 2 for j in range(3)))
                lut[c] = i
            px_out[x, y] = i
    flat = []
    for c in palette:
        flat += list(c)
    flat += [0, 0, 0] * (256 - len(palette))
    out.putpalette(flat)
    return out

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
                if c not in pal[1:]:      # pal[0] esta reservado a transparencia
                    pal.append(c)
    return pal

def report(name, im):
    w, h = im.size
    print('  %-22s %4dx%-4d  %2d colores  %3dx%-3d tiles'
          % (name, w, h, len(set(im.tobytes())), w // 8, h // 8))

# ==========================================================================
# 1) BLOQUE A/B/C  -- dolly vertical + haz de luz + tapa de alcantarilla
#    Los tres comparten la paleta de intro.png (PAL0).
# ==========================================================================
DOLLY_X0 = 24          # recorte horizontal: 304 -> 256 (pantalla H32)
DOLLY_W  = 256

dolly_src = load_rgba(os.path.join(SRC, 'intro.png')).crop(
    (DOLLY_X0, 0, DOLLY_X0 + DOLLY_W, Image.open(os.path.join(SRC, 'intro.png')).height))
luz_src   = load_rgba(os.path.join(ASSETS, 'luz.png'))
tapa_src  = load_rgba(os.path.join(ASSETS, 'tapa_alcantarilla.png'))

pal_abc = collect_colors([dolly_src, luz_src, tapa_src])
assert len(pal_abc) <= 16, 'PAL0 de la intro se paso de 16 colores: %d' % len(pal_abc)

dolly = build_indexed(dolly_src, pal_abc, transparent=False)
dolly.save(os.path.join(OUT, 'intro_dolly.png')); report('intro_dolly.png', dolly)

luz = build_indexed(luz_src, pal_abc)
luz.save(os.path.join(OUT, 'intro_luz.png')); report('intro_luz.png', luz)

# tapa: 62x31 -> lienzo 64x32 (8x4 tiles) centrada, resto transparente
tapa_c = Image.new('RGBA', (64, 32), (0, 0, 0, 0))
tapa_c.paste(tapa_src, (1, 0), tapa_src)
tapa = build_indexed(tapa_c, pal_abc)
tapa.save(os.path.join(OUT, 'intro_tapa.png')); report('intro_tapa.png', tapa)
print('  PAL0 A/B/C: %d colores' % len(pal_abc))

# ==========================================================================
# 1.5) ESCENA A -- nubes que cruzan el cielo por debajo de la luna
#      Paleta PROPIA (PAL2): la PAL0 de dolly/luz/tapa ya esta al limite de
#      16 colores (ver arriba), no hay lugar para sumarles las nubes ahi.
# ==========================================================================
nube_chica_src  = load_rgba(os.path.join(SRC, 'nube_chica.png'))
nube_grande_src = load_rgba(os.path.join(SRC, 'nube_grande.png'))

pal_nubes = collect_colors([nube_chica_src, nube_grande_src])
assert len(pal_nubes) <= 16, 'La paleta de las nubes se paso de 16: %d' % len(pal_nubes)

nube_chica = build_indexed(nube_chica_src, pal_nubes)
nube_chica.save(os.path.join(OUT, 'intro_nube_chica.png')); report('intro_nube_chica.png', nube_chica)

nube_grande = build_indexed(nube_grande_src, pal_nubes)
nube_grande.save(os.path.join(OUT, 'intro_nube_grande.png')); report('intro_nube_grande.png', nube_grande)
print('  PAL2 nubes (escena A): %d colores' % len(pal_nubes))

# ==========================================================================
# 2) ESCENA C -- spritesheet reducido del salto de las 4 tortugas
#    Fila 6 (ANIM_JUMP) de cada hoja, frames 1..8 (se descarta el 0 = despegue
#    y el 9 = aterrizaje), recortados al bbox comun y llevados a multiplo de 8.
# ==========================================================================
CELL      = 104
JUMP_ROW  = 6
# Frames 1..9: el 0 (despegue apoyado en el piso) se descarta y el 9 es la pose
# de aterrizaje, que se usa como remate cuando la tortuga toca el suelo.
JUMP_FR   = range(1, 10)             # 8 frames aereos + 1 de aterrizaje
# El recorte llega hasta y=101 para que entre el frame de aterrizaje completo:
# la linea de piso del arte original (y~95) cae en y=73 de la celda.
CROP_X, CROP_Y, CROP_W, CROP_H = 13, 22, 72, 80
ORDER = ['leo', 'mike', 'don', 'raph']   # mismo orden que personajeSeleccionado

sheet = Image.new('RGBA', (CROP_W * len(list(JUMP_FR)), CROP_H * len(ORDER)), (0, 0, 0, 0))
for r, name in enumerate(ORDER):
    src = load_rgba(os.path.join(SPR, '%s_anim_13x13.png' % name))
    for i, f in enumerate(JUMP_FR):
        cell = src.crop((f * CELL + CROP_X, JUMP_ROW * CELL + CROP_Y,
                         f * CELL + CROP_X + CROP_W, JUMP_ROW * CELL + CROP_Y + CROP_H))
        sheet.paste(cell, (i * CROP_W, r * CROP_H), cell)

# La paleta de las 4 tortugas es la MISMA (PAL1 unificada del juego): la copiamos
# tal cual desde la hoja original para no re-cuantizar nada.
leo_p = Image.open(os.path.join(SPR, 'leo_anim_13x13.png'))
pal_turtles = [tuple(leo_p.getpalette()[i * 3:i * 3 + 3]) for i in range(16)]
turtles = build_indexed(sheet, [md_quant(c) for c in pal_turtles])
turtles.save(os.path.join(OUT, 'intro_turtles.png')); report('intro_turtles.png', turtles)

# ==========================================================================
# 3) ESCENA D -- 4 retratos en cuadrantes (256x224, una sola paleta)
# ==========================================================================
QUAD_W, QUAD_H = 128, 112
# (archivo, columna, fila)  TL=Leo  TR=Don  BL=Mike  BR=Raph
QUADS = [('leo_image.png', 0, 0), ('don_image.png', 1, 0),
         ('mike_image.png', 0, 1), ('raph_image.png', 1, 1)]

quad_srcs = [load_rgba(os.path.join(ASSETS, f)) for f, _, _ in QUADS]
pal_quad = collect_colors(quad_srcs)
assert len(pal_quad) <= 16, 'La paleta de los 4 retratos se paso de 16: %d' % len(pal_quad)

canvas = Image.new('RGBA', (256, 224), (0, 0, 0, 255))
for (f, cx, cy), im in zip(QUADS, quad_srcs):
    bg = im.getpixel((0, 0))                       # color de fondo del cuadrante
    tile = Image.new('RGBA', (QUAD_W, QUAD_H), bg)
    tile.paste(im, ((QUAD_W - im.width) // 2, (QUAD_H - im.height) // 2), im)
    canvas.paste(tile, (cx * QUAD_W, cy * QUAD_H))
quad = build_indexed(canvas, pal_quad, transparent=False)
quad.save(os.path.join(OUT, 'intro_quad.png')); report('intro_quad.png', quad)
print('  PAL0 escena D: %d colores' % len(pal_quad))

# ==========================================================================
# 4) ESCENAS E/F -- banner, logo TURTLES y copyright de Konami
#    Los tres comparten paleta (PAL0) + el celeste de fondo como color 15.
# ==========================================================================
banner_src = load_rgba(os.path.join(ASSETS, 'teenage_mutant_ninja.png'))
logo_full  = load_rgba(os.path.join(ASSETS, 'logo_.png'))
scr        = load_rgba(os.path.join(ASSETS, 'logo_screen.png'))

# El logo trae antialias: todo lo que no es opaco se descarta (la MD no tiene alpha)
lp = logo_full.load()
for y in range(logo_full.height):
    for x in range(logo_full.width):
        r, g, b, a = lp[x, y]
        lp[x, y] = (r, g, b, 255 if a > 200 else 0)
logo_src = logo_full.crop((37, 64, 293, 144))          # 256x80, alineado a tile

# copyright: recorte del renglon de logo_screen, con el celeste como transparente
kon_box = (44, 144, 44 + 168, 152)                      # 168x8 (21x1 tiles)
kon = scr.crop(kon_box).convert('RGBA')
sky = md_quant(scr.getpixel((0, 0)))
kp = kon.load()
for y in range(kon.height):
    for x in range(kon.width):
        r, g, b, a = kp[x, y]
        if md_quant((r, g, b)) == sky:
            kp[x, y] = (0, 0, 0, 0)

# Si logo_screen.png quedo liso (sin el renglon de copyright), el recorte sale
# 100% transparente: no rompe nada (es 1 tile invisible) pero hay que avisar.
if not any(kon.getpixel((x, y))[3] for y in range(kon.height) for x in range(kon.width)):
    print('  AVISO: logo_screen.png no tiene el renglon "KONAMI (c) KONAMI 1989"')
    print('         -> intro_konami.png sale vacio y la escena F queda sin copyright.')

pal_ef = collect_colors([banner_src, logo_src, kon])
if sky not in pal_ef:
    pal_ef.append(sky)                                  # celeste = color de fondo
SKY_INDEX = pal_ef.index(sky)
assert len(pal_ef) <= 16, 'La paleta de las escenas E/F se paso de 16: %d' % len(pal_ef)

build_indexed(banner_src, pal_ef).save(os.path.join(OUT, 'intro_banner.png'))
report('intro_banner.png', Image.open(os.path.join(OUT, 'intro_banner.png')))
build_indexed(logo_src, pal_ef).save(os.path.join(OUT, 'intro_logo.png'))
report('intro_logo.png', Image.open(os.path.join(OUT, 'intro_logo.png')))
build_indexed(kon, pal_ef).save(os.path.join(OUT, 'intro_konami.png'))
report('intro_konami.png', Image.open(os.path.join(OUT, 'intro_konami.png')))
print('  PAL0 escenas E/F: %d colores | indice del celeste de fondo = %d'
      % (len(pal_ef), SKY_INDEX))
print()
print('OJO: INTRO_SKY_PAL_INDEX en src/intro_arcade.c debe valer %d' % SKY_INDEX)
