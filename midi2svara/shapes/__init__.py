import numpy as np


def _sample(note_length):
    samples = 512
    length = max(float(note_length), 1e-9)
    times = np.linspace(0.0, length, samples)
    return times, times / length


def _sat(value):
    return 1.0 / (1.0 + np.exp(-np.clip(value, -60.0, 60.0)))


def _ramp(note_length, offset, rate):
    return float(rate) * float(note_length) * offset


def sthira(note_length):
    times, u = _sample(note_length)
    return times, np.zeros_like(u)


def kampita(note_length, top, bottom, rate=4.0):
    times, u = _sample(note_length)
    phase = _ramp(note_length, u, rate)
    wave = (1.0 - np.cos(2.0 * np.pi * phase)) / 2.0
    return times, float(top) + (float(bottom) - float(top)) * wave


def andola(note_length, top, bottom, rate=1.5):
    times, u = _sample(note_length)
    phase = _ramp(note_length, u, rate)
    wave = (1.0 - np.cos(2.0 * np.pi * phase)) / 2.0
    return times, float(top) + (float(bottom) - float(top)) * wave


def vali(note_length, top, bottom, rate=3.0):
    times, u = _sample(note_length)
    phase = _ramp(note_length, u, rate)
    wave = np.abs(2.0 * (phase - np.floor(phase + 0.5)))
    return times, float(top) + (float(bottom) - float(top)) * wave


def sphurita(note_length, dip, at=0.6, rate=40.0):
    times, u = _sample(note_length)
    ramp = float(rate) * float(note_length)
    span = 0.16
    down = _sat(ramp * (u - (float(at) - span))) - _sat(ramp * (u - float(at)))
    up = _sat(ramp * (u - float(at))) - _sat(ramp * (u - (float(at) + span)))
    return times, -float(dip) * down + 0.6 * float(dip) * up


def ahata(note_length, top, at=0.5, width=0.12):
    times, u = _sample(note_length)
    spread = max(float(width), 1e-3)
    return times, float(top) * np.exp(-0.5 * ((u - float(at)) / spread) ** 2)


def khandippu(note_length, bottom, at=0.5, width=0.08):
    times, u = _sample(note_length)
    spread = max(float(width), 1e-3)
    return times, float(bottom) * np.exp(-0.5 * ((u - float(at)) / spread) ** 2)


def nokku(note_length, top, at=0.2, rate=40.0):
    times, u = _sample(note_length)
    wave = _sat(_ramp(note_length, u - float(at), rate))
    return times, float(top) * (1.0 - wave)


def odukkal(note_length, bottom, at=0.2, rate=40.0):
    times, u = _sample(note_length)
    wave = _sat(_ramp(note_length, u - float(at), rate))
    return times, float(bottom) * wave


def janta(note_length, top, at=0.5, rate=40.0):
    times, u = _sample(note_length)
    ramp = float(rate) * float(note_length)
    span = 0.09
    first = (_sat(ramp * (u - (float(at) - 0.18 - span)))
             - _sat(ramp * (u - (float(at) - 0.18 + span))))
    second = (_sat(ramp * (u - (float(at) + 0.18 - span)))
              - _sat(ramp * (u - (float(at) + 0.18 + span))))
    return times, float(top) * (first + second)


def orikai(note_length, top, at=0.75, rate=40.0):
    times, u = _sample(note_length)
    ramp = float(rate) * float(note_length)
    span = 0.07
    flick = (_sat(ramp * (u - (float(at) - span)))
             - _sat(ramp * (u - (float(at) + span))))
    return times, float(top) * flick


def tripuchcha(note_length, dip, at=0.6, rate=40.0):
    times, u = _sample(note_length)
    ramp = float(rate) * float(note_length)
    span = 0.12
    values = np.zeros_like(u)
    for centre in (float(at) - 0.16, float(at) + 0.16):
        down = _sat(ramp * (u - (centre - span))) - _sat(ramp * (u - centre))
        up = _sat(ramp * (u - centre)) - _sat(ramp * (u - (centre + span)))
        values = values - float(dip) * down + 0.6 * float(dip) * up
    return times, values


def jaru(note_length, start, end, at=0.5, rate=6.0):
    times, u = _sample(note_length)
    wave = _sat(_ramp(note_length, u - float(at), rate))
    return times, float(start) + (float(end) - float(start)) * wave


def ravai(note_length, bottom, at=0.5, hold=0.4):
    times, u = _sample(note_length)
    span = max(float(hold), 1e-3)
    edge = 8.0 / span
    centre = float(at)
    curve = (_sat(edge * (u - (centre - span / 2.0)))
             - _sat(edge * (u - (centre + span / 2.0))))
    return times, float(bottom) * curve
