from pathlib import Path
import subprocess
import sys

import numpy as np


# =========================================================
# Project paths
# =========================================================

PROJECT_ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = PROJECT_ROOT / "src"

sys.path.insert(0, str(SRC_DIR))


from effects.distortion import (
    apply_hard_clipping,
    apply_soft_clipping,
)


CPP_EXECUTABLE = (
    PROJECT_ROOT
    / "cpp"
    / "build"
    / "distortion_cli"
)


# =========================================================
# C++ runner
# =========================================================

def run_cpp_distortion(
    mode,
    audio_data,
    drive_db,
    threshold=0.7,
):
    command = [
        str(CPP_EXECUTABLE),
        mode,
        str(drive_db),
        str(threshold),
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

    output = [
        float(value)
        for value in result.stdout.splitlines()
    ]

    return np.array(
        output,
        dtype=np.float32,
    )


# =========================================================
# Test signal
# =========================================================

audio_data = np.array(
    [
        -1.0,
        -0.75,
        -0.5,
        -0.25,
        -0.1,
        0.0,
        0.1,
        0.25,
        0.5,
        0.75,
        1.0,
    ],
    dtype=np.float32,
)


drive_db = 12.0
threshold = 0.7


# =========================================================
# Hard Clipping
# =========================================================

python_hard = apply_hard_clipping(
    audio_data,
    drive_db=drive_db,
    threshold=threshold,
)


cpp_hard = run_cpp_distortion(
    "hard",
    audio_data,
    drive_db,
    threshold,
)


hard_error = np.max(
    np.abs(
        python_hard
        - cpp_hard
    )
)


hard_match = np.allclose(
    python_hard,
    cpp_hard,
    atol=1e-6,
)


# =========================================================
# Soft Clipping
# =========================================================

python_soft = apply_soft_clipping(
    audio_data,
    drive_db=drive_db,
)


cpp_soft = run_cpp_distortion(
    "soft",
    audio_data,
    drive_db,
    threshold,
)


soft_error = np.max(
    np.abs(
        python_soft
        - cpp_soft
    )
)


soft_match = np.allclose(
    python_soft,
    cpp_soft,
    atol=1e-6,
)


# =========================================================
# Results
# =========================================================

print()
print("========================================")
print("Python ↔ C++ Distortion Comparison")
print("========================================")

print()
print("[Hard Clipping]")
print(
    f"Max absolute error: "
    f"{hard_error:.9f}"
)
print(
    f"Match: {hard_match}"
)

print()
print("[Soft Clipping]")
print(
    f"Max absolute error: "
    f"{soft_error:.9f}"
)
print(
    f"Match: {soft_match}"
)

print()


if not hard_match:
    raise RuntimeError(
        "Hard clipping mismatch."
    )


if not soft_match:
    raise RuntimeError(
        "Soft clipping mismatch."
    )


print(
    "All distortion comparisons passed."
)