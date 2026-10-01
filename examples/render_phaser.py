from pathlib import Path

import numpy as np
import soundfile as sf

from effects.phaser import (
    apply_phaser,
)


PROJECT_ROOT = (
    Path(__file__)
    .resolve()
    .parent
    .parent
)

INPUT_PATH = (
    PROJECT_ROOT
    / "audio"
    / "input"
    / "test.wav"
)

OUTPUT_PATH = (
    PROJECT_ROOT
    / "audio"
    / "output"
    / "test_phaser.wav"
)


audio_data, sample_rate = sf.read(
    INPUT_PATH
)


processed_audio = apply_phaser(
    audio_data,
    sample_rate,

    rate_hz=0.5,

    min_frequency_hz=300.0,
    max_frequency_hz=1600.0,

    stages=4,

    mix=0.5,
)


print(
    "=== PHASER TEST ==="
)

print(
    f"Sample Rate : "
    f"{sample_rate} Hz"
)

print(
    "Rate        : 0.5 Hz"
)

print(
    "Sweep       : "
    "300 ~ 1600 Hz"
)

print(
    "Stages      : 4"
)

print(
    "Mix         : 0.5"
)


peak = np.max(
    np.abs(
        processed_audio
    )
)

print(
    f"Output Peak : "
    f"{peak:.4f}"
)


if peak > 0.95:

    processed_audio = (
        processed_audio
        / peak
        * 0.95
    )


sf.write(
    OUTPUT_PATH,
    processed_audio,
    sample_rate,
)


print()
print(
    f"Saved: {OUTPUT_PATH}"
)