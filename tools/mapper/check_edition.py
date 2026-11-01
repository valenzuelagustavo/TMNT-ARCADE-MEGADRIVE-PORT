#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_edition.py (01/11) -- revisa out/rom.bin segun la edicion (FULL o LITE) y la copia con nombre
==============================================================================================
Una sola base de codigo, dos ediciones (ver src/build_config.h):

  FULL  cabecera "SEGA SSF"        fin de ROM 0x7FFFFF  (mapper; puede pasar de 4 MB)
  LITE  cabecera "SEGA MEGA DRIVE" fin de ROM 0x3FFFFF  (sin mapper; tiene que entrar en 4 MB)

Uso:  python tools/mapper/check_edition.py full|lite
Sale con codigo 1 si la cabecera no coincide con la edicion pedida o si LITE pasa de 4 MB.
Si todo esta bien copia out/rom.bin a out/tmnt_megadrive_port_<version>_<edicion>.bin
(la version sale de GAME_VERSION en src/scenes.c). Esa copia es la que se publica.
"""
import os
import re
import shutil
import struct
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
ROM = os.path.join(ROOT, 'out', 'rom.bin')
LIMIT_4MB = 0x400000
EXPECT = {
    'full': ('SEGA SSF', 0x7FFFFF),
    'lite': ('SEGA MEGA DRIVE', 0x3FFFFF),
}


def game_version():
    src = open(os.path.join(ROOT, 'src', 'scenes.c'), encoding='latin1').read()
    m = re.search(r'#define\s+GAME_VERSION\s+"([^"]+)"', src)
    return m.group(1) if m else 'x.y.z'


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in EXPECT:
        print(__doc__)
        return 2
    ed = sys.argv[1]
    d = open(ROM, 'rb').read()
    system = d[0x100:0x110].decode('latin1').strip()
    rom_end = struct.unpack('>I', d[0x1A4:0x1A8])[0]
    want_sys, want_end = EXPECT[ed]
    ok = True
    print('edicion %s: cabecera "%s", fin de ROM 0x%X, tamano %d KB' % (ed.upper(), system, rom_end, len(d) // 1024))
    if system != want_sys:
        print('ERROR: la cabecera deberia decir "%s"' % want_sys)
        ok = False
    if rom_end != want_end:
        print('ERROR: el fin de ROM deberia ser 0x%X' % want_end)
        ok = False
    if ed == 'lite' and len(d) > LIMIT_4MB:
        print('ERROR: LITE no entra en 4 MB (%d KB de 4096 KB): hay que recortar contenido extra' % (len(d) // 1024))
        ok = False
    elif ed == 'lite':
        print('LITE: quedan %d KB hasta los 4 MB' % ((LIMIT_4MB - len(d)) // 1024))
    if not ok:
        print('(si los .o de out/ son de la otra edicion: hacer "clean"; los scripts ya lo hacen)')
        return 1
    dst = os.path.join(ROOT, 'out', 'tmnt_megadrive_port_%s_%s.bin' % (game_version(), ed))
    shutil.copyfile(ROM, dst)
    print('OK -> %s' % os.path.relpath(dst, ROOT))
    return 0


if __name__ == '__main__':
    sys.exit(main())
