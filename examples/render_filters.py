from pathlib import Path

import soundfile as sf

from effects.filters import (
    high_pass_filter,
    low_pass_filter,
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
    / "test_filters.wav"
)


audio_data, sample_rate = sf.read(
    INPUT_PATH
)


# 저역 제거
processed_audio = high_pass_filter(
    audio_data,
    sample_rate,
    cutoff=80.0,
)


# 고역 제거
processed_audio = low_pass_filter(
    processed_audio,
    sample_rate,
    cutoff=8000.0,
)


sf.write(
    OUTPUT_PATH,
    processed_audio,
    sample_rate,
)


print("=== FILTER TEST ===")
print(
    f"Sample Rate : "
    f"{sample_rate} Hz"
)

print(
    f"HPF Cutoff  : "
    f"80 Hz"
)

print(
    f"LPF Cutoff  : "
    f"8000 Hz"
)

print()
print(
    f"Saved: {OUTPUT_PATH}"
)