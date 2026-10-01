from pathlib import Path

import soundfile as sf

from effects.eq import peaking_eq


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
    / "test_eq.wav"
)


audio_data, sample_rate = sf.read(
    INPUT_PATH
)


print("=== EQ TEST ===")

print(
    f"Sample Rate : "
    f"{sample_rate} Hz"
)

print(
    f"Peak Before : "
    f"{abs(audio_data).max():.4f}"
)


processed_audio = peaking_eq(
    audio_data,
    sample_rate,
    frequency=1000.0,
    gain_db=6.0,
    q=1.0,
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