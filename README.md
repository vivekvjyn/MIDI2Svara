# MIDI2Svara

A pitch-expression plugin for Carnatic music, and the generator that feeds it.

The plugin takes a sequence of svaras and synthesises the *gamaka* — the
pitch ornamentation — a performer would apply to them. The generator decides what
that ornament should be: for every melodic context in every raga, it names the
gamaka, measures how wide this raga's version of it is, and writes the changepoints
that draw it.

The two live on separate branches, because they have almost nothing in common:

| Branch | |
|---|---|
| `main` | the JUCE plugin. C++, CMake, GoogleTest. |
| `generator` | the Python module that produces the tables the plugin reads. |

## The plugin

```bash
git clone --recurse-submodules https://github.com/vivekvjyn/MIDI2Svara.git
cd MIDI2Svara
git checkout main

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Builds VST3, AU and Standalone. The gamaka tables in `res/` are embedded at compile
time, so the generator has to run before the plugin will build — see the branch
below.

## The generator

```bash
git checkout generator
pip install -r requirements.txt

./run.sh                                    # all ragas
./run.sh --ragas kalyani                    # one raga
./run.sh --ragas kalyani --durations short  # short notes only
```

Needs the annotations and pitch tracks under `data/`. Writes one table per raga to
`res/` and one figure per raga, duration and svara to `docs/figures/`.

Every constant is set in `run.sh` and reaches the module as a command-line
argument. `config.yaml` holds the ragas; `data/shapes.pkl` holds the gamaka
taxonomy, written by `tools/make_shapes.py`.

See the generator branch's README for how a case is decided and what the output
looks like.

## Method

The gamaka a svara receives depends on its melodic context, so the engine matches
on the svara together with the intervals to its neighbours, its duration, and the
requested intensity. A svara's melodic context is what determines its form — that is
the finding the coarticulation literature on Indian art music rests on, and it is
what the generator's case space is built around.

## References

```bibtex
@article{pearson_coarticulation_2016,
    title = {Coarticulation and gesture: An analysis of melodic movement in {South} {Indian} raga performance},
    volume = {35},
    url = {https://doi.org/10.1111/musa.12071},
    number = {3},
    journal = {Music Analysis},
    author = {Pearson, Lara},
    year = {2016},
    pages = {280--313}
}

@article{Nuttall2025SvaraFormsIC,
  title={Svara-Forms in Carnatic Music: Contextual Influences on the Performance of Svara},
  author={Thomas Nuttall and Xavier Serra and Lara Pearson},
  journal={2025 IEEE International Conference on Acoustics, Speech, and Signal Processing Workshops (ICASSPW)},
  year={2025},
  pages={1-5},
  url={https://api.semanticscholar.org/CorpusID:278937114}
}
```