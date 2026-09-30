from pathlib import Path

import soundfile as sf

from effects.distortion import (
    apply_hard_clipping,
    apply_soft_clipping,
)


PROJECT_ROOT = Path(__file__).resolve().parent.parent

INPUT_PATH = PROJECT_ROOT / "audio" / "input" / "test.wav"

HARD_OUTPUT_PATH = (
    PROJECT_ROOT
    / "audio"
    / "output"
    / "test_hard_clipping.wav"
)

SOFT_OUTPUT_PATH = (
    PROJECT_ROOT
    / "audio"
    / "output"
    / "test_soft_clipping.wav"
)


audio_data, sample_rate = sf.read(INPUT_PATH)


DRIVE_DB = 12.0
THRESHOLD = 0.7


hard_audio = apply_hard_clipping(
    audio_data,
    drive_db=DRIVE_DB,
    threshold=THRESHOLD,
)

soft_audio = apply_soft_clipping(
    audio_data,
    drive_db=DRIVE_DB,
)


print("=== Distortion Comparison ===")
print(f"Sample Rate     : {sample_rate} Hz")
print(f"Drive           : {DRIVE_DB} dB")
print(f"Input Peak      : {abs(audio_data).max():.4f}")
print(f"Hard Clip Peak  : {abs(hard_audio).max():.4f}")
print(f"Soft Clip Peak  : {abs(soft_audio).max():.4f}")


sf.write(
    HARD_OUTPUT_PATH,
    hard_audio,
    sample_rate,
)

sf.write(
    SOFT_OUTPUT_PATH,
    soft_audio,
    sample_rate,
)


print()
print(f"Saved Hard : {HARD_OUTPUT_PATH}")
print(f"Saved Soft : {SOFT_OUTPUT_PATH}")