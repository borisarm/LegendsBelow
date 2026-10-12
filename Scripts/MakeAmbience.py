# MakeAmbience.py - Synthesizes the dungeon tiers' ambience loops (Assets/Sounds/ambience_*.wav):
# a crypt's low drone with distant drips, a cave's dripping water, the wind through ruins, and the
# hush of an archive (pages stirring, a faint glassy chime).
# Made for LegendsBelow and released as CC0. Plain Python (no packages); deterministic.
#
#   python Scripts/MakeAmbience.py [output directory, default Assets/Sounds]

import math
import random
import struct
import sys
import wave
from pathlib import Path

RATE = 22050
SECONDS = 16.0
FADE = 1.5          # seconds of the end blended into the start, so the loop has no seam


def one_pole(samples, cutoff):
    """A gentle low-pass filter"""
    a = math.exp(-2.0 * math.pi * cutoff / RATE)
    out, y = [], 0.0
    for x in samples:
        y = (1.0 - a) * x + a * y
        out.append(y)
    return out


def band_pass(samples, centers, q):
    """A resonant band-pass (state-variable filter) whose center follows `centers`"""
    out, low, band = [], 0.0, 0.0
    damp = 1.0 / q
    for x, center in zip(samples, centers):
        f = 2.0 * math.sin(math.pi * min(center, RATE / 6) / RATE)
        low += f * band
        high = x - low - damp * band
        band += f * high
        out.append(band)
    return out


def noise(count, rng):
    return [rng.uniform(-1.0, 1.0) for _ in range(count)]


def brown(count, rng):
    """Deep rumble: integrated noise, kept from drifting"""
    out, y = [], 0.0
    for _ in range(count):
        y = 0.995 * y + rng.uniform(-1.0, 1.0) * 0.05
        out.append(y)
    return out


def add_drip(buffer, at, pitch, loudness, echo):
    """A water drop: a short falling chirp that dies quickly, with a fainter echo"""
    length = int(0.09 * RATE)
    for copy, (delay, gain) in enumerate([(0.0, 1.0)] + ([(echo, 0.35), (echo * 2.1, 0.12)] if echo else [])):
        start = at + int(delay * RATE)
        phase = 0.0
        for i in range(length):
            if start + i >= len(buffer):
                break
            t = i / RATE
            frequency = pitch * (1.0 - 0.45 * min(1.0, t / 0.05))
            phase += 2.0 * math.pi * frequency / RATE
            buffer[start + i] += loudness * gain * math.sin(phase) * math.exp(-t * 55.0)


def loop(samples):
    """Blend the tail (past SECONDS) into the head so the end runs into the start"""
    length = int(SECONDS * RATE)
    fade = int(FADE * RATE)
    out = samples[:length]
    for i in range(fade):
        w = i / fade
        out[i] = out[i] * w + samples[length + i] * (1.0 - w)
    return out


def normalize(samples, peak):
    top = max(1e-9, max(abs(s) for s in samples))
    return [s * peak / top for s in samples]


def crypts(rng):
    count = int((SECONDS + FADE) * RATE)
    rumble = one_pole(brown(count, rng), 120.0)
    out = []
    for i, r in enumerate(rumble):
        t = i / RATE
        swell = 0.75 + 0.25 * math.sin(2.0 * math.pi * t / (SECONDS + FADE) * 2.0)
        hum = 0.06 * math.sin(2.0 * math.pi * 55.0 * t) + 0.04 * math.sin(2.0 * math.pi * 82.4 * t + 1.3)
        out.append(r * 2.2 + hum * swell)
    for _ in range(3):
        add_drip(out, rng.randrange(int(0.5 * RATE), int(SECONDS * RATE)), rng.uniform(1300, 1800), 0.10, 0.18)
    return normalize(loop(out), 0.6)


def caves(rng):
    count = int((SECONDS + FADE) * RATE)
    rumble = one_pole(brown(count, rng), 90.0)
    hiss = band_pass(noise(count, rng), [2400.0] * count, 0.7)
    out = [r * 1.6 + h * 0.012 for r, h in zip(rumble, hiss)]
    for _ in range(11):
        add_drip(out, rng.randrange(0, int(SECONDS * RATE)), rng.uniform(900, 2200), rng.uniform(0.12, 0.3), rng.uniform(0.12, 0.3))
    return normalize(loop(out), 0.6)


def ruins(rng):
    count = int((SECONDS + FADE) * RATE)
    total = SECONDS + FADE
    # Gusts: a slow wandering strength, and the wind's pitch rising with it
    gust = []
    for i in range(count):
        t = i / RATE
        g = 0.55 + 0.25 * math.sin(2.0 * math.pi * t / total * 3.0) + 0.2 * math.sin(2.0 * math.pi * t / total * 7.0 + 0.8)
        gust.append(max(0.1, g))
    centers = [300.0 + 450.0 * g for g in gust]
    wind = band_pass(noise(count, rng), centers, 3.5)
    rumble = one_pole(brown(count, rng), 80.0)
    out = [w * g * 0.6 + r * 0.8 for w, g, r in zip(wind, gust, rumble)]
    return normalize(loop(out), 0.6)


def archive(rng):
    count = int((SECONDS + FADE) * RATE)
    # A hushed hall: soft room tone breathing slowly
    tone = band_pass(noise(count, rng), [420.0] * count, 0.9)
    rumble = one_pole(brown(count, rng), 70.0)
    out = []
    for i, (t_, r) in enumerate(zip(tone, rumble)):
        t = i / RATE
        breath = 0.7 + 0.3 * math.sin(2.0 * math.pi * t / (SECONDS + FADE) * 2.0)
        out.append(t_ * 0.05 * breath + r * 0.9)
    # Pages stirring somewhere: short soft bursts of high noise
    for _ in range(5):
        start = rng.randrange(0, int(SECONDS * RATE))
        length = int(rng.uniform(0.15, 0.35) * RATE)
        rustle = band_pass(noise(length, rng), [3200.0] * length, 1.2)
        for i, x in enumerate(rustle):
            if start + i >= count:
                break
            envelope = math.sin(math.pi * i / length)
            out[start + i] += x * 0.05 * envelope
    # A faint glassy chime now and then (two partials, ringing out)
    for _ in range(3):
        start = rng.randrange(int(0.5 * RATE), int(SECONDS * RATE))
        pitch = rng.choice([660.0, 784.0, 880.0])
        for i in range(int(2.5 * RATE)):
            if start + i >= count:
                break
            t = i / RATE
            out[start + i] += 0.035 * math.exp(-t * 2.2) * (math.sin(2.0 * math.pi * pitch * t) + 0.4 * math.sin(2.0 * math.pi * pitch * 2.76 * t))
    return normalize(loop(out), 0.55)


def write(path, samples):
    with wave.open(str(path), 'wb') as file:
        file.setnchannels(1)
        file.setsampwidth(2)
        file.setframerate(RATE)
        file.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, s)) * 32767)) for s in samples))


if __name__ == '__main__':
    directory = Path(sys.argv[1] if len(sys.argv) > 1 else 'Assets/Sounds')
    for name, make, seed in (('ambience_crypts', crypts, 11), ('ambience_caves', caves, 23), ('ambience_ruins', ruins, 37),
                             ('ambience_archive', archive, 41)):
        write(directory / f'{name}.wav', make(random.Random(seed)))
        print(f'{name}.wav')
