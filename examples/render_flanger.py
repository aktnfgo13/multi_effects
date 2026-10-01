from pathlib import Path

import numpy as np
import soundfile as sf

from effects.flanger import (
    apply_flanger,
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
    / "test_flanger.wav"
)


audio_data, sample_rate = sf.read(
    INPUT_PATH
)


processed_audio = apply_flanger(
    audio_data,
    sample_rate,
    rate_hz=0.4,
    depth_ms=1.5,
    base_delay_ms=2.0,
    mix=0.5,
)


print("=== FLANGER TEST ===")
print(f"Sample Rate : {sample_rate} Hz")
print("Rate        : 0.4 Hz")
print("Depth       : 1.5 ms")
print("Base Delay  : 2.0 ms")
print("Mix         : 0.5")


peak = np.max(
    np.abs(processed_audio)
)

print(
    f"Output Peak : {peak:.4f}"
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