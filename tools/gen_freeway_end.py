# -*- coding: utf-8 -*-
"""
gen_freeway_end.py (23/10) -- la BARRICADA del final de la autopista (5-1)
=========================================================================
vallas.png (96x104, el arte del artista) viene con su propia paleta (azules
del tronco con pinchos, amarillo y negro de las rayas, grises). En la
autopista PAL3 la usan el auto, las motos y los foot soldiers BLANCOS (que
estan en pantalla justo cuando se ve la barricada) y PAL2 los foot soldiers:
la valla va con PAL0, la paleta de la RUTA, que tiene un azul en tres tonos
(0,0,115) (0,90,148) (0,148,230), el amarillo de las lineas y los grises.

    res/images/lvl_5_freeway/fw_valla_md.png   la valla en la paleta de la ruta

pile_tire.png (las gomas apiladas, 2 frames de 56x80) ya viene en el orden
de indices de los foot soldiers (PAL2): se usa tal cual.

Uso:  python3 tools/gen_freeway_end.py
"""
import os
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
D = os.path.join(ROOT, 'res', 'images', 'lvl_5_freeway')

# indice de vallas.png -> indice de la paleta de la ruta (bg_freeway.png)
REMAP = {
    0: 0,     # transparente
    1: 2,     # negro (contorno)
    2: 15, 3: 15,          # la sombra (gris oscuro)
    4: 15,                 # gris azulado oscuro (patas en sombra)
    5: 5,                  # amarillo oscuro
    6: 13,                 # amarillo de las rayas
    7: 6,                  # azul mas oscuro del tronco
    9: 11,                 # azul
    10: 4,                 # azul claro
    11: 4,                 # celeste (brillo)
    8: 12,                 # gris azulado (patas)
    12: 10, 13: 10, 14: 10,  # grises claros (pinchos)
    15: 8,                 # blanco
}


def main():
    src = Image.open(os.path.join(D, 'vallas.png'))
    a = np.array(src)
    lut = np.zeros(256, np.uint8)
    for s, d in REMAP.items():
        lut[s] = d
    assert set(np.unique(a)) <= set(REMAP)
    out = Image.fromarray(lut[a], 'P')
    rp = Image.open(os.path.join(D, 'bg_freeway.png')).getpalette()[:48]
    out.putpalette(rp)
    out.save(os.path.join(D, 'fw_valla_md.png'), transparency=0)
    print('fw_valla_md.png', out.size)


if __name__ == '__main__':
    main()
