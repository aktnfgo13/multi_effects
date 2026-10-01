from core.effect_chain import EffectChain

from core.effect_factory import (
    create_chorus_block,
    create_flanger_block,
    create_phaser_block,
    create_tremolo_block,
    create_vibrato_block,
)

from core.preset import (
    save_preset,
    load_preset,
)


def test_modulation_factory_types():
    effects = [
        create_chorus_block(),
        create_flanger_block(),
        create_phaser_block(),
        create_tremolo_block(),
        create_vibrato_block(),
    ]

    chain = EffectChain(effects)

    assert chain.effects[0].effect_type == "chorus"
    assert chain.effects[1].effect_type == "flanger"
    assert chain.effects[2].effect_type == "phaser"
    assert chain.effects[3].effect_type == "tremolo"
    assert chain.effects[4].effect_type == "vibrato"


def test_modulation_unique_ids():
    chorus_1 = create_chorus_block()
    chorus_2 = create_chorus_block()

    chain = EffectChain(
        [
            chorus_1,
            chorus_2,
        ]
    )

    assert chorus_1.effect_id == "chorus_1"
    assert chorus_2.effect_id == "chorus_2"


def test_modulation_parameter_clamp():
    chorus = create_chorus_block()

    chorus.set_parameter(
        "mix",
        10.0,
    )

    assert (
        chorus.get_parameter("mix")
        == 1.0
    )


def test_modulation_preset_restore(
    tmp_path,
):
    chorus = create_chorus_block(
        name="Wide Chorus"
    )

    tremolo = create_tremolo_block(
        name="Slow Tremolo"
    )

    chain = EffectChain(
        [
            chorus,
            tremolo,
        ]
    )

    chorus.set_parameter(
        "rate_hz",
        0.35,
    )

    chorus.set_parameter(
        "mix",
        0.42,
    )

    tremolo.set_parameter(
        "rate_hz",
        2.5,
    )

    preset_path = (
        tmp_path
        / "modulation.json"
    )

    save_preset(
        chain,
        preset_path,
    )

    # 일부러 변경
    chorus.set_parameter(
        "rate_hz",
        4.0,
    )

    chorus.set_parameter(
        "mix",
        0.1,
    )

    tremolo.set_parameter(
        "rate_hz",
        10.0,
    )

    # 복원
    load_preset(
        chain,
        preset_path,
    )

    assert (
        chorus.get_parameter("rate_hz")
        == 0.35
    )

    assert (
        chorus.get_parameter("mix")
        == 0.42
    )

    assert (
        tremolo.get_parameter("rate_hz")
        == 2.5
    )