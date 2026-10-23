#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_mapper.py (23/10) -- revisa el reparto de la ROM para el mapper SSF
=======================================================================
Con ENABLE_BANK_SWITCH, SGDK usa las regiones 6 y 7 (0x300000-0x3FFFFF) como
ventanas para los datos "FAR". Todo lo que el juego lee DIRECTO de la ROM (el
codigo, los .res marcados NEAR: fondos streameados, tiles por cuadro, audio)
tiene que quedar en los primeros 3 MB (< 0x300000); lo FAR puede ir despues.

Lee out/symbol.txt, out/rom.bin y los res/*.res y dice:
  - cuanto ocupa la ROM y si pasa de 4 MB (ahi el mapper deja de ser opcional);
  - donde termina lo NEAR y cuanto queda hasta 0x300000;
  - cuanto hay en FAR y que .res lo aportan.
Sale con codigo 1 si la ROM pasa de 4 MB y lo NEAR no entra en 3 MB.

Uso (desde la raiz del proyecto, despues de compilar):
    python tools/mapper/check_mapper.py
"""
import os
import re
import sys
import glob
import collections

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
NEAR_LIMIT = 0x300000
ROM_4MB = 0x400000


def res_files():
    out = {}
    for f in sorted(glob.glob(os.path.join(ROOT, 'res', '*.res'))):
        near = False
        names = []
        for l in open(f, encoding='latin1'):
            code = l.split('//')[0].strip()
            if not code:
                continue
            if re.match(r'^NEAR$', code):
                near = True
                continue
            m = re.match(r'^(\w+)\s+(\w+)\s+"', code)
            if m:
                names.append((m.group(1), m.group(2)))
        out[os.path.basename(f)] = (near, names)
    return out


def main():
    rom = open(os.path.join(ROOT, 'out', 'rom.bin'), 'rb').read()
    end = len(rom) - 1
    while end > 0 and rom[end] in (0x00, 0xFF):
        end -= 1
    used = end + 1

    syms = []
    for l in open(os.path.join(ROOT, 'out', 'symbol.txt'), encoding='latin1'):
        p = l.split()
        if len(p) < 3 or p[1] in 'aA':
            continue
        a = int(p[0], 16)
        if a < used:
            syms.append((a, p[2]))
    syms.sort()

    files = res_files()
    owner = {}
    names = []
    for f, (near, lst) in files.items():
        for t, n in lst:
            names.append((n, f, near))
    names.sort(key=lambda x: -len(x[0]))

    def own(sym):
        for n, f, near in names:
            if sym == n or sym.startswith(n + '_'):
                return f, near
        return None, None

    far_kb = collections.Counter()
    near_end = 0
    far_start = None
    for i, (a, n) in enumerate(syms):
        nxt = syms[i + 1][0] if i + 1 < len(syms) else used
        f, near = own(n)
        # Los blobs FAR: datos (no las estructuras ni las paletas) de un .res sin NEAR
        is_far = (f is not None and not near and n.endswith('_data')
                  and not n.endswith('palette_data'))
        if is_far:
            far_kb[f] += nxt - a
            far_start = a if far_start is None else min(far_start, a)
        else:
            near_end = max(near_end, nxt)

    print('ROM usada      : %7.1f KB (%s 4 MB)' % (used / 1024, 'MAS de' if used > ROM_4MB else 'dentro de'))
    print('NEAR hasta     : 0x%06X = %7.1f KB  (limite 3 MB: quedan %.1f KB)'
          % (near_end, near_end / 1024, (NEAR_LIMIT - near_end) / 1024))
    print('FAR            : %7.1f KB desde 0x%06X' % (sum(far_kb.values()) / 1024, far_start or 0))
    for f, s in far_kb.most_common():
        print('    %-22s %7.1f KB' % (f, s / 1024))
    nears = [f for f, (near, _) in files.items() if near]
    print('Archivos NEAR  : ' + ', '.join(n[:-4] for n in nears))
    if near_end > NEAR_LIMIT:
        msg = 'lo NEAR pasa de 3 MB (0x%06X)' % near_end
        if used > ROM_4MB:
            print('ERROR: ' + msg + ' y la ROM pasa de 4 MB: con el mapper se leerian datos equivocados.')
            return 1
        print('AVISO: ' + msg + '. Mientras la ROM no pase de 4 MB funciona igual, pero hay que bajarlo antes de crecer.')
    else:
        print('OK')
    return 0


if __name__ == '__main__':
    sys.exit(main())
