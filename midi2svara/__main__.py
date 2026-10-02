import argparse
import os

import yaml
from rich.console import Console
from rich.progress import Progress

from . import classify, plot, scale, segment, synthesise

console = Console()

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG = os.path.join(PROJECT_DIR, "config.yaml")
ANNOTATIONS = os.path.join(PROJECT_DIR, "data", "annotations")
PITCH_TRACKS = os.path.join(PROJECT_DIR, "data", "pitch_tracks")
CACHE = os.path.join(PROJECT_DIR, ".cache")
OUTPUTS = os.path.join(PROJECT_DIR, "outputs")
PLOTS = os.path.join(PROJECT_DIR, "plots")


def progress_bar(label):
    bar = Progress(console=console)
    task = None

    def advance(done, total):
        nonlocal task
        if task is None:
            task = bar.add_task(f"  {label}", total=max(total, 1))
        bar.update(task, completed=done)

    return bar, advance


def run(config, name, options, tonics, tempos):
    console.print(f"\n{name.capitalize()}", justify="center",
                  style=config["ragas"][name]["color"])

    bar, advance = progress_bar(f"{name.capitalize()} · segments")
    with bar:
        notes = segment.raga(config, name, options, tonics, tempos, advance)
    console.print(f"  segments {len(notes)}  cache {options['cacheDir']}/{name}",
                  style="dim")

    bar, advance = progress_bar(f"{name.capitalize()} · classify")
    with bar:
        rows = classify.raga(config, name, notes, options["outputDir"],
                             options["force"], advance)
    console.print(f"  cases {len(rows)}  lookup "
                  f"{os.path.join(options['outputDir'], name + '_lookup.json')}",
                  style="dim")

    bar, advance = progress_bar(f"{name.capitalize()} · plots")
    with bar:
        written = plot.raga(config, name, rows, options["plotsDir"],
                            options["lengths"], advance)
    console.print(f"  plots {written} in {os.path.join(options['plotsDir'], name)}",
                  style="dim")

    bar, advance = progress_bar(f"{name.capitalize()} · synthesise")
    with bar:
        synthesise.raga(config, name, notes, rows, tonics,
                        options["outputDir"], options["plotsDir"], advance)
    console.print(f"  synthesised {os.path.join(options['plotsDir'], 'synthesised', name + '.png')}"
                  f"  audio {os.path.join(options['outputDir'], 'synthesised')}",
                  style="dim")


def parse_args(argv=None):
    parser = argparse.ArgumentParser(prog="midi2svara")
    add = parser.add_argument

    add("--ragas", nargs="*")
    add("--lengths", type=float, nargs="*", default=[1.0, 2.0, 4.0])
    add("--force", action="store_true")
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    with open(CONFIG) as handle:
        config = yaml.safe_load(handle)

    config["settings"] = {
        "octave": 1200.0,
        "singleLetter": 1,
        "sthayiSpan": 2,
        "sthayiMarks": {"^": 1, "_": -1},
        "maxGap": 12,
        "smoothing": 0.5,
        "minPoints": 4,
        "minLength": 5,
        "maxNanFraction": 0.3,
    }
    config["names"] = {}

    with open(os.path.join(ANNOTATIONS, "tonics.yaml")) as handle:
        tonics = yaml.safe_load(handle)
    with open(os.path.join(ANNOTATIONS, "tempo.yaml")) as handle:
        tempos = yaml.safe_load(handle)

    options = {
        "annotationsDir": ANNOTATIONS,
        "pitchTracksDir": PITCH_TRACKS,
        "cacheDir": CACHE,
        "outputDir": OUTPUTS,
        "plotsDir": PLOTS,
        "force": args.force,
        "lengths": args.lengths,
        "ragas": args.ragas or scale.ragas(config),
    }

    console.print("midi2svara", justify="center", style="bold")
    for name in options["ragas"]:
        run(config, name, options, tonics, tempos)
    console.print()


if __name__ == "__main__":
    main()
