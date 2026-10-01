from pathlib import Path

import matplotlib.pyplot as plt
import soundfile as sf

from effects.distortion import (
    apply_hard_clipping,
    apply_soft_clipping,
)

from analysis.audio_analysis import (
    calculate_peak,
    calculate_rms,
    calculate_fft,
)


PROJECT_ROOT = Path(__file__).resolve().parent.parent

INPUT_PATH = PROJECT_ROOT / "audio" / "input" / "test.wav"

RESULT_PATH = (
    PROJECT_ROOT
    / "docs"
    / "clipping_analysis.png"
)


# =========================
# Load Audio
# =========================

audio_data, sample_rate = sf.read(INPUT_PATH)


# =========================
# DSP
# =========================

DRIVE_DB = 12.0

hard_audio = apply_hard_clipping(
    audio_data,
    drive_db=DRIVE_DB,
)

soft_audio = apply_soft_clipping(
    audio_data,
    drive_db=DRIVE_DB,
)


# =========================
# Numeric Analysis
# =========================

print("=== ORIGINAL ===")
print(f"Peak : {calculate_peak(audio_data):.4f}")
print(f"RMS  : {calculate_rms(audio_data):.4f}")

print()

print("=== HARD CLIPPING ===")
print(f"Peak : {calculate_peak(hard_audio):.4f}")
print(f"RMS  : {calculate_rms(hard_audio):.4f}")

print()

print("=== SOFT CLIPPING ===")
print(f"Peak : {calculate_peak(soft_audio):.4f}")
print(f"RMS  : {calculate_rms(soft_audio):.4f}")


# =========================
# Waveform
# =========================

wave_samples = min(
    int(sample_rate * 0.02),
    len(audio_data),
)

plt.figure(figsize=(12, 5))

plt.plot(
    audio_data[:wave_samples],
    label="Original",
)

plt.plot(
    hard_audio[:wave_samples],
    label="Hard",
)

plt.plot(
    soft_audio[:wave_samples],
    label="Soft",
)

plt.title("Waveform Comparison")
plt.xlabel("Sample")
plt.ylabel("Amplitude")
plt.legend()
plt.grid()

plt.tight_layout()
plt.show()


# =========================
# FFT
# =========================

freq_original, fft_original = calculate_fft(
    audio_data,
    sample_rate,
)

freq_hard, fft_hard = calculate_fft(
    hard_audio,
    sample_rate,
)

freq_soft, fft_soft = calculate_fft(
    soft_audio,
    sample_rate,
)


plt.figure(figsize=(12, 5))

plt.plot(
    freq_original,
    fft_original,
    label="Original",
)

plt.plot(
    freq_hard,
    fft_hard,
    label="Hard",
)

plt.plot(
    freq_soft,
    fft_soft,
    label="Soft",
)

plt.xlim(20, 20000)

plt.title("FFT Spectrum Comparison")
plt.xlabel("Frequency (Hz)")
plt.ylabel("Magnitude (dB)")
plt.legend()
plt.grid()

plt.tight_layout()

RESULT_PATH.parent.mkdir(
    parents=True,
    exist_ok=True,
)

plt.savefig(RESULT_PATH)

plt.show()

print()
print(f"Analysis saved: {RESULT_PATH}")