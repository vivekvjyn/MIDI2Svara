import os

import numpy as np
import pandas as pd

from . import raga
from .dsp import load_pitch_track


def _gaussian_smooth(counts, sigma):
    radius = int(np.ceil(3 * sigma))
    offsets = np.arange(-radius, radius + 1)
    kernel = np.exp(-0.5 * (offsets / sigma) ** 2)
    kernel /= kernel.sum()
    return np.convolve(counts, kernel, mode="same")


def _time_to_seconds(value):
    parts = str(value).split(":")
    return float(parts[-2]) * 60 + float(parts[-1])


def collect_deviations(config, name, annotations_dir, pitch_tracks_dir, tonics,
                       excluded, dsp, max_gap):
    deviations = {letter: [] for letter in raga.svarasthanas(config, name)}

    for file in sorted(os.listdir(os.path.join(annotations_dir, name))):
        artist = file.split("_")[0]
        if (name, artist) in excluded:
            continue

        annotations = pd.read_csv(os.path.join(annotations_dir, name, file), delimiter="\t")
        time, pitch = load_pitch_track(pitch_tracks_dir, name, artist,
                                       float(tonics[artist]), max_gap,
                                       dsp["smoothing"], dsp["minPoints"],
                                       dsp["centsPerOctave"])

        begins = annotations["Begin time"].map(_time_to_seconds).values
        ends = annotations["End time"].map(_time_to_seconds).values

        for i in range(len(annotations)):
            window = (time > begins[i]) & (time < ends[i])
            segment = pitch[window]
            segment = segment[~np.isnan(segment)]
            if len(segment) == 0:
                continue

            annotation = annotations.iloc[i]["Annotation"]
            parsed = raga.parse_annotation(config, name, annotation)
            if parsed is None:
                continue

            position = raga.resolve_svara(config, name, annotation, segment)
            deviations[parsed[0]].append(segment - position)

    return {letter: (np.concatenate(values) if values else np.empty(0))
            for letter, values in deviations.items()}


def estimate_offsets(config, name, deviations, max_deviation, histogram_sigma, min_frames):
    scale = raga.scale(config, name)
    positions = raga.svarasthanas(config, name)
    offsets = {}

    for letter, values in deviations.items():
        degree = positions[letter] % config["generator"]["octave"]

        half = config["generator"]["octave"] / 2
        neighbours = [abs(((other - degree + half) % (2 * half)) - half)
                      for other in scale if other != degree]
        window = min(max_deviation, (min(neighbours) / 2) if neighbours else max_deviation)

        values = values[np.abs(values) <= window]
        if len(values) < min_frames:
            offsets[letter] = 0.0
            continue

        edges = np.arange(-window, window + 1.0, 1.0)
        counts, _ = np.histogram(values, bins=edges)
        smoothed = _gaussian_smooth(counts.astype(float), histogram_sigma)
        centres = (edges[:-1] + edges[1:]) / 2

        offsets[letter] = float(centres[int(np.argmax(smoothed))])

    return offsets


def build(config, name, annotations_dir, pitch_tracks_dir, tonics, excluded, dsp):
    settings = config["generator"]
    deviations = collect_deviations(config, name, annotations_dir, pitch_tracks_dir,
                                    tonics, excluded, dsp, settings["maxGap"])
    counts = {letter: int(len(values)) for letter, values in deviations.items()}
    offsets = estimate_offsets(config, name, deviations, settings["maxDeviation"],
                               settings["histogramSigma"], settings["minFrames"])
    return offsets, counts
