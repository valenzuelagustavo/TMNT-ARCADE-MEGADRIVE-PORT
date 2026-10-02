#!/usr/bin/env python3
# ============================================================================
# gen_player_hitbox.py -- Hitbox de los ataques medido sobre el ARTE
# ============================================================================
# PROBLEMA QUE RESUELVE
#   El alcance del golpe era UNA constante por personaje (56 Leo, 40 Mike, 60
#   Don, 38 Raph), fija durante todo el swing y ciega a la altura:
#
#   1. El arma solo esta extendida en uno o dos frames de cada animacion. En el
#      frame 0 de ATTACK_1 de Leo la katana esta recogida y el arte llega a 6px
#      del centro, pero el hitbox ya valia 56: el golpe conectaba ~50px antes
#      de que los sprites se tocaran.
#   2. La ALTURA no se miraba nunca. La patada en salto conectaba igual con la
#      tortuga por encima de la cabeza del foot soldier, y encima el pixel mas
#      adelantado de ese frame no es el pie (30px) sino la katana que cuelga
#      hacia abajo (47px).
#
# QUE HACE
#   Mide, para cada personaje / animacion de ataque / frame / FRANJA HORIZONTAL
#   de 8px, hasta donde llega el pixel opaco mas a la derecha, en offsets desde
#   el centro de la celda de 104x104. El motor cruza esas franjas con la banda
#   de altura que ocupa el cuerpo del enemigo (descontando el jumpZ del salto),
#   asi que el golpe conecta solo si hay arte del jugador A LA ALTURA del
#   objetivo y lo suficientemente lejos.
#
#   El arte de las 4 sheets mira siempre a la derecha y el motor espeja el
#   sprite al mirar a la izquierda, asi que el mismo numero sirve de los dos
#   lados.
#
# SALIDA
#   src/player_hitbox.c / .h   (GENERADOS, no editar a mano)
#
# Correr desde la raiz del proyecto:  python3 tools/gen_player_hitbox.py
# ============================================================================

import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SPR  = os.path.join(ROOT, "res", "sprites")
SRC  = os.path.join(ROOT, "src")

CELL  = 104         # celda de las 4 sheets de tortuga (13x13 tiles)
BAND  = 8           # alto de cada franja (un tile)
BANDS = CELL // BAND
NONE  = -128        # centinela: esa franja no tiene un solo pixel opaco

CHARS = [("leo",  "leo_anim_13x13.png"),
         ("mike", "mike_anim_13x13.png"),
         ("don",  "don_anim_13x13.png"),
         ("raph", "raph_anim_13x13.png")]

# Animaciones que pueden golpear, en el orden en que van a la tabla.
# (indice en PlayerAnim, nombre)
ATK_ANIMS = [(2, "KICK"), (3, "ATTACK_1"), (4, "ATTACK_2"), (5, "ATTACK_3"),
             (7, "JUMP_KICK"), (10, "SPECIAL")]
NUM_ANIMS = 21      # filas de PlayerAnim
KICK_ANIMS = (2, 7)         # KICK y JUMP_KICK: pega el pie, no el arma
KICK_IGNORE = [4, 10, 13]   # sombra y grises del arma (paleta unica)

# Enemigos, solo para imprimir de referencia el alto real de su cuerpo.
ENEMIES = [("foot_soldier_16colors.png", 64, 80, 80),
           ("foot_soldier_orange.png", 104, 104, 96),
           ("rocksteady_boss.png", 104, 104, 96)]


def alpha_mask(im):
    """Mascara de pixeles OPACOS. En las sheets indexadas el indice 0 es el
    color transparente (asi las exporta Aseprite y asi las trata rescomp)."""
    if im.mode == "P":
        t = im.info.get("transparency", 0)
        if not isinstance(t, int):
            t = 0
        return np.array(im) != t
    return np.array(im.convert("RGBA"))[:, :, 3] > 0


def measure(path):
    """-> (frames_por_anim, {anim: [ [reach por franja] por frame ]})"""
    im = Image.open(path)
    a_all = alpha_mask(im)
    # (02/10) En las PATADAS pega el PIE: no cuentan el arma (grises 10/13 de
    # la paleta unica de las tortugas) ni la sombra (4). Antes la katana que
    # cuelga en la patada en salto agrandaba el alcance (pedido de Gustavo,
    # medido en Pruebas_TMNT_Control).
    idx = np.array(im) if im.mode == "P" else None
    a_kick = a_all & ~np.isin(idx, KICK_IGNORE) if idx is not None else a_all
    h, w = a_all.shape
    rows, cols = h // CELL, w // CELL
    half = CELL // 2
    data = {}
    for anim, _ in ATK_ANIMS:
        if anim >= rows:
            data[anim] = []
            continue
        frames = []
        a = a_kick if anim in KICK_ANIMS else a_all
        for c in range(cols):
            cell = a[anim * CELL:(anim + 1) * CELL, c * CELL:(c + 1) * CELL]
            bands = []
            for k in range(BANDS):
                strip = cell[k * BAND:(k + 1) * BAND]
                xs = np.nonzero(strip.any(axis=0))[0]
                # +1 -> borde (primer pixel vacio), no el ultimo lleno
                bands.append(int(xs[-1]) - half + 1 if len(xs) else NONE)
            frames.append(bands)
        data[anim] = frames
    return cols, data


def main():
    sheets, max_cols = {}, 0
    for name, fn in CHARS:
        cols, data = measure(os.path.join(SPR, fn))
        sheets[name] = data
        max_cols = max(max_cols, cols)
        print("%-5s %2d frames por animacion" % (name, cols))

    with open(os.path.join(SRC, "player_hitbox.h"), "w") as f:
        f.write(HDR % dict(frames=max_cols, bands=BANDS, band=BAND,
                           slots=len(ATK_ANIMS), anims=NUM_ANIMS, cell=CELL))

    with open(os.path.join(SRC, "player_hitbox.c"), "w") as f:
        f.write('#include "player_hitbox.h"\n\n')

        # anim -> slot
        slot_of = [-1] * NUM_ANIMS
        for i, (anim, _) in enumerate(ATK_ANIMS):
            slot_of[anim] = i
        f.write("// PlayerAnim -> fila de playerAtkReach (-1 = no golpea)\n")
        f.write("const s8 phbSlotOfAnim[PHB_ANIMS] = {\n    ")
        f.write(", ".join("%2d" % v for v in slot_of))
        f.write("\n};\n\n")

        f.write("const s8 playerAtkReach[PHB_CHARS][PHB_SLOTS][PHB_FRAMES][PHB_BANDS] = {\n")
        for name, _ in CHARS:
            f.write("  {   // ===== %s =====\n" % name)
            for anim, label in ATK_ANIMS:
                f.write("    {   // %s\n" % label)
                frames = sheets[name].get(anim, [])
                for c in range(max_cols):
                    bands = frames[c] if c < len(frames) else [NONE] * BANDS
                    f.write("      { %s },  // frame %d\n"
                            % (", ".join("%4d" % v for v in bands), c))
                f.write("    },\n")
            f.write("  },\n")
        f.write("};\n\n")

        # maximo por frame: para objetivos PUNTUALES (shuriken, bala), donde no
        # tiene sentido cruzar bandas de altura.
        f.write("// Maximo de todas las franjas: se usa contra objetivos puntuales\n"
                "// (shurikens, balas), donde no hay banda de altura que cruzar.\n")
        f.write("const s8 playerAtkReachMax[PHB_CHARS][PHB_SLOTS][PHB_FRAMES] = {\n")
        for name, _ in CHARS:
            f.write("  {   // %s\n" % name)
            for anim, label in ATK_ANIMS:
                frames = sheets[name].get(anim, [])
                vals = []
                for c in range(max_cols):
                    bands = frames[c] if c < len(frames) else [NONE] * BANDS
                    m = max(bands)
                    vals.append(m if m != NONE else NONE)
                f.write("    { %s },  // %s\n"
                        % (", ".join("%4d" % v for v in vals), label))
            f.write("  },\n")
        f.write("};\n")

    print("\nAlto REAL del cuerpo de los enemigos, medido desde los pies hacia\n"
          "arriba (para calibrar los ENEMY_BODY_H_* / ROCKSTEADY_BODY_H):")
    for fn, cw, ch, foot in ENEMIES:
        p = os.path.join(SPR, fn)
        if not os.path.exists(p):
            continue
        a = alpha_mask(Image.open(p))
        h, w = a.shape
        alturas = []
        for r in range(h // ch):
            for c in range(w // cw):
                sub = a[r * ch:(r + 1) * ch, c * cw:(c + 1) * cw]
                ys = np.nonzero(sub.any(axis=1))[0]
                if len(ys):
                    alturas.append(foot - int(ys[0]))
        if alturas:
            print("  %-26s mediana %3d   max %3d"
                  % (fn, int(np.median(alturas)), max(alturas)))


HDR = """// GENERADO POR tools/gen_player_hitbox.py -- NO EDITAR A MANO
#ifndef _PLAYER_HITBOX_H_
#define _PLAYER_HITBOX_H_

#include <genesis.h>

// ---------------------------------------------------------------------------
// Hitbox de los ataques, medido sobre los pixeles OPACOS del arte
// ---------------------------------------------------------------------------
// Para cada personaje, animacion de ataque, frame y FRANJA HORIZONTAL de
// PHB_BAND px: hasta donde llega el pixel opaco mas a la derecha, en offset
// desde el CENTRO de la celda de %(cell)d x %(cell)d. PHB_NONE = esa franja esta vacia.
//
// El arte de las 4 sheets mira a la derecha y el motor espeja el sprite al
// mirar a la izquierda, asi que el mismo numero vale para los dos lados.
//
// La franja 0 es la de ARRIBA del frame. Para saber a que altura de pantalla
// cae hay que partir del tope del frame dibujado, que es
// (p->y - PLAYER_FOOT_OFFSET - p->jumpZ): por eso saltar sube las franjas y
// una patada en el aire deja de tocar al enemigo que quedo abajo.
//
// Personaje: mismo orden que initPlayer() -- 0=Leo 1=Mike 2=Don 3=Raph.
// ---------------------------------------------------------------------------
#define PHB_CHARS   4
#define PHB_SLOTS   %(slots)d      // animaciones que pueden golpear
#define PHB_FRAMES  %(frames)d
#define PHB_BANDS   %(bands)d
#define PHB_BAND    %(band)d      // px de alto de cada franja
#define PHB_ANIMS   %(anims)d     // filas de PlayerAnim
#define PHB_NONE    (-128)  // franja sin un solo pixel opaco

extern const s8 phbSlotOfAnim[PHB_ANIMS];
extern const s8 playerAtkReach[PHB_CHARS][PHB_SLOTS][PHB_FRAMES][PHB_BANDS];
extern const s8 playerAtkReachMax[PHB_CHARS][PHB_SLOTS][PHB_FRAMES];

#endif
"""

if __name__ == "__main__":
    main()
