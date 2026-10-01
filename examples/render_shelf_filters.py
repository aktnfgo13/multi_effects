from pathlib import Path

import soundfile as sf

from effects.filters import (
    low_shelf_filter,
    high_shelf_filter,
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
    / "test_shelf_filters.wav"
)


audio_data, sample_rate = sf.read(
    INPUT_PATH
)


processed_audio = low_shelf_filter(
    audio_data,
    sample_rate,
    frequency=200.0,
    gain_db=6.0,
)

processed_audio = high_shelf_filter(
    processed_audio,
    sample_rate,
    frequency=4000.0,
    gain_db=-4.0,
)


sf.write(
    OUTPUT_PATH,
    processed_audio,
    sample_rate,
)


print("=== SHELF FILTER TEST ===")
print("Low Shelf : 200 Hz / +6 dB")
print("High Shelf: 4000 Hz / -4 dB")

print()
print(f"Saved: {OUTPUT_PATH}")