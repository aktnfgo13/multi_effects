from pathlib import Path

import numpy as np
import soundfile as sf

from effects.tremolo import (
    apply_tremolo,
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
    / "test_tremolo.wav"
)


audio_data, sample_rate = sf.read(
    INPUT_PATH
)


processed_audio = apply_tremolo(
    audio_data,
    sample_rate,
    rate_hz=4.0,
    depth=0.65,
)


print(
    "=== TREMOLO TEST ==="
)

print(
    "Rate  : 4.0 Hz"
)

print(
    "Depth : 0.65"
)


peak = np.max(
    np.abs(processed_audio)
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


print(
    f"Saved: {OUTPUT_PATH}"
)