import numpy as np


def apply_delay(
    audio_data,
    sample_rate,
    delay_ms=400.0,
    feedback=0.35,
    mix=0.3,
):
    """
    Basic Digital Delay

    delay_ms:
        반복되는 소리의 간격 (ms)

    feedback:
        딜레이 출력이 다시 입력으로 들어가는 비율
        0.0 ~ 0.95 권장

    mix:
        Wet 신호 비율
        0.0 = Dry only
        1.0 = Wet only
    """

    feedback = np.clip(
        feedback,
        0.0,
        0.95,
    )

    mix = np.clip(
        mix,
        0.0,
        1.0,
    )

    delay_samples = int(
        sample_rate
        * delay_ms
        / 1000.0
    )

    if delay_samples < 1:
        return audio_data.copy()

    # =========================
    # Mono
    # =========================

    if audio_data.ndim == 1:

        wet = np.zeros_like(
            audio_data,
            dtype=np.float64,
        )

        for i in range(
            len(audio_data)
        ):

            wet[i] = audio_data[i]

            if i >= delay_samples:

                wet[i] += (
                    wet[
                        i - delay_samples
                    ]
                    * feedback
                )

        delayed_signal = (
            wet - audio_data
        )

        output = (
            audio_data * (1 - mix)
            + delayed_signal * mix
        )

        return output

    # =========================
    # Stereo / Multi Channel
    # =========================

    output = np.zeros_like(
        audio_data,
        dtype=np.float64,
    )

    for channel in range(
        audio_data.shape[1]
    ):

        signal = audio_data[
            :,
            channel
        ]

        wet = np.zeros_like(
            signal,
            dtype=np.float64,
        )

        for i in range(
            len(signal)
        ):

            wet[i] = signal[i]

            if i >= delay_samples:

                wet[i] += (
                    wet[
                        i - delay_samples
                    ]
                    * feedback
                )

        delayed_signal = (
            wet - signal
        )

        output[:, channel] = (
            signal * (1 - mix)
            + delayed_signal * mix
        )

    return output