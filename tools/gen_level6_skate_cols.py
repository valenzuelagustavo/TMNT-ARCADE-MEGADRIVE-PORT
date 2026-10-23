#!/usr/bin/env python3
# =============================================================================
# gen_level6_skate_cols.py  (18/10) -- ruta de las patinetas (Scene 6) en
# formato ancho con DICCIONARIO DE COLUMNAS
# =============================================================================
# Antes iba como IMAGE de rescomp (bg_skate: 797x28 celdas, 44.632 bytes de
# tilemap). La ruta repite mucho: hay 525 columnas distintas, asi que se
# guardan solo esas (28 celdas seguidas cada una) mas un indice por columna:
# ~31 KB. Para ganar lugar en la ROM (ver rom-pasaba-4mb en el proyecto).
#
# Entrada: res/images/lvl_6_skate/bg_skate_p.png (la de gen_level6_skate.py)
# Salida:  skate_tiles.bin, skate_cols.bin, skate_coloff.bin (y skate_map.bin,
#          que NO va a la ROM). La paleta la toma rescomp del mismo png.
# Uso:     python3 tools/gen_level6_skate_cols.py
# =============================================================================
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stage_raw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = os.path.join(ROOT, 'res', 'images', 'lvl_6_skate')
stage_raw.build_cols(os.path.join(D, 'bg_skate_p.png'), D, 'skate')
