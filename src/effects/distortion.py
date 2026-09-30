import numpy as np


def apply_hard_clipping(audio_data, drive_db=12.0, threshold=0.7):
    drive_linear = 10 ** (drive_db / 20)

    driven_signal = audio_data * drive_linear

    output = np.clip(
        driven_signal,
        -threshold,
        threshold,
    )

    output = output / threshold

    return output


def apply_soft_clipping(audio_data, drive_db=12.0):
    drive_linear = 10 ** (drive_db / 20)

    driven_signal = audio_data * drive_linear

    # 부드러운 비선형 포화
    output = np.tanh(driven_signal)

    return output