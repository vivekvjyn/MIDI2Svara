import argparse
import os

import yaml
from rich.console import Console
from rich.progress import Progress

from . import classify, plot, scale, segment

console = Console()

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _pairs(values, cast=float):
    out = {}
    for item in values:
        key, _, value = item.rpartition("=")
        out[key] = cast(value)
    return out


def _path(value):
    return value if os.path.isabs(value) else os.path.join(PROJECT_DIR, value)


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


def parse_args(argv=None):
    parser = argparse.ArgumentParser(prog="midi2svara")
    add = parser.add_argument

    add("--config", default=os.path.join(PROJECT_DIR, "config.yaml"))
    add("--ragas", nargs="*")
    add("--cache-dir", default=".cache")
    add("--output-dir", default="outputs")
    add("--plots-dir", default="plots")
    add("--annotations-dir", default="data/annotations")
    add("--pitch-tracks-dir", default="data/pitch_tracks")
    add("--tonics-file", default="tonics.yaml")
    add("--tempo-file", default="tempo.yaml")
    add("--excluded", nargs="*", default=["kalyani:prasanna"])
    add("--lengths", type=float, nargs="*", default=[1.0, 2.0, 4.0])
    add("--svara-names", nargs="*",
        default=["S=Sa", "R=Ri", "G=Ga", "M=Ma", "P=Pa", "D=Da", "N=Ni"])
    add("--force", action="store_true")
    add("--octave", type=float, default=1200.0)
    add("--single-letter", type=int, default=1)
    add("--sthayi-span", type=int, default=2)
    add("--sthayi-marks", nargs="*", default=["^=1", "_=-1"])
    add("--max-gap", type=int, default=12)
    add("--smoothing", type=float, default=0.5)
    add("--min-points", type=int, default=4)
    add("--min-length", type=int, default=5)
    add("--max-nan-fraction", type=float, default=0.3)
    return parser.parse_args(argv)


def cli(argv=None):
    args = parse_args(argv)
    with open(args.config) as handle:
        config = yaml.safe_load(handle)

    config["settings"] = {
        "octave": args.octave,
        "singleLetter": args.single_letter,
        "sthayiSpan": args.sthayi_span,
        "sthayiMarks": _pairs(args.sthayi_marks, int),
        "maxGap": args.max_gap,
        "smoothing": args.smoothing,
        "minPoints": args.min_points,
        "minLength": args.min_length,
        "maxNanFraction": args.max_nan_fraction,
    }
    config["names"] = _pairs(args.svara_names, str)

    annotations = _path(args.annotations_dir)
    with open(os.path.join(annotations, args.tonics_file)) as handle:
        tonics = yaml.safe_load(handle)
    with open(os.path.join(annotations, args.tempo_file)) as handle:
        tempos = yaml.safe_load(handle)

    options = {
        "annotationsDir": annotations,
        "pitchTracksDir": _path(args.pitch_tracks_dir),
        "cacheDir": _path(args.cache_dir),
        "outputDir": _path(args.output_dir),
        "plotsDir": _path(args.plots_dir),
        "excluded": {(r, a) for r, a in (p.split(":") for p in args.excluded)},
        "force": args.force,
        "lengths": args.lengths,
        "ragas": args.ragas or scale.ragas(config),
    }

    console.print("midi2svara", justify="center", style="bold")
    for name in options["ragas"]:
        run(config, name, options, tonics, tempos)
    console.print()


if __name__ == "__main__":
    cli()
