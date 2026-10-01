from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from analysis.audio_analysis import (
    calculate_fft,
    calculate_peak,
    calculate_rms,
)

from analysis.harmonic_analysis import (
    calculate_harmonics,
    calculate_thd,
)

from analysis.test_signal import (
    generate_sine,
)

from effects.amp_sim import (
    apply_amp_sim,
)


PROJECT_ROOT = (
    Path(__file__)
    .resolve()
    .parent
    .parent
)

OUTPUT_PATH = (
    PROJECT_ROOT
    / "docs"
    / "amp_sim_analysis.png"
)


# =========================================================
# Test Signal
# =========================================================

sample_rate = 48000
frequency = 1000
duration = 2.0


test_signal = generate_sine(
    frequency=frequency,
    sample_rate=sample_rate,
    duration=duration,
    amplitude=0.1,
)


# =========================================================
# Amp Settings
# =========================================================

settings = {
    "Clean-ish": {
        "gain_db": 3.0,
        "drive": 0.8,
        "bias": 0.0,
        "master_db": -12.0,
        "power_drive": 0.8,
    },

    "Preamp Drive": {
        "gain_db": 12.0,
        "drive": 1.5,
        "bias": 0.0,
        "master_db": -12.0,
        "power_drive": 0.8,
    },

    "Biased Preamp": {
        "gain_db": 12.0,
        "drive": 1.5,
        "bias": 0.08,
        "master_db": -12.0,
        "power_drive": 0.8,
    },

    "Power Amp Push": {
        "gain_db": 12.0,
        "drive": 1.5,
        "bias": 0.05,
        "master_db": 3.0,
        "power_drive": 1.8,
    },
}


results = {}


# =========================================================
# Analysis
# =========================================================

for name, params in settings.items():

    output = apply_amp_sim(
        test_signal,
        sample_rate,

        gain_db=params["gain_db"],
        drive=params["drive"],
        bias=params["bias"],

        bass_db=0.0,
        mid_db=0.0,
        treble_db=0.0,

        master_db=params["master_db"],
        power_drive=params[
            "power_drive"
        ],
        presence_db=0.0,

        low_cut_hz=70.0,
        high_cut_hz=12000.0,

        output_db=-6.0,
    )

    # 필터 시작 부분의 transient 영향을 줄이기 위해
    # 뒤쪽 1초만 분석
    analysis_signal = output[
        sample_rate:
    ]

    harmonics = calculate_harmonics(
        analysis_signal,
        sample_rate,
        fundamental=frequency,
        max_harmonic=10,
    )

    thd = calculate_thd(
        harmonics
    )

    peak = calculate_peak(
        analysis_signal
    )

    rms = calculate_rms(
        analysis_signal
    )

    fft_frequency, fft_db = (
        calculate_fft(
            analysis_signal,
            sample_rate,
        )
    )

    results[name] = {
        "output": output,
        "harmonics": harmonics,
        "thd": thd,
        "peak": peak,
        "rms": rms,
        "fft_frequency": fft_frequency,
        "fft_db": fft_db,
    }


# =========================================================
# Console Result
# =========================================================

print(
    "=== AMP SIM ANALYSIS ==="
)


for name, result in results.items():

    print()
    print(
        f"[{name}]"
    )

    print(
        f"Peak : "
        f"{result['peak']:.4f}"
    )

    print(
        f"RMS  : "
        f"{result['rms']:.4f}"
    )

    print(
        f"THD  : "
        f"{result['thd'] * 100:.2f} %"
    )

    print(
        "Harmonics:"
    )

    fundamental_magnitude = (
        result[
            "harmonics"
        ][1]["magnitude"]
    )

    for harmonic_number, data in (
        result[
            "harmonics"
        ].items()
    ):

        relative = (
            data["magnitude"]
            / fundamental_magnitude
        )

        print(
            f"  H{harmonic_number}: "
            f"{data['frequency']:.0f} Hz "
            f"({relative:.4f})"
        )


# =========================================================
# FFT Graph
# =========================================================

plt.figure(
    figsize=(12, 7)
)


for name, result in results.items():

    frequencies = result[
        "fft_frequency"
    ]

    magnitude_db = result[
        "fft_db"
    ]

    # Guitar / harmonic 분석에 필요한 구간만 표시
    mask = (
        frequencies <= 12000
    )

    plt.plot(
        frequencies[mask],
        magnitude_db[mask],
        label=name,
    )


plt.title(
    "Amp Sim Harmonic / Frequency Analysis"
)

plt.xlabel(
    "Frequency (Hz)"
)

plt.ylabel(
    "Magnitude (dB)"
)

plt.xlim(
    0,
    12000,
)

plt.grid()

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