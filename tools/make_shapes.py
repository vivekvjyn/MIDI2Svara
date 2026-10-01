#!/usr/bin/env python3
"""Write the gamaka taxonomy to shapes.pkl.

The taxonomy is a small table of names, shapes, bounds and guesses. It is authored
once and read on every run, so it lives in a pickle next to the package rather than
in the config file, which holds the ragas and the numbers that a run actually tunes.

Run after editing the taxonomy:  python3 tools/make_shapes.py
"""

import os
import pickle
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
sys.path.insert(0, PROJECT)

OUTPUT = os.path.join(PROJECT, "data", "shapes.pkl")

RUNGS = ["both neighbours", "arrival", "departure", "svara alone", "the raga",
         "held svara"]

PREFERENCE = ["sthira", "khandippu", "ahata", "nokku", "odukkal", "kampita", "ravai",
              "sphurita", "orikai", "janta", "jaru", "andola", "vali"]

SHAPES = {
    "held": ["centre"],
    "wave": ["centre", "reach", "cycles", "phase", "decay"],
    "rectified": ["reach", "cycles", "phase", "bias"],
    "settle": ["reach", "at", "rate"],
    "leave": ["reach", "at", "rate"],
    "pulse": ["reach", "at", "width"],
    "plateau": ["reach", "at", "hold"],
}

TYPES = {
    "sthira": {
        "shape": "held",
        "bounds": [(-400.0, 400.0)],
        "guess": [0.0],
        "returns": True,
    },
    "kampita": {
        "shape": "wave",
        "bounds": [(-400.0, 400.0), (0.0, 1.0), (0.4, 14.0),
                   (-3.14, 9.42), (-0.12, 9.0)],
        "minReach": 0.15,
        "perBeat": ["cycles", "decay"],
        "guess": [0.0, 0.5, 2.0, 0.0, 0.0],
        "returns": True,
    },
    "vali": {
        "shape": "wave",
        "bounds": [(-400.0, 400.0), (1.0, 2.4), (1.2, 14.0),
                   (-3.14, 9.42), (-0.12, 9.0)],
        "minReach": 1.0,
        "perBeat": ["cycles", "decay"],
        "guess": [0.0, 2.0, 3.0, 0.0, 0.0],
        "returns": True,
    },
    "andola": {
        "shape": "wave",
        "bounds": [(-400.0, 400.0), (1.0, 2.6), (0.25, 1.2),
                   (-3.14, 9.42), (-0.12, 9.0)],
        "minReach": 1.0,
        "perBeat": ["cycles", "decay"],
        "guess": [0.0, 2.0, 0.8, 0.0, 0.0],
        "returns": True,
    },
    "sphurita": {
        "shape": "rectified",
        "bounds": [(0.0, 2.2), (1.0, 14.0), (-3.14, 9.42), (0.02, 1.0)],
        "minReach": 0.3,
        "perBeat": ["cycles"],
        "guess": [1.0, 3.0, 3.14, 0.5],
        "returns": True,
    },
    "nokku": {
        "shape": "settle",
        "bounds": [(0.3, 2.0), (0.0, 0.55), (4.0, 400.0)],
        "minReach": 0.3,
        "perBeat": ["rate"],
        "guess": [1.0, 0.2, 40.0],
        "returns": True,
    },
    "odukkal": {
        "shape": "settle",
        "bounds": [(0.3, 2.0), (0.0, 0.55), (4.0, 400.0)],
        "minReach": 0.3,
        "perBeat": ["rate"],
        "sign": -1,
        "guess": [1.0, 0.2, 40.0],
        "returns": True,
    },
    "ahata": {
        "shape": "pulse",
        "bounds": [(0.3, 2.0), (0.1, 0.9), (0.03, 0.6)],
        "minReach": 0.3,
        "guess": [1.0, 0.5, 0.25],
        "returns": True,
    },
    "khandippu": {
        "shape": "pulse",
        "bounds": [(0.3, 1.6), (0.0, 0.9), (0.02, 0.3)],
        "minReach": 0.3,
        "sign": -1,
        "guess": [1.0, 0.5, 0.08],
        "returns": True,
    },
    "ravai": {
        "shape": "plateau",
        "bounds": [(0.5, 2.2), (0.1, 0.9), (0.08, 0.85)],
        "minReach": 0.5,
        "sign": -1,
        "guess": [1.0, 0.5, 0.4],
        "returns": True,
    },
    "janta": {
        "shape": "leave",
        "bounds": [(0.3, 1.8), (0.02, 0.55), (4.0, 400.0)],
        "minReach": 0.3,
        "perBeat": ["rate"],
        "guess": [1.0, 0.15, 40.0],
    },
    "orikai": {
        "shape": "leave",
        "bounds": [(0.3, 1.8), (0.55, 0.995), (4.0, 400.0)],
        "minReach": 0.3,
        "perBeat": ["rate"],
        "guess": [1.0, 0.9, 40.0],
    },
    "jaru": {
        "shape": "leave",
        "bounds": [(-2.0, 2.0), (0.15, 0.85), (2.0, 400.0)],
        "minReach": 0.3,
        "perBeat": ["rate"],
        "guess": [1.0, 0.5, 25.0],
    },
}

SVARA_COLOURS = {
    "S": "#2a78d6", "R": "#eb6834", "G": "#1baf7a", "M": "#eda100",
    "P": "#e87ba4", "D": "#008300", "N": "#4a3aa7",
}


def main():
    for name, spec in TYPES.items():
        shape = spec["shape"]
        if len(spec["bounds"]) != len(SHAPES[shape]):
            raise SystemExit(f"{name}: {len(spec['bounds'])} bounds for a "
                             f"{shape} shape, which has {len(SHAPES[shape])}")
        if len(spec["guess"]) != len(spec["bounds"]):
            raise SystemExit(f"{name}: {len(spec['guess'])} guesses for "
                             f"{len(spec['bounds'])} bounds")
        if spec["shape"] != "held" and "reach" not in SHAPES[shape]:
            raise SystemExit(f"{name}: {shape} has no reach to bound")

    missing = [n for n in TYPES if n not in PREFERENCE]
    if missing:
        raise SystemExit(f"not in the preference order: {missing}")
    extra = [n for n in PREFERENCE if n not in TYPES]
    if extra:
        raise SystemExit(f"preference names a type that does not exist: {extra}")

    taxonomy = {
        "rungs": RUNGS,
        "preference": PREFERENCE,
        "shapes": SHAPES,
        "types": TYPES,
        "svaraColours": SVARA_COLOURS,
    }
    with open(OUTPUT, "wb") as f:
        pickle.dump(taxonomy, f, protocol=pickle.HIGHEST_PROTOCOL)

    print(f"wrote {OUTPUT} ({os.path.getsize(OUTPUT)} bytes, {len(TYPES)} types)")


if __name__ == "__main__":
    main()
