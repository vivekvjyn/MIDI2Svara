# midi2svara

Turns annotated Carnatic recordings into gamaka changepoints. For a svara, the
interval it arrived from, the interval it departs to, and how long it was held, it
names the gamaka, measures its width, and writes the changepoints that draw it.

## Setup

```bash
git clone https://github.com/vivekvjyn/MIDI2Svara.git
cd MIDI2Svara
git checkout generator
pip install -r requirements.txt
```

Needs `data/annotations/` and `data/pitch_tracks/`.

## Running

```bash
./run.sh                          # all ragas
./run.sh --ragas kalyani          # one raga
./run.sh --durations short        # short notes only
```

Every constant is set in `run.sh` and passed to the module as an argument.
`config.yaml` holds the ragas; `data/shapes.pkl` holds the taxonomy, written by
`tools/make_shapes.py`. Writes `res/` and `docs/figures/`.

## Output

`res/<raga>.json`, one record per melodic context:

```json
{
  "svara": "G", "svarasthana": 382,
  "previousInterval": 0, "nextInterval": -200,
  "durationName": "short", "durationBeats": 0.5,
  "type": "andola", "widthSteps": 2.12, "widthCents": 428.0,
  "rmse": 31.2, "margin": 544.9, "runnerUp": "kampita",
  "provenance": "departure", "evidence": 44,
  "seamOut": 0.0,
  "points": [[0.0, 11.1], [0.474, 19.7], [1.0, 0.0]]
}
```

`provenance` is the rung of the fallback ladder the case was decided on. `points`
are `(normalised time, cents from the measured svara)`.