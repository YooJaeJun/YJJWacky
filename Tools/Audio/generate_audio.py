"""YJJWacky 효과음·배경음 합성기. 외부 음원·라이브러리 없이 직접 만든다 (상업적 사용 자유).

실행 (엔진 Python):
    Engine/Binaries/ThirdParty/Python3/Win64/python.exe -I Tools/Audio/generate_audio.py Tools/Audio/Generated
결과: <출력 폴더>/*.wav (32kHz 16bit 모노). 임포트는 Tools/Content/build_content.py.

분위기: 만화 효과음. 뿅, 띠용, 퍽, 풍덩 + 뒤뚱거리는 움파 움파 배경음.
"""

import math
import os
import random
import struct
import sys
import wave

RATE = 32000
random.seed(20261116)
TAU = 2.0 * math.pi


# ---------------------------------------------------------------- 기본 재료

def mix(dst, src, start=0.0, gain=1.0):
    """dst에 src를 start초 위치부터 더한다 (필요하면 늘린다)"""
    offset = int(start * RATE)
    need = offset + len(src)
    if need > len(dst):
        dst.extend([0.0] * (need - len(dst)))
    for i, s in enumerate(src):
        dst[offset + i] += s * gain
    return dst


def note(name):
    names = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}
    pitch, octave = name[:-1], int(name[-1])
    return 440.0 * 2.0 ** ((names[pitch] + (octave - 4) * 12 - 9) / 12.0)


def tone(freq, length, volume=0.4, harmonics=((1, 1.0), (2, 0.4), (3, 0.2)), attack=0.01, decay=0.3, vibrato=0.0):
    """배음 몇 개를 얹은 음. decay가 length보다 길면 거의 일정하게 울린다"""
    count = int(length * RATE)
    out = [0.0] * count
    phases = [0.0] * len(harmonics)
    a = max(1, int(attack * RATE))
    release = max(1, int(0.02 * RATE))
    for i in range(count):
        t = i / RATE
        f = freq * (1.0 + vibrato * math.sin(TAU * 6.0 * t))
        s = 0.0
        for k, (ratio, amp) in enumerate(harmonics):
            phases[k] += TAU * f * ratio / RATE
            s += math.sin(phases[k]) * amp
        env = min(i / a, 1.0) * math.exp(-t / decay) * min((count - i) / release, 1.0)
        out[i] = s * env * volume
    return out


def bell(freq, length=0.8, volume=0.4, decay=0.3):
    return tone(freq, length, volume, ((1, 1.0), (2, 0.5), (2.76, 0.3), (5.4, 0.15)), 0.003, decay)


def noise(length, volume=0.5):
    return [random.uniform(-1.0, 1.0) * volume for _ in range(int(length * RATE))]


def lowpass(samples, cutoff):
    """한 극 저역 통과. cutoff는 숫자 또는 (i -> Hz) 함수"""
    out = []
    y = 0.0
    for i, x in enumerate(samples):
        c = cutoff(i) if callable(cutoff) else cutoff
        a = 1.0 - math.exp(-TAU * c / RATE)
        y += a * (x - y)
        out.append(y)
    return out


def bandpass(samples, low, high):
    lo = lowpass(samples, low)
    hi = lowpass(samples, high)
    return [h - l for h, l in zip(hi, lo)]


def envelope(samples, attack, decay):
    a = max(1, int(attack * RATE))
    out = []
    for i, s in enumerate(samples):
        t = i / RATE
        out.append(s * min(i / a, 1.0) * math.exp(-max(0.0, t - attack) / decay))
    return out


def sweep(f0, f1, length, volume=0.5, curve=1.0, wobble=0.0, wobble_rate=18.0):
    count = int(length * RATE)
    out = []
    phase = 0.0
    for i in range(count):
        a = (i / max(count - 1, 1)) ** curve
        f = f0 * (f1 / f0) ** a
        if wobble:
            f *= 1.0 + wobble * math.sin(TAU * wobble_rate * i / RATE)
        phase += TAU * f / RATE
        out.append(math.sin(phase) * volume)
    return out


def reverb(samples, wet=0.3, room=0.8, tail=0.8):
    """간단한 Schroeder 잔향: 콤 필터 4개"""
    src = samples + [0.0] * int(tail * RATE)
    acc = [0.0] * len(src)
    for delay in (1557, 1617, 1491, 1422):
        d = int(delay * RATE / 44100)
        buf = [0.0] * d
        idx = 0
        lp = 0.0
        for i, x in enumerate(src):
            y = buf[idx]
            lp = y * 0.7 + lp * 0.3
            buf[idx] = x + lp * room
            idx = (idx + 1) % d
            acc[i] += y
    return [s + a * wet * 0.25 for s, a in zip(src, acc)]


def normalize(samples, peak=0.85):
    m = max(1e-6, max(abs(s) for s in samples))
    return [s * peak / m for s in samples]


def fade_out(samples, seconds):
    n = int(seconds * RATE)
    count = len(samples)
    for i in range(max(0, count - n), count):
        samples[i] *= (count - i) / n
    return samples


def write(path, samples):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        frames = bytearray()
        for s in samples:
            v = max(-1.0, min(1.0, s))
            frames += struct.pack("<h", int(v * 32767))
        w.writeframes(bytes(frames))


# ---------------------------------------------------------------- 효과음

def sfx_punch():
    """뻥! 묵직한 타격 + 스프링 띠용"""
    out = envelope(sweep(190, 55, 0.2, 0.9, 0.6), 0.002, 0.07)
    mix(out, envelope(lowpass(noise(0.12, 0.8), 1800), 0.001, 0.035))
    mix(out, envelope(sweep(260, 520, 0.4, 0.3, 0.5, 0.08, 26.0), 0.01, 0.16), 0.05)
    return normalize(fade_out(out, 0.05))


def sfx_cannon():
    out = envelope(lowpass(noise(0.5, 1.0), lambda i: 2400 * math.exp(-i / (0.05 * RATE)) + 180), 0.002, 0.14)
    mix(out, envelope(sweep(130, 38, 0.45, 0.9, 0.5), 0.002, 0.16))
    return normalize(fade_out(out, 0.08), 0.8)


def sfx_boom():
    out = envelope(lowpass(noise(0.9, 1.0), lambda i: 3000 * math.exp(-i / (0.09 * RATE)) + 140), 0.002, 0.26)
    mix(out, envelope(sweep(95, 30, 0.7, 1.0, 0.5), 0.002, 0.25))
    return normalize(fade_out(reverb(out, 0.25, 0.75, 0.5), 0.2), 0.85)


def sfx_bonk():
    """나무 몽둥이 딱!"""
    out = envelope(sweep(760, 540, 0.14, 0.8, 0.5), 0.001, 0.035)
    mix(out, envelope(sweep(1520, 1100, 0.08, 0.3, 0.5), 0.001, 0.02))
    mix(out, envelope(bandpass(noise(0.03, 0.6), 900, 5000), 0.001, 0.008))
    return normalize(fade_out(out, 0.03))


def sfx_whoosh():
    src = noise(0.7, 1.0)
    out = bandpass(src, 300, lambda i: 600 + 2600 * math.sin(math.pi * i / len(src)))
    count = len(out)
    out = [s * math.sin(math.pi * i / count) ** 2 for i, s in enumerate(out)]
    return normalize(out, 0.6)


def sfx_splash():
    """풍덩: 물 튀는 소리 + 뽀글"""
    out = envelope(bandpass(noise(0.7, 1.0), 500, 5200), 0.008, 0.16)
    mix(out, envelope(sweep(170, 70, 0.22, 0.7, 0.6), 0.004, 0.08))
    for k in range(3):
        mix(out, envelope(sweep(320 + k * 130, 820 + k * 200, 0.07, 0.25), 0.004, 0.03), 0.2 + k * 0.09)
    return normalize(fade_out(out, 0.1), 0.8)


def sfx_pop():
    """뿅 + 꽥"""
    out = envelope(sweep(420, 1300, 0.06, 0.8), 0.002, 0.03)
    mix(out, envelope(sweep(980, 520, 0.2, 0.4, 0.7, 0.05, 30.0), 0.005, 0.07), 0.04)
    return normalize(fade_out(out, 0.03), 0.75)


def sfx_build():
    out = envelope(sweep(160, 70, 0.14, 0.8), 0.002, 0.05)
    mix(out, bell(note("E5"), 0.35, 0.4, 0.14), 0.04)
    mix(out, bell(note("A5"), 0.45, 0.4, 0.18), 0.13)
    return normalize(out, 0.8)


def sfx_sell():
    out = bell(note("A5"), 0.3, 0.4, 0.1)
    mix(out, bell(note("E5"), 0.3, 0.4, 0.12), 0.09)
    mix(out, bell(note("B5"), 0.5, 0.35, 0.2), 0.2)
    return normalize(out, 0.75)


def sfx_coin():
    out = bell(1568, 0.2, 0.4, 0.07)
    mix(out, bell(2093, 0.35, 0.4, 0.12), 0.055)
    return normalize(out, 0.6)


def brass(freq, length, volume=0.4):
    return tone(freq, length, volume, ((1, 1.0), (2, 0.7), (3, 0.5), (4, 0.3), (5, 0.18)), 0.03, 2.0, 0.006)


def sfx_wave_start():
    """빰빰빰 빠~"""
    out = []
    for i, (n, d) in enumerate((("G4", 0.13), ("G4", 0.13), ("G4", 0.13), ("C5", 0.55))):
        mix(out, brass(note(n), d, 0.5), i * 0.16)
    return normalize(fade_out(reverb(out, 0.2, 0.7, 0.4), 0.2), 0.75)


def sfx_wave_clear():
    out = []
    for i, n in enumerate(("C5", "E5", "G5", "C6")):
        mix(out, bell(note(n), 0.7, 0.4, 0.25), i * 0.09)
    mix(out, brass(note("C4"), 0.5, 0.25), 0.27)
    return normalize(fade_out(reverb(out, 0.25, 0.75, 0.5), 0.2), 0.75)


def sfx_chomp():
    """암냠: 푸딩 뜯기는 소리"""
    out = []
    for k in range(2):
        bite = envelope(lowpass(noise(0.1, 1.0), 1400 - k * 300), 0.004, 0.03)
        mix(out, bite, k * 0.13)
        mix(out, envelope(sweep(300 - k * 40, 120, 0.11, 0.7), 0.004, 0.04), k * 0.13)
    mix(out, envelope(sweep(90, 60, 0.3, 0.6, 1.0, 0.1, 9.0), 0.01, 0.12), 0.2)
    return normalize(fade_out(out, 0.05), 0.8)


def sfx_combo():
    out = envelope(sweep(300, 1500, 0.35, 0.4, 1.4), 0.02, 0.4)
    for i, n in enumerate(("C5", "E5", "G5", "B5", "D6")):
        mix(out, bell(note(n), 0.9, 0.3, 0.35), 0.28 + i * 0.03)
    return normalize(fade_out(reverb(out, 0.3, 0.8, 0.7), 0.3), 0.75)


def sfx_win():
    out = []
    melody = (("C5", 0.0, 0.16), ("C5", 0.18, 0.16), ("C5", 0.36, 0.16), ("E5", 0.54, 0.3), ("D5", 0.86, 0.16), ("E5", 1.04, 0.16), ("G5", 1.22, 0.9))
    for n, t, d in melody:
        mix(out, brass(note(n), d, 0.45), t)
    for n in ("C4", "E4", "G4"):
        mix(out, brass(note(n), 0.9, 0.2), 1.22)
    return normalize(fade_out(reverb(out, 0.25, 0.78, 0.8), 0.4), 0.8)


def sfx_lose():
    """와 와 와 와~~ (슬픈 트롬본)"""
    out = []
    for i, (n, d) in enumerate((("A#3", 0.36), ("A3", 0.36), ("G#3", 0.36), ("G3", 1.1))):
        f = note(n)
        part = sweep(f * 1.02, f * (0.94 if i == 3 else 0.985), d, 0.5, 1.0, 0.012 if i == 3 else 0.0, 5.5)
        part2 = sweep(f * 2.04, f * (1.88 if i == 3 else 1.97), d, 0.25)
        part = envelope([a + b for a, b in zip(part, part2)], 0.03, 1.5)
        mix(out, fade_out(part, 0.06), i * 0.4)
    return normalize(fade_out(reverb(out, 0.2, 0.7, 0.5), 0.3), 0.75)


def sfx_deny():
    out = []
    for k in range(2):
        mix(out, tone(150, 0.1, 0.5, ((1, 1.0), (3, 0.5), (5, 0.3)), 0.004, 1.0), k * 0.14)
    return normalize(out, 0.6)


def sfx_fan_on():
    src = noise(0.9, 1.0)
    count = len(src)
    out = bandpass(src, 200, lambda i: 400 + 1800 * (i / count))
    out = [s * min(1.0, i / (0.4 * RATE)) * min(1.0, (count - i) / (0.3 * RATE)) for i, s in enumerate(out)]
    mix(out, sweep(60, 150, 0.9, 0.25, 0.6))
    return normalize(fade_out(out, 0.3), 0.5)


# ---------------------------------------------------------------- 배경음

def bgm():
    """움파 움파 뒤뚱거리는 행진곡. 16마디 반복 (딱 떨어지게 끝나서 이음매 없이 돈다)"""
    bpm = 132.0
    beat = 60.0 / bpm
    bars = 16
    total = bars * 4 * beat
    out = [0.0] * int(total * RATE)

    def tuba(n, t, d=0.9):
        mix(out, tone(note(n), beat * d, 0.34, ((1, 1.0), (2, 0.5), (3, 0.22), (4, 0.1)), 0.012, 0.35), t)

    def pah(names, t):
        for n in names:
            mix(out, tone(note(n), beat * 0.32, 0.1, ((1, 1.0), (2, 0.35), (3, 0.3)), 0.005, 0.09), t)

    def kazoo(n, t, d):
        mix(out, tone(note(n), beat * d * 0.92, 0.2, ((1, 1.0), (2, 0.15), (3, 0.5), (5, 0.25), (7, 0.1)), 0.02, 1.2, 0.01), t)

    def block(t, high=False):
        mix(out, envelope(sweep(1500 if high else 1000, 1200 if high else 800, 0.05, 0.16), 0.001, 0.014), t)

    # 화음 진행: C C F G / C Am F G (두 번)
    chords = [("C2", "G2", ("E4", "G4", "C5")), ("C2", "G2", ("E4", "G4", "C5")), ("F2", "C3", ("F4", "A4", "C5")), ("G2", "D3", ("G4", "B4", "D5")),
              ("C2", "G2", ("E4", "G4", "C5")), ("A2", "E3", ("E4", "A4", "C5")), ("F2", "C3", ("F4", "A4", "C5")), ("G2", "D3", ("G4", "B4", "D5"))]
    for bar in range(bars):
        root, fifth, triad = chords[bar % 8]
        t0 = bar * 4 * beat
        tuba(root, t0)
        pah(triad, t0 + beat)
        tuba(fifth, t0 + 2 * beat)
        pah(triad, t0 + 3 * beat)
        block(t0 + beat * 1.5)
        block(t0 + beat * 3.5, True)

    # 멜로디 (음, 시작 박, 길이 박). 8마디짜리를 두 번, 두 번째는 끝만 바꾼다
    tune_a = [("E5", 0, 0.5), ("G5", 0.5, 0.5), ("C6", 1, 1), ("G5", 2, 0.5), ("E5", 2.5, 0.5), ("G5", 3, 1),
              ("E5", 4, 0.5), ("G5", 4.5, 0.5), ("C6", 5, 0.5), ("B5", 5.5, 0.5), ("A5", 6, 1), ("G5", 7, 1),
              ("A5", 8, 0.5), ("C6", 8.5, 0.5), ("F5", 9, 1), ("A5", 10, 0.5), ("C6", 10.5, 0.5), ("A5", 11, 1),
              ("B5", 12, 0.5), ("D6", 12.5, 0.5), ("G5", 13, 1), ("B5", 14, 0.5), ("A5", 14.5, 0.5), ("G5", 15, 1),
              ("E5", 16, 0.5), ("G5", 16.5, 0.5), ("C6", 17, 1), ("G5", 18, 0.5), ("E5", 18.5, 0.5), ("G5", 19, 1),
              ("C6", 20, 0.5), ("B5", 20.5, 0.5), ("A5", 21, 0.5), ("G5", 21.5, 0.5), ("A5", 22, 1), ("E5", 23, 1),
              ("F5", 24, 0.5), ("A5", 24.5, 0.5), ("C6", 25, 0.5), ("A5", 25.5, 0.5), ("F5", 26, 1), ("A5", 27, 1),
              ("G5", 28, 0.5), ("B5", 28.5, 0.5), ("D6", 29, 0.5), ("B5", 29.5, 0.5), ("G5", 30, 1.5)]
    for n, b, d in tune_a:
        kazoo(n, b * beat, d)
    tune_b = [(n, b + 32, d) for n, b, d in tune_a[:-5]] + [("G5", 60, 0.5), ("A5", 60.5, 0.5), ("B5", 61, 0.5), ("D6", 61.5, 0.5), ("C6", 62, 1.4)]
    for n, b, d in tune_b:
        kazoo(n, b * beat, d)

    out = out[:int(total * RATE)]
    return normalize(out, 0.7)


SOUNDS = {
    "SFX_Punch": sfx_punch,
    "SFX_Cannon": sfx_cannon,
    "SFX_Boom": sfx_boom,
    "SFX_Bonk": sfx_bonk,
    "SFX_Whoosh": sfx_whoosh,
    "SFX_Splash": sfx_splash,
    "SFX_Pop": sfx_pop,
    "SFX_Build": sfx_build,
    "SFX_Sell": sfx_sell,
    "SFX_Coin": sfx_coin,
    "SFX_WaveStart": sfx_wave_start,
    "SFX_WaveClear": sfx_wave_clear,
    "SFX_Chomp": sfx_chomp,
    "SFX_Combo": sfx_combo,
    "SFX_Win": sfx_win,
    "SFX_Lose": sfx_lose,
    "SFX_Deny": sfx_deny,
    "SFX_FanOn": sfx_fan_on,
    "BGM_Wacky": bgm,
}


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "Generated")
    os.makedirs(out_dir, exist_ok=True)
    only = set(sys.argv[2:])
    for name, fn in SOUNDS.items():
        if only and name not in only:
            continue
        path = os.path.join(out_dir, name + ".wav")
        write(path, fn())
        print("[Audio]", name, os.path.getsize(path) // 1024, "KB", flush=True)


if __name__ == "__main__":
    main()
