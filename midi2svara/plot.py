import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
import numpy as np

from . import scale, shapes


def rc():
    surface = "#fcfcfb"
    ink = "#0b0b0b"
    muted = "#898781"
    axis = "#c3c2b7"
    lane = 0.8
    plt.rcParams.update({
        "font.family": "sans-serif",
        "font.sans-serif": ["DejaVu Sans", "Segoe UI", "Helvetica", "Arial"],
        "figure.facecolor": surface,
        "axes.facecolor": surface,
        "savefig.facecolor": surface,
        "axes.edgecolor": axis,
        "axes.labelcolor": muted,
        "xtick.color": muted,
        "ytick.color": muted,
        "text.color": ink,
        "axes.linewidth": lane,
        "xtick.major.size": 0,
        "ytick.major.size": 0,
        "font.size": 9,
    })


def lanes(config, name):
    letters = scale.letters(config, name)
    octave = config["settings"]["octave"]
    span = config["settings"]["sthayiSpan"]
    below, above = scale.marks(config)
    lanes, labels = [], []
    for letter, base in letters.items():
        for k in range(-span, span + 1):
            mark = above * k if k > 0 else below * (-k)
            lanes.append(base + k * octave)
            labels.append(letter + mark)
    return lanes, labels


def ease(u, at, rate):
    raw = 1.0 / (1.0 + np.exp(-np.clip(rate * (u - at), -60.0, 60.0)))
    return (raw - raw[0]) / (raw[-1] - raw[0])


def split(axes, shown):
    groups = {}
    for lane, _ in shown:
        groups.setdefault(int(np.floor(lane / 1200.0)), []).append(lane)
    keys = sorted(groups)
    for lower, upper in zip(keys, keys[1:]):
        boundary = (max(groups[lower]) + min(groups[upper])) / 2.0
        axes.axhline(boundary, color="#c3c2b7", linewidth=1.0, zorder=0)


def curve(config, row, length, here, start=None):
    u = np.linspace(0.0, 1.0, 512)
    gamaka = row["gamaka"]
    params = row["params"]
    top = float(params["top"])
    bottom = float(params["bottom"])
    here = float(here)
    if start is None:
        start = here + float(row["previousInterval"])
    up = top - here
    down = here - bottom
    base = start + (here - start) * ease(u, 0.06, 40.0)
    if gamaka in ("sthira", "jaru", "nokku"):
        return base
    if gamaka in ("kampita", "andola", "vali"):
        swing = {"kampita": shapes.kampita, "andola": shapes.andola,
                 "vali": shapes.vali}[gamaka](length, top, bottom)[1]
        window = np.sin(np.pi * u) ** 2
        return base + window * (np.asarray(swing, dtype=float) - base)
    if gamaka == "sphurita":
        shape = shapes.sphurita(length, down)[1]
    elif gamaka == "tripuchcha":
        shape = shapes.tripuchcha(length, down)[1]
    elif gamaka == "ahata" or gamaka == "pratyahata":
        reach = up if up > 0.0 else -down
        shape = shapes.ahata(length, reach)[1]
    elif gamaka == "khandippu":
        reach = -down if down > 0.0 else up
        shape = shapes.khandippu(length, reach)[1]
    elif gamaka == "odukkal":
        reach = -down if down > 0.0 else up
        shape = shapes.odukkal(length, reach)[1]
    elif gamaka == "janta":
        reach = up if up >= down else -down
        shape = shapes.janta(length, reach)[1]
    elif gamaka == "orikai":
        shape = shapes.orikai(length, up)[1]
    else:
        shape = shapes.ravai(length, bottom - here)[1]
    shape = np.asarray(shape, dtype=float)
    gesture = shape - shape[0] * (1.0 - u) - shape[-1] * u
    return base + gesture


def context(config, row):
    return " ".join([scale.syllables(config, row["svara"]),
                     scale.syllables(config, row["previous"]),
                     scale.syllables(config, row["next"])])


def draw(axes, config, name, row, length):
    muted = "#898781"
    gridline = "#e1e0d9"
    axis = "#c3c2b7"
    ink = "#0b0b0b"
    lane = 0.8
    block = 68.0
    inset = 0.03
    edge = 0.15
    label = 8.5
    title = 8.0
    tick = 4.0
    padding = 160.0
    width = 2.2
    here_fill = "#d7efd1"
    here_edge = "#79b96b"
    here_text = "#3f7a38"
    context_fill = "#dbdbd6"
    context_edge = "#9b9b94"

    here = scale.position(config, name, row["svara"])
    grid, labels = lanes(config, name)
    margin = 0.18 * length
    x = np.linspace(0.0, length, 512)
    y = curve(config, row, length, here)

    notes = [(here + row["previousInterval"], scale.syllables(config, row["previous"]), False),
             (here + row["nextInterval"], scale.syllables(config, row["next"]), True)]

    low = min(y.min() - padding, here - block)
    high = max(y.max() + padding, here + block)
    for position, _, _ in notes:
        low, high = min(low, position - block / 2), max(high, position + block / 2)

    shown = [(lane_, label_) for lane_, label_ in zip(grid, labels)
             if low <= lane_ <= high]
    for lane_, _ in shown:
        axes.axhline(lane_, color=gridline, linewidth=lane, zorder=0)
    split(axes, shown)
    for beat in range(int(length) + 1):
        axes.axvline(beat, color=gridline, linewidth=lane, zorder=0)

    axes.add_patch(Rectangle((inset * length, here - block / 2),
                             (1.0 - 2 * inset) * length, block,
                             facecolor=here_fill, edgecolor="none", zorder=1))
    axes.add_patch(Rectangle((inset * length, here - block / 2),
                             (1.0 - 2 * inset) * length, block,
                             facecolor="none", edgecolor=here_edge,
                             linewidth=1.1, zorder=2))
    axes.text(length / 2.0, here, row["svara"], ha="center", va="center",
              fontsize=label, color=here_text, zorder=3)

    axes.plot(x, y, color=ink, linewidth=width, solid_capstyle="round", zorder=4)

    for position, syllable, right in notes:
        start = length if right else -edge * length
        axes.add_patch(Rectangle((start, position - block / 2),
                                 edge * length, block,
                                 facecolor=context_fill, edgecolor="none",
                                 zorder=1))
        axes.add_patch(Rectangle((start, position - block / 2),
                                 edge * length, block,
                                 facecolor="none", edgecolor=context_edge,
                                 linewidth=1.1, zorder=2))
        axes.text(start + edge * length / 2.0, position, syllable,
                  ha="center", va="center", fontsize=label, color=muted,
                  zorder=3)

    heading = context(config, row)
    axes.set_xlim(-margin, length + margin)
    axes.set_ylim(low, high)
    axes.set_yticks([lane_ for lane_, _ in shown])
    axes.set_yticklabels([label_ for _, label_ in shown])
    axes.set_xticks([])
    axes.set_xticklabels([])
    for side in ("top", "right"):
        axes.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        axes.spines[side].set_color(axis)
    axes.tick_params(axis="y", pad=tick)
    axes.set_title(f"{name.capitalize()} · {heading}\n{row['gamaka']}", loc="left",
                   fontsize=title, color=muted, linespacing=1.5, pad=4.0)


def raga(config, name, rows, plots_dir, lengths, advance=None):
    rc()
    written = 0
    if advance is not None:
        advance(0, len(rows))
    for done, row in enumerate(rows, 1):
        folder = os.path.join(plots_dir, name, row["svara"],
                              "_".join(scale.syllables(config, part)
                                       for part in (row["svara"], row["previous"],
                                                    row["next"])))
        os.makedirs(folder, exist_ok=True)
        for length in lengths:
            figure, axes = plt.subplots(figsize=(3.4, 2.6))
            draw(axes, config, name, row, float(length))
            figure.tight_layout()
            figure.savefig(os.path.join(folder, f"{float(length):g}.png"), dpi=150)
            plt.close(figure)
            written += 1
        if advance is not None:
            advance(done, len(rows))
    return written
