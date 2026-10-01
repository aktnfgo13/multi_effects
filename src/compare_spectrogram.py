from pathlib import Path

import matplotlib.pyplot as plt
import soundfile as sf

from effects.distortion import (
    apply_hard_clipping,
    apply_soft_clipping,
)

from analysis.spectrogram import (
    calculate_stft,
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
    "Hard Clipping": hard_audio,
    "Soft Clipping": soft_audio,
}


for name, signal in signals.items():

    spectrogram, frequencies, times = (
        calculate_stft(
            signal,
            sample_rate,
        )
    )

    plt.figure(figsize=(12, 5))

    plt.imshow(
        spectrogram,
        origin="lower",
        aspect="auto",
        extent=[
            times[0],
            times[-1],
            frequencies[0],
            frequencies[-1],
        ],
    )

    plt.ylim(0, 12000)

    plt.title(
        f"{name} - Spectrogram"
    )

    plt.xlabel("Time (sec)")
    plt.ylabel("Frequency (Hz)")

    plt.colorbar(
        label="Magnitude (dB)"
    )

    plt.tight_layout()

    output_path = (
        PROJECT_ROOT
        / "docs"
        / f"{name.lower().replace(' ', '_')}_spectrogram.png"
    )

    output_path.parent.mkdir(
    parents=True,
    exist_ok=True,
    )
    
    plt.savefig(output_path)

    plt.show()

    print(
        f"Saved: {output_path}"
    )