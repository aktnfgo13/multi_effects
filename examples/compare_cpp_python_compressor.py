from pathlib import Path
import subprocess
import sys

import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = PROJECT_ROOT / "src"

sys.path.insert(0, str(SRC_DIR))


from effects.compressor import apply_compressor


CPP_EXECUTABLE = (
    PROJECT_ROOT
    / "cpp"
    / "build"
    / "compressor_cli"
)


def run_cpp_compressor(
    audio_data,
    sample_rate,
    threshold_db,
    ratio,
    attack_ms,
    release_ms,
    makeup_gain_db,
):
    command = [
        str(CPP_EXECUTABLE),
        str(sample_rate),
        str(threshold_db),
        str(ratio),
        str(attack_ms),
        str(release_ms),
        str(makeup_gain_db),
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


sample_rate = 44100

t = (
    np.arange(4096)
    / sample_rate
)

audio_data = (
    0.8
    * np.sin(
        2 * np.pi * 440 * t
    )
).astype(np.float32)


threshold_db = -18.0
ratio = 4.0
attack_ms = 10.0
release_ms = 100.0
makeup_gain_db = 3.0


python_output = apply_compressor(
    audio_data,
    sample_rate,
    threshold_db=threshold_db,
    ratio=ratio,
    attack_ms=attack_ms,
    release_ms=release_ms,
    makeup_gain_db=makeup_gain_db,
)


cpp_output = run_cpp_compressor(
    audio_data,
    sample_rate,
    threshold_db,
    ratio,
    attack_ms,
    release_ms,
    makeup_gain_db,
)


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
print("========================================")
print("Python ↔ C++ Compressor Comparison")
print("========================================")
print()
print(
    f"Max absolute error: "
    f"{error:.9f}"
)
print(
    f"Match: {match}"
)
print()


if not match:
    raise RuntimeError(
        "Compressor mismatch."
    )


print(
    "Compressor comparison passed."
)