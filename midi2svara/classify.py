import json
import os

import numpy as np

from . import scale, shapes

POINTS = 64
BAND = 8

ORDER = ["sthira", "khandippu", "ahata", "nokku", "odukkal", "kampita", "ravai",
         "sphurita", "orikai", "janta", "jaru", "andola", "vali"]


def grids():
    amps = (60.0, 120.0, 200.0)
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
        "sphurita": [{"cycles": c, "amp": a}
                     for c in (3.0, 4.0, 6.0, 8.0)
                     for a in amps],
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
                   for a in amps for t in (0.4, 0.6, 0.8) for r in (15.0, 30.0, 60.0)],
        "jaru": [{"amp": a, "at": t, "rate": r}
                 for a in amps for t in (0.3, 0.5, 0.7) for r in (5.0, 10.0, 20.0)],
        "ravai": [{"amp": a, "at": t, "hold": h}
                  for a in amps for t in (0.4, 0.5, 0.6) for h in (0.2, 0.4, 0.6)],
    }


def render(gamaka, params, beats):
    amp = params.get("amp")
    at = params.get("at")
    if gamaka == "sthira":
        _, y = shapes.sthira(1.0)
    elif gamaka == "kampita":
        _, y = shapes.kampita(1.0, amp, -amp, cycles=params["cycles"] * beats)
    elif gamaka == "andola":
        _, y = shapes.andola(1.0, amp, -amp, cycles=params["cycles"] * beats)
    elif gamaka == "vali":
        _, y = shapes.vali(1.0, amp, -amp, cycles=params["cycles"] * beats)
    elif gamaka == "sphurita":
        _, y = shapes.sphurita(1.0, amp, 0.0, cycles=params["cycles"] * beats)
    elif gamaka == "ahata":
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
    elif gamaka == "jaru":
        _, y = shapes.jaru(1.0, amp, at, rate=params["rate"] * beats)
    else:
        _, y = shapes.ravai(1.0, -amp, at, params["hold"])
    grid = np.linspace(0.0, 1.0, POINTS)
    return np.interp(grid, np.linspace(0.0, 1.0, len(y)), y)


def pool(notes):
    curves = []
    beats = []
    for note in notes:
        pitch = np.asarray(note["pitch"], dtype=float)
        if len(pitch) < 2:
            continue
        relative = pitch - float(note["svarasthana"])
        grid = np.linspace(0.0, 1.0, POINTS)
        curves.append(np.interp(grid, np.linspace(0.0, 1.0, len(relative)), relative))
        beats.append(float(note["durationBeats"]))
    if not curves:
        return None
    return (np.median(np.array(curves), axis=0),
            float(np.median(beats)),
            int(round(np.median([n["previousInterval"] for n in notes]))),
            int(round(np.median([n["nextInterval"] for n in notes]))))


def dtw(target, curves, band):
    count, length = curves.shape
    cost = np.abs(target[None, :, None] - curves[:, None, :])
    dp = np.full((count, length + 1, length + 1), 1e18)
    dp[:, 0, 0] = 0.0
    for i in range(1, length + 1):
        low = max(1, i - band)
        high = min(length, i + band)
        for j in range(low, high + 1):
            dp[:, i, j] = cost[:, i - 1, j - 1] + np.minimum(
                np.minimum(dp[:, i - 1, j], dp[:, i, j - 1]), dp[:, i - 1, j - 1])
    return dp[:, length, length]


def match(curve, beats):
    grid = grids()
    rank = {name: i for i, name in enumerate(ORDER)}
    centred = curve - curve.mean()
    entries = []
    for name in ORDER:
        for params in grid[name]:
            entries.append((name, params))
    templates = []
    for name, params in entries:
        rendered = render(name, params, beats)
        templates.append(rendered - rendered.mean())
    templates = np.array(templates)
    distance = ((templates - centred[None, :]) ** 2).sum(axis=1)
    best_per_class = {}
    index = 0
    for name in ORDER:
        size = len(grid[name])
        best_per_class[name] = distance[index:index + size].min()
        index += size
    shortlist = sorted(ORDER, key=lambda n: (best_per_class[n], rank[n]))[:3]

    candidates = [i for i, (name, _) in enumerate(entries) if name in shortlist]
    warped = dtw(centred, templates[candidates], BAND) / (2.0 * POINTS)
    pick = min(range(len(candidates)),
               key=lambda k: (warped[k], rank[entries[candidates[k]][0]]))
    return entries[candidates[pick]][0]


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

    positions = scale.letters(config, name)
    keys = sorted(groups, key=lambda k: (positions[k[0]], positions[k[1]], positions[k[2]]))
    rows = []
    if advance is not None:
        advance(0, len(keys))
    for done, key in enumerate(keys, 1):
        pooled = pool(groups[key])
        if pooled is not None:
            curve, beats, previous_interval, next_interval = pooled
            gamaka = match(curve, beats)
            here = positions[key[0]]
            bounds = [here, here + previous_interval, here + next_interval]
            syllables = [scale.syllables(config, part) for part in key]
            folder = "_".join(syllables)
            rows.append({
                "svara": key[0],
                "previous": key[1],
                "next": key[2],
                "previousInterval": previous_interval,
                "nextInterval": next_interval,
                "gamaka": gamaka,
                "params": {"top": round(max(bounds), 1),
                           "bottom": round(min(bounds), 1)},
                "plot": f"{name}/{key[0]}/{folder}",
            })
        if advance is not None:
            advance(done, len(keys))

    os.makedirs(output_dir, exist_ok=True)
    with open(path, "w") as handle:
        json.dump(rows, handle, indent=1)
    return rows
