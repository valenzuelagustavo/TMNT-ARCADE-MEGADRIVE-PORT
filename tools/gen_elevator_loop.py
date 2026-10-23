# -*- coding: utf-8 -*-
# gen_elevator_loop.py (18/10) -- loop CORTO del ascensor del Technodrome
#
# elevator_tecnodrome.wav (de gen_sfx_arcade.py) dura 8 s y se iba apagando:
# a 6650 Hz ocupaba 53,5 KB y, en loop, cada 8 s el volumen saltaba de golpe.
# El sonido es un ciclo mecanico de 0,3997 s. Este script arma un loop de 4
# ciclos (~1,6 s) con volumen parejo:
#   - toma 4 ciclos desde el 2do (0,40 s), compensando el leve desvanecimiento
#     del tramo (el final queda al volumen del principio);
#   - lo pasa a 6650 Hz estirado a 42 x 256 muestras exactas (+1,1 %, no se
#     nota): XGM2 rellena los PCM a multiplos de 256 y en loop ese relleno
#     seria un hueco de silencio en cada vuelta;
#   - funde los ultimos 25 ms con el audio que va antes del inicio del tramo,
#     asi el empalme del loop no se nota.
# Salida: res/audio/gen/elevator_tecnodrome_loop.wav (8-bit, mono, 6650 Hz),
# ~10,5 KB en la ROM en vez de 53,5 KB.
# Uso: python3 tools/gen_elevator_loop.py
import os
import wave

import numpy as np

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "res", "audio", "gen", "elevator_tecnodrome.wav")
DST = os.path.join(ROOT, "res", "audio", "gen", "elevator_tecnodrome_loop.wav")

PERIOD = 0.39973          # s, medido por autocorrelacion
CYCLES = 4
OUT_RATE = 6650
OUT_LEN = 42 * 256        # 10752 muestras = 1,617 s
XFADE = 0.025             # s


def main():
    w = wave.open(SRC)
    r = w.getframerate()
    assert w.getsampwidth() == 1 and w.getnchannels() == 1
    a = np.frombuffer(w.readframes(w.getnframes()), np.uint8).astype(float) - 128.0

    t0 = int(round(PERIOD * r))
    L = int(round(CYCLES * PERIOD * r))
    seg = a[t0:t0 + L].copy()
    pre = a[:t0]                                     # lo que va antes del tramo

    # Volumen parejo: rms del primer y ultimo ciclo, rampa lineal de ganancia.
    P = L // CYCLES
    r0 = np.sqrt((seg[:P] ** 2).mean())
    r1 = np.sqrt((seg[-P:] ** 2).mean())
    seg *= np.linspace(1.0, r0 / r1, L)

    # Empalme: los ultimos N se funden con los N que van antes de t0.
    N = int(XFADE * r)
    k = np.linspace(0.0, 1.0, N)
    seg[-N:] = seg[-N:] * (1 - k) + pre[-N:] * k

    # A 6650 Hz con OUT_LEN muestras exactas (interpolacion lineal, ciclica).
    pos = np.arange(OUT_LEN) * (L / OUT_LEN)
    i0 = np.floor(pos).astype(int)
    fr = pos - i0
    out = seg[i0] * (1 - fr) + seg[(i0 + 1) % L] * fr
    out = np.clip(np.round(out + 128), 0, 255).astype(np.uint8)

    o = wave.open(DST, "wb")
    o.setnchannels(1)
    o.setsampwidth(1)
    o.setframerate(OUT_RATE)
    o.writeframes(out.tobytes())
    o.close()
    print("elevator_tecnodrome_loop.wav: %d muestras a %d Hz (%.3f s), rms %d -> %d compensado"
          % (OUT_LEN, OUT_RATE, OUT_LEN / OUT_RATE, r0, r1))


if __name__ == "__main__":
    main()
