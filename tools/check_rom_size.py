# -*- coding: utf-8 -*-
# check_rom_size.py (18/10) -- la ROM tiene que entrar en 4 MB
#
# La Mega Drive ve 4 MB de cartucho (0x000000-0x3FFFFF) sin mapper. Lo que
# rescomp/ld pongan mas arriba (recursos, codigo o los valores iniciales de las
# globales, que van al final) Gens, Kega y la consola lo leen como basura;
# Mednafen mapea la ROM entera y NO lo muestra. Paso: el 18/10 la ROM llego a
# 0x420000 (opciones con basura, Kega colgado, humo del 1-1 y Traag rotos).
#
# Uso (despues de compilar):  python3 tools/check_rom_size.py
import os
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
LIMIT = 0x400000

size = os.path.getsize(os.path.join(ROOT, "out", "rom.bin"))
end = 0
for line in open(os.path.join(ROOT, "out", "symbol.txt")):
    p = line.split()
    if len(p) >= 3 and p[2] in ("_stext",):
        end = int(p[0], 16)
data = 0
for line in open(os.path.join(ROOT, "out", "symbol.txt")):
    p = line.split()
    if len(p) >= 3 and p[2] == "_edata":
        data = int(p[0], 16) & 0xFFFF          # RAM 0xE0FF0000 + tamano de .data
used = end + data
print("ROM: %d bytes (0x%06X), usado hasta 0x%06X, libre %d KB de 4096"
      % (size, size, used, (LIMIT - used) // 1024))
if used > LIMIT or size > LIMIT:
    print("ERROR: la ROM pasa los 4 MB: lo de arriba de 0x400000 no existe en la consola")
    sys.exit(1)
