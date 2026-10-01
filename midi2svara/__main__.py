import os
import json
import pickle
import argparse

import yaml
from rich.console import Console
from rich.progress import Progress
from rich.table import Table

from . import extract, gamaka, intonation, plot, raga

console = Console()

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _pairs(values, cast=float):
    """Split `key=value` arguments, from the last '=' so a key may contain one."""
    out = {}
    for item in values:
        key, _, value = item.rpartition("=")
        out[key] = cast(value)
    return out


def build_config(config, taxonomy, options):
    config["rungs"] = taxonomy["rungs"]
    config["types"] = taxonomy["types"]
    config["shapes"] = taxonomy["shapes"]
    config["preference"] = taxonomy["preference"]
    config["generator"] = {
        "octave": options.octave,
        "grid": options.grid,
        "notes": options.notes,
        "tie": options.tie,
        "maxFev": options.max_fev,
        "tolerance": options.tolerance,
        "thinNotes": options.thin_notes,
        "thinMargin": options.thin_margin,
        "maxCycles": options.max_cycles,
        "rate": options.rate,
        "decay": options.decay,
        "seam": options.seam,
        "seamMinTime": options.seam_min_time,
        "seamSteps": options.seam_steps,
        "seamRelax": options.seam_relax,
        "arrive": options.arrive,
        "trimFactor": options.trim_factor,
        "widenFrom": options.widen_from,
        "widenStep": options.widen_step,
        "plateauEdge": options.plateau_edge,
        "limit": options.limit,
        "intervalEdges": options.interval_edges,
        "intervals": _pairs(options.intervals),
        "durations": _pairs(options.durations_set),
        "longBeat": options.long_beat,
        "singleLetter": options.single_letter,
        "sthayiSpan": options.sthayi_span,
        "maxDeviation": options.max_deviation,
        "histogramSigma": options.histogram_sigma,
        "minFrames": options.min_frames,
        "sthayiMarks": _pairs(options.sthayi_marks, str),
        "prominence": options.prominence,
        "maxGap": options.max_gap,
        "maxNanFraction": options.max_nan_fraction,
        "smoothing": options.smoothing,
        "minPoints": options.min_points,
    }
    config["snap"] = {
        "sigma": options.snap_sigma,
        "extremum": options.extremum_snap,
        "midpoint": options.midpoint_snap,
        "timeScale": options.time_scale,
    }
    config["plot"] = {
        "surface": options.surface,
        "ink": options.ink,
        "inkMuted": options.ink_muted,
        "gridline": options.gridline,
        "axis": options.axis,
        "reference": options.reference,
        "svaraColours": taxonomy["svaraColours"],
        "columns": options.columns,
        "panelWidth": options.panel_width,
        "panelHeight": options.panel_height,
        "dpi": options.dpi,
        "curvePoints": options.curve_points,
        "referencePoints": options.reference_points,
        "referenceCount": options.reference_count,
        "noteHeight": options.note_height,
        "noteInset": options.note_inset,
        "noteEdge": options.note_edge,
        "blockAlpha": options.block_alpha,
        "padding": options.padding,
        "margin": options.margin,
        "laneWidth": options.lane_width,
        "curveWidth": options.curve_width,
        "referenceWidth": options.reference_width,
        "referenceAlpha": options.reference_alpha,
        "pointSize": options.point_size,
        "pointEdge": options.point_edge,
        "fontFamily": options.font_family,
        "fontSans": options.font_sans,
        "fontSize": options.font_size,
        "titleSize": options.title_size,
        "titlePad": options.title_pad,
        "labelSize": options.label_size,
        "axisLabelSize": options.axis_label_size,
        "tickPad": options.tick_pad,
        "headingSize": options.heading_size,
        "subheadingSize": options.subheading_size,
        "titleTop": options.title_top,
        "titleGap": options.title_gap,
        "titleX": options.title_x,
        "layoutRect": options.layout_rect,
        "beats": options.beats,
        "markBelow": options.mark_below,
        "markAbove": options.mark_above,
        "departureArrow": options.departure_arrow,
        "intervalOrder": _pairs(options.intervals_label, int),
    }
    return config


def extract_raga(config, name, options, tonics, tempos):
    annotations_dir = os.path.join(PROJECT_DIR, options["annotationsDir"])
    pitch_dir = os.path.join(PROJECT_DIR, options["pitchTracksDir"])
    excluded = {(r, a) for r, a in (p.split(":") for p in options["excluded"])}

    offsets, frames = intonation.build(config, name, annotations_dir, pitch_dir,
                                       tonics, excluded, config["dsp"])
    targets = extract.svara_positions(config, name, offsets)
    stats = dict(total=0, kept=0, unusable=0, unparsed=0, flat=0, partial=0)
    samples = []
    artists = extract.list_artists(annotations_dir, name, excluded)

    with Progress(console=console) as progress:
        task = progress.add_task(name.capitalize(), total=len(artists))
        for artist in artists:
            progress.update(task, description=artist.capitalize())
            samples.extend(extract.extract_recording(
                config, name, artist, annotations_dir, pitch_dir,
                float(tonics[artist]), float(tempos[name][artist]), targets,
                config["dsp"], config["snap"], options["extractTolerance"], stats))
            progress.advance(task)

    return offsets, frames, samples, stats


def build(config, name, offsets, frames, samples, options):
    cases = gamaka.generate(config, name, offsets, samples,
                            workers=options["workers"],
                            tolerance=options["tolerance"])
    return {
        "raga": name,
        "version": 1,
        "units": {"pitch": "cents_from_measured_svara", "time": "normalized"},
        "scale": raga.scale(config, name),
        "intonation": {letter: round(value, 1) for letter, value in offsets.items()},
        "frames": frames,
        "cases": cases,
    }


def run(config, name, options, tonics, tempos):
    console.print(f"\n{name.capitalize()}", justify="center",
                  style=raga.colour(config, name))

    offsets, frames, samples, stats = extract_raga(config, name, options, tonics, tempos)
    console.print(f"  notes {stats['kept']}/{stats['total']}", style="dim")

    payload = build(config, name, offsets, frames, samples, options)
    cases = payload["cases"]

    res_dir = os.path.join(PROJECT_DIR, options["resDir"])
    os.makedirs(res_dir, exist_ok=True)
    path = os.path.join(res_dir, f"{name}.json")
    with open(path, "w") as f:
        json.dump(payload, f, separators=(",", ":"))

    points = sum(len(case["points"]) for case in cases)
    console.print(f"  cases {len(cases)}  changepoints {points}"
                  f"  ({points / len(cases):.1f} per case)"
                  f"  saved {path} ({os.path.getsize(path) / options['megabyte']:.2f} MB)",
                  style="dim")

    figures_dir = os.path.join(PROJECT_DIR, options["figuresDir"])
    for figure in plot.figures(config, name, offsets, samples, cases, figures_dir,
                               options["durations"]):
        console.print(f"  saved {figure}", style="dim")

    return payload


def types_table(payloads):
    names = sorted({case["type"] for payload in payloads for case in payload["cases"]},
                   key=lambda n: -sum(1 for p in payloads for c in p["cases"]
                                      if c["type"] == n))
    table = Table(title="Gamaka types", title_style="bold", header_style="bold",
                  title_justify="left")
    table.add_column("Raga")
    for name in names:
        table.add_column(name, justify="right")
    table.add_column("cases", justify="right")
    table.add_column("points/case", justify="right")

    for payload in payloads:
        cases = payload["cases"]
        counts = {name: sum(1 for case in cases if case["type"] == name) for name in names}
        points = sum(len(case["points"]) for case in cases)
        table.add_row(payload["raga"],
                      *[(str(counts[name]) if counts[name] else "-") for name in names],
                      str(len(cases)), f"{points / len(cases):.1f}")
    return table


def provenance_table(payloads, rungs):
    table = Table(title="Where each case came from", title_style="bold",
                  header_style="bold", title_justify="left")
    table.add_column("Raga")
    for rung in rungs:
        table.add_column(rung, justify="right")
    table.add_column("width elsewhere", justify="right")

    for payload in payloads:
        cases = payload["cases"]
        table.add_row(payload["raga"],
                      *[str(sum(1 for case in cases if case["provenance"] == rung))
                        for rung in rungs],
                      str(sum(1 for case in cases if case.get("widthFrom"))))
    return table


def main(config, options, tonics, tempos):
    console.print("\nmidi2svara", justify="center", style="bold")
    payloads = [run(config, name, options, tonics, tempos) for name in options["ragas"]]
    console.print()
    console.print(types_table(payloads))
    console.print()
    console.print(provenance_table(payloads, config["rungs"]))
    console.print()
    return payloads


def parse_args(argv=None):
    parser = argparse.ArgumentParser(prog="midi2svara")
    add = parser.add_argument

    add("--config", default=os.path.join(PROJECT_DIR, "config.yaml"))
    add("--shapes", default=os.path.join(PROJECT_DIR, "data", "shapes.pkl"))
    add("--tonics-file", default="tonics.yaml")
    add("--tempos-file", default="tempo.yaml")
    add("--ragas", nargs="*", default=None)
    add("--durations", nargs="*", default=["short", "long"])
    add("--workers", type=int, default=None)
    add("--annotations-dir", default="data/annotations")
    add("--pitch-tracks-dir", default="data/pitch_tracks")
    add("--res-dir", default="res")
    add("--figures-dir", default="docs/figures")
    add("--excluded", nargs="*", default=["kalyani:prasanna"])
    add("--extract-tolerance", type=float, default=6.0)
    add("--megabyte", type=float, default=1000000.0)

    add("--octave", type=float, default=1200.0)
    add("--single-letter", type=int, default=1)
    add("--sthayi-span", type=int, default=2)
    add("--max-gap", type=int, default=12)
    add("--smoothing", type=float, default=0.5)
    add("--min-points", type=int, default=4)
    add("--min-length", type=int, default=5)
    add("--prominence", type=float, default=5.0)
    add("--max-nan-fraction", type=float, default=0.3)
    add("--time-scale", type=float, default=200.0)
    add("--snap-sigma", type=float, default=45.0)
    add("--extremum-snap", type=float, default=0.7)
    add("--midpoint-snap", type=float, default=0.35)
    add("--max-deviation", type=float, default=70.0)
    add("--histogram-sigma", type=float, default=15.0)
    add("--min-frames", type=int, default=200)

    add("--grid", type=int, default=96)
    add("--notes", type=int, default=8)
    add("--tie", type=float, default=1.0)
    add("--max-fev", type=int, default=150)
    add("--tolerance", type=float, default=8.0)
    add("--thin-notes", type=int, default=3)
    add("--thin-margin", type=float, default=8.0)
    add("--max-cycles", type=float, default=24.0)
    add("--rate", type=float, nargs=2, default=[0.0, 400.0])
    add("--decay", type=float, nargs=2, default=[-0.12, 9.0])
    add("--seam", type=float, default=120.0)
    add("--seam-min-time", type=float, default=0.0167)
    add("--seam-steps", type=int, default=24)
    add("--seam-relax", type=float, default=0.6)
    add("--arrive", type=float, default=0.12)
    add("--trim-factor", type=float, default=2.5)
    add("--widen-from", type=int, default=4)
    add("--widen-step", type=float, default=0.15)
    add("--plateau-edge", type=float, default=8.0)
    add("--limit", type=float, default=60.0)
    add("--interval-edges", type=float, nargs=4, default=[-350.0, -50.0, 50.0, 350.0])
    add("--intervals", nargs="*",
        default=["vv=-400", "v=-200", "==0", "^=200", "^^=400"])
    add("--durations-set", nargs="*", default=["short=0.5", "long=2.0"])
    add("--long-beat", type=float, default=1.5)
    add("--sthayi-marks", nargs="*", default=["^=1", "_=-1"])
    add("--intervals-label", nargs="*", default=["?=0", "vv=1", "v=2", "==3", "^=4", "^^=5"])

    add("--columns", type=int, default=6)
    add("--panel-width", type=float, default=2.0)
    add("--panel-height", type=float, default=2.1)
    add("--dpi", type=int, default=150)
    add("--curve-points", type=int, default=1500)
    add("--reference-points", type=int, default=200)
    add("--reference-count", type=int, default=3)
    add("--note-height", type=float, default=68.0)
    add("--note-inset", type=float, default=0.03)
    add("--note-edge", type=float, default=1.1)
    add("--block-alpha", type=float, default=0.22)
    add("--padding", type=float, default=160.0)
    add("--margin", type=float, default=0.05)
    add("--lane-width", type=float, default=0.8)
    add("--curve-width", type=float, default=2.2)
    add("--reference-width", type=float, default=1.6)
    add("--reference-alpha", type=float, default=0.7)
    add("--point-size", type=float, default=2.9)
    add("--point-edge", type=float, default=0.9)
    add("--font-family", default="sans-serif")
    add("--font-sans", nargs="*",
        default=["DejaVu Sans", "Segoe UI", "Helvetica", "Arial"])
    add("--font-size", type=int, default=9)
    add("--title-size", type=int, default=8)
    add("--title-pad", type=float, default=4.0)
    add("--label-size", type=float, default=8.5)
    add("--axis-label-size", type=int, default=8)
    add("--tick-pad", type=float, default=4.0)
    add("--heading-size", type=int, default=15)
    add("--subheading-size", type=float, default=8.5)
    add("--title-top", type=float, default=0.997)
    add("--title-gap", type=float, default=0.45)
    add("--title-x", type=float, default=0.01)
    add("--layout-rect", type=float, nargs=4, default=[0.01, 0.03, 0.99, 0.98])
    add("--beats", type=float, nargs="*", default=[0.0, 0.5, 1.0])
    add("--mark-below", default="̱")
    add("--mark-above", default="̇")
    add("--departure-arrow", default=" →")
    add("--surface", default="#fcfcfb")
    add("--ink", default="#0b0b0b")
    add("--ink-muted", default="#898781")
    add("--gridline", default="#e1e0d9")
    add("--axis", default="#c3c2b7")
    add("--reference", default="#b3b1a8")
    return parser.parse_args(argv)


def cli(argv=None):
    args = parse_args(argv)

    global config
    with open(args.config) as f:
        config = yaml.safe_load(f)

    options = {
        "ragas": args.ragas or raga.ragas(config),
        "durations": args.durations,
        "workers": args.workers,
        "annotationsDir": args.annotations_dir,
        "pitchTracksDir": args.pitch_tracks_dir,
        "resDir": args.res_dir,
        "figuresDir": args.figures_dir,
        "excluded": args.excluded,
        "extractTolerance": args.extract_tolerance,
        "megabyte": args.megabyte,
        "shapes": args.shapes,
        "tonicsFile": args.tonics_file,
        "temposFile": args.tempos_file,
        "tolerance": args.tolerance,
    }
    with open(args.shapes, "rb") as f:
        taxonomy = pickle.load(f)
    config = build_config(config, taxonomy, args)
    config["dsp"] = {
        "maxGap": args.max_gap,
        "smoothing": args.smoothing,
        "minPoints": args.min_points,
        "minLength": args.min_length,
        "centsPerOctave": args.octave,
    }

    annotations = os.path.join(PROJECT_DIR, options["annotationsDir"])
    with open(os.path.join(annotations, options["tonicsFile"])) as f:
        tonics = yaml.safe_load(f)
    with open(os.path.join(annotations, options["temposFile"])) as f:
        tempos = yaml.safe_load(f)

    return main(config, options, tonics, tempos)


if __name__ == "__main__":
    cli()
