import numpy as np


def peaking_eq(
    audio_data,
    sample_rate,
    frequency=1000.0,
    gain_db=6.0,
    q=1.0,
):
    """
    Parametric Peaking EQ

    frequency : 중심 주파수 (Hz)
    gain_db   : boost / cut (dB)
    q         : 대역폭
    """

    A = 10 ** (gain_db / 40)
    omega = 2 * np.pi * frequency / sample_rate

    alpha = np.sin(omega) / (2 * q)
    cos_omega = np.cos(omega)

    b0 = 1 + alpha * A
    b1 = -2 * cos_omega
    b2 = 1 - alpha * A

    a0 = 1 + alpha / A
    a1 = -2 * cos_omega
    a2 = 1 - alpha / A

    # Normalize
    b0 /= a0
    b1 /= a0
    b2 /= a0

    a1 /= a0
    a2 /= a0

    return _apply_biquad(
        audio_data,
        b0,
        b1,
        b2,
        a1,
        a2,
    )


def _apply_biquad(
    audio_data,
    b0,
    b1,
    b2,
    a1,
    a2,
):
    if audio_data.ndim == 1:
        return _process_channel(
            audio_data,
            b0,
            b1,
            b2,
            a1,
            a2,
        )

    output = np.zeros_like(audio_data)

    for channel in range(audio_data.shape[1]):
        output[:, channel] = _process_channel(
            audio_data[:, channel],
            b0,
            b1,
            b2,
            a1,
            a2,
        )

    return output


def _process_channel(
    signal,
    b0,
    b1,
    b2,
    a1,
    a2,
):
    output = np.zeros_like(signal)

    x1 = 0.0
    x2 = 0.0
    y1 = 0.0
    y2 = 0.0

    for i, x0 in enumerate(signal):

        y0 = (
            b0 * x0
            + b1 * x1
            + b2 * x2
            - a1 * y1
            - a2 * y2
        )

        output[i] = y0

        x2 = x1
        x1 = x0

        y2 = y1
        y1 = y0

    return output