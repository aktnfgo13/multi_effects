from analysis.test_signal import generate_sine

from analysis.harmonic_analysis import (
    calculate_harmonics,
    calculate_thd,
)

from effects.distortion import (
    apply_hard_clipping,
    apply_soft_clipping,
)


SAMPLE_RATE = 48000
FREQUENCY = 1000

DRIVE_DB = 12.0


# =========================
# Test Signal
# =========================

clean = generate_sine(
    frequency=FREQUENCY,
    sample_rate=SAMPLE_RATE,
    duration=2.0,
    amplitude=0.2,
)


# =========================
# Distortion
# =========================

hard = apply_hard_clipping(
    clean,
    drive_db=DRIVE_DB,
)

soft = apply_soft_clipping(
    clean,
    drive_db=DRIVE_DB,
)


# =========================
# Analysis
# =========================

signals = {
    "CLEAN": clean,
    "HARD": hard,
    "SOFT": soft,
}


for name, signal in signals.items():

    harmonics = calculate_harmonics(
        signal,
        SAMPLE_RATE,
        fundamental=FREQUENCY,
        max_harmonic=10,
    )

    thd = calculate_thd(harmonics)

    print()
    print(f"=== {name} ===")

    for harmonic_number, data in harmonics.items():

        print(
            f"H{harmonic_number}: "
            f"{data['frequency']:.0f} Hz "
            f"| magnitude = "
            f"{data['magnitude']:.2f}"
        )

    print(f"THD: {thd * 100:.2f}%")