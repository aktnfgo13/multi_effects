import numpy as np


def calculate_envelope(
    audio_data,
    sample_rate,
    window_ms=10,
):
    # Stereo -> Mono
    if audio_data.ndim > 1:
        audio_data = np.mean(audio_data, axis=1)

    # 절댓값 = 순간 진폭
    rectified = np.abs(audio_data)

    window_size = max(
        1,
        int(sample_rate * window_ms / 1000),
    )

    kernel = np.ones(window_size) / window_size

    envelope = np.convolve(
        rectified,
        kernel,
        mode="same",
    )

    return envelope


def calculate_transient_strength(
    envelope,
):
    # Envelope 변화량
    difference = np.diff(
        envelope,
        prepend=envelope[0],
    )

    # 증가 방향만 사용
    transient = np.maximum(
        difference,
        0.0,
    )

    return transient


def calculate_attack_time(
    envelope,
    sample_rate,
):
    peak_index = np.argmax(envelope)
    peak_value = envelope[peak_index]

    if peak_value <= 0:
        return 0.0

    low_threshold = peak_value * 0.1
    high_threshold = peak_value * 0.9

    before_peak = envelope[:peak_index + 1]

    low_candidates = np.where(
        before_peak >= low_threshold
    )[0]

    high_candidates = np.where(
        before_peak >= high_threshold
    )[0]

    if (
        len(low_candidates) == 0
        or len(high_candidates) == 0
    ):
        return 0.0

    low_index = low_candidates[0]
    high_index = high_candidates[0]

    attack_samples = max(
        0,
        high_index - low_index,
    )

    return (
        attack_samples
        / sample_rate
        * 1000
    )