# MakeVoices.py - Synthesizes the creatures' voices (Assets/Sounds/voice_*.wav): for each voice
# three short hurt sounds and a death (a beast's snarl, a brute's grunt, a shriek, a giant's roar).
# A buzzing source (a sawtooth with a little noise) through a voice's formants, its pitch sliding.
# Made for LegendsBelow and released as CC0. Plain Python (no packages); deterministic.
#
#   python Scripts/MakeVoices.py [output directory, default Assets/Sounds]

import math
import random
import struct
import sys
import wave
from pathlib import Path

RATE = 22050


def band_pass(samples, center, q):
    """A resonant band-pass (state-variable filter)"""
    out, low, band = [], 0.0, 0.0
    damp = 1.0 / q
    f = 2.0 * math.sin(math.pi * min(center, RATE / 6) / RATE)
    for x in samples:
        low += f * band
        high = x - low - damp * band
        band += f * high
        out.append(band)
    return out


def voice(rng, seconds, pitch_from, pitch_to, formants, noise, rasp, attack=0.02):
    """A sliding buzz through formants: (center, q, gain) each"""
    count = int(seconds * RATE)
    source, phase = [], 0.0
    for i in range(count):
        t = i / count
        pitch = pitch_from + (pitch_to - pitch_from) * t
        pitch *= 1.0 + rasp * (rng.random() - 0.5)            # a rough, uneven throat
        phase = (phase + pitch / RATE) % 1.0
        saw = 2.0 * phase - 1.0
        source.append(saw * (1.0 - noise) + rng.uniform(-1.0, 1.0) * noise)
    out = [0.0] * count
    for center, q, gain in formants:
        for i, x in enumerate(band_pass(source, center, q)):
            out[i] += x * gain
    for i in range(count):                                       # swell in, die away
        t = i / RATE
        envelope = min(1.0, t / attack) * math.exp(-3.0 * max(0.0, t - seconds * 0.35) / seconds)
        out[i] *= envelope
    return out


def normalize(samples, peak):
    top = max(1e-9, max(abs(s) for s in samples))
    return [s * peak / top for s in samples]


# Each voice: how it sounds hurt (short) and dying (long), from its seed
VOICES = {
    'beast': lambda rng, death: voice(rng, 0.9 if death else 0.28, 230 if death else 180, 110 if death else 150,
                                      [(600, 3.0, 1.0), (1300, 4.0, 0.6), (2600, 5.0, 0.25)], 0.35, 0.25),
    'brute': lambda rng, death: voice(rng, 0.8 if death else 0.22, 120 if death else 105, 70 if death else 95,
                                      [(450, 3.5, 1.0), (900, 4.0, 0.5), (2300, 6.0, 0.15)], 0.15, 0.12),
    'shriek': lambda rng, death: voice(rng, 0.8 if death else 0.25, 700 if death else 620, 380 if death else 820,
                                       [(1400, 4.0, 1.0), (2800, 5.0, 0.5), (900, 3.0, 0.3)], 0.2, 0.06),
    'giant': lambda rng, death: voice(rng, 1.4 if death else 0.4, 75 if death else 70, 40 if death else 60,
                                      [(300, 3.0, 1.0), (700, 3.5, 0.6), (1600, 5.0, 0.2)], 0.4, 0.2, attack=0.05),
}


def write(path, samples):
    with wave.open(str(path), 'wb') as file:
        file.setnchannels(1)
        file.setsampwidth(2)
        file.setframerate(RATE)
        file.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, s)) * 32767)) for s in samples))


if __name__ == '__main__':
    directory = Path(sys.argv[1] if len(sys.argv) > 1 else 'Assets/Sounds')
    for seed, (name, make) in enumerate(VOICES.items(), start=101):
        rng = random.Random(seed)
        for i in range(3):
            write(directory / f'voice_{name}_hurt_{i}.wav', normalize(make(rng, False), 0.7))
        write(directory / f'voice_{name}_death.wav', normalize(make(rng, True), 0.75))
        print(f'voice_{name}_*.wav')
