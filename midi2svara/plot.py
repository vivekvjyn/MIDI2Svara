import os

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

from . import gamaka, raga


def _style(config):
    surface = config["plot"]["surface"]
    return {
        "font.family": config["plot"]["fontFamily"],
        "font.sans-serif": config["plot"]["fontSans"],
        "figure.facecolor": surface,
        "axes.facecolor": surface,
        "savefig.facecolor": surface,
        "axes.edgecolor": config["plot"]["axis"],
        "axes.labelcolor": config["plot"]["inkMuted"],
        "xtick.color": config["plot"]["inkMuted"],
        "ytick.color": config["plot"]["inkMuted"],
        "text.color": config["plot"]["ink"],
        "axes.linewidth": config["plot"]["laneWidth"],
        "xtick.major.size": 0,
        "ytick.major.size": 0,
        "font.size": config["plot"]["fontSize"],
    }


def _label(config, name, cents):
    octave = config["generator"]["octave"]
    for letter, base in raga.svarasthanas(config, name).items():
        for shift, mark in ((-octave, config["plot"]["markBelow"]),
                            (0, ""),
                            (octave, config["plot"]["markAbove"])):
            if base + shift == cents:
                return letter + mark
    return ""


def _draw(axes, config, name, case, scale, observed, svara, style):
    plot = config["plot"]
    grid = np.linspace(0.0, 1.0, style["curvePoints"])
    points = np.array(case["points"], dtype=float)
    contour = gamaka.spline(points[:, 0], points[:, 1], grid)

    lanes = list(scale.svarasthanas().values())
    low = min(contour.min(), observed.min() if len(observed) else 0.0) - plot["padding"]
    high = max(contour.max(), observed.max() if len(observed) else 0.0) + plot["padding"]

    for lane in lanes:
        if low <= lane <= high:
            axes.axhline(lane, color=style["gridline"],
                          linewidth=plot["laneWidth"], zorder=0)
    for beat in style["beats"]:
        axes.axvline(beat, color=style["gridline"],
                     linewidth=plot["laneWidth"], zorder=0)

    height = plot["noteHeight"]
    inset = plot["noteInset"]
    colour = style["svaraColours"].get(case["svara"], style["inkMuted"])
    axes.add_patch(Rectangle((inset, svara - height / 2), 1.0 - 2 * inset, height,
                             facecolor=colour, alpha=plot["blockAlpha"],
                             edgecolor="none", zorder=1))
    axes.add_patch(Rectangle((inset, svara - height / 2), 1.0 - 2 * inset, height,
                             facecolor="none", edgecolor=colour,
                             linewidth=plot["noteEdge"], zorder=2))
    axes.text(0.5, svara, case["svara"], ha="center", va="center",
              fontsize=plot["labelSize"], color=style["inkMuted"], zorder=3)

    for row in observed:
        axes.plot(row, color=style["reference"],
                  linewidth=plot["referenceWidth"], alpha=plot["referenceAlpha"],
                  solid_capstyle="round", zorder=3.5)

    axes.plot(grid, contour, color=style["ink"],
              linewidth=plot["curveWidth"], solid_capstyle="round", zorder=4)
    axes.plot(points[:, 0], points[:, 1], linestyle="none", marker="o",
              markersize=plot["pointSize"], markerfacecolor=style["surface"],
              markeredgecolor=style["ink"], markeredgewidth=plot["pointEdge"], zorder=5)

    axes.set_xlim(-style["margin"], 1.0 + style["margin"])
    axes.set_ylim(low, high)
    axes.set_yticks(lanes)
    axes.set_yticklabels([_label(config, name, c) for c in lanes])
    axes.set_xticks(list(style["beats"]))
    axes.set_xticklabels([""] * len(style["beats"]))
    for side in ("top", "right"):
        axes.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        axes.spines[side].set_color(style["axis"])
    axes.tick_params(axis="both", pad=plot["tickPad"])

    axes.set_title(f"{case['type']} · {case['widthSteps']:.1f}st · n={case['evidence']}",
                   loc="left", fontsize=plot["titleSize"], color=style["inkMuted"],
                   pad=plot["titlePad"])


def _observed(config, samples, case, scale):
    grid = np.linspace(0.0, 1.0, config["plot"]["referencePoints"])
    here = case["svarasthana"]
    rows = []
    for sample in samples:
        if int(scale.position(sample["svarasthana"], 0)) != int(here):
            continue
        times = np.asarray(sample["normalizedTime"], dtype=float)
        if len(times) < 2:
            continue
        rows.append(np.interp(grid, times, scale.reframe(sample["svarasthana"],
                                                         sample["pitchOffsetCents"])))
    if not rows:
        return np.zeros((0, len(grid)))
    excursion = np.array([np.ptp(row) for row in rows])
    chosen = np.argsort(np.abs(excursion - np.median(excursion)))[
        :config["plot"]["referenceCount"]]
    return np.array([rows[i] for i in np.sort(chosen)])


def _order(config, case):
    ranks = config["plot"]["intervalOrder"]
    return (ranks.get(gamaka.interval_class(config, case["previousInterval"]),
                      len(ranks)),
            ranks.get(gamaka.interval_class(config, case["nextInterval"]),
                      len(ranks)))


def figures(config, name, offsets, samples, cases, output_dir, durations):
    scale = gamaka.Scale(config, name, offsets)
    plot = config["plot"]
    style = {
        "surface": plot["surface"],
        "ink": plot["ink"],
        "inkMuted": plot["inkMuted"],
        "gridline": plot["gridline"],
        "axis": plot["axis"],
        "reference": plot["reference"],
        "svaraColours": plot["svaraColours"],
        "curvePoints": plot["curvePoints"],
        "beats": plot["beats"],
        "margin": plot["margin"],
    }
    columns = plot["columns"]
    os.makedirs(output_dir, exist_ok=True)
    written = []

    for duration in durations:
        chosen = [c for c in cases if duration is None or c["durationName"] == duration]
        if not chosen:
            continue

        for letter in sorted({c["svara"] for c in chosen}, key=scale.svarasthanas().get):
            group = sorted([c for c in chosen if c["svara"] == letter],
                           key=lambda c: _order(config, c))
            rows = int(np.ceil(len(group) / columns))
            total = rows * columns

            plt.rcParams.update(_style(config))
            figure_, axes_list = plt.subplots(
                rows, columns,
                figsize=(plot["panelWidth"] * columns, plot["panelHeight"] * rows),
                squeeze=False)
            axes_list = axes_list.ravel()
            svara = scale.svarasthanas()[letter]

            for position, case in enumerate(group):
                axes = axes_list[position]
                _draw(axes, config, name, case, scale,
                      _observed(config, samples, case, scale), svara, style)
                if position % columns == 0:
                    axes.set_ylabel(gamaka.interval_class(
                        config, case["previousInterval"]),
                        fontsize=plot["axisLabelSize"])
                if position // columns == 0:
                    axes.set_xlabel(gamaka.interval_class(
                        config, case["nextInterval"]) + plot["departureArrow"],
                        fontsize=plot["axisLabelSize"])

            for axes in axes_list[len(group):total]:
                axes.set_visible(False)

            figure_.tight_layout(rect=tuple(plot["layoutRect"]))
            top = plot["titleTop"]
            figure_.text(plot["titleX"], top,
                         f"{name.capitalize()} · {letter}", ha="left", va="top",
                         fontsize=plot["headingSize"], color=style["ink"],
                         fontweight="bold")
            figure_.text(plot["titleX"], top - plot["titleGap"] / figure_.get_figheight(),
                         f"{duration} notes · {len(group)} contexts", ha="left",
                         va="top", fontsize=plot["subheadingSize"],
                         color=style["inkMuted"])

            suffix = "" if duration is None else f"_{duration}"
            path = os.path.join(output_dir, f"{name}_{letter.lower()}{suffix}.png")
            figure_.savefig(path, dpi=plot["dpi"])
            plt.close(figure_)
            written.append(path)

    return written
