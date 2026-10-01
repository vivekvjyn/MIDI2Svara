# midi2svara

Turns annotated Carnatic recordings into the gamaka changepoints a plugin can play.

Given a svara, the interval it was approached from, the interval it departs to, and
how long it was held, it decides which Carnatic gamaka that context uses, how wide
this raga's version of it is, and which points of the pitch curve carry it. It does
that for every such context in every raga, whether or not anybody happened to sing
that one, and plots each case against the performances it was measured from.

## Install

```bash
pip install -r requirements.txt
pip install -e .
```

## Run

Everything is set in `run.sh`, which passes each value to the module as an
argument:

```bash
./run.sh                                    # all ragas
./run.sh --ragas kalyani                    # one raga
./run.sh --ragas kalyani --durations short  # short notes only
```

Needs `data/annotations/`, `data/pitch_tracks/`, `data/tonics.yaml`,
`data/tempo.yaml` and `data/shapes.pkl`. Writes one table per raga to `res/` and
one figure per raga, duration and svara to `docs/figures/`.

## Layout

```
run.sh              every constant, passed to the module as an argument
config.yaml         the ragas: svaras, positions, colours
tools/
  make_shapes.py    writes data/shapes.pkl, the gamaka taxonomy
data/
  shapes.pkl        the taxonomy: names, shapes, bounds, preferences
  annotations/      svara annotations per raga
  pitch_tracks/     pitch tracks per raga
midi2svara/
  __main__.py       argument parsing, tables
  gamaka.py         taxonomy, identification, changepoints, the ladder
  plot.py           one figure per raga, duration and svara
  extract.py        annotations and pitch tracks -> one contour per note
  dsp.py            pitch-track repair and extrema
  intonation.py     measured svarasthana per raga
  raga.py           raga lookups
```

`data/`, `res/` and `docs/` are all generated or supplied, and none is in the
repository.

## How a case is decided

**Name it.** Every gamaka in `data/shapes.pkl` is fitted to every note of the
context and the residuals summed; the lowest BIC wins. Fitting per note matters:
the notes in one context oscillate at different phases, so their mean has a
fraction of the excursion any of them actually had, and an ornament averaged out of
existence is then fitted beautifully by whichever type is nearest a flat line.

**Widen it.** The median reach across the notes, in scale steps. A reach is measured
in scale steps and resolved against the raga's own measured svarasthanas, so a reach
of 1.0 is the neighbouring svara however wide that raga makes it — in Saveri the step
from Re to Ga is 300 cents where in Kalyani it is 200.

**Draw it.** The type is reduced to the changepoints that carry it, to within the
tolerance, measured against the spline the plugin draws between them.
Douglas-Peucker is the obvious choice and is wrong here: an oscillation sits on a
straight chord, so it discards the ornament.

**Join it.** A returning gamaka is eased onto its own svara over the last stretch of
the note; a leaving one keeps the neighbour it was leaving towards. The two sides of
a boundary therefore agree by construction.

**Fall back.** Around two thirds of the context space has no evidence at all, so a
ladder backs off from both neighbours out to the raga as a whole, and every case
records which rung it was decided on. A case measured from its own context and a
case inherited from the raga in general are different claims and the output says
which.

## Output

`res/<raga>.json` holds one record per case:

```json
{
  "svara": "G", "svarasthana": 382,
  "previousInterval": 0, "nextInterval": -200,
  "durationName": "short", "durationBeats": 0.5,
  "type": "andola", "params": {"centre": -12.4, "reach": 2.1, "cycles": 0.6},
  "widthSteps": 2.12, "widthCents": 428.0,
  "rmse": 31.2, "margin": 544.9, "runnerUp": "kampita",
  "provenance": "departure", "evidence": 44,
  "seamOut": 0.0,
  "points": [[0.0, 11.1], [0.474, 19.7], [1.0, 0.0]]
}
```

`points` is the changepoints, as `(normalised time, cents from the measured svara)`.
`params` travels with them so a case can be re-rendered at a length other than the
one it was decided for, which is the only reason to hold a rate per beat.
`provenance` is the rung of the ladder; `widthFrom` appears when the width was taken
from a broader one.