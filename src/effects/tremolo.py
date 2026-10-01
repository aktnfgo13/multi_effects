import numpy as np


def apply_tremolo(
    audio_data,
    sample_rate,
    rate_hz=4.0,
    depth=0.5,
    stereo_phase_deg=0.0,
):
    """
    Basic Tremolo DSP

    rate_hz:
        볼륨이 흔들리는 속도

    depth:
        0.0 = 효과 없음
        1.0 = 최대 깊이

    stereo_phase_deg:
        Stereo 좌/우 LFO 위상 차이
    """

    audio_data = np.asarray(
        audio_data,
        dtype=np.float64,
    )

    rate_hz = max(
        0.0,
        float(rate_hz),
    )

    depth = float(
        np.clip(
            depth,
            0.0,
            1.0,
        )
    )

    sample_count = (
        audio_data.shape[0]
    )

    positions = np.arange(
        sample_count,
        dtype=np.float64,
    )

    # =========================================
    # Mono
    # =========================================

    if audio_data.ndim == 1:

        lfo = np.sin(
            2.0
            * np.pi
            * rate_hz
            * positions
            / sample_rate
        )

        # LFO -1~+1을 0~1로 변환
        normalized_lfo = (
            lfo + 1.0
        ) * 0.5

        # depth=0 -> gain=1
        # depth=1 -> gain=0~1
        gain = (
            1.0
            - depth * normalized_lfo
        )

        return (
            audio_data * gain
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

        lfo = np.sin(
            2.0
            * np.pi
            * rate_hz
            * positions
            / sample_rate
            + phase_offset
        )

        normalized_lfo = (
            lfo + 1.0
        ) * 0.5

        gain = (
            1.0
            - depth * normalized_lfo
        )

        output[:, channel] = (
            audio_data[:, channel]
            * gain
        )

    return output