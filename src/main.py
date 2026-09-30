from pathlib import Path

import soundfile as sf

from effects.distortion import apply_distortion


PROJECT_ROOT = Path(__file__).resolve().parent.parent

INPUT_PATH = PROJECT_ROOT / "audio" / "input" / "test.wav"
OUTPUT_PATH = PROJECT_ROOT / "audio" / "output" / "test_distortion.wav"


audio_data, sample_rate = sf.read(INPUT_PATH)


print("=== Input Audio ===")
print(f"Sample Rate : {sample_rate} Hz")
print(f"Data Shape  : {audio_data.shape}")
print(f"Peak Before : {abs(audio_data).max():.4f}")


DRIVE_DB = 12.0
THRESHOLD = 0.7


processed_audio = apply_distortion(
    audio_data,
    drive_db=DRIVE_DB,
    threshold=THRESHOLD,
)


print()
print("=== Distortion Effect ===")
print(f"Drive       : {DRIVE_DB} dB")
print(f"Threshold   : {THRESHOLD}")
print(f"Peak After  : {abs(processed_audio).max():.4f}")


sf.write(
    OUTPUT_PATH,
    processed_audio,
    sample_rate,
)


print()
print(f"Saved: {OUTPUT_PATH}")