from pathlib import Path
import subprocess
import sys

import numpy as np


PROJECT_ROOT = (
    Path(__file__)
    .resolve()
    .parent
    .parent
)


SRC_PATH = (
    PROJECT_ROOT
    / "src"
)

sys.path.insert(
    0,
    str(SRC_PATH),
)


from effects.gain import apply_gain


# =========================================================
# Find C++ Executable
# =========================================================

candidates = [
    PROJECT_ROOT
    / "cpp"
    / "build"
    / "Debug"
    / "gain_cli.exe",

    PROJECT_ROOT
    / "cpp"
    / "build"
    / "Release"
    / "gain_cli.exe",

    PROJECT_ROOT
    / "cpp"
    / "build"
    / "gain_cli.exe",

    PROJECT_ROOT
    / "cpp"
    / "build"
    / "gain_cli",
]


cpp_executable = None


for candidate in candidates:

    if candidate.exists():

        cpp_executable = candidate

        break


if cpp_executable is None:

    raise FileNotFoundError(
        "gain_cli executable not found. "
        "Build the C++ project first."
    )


# =========================================================
# Test Input
# =========================================================

gain_db = 6.0206


input_samples = np.array(
    [
        -0.8,
        -0.5,
        -0.25,
        -0.1,
        0.0,
        0.1,
        0.25,
        0.5,
        0.8,
    ],
    dtype=np.float32,
)


# =========================================================
# Python Gain
# =========================================================

python_output = apply_gain(
    input_samples,
    gain_db,
)


# =========================================================
# C++ Gain
# =========================================================

command = [
    str(cpp_executable),
    str(gain_db),
]

command.extend(
    str(float(sample))
    for sample in input_samples
)


result = subprocess.run(
    command,
    capture_output=True,
    text=True,
    check=True,
)


cpp_output = np.array(
    [
        float(line)

        for line
        in result.stdout.splitlines()

        if line.strip()
    ],
    dtype=np.float32,
)


# =========================================================
# Comparison
# =========================================================

difference = np.abs(
    python_output
    - cpp_output
)


print()

print(
    "========================================"
)

print(
    "      PYTHON vs C++ GAIN COMPARISON"
)

print(
    "========================================"
)

print()

print(
    f"Gain: {gain_db:.4f} dB"
)

print()

print(
    f"{'Input':>10}"
    f"{'Python':>12}"
    f"{'C++':>12}"
    f"{'Error':>14}"
)

print(
    "-" * 48
)


for (
    input_sample,
    python_sample,
    cpp_sample,
    error,
) in zip(
    input_samples,
    python_output,
    cpp_output,
    difference,
):

    print(
        f"{input_sample:10.6f}"
        f"{python_sample:12.6f}"
        f"{cpp_sample:12.6f}"
        f"{error:14.9f}"
    )


print()

max_error = float(
    np.max(
        difference
    )
)


is_match = np.allclose(
    python_output,
    cpp_output,
    atol=1e-6,
    rtol=1e-6,
)


print(
    f"Max absolute error : "
    f"{max_error:.9f}"
)

print(
    f"Match              : "
    f"{is_match}"
)

print()

print(
    "========================================"
)