import numpy as np


def ragas(config):
    return sorted(config["ragas"])


def letters(config, name):
    entry = config["ragas"][name]
    single = config["settings"]["singleLetter"]
    return {letter: float(cents)
            for letter, cents in zip(entry["svaras"], entry["svarasthanas"])
            if len(letter) == single}


def parse(config, name, annotation):
    annotation = str(annotation).strip()
    if not annotation:
        return None
    letter = annotation[0]
    if letter not in letters(config, name):
        return None
    octave = config["settings"]["octave"]
    shift = None
    for mark, sign in config["settings"]["sthayiMarks"].items():
        if mark in annotation:
            shift = sign * octave
            break
    return letter, shift


def marks(config):
    below = above = ""
    for mark, sign in config["settings"]["sthayiMarks"].items():
        if sign < 0:
            below = mark
        else:
            above = mark
    return below, above


def positions(config, name):
    base = letters(config, name)
    octave = config["settings"]["octave"]
    span = config["settings"]["sthayiSpan"]
    below, above = marks(config)
    out = dict(base)
    for letter, cents in base.items():
        for k in range(1, span + 1):
            out[letter + above * k] = cents + octave * k
            out[letter + below * k] = cents - octave * k
    return out


def qualified(config, letter, offset):
    below, above = marks(config)
    if offset > 0:
        return letter + above * int(offset)
    if offset < 0:
        return letter + below * int(-offset)
    return letter


def position(config, name, svara):
    return positions(config, name)[svara]


def resolve(config, name, annotation, pitch=None):
    parsed = parse(config, name, annotation)
    if parsed is None:
        return None, None
    letter, shift = parsed
    base = letters(config, name)[letter]
    octave = config["settings"]["octave"]
    if shift is None:
        if pitch is None or len(pitch) == 0 or np.isnan(pitch).all():
            offset = 0
        else:
            span = config["settings"]["sthayiSpan"]
            centre = float(np.nanmedian(pitch))
            candidates = [base + octave * k for k in range(-span, span + 1)]
            best = min(candidates, key=lambda cents: abs(cents - centre))
            offset = int(round((best - base) / octave))
    else:
        offset = int(round(shift / octave))
    return qualified(config, letter, offset), base + offset * octave


def fold(config, cents):
    return int(round(cents)) % int(config["settings"]["octave"])


def syllables(config, letter):
    return config["names"].get(letter, letter)
