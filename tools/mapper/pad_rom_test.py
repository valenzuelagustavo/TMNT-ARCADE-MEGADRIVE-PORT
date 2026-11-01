#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pad_rom_test.py (01/11) -- rellena la ROM FULL hasta 5 MB para probar el mapper en flash carts viejos
==================================================================================================
Prueba para el Everdrive MD v2.6: ese cart solo activa el mapper SSF con ROMs de mas de 4 MB, asi que
a la edicion FULL (que hoy mide ~3,9 MB) se le agregan bytes 0xFF hasta 5 MB (0x500000). El juego es el
mismo; los bytes de relleno nunca se leen. NO se publica: es solo para pruebas con voluntarios.

Uso:  python tools/mapper/pad_rom_test.py      (despues de build_rom_ssf.bat)
Lee out/tmnt_megadrive_port_<version>_full.bin y escribe ..._full_pad5mb.bin
"""
import glob
import os
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
SIZE = 0x500000


def main():
    src = sorted(glob.glob(os.path.join(ROOT, 'out', 'tmnt_megadrive_port_*_full.bin')), key=os.path.getmtime)
    if not src:
        print('Falta out/tmnt_megadrive_port_<version>_full.bin: correr tools\\mapper\\build_rom_ssf.bat primero')
        return 1
    d = open(src[-1], 'rb').read()
    if d[0x100:0x108] != b'SEGA SSF':
        print('ERROR: %s no tiene cabecera SEGA SSF (no es la edicion FULL)' % os.path.basename(src[-1]))
        return 1
    if len(d) > SIZE:
        print('La ROM ya pasa de 5 MB; no hace falta rellenar')
        return 1
    dst = src[-1][:-4] + '_pad5mb.bin'
    open(dst, 'wb').write(d + b'\xff' * (SIZE - len(d)))
    print('OK -> %s (%d KB de ROM + relleno hasta 5120 KB)' % (os.path.relpath(dst, ROOT), len(d) // 1024))
    return 0


if __name__ == '__main__':
    sys.exit(main())
