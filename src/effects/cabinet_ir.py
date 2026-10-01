import math

import numpy as np
import soundfile as sf

from scipy.signal import (
    fftconvolve,
    resample_poly,
)


def load_ir(
    ir_path,
    target_sample_rate,
):
    """
    Cabinet IR WAV를 읽고
    입력 신호의 Sample Rate에 맞춘다.
    """

    ir_data, ir_sample_rate = sf.read(
        ir_path
    )

    # Stereo IR -> Mono
    if ir_data.ndim > 1:
        ir_data = np.mean(
            ir_data,
            axis=1,
        )

    # DC 제거
    ir_data = (
        ir_data
        - np.mean(ir_data)
    )

    # Sample Rate가 다르면 resampling
    if ir_sample_rate != target_sample_rate:

        gcd = math.gcd(
            ir_sample_rate,
            target_sample_rate,
        )

        up = (
            target_sample_rate
            // gcd
        )

        down = (
            ir_sample_rate
            // gcd
        )

        ir_data = resample_poly(
            ir_data,
            up,
            down,
        )

    # IR peak normalization
    peak = np.max(
        np.abs(ir_data)
    )

    if peak > 0:
        ir_data = (
            ir_data
            / peak
        )

    return ir_data


def apply_cabinet_ir(
    audio_data,
    ir_data,
):
    """
    FFT Convolution을 이용해
    Cabinet IR을 오디오에 적용한다.
    """

    # Mono input
    if audio_data.ndim == 1:

        output = fftconvolve(
            audio_data,
            ir_data,
            mode="full",
        )

        # 원래 입력 길이에 맞춤
        return output[
            :len(audio_data)
        ]

    # Stereo / Multi-channel input
    output = np.zeros_like(
        audio_data,
        dtype=np.float64,
    )

    for channel in range(
        audio_data.shape[1]
    ):
        convolved = fftconvolve(
            audio_data[:, channel],
            ir_data,
            mode="full",
        )

        output[:, channel] = (
            convolved[
                :len(audio_data)
            ]
        )

    return output