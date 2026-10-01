from pathlib import Path

import numpy as np
import soundfile as sf

from effects.cabinet_ir import (
    load_ir,
    apply_cabinet_ir,
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

IR_PATH = (
    PROJECT_ROOT
    / "audio"
    / "ir"
    / "cabinet.wav"
)

OUTPUT_PATH = (
    PROJECT_ROOT
    / "audio"
    / "output"
    / "test_cabinet_ir.wav"
)


# =========================
# Load Guitar
# =========================

audio_data, sample_rate = sf.read(
    INPUT_PATH
)


# =========================
# Load IR
# =========================

ir_data = load_ir(
    IR_PATH,
    sample_rate,
)


print("=== CABINET IR TEST ===")

print(
    f"Sample Rate : "
    f"{sample_rate} Hz"
)

print(
    f"IR Samples  : "
    f"{len(ir_data)}"
)

print(
    f"Input Peak  : "
    f"{np.max(np.abs(audio_data)):.4f}"
)


# =========================
# Convolution
# =========================

processed_audio = apply_cabinet_ir(
    audio_data,
    ir_data,
)


print(
    f"Output Peak : "
    f"{np.max(np.abs(processed_audio)):.4f}"
)


# =========================
# Listening Output
# =========================

# 테스트 청취용으로 clipping만 방지
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