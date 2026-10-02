import os
import random

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
from matplotlib.ticker import MaxNLocator
import mir_eval
import numpy as np
from scipy.io import wavfile

from . import plot, scale


def usable(note, present):
    if note["previous"] is None or note["next"] is None:
        return False
    return (note["svara"], note["previous"], note["next"]) in present


def flush(run, count, windows):
    if len(run) >= count:
        for start in range(len(run) - count + 1):
            windows.append(run[start:start + count])


def collect(notes, present, count):
    windows = []
    run = []
    for note in notes:
        keep = usable(note, present) and (
            not run or run[-1]["artist"] == note["artist"])
        if keep:
            run.append(note)
            continue
        flush(run, count, windows)
        run = [note] if usable(note, present) else []
    flush(run, count, windows)
    return windows


def choose(notes, present, least, most):
    for count in range(random.randint(least, most), least - 1, -1):
        windows = collect(notes, present, count)
        if windows:
            return random.choice(windows)
    return None


def series(config, chosen, present, positions):
    actual_times, actual_cents = [], []
    synth_times, synth_cents = [], []
    bands = []
    start = 0.0
    for note in chosen:
        duration = float(note["durationSeconds"])
        here = positions[note["svara"]]
        row = present[(note["svara"], note["previous"], note["next"])]
        pitch = np.asarray(note["pitch"], dtype=float)
        actual_times.append(
            start + np.linspace(0.0, duration, len(pitch), endpoint=False))
        actual_cents.append(pitch)
        shape = np.asarray(
            plot.curve(config, row, float(note["durationBeats"]), here,
                       positions[note["previous"]]),
            dtype=float)
        synth_times.append(
            start + np.linspace(0.0, duration, len(shape), endpoint=False))
        synth_cents.append(shape)
        bands.append((start, duration, here))
        start += duration
    return (np.concatenate(actual_times), np.concatenate(actual_cents),
            np.concatenate(synth_times), np.concatenate(synth_cents),
            bands, start)


def connect(times, values, marks, total, rate=400.0, glide=0.06):
    grid = np.arange(0.0, total, 1.0 / rate)
    flat = np.interp(grid, times, values)
    durations = np.diff(np.append(np.asarray(marks, dtype=float), total))
    half = min(glide, 0.4 * float(np.min(durations))) / 2.0
    for boundary in marks[1:]:
        low = max(boundary - half, 0.0)
        high = min(boundary + half, total)
        if high <= low:
            continue
        left = np.interp(low, times, values)
        right = np.interp(high, times, values)
        mask = (grid >= low) & (grid <= high)
        if np.count_nonzero(mask) < 2:
            continue
        u = (grid[mask] - low) / (high - low)
        flat[mask] = left + (right - left) * (1.0 - np.cos(np.pi * u)) / 2.0
    return grid, flat


def sonify(path, times, cents, tonic, sample_rate):
    frequencies = float(tonic) * 2.0 ** (np.asarray(cents, dtype=float) / 1200.0)
    signal = mir_eval.sonify.pitch_contour(times, frequencies, sample_rate)
    peak = float(np.max(np.abs(signal)))
    if peak > 0.0:
        signal = signal / peak
    wavfile.write(path, sample_rate,
                  (np.clip(signal, -1.0, 1.0) * 32767.0).astype(np.int16))


def draw(config, name, chosen, actual_times, actual, synth_times, synth,
         bands, total):
    plot.rc()
    muted = "#898781"
    ink = "#0b0b0b"
    gridline = "#e1e0d9"
    axis = "#c3c2b7"
    here_fill = "#d7efd1"
    here_edge = "#79b96b"
    block = 68.0
    padding = 160.0

    grid, labels = plot.lanes(config, name)
    low = min(float(np.min(actual)), float(np.min(synth))) - padding
    high = max(float(np.max(actual)), float(np.max(synth))) + padding
    for _, _, here in bands:
        low, high = min(low, here - block), max(high, here + block)
    shown = [(lane, label) for lane, label in zip(grid, labels)
             if low <= lane <= high]

    figure, axes = plt.subplots(figsize=(14.0, 3.2))
    for lane, _ in shown:
        axes.axhline(lane, color=gridline, linewidth=0.8, zorder=0)
    plot.split(axes, shown)
    for start, duration, here in bands:
        axes.axvline(start, color=gridline, linewidth=0.8, zorder=0)
        axes.add_patch(Rectangle((start, here - block / 2.0), duration, block,
                                 facecolor=here_fill, edgecolor=here_edge,
                                 linewidth=0.8, zorder=1))
    axes.axvline(total, color=gridline, linewidth=0.8, zorder=0)
    axes.plot(actual_times, actual, color=muted, linewidth=1.2, zorder=3,
              label="actual")
    axes.plot(synth_times, synth, color=ink, linewidth=1.8, zorder=4,
              label="synthesised")
    axes.set_xlim(0.0, total)
    axes.set_ylim(low, high)
    axes.set_yticks([lane for lane, _ in shown])
    axes.set_yticklabels([label for _, label in shown])
    axes.xaxis.set_major_locator(MaxNLocator(8))
    axes.tick_params(axis="both", labelsize=7, pad=3)
    for side in ("top", "right"):
        axes.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        axes.spines[side].set_color(axis)
    heading = " ".join(scale.syllables(config, note["svara"])
                       for note in chosen)
    axes.set_title(f"{name.capitalize()} · {heading}", loc="left",
                   fontsize=9.0, color=muted, pad=6.0)
    axes.legend(loc="best", frameon=False, fontsize=7.5)
    figure.tight_layout()
    return figure


def raga(config, name, notes, rows, tonics, output_dir, plots_dir,
         advance=None):
    if advance is not None:
        advance(0, 4)
    present = {(r["svara"], r["previous"], r["next"]): r for r in rows}
    positions = scale.positions(config, name)
    chosen = choose(notes, present, 10, 15)
    if chosen is None:
        if advance is not None:
            advance(4, 4)
        return 0
    actual_times, actual, synth_times, synth, bands, total = series(
        config, chosen, present, positions)
    synth_times, synth = connect(
        synth_times, synth, [start for start, _, _ in bands], total)
    if advance is not None:
        advance(1, 4)

    audio_dir = os.path.join(output_dir, "synthesised")
    image_dir = os.path.join(plots_dir, "synthesised")
    os.makedirs(audio_dir, exist_ok=True)
    os.makedirs(image_dir, exist_ok=True)
    tonic = float(tonics[chosen[0]["artist"]])
    sonify(os.path.join(audio_dir, f"{name}_pitchtrack.wav"),
           actual_times, actual, tonic, 22050)
    if advance is not None:
        advance(2, 4)
    sonify(os.path.join(audio_dir, f"{name}_gamaka.wav"),
           synth_times, synth, tonic, 22050)
    if advance is not None:
        advance(3, 4)

    figure = draw(config, name, chosen, actual_times, actual,
                  synth_times, synth, bands, total)
    figure.savefig(os.path.join(image_dir, f"{name}.png"), dpi=150)
    plt.close(figure)
    if advance is not None:
        advance(4, 4)
    return 1
