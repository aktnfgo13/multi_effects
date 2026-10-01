from pathlib import Path

import numpy as np
import soundfile as sf

from effects.reverb import (
    apply_reverb,
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
    / "audio"
    / "output"
    / "test_reverb.wav"
)


audio_data, sample_rate = sf.read(
    INPUT_PATH
)


processed_audio = apply_reverb(
    audio_data,
    sample_rate,
    room_size=0.6,
    damping=0.4,
    mix=0.3,
    tail_seconds=2.0,
)


print(
    "=== REVERB TEST ==="
)

print(
    f"Sample Rate : "
    f"{sample_rate} Hz"
)

print(
    "Room Size   : 0.60"
)

print(
    "Damping     : 0.40"
)

print(
    "Mix         : 0.30"
)

print(
    f"Input Length  : "
    f"{len(audio_data) / sample_rate:.2f} sec"
)

print(
    f"Output Length : "
    f"{len(processed_audio) / sample_rate:.2f} sec"
)


# 청취 파일의 clipping 방지
peak = np.max(
    np.abs(processed_audio)
)

if peak > 0.95:

    processed_audio = (
        processed_audio
        / peak
        * 0.95
    )


sf.write(
    OUTPUT_PATH,
    processed_audio,
    sample_rate,
)


print()
print(
    f"Saved: {OUTPUT_PATH}"
)