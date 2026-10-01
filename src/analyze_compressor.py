from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import soundfile as sf

from effects.compressor import (
    apply_compressor,
)


PROJECT_ROOT = (
    Path(__file__)
    .resolve()
    .parent
    .parent
)

INPUT_PATH = (
    PROJECT_ROOT
    / "audio"
    / "input"
    / "test.wav"
)

OUTPUT_PATH = (
    PROJECT_ROOT
    / "docs"
    / "compressor_gain_reduction.png"
)


audio_data, sample_rate = sf.read(
    INPUT_PATH
)


processed_audio, gain_reduction_db = (
    apply_compressor(
        audio_data,
        sample_rate,
        threshold_db=-18.0,
        ratio=4.0,
        attack_ms=10.0,
        release_ms=100.0,
        makeup_gain_db=0.0,
        return_gain_reduction=True,
    )
)


if audio_data.ndim > 1:
    input_mono = np.mean(
        audio_data,
        axis=1,
    )
else:
    input_mono = audio_data


time = (
    np.arange(len(input_mono))
    / sample_rate
)


print("=== COMPRESSOR ANALYSIS ===")

print(
    f"Maximum Gain Reduction : "
    f"{gain_reduction_db.min():.2f} dB"
)

print(
    f"Average Gain Reduction : "
    f"{gain_reduction_db.mean():.2f} dB"
)


plt.figure(
    figsize=(12, 6)
)

plt.plot(
    time,
    gain_reduction_db,
)

plt.title(
    "Compressor Gain Reduction"
)

plt.xlabel(
    "Time (sec)"
)

plt.ylabel(
    "Gain Reduction (dB)"
)

plt.grid()

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