import os

import numpy as np
import pandas as pd

from . import raga
from .dsp import (load_pitch_track, find_extrema, midpoints, repair_segment,
                  douglas_peucker)


def time_to_seconds(value):
    parts = str(value).split(":")
    return float(parts[-2]) * 60 + float(parts[-1])


def svara_positions(config, name, offsets):
    octave_cents = config["generator"]["octave"]
    targets = []
    for letter, base in raga.svarasthanas(config, name).items():
        for octave in (-octave_cents, 0, octave_cents):
            targets.append(base + offsets.get(letter, 0.0) + octave)
    return np.sort(np.unique(np.array(targets, dtype=float)))


def snap_to_svara(value, targets, strength, sigma):
    nearest = float(targets[int(np.argmin(np.abs(targets - value)))])
    distance = nearest - value
    weight = float(np.exp(-0.5 * (distance / sigma) ** 2))
    return value + weight * strength * distance


def extract_contour(pitch, targets, centre, prominence, tolerance, snap_sigma,
                    extremum_strength, midpoint_strength, time_scale):
    change_points = find_extrema(pitch, prominence=prominence, height=None)
    change_points = np.concatenate([np.array([0]), change_points,
                                    np.array([len(pitch) - 1])])
    change_points = np.sort(np.unique(change_points))

    knees = midpoints(change_points)
    all_points = np.sort(np.unique(np.concatenate([change_points, knees])))
    knees = midpoints(all_points)
    all_points = np.sort(np.unique(np.concatenate([all_points, knees])))

    if len(all_points) < 2:
        return None

    extrema = set(change_points.tolist())
    x = all_points / float(len(pitch) - 1)
    y = np.array([
        snap_to_svara(float(pitch[i]), targets,
                      extremum_strength if i in extrema else midpoint_strength,
                      snap_sigma)
        for i in all_points
    ]) - centre

    if tolerance > 0:
        kept = douglas_peucker(x * time_scale, y, tolerance)
        x, y = x[kept], y[kept]

    if len(x) < 2:
        return None

    return [round(float(v), 4) for v in x], [round(float(v), 2) for v in y]


def read_annotations(annotations_dir, name, artist):
    frame = pd.read_csv(os.path.join(annotations_dir, name, f"{artist}_{name}.tsv"),
                        delimiter="\t")
    frame["begin"] = frame["Begin time"].map(time_to_seconds)
    frame["end"] = frame["End time"].map(time_to_seconds)
    frame["label"] = frame["Annotation"].astype(str)
    return frame


def list_artists(annotations_dir, name, excluded=frozenset()):
    names = []
    for file in sorted(os.listdir(os.path.join(annotations_dir, name))):
        artist = file.split("_")[0]
        if (name, artist) not in excluded:
            names.append(artist)
    return names


def extract_recording(config, name, artist, annotations_dir, pitch_tracks_dir, tonic,
                      tempo, targets, dsp, snap, tolerance, stats):
    beat_seconds = 60.0 / tempo

    annotations = read_annotations(annotations_dir, name, artist)
    time, pitch = load_pitch_track(pitch_tracks_dir, name, artist, tonic,
                                   dsp["maxGap"], dsp["smoothing"],
                                   dsp["minPoints"], dsp["centsPerOctave"])

    begins = annotations["begin"].values
    ends = annotations["end"].values
    labels = annotations["label"].values

    def voiced(index):
        if index < 0 or index >= len(annotations):
            return None
        segment = pitch[(time > begins[index]) & (time < ends[index])]
        segment = segment[~np.isnan(segment)]
        return segment if len(segment) else None

    samples = []

    for i in range(len(annotations)):
        stats["total"] += 1

        current = raga.resolve_svara(config, name, labels[i], voiced(i))
        if current is None:
            stats["unparsed"] += 1
            continue

        segment = pitch[(time > begins[i]) & (time < ends[i])]
        repaired = repair_segment(segment, config["generator"]["maxNanFraction"],
                                  dsp["minLength"])
        if repaired is None:
            stats["unusable"] += 1
            continue

        previous = raga.resolve_svara(config, name, labels[i - 1], voiced(i - 1)) \
            if i > 0 else None
        following = raga.resolve_svara(config, name, labels[i + 1], voiced(i + 1)) \
            if i + 1 < len(annotations) else None

        curve = extract_contour(repaired, targets, float(current),
                                config["generator"]["prominence"], tolerance,
                                snap["sigma"], snap["extremum"], snap["midpoint"],
                                snap["timeScale"])
        if curve is None:
            stats["flat"] += 1
            continue

        x, y = curve
        duration = float(ends[i] - begins[i])

        samples.append({
            "artist": artist,
            "svarasthana": raga.fold_to_octave(config, current),
            "previousInterval": None if previous is None else int(round(previous - current)),
            "nextInterval": None if following is None else int(round(following - current)),
            "durationSeconds": round(duration, 4),
            "durationBeats": round(duration / beat_seconds, 4),
            "excursionCents": round(float(np.ptp(y)), 1),
            "normalizedTime": x,
            "pitchOffsetCents": y,
        })
        stats["kept"] += 1
        if previous is None or following is None:
            stats["partial"] += 1

    return samples
