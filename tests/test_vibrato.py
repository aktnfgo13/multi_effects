import numpy as np

from effects.vibrato import (
    apply_vibrato,
)


SAMPLE_RATE = 48000


def create_signal():

    time = (
        np.arange(
            SAMPLE_RATE
        )
        / SAMPLE_RATE
    )

    return (
        0.2
        * np.sin(
            2
            * np.pi
            * 440
            * time
        )
    )


def test_vibrato_length():

    signal = create_signal()

    output = apply_vibrato(
        signal,
        SAMPLE_RATE,
    )

    assert (
        len(output)
        == len(signal)
    )


def test_vibrato_changes_signal():

    signal = create_signal()

    output = apply_vibrato(
        signal,
        SAMPLE_RATE,
    )

    assert not np.allclose(
        output,
        signal,
    )


def test_vibrato_finite():

    signal = create_signal()

    output = apply_vibrato(
        signal,
        SAMPLE_RATE,
    )

    assert np.all(
        np.isfinite(output)
    )


def test_vibrato_stereo_shape():

    mono = create_signal()

    stereo = np.column_stack(
        [
            mono,
            mono,
        ]
    )

    output = apply_vibrato(
        stereo,
        SAMPLE_RATE,
    )

    assert (
        output.shape
        == stereo.shape
    )