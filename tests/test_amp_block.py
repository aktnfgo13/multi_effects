from core.effect_factory import (
    create_amp_sim_block,
)


def test_amp_default_parameters():
    amp = create_amp_sim_block()

    assert (
        amp.get_parameter("gain_db")
        == 12.0
    )

    assert (
        amp.get_parameter("bass_db")
        == 0.0
    )

    assert (
        amp.get_parameter("master_db")
        == -3.0
    )


def test_amp_parameter_change():
    amp = create_amp_sim_block()

    amp.set_parameter(
        "gain_db",
        18.0,
    )

    amp.set_parameter(
        "bass_db",
        3.0,
    )

    assert (
        amp.get_parameter("gain_db")
        == 18.0
    )

    assert (
        amp.get_parameter("bass_db")
        == 3.0
    )


def test_amp_parameter_clamp():
    amp = create_amp_sim_block()

    amp.set_parameter(
        "gain_db",
        100.0,
    )

    assert (
        amp.get_parameter("gain_db")
        == 30.0
    )


def test_amp_bypass():
    amp = create_amp_sim_block()

    amp.set_bypass(True)

    assert amp.bypass is True

    amp.set_bypass(False)

    assert amp.bypass is False