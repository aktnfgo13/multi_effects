import numpy as np

from effects.chorus import (
    apply_chorus,
)


SAMPLE_RATE = 48000


def create_test_signal():
    duration = 1.0

    time = (
        np.arange(
            int(
                SAMPLE_RATE
                * duration
            )
        )
        / SAMPLE_RATE
    )

    return (
        0.2
        * np.sin(
            2
            * np.pi
            * 440.0
            * time
        )
    )


def test_chorus_output_length():
    signal = create_test_signal()

    output = apply_chorus(
        signal,
        SAMPLE_RATE,
    )

    assert (
        len(output)
        == len(signal)
    )


def test_chorus_dry_mix():
    signal = create_test_signal()

    output = apply_chorus(
        signal,
        SAMPLE_RATE,
        mix=0.0,
    )

    assert np.allclose(
        output,
        signal,
    )


def test_chorus_output_is_finite():
    signal = create_test_signal()

    output = apply_chorus(
        signal,
        SAMPLE_RATE,
        rate_hz=0.8,
        depth_ms=5.0,
        base_delay_ms=15.0,
        mix=0.35,
    )

    assert np.all(
        np.isfinite(
            output
        )
    )


def test_chorus_changes_signal():
    signal = create_test_signal()

    output = apply_chorus(
        signal,
        SAMPLE_RATE,
        rate_hz=0.8,
        depth_ms=5.0,
        base_delay_ms=15.0,
        mix=0.5,
    )

    assert not np.allclose(
        output,
        signal,
    )


def test_chorus_stereo_shape():
    mono = create_test_signal()

    stereo = np.column_stack(
        [
            mono,
            mono,
        ]
    )

    output = apply_chorus(
        stereo,
        SAMPLE_RATE,
    )

    assert (
        output.shape
        == stereo.shape
    )