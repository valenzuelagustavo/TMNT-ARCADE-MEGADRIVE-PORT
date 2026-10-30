#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_temp_palette.py (29/10) -- solucion TEMPORAL de colores del foot purple (idea del artista)
==========================================================================================
Paleta nueva (NEW_PALETA_FOOT_SOLDIERS.act) = los 3 morados nuevos:
    oscuro (72, 0, 145)   medio (145, 36, 218)   claro (182, 109, 255)
  1. Paleta "chamas" (PAL2, la compartida de enemigos): sus 3 morados viejos (indices 1, 2, 3)
     pasan a los nuevos:   (96,0,64) -> oscuro, (160,0,160) -> medio, (192,64,192) -> claro.
  2. Paleta de las tortugas (PAL1): los 2 violetas (indices 11 y 12) pasan a oscuro y medio, igual
     que en la hoja nueva de Leo del artista (1600_1604_turtles): (74,0,222) -> (72,0,145),
     (181,0,255) -> (145,36,218).
Solo se cambian las ENTRADAS de la paleta de cada PNG indexado (se reescribe el chunk PLTE; los
pixeles / indices no se tocan). Se aplica a todos los PNG de res/ que tengan la paleta vieja en
esos indices (menos res/sprites/players/orig/, copias de respaldo). Los archivos tocados quedan en
tools/gen_temp_palette_log.json.
Uso:   python3 tools/gen_temp_palette.py            aplica (idempotente)
       python3 tools/gen_temp_palette.py --revert   vuelve a la paleta vieja (los del log)
"""
import json
import os
import struct
import sys
import zlib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
RES = os.path.join(ROOT, 'res')
LOG = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'gen_temp_palette_log.json')

DARK, MID, LIGHT = (72, 0, 145), (145, 36, 218), (182, 109, 255)
# nombre -> {indice: (viejo, nuevo)}
GROUPS = {
    'enemigos (PAL2, chamas)': {1: ((96, 0, 64), DARK), 2: ((160, 0, 160), MID), 3: ((192, 64, 192), LIGHT)},
    'tortugas (PAL1)':         {11: ((74, 0, 222), DARK), 12: ((181, 0, 255), MID)},
}
SKIP = ('audio', 'music', '_descartados', os.path.join('sprites', 'players', 'orig'))


def read_plte(data):
    """(offset de los datos del PLTE, largo) o None. Solo PNG indexados."""
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        return None
    pos = 8
    while pos + 8 <= len(data):
        ln, typ = struct.unpack('>I4s', data[pos:pos + 8])
        if typ == b'PLTE':
            return pos + 8, ln
        if typ in (b'IDAT', b'IEND'):
            return None
        pos += 12 + ln
    return None


def patch(data, offs, ln, swaps):
    """swaps: {indice: (de, a)}. Devuelve (datos nuevos, cambiado?)."""
    b = bytearray(data)
    changed = False
    for idx, (de, a) in swaps.items():
        if idx * 3 + 3 > ln:
            return data, False
        o = offs + idx * 3
        if tuple(b[o:o + 3]) == de:
            b[o:o + 3] = bytes(a)
            changed = True
    if changed:
        crc = zlib.crc32(b'PLTE' + bytes(b[offs:offs + ln])) & 0xffffffff
        b[offs + ln:offs + ln + 4] = struct.pack('>I', crc)
    return bytes(b), changed


def matches(data, offs, ln, spec, which):
    for idx, (old, new) in spec.items():
        if idx * 3 + 3 > ln:
            return False
        if tuple(data[offs + idx * 3: offs + idx * 3 + 3]) != (old if which == 'old' else new):
            return False
    return True


def main():
    revert = '--revert' in sys.argv
    log = {}
    if revert:
        log = json.load(open(LOG))
    done = {}
    for root, ds, fs in os.walk(RES):
        rel_root = os.path.relpath(root, RES)
        parts = rel_root.split(os.sep)
        if any(s in parts for s in ('audio', 'music', '_descartados')) or rel_root.startswith(SKIP[3]):
            ds[:] = []
            continue
        for f in fs:
            if not f.lower().endswith('.png'):
                continue
            p = os.path.join(root, f)
            rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
            if revert and rel not in log:
                continue
            data = open(p, 'rb').read()
            pl = read_plte(data)
            if not pl:
                continue
            offs, ln = pl
            new = data
            tags = []
            for name, spec in GROUPS.items():
                if revert:
                    if name in log.get(rel, []) and matches(new, offs, ln, spec, 'new'):
                        new, ch = patch(new, offs, ln, {i: (n, o) for i, (o, n) in spec.items()})
                        tags.append(name)
                else:
                    if matches(new, offs, ln, spec, 'old'):
                        new, ch = patch(new, offs, ln, spec)
                        tags.append(name)
            if tags:
                open(p, 'wb').write(new)
                done[rel] = tags
    if revert:
        print('revertidos: %d archivos' % len(done))
    else:
        old = json.load(open(LOG)) if os.path.exists(LOG) else {}
        old.update(done)
        json.dump(old, open(LOG, 'w'), indent=1, sort_keys=True)
        for rel, t in sorted(done.items()):
            print('%-80s %s' % (rel, ', '.join(t)))
        print('modificados ahora: %d (log: %d archivos)' % (len(done), len(old)))


if __name__ == '__main__':
    main()
