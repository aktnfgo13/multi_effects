import numpy as np


def _process_flanger_channel(
    signal,
    sample_rate,
    rate_hz,
    depth_ms,
    base_delay_ms,
    mix,
    phase_offset=0.0,
):
    """
    Feed-forward Flanger v0.1

    Chorus보다 매우 짧은 modulated delay를 사용해
    Comb Filtering을 만든다.
    """

    signal = np.asarray(
        signal,
        dtype=np.float64,
    )

    sample_count = len(signal)

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
    # Delay Time Modulation
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
        0.1,
    )

    # =========================================
    # Fractional Read Position
    # =========================================

    read_positions = (
        sample_positions
        - delay_samples
    )

    delayed_signal = np.interp(
        read_positions,
        sample_positions,
        signal,
        left=0.0,
        right=0.0,
    )

    # =========================================
    # Dry + Wet
    # =========================================

    output = (
        signal * (1.0 - mix)
        + delayed_signal * mix
    )

    return output


def apply_flanger(
    audio_data,
    sample_rate,
    rate_hz=0.4,
    depth_ms=1.5,
    base_delay_ms=2.0,
    mix=0.5,
    stereo_phase_deg=90.0,
):
    """
    Basic Flanger DSP

    rate_hz:
        LFO 속도

    depth_ms:
        Delay 변화 폭

    base_delay_ms:
        기본 Delay Time

    mix:
        Dry / Wet 비율

    stereo_phase_deg:
        Stereo LFO 위상 차이
    """

    audio_data = np.asarray(
        audio_data,
        dtype=np.float64,
    )

    rate_hz = max(
        0.0,
        float(rate_hz),
    )

    depth_ms = max(
        0.0,
        float(depth_ms),
    )

    base_delay_ms = max(
        0.2,
        float(base_delay_ms),
    )

    # Delay가 음수가 되지 않게 제한
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

        return _process_flanger_channel(
            audio_data,
            sample_rate,
            rate_hz,
            depth_ms,
            base_delay_ms,
            mix,
            phase_offset=0.0,
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
            _process_flanger_channel(
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