import json
import os
import pickle
import random
import shutil
import sys
import types
import unicodedata
import urllib.request
from collections import Counter

import librosa
import numpy as np
import torch
from compiam.utils.pitch import resampling
from rich.table import Table
from scipy.io import wavfile

from . import plot, scale, synthesise


def plain(text):
    decomposed = unicodedata.normalize("NFD", str(text))
    letters = "".join(c for c in decomposed
                      if unicodedata.category(c) != "Mn")
    return letters.casefold().replace(" ", "")


def occurrence(folder, name, rows):
    tally = Counter()
    path = os.path.join(folder, name)
    for entry in sorted(os.listdir(path)):
        if not entry.endswith(".pkl"):
            continue
        with open(os.path.join(path, entry), "rb") as handle:
            notes = pickle.load(handle)[1]
        for note in notes:
            tally[(note["svara"], note["previous"], note["next"])] += 1
    return [tally.get((r["svara"], r["previous"], r["next"]), 1) for r in rows]


def passage(config, name, rows, present, artists, tempos, settings, weights):
    artist = random.choice(artists)
    beat = 60.0 / tempos[name][artist]
    count = random.randint(settings["minSvaras"], settings["maxSvaras"])
    by_svara = {}
    for offset, row in enumerate(rows):
        by_svara.setdefault(row["svara"], []).append(offset)
    chosen = []
    offset = random.choices(range(len(rows)), weights)[0]
    previous = None
    while len(chosen) < count:
        seconds = random.uniform(settings["minSeconds"], settings["maxSeconds"])
        row = rows[offset]
        chosen.append((row, previous, seconds, seconds / beat))
        previous = row["svara"]
        target = by_svara.get(row["next"], [])
        if target and random.random() < 0.6:
            exact = [i for i in target if rows[i]["previous"] == previous]
            picks = exact or target
            offset = random.choices(picks, [weights[i] for i in picks])[0]
        else:
            offset = random.choices(range(len(rows)), weights)[0]
        if rows[offset]["previous"] != previous:
            previous = None
    return artist, chosen


def contour(config, chosen, positions):
    times, cents, marks = [], [], []
    start = 0.0
    settings = config.get("evaluation") or {}
    every = settings.get("restEvery")
    lengths = settings.get("restSeconds") or []
    for index, (row, previous, seconds, beats) in enumerate(chosen):
        if every and index and index % every == 0 and lengths:
            start += lengths[(index // every - 1) % len(lengths)]
        here = positions[row["svara"]]
        origin = positions[previous] if previous is not None else None
        shape = np.asarray(plot.curve(config, row, beats, here, origin),
                           dtype=float)
        times.append(
            start + np.linspace(0.0, seconds, len(shape), endpoint=False))
        cents.append(shape)
        marks.append(start)
        start += seconds
    return (np.concatenate(times), np.concatenate(cents), marks, start)


def write(folder, index, times, cents, marks, total, tonic, rate):
    grid, flat = synthesise.connect(times, cents, marks, total)
    frequencies = float(tonic) * 2.0 ** (flat / 1200.0)
    frames = max(int(round(total * rate)), 2)
    values = resampling(np.column_stack([grid, frequencies]), frames)[:, 1]
    synthesise.sonify(os.path.join(folder, f"{index}.wav"), grid, flat,
                      tonic, 22050, marks)
    with open(os.path.join(folder, f"{index}.pitch"), "w") as handle:
        handle.write("\n".join(f"{value:.6f}" for value in values))
        handle.write("\n")
    with open(os.path.join(folder, f"{index}.tonic"), "w") as handle:
        handle.write(f"{float(tonic):.6f}")


def table(summary):
    panel = Table(title="lookup evaluation")
    panel.add_column("raga")
    panel.add_column("correct", justify="right")
    panel.add_column("accuracy", justify="right")
    panel.add_column("identified as")
    passages = 0
    total = 0
    for name, predictions in summary:
        count = len(predictions)
        hits = sum(1 for value in predictions if plain(value) == plain(name))
        passages += count
        total += hits
        counts = Counter(predictions)
        top = counts.most_common(3)
        spread = "  ".join(f"{label}×{n}" for label, n in top)
        if len(counts) > len(top):
            spread += "  ..."
        panel.add_row(name, str(hits),
                      f"{100.0 * hits / max(count, 1):.1f}%", spread)
    panel.add_row("all", str(total),
                  f"{100.0 * total / max(passages, 1):.1f}%", "-")
    return panel


class Block(torch.nn.Module):
    def __init__(self, channels, kernel):
        super().__init__()
        self.conv_block1 = torch.nn.Sequential(
            torch.nn.Conv1d(channels, channels, kernel_size=kernel,
                            stride=1, padding="same"),
            torch.nn.BatchNorm1d(channels),
            torch.nn.ReLU(),
        )
        self.conv_block2 = torch.nn.Sequential(
            torch.nn.Conv1d(channels, channels, kernel_size=3,
                            stride=1, padding="same"),
            torch.nn.BatchNorm1d(channels),
            torch.nn.ReLU(),
        )

    def forward(self, identity):
        x = self.conv_block1(identity)
        x = self.conv_block2(x)
        return x + identity


class RagaNet(torch.nn.Module):
    def __init__(self, params):
        super().__init__()
        channels = params["channels"]
        self.conv_first = torch.nn.Sequential(
            torch.nn.Conv1d(params["input"], channels, kernel_size=80,
                            stride=params["stride"]),
            torch.nn.BatchNorm1d(channels),
            torch.nn.ReLU(),
        )
        self.res_blocks = torch.nn.ModuleList(
            [Block(channels, 3) for _ in range(params["blocks"])])
        self.fc1 = torch.nn.Linear(channels, params["classes"])
        self.max_pool_every = params["poolEvery"]

    def forward(self, x):
        x = self.conv_first(x)
        for index, block in enumerate(self.res_blocks):
            x = block(x)
            if index % self.max_pool_every == 0:
                x = torch.nn.functional.max_pool1d(x, 2)
        x = torch.nn.functional.avg_pool1d(x, x.shape[-1])
        x = x.permute(0, 2, 1)
        x = self.fc1(x)
        return torch.nn.functional.log_softmax(x, dim=-1)


def fetch(url, path):
    if os.path.exists(path):
        return path
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with urllib.request.urlopen(url) as response:
        with open(path + ".part", "wb") as handle:
            shutil.copyfileobj(response, handle)
    os.replace(path + ".part", path)
    return path


def scalar():
    try:
        from ruamel.yaml.scalarfloat import ScalarFloat
        return ScalarFloat
    except ImportError:
        module = types.ModuleType("ruamel.yaml.scalarfloat")
        module.ScalarFloat = type("ScalarFloat", (float,), {})
        sys.modules.setdefault("ruamel", types.ModuleType("ruamel"))
        sys.modules.setdefault("ruamel.yaml", types.ModuleType("ruamel.yaml"))
        sys.modules["ruamel.yaml.scalarfloat"] = module
        return module.ScalarFloat


def raganet(config):
    params = config["evaluation"]["raganet"]
    folder = config["evaluation"]["modelDir"]
    checkpoint = fetch(params["checkpoint"], os.path.join(folder, "raganet.tar"))
    labels_path = fetch(params["labels"],
                        os.path.join(folder, "raganet_labels.json"))
    with open(labels_path) as handle:
        labels = list(json.load(handle))[:params["classes"]]
    model = RagaNet(params)
    torch.serialization.add_safe_globals([scalar()])
    try:
        stored = torch.load(checkpoint, map_location="cpu")
    except Exception:
        stored = torch.load(checkpoint, map_location="cpu",
                            weights_only=False)
    state = stored["model_state"] if "model_state" in stored else stored
    state = {name[7:] if name.startswith("module.") else name: value
             for name, value in state.items()}
    model.load_state_dict(state)
    model.eval()
    return model, labels


def identify(model, labels, path, params):
    rate, signal = wavfile.read(path)
    audio = np.asarray(signal, dtype=np.float32)
    audio = np.stack([audio, audio]) if audio.ndim == 1 else audio.T
    audio = librosa.resample(audio, orig_sr=rate,
                             target_sr=params["sampleRate"], axis=-1)
    if params["normalize"]:
        audio = (audio - audio.mean(axis=1, keepdims=True)) / (
            audio.std(axis=1, keepdims=True) + 1e-5)
    clip = int(params["sampleRate"] * params["clipSeconds"])
    frames = max(len(audio) // clip, 1)
    scores = np.zeros(len(labels))
    with torch.no_grad():
        for index in range(frames):
            block = audio[:, index * clip:(index + 1) * clip]
            if block.shape[1] < clip:
                block = np.pad(block, ((0, 0), (0, clip - block.shape[1])))
            tensor = torch.from_numpy(
                np.ascontiguousarray(block)).unsqueeze(0)
            log = model(tensor).reshape(-1, len(labels)).numpy()[0]
            scores += np.exp(log)
    return labels[int(np.argmax(scores))]


def stage(config, options, lookups, tonics, tempos, advance=None):
    settings = config["evaluation"]
    model, labels = raganet(config)
    root = options["evaluationDir"]
    total = settings["samples"] * max(len(options["ragas"]), 1)
    counter = 0
    summary = []
    for name in options["ragas"]:
        rows = lookups[name]
        present = {(r["svara"], r["previous"], r["next"]): r for r in rows}
        positions = scale.positions(config, name)
        artists = sorted(tempos[name])
        folder = os.path.join(root, name)
        os.makedirs(folder, exist_ok=True)
        predictions = []
        for index in range(settings["samples"]):
            counter += 1
            random.seed(settings["seed"] + counter)
            np.random.seed((settings["seed"] + counter) % (1 << 32))
            record = os.path.join(folder, f"{index}.json")
            data = {}
            if os.path.exists(record) and not options["force"]:
                with open(record) as handle:
                    data = json.load(handle)
            data.pop("predicted", None)
            audio = os.path.join(folder, f"{index}.wav")
            written = os.path.exists(audio)
            if options["force"] or not written:
                artist, chosen = passage(config, name, rows, present,
                                         artists, tempos, settings,
                                         occurrence(options["cacheDir"], name,
                                                    rows))
                times, cents, marks, length = contour(config, chosen,
                                                      positions)
                write(folder, index, times, cents, marks, length,
                      tonics[artist], settings["rate"])
                written = True
                data = {}
            if "raganet" not in data:
                data = {"raganet": identify(model, labels, audio,
                                            settings["raganet"])}
                with open(record, "w") as handle:
                    json.dump(data, handle, indent=1)
            predictions.append(data["raganet"])
            if advance is not None:
                advance(counter, total)
        summary.append((name, predictions))
    return table(summary)
