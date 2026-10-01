from pathlib import Path

import numpy as np
import soundfile as sf

from effects.chorus import (
    apply_chorus,
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
    / "test_chorus.wav"
)


audio_data, sample_rate = sf.read(
    INPUT_PATH
)


processed_audio = apply_chorus(
    audio_data,
    sample_rate,

    rate_hz=0.8,
    depth_ms=5.0,
    base_delay_ms=15.0,
    mix=0.35,
)


print(
    "=== CHORUS TEST ==="
)

print(
    f"Sample Rate : "
    f"{sample_rate} Hz"
)

print(
    "Rate        : 0.8 Hz"
)

print(
    "Depth       : 5.0 ms"
)

print(
    "Base Delay  : 15.0 ms"
)

print(
    "Mix         : 0.35"
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