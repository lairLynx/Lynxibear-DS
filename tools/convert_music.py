"""Download the CC0 music tracks and convert them to the game's stream format.

Output goes to nitrofs/music/ as raw signed 16-bit little-endian mono PCM at
22050 Hz, which source/audio.c streams from NitroFS. Requires numpy, scipy and
soundfile (pip install numpy scipy soundfile).

Tracks (all CC0, from OpenGameArt.org):
  menu.pcm  - "Apple Cider" (loop version), Zane Little Music
              https://opengameart.org/content/apple-cider-juiced
  game1.pcm - "Hush Hamlet" (loop version), Zane Little Music
              https://opengameart.org/content/hush-hamlet
  game2.pcm - "Hot Springs Town", Kistol
              https://opengameart.org/content/hot-springs-town
"""

import math
import pathlib
import urllib.request

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE_DIR = ROOT / "assets" / "incoming" / "music"
OUTPUT_DIR = ROOT / "nitrofs" / "music"
BASE_URL = "https://opengameart.org/sites/default/files/"
SAMPLE_RATE = 22050
PEAK = 0.7  # about -3 dBFS; leaves headroom for the small DSi speaker

TRACKS = {
    "menu.pcm": "apple_cider_loop.wav",
    "game1.pcm": "hush_hamlet_loop_0.wav",
    "game2.pcm": "hot_spring_town_1.ogg",
}


def fetch(name):
    path = SOURCE_DIR / name
    if not path.exists():
        SOURCE_DIR.mkdir(parents=True, exist_ok=True)
        request = urllib.request.Request(BASE_URL + name,
                                         headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(request) as response:
            path.write_bytes(response.read())
    return path


def convert(source, target):
    data, rate = sf.read(source, dtype="float32", always_2d=True)
    mono = data.mean(axis=1)
    divisor = math.gcd(SAMPLE_RATE, rate)
    mono = resample_poly(mono, SAMPLE_RATE // divisor, rate // divisor)
    peak = float(np.abs(mono).max())
    if peak > 0:
        mono = mono * (PEAK / peak)
    pcm = np.clip(np.round(mono * 32767), -32768, 32767).astype("<i2")
    target.write_bytes(pcm.tobytes())
    return len(pcm) / SAMPLE_RATE


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    for output, source in TRACKS.items():
        target = OUTPUT_DIR / output
        seconds = convert(fetch(source), target)
        print(f"{output}: {seconds:.1f}s, {target.stat().st_size / 1e6:.1f} MB")


if __name__ == "__main__":
    main()
