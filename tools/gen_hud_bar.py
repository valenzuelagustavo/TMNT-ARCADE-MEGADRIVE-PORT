#!/usr/bin/env python3
# ============================================================================
# gen_hud_bar.py -- arte del HUD de 1-2 jugadores (barra de vida + vidas)
# ============================================================================
# Genera dos PNG indexados sobre PAL1 (la paleta de las tortugas, la misma que
# usa el marco del HUD), a partir de una captura del arcade:
#
#   res/sprites/hp_bar.png          32x176 = 11 frames de 32x16
#       La barra de vida, del doble de alto que la anterior (era 32x8). 10
#       segmentos de 2px con separador negro de 1px, igual que el arcade.
#       frame[0] = llena ... frame[10] = vacia (01/10: el fondo negro se
#       achica con los segmentos; lo vacio es transparente). Sin comprimir ni
#       deduplicar
#       (TILESET ... NONE NONE) para poder indexar frame N = tiles N*8.
#
#   res/images/hud/lives_digits.png 8x160 = 10 digitos de 8x16
#       Las vidas, que en el arcade son UN DIGITO GRANDE en verde pegado a la
#       barra (y no un "x3" como teniamos). Se arman estirando el glifo de la
#       fuente del HUD (8x8) a 16px de alto -- duplicando filas del medio, que
#       conserva el estilo -- y recoloreando.
#
# Los dos se dibujan como TILES de BG_A con la misma tecnica de streaming que
# el fuego: un bloque de VRAM por jugador y un DMA del frame/digito nuevo
# cuando cambia.
# ============================================================================

import os
from PIL import Image
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PALSRC = os.path.join(ROOT, "res", "images", "hud", "hud_1p.png")
FONT   = os.path.join(ROOT, "res", "images", "font", "font_tmnt_arcade_2.png")
BAROUT = os.path.join(ROOT, "res", "sprites", "hp_bar.png")
DIGOUT = os.path.join(ROOT, "res", "images", "hud", "lives_digits.png")

BAR_W, BAR_H, BAR_FRAMES = 32, 16, 11
SEGMENTS = 10

# Degrade de la barra, de izquierda (ultimo pedazo de vida) a derecha (vida
# llena). El arcade va de azul oscuro a blanco; PAL1 no tiene azul oscuro, asi
# que lo mas parecido es lavanda -> cyan -> violeta claro -> blanco, que ademas
# sube en luminancia de forma pareja (137, 150, 166, 214).
#   9 = (0,189,255)    11 = (140,132,206)   12 = (173,148,214)   13 = (214,214,214)
RAMP_ARCADE  = [11, 11, 11, 9, 9, 9, 12, 12, 13, 13]
# El de antes, por si se quiere volver: magenta -> cyan -> gris claro.
RAMP_CLASICO = [15, 15, 9, 9, 9, 9, 13, 13, 13, 13]

RAMP = RAMP_ARCADE

# Vidas: verde del arcade. PAL1 -> 5 = (107,165,0) y 2 = (0,107,0) de sombra.
DIG_MAIN, DIG_SHADOW = 5, 2
DIG_H = 16


def load_pal():
    p = Image.open(PALSRC).getpalette()[:48]
    return p + [0] * (768 - len(p))


def make_bar():
    img = np.zeros((BAR_H * BAR_FRAMES, BAR_W), np.uint8)
    for f in range(BAR_FRAMES):
        top = f * BAR_H
        filled = SEGMENTS - f
        # (01/10) El fondo negro ACOMPANA a los segmentos: cubre solo hasta
        # el separador que sigue al ultimo segmento lleno, el resto queda
        # transparente (indice 0). Antes era un rectangulo negro fijo de
        # 32x16 y con poca vida quedaba un cuadrado negro vacio.
        if filled > 0:
            img[top:top + BAR_H, :min(BAR_W, filled * 3 + 2)] = 1   # negro (borde y fondo)
        for s in range(SEGMENTS):
            x = 1 + s * 3
            if s < filled:
                img[top + 1:top + BAR_H - 1, x:x + 2] = RAMP[s]
    return img


def make_digits():
    font = np.array(Image.open(FONT))
    out = np.zeros((DIG_H * 10, 8), np.uint8)
    for d in range(10):
        g = font[:, (ord('0') - 32 + d) * 8:(ord('0') - 32 + d + 1) * 8]
        # estirar a DIG_H: 2x exacto (cada fila va dos veces). Repartir las
        # filas extra "donde caiga" deforma el glifo -- probado, el 3 quedaba
        # con la barra del medio gordisima.
        reps = [DIG_H // 8] * 8
        y = d * DIG_H
        for r in range(8):
            for _ in range(reps[r]):
                row = g[r].copy()
                row[g[r] == 13] = DIG_MAIN      # cuerpo claro -> verde
                row[g[r] == 11] = DIG_SHADOW    # sombra lavanda -> verde oscuro
                out[y] = row
                y += 1
    return out


def save(arr, path, pal):
    im = Image.fromarray(arr, mode="P")
    im.putpalette(pal)
    im.save(path)
    print("%s  %dx%d" % (path.split('/')[-1], arr.shape[1], arr.shape[0]))


def main():
    pal = load_pal()
    save(make_bar(), BAROUT, pal)
    save(make_digits(), DIGOUT, pal)


if __name__ == "__main__":
    main()
