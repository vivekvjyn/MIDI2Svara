import os
import pickle

import numpy as np
import pandas as pd
from scipy.interpolate import UnivariateSpline

from . import scale


def fill_gaps(values, max_gap):
    gaps = values == 0
    ranges = []
    start = None
    for i, gap in enumerate(gaps):
        if gap and start is None:
            start = i
        elif not gap and start is not None:
            ranges.append((start, i))
            start = None
    if start is not None:
        ranges.append((start, len(values)))
    out = np.array(values, dtype=float)
    for a, b in ranges:
        if (b - a) <= max_gap:
            out[a:b] = np.nan
    try:
        return pd.Series(out).interpolate(method="cubicspline").ffill().bfill().values
    except Exception:
        try:
            return pd.Series(out).interpolate().ffill().bfill().values
        except Exception:
            return out


def smooth(time, pitch, smoothing, min_points):
    time = np.asarray(time, dtype=float)
    pitch = np.asarray(pitch, dtype=float)
    out = np.full_like(pitch, np.nan)
    valid = np.where(np.isfinite(time) & np.isfinite(pitch))[0]
    if len(valid) == 0:
        return out
    chunks = np.split(valid, np.where(np.diff(valid) > 1)[0] + 1)
    for chunk in chunks:
        if len(chunk) < min_points:
            out[chunk] = pitch[chunk]
            continue
        t = time[chunk]
        v = pitch[chunk]
        low, high = np.min(v), np.max(v)
        normalised = (v - low) / (high - low + 1e-10)
        spline = UnivariateSpline(t, normalised, s=smoothing)
        out[chunk] = spline(t) * (high - low) + low
    return out


def load_pitch(path, tonic, settings):
    data = np.loadtxt(path, delimiter="\t")
    if data.ndim != 2 or data.shape[1] < 2 or len(data) < 2:
        return None
    time = data[:, 0]
    values = np.array(fill_gaps(data[:, 1], settings["maxGap"]), dtype=float)
    values[values == 0] = np.nan
    with np.errstate(divide="ignore", invalid="ignore"):
        values = settings["octave"] * np.log2(values / float(tonic))
    return time, smooth(time, values, settings["smoothing"], settings["minPoints"])


def repair(segment, max_nan_fraction, min_length):
    segment = np.asarray(segment, dtype=float)
    if len(segment) == 0:
        return None
    valid = np.where(~np.isnan(segment))[0]
    if len(valid) == 0:
        return None
    core = segment[valid[0]:valid[-1] + 1]
    if len(core) < min_length:
        return None
    nan_count = int(np.isnan(core).sum())
    if nan_count / len(core) > max_nan_fraction:
        return None
    if nan_count:
        index = np.arange(len(core))
        good = ~np.isnan(core)
        core = np.interp(index, index[good], core[good])
    return core


def seconds(value):
    parts = str(value).split(":")
    return float(parts[-2]) * 60 + float(parts[-1])


def annotations(path):
    frame = pd.read_csv(path, delimiter="\t")
    begin = np.array([seconds(v) for v in frame["Begin time"]], dtype=float)
    end = np.array([seconds(v) for v in frame["End time"]], dtype=float)
    labels = np.array([str(v) for v in frame["Annotation"]])
    return begin, end, labels


def recording(config, name, artist, annotation_file, pitch_file, tonic, tempo):
    settings = config["settings"]
    begin, end, labels = annotations(annotation_file)
    loaded = load_pitch(pitch_file, tonic, settings)
    if loaded is None:
        return []
    time, pitch = loaded
    beat = 60.0 / float(tempo)

    def window(index):
        if index < 0 or index >= len(labels):
            return None, None
        return labels[index], pitch[(time > begin[index]) & (time < end[index])]

    notes = []
    for i in range(len(labels)):
        current_label, current_window = window(i)
        if current_label is None or current_window is None or len(current_window) == 0:
            continue
        letter, svarasthana = scale.resolve(config, name, current_label, current_window)
        if svarasthana is None:
            continue
        core = repair(current_window, settings["maxNanFraction"], settings["minLength"])
        if core is None:
            continue

        previous = next_svara = None
        previous_interval = next_interval = None
        for step, side in ((-1, "previous"), (1, "next")):
            neighbour_label, neighbour_window = window(i + step)
            if neighbour_label is None or neighbour_window is None or len(neighbour_window) == 0:
                continue
            neighbour_letter, neighbour = scale.resolve(config, name, neighbour_label,
                                                        neighbour_window)
            if neighbour is None:
                continue
            if side == "previous":
                previous = neighbour_letter
                previous_interval = int(round(neighbour - svarasthana))
            else:
                next_svara = neighbour_letter
                next_interval = int(round(neighbour - svarasthana))

        duration = float(end[i] - begin[i])
        notes.append({
            "index": i,
            "artist": artist,
            "svara": letter,
            "previous": previous,
            "next": next_svara,
            "previousInterval": previous_interval,
            "nextInterval": next_interval,
            "svarasthana": float(svarasthana),
            "durationSeconds": round(duration, 4),
            "durationBeats": round(duration / beat, 4),
            "pitch": [round(float(v), 2) for v in core],
        })
    return notes


def raga(config, name, options, tonics, tempos, advance=None):
    annotations_dir = options["annotationsDir"]
    pitch_dir = options["pitchTracksDir"]
    folder = os.path.join(options["cacheDir"], name)
    os.makedirs(folder, exist_ok=True)
    source = os.path.join(annotations_dir, name)
    files = sorted(f for f in os.listdir(source) if f.endswith(".tsv"))
    artists = sorted({f.split("_")[0] for f in files})

    notes = []
    if advance is not None:
        advance(0, len(artists))
    for done, artist in enumerate(artists, 1):
        cache = os.path.join(folder, artist + ".pkl")
        stored = None
        if os.path.exists(cache) and not options["force"]:
            with open(cache, "rb") as handle:
                cached = pickle.load(handle)
            if isinstance(cached, tuple) and len(cached) == 2 and cached[0] == 2:
                stored = cached[1]
        if stored is not None:
            notes.extend(stored)
        else:
            pitch_file = os.path.join(pitch_dir, name, artist + ".tsv")
            annotation_file = os.path.join(source, f"{artist}_{name}.tsv")
            if (os.path.exists(pitch_file) and artist in tonics
                    and artist in tempos.get(name, {})):
                built = recording(config, name, artist, annotation_file, pitch_file,
                                  float(tonics[artist]), float(tempos[name][artist]))
                with open(cache, "wb") as handle:
                    pickle.dump((2, built), handle, protocol=4)
                notes.extend(built)
            elif os.path.exists(cache):
                os.remove(cache)
        if advance is not None:
            advance(done, len(artists))
    return notes
