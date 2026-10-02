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


def resolve(config, name, annotation, pitch=None):
    parsed = parse(config, name, annotation)
    if parsed is None:
        return None, None
    letter, shift = parsed
    position = letters(config, name)[letter]
    if shift is not None:
        return letter, position + shift
    if pitch is None or len(pitch) == 0 or np.isnan(pitch).all():
        return letter, position
    octave = config["settings"]["octave"]
    span = config["settings"]["sthayiSpan"]
    centre = float(np.nanmedian(pitch))
    candidates = [position + octave * k for k in range(-span, span + 1)]
    return letter, min(candidates, key=lambda cents: abs(cents - centre))


def fold(config, cents):
    return int(round(cents)) % int(config["settings"]["octave"])


def syllables(config, letter):
    return config["names"].get(letter, letter)


def marks(config):
    below = above = ""
    for mark, sign in config["settings"]["sthayiMarks"].items():
        if sign < 0:
            below = mark
        else:
            above = mark
    return below, above
