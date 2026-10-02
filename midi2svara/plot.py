import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
import numpy as np

from . import scale, shapes

SURFACE = "#fcfcfb"
INK = "#0b0b0b"
MUTED = "#898781"
GRIDLINE = "#e1e0d9"
AXIS = "#c3c2b7"
PANEL = (3.4, 2.6)
DPI = 150
PADDING = 160.0
MARGIN = 0.12
EDGE = 0.10
GAP = 0.02
LANE = 0.8
CURVE = 2.2
BLOCK = 68.0
INSET = 0.03
BLOCK_ALPHA = 0.22
LABEL = 8.5
TITLE = 8.0
TICK = 4.0


def rc():
    plt.rcParams.update({
        "font.family": "sans-serif",
        "font.sans-serif": ["DejaVu Sans", "Segoe UI", "Helvetica", "Arial"],
        "figure.facecolor": SURFACE,
        "axes.facecolor": SURFACE,
        "savefig.facecolor": SURFACE,
        "axes.edgecolor": AXIS,
        "axes.labelcolor": MUTED,
        "xtick.color": MUTED,
        "ytick.color": MUTED,
        "text.color": INK,
        "axes.linewidth": LANE,
        "xtick.major.size": 0,
        "ytick.major.size": 0,
        "font.size": 9,
    })


def lanes(config, name):
    letters = scale.letters(config, name)
    octave = config["settings"]["octave"]
    below, above = scale.marks(config)
    lanes, labels = [], []
    for letter, base in letters.items():
        for shift, mark in ((-octave, below), (0.0, ""), (octave, above)):
            lanes.append(base + shift)
            labels.append(letter + mark)
    return lanes, labels


def curve(config, row, length, here):
    gamaka = row["gamaka"]
    params = row["params"]
    top = float(params["top"])
    bottom = float(params["bottom"])
    if gamaka == "sthira":
        return np.full(512, float(here))
    if gamaka in ("kampita", "andola", "vali"):
        function = {"kampita": shapes.kampita, "andola": shapes.andola,
                    "vali": shapes.vali}[gamaka]
        return function(length, top, bottom)[1]
    if gamaka == "sphurita":
        return shapes.sphurita(length, top, bottom)[1]
    if gamaka == "ahata":
        return bottom + shapes.ahata(length, top - bottom)[1]
    if gamaka == "khandippu":
        return top + shapes.khandippu(length, bottom - top)[1]
    if gamaka == "nokku":
        return bottom + shapes.nokku(length, top - bottom)[1]
    if gamaka == "odukkal":
        return top + shapes.odukkal(length, bottom - top)[1]
    if gamaka in ("janta", "orikai", "jaru"):
        function = {"janta": shapes.janta, "orikai": shapes.orikai,
                    "jaru": shapes.jaru}[gamaka]
        return bottom + function(length, top - bottom)[1]
    return top + shapes.ravai(length, bottom - top)[1]


def context(config, row):
    return " ".join([scale.syllables(config, row["svara"]),
                     scale.syllables(config, row["previous"]),
                     scale.syllables(config, row["next"])])


def draw(axes, config, name, row, length):
    letters = scale.letters(config, name)
    here = letters[row["svara"]]
    grid, labels = lanes(config, name)
    margin = MARGIN * length
    x = np.linspace(0.0, length, 512)
    y = curve(config, row, length, here)

    notes = [(here + row["previousInterval"], scale.syllables(config, row["previous"]), False),
             (here + row["nextInterval"], scale.syllables(config, row["next"]), True)]

    low = min(y.min() - PADDING, here - BLOCK)
    high = max(y.max() + PADDING, here + BLOCK)
    for position, _, _ in notes:
        low, high = min(low, position - BLOCK / 2), max(high, position + BLOCK / 2)

    shown = [(lane, label) for lane, label in zip(grid, labels) if low <= lane <= high]
    for lane, _ in shown:
        axes.axhline(lane, color=GRIDLINE, linewidth=LANE, zorder=0)
    for beat in range(int(length) + 1):
        axes.axvline(beat, color=GRIDLINE, linewidth=LANE, zorder=0)

    axes.add_patch(Rectangle((INSET * length, here - BLOCK / 2),
                             (1.0 - 2 * INSET) * length, BLOCK,
                             facecolor=GRIDLINE, alpha=BLOCK_ALPHA,
                             edgecolor="none", zorder=1))
    axes.add_patch(Rectangle((INSET * length, here - BLOCK / 2),
                             (1.0 - 2 * INSET) * length, BLOCK,
                             facecolor="none", edgecolor=AXIS,
                             linewidth=1.1, zorder=2))
    axes.text(length / 2.0, here, row["svara"], ha="center", va="center",
              fontsize=LABEL, color=MUTED, zorder=3)

    axes.plot(x, y, color=INK, linewidth=CURVE, solid_capstyle="round", zorder=4)

    for position, syllable, right in notes:
        start = length if right else -EDGE * length
        axes.add_patch(Rectangle((start, position - BLOCK / 2),
                                 EDGE * length, BLOCK,
                                 facecolor=GRIDLINE, alpha=BLOCK_ALPHA,
                                 edgecolor="none", zorder=1))
        axes.add_patch(Rectangle((start, position - BLOCK / 2),
                                 EDGE * length, BLOCK,
                                 facecolor="none", edgecolor=AXIS,
                                 linewidth=1.1, zorder=2))
        axes.text(start + EDGE * length / 2.0, position, syllable,
                  ha="center", va="center", fontsize=LABEL, color=MUTED, zorder=3)

    heading = context(config, row)
    axes.set_xlim(-margin, length + margin)
    axes.set_ylim(low, high)
    axes.set_yticks([lane for lane, _ in shown])
    axes.set_yticklabels([label for _, label in shown])
    axes.set_xticks([])
    axes.set_xticklabels([])
    for side in ("top", "right"):
        axes.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        axes.spines[side].set_color(AXIS)
    axes.tick_params(axis="y", pad=TICK)
    axes.set_title(f"{name.capitalize()} · {heading}\n{row['gamaka']}", loc="left",
                   fontsize=TITLE, color=MUTED, linespacing=1.5, pad=4.0)


def raga(config, name, rows, plots_dir, lengths, advance=None):
    rc()
    written = 0
    if advance is not None:
        advance(0, len(rows))
    for done, row in enumerate(rows, 1):
        folder = os.path.join(plots_dir, row["plot"])
        os.makedirs(folder, exist_ok=True)
        for length in lengths:
            figure, axes = plt.subplots(figsize=PANEL)
            draw(axes, config, name, row, float(length))
            figure.tight_layout()
            figure.savefig(os.path.join(folder, f"{float(length):g}.png"), dpi=DPI)
            plt.close(figure)
            written += 1
        if advance is not None:
            advance(done, len(rows))
    return written
