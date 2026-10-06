from pathlib import Path

from analysis.benchmark_visualization import (
    create_benchmark_plots,
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
# Files
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

output_directory = (
    PROJECT_ROOT
    / "docs"
)


# =========================================================
# Create Benchmark Plots
# =========================================================

results = create_benchmark_plots(
    reference_path,
    target_path,
    output_directory,
)


# =========================================================
# Output
# =========================================================

print()
print(
    "========================================"
)

print(
    "    REFERENCE BENCHMARK VISUALIZATION"
)

print(
    "========================================"
)

print()

for key, value in results.items():
    print(
        f"{key:25s}: {value}"
    )

print()
print(
    "========================================"
)