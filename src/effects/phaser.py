import numpy as np

from scipy.signal import lfilter


def _allpass_coefficient(
    frequency,
    sample_rate,
):
    """
    First-order All-pass Filter coefficient.

    H(z) = (a + z^-1) / (1 + a*z^-1)
    """

    frequency = np.clip(
        frequency,
        20.0,
        sample_rate * 0.45,
    )

    warped = np.tan(
        np.pi
        * frequency
        / sample_rate
    )

    coefficient = (
        (1.0 - warped)
        / (1.0 + warped)
    )

    return coefficient


def _process_phaser_channel(
    signal,
    sample_rate,
    rate_hz,
    min_frequency_hz,
    max_frequency_hz,
    stages,
    mix,
    phase_offset=0.0,
    block_size=256,
):
    """
    Block-based Phaser processing.

    여러 개의 All-pass Filter를 직렬로 연결하고
    중심 주파수를 LFO로 움직인다.
    """

    signal = np.asarray(
        signal,
        dtype=np.float64,
    )

    sample_count = len(
        signal
    )

    wet = np.zeros_like(
        signal,
        dtype=np.float64,
    )

    # 각 All-pass stage의 내부 state
    states = [
        np.zeros(
            1,
            dtype=np.float64,
        )
        for _ in range(stages)
    ]

    # =========================================
    # Block Processing
    # =========================================

    for start in range(
        0,
        sample_count,
        block_size,
    ):
        end = min(
            start + block_size,
            sample_count,
        )

        block = signal[
            start:end
        ]

        # Block 중간 위치에서 LFO 계산
        block_center = (
            start + end
        ) * 0.5

        time_seconds = (
            block_center
            / sample_rate
        )

        lfo = np.sin(
            2.0
            * np.pi
            * rate_hz
            * time_seconds
            + phase_offset
        )

        # -1 ~ +1
        # ↓
        # 0 ~ 1
        normalized_lfo = (
            lfo + 1.0
        ) * 0.5

        # =====================================
        # Logarithmic Frequency Sweep
        # =====================================

        frequency = (
            min_frequency_hz
            * (
                max_frequency_hz
                / min_frequency_hz
            )
            ** normalized_lfo
        )

        coefficient = (
            _allpass_coefficient(
                frequency,
                sample_rate,
            )
        )

        processed = block.copy()

        # =====================================
        # Cascaded All-pass Filters
        # =====================================

        for stage in range(
            stages
        ):
            b = np.array(
                [
                    coefficient,
                    1.0,
                ]
            )

            a = np.array(
                [
                    1.0,
                    coefficient,
                ]
            )

            processed, states[stage] = (
                lfilter(
                    b,
                    a,
                    processed,
                    zi=states[stage],
                )
            )

        wet[
            start:end
        ] = processed

    # =========================================
    # Dry / Wet
    # =========================================

    output = (
        signal * (1.0 - mix)
        + wet * mix
    )

    return output


def apply_phaser(
    audio_data,
    sample_rate,
    rate_hz=0.5,
    min_frequency_hz=300.0,
    max_frequency_hz=1600.0,
    stages=4,
    mix=0.5,
    stereo_phase_deg=90.0,
    block_size=256,
):
    """
    Phaser DSP v0.1

    rate_hz:
        LFO 속도

    min_frequency_hz:
        Phaser sweep 최저 주파수

    max_frequency_hz:
        Phaser sweep 최고 주파수

    stages:
        All-pass Filter 개수

    mix:
        Dry / Wet 비율

    stereo_phase_deg:
        Stereo 좌우 LFO 위상 차이

    block_size:
        Python prototype에서
        filter coefficient를 갱신하는 간격
    """

    audio_data = np.asarray(
        audio_data,
        dtype=np.float64,
    )

    rate_hz = max(
        0.0,
        float(rate_hz),
    )

    min_frequency_hz = max(
        20.0,
        float(min_frequency_hz),
    )

    max_frequency_hz = max(
        min_frequency_hz,
        float(max_frequency_hz),
    )

    max_frequency_hz = min(
        max_frequency_hz,
        sample_rate * 0.45,
    )

    stages = int(
        np.clip(
            stages,
            1,
            12,
        )
    )

    mix = float(
        np.clip(
            mix,
            0.0,
            1.0,
        )
    )

    block_size = max(
        1,
        int(block_size),
    )

    # =========================================
    # Mono
    # =========================================

    if audio_data.ndim == 1:

        return _process_phaser_channel(
            audio_data,
            sample_rate,
            rate_hz,
            min_frequency_hz,
            max_frequency_hz,
            stages,
            mix,
            phase_offset=0.0,
            block_size=block_size,
        )

    # =========================================
    # Stereo
    # =========================================

    output = np.zeros_like(
        audio_data,
        dtype=np.float64,
    )

    stereo_phase_rad = np.deg2rad(
        stereo_phase_deg
    )

    for channel in range(
        audio_data.shape[1]
    ):

        phase_offset = (
            stereo_phase_rad
            * channel
        )

        output[:, channel] = (
            _process_phaser_channel(
                audio_data[:, channel],
                sample_rate,
                rate_hz,
                min_frequency_hz,
                max_frequency_hz,
                stages,
                mix,
                phase_offset,
                block_size,
            )
        )

    return output