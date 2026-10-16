#!/usr/bin/env python3
# ============================================================================
# vgm_free_fm6.py -- Libera el canal FM6 de un vgm para poder tocar PCM encima
# ============================================================================
# (12/10) QUE PROBLEMA RESUELVE
#   El PCM de XGM2 (voces, SFX) sale por el DAC del YM2612, que es el canal
#   FM6. "01 - Opening Demo.vgm" usa FM6 como canal FM (un golpe de percusion
#   en corcheas) y ademas escribe el registro 0x2B = 0 (DAC apagado): con ese
#   tema sonando, cualquier PCM queda MUDO (por eso no salia la voz de la intro).
#
#   En ese tema FM5 es una COPIA EXACTA de FM4 (el bajo doblado), asi que se
#   puede sacrificar sin perder ninguna parte: este script
#     - descarta todo lo que se escribe en FM5 (registros y key on/off),
#     - mueve todo lo de FM6 a FM5 (mismo instrumento, mismas notas),
#     - borra las escrituras al registro 0x2B (el DAC lo maneja el driver).
#   Resultado: misma musica (el bajo pierde el doblaje) y FM6 libre para PCM.
#
# USO (desde la raiz):
#   python3 tools/vgm_free_fm6.py "res/music/01 - Opening Demo.vgm" \
#                                 "res/music/01 - Opening Demo (FM6 libre).vgm"
# ============================================================================
import struct, sys

def cmd_len(d, i):
    c = d[i]
    if 0x30 <= c <= 0x3F or c in (0x4F, 0x50, 0x94): return 2
    if 0x40 <= c <= 0x4E or 0x51 <= c <= 0x5F or c == 0x61 or 0xA0 <= c <= 0xBF: return 3
    if c in (0x62, 0x63, 0x66) or 0x70 <= c <= 0x8F: return 1
    if c == 0x67: return 7 + struct.unpack('<I', d[i+3:i+7])[0]
    if c == 0x68: return 12
    if c in (0x90, 0x91, 0x95): return 5
    if c == 0x92: return 6
    if c == 0x93: return 11
    if 0xC0 <= c <= 0xDF: return 4
    if 0xE0 <= c <= 0xFF: return 5
    raise ValueError("comando vgm desconocido 0x%02X en 0x%X" % (c, i))

def main(src, dst):
    d = bytearray(open(src, 'rb').read())
    rel = struct.unpack('<I', d[0x34:0x38])[0]
    start = 0x34 + rel if rel else 0x40
    eof = 4 + struct.unpack('<I', d[4:8])[0]
    gd3rel = struct.unpack('<I', d[0x14:0x18])[0]
    gd3 = 0x14 + gd3rel if gd3rel else 0
    loopoff = struct.unpack('<I', d[0x1C:0x20])[0]
    assert loopoff == 0, "este script no recalcula el punto de loop"
    out = bytearray(d[:start]); i = start
    dropped = moved = 0
    while i < len(d):
        c = d[i]; n = cmd_len(d, i); cm = bytearray(d[i:i+n])
        if c == 0x66:
            out += cm; i += n; break
        if c in (0x52, 0x53):
            r, v = cm[1], cm[2]
            if c == 0x52 and r == 0x2B:            # DAC on/off: lo maneja el driver
                i += n; dropped += 1; continue
            if c == 0x52 and r == 0x28:            # key on/off
                ch = v & 7
                if ch == 5: i += n; dropped += 1; continue      # FM5 (bajo doblado)
                if ch == 6: cm[2] = (v & 0xF8) | 5; moved += 1   # FM6 -> FM5
            if c == 0x53 and r >= 0x30 and (r & 3) != 3:
                if (r & 3) == 1: i += n; dropped += 1; continue  # registros de FM5
                if (r & 3) == 2: cm[1] = r - 1; moved += 1       # FM6 -> FM5
        out += cm; i += n
    end_data = i
    tail = d[end_data:]                      # GD3 y lo que siga
    shift = len(out) - end_data
    out += tail
    struct.pack_into('<I', out, 4, len(out) - 4)
    if gd3:
        struct.pack_into('<I', out, 0x14, gd3 + shift - 0x14)
    open(dst, 'wb').write(out)
    print("%d escrituras descartadas, %d movidas de FM6 a FM5 -> %s" % (dropped, moved, dst))

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
