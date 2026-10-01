from pathlib import Path

import matplotlib.pyplot as plt
import soundfile as sf

from effects.distortion import (
    apply_hard_clipping,
    apply_soft_clipping,
)

from analysis.envelope import (
    calculate_envelope,
    calculate_transient_strength,
    calculate_attack_time,
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
    / "envelope_comparison.png"
)


audio_data, sample_rate = sf.read(
    INPUT_PATH
)


DRIVE_DB = 12.0


hard_audio = apply_hard_clipping(
    audio_data,
    drive_db=DRIVE_DB,
)

soft_audio = apply_soft_clipping(
    audio_data,
    drive_db=DRIVE_DB,
)


signals = {
    "Original": audio_data,
    "Hard": hard_audio,
    "Soft": soft_audio,
}


plt.figure(figsize=(12, 6))


for name, signal in signals.items():

    envelope = calculate_envelope(
        signal,
        sample_rate,
    )

    transient = calculate_transient_strength(
        envelope
    )

    attack_time = calculate_attack_time(
        envelope,
        sample_rate,
    )

    print()
    print(f"=== {name} ===")
    print(
        f"Envelope Peak : "
        f"{envelope.max():.4f}"
    )
    print(
        f"Transient Peak: "
        f"{transient.max():.6f}"
    )
    print(
        f"Attack Time   : "
        f"{attack_time:.2f} ms"
    )

    time_axis = (
        range(len(envelope))
    )

    plt.plot(
        time_axis,
        envelope,
        label=name,
    )


plt.title(
    "Envelope Comparison"
)

plt.xlabel("Sample")
plt.ylabel("Amplitude")

plt.legend()
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