import numpy as np


def apply_distortion(audio_data, drive_db=12.0, threshold=0.7):
    # 1. dB를 실제 배율로 변환
    drive_linear = 10 ** (drive_db / 20)

    # 2. 입력 신호 증폭
    driven_signal = audio_data * drive_linear

    # 3. Hard Clipping
    output = np.clip(
        driven_signal,
        -threshold,
        threshold,
    )

    # 4. 다시 -1 ~ 1 범위로 정규화
    output = output / threshold

    return output