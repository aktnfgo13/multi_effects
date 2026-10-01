from pathlib import Path

from core.effect import (
    EffectBlock,
)

from core.effect_chain import (
    EffectChain,
)

from core.preset import (
    save_preset,
    load_preset,
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

PRESET_PATH = (
    PROJECT_ROOT
    / "presets"
    / "test_preset.json"
)


# =========================================================
# Effect 생성
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
)


distortion = EffectBlock(
    name="Distortion",
    processor=apply_soft_clipping,
    parameters={
        "drive_db": 8.0,
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


chain = EffectChain(
    [
        noise_gate,
        compressor,
        distortion,
        delay,
        reverb,
    ]
)


# =========================================================
# 현재 설정
# =========================================================

print(
    "=== ORIGINAL CHAIN ==="
)

chain.print_chain()

print()

print(
    "Delay Time:",
    delay.get_parameter(
        "delay_ms"
    ),
)

print(
    "Distortion Bypass:",
    distortion.bypass,
)


# =========================================================
# Preset 저장
# =========================================================

save_preset(
    chain,
    PRESET_PATH,
)

print()
print(
    f"Preset Saved: "
    f"{PRESET_PATH}"
)


# =========================================================
# 일부러 설정 변경
# =========================================================

delay.set_parameter(
    "delay_ms",
    900.0,
)

distortion.set_bypass(
    True
)

compressor.set_parameter(
    "ratio",
    10.0,
)


print()
print(
    "=== MODIFIED ==="
)

print(
    "Delay Time:",
    delay.get_parameter(
        "delay_ms"
    ),
)

print(
    "Distortion Bypass:",
    distortion.bypass,
)

print(
    "Compressor Ratio:",
    compressor.get_parameter(
        "ratio"
    ),
)


# =========================================================
# Preset 다시 Load
# =========================================================

load_preset(
    chain,
    PRESET_PATH,
)


print()
print(
    "=== PRESET RESTORED ==="
)

chain.print_chain()

print()

print(
    "Delay Time:",
    delay.get_parameter(
        "delay_ms"
    ),
)

print(
    "Distortion Bypass:",
    distortion.bypass,
)

print(
    "Compressor Ratio:",
    compressor.get_parameter(
        "ratio"
    ),
)