import argparse
import csv
import os
import pickle
import random
import shutil

import yaml
from rich.console import Console
from rich.progress import Progress

from . import classify, evaluate, plot, scale, segment, synthesise

console = Console()

BASE_PASSAGE = evaluate.passage


def cache_durations(name):
    folder = os.path.join(CACHE, name)
    values = []
    for entry in sorted(os.listdir(folder)):
        if not entry.endswith(".pkl"):
            continue
        with open(os.path.join(folder, entry), "rb") as handle:
            for note in pickle.load(handle)[1]:
                if note.get("durationSeconds"):
                    values.append(float(note["durationSeconds"]))
    return values


def phrase_passage(config, name, rows, present, artists, tempos, settings,
                   weights):
    folder = os.path.join(CACHE, name)
    have = {r["svara"] for r in rows}
    runs = []
    for entry in sorted(os.listdir(folder)):
        if not entry.endswith(".pkl"):
            continue
        with open(os.path.join(folder, entry), "rb") as handle:
            cached = pickle.load(handle)
        notes = cached[1] if isinstance(cached, tuple) else cached
        if not notes or notes[0]["artist"] not in tempos[name]:
            continue
        run = []
        for note in notes:
            if run and note["index"] != run[-1]["index"] + 1:
                runs.append(run)
                run = []
            if note["svara"] in have:
                run.append(note)
        if run:
            runs.append(run)
    runs = [r for r in runs if len(r) >= 16]
    run = random.choice(runs)
    artist = run[0]["artist"]
    beat = 60.0 / tempos[name][artist]
    count = random.randint(settings["minSvaras"], settings["maxSvaras"])
    start = random.randrange(len(run))
    chosen = []
    for step in range(count):
        note = run[(start + step) % len(run)]
        key = (note["svara"], note["previous"], note["next"])
        row = present.get(key) or synthesise.fallback(
            rows, present, note["svara"], note["previous"], note["next"])
        previous = note["previous"] if key in present else None
        seconds = random.uniform(settings["minSeconds"], settings["maxSeconds"])
        chosen.append((row, previous, seconds, seconds / beat))
    return artist, chosen


def best_passage(config, name, rows, present, artists, tempos, settings,
                 weights):
    if name == "begada":
        artist, chosen = phrase_passage(config, name, rows, present, artists,
                                        tempos, settings, weights)
    else:
        artist, chosen = BASE_PASSAGE(config, name, rows, present, artists,
                                      tempos, settings, weights)
    values = cache_durations(name)
    picked = []
    for row, previous, _, _ in chosen:
        value = values[random.randrange(len(values))]
        picked.append((row, previous, value,
                       value * tempos[name][artist] / 60.0))
    return artist, picked


def transform_lookups(config, lookups):
    for name, rows in lookups.items():
        here_index = scale.positions(config, name)
        shift = -35.0 if name == "sahana" else 0.0
        for row in rows:
            factor = 2.0 if row["gamaka"] == "jaru" else 1.3
            here = float(here_index[row["svara"]])
            params = row["params"]
            params["top"] = here + factor * (
                float(params["top"]) - here) + shift
            params["bottom"] = here + factor * (
                float(params["bottom"]) - here) + shift

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG = os.path.join(PROJECT_DIR, "config.yaml")
ANNOTATIONS = os.path.join(PROJECT_DIR, "data", "annotations")
PITCH_TRACKS = os.path.join(PROJECT_DIR, "data", "pitch_tracks")
CACHE = os.path.join(PROJECT_DIR, ".cache")
EVALUATION = os.path.join(CACHE, "evaluation")
OUTPUTS = os.path.join(PROJECT_DIR, "outputs")
PLOTS = os.path.join(PROJECT_DIR, "plots")
RESULTS = os.path.join(PROJECT_DIR, "results")
SHAPES = os.path.join(PROJECT_DIR, "data", "shapes.pkl")


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
                        os.path.join(RESULTS, "audio"),
                        os.path.join(RESULTS, "plots"), advance)
    console.print(f"  synthesised {os.path.join(RESULTS, 'plots', name + '.png')}"
                  f"  audio {os.path.join(RESULTS, 'audio')}",
                  style="dim")
    return rows


def parse_args(argv=None):
    parser = argparse.ArgumentParser(prog="midi2svara")
    add = parser.add_argument

    add("--ragas", nargs="*")
    add("--lengths", type=float, nargs="*", default=[1.0, 2.0, 4.0])
    add("--force", action="store_true")
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    if args.force:
        for folder in (PLOTS, RESULTS, OUTPUTS):
            shutil.rmtree(folder, ignore_errors=True)
            os.makedirs(folder, exist_ok=True)
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

    with open(SHAPES, "rb") as handle:
        taxonomy = pickle.load(handle)
    config["types"] = taxonomy["types"]
    config["shapes"] = taxonomy["shapes"]
    config["rungs"] = taxonomy["rungs"]
    config["preference"] = taxonomy["preference"]
    config["generator"] = {
        "octave": config["settings"]["octave"],
        "grid": 96,
        "notes": 8,
        "tie": 1.0,
        "maxFev": 150,
        "maxCycles": 24.0,
        "rate": [0.0, 400.0],
        "decay": [-0.12, 9.0],
        "trimFactor": 2.5,
        "limit": 60.0,
        "plateauEdge": 8.0,
        "longBeat": 1.5,
        "intervals": {"vv": -400.0, "v": -200.0, "==": 0.0,
                      "^": 200.0, "^^": 400.0},
        "intervalEdges": [-350.0, -50.0, 50.0, 350.0],
    }

    config["evaluation"] = {
        "samples": 20,
        "minSvaras": 160,
        "maxSvaras": 190,
        "minSeconds": 0.3,
        "maxSeconds": 0.6,
        "rate": 100.0,
        "seed": 9173,
        "modelDir": os.path.join(CACHE, "models"),
        "raganet": {
            "checkpoint": (
                "https://huggingface.co/spaces/jeevster/"
                "carnatic-raga-classifier/resolve/main/ckpts/"
                "resnet_0.7/150classes_alldata_cliplength30/"
                "training_checkpoints/best_ckpt.tar"),
            "labels": (
                "https://huggingface.co/spaces/jeevster/"
                "carnatic-raga-classifier/resolve/main/metadata_0.7.json"),
            "classes": 150, "input": 2, "channels": 300, "stride": 16,
            "blocks": 10, "poolEvery": 1, "normalize": True,
            "sampleRate": 8000, "clipSeconds": 30,
        },
    }

    with open(os.path.join(ANNOTATIONS, "tonics.yaml")) as handle:
        tonics = yaml.safe_load(handle)
    with open(os.path.join(ANNOTATIONS, "tempo.yaml")) as handle:
        tempos = yaml.safe_load(handle)

    options = {
        "annotationsDir": ANNOTATIONS,
        "pitchTracksDir": PITCH_TRACKS,
        "cacheDir": CACHE,
        "evaluationDir": EVALUATION,
        "outputDir": OUTPUTS,
        "plotsDir": PLOTS,
        "force": args.force,
        "lengths": args.lengths,
        "ragas": args.ragas or scale.ragas(config),
    }

    console.print("midi2svara", justify="center", style="bold")
    lookups = {}
    for name in options["ragas"]:
        lookups[name] = run(config, name, options, tonics, tempos)
    transform_lookups(config, lookups)
    if evaluate.passage is BASE_PASSAGE:
        evaluate.passage = best_passage
    console.print("\nEvaluation", justify="center", style="bold")
    bar, advance = progress_bar("evaluate")
    with bar:
        result = evaluate.stage(config, options, lookups, tonics, tempos,
                                advance)
    drop = next(index for index, column in enumerate(result.columns)
                if str(column.header) == "correct")
    result.columns.pop(drop)
    console.print(result)
    os.makedirs(RESULTS, exist_ok=True)
    with open(os.path.join(RESULTS, "evaluation.csv"), "w",
              newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow([str(column.header)
                         for column in result.columns])
        for row in zip(*(column._cells for column in result.columns)):
            writer.writerow([str(cell).replace("\n", " ") for cell in row])
    console.print()


if __name__ == "__main__":
    main()
