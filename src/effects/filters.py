import numpy as np


def _process_channel(
    signal,
    b0,
    b1,
    b2,
    a1,
    a2,
):
    """
    하나의 mono channel에 Biquad Filter 적용
    """

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


def _apply_biquad(
    audio_data,
    b0,
    b1,
    b2,
    a1,
    a2,
):
    """
    Mono / Stereo 모두 처리 가능한 공통 Biquad 함수
    """

    # Mono
    if audio_data.ndim == 1:
        return _process_channel(
            audio_data,
            b0,
            b1,
            b2,
            a1,
            a2,
        )

    # Stereo / Multi Channel
    output = np.zeros_like(audio_data)

    for channel in range(
        audio_data.shape[1]
    ):
        output[:, channel] = (
            _process_channel(
                audio_data[:, channel],
                b0,
                b1,
                b2,
                a1,
                a2,
            )
        )

    return output


# =========================================================
# Low Pass Filter
# =========================================================

def low_pass_filter(
    audio_data,
    sample_rate,
    cutoff=8000.0,
    q=0.707,
):
    """
    Low Pass Filter

    cutoff:
        cutoff frequency (Hz)

    q:
        resonance / filter shape
        0.707 = 비교적 평탄한 Butterworth 응답
    """

    omega = (
        2
        * np.pi
        * cutoff
        / sample_rate
    )

    sin_omega = np.sin(omega)
    cos_omega = np.cos(omega)

    alpha = (
        sin_omega
        / (2 * q)
    )

    b0 = (
        1 - cos_omega
    ) / 2

    b1 = (
        1 - cos_omega
    )

    b2 = (
        1 - cos_omega
    ) / 2

    a0 = (
        1 + alpha
    )

    a1 = (
        -2 * cos_omega
    )

    a2 = (
        1 - alpha
    )

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


# =========================================================
# High Pass Filter
# =========================================================

def high_pass_filter(
    audio_data,
    sample_rate,
    cutoff=80.0,
    q=0.707,
):
    """
    High Pass Filter

    cutoff:
        cutoff frequency (Hz)

    q:
        resonance / filter shape
    """

    omega = (
        2
        * np.pi
        * cutoff
        / sample_rate
    )

    sin_omega = np.sin(omega)
    cos_omega = np.cos(omega)

    alpha = (
        sin_omega
        / (2 * q)
    )

    b0 = (
        1 + cos_omega
    ) / 2

    b1 = -(
        1 + cos_omega
    )

    b2 = (
        1 + cos_omega
    ) / 2

    a0 = (
        1 + alpha
    )

    a1 = (
        -2 * cos_omega
    )

    a2 = (
        1 - alpha
    )

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


# =========================================================
# Low Shelf Filter
# =========================================================

def low_shelf_filter(
    audio_data,
    sample_rate,
    frequency=200.0,
    gain_db=6.0,
    slope=1.0,
):
    """
    Low Shelf Filter

    frequency:
        shelf 중심 주파수

    gain_db:
        저역 boost / cut

    slope:
        shelf 기울기
    """

    A = 10 ** (
        gain_db / 40
    )

    omega = (
        2
        * np.pi
        * frequency
        / sample_rate
    )

    sin_omega = np.sin(
        omega
    )

    cos_omega = np.cos(
        omega
    )

    alpha = (
        sin_omega
        / 2
        * np.sqrt(
            (
                A
                + 1 / A
            )
            * (
                1 / slope
                - 1
            )
            + 2
        )
    )

    sqrt_A = np.sqrt(
        A
    )

    b0 = (
        A
        * (
            A + 1
            - (
                A - 1
            )
            * cos_omega
            + 2
            * sqrt_A
            * alpha
        )
    )

    b1 = (
        2
        * A
        * (
            A - 1
            - (
                A + 1
            )
            * cos_omega
        )
    )

    b2 = (
        A
        * (
            A + 1
            - (
                A - 1
            )
            * cos_omega
            - 2
            * sqrt_A
            * alpha
        )
    )

    a0 = (
        A + 1
        + (
            A - 1
        )
        * cos_omega
        + 2
        * sqrt_A
        * alpha
    )

    a1 = (
        -2
        * (
            A - 1
            + (
                A + 1
            )
            * cos_omega
        )
    )

    a2 = (
        A + 1
        + (
            A - 1
        )
        * cos_omega
        - 2
        * sqrt_A
        * alpha
    )

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


# =========================================================
# High Shelf Filter
# =========================================================

def high_shelf_filter(
    audio_data,
    sample_rate,
    frequency=4000.0,
    gain_db=6.0,
    slope=1.0,
):
    """
    High Shelf Filter

    frequency:
        shelf 중심 주파수

    gain_db:
        고역 boost / cut

    slope:
        shelf 기울기
    """

    A = 10 ** (
        gain_db / 40
    )

    omega = (
        2
        * np.pi
        * frequency
        / sample_rate
    )

    sin_omega = np.sin(
        omega
    )

    cos_omega = np.cos(
        omega
    )

    alpha = (
        sin_omega
        / 2
        * np.sqrt(
            (
                A
                + 1 / A
            )
            * (
                1 / slope
                - 1
            )
            + 2
        )
    )

    sqrt_A = np.sqrt(
        A
    )

    b0 = (
        A
        * (
            A + 1
            + (
                A - 1
            )
            * cos_omega
            + 2
            * sqrt_A
            * alpha
        )
    )

    b1 = (
        -2
        * A
        * (
            A - 1
            + (
                A + 1
            )
            * cos_omega
        )
    )

    b2 = (
        A
        * (
            A + 1
            + (
                A - 1
            )
            * cos_omega
            - 2
            * sqrt_A
            * alpha
        )
    )

    a0 = (
        A + 1
        - (
            A - 1
        )
        * cos_omega
        + 2
        * sqrt_A
        * alpha
    )

    a1 = (
        2
        * (
            A - 1
            - (
                A + 1
            )
            * cos_omega
        )
    )

    a2 = (
        A + 1
        - (
            A - 1
        )
        * cos_omega
        - 2
        * sqrt_A
        * alpha
    )

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