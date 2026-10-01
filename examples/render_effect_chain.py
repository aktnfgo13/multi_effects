from pathlib import Path

import numpy as np
import soundfile as sf

from core.effect import (
    EffectBlock,
)

from core.effect_chain import (
    EffectChain,
)

from effects.noise_gate import (
    apply_noise_gate,
)

from effects.compressor import (
    apply_compressor,
)

from effects.distortion import (
    apply_soft_clipping,
)

from effects.cabinet_ir import (
    load_ir,
    apply_cabinet_ir,
)

from effects.delay import (
    apply_delay,
)

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
    / "test_effect_chain.wav"
)


# =========================================================
# Audio Load
# =========================================================

audio_data, sample_rate = sf.read(
    INPUT_PATH
)


# =========================================================
# Cabinet IR Load
# =========================================================

ir_data = load_ir(
    IR_PATH,
    sample_rate,
)


# =========================================================
# Effect Blocks
# =========================================================

noise_gate = EffectBlock(
    name="Noise Gate",
    processor=apply_noise_gate,
    parameters={
        "threshold_db": -50.0,
        "attack_ms": 5.0,
        "release_ms": 100.0,
    },
)


compressor = EffectBlock(
    name="Compressor",
    processor=apply_compressor,
    parameters={
        "threshold_db": -18.0,
        "ratio": 4.0,
        "attack_ms": 10.0,
        "release_ms": 100.0,
        "makeup_gain_db": 3.0,
    },
    bypass=True
)


distortion = EffectBlock(
    name="Distortion",
    processor=apply_soft_clipping,
    parameters={
        "drive_db": 8.0,
    },
    use_sample_rate=False,
)


cabinet = EffectBlock(
    name="Cabinet IR",
    processor=apply_cabinet_ir,
    parameters={
        "ir_data": ir_data,
    },
    use_sample_rate=False,
)


delay = EffectBlock(
    name="Delay",
    processor=apply_delay,
    parameters={
        "delay_ms": 400.0,
        "feedback": 0.3,
        "mix": 0.2,
    },
)


reverb = EffectBlock(
    name="Reverb",
    processor=apply_reverb,
    parameters={
        "room_size": 0.6,
        "damping": 0.4,
        "mix": 0.2,
        "tail_seconds": 2.0,
    },
)


# =========================================================
# Effect Chain
# =========================================================

chain = EffectChain(
    [
        noise_gate,
        compressor,
        distortion,
        cabinet,
        delay,
        reverb,
    ]
)


chain.print_chain()


# =========================================================
# DSP Processing
# =========================================================

processed_audio = chain.process(
    audio_data,
    sample_rate,
)


# =========================================================
# Output Protection
# =========================================================

peak = np.max(
    np.abs(processed_audio)
)

print()
print(
    f"Peak Before Normalize: "
    f"{peak:.4f}"
)


if peak > 0.95:

    processed_audio = (
        processed_audio
        / peak
        * 0.95
    )


# =========================================================
# Save
# =========================================================

sf.write(
    OUTPUT_PATH,
    processed_audio,
    sample_rate,
)


print()
print(
    f"Saved: {OUTPUT_PATH}"
)