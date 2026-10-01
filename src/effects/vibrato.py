import numpy as np


def _process_vibrato_channel(
    signal,
    sample_rate,
    rate_hz,
    depth_ms,
    base_delay_ms,
    phase_offset=0.0,
):
    signal = np.asarray(
        signal,
        dtype=np.float64,
    )

    sample_count = len(
        signal
    )

    positions = np.arange(
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
        * positions
        / sample_rate
        + phase_offset
    )

    # =========================================
    # Delay modulation
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

    read_positions = (
        positions
        - delay_samples
    )

    # Fractional delay interpolation
    output = np.interp(
        read_positions,
        positions,
        signal,
        left=0.0,
        right=0.0,
    )

    return output


def apply_vibrato(
    audio_data,
    sample_rate,
    rate_hz=5.0,
    depth_ms=2.0,
    base_delay_ms=5.0,
    stereo_phase_deg=0.0,
):
    """
    Vibrato DSP

    rate_hz:
        Pitch modulation 속도

    depth_ms:
        Pitch 흔들림 깊이

    base_delay_ms:
        중심 Delay Time
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
    depth_ms = min(
        depth_ms,
        base_delay_ms - 0.1,
    )

    # =========================================
    # Mono
    # =========================================

    if audio_data.ndim == 1:

        return _process_vibrato_channel(
            audio_data,
            sample_rate,
            rate_hz,
            depth_ms,
            base_delay_ms,
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
            _process_vibrato_channel(
                audio_data[:, channel],
                sample_rate,
                rate_hz,
                depth_ms,
                base_delay_ms,
                phase_offset,
            )
        )

    return output