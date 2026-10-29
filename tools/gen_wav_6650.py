#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_wav_6650.py (28/10) -- pasa los WAV de res/audio.res a 6650 Hz (la mitad)
==========================================================================
El driver XGM2 solo acepta 13300 Hz (por defecto) o 6650 Hz (halfRate). A 6650 Hz
cada muestra pesa la mitad en la ROM. Esto:
  1. copia el WAV de partida a res/audio/originales_pre6650/<ruta> (una vez;
     los originales NO se tocan ni se sobreescriben);
  2. lo pasa a mono y 6650 Hz con filtro pasa-bajos (resample_poly, kaiser) y lo
     deja en res/audio/6650/<ruta> (16 bit, asi rescomp no re-muestrea);
  3. en res/audio.res apunta la linea WAV al nuevo archivo y le agrega " 6650".

Idempotente: si la linea ya apunta a audio/6650/..., parte del original guardado.
El codigo reproduce estos WAV con halfRate = TRUE (XGM2_playPCMEx).

Uso (desde la raiz del proyecto):  python tools/gen_wav_6650.py
"""
import os
import re
import shutil
import wave
from fractions import Fraction

import numpy as np
from scipy.signal import resample_poly

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
RES = os.path.join(ROOT, 'res')
OUT_RATE = 6650
BACKUP = 'audio/originales_pre6650'
DEST = 'audio/6650'


def read_wav(path):
    w = wave.open(path, 'rb')
    n, ch, sw, fr = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
    raw = w.readframes(n)
    w.close()
    if sw == 1:
        x = (np.frombuffer(raw, dtype=np.uint8).astype(np.float64) - 128.0) / 128.0
    elif sw == 2:
        x = np.frombuffer(raw, dtype='<i2').astype(np.float64) / 32768.0
    else:
        raise ValueError('ancho de muestra %d no soportado: %s' % (sw, path))
    if ch > 1:
        x = x.reshape(-1, ch).mean(axis=1)
    return x, fr


def convert(src, dst):
    x, fr = read_wav(src)
    f = Fraction(OUT_RATE, fr)
    y = resample_poly(x, f.numerator, f.denominator, window=('kaiser', 8.6)) if fr != OUT_RATE else x
    y = np.clip(y, -1.0, 1.0)
    pcm = np.round(y * 32767.0).astype('<i2')
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    w = wave.open(dst, 'wb')
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(OUT_RATE)
    w.writeframes(pcm.tobytes())
    w.close()
    return len(x), fr, len(y)


def main():
    path = os.path.join(RES, 'audio.res')
    raw = open(path, 'rb').read().decode('latin-1')
    crlf = '\r\n' in raw
    lines = raw.replace('\r\n', '\n').split('\n')
    out = []
    tot_in = tot_out = 0
    for l in lines:
        m = re.match(r'^(WAV\s+\w+\s+")([^"]+)("\s+XGM2)(\s+\d+)?(\s*(?://.*)?)$', l)
        if not m or m.group(4):          # no es WAV XGM2, o ya tiene frecuencia (6650)
            out.append(l)
            continue
        rel = m.group(2)
        if rel.startswith(DEST + '/'):
            rel = rel[len(DEST) + 1:]
        else:
            rel = rel[len('audio/'):]
        bak = os.path.join(RES, BACKUP, rel)
        if not os.path.exists(bak):
            os.makedirs(os.path.dirname(bak), exist_ok=True)
            shutil.copy2(os.path.join(RES, 'audio', rel), bak)
        dst = os.path.join(RES, DEST, rel)
        n_in, fr, n_out = convert(bak, dst)
        tot_in += n_in * (13300.0 / fr)
        tot_out += n_out
        out.append('%s%s/%s%s %d%s' % (m.group(1), DEST, rel, m.group(3), OUT_RATE, m.group(5)))
        print('%-40s %6d Hz -> %d Hz  (%d -> %d muestras)' % (rel, fr, OUT_RATE, n_in, n_out))
    s = '\n'.join(out)
    if crlf:
        s = s.replace('\n', '\r\n')
    open(path, 'wb').write(s.encode('latin-1'))
    print('Total aprox. a 13300 Hz: %.1f KB  ->  a 6650 Hz: %.1f KB' % (tot_in / 1024, tot_out / 1024))


if __name__ == '__main__':
    main()
