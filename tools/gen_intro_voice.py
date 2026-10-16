#!/usr/bin/env python3
# ============================================================================
# gen_intro_voice.py -- Prepara la voz cantada de la intro (12/10)
# ============================================================================
# ENTRADA  res/music/intro_voice/0X_*.wav   (las originales, no se tocan)
# SALIDA   res/music/intro_voice/gen/0X_*.wav  (las que declara audio.res)
#
# Dos arreglos:
#   1. VOLUMEN: los WAV vienen muy bajos (picos de 15-19 sobre 128 en 8 bits):
#      encima de la musica no se escuchaban. Se normalizan a PEAK.
#   2. TEMPO: el tema del arcade va mas lento que nuestro vgm. Medido en el
#      video de la intro del arcade (gameplayARCADE/videoplayback.mp4): compas
#      de 2,595 s contra 2,133 s del vgm. Las voces se COMPRIMEN en el tiempo
#      a 2,133/2,595 SIN cambiar el tono (WSOLA: se pegan pedacitos de 30 ms
#      solapados, buscando el empalme que mejor calza), asi las silabas caen
#      sobre las notas del vgm. Cambiar la velocidad resampleando subiria el
#      tono ~3,4 semitonos y desafinaria con el tema.
#
# Correr desde la raiz:  python3 tools/gen_intro_voice.py
# ============================================================================
import os, glob, wave
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC  = os.path.join(ROOT, "res", "music", "intro_voice")
DST  = os.path.join(SRC, "gen")

RATE    = 16000                 # salida (rescomp la pasa a 13,3 kHz)
STRETCH = 2.133 / 2.595         # duracion nueva / vieja
PEAK    = 118                   # pico de salida (8 bits sin signo, +-127)
WIN     = 480                   # 30 ms
HOP_OUT = WIN // 2
TOL     = 160                   # +-10 ms de busqueda del empalme


def read(path):
    w = wave.open(path)
    n, sr, sw, ch = w.getnframes(), w.getframerate(), w.getsampwidth(), w.getnchannels()
    raw = w.readframes(n)
    a = (np.frombuffer(raw, np.uint8).astype(np.float64) - 128.0) if sw == 1 \
        else np.frombuffer(raw, np.int16).astype(np.float64) / 256.0
    if ch > 1:
        a = a.reshape(-1, ch).mean(1)
    if sr != RATE:                                   # resampleo lineal
        t = np.arange(int(len(a) * RATE / sr)) * (sr / RATE)
        a = np.interp(t, np.arange(len(a)), a)
    return a - a.mean()


def wsola(x, alpha):
    hop_in = HOP_OUT / alpha
    win = np.hanning(WIN)
    n_out = int(len(x) * alpha) + WIN
    y = np.zeros(n_out); norm = np.zeros(n_out)
    xp = np.concatenate([np.zeros(TOL), x, np.zeros(WIN + TOL + int(hop_in) + 2)])
    prev = None
    k = 0
    while True:
        out_pos = k * HOP_OUT
        in_pos = int(round(k * hop_in))
        if in_pos >= len(x) or out_pos + WIN > n_out:
            break
        base = in_pos + TOL
        if prev is None:
            best = base
        else:
            # empalme: el segmento que mejor continua al anterior
            target = xp[prev + HOP_OUT: prev + HOP_OUT + WIN]
            best, bestv = base, -1e18
            for d in range(-TOL, TOL + 1, 4):
                s = xp[base + d: base + d + WIN]
                v = float(np.dot(s, target))
                if v > bestv:
                    bestv, best = v, base + d
        y[out_pos:out_pos + WIN] += xp[best:best + WIN] * win
        norm[out_pos:out_pos + WIN] += win
        prev = best
        k += 1
    norm[norm < 1e-6] = 1.0
    return (y / norm)[:int(len(x) * alpha)]


def main():
    os.makedirs(DST, exist_ok=True)
    for path in sorted(glob.glob(os.path.join(SRC, "*.wav"))):
        x = read(path)
        y = wsola(x, STRETCH)
        y = y * (PEAK / max(1e-6, np.abs(y).max()))
        out = np.clip(np.round(y) + 128, 0, 255).astype(np.uint8)
        dst = os.path.join(DST, os.path.basename(path))
        w = wave.open(dst, "wb")
        w.setnchannels(1); w.setsampwidth(1); w.setframerate(RATE)
        w.writeframes(out.tobytes()); w.close()
        print("%-32s %.2f s -> %.2f s" % (os.path.basename(path), len(x) / RATE, len(y) / RATE))


if __name__ == "__main__":
    main()
