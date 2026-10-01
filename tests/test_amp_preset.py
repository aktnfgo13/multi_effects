from core.effect_chain import (
    EffectChain,
)

from core.effect_factory import (
    create_amp_sim_block,
)

from core.preset import (
    save_preset,
    load_preset,
)


def test_amp_preset_save_and_restore(
    tmp_path,
):
    amp = create_amp_sim_block()

    chain = EffectChain(
        [
            amp,
        ]
    )

    preset_path = (
        tmp_path
        / "amp_test.json"
    )

    # Original
    amp.set_parameter(
        "gain_db",
        16.0,
    )

    amp.set_parameter(
        "bass_db",
        2.0,
    )

    amp.set_parameter(
        "mid_db",
        -2.0,
    )

    amp.set_parameter(
        "treble_db",
        3.0,
    )

    amp.set_parameter(
        "presence_db",
        2.0,
    )

    save_preset(
        chain,
        preset_path,
    )

    # 일부러 변경
    amp.set_parameter(
        "gain_db",
        2.0,
    )

    amp.set_parameter(
        "bass_db",
        -10.0,
    )

    amp.set_parameter(
        "presence_db",
        -10.0,
    )

    # Restore
    load_preset(
        chain,
        preset_path,
    )

    assert (
        amp.get_parameter("gain_db")
        == 16.0
    )

    assert (
        amp.get_parameter("bass_db")
        == 2.0
    )

    assert (
        amp.get_parameter("mid_db")
        == -2.0
    )

    assert (
        amp.get_parameter("treble_db")
        == 3.0
    )

    assert (
        amp.get_parameter("presence_db")
        == 2.0
    )