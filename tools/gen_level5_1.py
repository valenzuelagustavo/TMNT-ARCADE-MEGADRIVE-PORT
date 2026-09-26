#!/usr/bin/env python3
# =============================================================================
# gen_level5_1.py  (26/09) -- fondo de la Scene 5 (freeway) en formato ancho
# =============================================================================
# La ruta de la autopista (bg_freeway_near.png del proyecto de Ray, recortada a
# las 224 filas de abajo -> res/images/lvl_5_freeway/bg_freeway.png) tiene 2369
# tiles unicos aun deduplicando con flips: no entra en el indice de 11 bits del
# tilemap de un IMAGE de rescomp. Arma bg_freeway_tiles.bin y
# bg_freeway_map.bin (ver tools/stage_raw.py).
#
# Uso: python3 tools/gen_level5_1.py
# =============================================================================
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stage_raw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = os.path.join(ROOT, 'res', 'images', 'lvl_5_freeway')
stage_raw.build(os.path.join(D, 'bg_freeway.png'), D, 'bg_freeway')
