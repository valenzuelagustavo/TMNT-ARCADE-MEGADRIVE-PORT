#!/usr/bin/env python3
# =============================================================================
# gen_nivel1_2_artista.py  -  Sala de April (nivel 1-2) con el arte nuevo
# =============================================================================
# (22/09) Un artista paso un COLLAGE (749x297) con la version nueva de la sala:
#     res/images/lvl_1_scene/nivel1_2_collage_artista.png
# Adentro vienen, todos con UNA sola paleta de 15 colores compartida:
#   - el fondo de la sala          448x224 en (206,4)
#   - 3 frames del humo del techo  128x64  en (73,4) (73,79) (73,158)
#   - 8 frames del taladro         79x56   abajo, en fila
#   - la capsula cerrada y abierta 83x143  a la derecha
# Los frames del taladro y de la capsula vienen RECORTADOS CON EL FONDO DETRAS
# (son "capturas", no sprites). Este script los separa del fondo y los deja
# listos para rescomp. Salidas:
#
#   res/images/lvl_1_scene/bg_nivel1_2.png   440x224  (reemplaza a bg_test.png)
#   res/sprites/smoke_lvl1_2.png             tiles del humo, deduplicados por frame
#   src/smoke_lvl1_2.h                       el mapa 16x8 de cada frame del humo
#   res/sprites/taladro_capsula_v2.png       864x240 = 9x2 celdas de 96x120
#
# TODAS comparten la misma paleta de 16: el 0 queda libre (en Megadrive es
# transparente) y los 15 colores del collage van en 1..15. O sea que fondo,
# humo y capsula van TODOS en PAL0.
#
# ---------------------------------------------------------------------------
# DECISIONES
# ---------------------------------------------------------------------------
# FONDO. El fondo nuevo es el MISMO dibujo que bg_test.png con 3px de mas a la
# izquierda, 5 a la derecha y 32 arriba (verificado: el viejo encaja en (3,32)
# con 88% de pixeles iguales; el resto es la paleta nueva). En X se recorta a la
# ventana del viejo (440) para que todas las coordenadas del nivel (paredes,
# sofa, April, capsula, Rocksteady) sigan valiendo. En Y va COMPLETO (224 =
# toda la pantalla): las 32 filas de arriba son la pared y
# el tope de la biblioteca, y ocupan la franja que antes quedaba negra.
# Como la pantalla empieza justo en la fila 0 del dibujo nuevo, la Y de
# pantalla sigue siendo la Y del collage (lo usa la capsula, abajo).
#
# HUMO. Cada frame es de 128x64 (16x8 tiles) y hace mosaico perfecto cada
# 128px. Cargado entero serian 128 tiles de VRAM por frame (el viejo usaba 64)
# y la VRAM del nivel ya esta al limite (quedan 5 tiles libres). Pero la mitad
# de cada frame es purpura liso o transparente: DEDUPLICADO cada frame usa
# ~42 tiles. Asi que se streamea igual que antes -- un frame a la vez -- pero
# con sus tiles unicos y un mapa aparte que la escena vuelve a escribir en cada
# paso. Termina usando MENOS VRAM que el humo viejo.
#
# CAPSULA. Los frames se ubican solos dentro del fondo (template matching: los
# del taladro caen en (297,102) y los altos en (294,16), coordenadas del
# collage). Se vuelve transparente todo pixel IGUAL al fondo que este conectado
# con el borde de la celda: los que coinciden de casualidad adentro del cuerpo
# de la capsula quedan opacos, asi no aparecen agujeros cuando tiembla.
# La celda es de 96x104 como la vieja, anclada en el MUNDO en (288,56): la
# capsula alta arranca en y=16, pero de ahi hasta ~77 la tapa el humo (que en
# esas columnas va con prioridad alta), asi que lo que queda arriba de y=56 no
# se ve nunca y no vale los tiles.
# =============================================================================
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COLLAGE = os.path.join(ROOT, "res", "images", "lvl_1_scene", "nivel1_2_collage_artista.png")
OUT_BG = os.path.join(ROOT, "res", "images", "lvl_1_scene", "bg_nivel1_2.png")
OUT_SMOKE = os.path.join(ROOT, "res", "sprites", "smoke_lvl1_2.png")
OUT_SMOKE_H = os.path.join(ROOT, "src", "smoke_lvl1_2.h")
OUT_CAPS = os.path.join(ROOT, "res", "sprites", "taladro_capsula_v2.png")

GREY = 12                      # fondo del collage (y tambien un color real del fondo)
BG_BOX = (206, 4, 654, 228)    # fondo nuevo completo, 448x224
BG_CROP_X, BG_CROP_Y = 3, 0    # x: donde cae el bg viejo adentro del nuevo
BG_W, BG_H = 440, 224          # (22/09) ALTO COMPLETO: ocupa toda la pantalla

SMOKE_Y = (4, 79, 158)         # los 3 frames del humo, x = 73..201
PURPLE = 14                    # el violeta del humo en el collage
SMOKE_TOP_TILE = 4             # fila de pantalla donde arranca la celda del humo
                               # (debajo del HUD, filas 0-3) -- SMOKE_Y_TILE viejo
SMOKE_X = 73
SMOKE_W, SMOKE_H = 128, 64

# Frames de la capsula: (x, y, w, h) en el collage
DRILL = [(1 + 82 * i, 238, 79, 56) for i in range(8)]
DRILL[6] = (493, 238, 79, 55)
DRILL[7] = (575, 238, 79, 55)
CLOSED = (660, 4, 83, 143)
OPEN = (660, 150, 83, 143)

# Celda de la capsula en coordenadas de MUNDO de la escena (x = x del bg viejo)
# (22/09, 2da vuelta) Con el humo mas arriba, lo opaco termina en y~45: la
# celda arranca en 40 para que el tope recortado de la capsula siga cayendo
# adentro del humo opaco. 120 de alto = 15 tiles (antes 104 = 13).
CELL_X, CELL_Y = 288, 40
CELL_W, CELL_H = 96, 120


def make_palette(src):
    pal = src.getpalette()[:15 * 3]
    out = [0, 0, 0] + pal
    return out + [0] * (768 - len(out))


def save_p(arr, pal, path):
    im = Image.fromarray(arr.astype(np.uint8), "P")
    im.putpalette(pal)
    im.save(path)


def locate(bg, frame):
    """Mejor (dx, dy) de 'frame' dentro de 'bg' por igualdad de indices."""
    h, w = frame.shape
    best = None
    for dy in range(bg.shape[0] - h + 1):
        for dx in range(bg.shape[1] - w + 1):
            eq = np.count_nonzero(bg[dy:dy + h, dx:dx + w] == frame)
            if best is None or eq > best[0]:
                best = (eq, dx, dy)
    return best[1], best[2]


def border_connected(mask):
    """True en los pixeles de 'mask' conectados (4-vecinos) con el borde."""
    h, w = mask.shape
    seen = np.zeros_like(mask, dtype=bool)
    stack = [(y, x) for y in range(h) for x in (0, w - 1) if mask[y, x]]
    stack += [(y, x) for x in range(w) for y in (0, h - 1) if mask[y, x]]
    while stack:
        y, x = stack.pop()
        if seen[y, x]:
            continue
        seen[y, x] = True
        for ny, nx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
            if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not seen[ny, nx]:
                stack.append((ny, nx))
    return seen


def main():
    src = Image.open(COLLAGE)
    if src.mode != "P":
        sys.exit("El collage tiene que ser indexado")
    A = np.asarray(src).astype(np.int16)
    if A.max() > 14:
        sys.exit("El collage usa mas de 15 colores: con el 0 libre no entra en 16")
    pal = make_palette(src)
    BG = A[BG_BOX[1]:BG_BOX[3], BG_BOX[0]:BG_BOX[2]]

    # --- 0. Cuantas filas de tiles del humo son violeta LISO en los 3 frames --
    # (22/09) Esas filas no se animan: pasan al FONDO, junto
    # con la franja de detras del HUD, y en el plano de adelante queda solo la
    # parte que se mueve. Medido: son las 4 primeras (la 5ta ya tiene chispas).
    solid = 0
    for r in range(SMOKE_H // 8):
        if all((A[y0 + r * 8:y0 + r * 8 + 8, SMOKE_X:SMOKE_X + SMOKE_W] == PURPLE).all()
               for y0 in SMOKE_Y):
            solid += 1
        else:
            break
    # (22/09, 2da vuelta) Esas filas lisas se CORTAN: no van ni al fondo ni al
    # plano de adelante. La parte animada sube a SMOKE_TOP_TILE (justo debajo
    # del HUD) y el fondo solo lleva violeta detras del HUD. Asi el humo es
    # menos invasivo -- con las filas lisas bajaba hasta y=95.
    purple_px = SMOKE_TOP_TILE * 8

    # --- 1. Fondo -----------------------------------------------------------
    bg = BG[BG_CROP_Y:BG_CROP_Y + BG_H, BG_CROP_X:BG_CROP_X + BG_W] + 1
    # Franja de humo liso pintada EN EL FONDO: solo detras del HUD.
    bg[:purple_px, :] = PURPLE + 1
    save_p(bg, pal, OUT_BG)
    print("bg_nivel1_2.png        %dx%d  (violeta liso en y=0..%d: %d filas de tiles)"
          % (BG_W, BG_H, purple_px - 1, purple_px // 8))

    # --- 2. Humo: tiles unicos por frame + mapa ------------------------------
    per_frame_tiles, maps = [], []
    for y0 in SMOKE_Y:
        f = A[y0:y0 + SMOKE_H, SMOKE_X:SMOKE_X + SMOKE_W] + 1
        f[f == GREY + 1] = 0                     # gris del collage -> transparente
        tiles, index, m = [], {}, []
        for r in range(solid, SMOKE_H // 8):     # solo la parte que se mueve
            row = []
            for c in range(SMOKE_W // 8):
                t = f[r * 8:r * 8 + 8, c * 8:c * 8 + 8]
                if not t.any():
                    row.append(0xFF)             # vacio: tile 0 del VDP
                    continue
                k = t.tobytes()
                if k not in index:
                    index[k] = len(tiles)
                    tiles.append(t)
                row.append(index[k])
            m.append(row)
        per_frame_tiles.append(tiles)
        maps.append(m)
    n = max(len(t) for t in per_frame_tiles)
    strip = np.zeros((8 * len(SMOKE_Y), 8 * n), dtype=np.int16)
    for fi, tiles in enumerate(per_frame_tiles):
        for ti, t in enumerate(tiles):
            strip[fi * 8:fi * 8 + 8, ti * 8:ti * 8 + 8] = t
    save_p(strip, pal, OUT_SMOKE)
    print("smoke_lvl1_2.png       %d frames x %d tiles (por frame: %s)"
          % (len(SMOKE_Y), n, [len(t) for t in per_frame_tiles]))

    with open(OUT_SMOKE_H, "w") as fh:
        fh.write("// GENERADO POR tools/gen_nivel1_2_artista.py -- NO EDITAR A MANO\n")
        fh.write("// Humo del techo de la sala de April (nivel 1-2), arte del 22/09.\n")
        fh.write("// Para cada frame, el mapa de la parte ANIMADA de su celda: el numero\n")
        fh.write("// es el tile DENTRO del frame (0..SMOKE2_TILES-1); 0xFF = vacio.\n")
        fh.write("// Las SMOKE2_SOLID_ROWS filas de arriba de la celda son violeta liso en\n")
        fh.write("// los 3 frames: no estan aca, las lleva pintadas el FONDO.\n")
        fh.write("#ifndef _SMOKE_LVL1_2_H_\n#define _SMOKE_LVL1_2_H_\n\n")
        fh.write("#define SMOKE2_FRAMES   %d\n" % len(SMOKE_Y))
        fh.write("#define SMOKE2_TILES    %d   // tiles por frame (el mayor)\n" % n)
        fh.write("#define SMOKE2_CELL_W   %d\n" % (SMOKE_W // 8))
        fh.write("#define SMOKE2_SOLID_ROWS %d   // filas lisas que van en el fondo\n" % solid)
        fh.write("#define SMOKE2_CELL_H   %d   // filas animadas (las que quedan en BG_A)\n\n"
                 % (SMOKE_H // 8 - solid))
        fh.write("static const u8 smoke2Map[SMOKE2_FRAMES][SMOKE2_CELL_H][SMOKE2_CELL_W] = {\n")
        for m in maps:
            fh.write("    {\n")
            for row in m:
                fh.write("        { " + ", ".join("0x%02X" % v for v in row) + " },\n")
            fh.write("    },\n")
        fh.write("};\n\n#endif\n")

    # --- 3. Capsula ------------------------------------------------------------
    def cutout(box):
        x, y, w, h = box
        f = A[y:y + h, x:x + w]
        dx, dy = locate(BG, f)
        same = (BG[dy:dy + h, dx:dx + w] == f)
        clear = border_connected(same)
        spr = f + 1
        spr[clear] = 0
        # a coordenadas de mundo de la escena y de ahi a la celda
        wx = BG_BOX[0] * 0 + dx - BG_CROP_X     # x de mundo (la del bg viejo)
        wy = dy                                  # y de pantalla (bg abajo, offset 4)
        cell = np.zeros((CELL_H, CELL_W), dtype=np.int16)
        ox, oy = wx - CELL_X, wy - CELL_Y
        for yy in range(h):
            cy = oy + yy
            if cy < 0 or cy >= CELL_H:
                continue
            for xx in range(w):
                cx = ox + xx
                if 0 <= cx < CELL_W and spr[yy, xx]:
                    cell[cy, cx] = spr[yy, xx]
        return cell, (wx, wy)

    row0 = [cutout(b) for b in DRILL] + [cutout(CLOSED)]
    row1 = [cutout(CLOSED), cutout(OPEN)]
    cols = len(row0)
    sheet = np.zeros((CELL_H * 2, CELL_W * cols), dtype=np.int16)
    for i, (c, _) in enumerate(row0):
        sheet[0:CELL_H, i * CELL_W:(i + 1) * CELL_W] = c
    for i, (c, _) in enumerate(row1):
        sheet[CELL_H:, i * CELL_W:(i + 1) * CELL_W] = c
    save_p(sheet, pal, OUT_CAPS)
    print("taladro_capsula_v2.png %dx%d  (fila 0: %d frames, fila 1: %d)"
          % (sheet.shape[1], sheet.shape[0], len(row0), len(row1)))
    print("  posiciones de mundo: taladro %s, capsula %s"
          % (row0[0][1], row0[-1][1]))


if __name__ == "__main__":
    main()
