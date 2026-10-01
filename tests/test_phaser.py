import numpy as np

from effects.phaser import (
    apply_phaser,
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


def test_phaser_output_length():
    signal = create_test_signal()

    output = apply_phaser(
        signal,
        SAMPLE_RATE,
    )

    assert (
        len(output)
        == len(signal)
    )


def test_phaser_dry_mix():
    signal = create_test_signal()

    output = apply_phaser(
        signal,
        SAMPLE_RATE,
        mix=0.0,
    )

    assert np.allclose(
        output,
        signal,
    )


def test_phaser_output_is_finite():
    signal = create_test_signal()

    output = apply_phaser(
        signal,
        SAMPLE_RATE,
    )

    assert np.all(
        np.isfinite(
            output
        )
    )


def test_phaser_changes_signal():
    signal = create_test_signal()

    output = apply_phaser(
        signal,
        SAMPLE_RATE,
        mix=0.5,
    )

    assert not np.allclose(
        output,
        signal,
    )


def test_phaser_stereo_shape():
    mono = create_test_signal()

    stereo = np.column_stack(
        [
            mono,
            mono,
        ]
    )

    output = apply_phaser(
        stereo,
        SAMPLE_RATE,
    )

    assert (
        output.shape
        == stereo.shape
    )