from core.effect import EffectBlock
from core.parameter import ParameterSpec
from effects.delay import apply_delay


def create_delay_block():
    return EffectBlock(
        name="Delay",
        processor=apply_delay,
        parameters={
            "delay_ms": 400.0,
            "feedback": 0.3,
            "mix": 0.2,
        },
        parameter_specs={
            "delay_ms": ParameterSpec(
                name="Delay Time",
                default=400.0,
                minimum=1.0,
                maximum=2000.0,
                unit="ms",
                step=1.0,
            ),
            "feedback": ParameterSpec(
                name="Feedback",
                default=0.3,
                minimum=0.0,
                maximum=0.95,
                step=0.01,
            ),
            "mix": ParameterSpec(
                name="Mix",
                default=0.2,
                minimum=0.0,
                maximum=1.0,
                step=0.01,
            ),
        },
    )


def test_parameter_normal_value():
    delay = create_delay_block()

    delay.set_parameter(
        "delay_ms",
        750.0,
    )

    assert (
        delay.get_parameter("delay_ms")
        == 750.0
    )


def test_parameter_maximum_clamp():
    delay = create_delay_block()

    delay.set_parameter(
        "delay_ms",
        5000.0,
    )

    assert (
        delay.get_parameter("delay_ms")
        == 2000.0
    )


def test_parameter_minimum_clamp():
    delay = create_delay_block()

    delay.set_parameter(
        "feedback",
        -5.0,
    )

    assert (
        delay.get_parameter("feedback")
        == 0.0
    )


def test_parameter_reset():
    delay = create_delay_block()

    delay.set_parameter(
        "delay_ms",
        1000.0,
    )

    delay.reset_parameter(
        "delay_ms"
    )

    assert (
        delay.get_parameter("delay_ms")
        == 400.0
    )


def test_parameter_spec():
    delay = create_delay_block()

    spec = delay.get_parameter_spec(
        "delay_ms"
    )

    assert spec.name == "Delay Time"
    assert spec.default == 400.0
    assert spec.minimum == 1.0
    assert spec.maximum == 2000.0
    assert spec.unit == "ms"
    assert spec.step == 1.0