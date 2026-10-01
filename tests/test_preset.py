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

from effects.compressor import (
    apply_compressor,
)

from effects.delay import (
    apply_delay,
)

from effects.distortion import (
    apply_soft_clipping,
)


def create_chain():
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

    chain = EffectChain(
        [
            compressor,
            distortion,
            delay,
        ]
    )

    return (
        chain,
        compressor,
        distortion,
        delay,
    )


def test_preset_restore(
    tmp_path,
):
    (
        chain,
        compressor,
        distortion,
        delay,
    ) = create_chain()

    preset_path = (
        tmp_path
        / "test_preset.json"
    )

    save_preset(
        chain,
        preset_path,
    )

    # 설정 변경
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

    # 프리셋 복원
    load_preset(
        chain,
        preset_path,
    )

    assert (
        delay.get_parameter("delay_ms")
        == 400.0
    )

    assert (
        distortion.bypass
        is False
    )

    assert (
        compressor.get_parameter("ratio")
        == 4.0
    )


def test_preset_effect_order(
    tmp_path,
):
    (
        chain,
        compressor,
        distortion,
        delay,
    ) = create_chain()

    preset_path = (
        tmp_path
        / "order_test.json"
    )

    save_preset(
        chain,
        preset_path,
    )

    # 일부러 순서를 변경
    chain.effects = [
        delay,
        compressor,
        distortion,
    ]

    load_preset(
        chain,
        preset_path,
    )

    names = [
        effect.name
        for effect in chain.effects
    ]

    assert names == [
        "Compressor",
        "Distortion",
        "Delay",
    ]