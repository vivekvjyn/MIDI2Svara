import numpy as np


def ragas(config):
    return sorted(config["ragas"])


def svarasthanas(config, raga):
    """Svara letter -> its equal-tempered position in madhya sthayi, in cents."""
    entry = config["ragas"][raga]
    return {name: cents for name, cents in zip(entry["svaras"], entry["svarasthanas"])
            if len(name) == config["generator"]["singleLetter"]}


def scale(config, raga):
    octave = config["generator"]["octave"]
    return sorted({cents % octave
                   for cents in config["ragas"][raga]["svarasthanas"]})


def colour(config, raga):
    return config["ragas"][raga]["color"]


def parse_annotation(config, raga, annotation):
    """Split an annotation into (svara letter, explicit sthayi shift in cents).

    The shift is None when the annotation carries no register sigil. Annotations
    spell the sthayi with a trailing sigil rather than the "S-" form the config uses.
    """
    annotation = str(annotation).strip()
    if not annotation:
        return None

    letter = annotation[0]
    if letter not in svarasthanas(config, raga):
        return None

    octave = config["generator"]["octave"]
    shift = None
    for mark, sign in config["generator"]["sthayiMarks"].items():
        if mark in annotation:
            shift = sign * octave
            break

    return letter, shift


def resolve_svara(config, raga, annotation, pitch=None):
    """Absolute svarasthana of an annotated svara, in cents from the tonic.

    Trusts the sthayi sigil when the annotator supplied one, and otherwise falls back
    to the sthayi whose position is closest to the note's median pitch - median rather
    than mean, because gamaka excursions are asymmetric and drag the mean off the
    svara.
    """
    parsed = parse_annotation(config, raga, annotation)
    if parsed is None:
        return None

    letter, shift = parsed
    position = svarasthanas(config, raga)[letter]

    if shift is not None:
        return position + shift

    if pitch is None or len(pitch) == 0 or np.isnan(pitch).all():
        return position

    octave = config["generator"]["octave"]
    span = config["generator"]["sthayiSpan"]
    centre = float(np.nanmedian(pitch))
    candidates = [position + octave * k for k in range(-span, span + 1)]
    return min(candidates, key=lambda cents: abs(cents - centre))


def fold_to_octave(config, cents):
    return int(round(cents)) % config["generator"]["octave"]
