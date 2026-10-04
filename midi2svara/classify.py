import json
import os

import librosa
import numpy as np

from . import scale, shapes


def grids(previous_interval, next_interval):
    amps = (60.0, 120.0, 200.0)
    slides = []
    if previous_interval:
        slides.append((previous_interval, 0))
    if next_interval:
        slides.append((0, next_interval))
    if not slides:
        slides.append((0, 0))
    return {
        "sthira": [{}],
        "kampita": [{"cycles": c, "amp": a}
                    for c in (2.0, 3.0, 4.0, 6.0, 8.0)
                    for a in (20.0, 40.0, 60.0, 80.0, 100.0)],
        "andola": [{"cycles": c, "amp": a}
                   for c in (1.0, 1.5, 2.0)
                   for a in (120.0, 180.0, 250.0)],
        "vali": [{"cycles": c, "amp": a}
                 for c in (2.0, 3.0, 4.0, 5.0)
                 for a in (120.0, 180.0, 250.0)],
        "sphurita": [{"amp": a, "at": t, "rate": r}
                     for a in amps for t in (0.5, 0.6, 0.7)
                     for r in (30.0, 60.0, 100.0)],
        "ahata": [{"amp": a, "at": t, "width": w}
                  for a in amps for t in (0.3, 0.5, 0.7) for w in (0.1, 0.2, 0.3)],
        "khandippu": [{"amp": a, "at": t, "width": w}
                      for a in amps for t in (0.3, 0.5, 0.7) for w in (0.06, 0.1, 0.2)],
        "nokku": [{"amp": a, "at": t, "rate": r}
                  for a in amps for t in (0.15, 0.3, 0.5) for r in (15.0, 30.0, 60.0)],
        "odukkal": [{"amp": a, "at": t, "rate": r}
                    for a in amps for t in (0.15, 0.3, 0.5) for r in (15.0, 30.0, 60.0)],
        "janta": [{"amp": a, "at": t, "rate": r}
                  for a in amps for t in (0.3, 0.5, 0.7) for r in (30.0, 60.0, 100.0)],
        "orikai": [{"amp": a, "at": t, "rate": r}
                   for a in amps for t in (0.65, 0.75, 0.85)
                   for r in (15.0, 30.0, 60.0)],
        "tripuchcha": [{"amp": a, "at": t, "rate": r}
                       for a in amps for t in (0.5, 0.6, 0.7)
                       for r in (30.0, 60.0, 100.0)],
        "jaru": [{"start": s, "end": e, "at": t, "rate": r}
                 for s, e in slides for t in (0.3, 0.5, 0.7)
                 for r in (5.0, 10.0, 20.0)],
        "ravai": [{"amp": a, "at": t, "hold": h}
                  for a in amps for t in (0.4, 0.5, 0.6) for h in (0.2, 0.4, 0.6)],
    }


def render(gamaka, params, beats):
    points = 64
    amp = params.get("amp")
    at = params.get("at")
    if gamaka == "sthira":
        _, y = shapes.sthira(1.0)
    elif gamaka == "kampita":
        _, y = shapes.kampita(1.0, amp, -amp, rate=params["cycles"] * beats)
    elif gamaka == "andola":
        _, y = shapes.andola(1.0, amp, -amp, rate=params["cycles"] * beats)
    elif gamaka == "vali":
        _, y = shapes.vali(1.0, amp, -amp, rate=params["cycles"] * beats)
    elif gamaka == "sphurita":
        _, y = shapes.sphurita(1.0, amp, at, rate=params["rate"] * beats)
    elif gamaka == "ahata":
        _, y = shapes.ahata(1.0, amp, at, params["width"])
    elif gamaka == "pratyahata":
        _, y = shapes.ahata(1.0, amp, at, params["width"])
    elif gamaka == "khandippu":
        _, y = shapes.khandippu(1.0, -amp, at, params["width"])
    elif gamaka == "nokku":
        _, y = shapes.nokku(1.0, amp, at, rate=params["rate"] * beats)
    elif gamaka == "odukkal":
        _, y = shapes.odukkal(1.0, -amp, at, rate=params["rate"] * beats)
    elif gamaka == "janta":
        _, y = shapes.janta(1.0, amp, at, rate=params["rate"] * beats)
    elif gamaka == "orikai":
        _, y = shapes.orikai(1.0, amp, at, rate=params["rate"] * beats)
    elif gamaka == "tripuchcha":
        _, y = shapes.tripuchcha(1.0, amp, at, rate=params["rate"] * beats)
    elif gamaka == "jaru":
        _, y = shapes.jaru(1.0, params["start"], params["end"], at,
                           rate=params["rate"] * beats)
    else:
        _, y = shapes.ravai(1.0, -amp, at, params["hold"])
    grid = np.linspace(0.0, 1.0, points)
    return np.interp(grid, np.linspace(0.0, 1.0, len(y)), y)


def pool(notes):
    points = 64
    curves = []
    beats = []
    for note in notes:
        pitch = np.asarray(note["pitch"], dtype=float)
        if len(pitch) < 2:
            continue
        relative = pitch - float(note["svarasthana"])
        grid = np.linspace(0.0, 1.0, points)
        curves.append(np.interp(grid, np.linspace(0.0, 1.0, len(relative)), relative))
        beats.append(float(note["durationBeats"]))
    if not curves:
        return None
    return (np.median(np.array(curves), axis=0),
            float(np.median(beats)),
            int(round(np.median([n["previousInterval"] for n in notes]))),
            int(round(np.median([n["nextInterval"] for n in notes]))))


def dtw(target, curves, band):
    length = len(target)
    costs = []
    for curve in curves:
        accumulated = librosa.sequence.dtw(
            X=target[None, :], Y=curve[None, :],
            band_rad=band / float(length), backtrack=False)
        costs.append(accumulated[-1, -1])
    return np.asarray(costs)


def match(curve, beats, previous_interval, next_interval):
    points = 64
    band = 8
    order = ["sthira", "khandippu", "ahata", "nokku", "odukkal", "kampita",
             "ravai", "sphurita", "orikai", "janta", "tripuchcha", "jaru",
             "andola", "vali"]
    grid = grids(previous_interval, next_interval)
    rank = {name: i for i, name in enumerate(order)}
    centred = curve - curve.mean()
    entries = []
    for name in order:
        for params in grid[name]:
            entries.append((name, params))
    templates = []
    for name, params in entries:
        rendered = render(name, params, beats)
        templates.append(rendered - rendered.mean())
    templates = np.array(templates)
    warped = dtw(centred, templates, band) / (2.0 * points)
    pick = min(range(len(entries)),
               key=lambda k: (warped[k], rank[entries[k][0]]))
    gamaka = entries[pick][0]
    if gamaka == "ahata" and next_interval < 0:
        return "pratyahata"
    return gamaka


def raga(config, name, notes, output_dir, force, advance=None):
    path = os.path.join(output_dir, f"{name}_lookup.json")
    if os.path.exists(path) and not force:
        with open(path) as handle:
            rows = json.load(handle)
        if advance is not None:
            advance(len(rows), max(len(rows), 1))
        return rows

    groups = {}
    for note in notes:
        if note["previous"] is None or note["next"] is None:
            continue
        key = (note["svara"], note["previous"], note["next"])
        groups.setdefault(key, []).append(note)

    positions = scale.positions(config, name)
    keys = sorted(groups, key=lambda k: (positions[k[0]], positions[k[1]], positions[k[2]]))
    rows = []
    if advance is not None:
        advance(0, len(keys))
    for done, key in enumerate(keys, 1):
        pooled = pool(groups[key])
        if pooled is not None:
            curve, beats, previous_interval, next_interval = pooled
            gamaka = match(curve, beats, previous_interval, next_interval)
            here = positions[key[0]]
            rows.append({
                "svara": key[0],
                "previous": key[1],
                "next": key[2],
                "previousInterval": previous_interval,
                "nextInterval": next_interval,
                "gamaka": gamaka,
                "params": {"top": round(here + float(np.max(curve)), 1),
                           "bottom": round(here + float(np.min(curve)), 1)},
            })
        if advance is not None:
            advance(done, len(keys))

    os.makedirs(output_dir, exist_ok=True)
    with open(path, "w") as handle:
        json.dump(rows, handle, indent=1)
    return rows
