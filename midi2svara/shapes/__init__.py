import numpy as np

SAMPLES = 512
RATE = 40.0
EDGE = 8.0


def _sample(note_length):
    length = max(float(note_length), 1e-9)
    times = np.linspace(0.0, length, SAMPLES)
    return times, times / length


def _sat(value):
    return 1.0 / (1.0 + np.exp(-np.clip(value, -60.0, 60.0)))


def _swing(note_length, top, bottom, cycles):
    times, u = _sample(note_length)
    wave = (1.0 - np.cos(2.0 * np.pi * float(cycles) * u)) / 2.0
    return times, float(top) + (float(bottom) - float(top)) * wave


def sthira(note_length):
    times, u = _sample(note_length)
    return times, np.zeros_like(u)


def kampita(note_length, top, bottom, cycles=2.0):
    return _swing(note_length, top, bottom, cycles)


def andola(note_length, top, bottom, cycles=2.0):
    return _swing(note_length, top, bottom, cycles)


def vali(note_length, top, bottom, cycles=3.0):
    return _swing(note_length, top, bottom, cycles)


def sphurita(note_length, top, bottom, cycles=3.0):
    return _swing(note_length, top, bottom, cycles)


def ahata(note_length, top, at=0.5, width=0.25):
    times, u = _sample(note_length)
    spread = max(float(width), 1e-3)
    return times, float(top) * np.exp(-0.5 * ((u - float(at)) / spread) ** 2)


def khandippu(note_length, bottom, at=0.5, width=0.08):
    times, u = _sample(note_length)
    spread = max(float(width), 1e-3)
    return times, float(bottom) * np.exp(-0.5 * ((u - float(at)) / spread) ** 2)


def nokku(note_length, top, at=0.2, rate=RATE):
    times, u = _sample(note_length)
    return times, float(top) * (1.0 - _sat(float(rate) * (u - float(at))))


def odukkal(note_length, bottom, at=0.2, rate=RATE):
    times, u = _sample(note_length)
    return times, float(bottom) * (1.0 - _sat(float(rate) * (u - float(at))))


def janta(note_length, top, at=0.5, rate=RATE):
    times, u = _sample(note_length)
    return times, float(top) * _sat(float(rate) * (u - float(at)))


def orikai(note_length, top, at=0.5, rate=RATE):
    times, u = _sample(note_length)
    return times, float(top) * _sat(float(rate) * (u - float(at)))


def jaru(note_length, top, at=0.5, rate=25.0):
    times, u = _sample(note_length)
    return times, float(top) * _sat(float(rate) * (u - float(at)))


def ravai(note_length, bottom, at=0.5, hold=0.4):
    times, u = _sample(note_length)
    span = max(float(hold), 1e-3)
    edge = EDGE / span
    centre = float(at)
    curve = (_sat(edge * (u - (centre - span / 2.0)))
             - _sat(edge * (u - (centre + span / 2.0))))
    return times, float(bottom) * curve
