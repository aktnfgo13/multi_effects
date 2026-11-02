"""Compare the unchanged mono reference against the streaming C++ gate."""
import argparse
from pathlib import Path
import subprocess

import numpy as np
from effects.noise_gate import apply_noise_gate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path)
    args = parser.parse_args()
    build = Path(__file__).resolve().parents[1] / "cpp" / "build"
    candidates = [build / "noise_gate_cli", build / "noise_gate_cli.exe",
                  build / "Debug" / "noise_gate_cli.exe",
                  build / "Release" / "noise_gate_cli.exe"]
    executable = args.executable or next((p for p in candidates if p.is_file()), None)
    if executable is None:
        raise FileNotFoundError("Build noise_gate_cli first")
    for rate in (44100, 48000, 96000):
        # Exact threshold=1 makes the equality branch unambiguous in both precisions.
        for threshold in (-45.0, 0.0):
            level = 10 ** (threshold / 20)
            signal = np.concatenate([
                np.zeros(97), np.full(3000, level * 2),
                np.full(3000, level * 0.5),
                np.full(300, level if threshold == 0.0 else level * 1.01),
                np.full(3000, -level * 2), np.zeros(10000),
            ]).astype(np.float32)
            reference = apply_noise_gate(signal.astype(np.float64), rate,
                                         threshold, 5.0, 100.0)
            result = subprocess.run(
                [str(executable), str(rate), str(threshold), "5", "100"],
                input="\n".join(map(str, signal.tolist())),
                capture_output=True, text=True, check=True)
            actual = np.fromstring(result.stdout, sep="\n")
            if actual.shape != reference.shape:
                raise AssertionError("Output shape mismatch")
            error = np.max(np.abs(actual - reference))
            print(f"rate={rate} threshold={threshold} max_abs_error={error:.9g}")
            np.testing.assert_allclose(actual, reference, atol=1e-4, rtol=1e-5)


if __name__ == "__main__":
    main()
