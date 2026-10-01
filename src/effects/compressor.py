import numpy as np


def db_to_linear(db):
    return 10 ** (db / 20)


def linear_to_db(value):
    return 20 * np.log10(np.maximum(value, 1e-12))


def apply_compressor(
    audio_data,
    sample_rate,
    threshold_db=-18.0,
    ratio=4.0,
    attack_ms=10.0,
    release_ms=100.0,
    makeup_gain_db=0.0,
):
    if audio_data.ndim > 1:
        detector_signal = np.mean(
            np.abs(audio_data),
            axis=1,
        )
    else:
        detector_signal = np.abs(audio_data)

    level_db = linear_to_db(detector_signal)

    gain_reduction_db = np.zeros_like(level_db)

    above_threshold = level_db > threshold_db

    gain_reduction_db[above_threshold] = (
        threshold_db
        + (
            level_db[above_threshold]
            - threshold_db
        ) / ratio
        - level_db[above_threshold]
    )

    attack_coeff = np.exp(
        -1.0
        / (
            sample_rate
            * attack_ms
            / 1000.0
        )
    )

    release_coeff = np.exp(
        -1.0
        / (
            sample_rate
            * release_ms
            / 1000.0
        )
    )

    smoothed_gain_db = np.zeros_like(
        gain_reduction_db
    )

    current_gain = 0.0

    for i, target_gain in enumerate(
        gain_reduction_db
    ):
        if target_gain < current_gain:
            coeff = attack_coeff
        else:
            coeff = release_coeff

        current_gain = (
            coeff * current_gain
            + (1 - coeff) * target_gain
        )

        smoothed_gain_db[i] = current_gain

    total_gain_db = (
        smoothed_gain_db
        + makeup_gain_db
    )

    gain_linear = db_to_linear(
        total_gain_db
    )

    if audio_data.ndim > 1:
        gain_linear = gain_linear[:, None]

    output = audio_data * gain_linear

    return output