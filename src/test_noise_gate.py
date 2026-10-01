from pathlib import Path

import soundfile as sf

from effects.noise_gate import (
    apply_noise_gate,
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
    / "test_noise_gate.wav"
)


audio_data, sample_rate = sf.read(
    INPUT_PATH
)


processed_audio = apply_noise_gate(
    audio_data,
    sample_rate,
    threshold_db=-45.0,
    attack_ms=5.0,
    release_ms=100.0,
)


print("=== NOISE GATE TEST ===")

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