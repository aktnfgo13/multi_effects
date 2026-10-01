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

from effects.delay import (
    apply_delay,
)


def create_delay(
    delay_ms,
):
    return EffectBlock(
        name="Delay",
        effect_type="delay",

        processor=apply_delay,

        parameters={
            "delay_ms": delay_ms,
            "feedback": 0.3,
            "mix": 0.2,
        },
    )


def test_duplicate_effect_names_get_unique_ids():
    delay_1 = create_delay(
        120.0
    )

    delay_2 = create_delay(
        600.0
    )

    chain = EffectChain(
        [
            delay_1,
            delay_2,
        ]
    )

    assert (
        delay_1.effect_id
        == "delay_1"
    )

    assert (
        delay_2.effect_id
        == "delay_2"
    )

    assert (
        delay_1.effect_id
        != delay_2.effect_id
    )


def test_duplicate_delay_preset_restore(
    tmp_path,
):
    delay_1 = create_delay(
        120.0
    )

    delay_2 = create_delay(
        600.0
    )

    chain = EffectChain(
        [
            delay_1,
            delay_2,
        ]
    )

    delay_1.name = (
        "Slapback Delay"
    )

    delay_2.name = (
        "Ambient Delay"
    )

    preset_path = (
        tmp_path
        / "dual_delay.json"
    )

    save_preset(
        chain,
        preset_path,
    )

    # 일부러 값 변경
    delay_1.set_parameter(
        "delay_ms",
        900.0,
    )

    delay_2.set_parameter(
        "delay_ms",
        50.0,
    )

    # 순서까지 뒤집기
    chain.effects = [
        delay_2,
        delay_1,
    ]

    load_preset(
        chain,
        preset_path,
    )

    # 순서 복원
    assert (
        chain.effects[0].effect_id
        == "delay_1"
    )

    assert (
        chain.effects[1].effect_id
        == "delay_2"
    )

    # Delay 각각의 값도 정확하게 복원
    assert (
        delay_1.get_parameter(
            "delay_ms"
        )
        == 120.0
    )

    assert (
        delay_2.get_parameter(
            "delay_ms"
        )
        == 600.0
    )

    # 사용자 표시 이름도 복원
    assert (
        delay_1.name
        == "Slapback Delay"
    )

    assert (
        delay_2.name
        == "Ambient Delay"
    )