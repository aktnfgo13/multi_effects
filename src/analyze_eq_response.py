from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from effects.eq import peaking_eq
from analysis.test_signal import generate_sine


PROJECT_ROOT = (
    Path(__file__)
    .resolve()
    .parent
    .parent
)

OUTPUT_PATH = (
    PROJECT_ROOT
    / "docs"
    / "eq_frequency_response.png"
)


# =========================
# EQ Settings
# =========================

SAMPLE_RATE = 48000

CENTER_FREQUENCY = 1000.0
GAIN_DB = 6.0
Q = 1.0


# =========================
# Test Frequencies
# =========================

frequencies = np.logspace(
    np.log10(20),
    np.log10(20000),
    100,
)


measured_gain_db = []


# =========================
# Frequency Response Test
# =========================

for frequency in frequencies:

    input_signal = generate_sine(
        frequency=frequency,
        sample_rate=SAMPLE_RATE,
        duration=0.3,
        amplitude=0.1,
    )

    output_signal = peaking_eq(
        input_signal,
        SAMPLE_RATE,
        frequency=CENTER_FREQUENCY,
        gain_db=GAIN_DB,
        q=Q,
    )

    # 초기 필터 transient를 제외하기 위해
    # 뒤쪽 절반만 측정
    start = len(input_signal) // 2

    input_test = input_signal[start:]
    output_test = output_signal[start:]

    input_rms = np.sqrt(
        np.mean(input_test ** 2)
    )

    output_rms = np.sqrt(
        np.mean(output_test ** 2)
    )

    gain = 20 * np.log10(
        output_rms / (input_rms + 1e-12)
    )

    measured_gain_db.append(gain)


# =========================
# Result
# =========================

measured_gain_db = np.array(
    measured_gain_db
)


peak_index = np.argmax(
    measured_gain_db
)

print("=== EQ RESPONSE TEST ===")
print(
    f"Target Frequency : "
    f"{CENTER_FREQUENCY:.0f} Hz"
)

print(
    f"Target Gain      : "
    f"{GAIN_DB:+.2f} dB"
)

print(
    f"Measured Peak    : "
    f"{measured_gain_db[peak_index]:+.2f} dB"
)

print(
    f"Peak Frequency   : "
    f"{frequencies[peak_index]:.0f} Hz"
)


# =========================
# Plot
# =========================

plt.figure(
    figsize=(12, 6)
)

plt.semilogx(
    frequencies,
    measured_gain_db,
)

plt.axvline(
    CENTER_FREQUENCY,
    linestyle="--",
    label="Target Frequency",
)

plt.axhline(
    0,
    linewidth=1,
)

plt.title(
    "Peaking EQ Frequency Response"
)

plt.xlabel(
    "Frequency (Hz)"
)

plt.ylabel(
    "Gain (dB)"
)

plt.xlim(
    20,
    20000,
)

plt.grid(
    True,
    which="both",
)

plt.legend()

plt.tight_layout()


OUTPUT_PATH.parent.mkdir(
    parents=True,
    exist_ok=True,
)

plt.savefig(
    OUTPUT_PATH
)

plt.show()


print()
print(
    f"Saved: {OUTPUT_PATH}"
)