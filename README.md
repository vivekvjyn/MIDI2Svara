# MIDI2svara

Turn MIDI to svara based on its melodic context. This branch generates the lookup table for each context case to synthesise the gamaka.

## Setup

```bash
git clone https://github.com/vivekvjyn/MIDI2Svara.git
cd MIDI2Svara
git checkout generator
pip install -r requirements.txt
```


## Run

```bash
./run.sh                          # all ragas
./run.sh --ragas kalyani          # one raga
```

## License

MIT License — see [LICENSE](LICENSE) for details.
