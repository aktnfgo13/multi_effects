from pathlib import Path

import numpy as np
import soundfile as sf

from effects.vibrato import (
    apply_vibrato,
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
    / "test_vibrato.wav"
)


audio_data, sample_rate = sf.read(
    INPUT_PATH
)


processed_audio = apply_vibrato(
    audio_data,
    sample_rate,
    rate_hz=5.0,
    depth_ms=2.0,
    base_delay_ms=5.0,
)


print(
    "=== VIBRATO TEST ==="
)

print(
    "Rate       : 5.0 Hz"
)

print(
    "Depth      : 2.0 ms"
)

print(
    "Base Delay : 5.0 ms"
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