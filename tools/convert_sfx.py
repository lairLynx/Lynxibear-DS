"""Download the sound effects the game uses and prepare them for the DS.

Writes mono 16-bit 22050 Hz WAVs to audio/, which the BlocksDS makefile turns
into a Maxmod soundbank (nitro:/soundbank.bin). Requires numpy, scipy,
soundfile and py7zr (pip install numpy scipy soundfile py7zr).

Sources:
  "Cozy Farm SFX" by Bramble & Byte (https://aquumgifts.itch.io), CC-BY 4.0,
  https://opengameart.org/content/cozy-farm-sfx-50-farming-game-sound-effects-tools-harvest-shop-ui-jingles
  "Fantozzi's Footsteps" by Fantozzi, CC0 (single steps on sand, which sounds
  like soft grass and soil), https://opengameart.org/content/fantozzis-footsteps-grasssand-stone
"""

import io
import math
import pathlib
import tempfile
import urllib.request
import zipfile

import numpy as np
import py7zr
import soundfile as sf
from scipy.signal import resample_poly

ROOT = pathlib.Path(__file__).resolve().parent.parent
INCOMING = ROOT / "assets" / "incoming" / "sfx"
OUTPUT_DIR = ROOT / "audio"
COZY_URL = "https://opengameart.org/sites/default/files/cozy-farm-sfx-cc-by_0.zip"
FOOTSTEP_URL = "https://opengameart.org/sites/default/files/Fantozzi-footsteps.7z"
SAMPLE_RATE = 22050
PEAK = 0.7

USED = [
    "hoe-dig", "hoe-dig-2", "axe-chop", "tree-fall", "watering", "seed-plant",
    "harvest-pop", "sell-coins", "buy", "not-enough-money", "jingle-level-up",
    "sleep", "ui-error", "ui-click", "ui-hover", "ui-confirm", "ui-back",
    "menu-open", "menu-close", "inventory-open", "place-item", "gate",
    "shop-bell",
]

# Left and right feet alternate in the game.
FOOTSTEPS = {
    "step-grass-l": "Fantozzi-SandL2", "step-grass-r": "Fantozzi-SandR2",
    "step-dirt-l": "Fantozzi-SandL3", "step-dirt-r": "Fantozzi-SandR3",
}

# Sounds cut short (seconds) so they end with the action; a short fade hides the cut.
MAX_SECONDS = {"watering": 0.5, **{name: 0.26 for name in FOOTSTEPS}}
FADE_SECONDS = 0.12
# Footsteps are kept well below the other effects.
PEAK_FOR = {"step-grass-l": 0.4, "step-grass-r": 0.4,
            "step-dirt-l": 0.45, "step-dirt-r": 0.45}


def download(url, path):
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(request) as response:
            path.write_bytes(response.read())
    return path


def cozy_sounds():
    archive = zipfile.ZipFile(download(COZY_URL, INCOMING / "cozy-farm.zip"))
    return {pathlib.PurePosixPath(n).stem: archive.read(n)
            for n in archive.namelist() if n.endswith(".wav")}


def footstep_sounds():
    with tempfile.TemporaryDirectory() as folder:
        with py7zr.SevenZipFile(download(FOOTSTEP_URL, INCOMING / "fantozzi.7z")) as archive:
            archive.extractall(folder)
        return {path.stem: path.read_bytes()
                for path in pathlib.Path(folder).rglob("*.ogg")}


def convert(data, name):
    samples, rate = sf.read(io.BytesIO(data), dtype="float32", always_2d=True)
    mono = samples.mean(axis=1)
    divisor = math.gcd(SAMPLE_RATE, rate)
    mono = resample_poly(mono, SAMPLE_RATE // divisor, rate // divisor)
    limit = MAX_SECONDS.get(name)
    if limit is not None and len(mono) > int(limit * SAMPLE_RATE):
        mono = mono[: int(limit * SAMPLE_RATE)].copy()
        fade = int(FADE_SECONDS * SAMPLE_RATE)
        mono[-fade:] *= np.linspace(1.0, 0.0, fade, dtype=np.float32)
    peak = float(np.abs(mono).max())
    if peak > 0:
        mono = mono * (PEAK_FOR.get(name, PEAK) / peak)
    return mono


def main():
    OUTPUT_DIR.mkdir(exist_ok=True)
    cozy = cozy_sounds()
    steps = footstep_sounds()
    sounds = {name: cozy[name] for name in USED}
    sounds.update({name: steps[source] for name, source in FOOTSTEPS.items()})

    total = 0
    for name, data in sounds.items():
        target = OUTPUT_DIR / f"{name}.wav"
        sf.write(target, convert(data, name), SAMPLE_RATE, subtype="PCM_16")
        total += target.stat().st_size
    print(f"{len(sounds)} sounds, {total / 1e3:.0f} KB")


if __name__ == "__main__":
    main()
