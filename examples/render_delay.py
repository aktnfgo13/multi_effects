from pathlib import Path

import numpy as np
import soundfile as sf

from effects.delay import (
    apply_delay,
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
    / "test_delay.wav"
)


audio_data, sample_rate = sf.read(
    INPUT_PATH
)


processed_audio = apply_delay(
    audio_data,
    sample_rate,
    delay_ms=400.0,
    feedback=0.35,
    mix=0.35,
)


print("=== DELAY TEST ===")

print(
    f"Sample Rate : "
    f"{sample_rate} Hz"
)

print(
    f"Delay Time  : "
    f"400 ms"
)

print(
    f"Feedback    : "
    f"0.35"
)

print(
    f"Mix         : "
    f"0.35"
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


print()
print(
    f"Saved: {OUTPUT_PATH}"
)