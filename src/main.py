from pathlib import Path

import soundfile as sf

from effects.gain import apply_gain


# =========================
# Path Settings
# =========================

PROJECT_ROOT = Path(__file__).resolve().parent.parent

INPUT_PATH = PROJECT_ROOT / "audio" / "input" / "test.wav"
OUTPUT_PATH = PROJECT_ROOT / "audio" / "output" / "test_gain.wav"


# =========================
# Gain Settings
# =========================

GAIN_DB = 6.0


# =========================
# Load Audio
# =========================

audio_data, sample_rate = sf.read(INPUT_PATH)


# =========================
# Input Audio Information
# =========================

print("=== Input Audio ===")
print(f"Sample Rate : {sample_rate} Hz")

if audio_data.ndim == 1:
    channels = 1
else:
    channels = audio_data.shape[1]

duration = len(audio_data) / sample_rate

print(f"Channels    : {channels}")
print(f"Duration    : {duration:.2f} sec")
print(f"Samples     : {len(audio_data)}")
print(f"Data Shape  : {audio_data.shape}")
print(f"Peak Before : {abs(audio_data).max():.4f}")


# =========================
# Apply Gain
# =========================

processed_audio = apply_gain(
    audio_data,
    GAIN_DB,
)


# =========================
# Gain Result
# =========================

print()
print("=== Gain Effect ===")
print(f"Gain        : {GAIN_DB} dB")
print(f"Peak After  : {abs(processed_audio).max():.4f}")


# =========================
# Save Audio
# =========================

OUTPUT_PATH.parent.mkdir(
    parents=True,
    exist_ok=True,
)

sf.write(
    OUTPUT_PATH,
    processed_audio,
    sample_rate,
)


print()
print(f"Saved: {OUTPUT_PATH}")