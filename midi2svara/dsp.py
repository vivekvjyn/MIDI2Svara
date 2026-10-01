import os

import numpy as np
import pandas as pd
from scipy.interpolate import UnivariateSpline
from scipy.signal import find_peaks


def load_pitch_track(pitch_dir, name, artist, tonic, max_gap, smoothing, min_points,
                     cents_per_octave):
    data = pd.read_csv(os.path.join(pitch_dir, name, f"{artist}.tsv"),
                       delimiter="\t", header=None)
    time = data[0].values
    pitch = data[1].values

    pitch = fill_short_gaps(pitch, 0, max_gap=max_gap)
    pitch[pitch == 0] = np.nan
    pitch = cents_per_octave * np.log2(pitch / tonic)
    pitch = smooth_pitch(time, pitch, smoothing, min_points)

    return time, pitch


def find_extrema(signal, prominence, height=None):
    peaks, _ = find_peaks(signal, prominence=prominence, height=height)
    troughs, _ = find_peaks(-signal, prominence=prominence, height=height)
    return np.sort(np.unique(np.concatenate([peaks, troughs])))


def midpoints(changepoints):
    if len(changepoints) < 2:
        return np.array([], dtype=int)
    knees = [(changepoints[i - 1] + changepoints[i]) // 2
             for i in range(1, len(changepoints))]
    return np.array(knees).astype(int)


def smooth_pitch(time, pitch, smoothing_factor, min_points):
    time = np.asarray(time, dtype=float)
    pitch = np.asarray(pitch, dtype=float)
    smoothed = np.full_like(pitch, np.nan)

    valid_mask = ~pd.isna(time) & ~pd.isna(pitch)
    valid_indices = np.where(valid_mask)[0]
    if len(valid_indices) == 0:
        return smoothed

    chunks = np.split(valid_indices, np.where(np.diff(valid_indices) > 1)[0] + 1)

    for chunk in chunks:
        if len(chunk) < min_points:
            smoothed[chunk] = pitch[chunk]
            continue

        time_chunk = time[chunk]
        pitch_chunk = pitch[chunk]
        low, high = np.min(pitch_chunk), np.max(pitch_chunk)
        normalized = (pitch_chunk - low) / (high - low + 1e-10)
        spline = UnivariateSpline(time_chunk, normalized, s=smoothing_factor)
        smoothed[chunk] = spline(time_chunk) * (high - low) + low

    return smoothed


def fill_short_gaps(arr, val, max_gap):
    s = np.copy(arr)
    is_gap = np.isnan(s) if np.isnan(val) else (s == val)

    gap_ranges = []
    in_gap = False
    gap_start = None
    for i, g in enumerate(is_gap):
        if g and not in_gap:
            in_gap, gap_start = True, i
        elif not g and in_gap:
            in_gap = False
            gap_ranges.append((gap_start, i))
    if in_gap:
        gap_ranges.append((gap_start, len(s)))

    for start, end in gap_ranges:
        if (end - start) <= max_gap:
            s[start:end] = np.nan

    series = pd.Series(s)
    interpolated = series.interpolate(method="cubicspline").ffill().bfill().values
    return np.array(interpolated)


def repair_segment(segment, max_nan_fraction, min_length):
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


def resample_curve(x, y, num_points):
    grid = np.linspace(0.0, 1.0, num_points)
    return np.interp(grid, x, y)


def douglas_peucker(x, y, tolerance):
    keep = np.zeros(len(x), dtype=bool)
    keep[0] = keep[-1] = True
    stack = [(0, len(x) - 1)]

    while stack:
        start, end = stack.pop()
        if end <= start + 1:
            continue

        x0, y0, x1, y1 = x[start], y[start], x[end], y[end]
        dx, dy = x1 - x0, y1 - y0
        norm = np.hypot(dx, dy)

        if norm < 1e-12:
            distances = np.hypot(x[start + 1:end] - x0, y[start + 1:end] - y0)
        else:
            distances = np.abs(dy * (x[start + 1:end] - x0) - dx * (y[start + 1:end] - y0)) / norm

        offset = int(np.argmax(distances))
        if distances[offset] > tolerance:
            split = start + 1 + offset
            keep[split] = True
            stack.append((start, split))
            stack.append((split, end))

    return np.where(keep)[0]
