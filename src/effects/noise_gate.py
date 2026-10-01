import numpy as np


def db_to_linear(db):
    return 10 ** (db / 20)


def apply_noise_gate(
    audio_data,
    sample_rate,
    threshold_db=-45.0,
    attack_ms=5.0,
    release_ms=100.0,
):
    """
    Basic Noise Gate

    threshold_db:
        이 값보다 작은 신호를 줄임

    attack_ms:
        Gate가 열리는 속도

    release_ms:
        Gate가 닫히는 속도
    """

    threshold = db_to_linear(
        threshold_db
    )

    # Stereo면 detector용 mono 신호 생성
    if audio_data.ndim > 1:
        detector = np.max(
            np.abs(audio_data),
            axis=1,
        )
    else:
        detector = np.abs(audio_data)

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

    gain = np.zeros(
        len(detector)
    )

    current_gain = 0.0

    for i, level in enumerate(detector):

        if level >= threshold:
            target_gain = 1.0
            coeff = attack_coeff

        else:
            target_gain = 0.0
            coeff = release_coeff

        current_gain = (
            coeff * current_gain
            + (1 - coeff) * target_gain
        )

        gain[i] = current_gain

    if audio_data.ndim > 1:
        gain = gain[:, None]

    output = audio_data * gain

    return output
