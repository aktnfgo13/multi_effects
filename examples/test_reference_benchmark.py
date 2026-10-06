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
    "       REFERENCE BENCHMARK v0.3"
)

print(
    "========================================"
)

print()


for key, value in results.items():

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


print()

print(
    "========================================"
)