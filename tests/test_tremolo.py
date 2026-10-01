import numpy as np

from effects.tremolo import (
    apply_tremolo,
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


def test_tremolo_length():

    signal = create_signal()

    output = apply_tremolo(
        signal,
        SAMPLE_RATE,
    )

    assert (
        len(output)
        == len(signal)
    )


def test_tremolo_depth_zero():

    signal = create_signal()

    output = apply_tremolo(
        signal,
        SAMPLE_RATE,
        depth=0.0,
    )

    assert np.allclose(
        signal,
        output,
    )


def test_tremolo_changes_signal():

    signal = create_signal()

    output = apply_tremolo(
        signal,
        SAMPLE_RATE,
        depth=0.8,
    )

    assert not np.allclose(
        signal,
        output,
    )


def test_tremolo_finite():

    signal = create_signal()

    output = apply_tremolo(
        signal,
        SAMPLE_RATE,
    )

    assert np.all(
        np.isfinite(output)
    )


def test_tremolo_stereo_shape():

    mono = create_signal()

    stereo = np.column_stack(
        [
            mono,
            mono,
        ]
    )

    output = apply_tremolo(
        stereo,
        SAMPLE_RATE,
    )

    assert (
        output.shape
        == stereo.shape
    )