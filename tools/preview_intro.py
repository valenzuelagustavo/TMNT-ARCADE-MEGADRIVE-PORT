# -*- coding: utf-8 -*-
"""
preview_intro.py - Render offline de la intro arcade, con las MISMAS constantes
que src/intro_arcade.c. No toca la ROM: sirve para validar encuadres, tiempos y
posiciones antes de compilar. Genera _tmp_intro/preview_intro.png.
"""
import os
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN  = os.path.join(ROOT, 'res', 'images', 'intro_tmnt', 'genesis')
OUT  = os.path.join(ROOT, '_tmp_intro')
os.makedirs(OUT, exist_ok=True)

W, H = 256, 224
# --- constantes espejadas de intro_arcade.c ---
RAIN_ROW_START, RAIN_PERIOD, RAIN_INSERT_ROW, RAIN_EXTRA_LOOPS = 87, 8, 95, 4
RAIN_EXTRA_ROWS = RAIN_EXTRA_LOOPS * RAIN_PERIOD
T_SKY, T_HOLD, T_FLASH, T_BEAM, T_JUMP = 200, 2, 3, 10, 77
T_QGROW, T_QHOLD, T_SKYH, T_BFALL, T_BANNER, T_WIPE, T_LHOLD = 32, 120, 15, 25, 82, 13, 105
BANNER_FALL_FROM, BANNER_BOUNCE, BOUNCE_TICKS = 64, 5, 8
DOLLY_END = (1496 - H) + RAIN_EXTRA_ROWS * 8
T_DOLLY   = (DOLLY_END * 256) // 1272
HOLE_CX, HOLE_CY = 133, 198
TAPA_W, TAPA_H = 64, 32
TAPA_X, TAPA_Y = HOLE_CX - TAPA_W // 2, HOLE_CY - TAPA_H // 2
BEAM_COL, BEAM_SHIFT, BEAM_MAX, BEAM_MIN = 13, 2, 26, 3
TW, TH, TFOOT, TFRAMES, TLAND, APEX = 72, 80, 73, 8, 8, 72
DSTX  = [156, 44, 100, 212]
DSTY  = [202, 194, 206, 198]
DELAY = [14, 0, 6, 10]
FLIP  = [0, 1, 1, 0]
TIME  = [46, 52, 50, 48]
QCOLS, QROWS = 16, 14
BANNER_COL, BANNER_ROW = 3, 5
LOGO_COL, LOGO_ROW = 0, 8
KON_COL, KON_ROW = 5, 18

def load(name, transparent=True):
    """Carga un PNG indexado emulando el VDP: el indice 0 no se dibuja."""
    im = Image.open(os.path.join(GEN, name))
    rgba = im.convert('RGBA')
    if transparent:
        px_i, px_o = im.load(), rgba.load()
        for y in range(im.height):
            for x in range(im.width):
                if px_i[x, y] == 0:
                    px_o[x, y] = (0, 0, 0, 0)
    return rgba

dolly   = load('intro_dolly.png',   False)   # opaco, va de fondo en BG_B
quad    = load('intro_quad.png',    False)
luz     = load('intro_luz.png')
tapa    = load('intro_tapa.png')
turtles = load('intro_turtles.png')
banner  = load('intro_banner.png')
logo    = load('intro_logo.png')
konami  = load('intro_konami.png')
SKY = konami.convert('P').getpalette()  # no usado; el celeste se toma del generador
SKY_RGB = (73, 146, 219)

def dolly_window(sy):
    """Reproduce el streamer: arma la ventana de 224 px a partir de las filas
    VIRTUALES, ciclando el bloque de lluvia como hace dollyMapRow() en el C."""
    out = Image.new('RGBA', (W, H), (0, 0, 0, 255))
    top = sy // 8
    for i in range(H // 8 + 1):
        v = top + i
        if v < RAIN_INSERT_ROW:                      src = v
        elif v < RAIN_INSERT_ROW + RAIN_EXTRA_ROWS:  src = RAIN_ROW_START + ((v - RAIN_INSERT_ROW) % RAIN_PERIOD)
        else:                                        src = v - RAIN_EXTRA_ROWS
        src = min(src, dolly.height // 8 - 1)
        out.paste(dolly.crop((0, src * 8, W, src * 8 + 8)), (0, i * 8 - (sy % 8)))
    return out

def beam_layer(rows):
    """Reproduce beamDraw(): base fija sobre el pozo + cuerpo repetido arriba."""
    lay = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    base_top = 28 - 1 - BEAM_SHIFT - 2          # fila 25
    x = BEAM_COL * 8
    for i in range(3):                           # base redondeada
        src = luz.crop((0, (25 + i) * 8, 64, (26 + i) * 8))
        lay.paste(src, (x, (base_top + i) * 8), src)
    for i in range(3, rows):                     # cuerpo
        r = base_top - (i - 2)
        if 0 <= r < 28:
            src = luz.crop((0, 0, 64, 8))
            lay.paste(src, (x, r * 8), src)
    return lay

def turtle_at(i, lt):
    T = TIME[i]
    if lt < T:
        x  = HOLE_CX + (DSTX[i] - HOLE_CX) * lt // T
        fy = HOLE_CY + (DSTY[i] - HOLE_CY) * lt // T - (APEX * 4 * lt * (T - lt)) // (T * T)
        fr = min(lt * TFRAMES // T, TFRAMES - 1)
    else:
        x, fy, fr = DSTX[i], DSTY[i], TLAND
    cell = turtles.crop((fr * TW, i * TH, (fr + 1) * TW, (i + 1) * TH))
    if FLIP[i]:
        cell = cell.transpose(Image.FLIP_LEFT_RIGHT)
    return cell, x - TW // 2, fy - TFOOT

def frame(tick):
    """Devuelve el cuadro de pantalla (256x224) para el tick global dado."""
    f = Image.new('RGBA', (W, H), (0, 0, 0, 255))
    t = tick
    if t < T_SKY:                                            # A
        f.paste(dolly_window(0), (0, 0)); return f, 'A t=%d' % t
    t -= T_SKY
    if t < T_DOLLY:                                          # B
        u = t * 256 // T_DOLLY
        s = (3 * u * u) // 256 - (u * u * u) // 32768
        sy = DOLLY_END * s // 256
        f.paste(dolly_window(sy), (0, 0))
        ty = DOLLY_END + TAPA_Y - sy
        if ty < H: f.paste(tapa, (TAPA_X, ty), tapa)
        return f, 'B t=%d sY=%d' % (t, sy)
    t -= T_DOLLY
    bottom = dolly_window(DOLLY_END)
    if t < T_HOLD:                                           # tapa quieta
        f.paste(bottom, (0, 0)); f.paste(tapa, (TAPA_X, TAPA_Y), tapa)
        return f, 'tapa t=%d' % t
    t -= T_HOLD
    if t < T_FLASH:                                          # flash
        return Image.new('RGBA', (W, H), (238, 238, 238, 255)), 'FLASH'
    t -= T_FLASH
    if t < T_BEAM:                                           # crece el haz
        rows = BEAM_MIN + (BEAM_MAX - BEAM_MIN) * (t + 1) // T_BEAM
        f.paste(bottom, (0, 0))
        lay = beam_layer(rows); f.paste(lay, (0, 0), lay)
        tx, ty = TAPA_X + 2 * (t + 1), TAPA_Y - 7 * (t + 1)
        f.paste(tapa, (tx, ty), tapa)
        return f, 'haz t=%d (%d filas)' % (t, rows)
    t -= T_BEAM
    if t < T_JUMP:                                           # C
        f.paste(bottom, (0, 0))
        lay = beam_layer(BEAM_MAX); f.paste(lay, (0, 0), lay)
        tx, ty = TAPA_X + 2 * (T_BEAM + t + 1), TAPA_Y - 7 * (T_BEAM + t + 1)
        if ty + TAPA_H > 0: f.paste(tapa, (tx, ty), tapa)
        for i in (3, 2, 1, 0):                               # Leo (0) ultimo = al frente
            lt = t - DELAY[i]
            if lt < 0: continue
            cell, x, y = turtle_at(i, lt)
            f.paste(cell, (x, y), cell)
        return f, 'C t=%d' % t
    t -= T_JUMP
    if t < T_QGROW:                                          # D crece
        px = [quad.getpixel((0, 0)), quad.getpixel((W - 1, 0)),
              quad.getpixel((0, H - 1)), quad.getpixel((W - 1, H - 1))]
        w = QCOLS * (t + 1) // T_QGROW * 8
        h = QROWS * (t + 1) // T_QGROW * 8
        d = ImageDraw.Draw(f)
        if w and h:
            d.rectangle([0, 0, w - 1, h - 1], fill=px[0])
            d.rectangle([W - w, 0, W - 1, h - 1], fill=px[1])
            d.rectangle([0, H - h, w - 1, H - 1], fill=px[2])
            d.rectangle([W - w, H - h, W - 1, H - 1], fill=px[3])
        return f, 'D crece t=%d' % t
    t -= T_QGROW
    if t < T_QHOLD:
        f.paste(quad, (0, 0)); return f, 'D t=%d' % t
    t -= T_QHOLD
    sky = Image.new('RGBA', (W, H), SKY_RGB + (255,))
    if t < T_SKYH:
        return sky, 'E celeste t=%d' % t
    t -= T_SKYH
    if t < T_BFALL:                                          # E: cae el banner
        fall = T_BFALL - BOUNCE_TICKS
        if t < fall:
            p = (t + 1) * 256 // fall
            sy = BANNER_FALL_FROM * (65536 - p * p) // 65536
        else:
            b, half = t - fall, BOUNCE_TICKS // 2
            sy = (BANNER_BOUNCE * (b + 1) // half) if b < half else \
                 (BANNER_BOUNCE * (BOUNCE_TICKS - b - 1) // half)
        sky.paste(banner, (BANNER_COL * 8, BANNER_ROW * 8 - sy), banner)
        return sky, 'E cae t=%d (y=%d)' % (t, BANNER_ROW * 8 - sy)
    t -= T_BFALL
    if t < T_BANNER:
        sky.paste(banner, (BANNER_COL * 8, BANNER_ROW * 8), banner)
        return sky, 'E banner t=%d' % t
    t -= T_BANNER
    sky.paste(banner, (BANNER_COL * 8, BANNER_ROW * 8), banner)
    if t < T_WIPE:                                           # F wipe del logo
        cols = logo.width // 8
        target = cols - cols * (t + 1) // T_WIPE
        part = logo.crop((target * 8, 0, logo.width, logo.height))
        sky.paste(part, (LOGO_COL * 8 + target * 8, LOGO_ROW * 8), part)
        return sky, 'F wipe t=%d' % t
    t -= T_WIPE
    sky.paste(logo, (LOGO_COL * 8, LOGO_ROW * 8), logo)
    sky.paste(konami, (KON_COL * 8, KON_ROW * 8), konami)
    return sky, 'F final t=%d' % t

TICKS = [0, 250, 330, 360, 390, 420, 470, 503, 508, 510, 514, 519,
         526, 536, 561, 581, 598, 631, 751, 771, 877, 881, 991]
cols, rows = 4, (len(TICKS) + 3) // 4
sheet = Image.new('RGB', (cols * (W + 8) + 8, rows * (H + 22) + 8), (24, 24, 28))
d = ImageDraw.Draw(sheet)
for n, tk in enumerate(TICKS):
    img, label = frame(tk)
    x = 8 + (n % cols) * (W + 8)
    y = 8 + (n // cols) * (H + 22)
    sheet.paste(img.convert('RGB'), (x, y))
    d.text((x + 2, y + H + 4), 'tick %d  %s' % (tk, label), fill=(220, 220, 220))
sheet.save(os.path.join(OUT, 'preview_intro.png'))
print('preview:', sheet.size, '->', os.path.join(OUT, 'preview_intro.png'))
