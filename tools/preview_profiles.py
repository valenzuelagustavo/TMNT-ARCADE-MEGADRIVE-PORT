# -*- coding: utf-8 -*-
"""
preview_profiles.py - Render offline de la escena de perfiles (SCENE_PROFILES)
con las MISMAS constantes y la MISMA fuente que src/scene_profiles.c.
Genera _tmp_intro/preview_profiles.png. No toca la ROM.
"""
import os
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROF = os.path.join(ROOT, 'res', 'images', 'profiles')
FONT = os.path.join(ROOT, 'res', 'images', 'font', 'font_tmnt_arcade.png')
OUT  = os.path.join(ROOT, '_tmp_intro')
os.makedirs(OUT, exist_ok=True)

W, H = 320, 224
PROFILE_REST_X, PROFILE_Y, PROFILE_START_X = 40, 16, 320
PROFILE_SLIDE_TICKS = 44
DATA_COL, DATA_ROW, DATA_STEP = 14, 4, 2
INFO_COL, INFO_ROW, INFO_STEP = 2, 20, 2

# La fuente es una tira de 95 tiles de 8x8 en orden ASCII 32..126
font = Image.open(FONT).convert('RGBA')
fp = font.load()
for y in range(font.height):                 # indice 0 = transparente
    for x in range(font.width):
        if fp[x, y][:3] == (0, 0, 0): fp[x, y] = (0, 0, 0, 0)

def draw_text(dst, text, col, row):
    for i, ch in enumerate(text):
        c = ord(ch)
        if c == 32 or c < 32 or c > 126: continue
        g = font.crop(((c - 32) * 8, 0, (c - 32) * 8 + 8, 8))
        dst.paste(g, ((col + i) * 8, row * 8), g)

PROFILES = [
    ('leo',  ["LEONARDO", "16", "5'01''", "155 LBS", "KATANA BLADE"],
             [" LEADER OF THE BOYS.",
              " IF GETS SERIOUS, HIS SWORDS START",
              "SLICING EVERYTHING IN SIGHT .....",
              "INCLUDING SALAMI PIZZA!"]),
    ('mike', ["MICHAELANGELO", "15", "5'00''", "150 LBS", "NUNCHAKUS"],
             [" PARTY DUDE AND PIZZA CONNOISSEUR",
              "EXTRAORDINAIRE.",
              " TAKES OCCASIONAL BREAK FROM",
              "PARTYING TO SMASH HEADS...AND FOOTS!"]),
    ('don',  ["DONATELLO", "15", "4'09''", "145 LBS", "BO STAFF"],
             [" HIPPEST MACHINE FREAK THIS SIDE",
              "OF SHELLVILLE.",
              " AVOIDS SUSHI LIKE A BAD CASE",
              "OF RUST!"]),
    ('raph', ["RAPHAEL", "15", "5'01''", "147 LBS", "PAIR OF SAI"],
             [" WILD BOY OF THE BUNCH.",
              " RAW ENERGY CAN FINISH OFF A FOOT..",
              "OR A PIZZA BEFORE YOU CAN SAY",
              "'TURTLES'."]),
]

def slide_x(t):
    p = t * 256 // PROFILE_SLIDE_TICKS
    e = 65536 - (256 - p) * (256 - p)
    return PROFILE_START_X - (PROFILE_START_X - PROFILE_REST_X) * e // 65536

def frame(name, data, info, tick=None, ndata=5, ninfo=4):
    f = Image.new('RGBA', (W, H), (0, 0, 0, 255))
    p = Image.open(os.path.join(PROF, '%s_profile.png' % name)).convert('RGBA')
    x = PROFILE_REST_X if tick is None else slide_x(tick)
    f.paste(p, (x, PROFILE_Y), p)
    if tick is None:
        for i in range(ndata): draw_text(f, data[i], DATA_COL, DATA_ROW + i * DATA_STEP)
        for i in range(ninfo): draw_text(f, info[i], INFO_COL, INFO_ROW + i * INFO_STEP)
    return f

cells = []
for t, lbl in [(6, 'entra t=6'), (18, 'entra t=18'), (32, 'entra t=32')]:
    cells.append((frame('leo', PROFILES[0][1], PROFILES[0][2], tick=t), 'LEO ' + lbl))
cells.append((frame('leo', PROFILES[0][1], PROFILES[0][2], ndata=3, ninfo=0), 'LEO datos apareciendo'))
for n, d, i in PROFILES:
    cells.append((frame(n, d, i), n.upper() + ' completo'))

cols = 2
rows = (len(cells) + cols - 1) // cols
sheet = Image.new('RGB', (cols * (W + 8) + 8, rows * (H + 22) + 8), (24, 24, 28))
dr = ImageDraw.Draw(sheet)
for n, (img, label) in enumerate(cells):
    x = 8 + (n % cols) * (W + 8)
    y = 8 + (n // cols) * (H + 22)
    sheet.paste(img.convert('RGB'), (x, y))
    dr.text((x + 2, y + H + 4), label, fill=(220, 220, 220))
sheet.save(os.path.join(OUT, 'preview_profiles.png'))
print('preview:', sheet.size)
