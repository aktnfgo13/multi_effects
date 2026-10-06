from pathlib import Path

from analysis.reference_benchmark import (
    compare_audio,
)


# =========================================================
# Project Root
# =========================================================

PROJECT_ROOT = (
    Path(__file__)
    .resolve()
    .parent
    .parent
)


# =========================================================
# Audio Files
# =========================================================

reference_path = (
    PROJECT_ROOT
    / "audio"
    / "input"
    / "test.wav"
)

target_path = (
    PROJECT_ROOT
    / "audio"
    / "output"
    / "test_amp_preamp_raw.wav"
)


# =========================================================
# Benchmark
# =========================================================

results = compare_audio(
    reference_path,
    target_path,
)


# =========================================================
# Output
# =========================================================

print()

print(
    "========================================"
)

print(
    "       REFERENCE BENCHMARK v0.6"
)

print(
    "========================================"
)

print()


# =========================================================
# Main Results
# =========================================================

for key, value in results.items():

    if key == "frequency_bands":
        continue

    if isinstance(value, bool):

        print(
            f"{key:30s}: "
            f"{value}"
        )

    elif isinstance(value, float):

        print(
            f"{key:30s}: "
            f"{value:.6f}"
        )

    else:

        print(
            f"{key:30s}: "
            f"{value}"
        )


# =========================================================
# Frequency Band Results
# =========================================================

print()

print(
    "----------------------------------------"
)

print(
    "        FREQUENCY BAND ANALYSIS"
)

print(
    "----------------------------------------"
)

print()

bands = results[
    "frequency_bands"
]


for band_name, band in bands.items():

    print(
        f"{band_name:12s} "
        f"{band['min_hz']:7.0f}"
        f" - "
        f"{band['max_hz']:7.0f} Hz"
        f" | Diff: "
        f"{band['difference_db']:8.3f} dB"
        f" | MAE: "
        f"{band['mae_db']:8.3f} dB"
    )


print()

print(
    "========================================"
)