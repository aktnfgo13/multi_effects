import numpy as np


def _process_chorus_channel(
    signal,
    sample_rate,
    rate_hz,
    depth_ms,
    base_delay_ms,
    mix,
    phase_offset=0.0,
):
    """
    Vectorized Chorus Processing

    Python 프로토타입에서는
    전체 신호의 LFO / Delay Position을
    NumPy로 한 번에 계산한다.

    나중에 C++ realtime DSP에서는
    Circular Buffer 방식으로 변경한다.
    """

    signal = np.asarray(
        signal,
        dtype=np.float64,
    )

    sample_count = len(
        signal
    )

    # =========================================
    # Sample Position
    # =========================================

    sample_positions = np.arange(
        sample_count,
        dtype=np.float64,
    )

    # =========================================
    # LFO
    # =========================================

    lfo = np.sin(
        2.0
        * np.pi
        * rate_hz
        * sample_positions
        / sample_rate
        + phase_offset
    )

    # =========================================
    # Modulated Delay
    # =========================================

    delay_ms = (
        base_delay_ms
        + depth_ms * lfo
    )

    delay_samples = (
        delay_ms
        * sample_rate
        / 1000.0
    )

    delay_samples = np.maximum(
        delay_samples,
        1.0,
    )

    # =========================================
    # Fractional Read Position
    # =========================================

    read_positions = (
        sample_positions
        - delay_samples
    )

    # =========================================
    # Linear Interpolation
    # =========================================
    #
    # 예:
    #
    # read_position = 100.4
    #
    # signal[100]과 signal[101] 사이를
    # 선형 보간해서 값을 계산한다.
    #
    # 음원 시작 전 위치는 0으로 처리한다.
    # =========================================

    delayed_signal = np.interp(
        read_positions,
        sample_positions,
        signal,
        left=0.0,
        right=0.0,
    )

    # =========================================
    # Dry / Wet
    # =========================================

    output = (
        signal * (1.0 - mix)
        + delayed_signal * mix
    )

    return output


def apply_chorus(
    audio_data,
    sample_rate,
    rate_hz=0.8,
    depth_ms=5.0,
    base_delay_ms=15.0,
    mix=0.35,
    stereo_phase_deg=90.0,
):
    """
    Basic Chorus DSP

    rate_hz:
        LFO 속도

    depth_ms:
        Delay Time modulation 폭

    base_delay_ms:
        기본 Delay Time

    mix:
        Wet 비율

        0.0 = Dry
        1.0 = Wet

    stereo_phase_deg:
        Stereo 좌/우 LFO 위상 차이
    """

    audio_data = np.asarray(
        audio_data,
        dtype=np.float64,
    )

    # =========================================
    # Parameter Protection
    # =========================================

    rate_hz = max(
        0.0,
        float(rate_hz),
    )

    depth_ms = max(
        0.0,
        float(depth_ms),
    )

    base_delay_ms = max(
        1.0,
        float(base_delay_ms),
    )

    # Delay 시간이 0 이하가 되지 않도록 제한
    max_depth = (
        base_delay_ms
        - 0.1
    )

    depth_ms = min(
        depth_ms,
        max_depth,
    )

    mix = float(
        np.clip(
            mix,
            0.0,
            1.0,
        )
    )

    # =========================================
    # Mono
    # =========================================

    if audio_data.ndim == 1:

        return _process_chorus_channel(
            audio_data,
            sample_rate,
            rate_hz,
            depth_ms,
            base_delay_ms,
            mix,
            phase_offset=0.0,
        )

    # =========================================
    # Stereo / Multi Channel
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
            _process_chorus_channel(
                audio_data[:, channel],
                sample_rate,
                rate_hz,
                depth_ms,
                base_delay_ms,
                mix,
                phase_offset,
            )
        )

    return output