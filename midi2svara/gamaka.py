import math
import multiprocessing
import os

import numpy as np
from scipy.optimize import least_squares

from . import raga


def _sat(x, limit):
    return 1.0 / (1.0 + np.exp(-np.clip(x, -limit, limit)))


def _circular(a, b, octave):
    d = (float(a) - float(b)) % octave
    return d - octave if d > octave / 2 else d


class Scale:
    def __init__(self, config, name, offsets):
        self.config = config
        self.set = config["generator"]
        self.octave = self.set["octave"]
        self.name = name
        self.letters = list(raga.svarasthanas(config, name))
        self.base = np.array([raga.svarasthanas(config, name)[letter]
                              + float(offsets.get(letter, 0.0))
                              for letter in self.letters], dtype=float)
        self.period = len(self.base)

    def degree(self, svarasthana):
        folded = float(svarasthana) % self.octave
        return int(np.argmin([abs(_circular(folded, b, self.octave)) for b in self.base]))

    def position(self, svarasthana, step):
        octave, index = divmod(int(np.floor(self.degree(svarasthana) + float(step))),
                               self.period)
        return float(self.base[index] + self.octave * octave)

    def cents(self, svarasthana, steps):
        total = self.degree(svarasthana) + np.asarray(steps, dtype=float)
        lower = np.floor(total)
        fraction = total - lower
        below = self.base[np.mod(lower, self.period).astype(int)] \
            + self.octave * np.floor(lower / self.period)
        above = self.base[np.mod(lower + 1.0, self.period).astype(int)] \
            + self.octave * np.floor((lower + 1.0) / self.period)
        return below + fraction * (above - below) - float(svarasthana)

    def svarasthanas(self):
        return {letter: self.position(raga.svarasthanas(self.config, self.name)[letter], 0)
                for letter in self.letters}

    def reframe(self, svarasthana, pitch_offset):
        return np.asarray(pitch_offset, dtype=float) - float(self.cents(svarasthana, 0.0)[()])


def _held(u, p, s, sv):
    return np.full_like(u, p[0])


def _wave(u, p, s, sv):
    return p[0] + s.cents(sv, p[1]) * np.exp(-np.clip(p[4], -s.set["limit"],
                                                      s.set["limit"])) \
        * np.cos(2 * np.pi * p[2] * u + p[3])


def _rectified(u, p, s, sv):
    return -s.cents(sv, p[0]) * (p[3] + 0.5 * np.cos(2 * np.pi * p[1] * u + p[2]))


def _settle(u, p, s, sv):
    return s.cents(sv, p[0]) * (1.0 - _sat(p[2] * (u - p[1]), s.set["limit"]))


def _leave(u, p, s, sv):
    return s.cents(sv, p[0]) * _sat(p[2] * (u - p[1]), s.set["limit"])


def _pulse(u, p, s, sv):
    return s.cents(sv, p[0]) * np.exp(-0.5 * ((u - p[1]) / max(p[2], 1e-3)) ** 2)


def _plateau(u, p, s, sv):
    hold = max(p[2], 1e-3)
    edge = s.set["plateauEdge"] / hold
    return s.cents(sv, p[0]) * (_sat(edge * (u - (p[1] - hold / 2)), s.set["limit"])
                                - _sat(edge * (u - (p[1] + hold / 2)), s.set["limit"]))


SHAPES = {"held": _held, "wave": _wave, "rectified": _rectified, "settle": _settle,
          "leave": _leave, "pulse": _pulse, "plateau": _plateau}


class Taxonomy:
    def __init__(self, config):
        self.config = config
        self.set = config["generator"]
        self.types = config["types"]
        self.params = config["shapes"]
        self.rungs = config["rungs"]
        self.preference = config["preference"]
        self.returns = frozenset(n for n, t in self.types.items() if t.get("returns"))
        self.limits = {"rate": tuple(self.set["rate"]), "decay": tuple(self.set["decay"])}

    def names(self, name):
        return self.params[self.types[name]["shape"]]

    def ordered(self, name, values, beats, stored):
        spec = self.types[name]
        names = self.names(name)
        out = []
        for i, param in enumerate(names):
            value = float(values[i])
            if stored and param in spec.get("perBeat", ()):
                value *= max(beats, 1e-3)
            if param == "cycles":
                value = min(max(value, 0.0), self.set["maxCycles"])
            elif param in self.limits:
                low, high = self.limits[param]
                value = min(max(value, low), high)
            if spec.get("sign", 1) < 0 and names[1] == "reach":
                value = -value
            out.append(value)
        return tuple(out)

    def draw(self, name, values, svarasthana, scale, grid, beats=1.0, stored=True):
        return SHAPES[self.types[name]["shape"]](
            grid, self.ordered(name, values, beats, stored), scale, svarasthana)

    def values(self, name, params):
        return [params[p] for p in self.names(name)]


def _fit(tax, name, grid, target, svarasthana, scale):
    spec = tax.types[name]
    lower = [b[0] for b in spec["bounds"]]
    upper = [b[1] for b in spec["bounds"]]
    guess = np.clip(spec["guess"], lower, upper)
    render = lambda p: tax.draw(name, p, svarasthana, scale, grid, stored=False)

    try:
        result = least_squares(lambda p: render(p) - target, guess,
                               bounds=(lower, upper), max_nfev=tax.set["maxFev"])
    except (ValueError, RuntimeError):
        return None
    if not np.all(np.isfinite(result.x)):
        return None

    if spec.get("minReach"):
        reach = float(result.x[tax.names(name).index("reach")])
        if abs(reach) < spec["minReach"]:
            return None

    if spec.get("minCycles"):
        cycles = float(result.x[tax.names(name).index("cycles")])
        if cycles < spec["minCycles"]:
            return None

    return result.x, float(np.sum((render(result.x) - target) ** 2))


def _pool(samples, scale, grid):
    rows = []
    for sample in samples:
        times = np.asarray(sample["normalizedTime"], dtype=float)
        if len(times) < 2:
            continue
        row = np.interp(grid, times, scale.reframe(sample["svarasthana"],
                                                   sample["pitchOffsetCents"]))
        if np.all(np.isfinite(row)):
            rows.append(row)
    return np.array(rows) if rows else np.zeros((0, len(grid)))


def classify(tax, samples, svarasthana, scale, grid):
    pooled = _pool(samples, scale, grid)
    if len(pooled) == 0:
        return None

    if len(pooled) >= 4:
        excursion = np.ptp(pooled, axis=1)
        keep = excursion <= max(tax.set["trimFactor"] * float(np.median(excursion)), 1.0)
        pooled = pooled[keep] if keep.any() else pooled
    if len(pooled) > tax.set["notes"]:
        excursion = np.ptp(pooled, axis=1)
        chosen = np.argsort(np.abs(excursion - np.median(excursion)))[:tax.set["notes"]]
        pooled = pooled[np.sort(chosen)]

    measured_at = float(np.median([s["durationBeats"] for s in samples]))
    points = len(pooled) * len(grid)
    scored = []
    for name in tax.types:
        fits, rss = [], 0.0
        for note in pooled:
            outcome = _fit(tax, name, grid, note, svarasthana, scale)
            if outcome is None:
                fits = None
                break
            fits.append(outcome[0])
            rss += outcome[1]
        if fits:
            k = len(tax.names(name))
            scored.append((points * math.log(max(rss, 1e-12) / points) + k * math.log(points),
                           name, fits, rss))
    if not scored:
        return None

    scored.sort(key=lambda row: row[0])
    best, runner = scored[0], (scored[1] if len(scored) > 1 else None)
    if runner and runner[0] - best[0] < tax.set["tie"] \
            and tax.preference.index(runner[1]) < tax.preference.index(best[1]):
        best, runner = runner, best

    name = best[1]
    spec = tax.types[name]
    names = tax.names(name)
    index = names.index("reach") if "reach" in names else None

    stored = {}
    for i, param in enumerate(names):
        column = np.array([f[i] for f in best[2]], dtype=float)
        if param == "phase":
            resultant = np.mean(np.exp(1j * column))
            value = float(np.angle(resultant)) if abs(resultant) > 1e-9 else 0.0
        else:
            value = float(np.median(column))
        if param in spec.get("perBeat", ()):
            value /= max(measured_at, 1e-3)
        stored[param] = value

    width = float(np.median([abs(float(f[index])) for f in best[2]])) \
        if index is not None else 0.0
    return {"type": name, "params": stored, "widthSteps": width,
            "widthCents": float(abs(scale.cents(svarasthana, width)[()])),
            "rmse": float(math.sqrt(best[3] / max(points, 1))),
            "margin": None if runner is None else float(runner[0] - best[0]),
            "runnerUp": None if runner is None else runner[1],
            "notes": int(len(pooled))}


def arrive(tax, curve, name):
    if name not in tax.returns:
        return curve
    width = tax.set["arrive"]
    u = np.linspace(0.0, 1.0, len(curve))
    return curve * 0.5 * (1.0 + np.cos(np.pi * np.clip((u - 1.0 + width) / width, 0, 1)))


def spline(times, values, at):
    """Catmull-Rom through the changepoints, matching PitchCurveInterpolator."""
    times = np.asarray(times, dtype=float)
    values = np.asarray(values, dtype=float)
    index = np.clip(np.searchsorted(times, at, side="right") - 1, 0, len(times) - 2)
    span = np.where(times[index + 1] - times[index] > 1e-12,
                    times[index + 1] - times[index], 1.0)
    t = np.clip((at - times[index]) / span, 0.0, 1.0)
    p1, p2 = values[index], values[index + 1]
    p0 = values[np.maximum(index - 1, 0)]
    p3 = values[np.minimum(index + 2, len(values) - 1)]
    return 0.5 * (2.0 * p1 + (-p0 + p2) * t
                  + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t ** 2
                  + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t ** 3)


def simplify(curve, tolerance):
    count = len(curve)
    grid = np.arange(count) / (count - 1)
    keep = [0, count - 1]
    while len(keep) < count:
        drawn = spline(np.array(keep, dtype=float) / (count - 1),
                       np.array([curve[i] for i in keep]), grid)
        error = np.abs(curve - drawn)
        worst = int(np.argmax(error))
        if error[worst] <= tolerance:
            break
        keep = sorted(set(keep + [worst]))
    return keep


def seam(tax, name, params, svarasthana, scale, curve):
    if name in tax.returns:
        value = 0.0
    else:
        value = float(scale.cents(svarasthana, params["reach"])[()])

    value = float(np.clip(value, -tax.set["seam"], tax.set["seam"]))
    slope = float(curve[-1]) - float(curve[-2])
    for _ in range(tax.set["seamSteps"]):
        if slope == 0.0 or (value - float(curve[-1])) * slope > 0.0 or abs(value) <= 1.0:
            break
        value *= tax.set["seamRelax"]
    return value


def interval_class(config, cents):
    if cents is None:
        return "?"
    return tuple(config["generator"]["intervals"])[
        int(np.searchsorted(config["generator"]["intervalEdges"], float(cents)))]


def cases(tax, scale):
    out = []
    for letter, svarasthana in scale.svarasthanas().items():
        for previous in tuple(tax.set["intervals"]) + (None,):
            for following in tuple(tax.set["intervals"]) + (None,):
                for name, beats in tax.set["durations"].items():
                    out.append({"svara": letter, "svarasthana": int(svarasthana),
                                "previousInterval": tax.set["intervals"].get(previous),
                                "nextInterval": tax.set["intervals"].get(following),
                                "durationName": name, "durationBeats": float(beats)})
    return out


def chain(tax, here, previous, following, beats):
    before = interval_class(tax.config, previous)
    after = interval_class(tax.config, following)
    length = "l" if beats > 1.5 else "s"
    # Both neighbours before either one alone, and the two single-neighbour rungs
    # ordered so arrival is exhausted before departure. Dropping a neighbour one at a
    # time is what lets a context with a known departure but an unseen arrival still
    # find its own evidence, and it is why departure is tried second: a raga's svara
    # is shaped more by what precedes it, so arrival is the better of the two to
    # trust when both are thin.
    return [(here, before, after, length), (here, before, after), (here, before),
            (here, after), (here,), ()]


def index(tax, samples, scale):
    by_rung = [{} for _ in tax.rungs]
    for sample in samples:
        if len(sample.get("normalizedTime", ())) < 2:
            continue
        here = int(scale.position(sample["svarasthana"], 0))
        for level, key in enumerate(chain(tax, here, sample["previousInterval"],
                                          sample["nextInterval"],
                                          sample["durationBeats"])):
            by_rung[level].setdefault(key, []).append(sample)
    return by_rung


def ladder(tax, svarasthana, scale, grid, by_rung, keys):
    for level, key in enumerate(keys):
        evidence = by_rung[level].get(key)
        if not evidence:
            continue
        decided = classify(tax, evidence, svarasthana, scale, grid)
        if decided is None:
            continue
        decided.update(rung=level, provenance=tax.rungs[level], evidence=len(evidence))
        if level >= tax.set["widenFrom"] and "reach" in decided["params"]:
            # Clipped back to the type's own bound. A coarse rung has a wide reach
            # already, and widening it past what the type permits would put a reach
            # outside its own definition - the taxonomy would no longer mean what
            # the name says.
            spec = tax.types[decided["type"]]
            high = spec["bounds"][tax.names(decided["type"]).index("reach")][1]
            decided["params"]["reach"] = min(
                abs(decided["params"]["reach"]) * (1.0 + tax.set["widenStep"] * level), high)
            decided["widthSteps"] = decided["params"]["reach"]
            decided["widthCents"] = float(abs(
                scale.cents(svarasthana, decided["params"]["reach"])[()]))
        return decided
    return None


def held(tax):
    return {"type": "sthira", "params": {"centre": 0.0}, "widthSteps": 0.0,
            "widthCents": 0.0, "rmse": 0.0, "margin": None, "runnerUp": None,
            "notes": 0, "rung": len(tax.rungs), "provenance": "held svara", "evidence": 0}


def build(tax, case, scale, grid, by_rung, tolerance):
    svarasthana = float(case["svarasthana"])
    beats = float(case["durationBeats"])
    keys = chain(tax, int(svarasthana), case["previousInterval"],
                 case["nextInterval"], beats)

    decided = ladder(tax, svarasthana, scale, grid, by_rung, keys) or held(tax)

    if decided["rung"] == 0 and "reach" in decided["params"] and (
            decided["notes"] < tax.set["thinNotes"]
            or (decided["margin"] is not None
                and decided["margin"] < tax.set["thinMargin"])):
        for level in range(1, len(keys)):
            evidence = by_rung[level].get(keys[level])
            if not evidence:
                continue
            broader = classify(tax, evidence, svarasthana, scale, grid)
            if broader and "reach" in broader["params"]:
                decided["params"]["reach"] = broader["params"]["reach"]
                decided["widthSteps"] = abs(decided["params"]["reach"])
                decided["widthCents"] = float(abs(
                    scale.cents(svarasthana, decided["params"]["reach"])[()]))
                decided["widthFrom"] = tax.rungs[level]
                break

    name = decided["type"]
    curve = arrive(tax, tax.draw(name, tax.values(name, decided["params"]),
                                 svarasthana, scale, grid, beats), name)

    keep = simplify(curve, tolerance)
    times = [float(grid[i]) for i in keep]
    values = [float(curve[i]) for i in keep]
    values[-1] = seam(tax, name, decided["params"], svarasthana, scale, curve)
    if len(times) > 1 and times[1] - times[0] <= tax.set["seamMinTime"] \
            and abs(values[-1]) > 1e-9:
        values[-1] += np.sign(values[-1]) * tax.set["seamMinTime"] * 0.5

    out = dict(case)
    out.update({
        "type": name,
        "params": {k: round(v, 5) for k, v in decided["params"].items()},
        "widthSteps": round(decided["widthSteps"], 3),
        "widthCents": round(decided["widthCents"], 1),
        "rmse": round(decided["rmse"], 2),
        "margin": None if decided["margin"] is None else round(decided["margin"], 1),
        "runnerUp": decided["runnerUp"],
        "provenance": decided["provenance"],
        "evidence": decided["evidence"],
        "seamOut": round(values[-1], 2),
        "points": [[round(t, 5), round(v, 2)] for t, v in zip(times, values)],
    })
    if decided.get("widthFrom"):
        out["widthFrom"] = decided["widthFrom"]
    return out


_STATE = {}


def _worker(tax, scale, grid, by_rung, tolerance):
    _STATE.update(tax=tax, scale=scale, grid=grid, by_rung=by_rung, tolerance=tolerance)


def _one(case):
    return build(_STATE["tax"], case, _STATE["scale"], _STATE["grid"],
                 _STATE["by_rung"], _STATE["tolerance"])


def generate(config, name, offsets, samples, workers=None, advance=None, tolerance=None):
    tax = Taxonomy(config)
    scale = Scale(config, name, offsets)
    grid = np.linspace(0.0, 1.0, tax.set["grid"])
    tolerance = tax.set["tolerance"] if tolerance is None else tolerance
    by_rung = index(tax, samples, scale)

    workers = workers or max(1, (os.cpu_count() or 2) - 1)
    out = []
    with multiprocessing.Pool(workers, initializer=_worker,
                              initargs=(tax, scale, grid, by_rung, tolerance)) as pool:
        for result in pool.imap(_one, cases(tax, scale), chunksize=8):
            out.append(result)
            if advance is not None:
                advance()
    return out
