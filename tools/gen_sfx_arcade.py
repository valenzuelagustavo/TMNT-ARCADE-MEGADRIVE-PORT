# -*- coding: utf-8 -*-
# Genera los SFX del arcade (tanda del 16/10) en la convencion de la casa:
# MONO, 8-bit, 11025 Hz, puntas recortadas, pico 96,9% y, si quedan bajos,
# ganancia hasta TARGET_RMS con la rodilla suave tanh desde el 80% (mismo
# criterio que dinne_turtle / pizza_time).
#
# Fuente: los wav tal como llegaron (res/audio/<nombre original>.wav, estereo
# 8-bit 44100/48000 Hz). Salida: res/audio/gen/<nombre>.wav, que es lo que
# declara audio.res. Los originales no se tocan.
#
# Uso:  python3 tools/gen_sfx_arcade.py      (desde la raiz del proyecto)
import os
import sys
import wave

import numpy as np

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "res", "audio")
DST = os.path.join(SRC, "gen")

OUT_RATE = 11025
PEAK = 0.969
KNEE = 0.80
TARGET_RMS = 0.18     # golpes / sfx: arriba de esto no se les sube nada
MAX_GAIN = 4.0
PRE_MS, POST_MS, FADE_MS = 10, 20, 5
GATE = 0.03           # umbral (fraccion del pico) para recortar las puntas

# (fuente, salida)
SFX = [
    ("mike (don) attack.wav",                "mike_don_attack.wav"),
    ("leo (raph) attack.wav",                "leo_raph_attack.wav"),
    ("window_broken.wav",                    "window_broken.wav"),
    ("1 up.wav",                             "one_up.wav"),
    ("hydrant.wav",                          "hydrant.wav"),
    ("car.wav",                              "car.wav"),
    ("foot shuriken (kunai) throw.wav",      "shuriken_throw.wav"),
    ("mouser (rodney or manhole) hit.wav",   "mouser_hit.wav"),
    ("pause.wav",                            "pause.wav"),
    ("hit foot (hit rock soldier).wav",      "hit_foot.wav"),
    ("jump attack.wav",                      "jump_attack.wav"),
    ("turtles special attack.wav",           "special_attack.wav"),
    ("rodney lasso.wav",                     "rodney_lasso.wav"),
    ("rodney shoot.wav",                     "rodney_shoot.wav"),
    ("rodney stop.wav",                      "rodney_stop.wav"),
    ("get pizza.wav",                        "get_pizza.wav"),
    ("turtles shock pain.wav",               "shock_pain.wav"),
    # (18/10) Llegaron MONO 8-bit 16000 Hz y bajos (pico 25% / 9%).
    ("car_highway.wav",                      "car_highway.wav"),
    ("elevator_tecnodrome.wav",              "elevator_tecnodrome.wav"),
]


def read_wav(path):
    w = wave.open(path, "rb")
    ch, sw, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    raw = w.readframes(n)
    w.close()
    if sw == 1:
        a = (np.frombuffer(raw, np.uint8).astype(np.float64) - 128.0) / 128.0
    elif sw == 2:
        a = np.frombuffer(raw, "<i2").astype(np.float64) / 32768.0
    else:
        raise ValueError("%s: %d bytes por muestra" % (path, sw))
    a = a.reshape(-1, ch).mean(axis=1)
    return a, rate


def resample(a, rate):
    if rate == OUT_RATE:
        return a
    # Pasa-bajos (sinc con ventana de Blackman) por debajo del Nyquist de
    # salida y despues interpolacion lineal.
    fc = 0.45 * OUT_RATE / rate
    taps = 127
    k = np.arange(taps) - (taps - 1) / 2.0
    h = 2 * fc * np.sinc(2 * fc * k) * np.blackman(taps)
    h /= h.sum()
    f = np.convolve(a, h, mode="same")
    n_out = int(round(len(a) * OUT_RATE / rate))
    t = np.arange(n_out) * (rate / OUT_RATE)
    return np.interp(t, np.arange(len(f)), f)


def process(a, rate):
    a = a - a.mean()
    a = resample(a, rate)
    pk = np.abs(a).max()
    if pk <= 0:
        return a
    on = np.nonzero(np.abs(a) >= GATE * pk)[0]
    s = max(0, on[0] - PRE_MS * OUT_RATE // 1000)
    e = min(len(a), on[-1] + 1 + POST_MS * OUT_RATE // 1000)
    a = a[s:e]
    a = a * (PEAK / np.abs(a).max())
    rms = np.sqrt((a * a).mean())
    if rms < TARGET_RMS:
        g = min(MAX_GAIN, TARGET_RMS / rms)
        for _ in range(8):   # la rodilla baja el rms: se corrige un par de veces
            b = soft(a * g)
            r = np.sqrt((b * b).mean())
            if r >= TARGET_RMS * 0.995 or g >= MAX_GAIN:
                break
            g = min(MAX_GAIN, g * TARGET_RMS / r)
        a = soft(a * g)
    nf = FADE_MS * OUT_RATE // 1000
    if len(a) > nf:
        a[-nf:] *= np.linspace(1, 0, nf)
    return a


def soft(x):
    y = x.copy()
    m = np.abs(y) > KNEE
    span = PEAK - KNEE
    y[m] = np.sign(y[m]) * (KNEE + span * np.tanh((np.abs(y[m]) - KNEE) / span))
    return y


def write_wav(path, a):
    q = np.clip(np.round(a * 128.0 + 128.0), 0, 255).astype(np.uint8)
    w = wave.open(path, "wb")
    w.setnchannels(1)
    w.setsampwidth(1)
    w.setframerate(OUT_RATE)
    w.writeframes(q.tobytes())
    w.close()


def main():
    os.makedirs(DST, exist_ok=True)
    for src, dst in SFX:
        p = os.path.join(SRC, src)
        if not os.path.exists(p):
            print("FALTA   %-38s (se saltea)" % src)
            continue
        a, rate = read_wav(p)
        b = process(a, rate)
        write_wav(os.path.join(DST, dst), b)
        print("%-22s %5.3fs  pico %4.1f%%  rms %4.1f%%  %6d B" % (
            dst, len(b) / OUT_RATE, np.abs(b).max() * 100,
            np.sqrt((b * b).mean()) * 100, len(b)))


if __name__ == "__main__":
    sys.exit(main())
