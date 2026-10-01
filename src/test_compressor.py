from pathlib import Path

import soundfile as sf

from effects.compressor import (
    apply_compressor,
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
    / "test_compressor.wav"
)


audio_data, sample_rate = sf.read(
    INPUT_PATH
)


processed_audio = apply_compressor(
    audio_data,
    sample_rate,
    threshold_db=-18.0,
    ratio=4.0,
    attack_ms=10.0,
    release_ms=100.0,
    makeup_gain_db=6.0,
)


print("=== COMPRESSOR TEST ===")
print(
    f"Peak Before : "
    f"{abs(audio_data).max():.4f}"
)

print(
    f"Peak After  : "
    f"{abs(processed_audio).max():.4f}"
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