# -*- coding: utf-8 -*-
"""
gen_roof_smoke.py - Genera roof_bg_a_smoke_strip.png para el humo animado de
la Escena A de la cinematica de April.

Entrada: images/roof_april_scene/genesis/roof_bg_a_smoke.png (192x152, 3
frames de 64x152 lado a lado, exportado de Aseprite).

Salida: images/roof_april_scene/genesis/roof_bg_a_smoke_strip.png (64x456,
los mismos 3 frames apilados VERTICALMENTE, con la paleta forzada a ser
BYTE A BYTE igual a la de roof_bg_a.png).

Por que reordenar: TILESET con NONE NONE (sin dedup) extrae los tiles 8x8 en
orden raster de la imagen completa. Lado a lado, cada fila de tiles mezclaria
los 3 frames. Apilados, cada frame son 152 tiles CONTIGUOS en la ROM, mismo
truco que fire_strip.png / smoke_lvl1.png ya usan en este proyecto.

Por que forzar paleta, y por que el intento anterior salio mal: TILESET no
lleva paleta propia -- el pixel de cada tile es directamente un INDICE (0-15)
contra la paleta que ya este cargada en PAL0 en ese momento (la de
roof_bg_a). La primera version de este script armaba una paleta nueva
ordenando los colores del fondo alfabeticamente por RGB (`sorted(set(...))`)
-- eso NO es el orden real de indices de roof_bg_a.png (que es el orden en
que Aseprite/el generador los escribio en la tabla de paleta del PNG, sin
ningun criterio en particular). El resultado: el indice 3 del humo, por
ejemplo, terminaba correspondiendo a un color distinto del indice 3 real de
roof_bg_a.png -> "paleta rara" en el emulador aunque la animacion se moviera
bien.

Fix: usar `bg.getpalette()` TAL CUAL (orden nativo de indices de
roof_bg_a.png, sin reordenar) como la paleta contra la que se cuantiza el
humo. Asi el indice N de roof_bg_a_smoke_strip.png es, por construccion, el
mismo color que el indice N de roof_bg_a.png -- da igual si D visualmente
"parece" ordenado o no, lo que importa es que sea EL MISMO objeto de paleta.
"""
from PIL import Image
import os

BASE = os.path.join(os.path.dirname(__file__), '..', 'res', 'images',
                     'roof_april_scene', 'genesis')

bg = Image.open(os.path.join(BASE, 'roof_bg_a.png'))
assert bg.mode == 'P', 'roof_bg_a.png debe ser PNG indexado (paleta), no RGB'
sm = Image.open(os.path.join(BASE, 'roof_bg_a_smoke.png')).convert('RGB')

FW, FH, NFRAMES = 64, 152, 3
assert sm.size == (FW * NFRAMES, FH), sm.size

# --- Apilar los 3 frames verticalmente (para que TILESET NONE NONE los deje
#     contiguos en ROM, un frame = 152 tiles seguidos). ---
strip = Image.new('RGB', (FW, FH * NFRAMES))
for i in range(NFRAMES):
    frame = sm.crop((i * FW, 0, (i + 1) * FW, FH))
    strip.paste(frame, (0, i * FH))

# --- Cuantizar contra la paleta NATIVA de roof_bg_a.png, sin tocar el orden
#     de sus indices: pal_img usa exactamente bg.getpalette() (256 entradas,
#     las no usadas por bg quedan en 0 pero eso no importa, quantize matchea
#     por color mas cercano y el humo solo necesita los 12 que ya estan ahi). ---
pal_img = Image.new('P', (1, 1))
pal_img.putpalette(bg.getpalette())
strip_p = strip.quantize(palette=pal_img, dither=Image.NONE)

# Verificacion: todo indice usado por el humo tiene que ser un indice
# realmente usado por roof_bg_a (si no, algo se desvio del PAL0 real).
bg_used_idx = set(bg.getdata())
sm_used_idx = set(strip_p.getdata())
stray = sm_used_idx - bg_used_idx
if stray:
    raise SystemExit(f'ERROR: el humo uso indices que roof_bg_a.png no usa: {stray}')

out_path = os.path.join(BASE, 'roof_bg_a_smoke_strip.png')
strip_p.save(out_path)
print('wrote', out_path, strip_p.size,
      'indices de paleta usados (deben ser subconjunto de los de roof_bg_a):',
      sorted(sm_used_idx))
