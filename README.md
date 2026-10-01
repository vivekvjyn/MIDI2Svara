# MIDI2Svara

A pitch-expression plugin for Carnatic music, and the generator that feeds it. The
plugin synthesises the *gamaka* a performer would apply to a sequence of svaras.
The generator decides what that ornament should be for every melodic context in
every raga.

Two branches: `main` is the plugin, `generator` is the Python module.

## Setup

```bash
git clone --recurse-submodules https://github.com/vivekvjyn/MIDI2Svara.git
cd MIDI2Svara
git checkout main
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

## Generator

```bash
git checkout generator
pip install -r requirements.txt
./run.sh --ragas kalyani
```

Needs `data/annotations/` and `data/pitch_tracks/`. Writes `res/` and `docs/figures/`.

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
```