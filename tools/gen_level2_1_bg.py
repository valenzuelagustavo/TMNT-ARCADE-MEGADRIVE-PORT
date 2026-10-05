#!/usr/bin/env python3
# ============================================================================
# gen_level2_1_bg.py -- Fondo del nivel "SCENE 2" (la calle) para SGDK
# ============================================================================
# ENTRADA
#   res/images/lvl_2_scene/Stage 2-_16_colors.png   (2560x640, 16 colores,
#   indice 0 = transparente).  Es el nivel COMPLETO del arcade, con las zonas
#   que la camara nunca muestra dejadas en transparente. El recorrido es la
#   clasica "L" de los beat'em up: se avanza a la derecha por la vereda de
#   arriba, se baja por la calle diagonal de la esquina y se vuelve a avanzar
#   a la derecha por la vereda de abajo.
#
# POR QUE HACE FALTA ESTE SCRIPT
#   El nivel entero son 2069 tiles unicos: NO entra en VRAM (descontando el
#   HUD y el motor de sprites quedan RING_TILES para el fondo). Igual que el
#   nivel 1 streamea columnas, aca hay que streamear TILES: el fondo se parte
#   en SECCIONES y solo viven en VRAM las que la camara esta usando.
#
#   (17/09) COMO SE DECIDE QUE SECCION VA CON QUE CELDA. Antes se simulaba UNA
#   trayectoria de camara y se repartian las celdas en el orden en que ESA
#   trayectoria las revelaba. Estaba mal: en la partida la camara toma otro
#   camino (va pegada al techo del corredor, no al piso) y las celdas que la
#   simulacion no habia visto quedaban sin seccion -> tile 0 -> banda negra.
#   Ahora se calcula la ENVOLVENTE de TODOS los recorridos posibles, con el
#   progreso minimo y maximo de cada celda, y la verificacion del ring se hace
#   contra esa envolvente. No hay trayectoria simulada.
#
# SALIDA
#   res/images/lvl_2_scene/genesis/lvl21_secNN.png  un PNG por seccion; cada
#       uno es una tira de tiles en grilla de 16 columnas, pensada para
#       "TILESET ... NONE NONE" (sin comprimir y sin deduplicar, para que el
#       orden de los tiles sea exactamente este).
#   res/images/lvl_2_scene/genesis/lvl21_pal.png    1x16 px con la paleta.
#   src/level2_1_map.c / .h    el tilemap del MUNDO (u16 por celda:
#       (seccion << 9) | indice-dentro-de-la-seccion), las tablas de
#       secciones y el corredor de camara.
#
# El recorte superior NO se hace pisando la imagen: la
# camara arranca en camY=32, o sea que muestra la banda y=32..255 de la
# imagen (224px, el alto de pantalla) y los 32px de arriba quedan fuera. El
# HUD se dibuja encima de la franja superior igual que en el nivel 1.
#
# Las zonas transparentes del PNG no se cargan a VRAM: sus celdas se dibujan
# con el tile 0 del VDP, que con PAL0[0] negro se ve como cielo nocturno.
# Solo asoman arriba a la derecha mientras la camara dobla la esquina.
# ============================================================================

import os, sys
from PIL import Image
import numpy as np

HERE   = os.path.dirname(os.path.abspath(__file__))
ROOT   = os.path.dirname(HERE)
SRCPNG = os.path.join(ROOT, "res", "images", "lvl_2_scene", "Stage 2-_16_colors_v2.png")
OUTDIR = os.path.join(ROOT, "res", "images", "lvl_2_scene", "genesis")
SRCDIR = os.path.join(ROOT, "src")

SCREEN_W, SCREEN_H = 320, 224
PLANE_W, PLANE_H   = 64, 32          # plano BG_B circular (512x256 px)
# Ventana que se mantiene dibujada en el plano: lo visible + 1 celda de margen.
# Dibujar las 64x32 celdas del plano gastaria VRAM en filas que nunca se ven
# (el arte arranca en y=0 pero la camara nunca sube de y=32).
VIS_COLS, VIS_ROWS = 41, 29

# --- Trayectoria REAL de la camara del arcade -------------------------------
# Sacada de 16 capturas del arcade original (13/09): cada
# frame se localizo dentro del PNG del nivel por minimos cuadrados sobre el
# fondo, y los 7 matches limpios cayeron sobre una RECTA:
#
#     camX = 0.803 * camY + 1289      (camX = (camY*103 >> 7) + 1289)
#
# O sea que en el arcade la camara baja la esquina en DIAGONAL, no en L: se va
# corriendo a la derecha 0,8px por cada px que baja. La version anterior hacia
# un codo (camX clavado en 1536 durante casi todo el descenso) y por eso
# asomaba el cuadro negro arriba a la derecha: ahi el arte solo llega a x=1735
# y el viewport pedia hasta 1855.
CAM_SLOPE_NUM, CAM_SLOPE_DEN, CAM_SLOPE_OFF = 103, 128, 1289
CAM_Y_MIN, CAM_Y_MAX = 32, 416
CAM_X_MIN, CAM_X_MAX = 0, 2240       # 2240 = 2560 - 320 (fin del nivel)

# --- Relleno del hueco de la calle ------------------------------------------
# Al rip le falta un pedazo de calle abajo a la izquierda del codo que el
# arcade SI dibuja: con el viewport a camY 144..216 quedaban hasta 4800 px
# (6,7% de la pantalla) sin arte, hiciera lo que hiciera la camara. Se rellena
# EN MEMORIA (el PNG original no se toca) copiando en diagonal: la calle baja
# a 45 grados, asi que el pixel (x, y) es equivalente al (x+D, y+D).
FILL_ZONE = (340, 470, 1300, 1620)   # y0, y1, x0, x1
FILL_DIAG = 192                      # desplazamiento diagonal de la copia

# Radio del "cerrado de huecos" del corredor (ver close_holes). 24px tapa
# contornos, juntas entre edificios y aleros; las zonas vacias de verdad del
# PNG miden cientos de px y siguen contando como vacias.
HOLE_R = 24

# Holgura del piso de cada peldano respecto del tope del peldano ANTERIOR (ver
# build_corridor). 0 = una vez que bajo un escalon, la camara no puede volver
# atras del tope del anterior. Subirlo ensancha el corredor y hace falta mas
# VRAM de fondo.
STEP_SLACK = 0

# Las secciones se cortan por AVANCE DE CAMARA, no por cantidad de tiles: el
# arte se repite muchisimo a lo largo de la calle (ladrillos, ventanas, vereda)
# y la deduplicacion es local a cada seccion, asi que secciones chicas
# desperdician VRAM duplicando los mismos tiles una y otra vez.
SEC_SPAN_PX   = 208     # px de avance de camara que cubre cada seccion
RING_TILES    = 768     # VRAM reservada para el fondo (en tiles)
PRELOAD_PX    = 16      # cuanto se adelanta la carga de la seccion siguiente
LOAD_TILES_FR = 128     # tiles que el runtime copia por frame (2KB de DMA)


def arcade_cam_x(cam_y):
    """camX de la recta que siguen los frames del arcade."""
    return (cam_y * CAM_SLOPE_NUM) // CAM_SLOPE_DEN + CAM_SLOPE_OFF


def fill_street_gap(pix):
    """Rellena en memoria el pedazo de calle que le falta al rip (ver FILL_*)."""
    y0, y1, x0, x1 = FILL_ZONE
    H, W = pix.shape
    sub = pix[y0:y1, x0:x1]
    for _ in range(3):                       # un par de pasadas: la fuente
        ys, xs = np.nonzero(sub == 0)        # tambien puede estar vacia
        if len(ys) == 0:
            break
        gy, gx = ys + y0 + FILL_DIAG, xs + x0 + FILL_DIAG
        ok = (gy < H) & (gx < W)
        vals = np.zeros(len(ys), np.uint8)
        vals[ok] = pix[gy[ok], gx[ok]]
        sub[ys, xs] = vals
    pix[y0:y1, x0:x1] = sub
    return pix


def _reach(mask, axis, rev, radius):
    """True donde hay un pixel de 'mask' a <= radius, mirando hacia atras sobre
    'axis' (o hacia adelante si rev)."""
    a = np.flip(mask, axis) if rev else mask
    idx = np.arange(a.shape[axis])
    if axis == 1:
        pos = np.maximum.accumulate(np.where(a, idx[None, :], -1 << 20), axis=1)
        out = (idx[None, :] - pos) <= radius
    else:
        pos = np.maximum.accumulate(np.where(a, idx[:, None], -1 << 20), axis=0)
        out = (idx[:, None] - pos) <= radius
    return np.flip(out, axis) if rev else out


def close_holes(pix):
    """Mascara de 'zona cubierta' para el corredor de camara.

    (17/09) El rip usa el INDICE 0 tambien como NEGRO: los contornos de los
    techos, las juntas de 2px entre edificios y la linea del alero del final
    estan dibujados con el color transparente. En pantalla se ven bien (el VDP
    pinta el indice 0 con el color de fondo, que es negro), pero el corredor
    los contaba como "hueco" y por eso el techo se clavaba en camX=2118: ahi
    arrancan las juntas de x=2438 y x=2477, y una diagonal de 1px del alero.
    Con el tope en 2118 la camara NUNCA llegaba a LVL21_CAM_X_MAX (2240) y la
    condicion de fin de nivel no disparaba nunca.

    Asi que para DECIDIR el corredor se usa esta mascara "cerrada": un pixel
    vacio cuenta como cubierto si tiene arte a menos de HOLE_R px a la izq Y a
    la der de su fila, o arriba Y abajo de su columna. Eso tapa contornos,
    juntas y aleros (unos pocos px de ancho) pero NO las zonas realmente
    vacias del PNG, que miden cientos de px. Los PIXELES NO SE TOCAN: los
    tiles siguen teniendo el indice 0 donde el arte lo tiene."""
    m = (pix != 0)
    l = _reach(m, 1, False, HOLE_R)
    r = _reach(m, 1, True,  HOLE_R)
    u = _reach(m, 0, False, HOLE_R)
    d = _reach(m, 0, True,  HOLE_R)
    covered = m | (l & r) | (u & d)
    print("cerrado de huecos: %d px de contorno/junta pasan a cubiertos"
          % int((covered & ~m).sum()))
    return covered


def build_corridor(covered):
    """Para cada camY (de a 8px) el rango de camX SIN un solo pixel vacio,
    quedandose con el tramo contiguo que contiene a la recta del arcade."""
    H, W = covered.shape
    m = covered.astype(np.int32)
    integ = np.zeros((H + 1, W + 1), np.int64)
    integ[1:, 1:] = m.cumsum(0).cumsum(1)

    def empty(x, y):
        op = (integ[y + SCREEN_H, x + SCREEN_W] - integ[y, x + SCREEN_W]
              - integ[y + SCREEN_H, x] + integ[y, x])
        return SCREEN_W * SCREEN_H - op

    lo_t, hi_t = [], []
    for cy in range(CAM_Y_MIN, CAM_Y_MAX + 1, 8):
        want = arcade_cam_x(cy)
        good = [cx for cx in range(0, CAM_X_MAX + 1) if empty(cx, cy) == 0]
        if not good:
            sys.exit("camY %d no tiene ninguna posicion sin huecos" % cy)
        gs = set(good)
        seed = want if want in gs else min(good, key=lambda v: abs(v - want))
        lo = hi = seed
        while lo - 1 in gs:
            lo -= 1
        while hi + 1 in gs:
            hi += 1
        lo_t.append(lo)
        hi_t.append(hi)
    # monotonas: la camara nunca retrocede
    for i in range(1, len(lo_t)):
        lo_t[i] = max(lo_t[i], lo_t[i - 1])
        hi_t[i] = max(hi_t[i], hi_t[i - 1])

    # --- Los PELDANOS (17/09) ----------------------------------------------
    # El techo es una escalera: se queda clavado en un valor durante todo un
    # tramo de camY y ahi pega el salto al peldano siguiente. Esos saltos son
    # los "topes" marcados con flechas.
    #
    # El piso que sale del arte es muy bajo (1254..1304 durante TODO el
    # descenso), asi que la camara podria bajar entera pegada a la izquierda.
    # Dos problemas con eso: (1) no se parece al arcade, donde la camara baja
    # la esquina en diagonal; (2) el runtime tendria que tener en VRAM la
    # union de todas las posiciones posibles -- medido: 1000 tiles de fondo
    # contra los 800 que hay -- y aparecerian celdas sin seccion (negras).
    #
    # Asi que el piso de cada peldano se sube al TECHO DEL PELDANO ANTERIOR:
    # una vez que la camara bajo al escalon k, no puede estar a la izquierda
    # del tope del escalon k-1. Como camY tiene el freno de no adelantarse a
    # camX, el efecto en la partida es exactamente el buscado: la
    # camara se clava en el tope del escalon, el jugador queda topado contra
    # el borde derecho, y recien cuando baja se libera el escalon siguiente.
    # El piso de cada peldano se sube al TECHO DEL PELDANO ANTERIOR. Dos
    # motivos:
    #
    # 1. ES LO QUE SE BUSCA. Como camY tiene el freno de no adelantarse a
    #    camX (ver el bucle en level2_1.c), para BAJAR al escalon k+1 la camara
    #    tiene que haber llegado antes al tope del escalon k. O sea: la camara
    #    se clava en el tope, el jugador queda topado contra el borde derecho
    #    de la pantalla, y la unica forma de seguir es bajar.
    #
    # 2. Y ADEMAS ENTRA EN VRAM. El piso que sale del arte es bajisimo (1254
    #    ..1304 durante TODO el descenso): con ese piso la camara podria bajar
    #    pegada a la izquierda y el streaming tendria que tener cargada la
    #    union de todas las posiciones posibles -- medido: 960 tiles de fondo
    #    contra los ~770 que hay. Las celdas que no llegaban a tener seccion se
    #    dibujaban con el tile 0: la banda negra pegada al borde derecho.
    #    Atando el piso al tope anterior, el corredor de cada peldano mide lo
    #    que mide el salto (72..128px) y la cuenta cierra en 768.
    #
    # OJO: el piso es el techo ANTERIOR, no el propio. Igualar piso y techo
    # trababa la camara: para bajar hacia camY=104 hacia falta camX>=1416, pero
    # el techo de camY=103 era 1344, asi que ni camY ni camX podian moverse.
    prev_ceil = lo_t[0]
    for i in range(1, len(hi_t)):
        lo_t[i] = max(lo_t[i], prev_ceil - STEP_SLACK)
        if hi_t[i] != hi_t[i - 1]:
            prev_ceil = hi_t[i - 1]
            lo_t[i] = max(lo_t[i], prev_ceil - STEP_SLACK)
    for i in range(1, len(lo_t)):
        lo_t[i] = max(lo_t[i], lo_t[i - 1])
    return lo_t, hi_t


def main():
    img = Image.open(SRCPNG)
    if img.mode != "P":
        sys.exit("El PNG tiene que ser indexado de 16 colores")
    pix = np.array(img).copy()
    # (16/09) El PNG v2 mide 2560x722: el arte sigue estando
    # en las filas 0..639 y las 82 de abajo son transparentes (sobra del
    # export). Como 722 no es multiplo de 8, se recorta a filas completas de
    # tiles y se descartan las filas vacias del final.
    rows_used = int(np.nonzero((pix != 0).any(axis=1))[0].max()) + 1
    pix = pix[: ((rows_used + 7) // 8) * 8]
    pix = fill_street_gap(pix)
    H, W = pix.shape
    COLS, ROWS = W // 8, H // 8

    pal = img.getpalette()[:48]

    # tiles del mundo: (ROWS, COLS, 64)
    tiles = (pix.reshape(ROWS, 8, COLS, 8)
                .transpose(0, 2, 1, 3)
                .reshape(ROWS, COLS, 64))
    used = (tiles != 0).any(axis=2)          # celda con arte

    lo_tab, hi_tab = build_corridor(close_holes(pix))
    print("corredor: camX %d..%d (camY=%d)  ->  %d..%d (camY=%d)"
          % (lo_tab[0], hi_tab[0], CAM_Y_MIN, lo_tab[-1], hi_tab[-1], CAM_Y_MAX))
    # Los ESCALONES: los saltos del techo son los "topes" de camara marcados
    # con flechas. Se imprimen para poder cotejarlos con la captura.
    prev = None
    for i, v in enumerate(hi_tab):
        if v != prev:
            print("  tope camX=%4d a partir de camY=%d" % (v, CAM_Y_MIN + i * 8))
            prev = v
    if hi_tab[-1] < CAM_X_MAX:
        sys.exit("el techo del corredor (%d) no llega a CAM_X_MAX (%d): el "
                 "nivel no podria terminar" % (hi_tab[-1], CAM_X_MAX))

    # ---------------------------------------------------------------- pasada 1
    # QUE CELDAS PUEDE LLEGAR A DIBUJAR EL RUNTIME, Y CON QUE PROGRESO.
    #
    # (17/09) Antes esto salia de SIMULAR UNA trayectoria de camara: la que va
    # pegada al PISO del corredor. Estaba mal. En la partida el jugador empuja
    # a la derecha y la camara va pegada al TECHO, asi que dibujaba columnas
    # que la simulacion nunca habia visto; esas celdas quedaban sin seccion
    # asignada (valor 0 en el tilemap = tile 0 = negro) y se veia una BANDA
    # NEGRA pegada al borde derecho de la pantalla durante todo el descenso.
    #
    # Ahora no se simula UN camino sino la ENVOLVENTE de TODOS. camX y camY
    # solo crecen y el corredor los acota, asi que para cada fila de plano R
    # el planeCol posible es un intervalo [Cmin(R), Cmax(R)]; de ahi sale
    # exactamente que celdas puede dibujar el runtime -- con cualquier
    # recorrido -- y el progreso MINIMO con el que se las puede pedir.
    def floor_of(cy):
        return lo_tab[min(max((cy - CAM_Y_MIN) >> 3, 0), len(lo_tab) - 1)]

    def ceil_of(cy):
        return hi_tab[min(max((cy - CAM_Y_MIN) >> 3, 0), len(hi_tab) - 1)]

    R_MIN = (CAM_Y_MIN + SCREEN_H - 1) >> 3
    R_MAX = (CAM_Y_MAX + SCREEN_H - 1) >> 3
    cmin, cmax = {}, {}
    for cy in range(CAM_Y_MIN, CAM_Y_MAX + 1):
        rr = (cy + SCREEN_H - 1) >> 3
        a = (max(CAM_X_MIN, floor_of(cy)) + SCREEN_W - 1) >> 3
        b = (min(CAM_X_MAX, ceil_of(cy)) + SCREEN_W - 1) >> 3
        cmin[rr] = min(cmin.get(rr, 1 << 30), a)
        cmax[rr] = max(cmax.get(rr, -1), b)

    def prog_state(cc, rr):
        """Progreso (camX+camY) MINIMO con el que se puede estar en el estado
        de plano (planeCol=cc, planeRow=rr)."""
        return max(CAM_Y_MIN, 8 * (cc + rr) - (SCREEN_W - 1) - (SCREEN_H - 1))

    minprog = {}

    def note(cc, rr, p):
        if cc < 0 or rr < 0 or cc >= COLS or rr >= ROWS:
            return
        k = (cc, rr)
        if k not in minprog or p < minprog[k]:
            minprog[k] = p

    # El bloque que dibuja lvl21BgInit de una, con la camara en el arranque.
    INIT_C = (CAM_X_MIN + SCREEN_W - 1) >> 3
    INIT_R = (CAM_Y_MIN + SCREEN_H - 1) >> 3
    for c in range(max(0, INIT_C - VIS_COLS + 1), INIT_C + 1):
        for r in range(max(0, INIT_R - VIS_ROWS + 1), INIT_R + 1):
            note(c, r, CAM_Y_MIN)

    for rr in range(R_MIN, R_MAX + 1):
        for cc in range(cmin[rr], cmax[rr] + 1):
            p = prog_state(cc, rr)
            # columna nueva a la derecha: se dibuja entera
            for r in range(max(0, rr - VIS_ROWS + 1), rr + 1):
                note(cc, r, p)
            # fila nueva abajo: se dibuja entera
            for c in range(max(0, cc - VIS_COLS + 1), cc + 1):
                note(c, rr, p)

    order = sorted(minprog.keys(), key=lambda k: (minprog[k], k[1], k[0]))
    print("celdas alcanzables:", len(order))

    # ---------------------------------------------------------------- pasada 2
    # Reparte las celdas CON ARTE en secciones siguiendo ese mismo orden.
    sec_of_cell = {}
    idx_of_cell = {}
    sections    = []            # [ (tiles, lookup) ]
    cur_tiles   = []
    cur_lookup  = {}
    cur_id      = 0
    sec_start_px = []           # progreso en el que arranca cada seccion
    sec_open_prog = None
    for cell in order:
        c, r = cell
        if not used[r, c]:
            continue                                   # cielo: queda en tile 0
        prog = minprog[cell]
        if sec_open_prog is None:
            sec_open_prog = prog
            sec_start_px.append(prog)
        elif prog - sec_open_prog >= SEC_SPAN_PX:
            sections.append((cur_tiles, cur_lookup))
            cur_id += 1
            cur_tiles, cur_lookup = [], {}
            sec_open_prog = prog
            sec_start_px.append(prog)
        key = tiles[r, c].tobytes()
        if key not in cur_lookup:
            cur_lookup[key] = len(cur_tiles)
            cur_tiles.append(key)
        sec_of_cell[cell] = cur_id
        idx_of_cell[cell] = cur_lookup[key]
    if cur_tiles:
        sections.append((cur_tiles, cur_lookup))

    n_sec = len(sections)
    print("secciones:", n_sec, "tiles:", [len(t) for t, _ in sections])
    # (17/09) El tilemap del mundo pasa a 6 bits de seccion + 9 de indice
    # (antes 5 + 10): con el corredor clavado hacen falta mas secciones y
    # ninguna pasa de 512 tiles.
    if n_sec > 64:
        sys.exit("mas de 64 secciones: no entran en los 6 bits del tilemap")
    big = max(len(t) for t, _ in sections)
    if big > 512:
        sys.exit("una seccion tiene %d tiles: no entra en los 9 bits del "
                 "indice. Baja SEC_SPAN_PX." % big)

    # ---------------------------------------------------------------- pasada 3
    # Verificacion del ring de VRAM, tambien sobre la ENVOLVENTE: para cada
    # celda se calcula el ultimo progreso en el que puede seguir VISIBLE y se
    # exige que su seccion siga viva hasta ese momento.
    maxprog = {}
    for (c, r) in order:
        best = -1
        cy_hi = min(CAM_Y_MAX, 8 * r + 7)
        cy_lo = max(CAM_Y_MIN, 8 * r - (SCREEN_H - 1))
        for cy in range(cy_hi, cy_lo - 1, -1):
            cx = min(ceil_of(cy), CAM_X_MAX, 8 * c + 7)
            if cx < floor_of(cy) or cx < 8 * c - (SCREEN_W - 1):
                continue
            best = cx + cy
            break
        maxprog[(c, r)] = best if best >= 0 else minprog[(c, r)]

    def occupies(b, ln):
        return set((b + i) % RING_TILES for i in range(ln))

    base       = [-1] * n_sec
    load_prog  = [0] * n_sec
    evict_prog = [1 << 30] * n_sec
    ring, loaded, worst = 0, [], 0
    for s in range(n_sec):
        p = sec_start_px[s] - PRELOAD_PX
        n = len(sections[s][0])
        newset = occupies(ring, n)
        for t in list(loaded):
            if occupies(base[t], len(sections[t][0])) & newset:
                evict_prog[t] = p
                loaded.remove(t)
        base[s], load_prog[s] = ring, p
        loaded.append(s)
        ring = (ring + n) % RING_TILES
        worst = max(worst, sum(len(sections[t][0]) for t in loaded))

    for cell, s in sec_of_cell.items():
        if minprog[cell] < load_prog[s]:
            sys.exit("FALLO: la celda %s se puede dibujar con progreso %d pero "
                     "su seccion %d recien se carga en %d"
                     % (cell, minprog[cell], s, load_prog[s]))
        if maxprog[cell] >= evict_prog[s]:
            sys.exit("FALLO: la celda %s puede seguir visible hasta el progreso "
                     "%d, pero el ring pisa su seccion %d en %d. Subi "
                     "RING_TILES o baja SEC_SPAN_PX."
                     % (cell, maxprog[cell], s, evict_prog[s]))
    print("ring OK. pico de tiles vivos:", worst, "/", RING_TILES)

    # ------------------------------------------------------------------ salida
    os.makedirs(OUTDIR, exist_ok=True)
    # Limpiar PNG de corridas anteriores: si una corrida previa genero mas
    # secciones, los sobrantes quedarian sueltos y confundirian (el .res los
    # ignora, pero se suben al repo por error).
    for old in os.listdir(OUTDIR):
        if old.startswith("lvl21_sec") and old.endswith(".png"):
            os.remove(os.path.join(OUTDIR, old))
    for s, (tl, _) in enumerate(sections):
        n = len(tl)
        cols = 16
        rows = (n + cols - 1) // cols
        buf = np.zeros((rows * 8, cols * 8), dtype=np.uint8)
        for i, key in enumerate(tl):
            t = np.frombuffer(key, dtype=np.uint8).reshape(8, 8)
            ty, tx = divmod(i, cols)
            buf[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8] = t
        out = Image.fromarray(buf, mode="P")
        out.putpalette(pal + [0] * (768 - len(pal)))
        out.save(os.path.join(OUTDIR, "lvl21_sec%02d.png" % s))

    # El .res y la tabla de TileSet del runtime se generan TAMBIEN aca: la
    # cantidad de secciones cambia con SEC_SPAN_PX y a mano se desincroniza
    # (paso: quedaron 11 declaradas y el generador escribio 13).
    with open(os.path.join(ROOT, "res", "level2_1.res"), "w") as f:
        f.write(RES_TXT % dict(nsec=n_sec))
        f.write('PALETTE lvl21_pal "images/lvl_2_scene/genesis/lvl21_pal.png"\n\n')
        for s2 in range(n_sec):
            f.write('TILESET lvl21_sec%02d "images/lvl_2_scene/genesis/'
                    'lvl21_sec%02d.png" NONE NONE\n' % (s2, s2))
    with open(os.path.join(SRCDIR, "level2_1_sections.h"), "w") as f:
        f.write("// GENERADO POR tools/gen_level2_1_bg.py -- NO EDITAR A MANO\n")
        f.write("#ifndef _LEVEL2_1_SECTIONS_H_\n#define _LEVEL2_1_SECTIONS_H_\n\n")
        f.write('#include "level2_1.h"\n#include "level2_1_map.h"\n\n')
        f.write("static const TileSet* const lvl21Sec[LVL21_SECTIONS] = {\n")
        for i in range(0, n_sec, 4):
            f.write("    " + ", ".join("&lvl21_sec%02d" % k
                                       for k in range(i, min(i + 4, n_sec))) + ",\n")
        f.write("};\n\n#endif\n")

    palimg = Image.fromarray(np.arange(16, dtype=np.uint8).reshape(1, 16), mode="P")
    palimg.putpalette(pal + [0] * (768 - len(pal)))
    palimg.save(os.path.join(OUTDIR, "lvl21_pal.png"))

    # tilemap del mundo, solo sobre el rectangulo realmente usado
    cells = [c for c in order]
    c0 = min(c for c, _ in cells); c1 = max(c for c, _ in cells)
    r0 = min(r for _, r in cells); r1 = max(r for _, r in cells)
    c0, r0 = 0, 0
    mw, mh = c1 - c0 + 1, r1 - r0 + 1
    world = np.zeros((mh, mw), dtype=np.uint16)
    for cell, s in sec_of_cell.items():
        c, r = cell
        # bit 15 = "esta celda tiene arte". Sin el, la celda (seccion 0,
        # tile 0) valdria 0 y se confundiria con el cielo.
        world[r - r0, c - c0] = 0x8000 | (s << 9) | idx_of_cell[cell]

    with open(os.path.join(SRCDIR, "level2_1_map.h"), "w") as f:
        f.write(HEADER_H % dict(w=mw, h=mh, nsec=n_sec, ring=RING_TILES,
                                preload=PRELOAD_PX, loadfr=LOAD_TILES_FR,
                                camymin=CAM_Y_MIN, camymax=CAM_Y_MAX,
                                camxmax=CAM_X_MAX,
                                camrows=(CAM_Y_MAX - CAM_Y_MIN) // 8 + 1))
    with open(os.path.join(SRCDIR, "level2_1_map.c"), "w") as f:
        f.write('#include "level2_1_map.h"\n\n')
        f.write("const u16 lvl21Map[%d * %d] = {\n" % (mh, mw))
        flat = world.reshape(-1)
        for i in range(0, len(flat), 16):
            f.write("    " + ",".join("0x%04X" % v for v in flat[i:i + 16]) + ",\n")
        f.write("};\n\n")
        f.write("const u16 lvl21SecTiles[%d] = {\n    " % n_sec)
        f.write(",".join(str(len(t)) for t, _ in sections))
        f.write("\n};\n\n")
        f.write("const u16 lvl21SecStart[%d] = {\n    " % n_sec)
        f.write(",".join(str(v) for v in sec_start_px))
        f.write("\n};\n\n")

        f.write("const u16 lvl21CamFloor[LVL21_CAM_ROWS] = {\n    ")
        f.write(",".join(str(v) for v in lo_tab))
        f.write("\n};\n\n")
        f.write("const u16 lvl21CamCeil[LVL21_CAM_ROWS] = {\n    ")
        f.write(",".join(str(v) for v in hi_tab))
        f.write("\n};\n")
    print("listo:", mw, "x", mh, "celdas de mundo")


RES_TXT = """// =============================================================================
// level2_1.res  Fondo del nivel 2-1: la calle (SCENE 2 del arcade)
// =============================================================================
// GENERADO POR tools/gen_level2_1_bg.py -- NO EDITAR A MANO.
//
// POR QUE ESTA PARTIDO EN SECCIONES
// El nivel completo son ~2069 tiles unicos y en VRAM entran ~770 (el resto se
// lo llevan los dos planos, el HUD y el presupuesto de SPR_initEx). Asi que el
// fondo se streamea POR TILES: el generador parte el recorrido en %(nsec)d
// secciones y level2_1.c mantiene en VRAM, en un buffer circular de
// LVL21_RING_TILES tiles, solo las que la camara esta usando.
//
// NONE NONE es OBLIGATORIO en todas:
//   - sin comprimir, porque los tiles se copian a mano con VDP_loadTileData a
//     una direccion de VRAM que cambia en cada vuelta del ring;
//   - sin deduplicar, porque el tilemap del mundo (src/level2_1_map.c) indexa
//     los tiles por su posicion EXACTA dentro de la seccion.
//
// La paleta va aparte (un PNG de 1x16) y se carga en PAL0.
// =============================================================================

"""

HEADER_H = """// GENERADO POR tools/gen_level2_1_bg.py -- NO EDITAR A MANO
#ifndef _LEVEL2_1_MAP_H_
#define _LEVEL2_1_MAP_H_

#include <genesis.h>

#define LVL21_MAP_W      %(w)d
#define LVL21_MAP_H      %(h)d
#define LVL21_SECTIONS   %(nsec)d
#define LVL21_RING_TILES %(ring)d
#define LVL21_PRELOAD_PX %(preload)d
#define LVL21_LOAD_TILES_PER_FRAME %(loadfr)d

// Corredor de camara: para cada camY (de a 8px, desde LVL21_CAM_Y_MIN) el
// minimo y el maximo camX que no dejan asomar zona sin arte.
//
// El TECHO es una escalera -- son los "topes" de camara marcados sobre el
// mapa -- y el PISO de cada peldano es el techo del peldano ANTERIOR. Con el
// freno de camY del runtime (no puede adelantarse a camX) eso da el recorrido
// del arcade: la camara avanza hasta el tope del escalon, se clava, y solo se
// libera cuando los jugadores bajan al escalon siguiente.
#define LVL21_CAM_Y_MIN  %(camymin)d
#define LVL21_CAM_Y_MAX  %(camymax)d
#define LVL21_CAM_X_MAX  %(camxmax)d
#define LVL21_CAM_ROWS   %(camrows)d

// Cada celda: bit15 = tiene arte, bits 9..14 = seccion, bits 0..8 = tile.
// Celda 0 = sin arte -> se dibuja con el tile 0 del VDP (negro).
extern const u16 lvl21Map[LVL21_MAP_W * LVL21_MAP_H];
extern const u16 lvl21SecTiles[LVL21_SECTIONS];
extern const u16 lvl21SecStart[LVL21_SECTIONS];
extern const u16 lvl21CamFloor[LVL21_CAM_ROWS];
extern const u16 lvl21CamCeil[LVL21_CAM_ROWS];

#endif
"""

if __name__ == "__main__":
    main()
