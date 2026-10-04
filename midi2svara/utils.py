import os
import subprocess

from rich.progress import Progress


def progress_bar(label, console):
    bar = Progress(console=console)
    task = None

    def advance(done, total):
        nonlocal task
        if task is None:
            task = bar.add_task(f"  {label}", total=max(total, 1))
        bar.update(task, completed=done)

    return bar, advance


def to_mp3(folder):
    for root, _, files in os.walk(folder):
        for entry in files:
            if not entry.endswith(".wav"):
                continue
            source = os.path.join(root, entry)
            target = source[:-4] + ".mp3"
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error",
                            "-i", source, "-codec:a", "libmp3lame",
                            "-q:a", "2", target], check=True)
            os.remove(source)
