import numpy as np


def _comb_filter(
    signal,
    delay_samples,
    feedback,
    damping,
):
    """
    Feedback Comb Filter

    delay_samples:
        반사음의 지연 길이

    feedback:
        얼마나 오래 반복될지 결정

    damping:
        반복될수록 고역을 얼마나 줄일지 결정
    """

    buffer = np.zeros(
        delay_samples,
        dtype=np.float64,
    )

    output = np.zeros_like(
        signal,
        dtype=np.float64,
    )

    buffer_index = 0
    filter_store = 0.0

    for i, sample in enumerate(signal):

        delayed = buffer[
            buffer_index
        ]

        # 간단한 damping low-pass
        filter_store = (
            delayed * (1.0 - damping)
            + filter_store * damping
        )

        output[i] = delayed

        buffer[
            buffer_index
        ] = (
            sample
            + filter_store * feedback
        )

        buffer_index += 1

        if buffer_index >= delay_samples:
            buffer_index = 0

    return output


def _allpass_filter(
    signal,
    delay_samples,
    feedback=0.5,
):
    """
    All-pass Filter

    주파수 크기를 크게 바꾸기보다는
    반사음의 밀도와 시간 구조를 변화시킨다.
    """

    buffer = np.zeros(
        delay_samples,
        dtype=np.float64,
    )

    output = np.zeros_like(
        signal,
        dtype=np.float64,
    )

    buffer_index = 0

    for i, sample in enumerate(signal):

        delayed = buffer[
            buffer_index
        ]

        output[i] = (
            -sample
            + delayed
        )

        buffer[
            buffer_index
        ] = (
            sample
            + delayed * feedback
        )

        buffer_index += 1

        if buffer_index >= delay_samples:
            buffer_index = 0

    return output


def _process_reverb_channel(
    signal,
    sample_rate,
    room_size,
    damping,
    channel_offset=1.0,
):
    """
    하나의 채널에 Reverb 처리
    """

    # 서로 다른 길이의 초기 반사
    comb_delays_ms = [
        29.7,
        37.1,
        41.1,
        43.7,
    ]

    # Diffusion용 all-pass
    allpass_delays_ms = [
        5.0,
        1.7,
    ]

    # room_size를 feedback 값으로 변환
    feedback = (
        0.2
        + room_size * 0.72
    )

    feedback = np.clip(
        feedback,
        0.0,
        0.95,
    )

    wet = np.zeros_like(
        signal,
        dtype=np.float64,
    )

    # =========================
    # Parallel Comb Filters
    # =========================

    for delay_ms in comb_delays_ms:

        delay_samples = max(
            1,
            int(
                sample_rate
                * delay_ms
                * channel_offset
                / 1000.0
            ),
        )

        wet += _comb_filter(
            signal,
            delay_samples,
            feedback,
            damping,
        )

    # Comb 수만큼 레벨 보정
    wet /= len(
        comb_delays_ms
    )

    # =========================
    # Serial All-pass Filters
    # =========================

    for delay_ms in allpass_delays_ms:

        delay_samples = max(
            1,
            int(
                sample_rate
                * delay_ms
                * channel_offset
                / 1000.0
            ),
        )

        wet = _allpass_filter(
            wet,
            delay_samples,
            feedback=0.5,
        )

    return wet


def apply_reverb(
    audio_data,
    sample_rate,
    room_size=0.6,
    damping=0.4,
    mix=0.3,
    tail_seconds=2.0,
):
    """
    Basic Algorithmic Reverb

    room_size:
        0.0 ~ 1.0
        높을수록 decay가 길어짐

    damping:
        0.0 ~ 0.99
        높을수록 고역이 빠르게 감소

    mix:
        0.0 = Dry
        1.0 = Wet

    tail_seconds:
        입력이 끝난 뒤 Reverb tail을
        몇 초 더 계산할지 결정
    """

    room_size = np.clip(
        room_size,
        0.0,
        1.0,
    )

    damping = np.clip(
        damping,
        0.0,
        0.99,
    )

    mix = np.clip(
        mix,
        0.0,
        1.0,
    )

    tail_samples = int(
        sample_rate
        * tail_seconds
    )

    # =========================
    # Mono
    # =========================

    if audio_data.ndim == 1:

        dry = np.concatenate(
            [
                audio_data,
                np.zeros(
                    tail_samples
                ),
            ]
        )

        wet = _process_reverb_channel(
            dry,
            sample_rate,
            room_size,
            damping,
        )

        output = (
            dry * (1.0 - mix)
            + wet * mix
        )

        return output

    # =========================
    # Stereo / Multi-channel
    # =========================

    channel_count = (
        audio_data.shape[1]
    )

    dry = np.vstack(
        [
            audio_data,
            np.zeros(
                (
                    tail_samples,
                    channel_count,
                )
            ),
        ]
    )

    output = np.zeros_like(
        dry,
        dtype=np.float64,
    )

    for channel in range(
        channel_count
    ):

        # 좌우 delay 시간을 미세하게 다르게 해서
        # stereo 공간감을 조금 넓힘
        channel_offset = (
            1.0
            + channel * 0.013
        )

        wet = _process_reverb_channel(
            dry[:, channel],
            sample_rate,
            room_size,
            damping,
            channel_offset,
        )

        output[:, channel] = (
            dry[:, channel]
            * (1.0 - mix)
            + wet * mix
        )

    return output