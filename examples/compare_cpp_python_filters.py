from pathlib import Path
import subprocess
import sys

import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = PROJECT_ROOT / "src"

sys.path.insert(0, str(SRC_DIR))


from effects.filters import (
    low_pass_filter,
    high_pass_filter,
    low_shelf_filter,
    high_shelf_filter,
)

from effects.eq import peaking_eq


CPP_EXECUTABLE = (
    PROJECT_ROOT
    / "cpp"
    / "build"
    / "filter_cli"
)


def run_cpp_filter(
    mode,
    audio_data,
    sample_rate,
    frequency,
    param1,
    param2,
):
    command = [
        str(CPP_EXECUTABLE),
        mode,
        str(sample_rate),
        str(frequency),
        str(param1),
        str(param2),
    ]

    command.extend(
        str(float(sample))
        for sample in audio_data
    )

    result = subprocess.run(
        command,
        capture_output=True,
        text=True,
        check=True,
    )

    return np.array(
        [
            float(value)
            for value
            in result.stdout.splitlines()
        ],
        dtype=np.float32,
    )


def compare(
    name,
    python_output,
    cpp_output,
):
    error = np.max(
        np.abs(
            python_output
            - cpp_output
        )
    )

    match = np.allclose(
        python_output,
        cpp_output,
        atol=1e-4,
        rtol=1e-5,
    )

    print()
    print(f"[{name}]")
    print(
        f"Max absolute error: "
        f"{error:.9f}"
    )
    print(
        f"Match: {match}"
    )

    if not match:
        raise RuntimeError(
            f"{name} mismatch."
        )


# =========================================================
# Test Signal
# =========================================================

sample_rate = 44100

t = (
    np.arange(128)
    / sample_rate
)

audio_data = (
    0.4
    * np.sin(
        2 * np.pi * 440 * t
    )
    +
    0.2
    * np.sin(
        2 * np.pi * 4000 * t
    )
).astype(np.float32)


print()
print("========================================")
print("Python ↔ C++ Filter Comparison")
print("========================================")


# =========================================================
# Low Pass
# =========================================================

python_output = low_pass_filter(
    audio_data,
    sample_rate,
    cutoff=8000.0,
    q=0.707,
)

cpp_output = run_cpp_filter(
    "lowpass",
    audio_data,
    sample_rate,
    8000.0,
    0.707,
    0.0,
)

compare(
    "Low Pass",
    python_output,
    cpp_output,
)


# =========================================================
# High Pass
# =========================================================

python_output = high_pass_filter(
    audio_data,
    sample_rate,
    cutoff=80.0,
    q=0.707,
)

cpp_output = run_cpp_filter(
    "highpass",
    audio_data,
    sample_rate,
    80.0,
    0.707,
    0.0,
)

compare(
    "High Pass",
    python_output,
    cpp_output,
)


# =========================================================
# Low Shelf
# =========================================================

python_output = low_shelf_filter(
    audio_data,
    sample_rate,
    frequency=200.0,
    gain_db=6.0,
    slope=1.0,
)

cpp_output = run_cpp_filter(
    "lowshelf",
    audio_data,
    sample_rate,
    200.0,
    6.0,
    1.0,
)

compare(
    "Low Shelf",
    python_output,
    cpp_output,
)


# =========================================================
# High Shelf
# =========================================================

python_output = high_shelf_filter(
    audio_data,
    sample_rate,
    frequency=4000.0,
    gain_db=6.0,
    slope=1.0,
)

cpp_output = run_cpp_filter(
    "highshelf",
    audio_data,
    sample_rate,
    4000.0,
    6.0,
    1.0,
)

compare(
    "High Shelf",
    python_output,
    cpp_output,
)


# =========================================================
# Peaking EQ
# =========================================================

python_output = peaking_eq(
    audio_data,
    sample_rate,
    frequency=1000.0,
    gain_db=6.0,
    q=1.0,
)

cpp_output = run_cpp_filter(
    "peaking",
    audio_data,
    sample_rate,
    1000.0,
    6.0,
    1.0,
)

compare(
    "Peaking EQ",
    python_output,
    cpp_output,
)


print()
print(
    "All filter comparisons passed."
)