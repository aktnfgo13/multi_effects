import numpy as np

from effects.filters import (
    high_pass_filter,
    low_pass_filter,
)


def db_to_linear(db):
    """
    dB 값을 Linear Gain으로 변환
    """
    return 10 ** (db / 20)


def _preamp_saturation(
    signal,
    drive,
    bias,
):
    """
    간단한 Preamp Nonlinear Saturation

    drive:
        비선형 왜곡 강도

    bias:
        비대칭 왜곡 정도

    bias를 사용하면 완전히 대칭적인 tanh보다
    짝수차 고조파가 추가될 수 있다.
    """

    biased_signal = (
        signal + bias
    )

    saturated = np.tanh(
        biased_signal * drive
    )

    # DC offset 보정
    dc_reference = np.tanh(
        bias * drive
    )

    saturated = (
        saturated - dc_reference
    )

    return saturated


def _process_preamp_channel(
    signal,
    gain_db,
    drive,
    bias,
):
    """
    하나의 채널에 Preamp 처리
    """

    gain_linear = db_to_linear(
        gain_db
    )

    # =========================
    # Input Gain
    # =========================

    stage_input = (
        signal * gain_linear
    )

    # =========================
    # Preamp Stage 1
    # =========================

    stage_1 = _preamp_saturation(
        stage_input,
        drive=drive,
        bias=bias,
    )

    # =========================
    # Preamp Stage 2
    #
    # 실제 앰프처럼 여러 증폭 단계를
    # 흉내내기 위한 간단한 구조
    # =========================

    stage_2 = _preamp_saturation(
        stage_1 * 1.3,
        drive=drive * 0.8,
        bias=-bias * 0.5,
    )

    return stage_2


def apply_amp_preamp(
    audio_data,
    sample_rate,
    gain_db=12.0,
    drive=1.5,
    bias=0.05,
    low_cut_hz=70.0,
    high_cut_hz=9000.0,
    output_db=-6.0,
):
    """
    Amp Sim v0.1 - Preamp

    처리 순서:

    Input
      ↓
    HPF
      ↓
    Input Gain
      ↓
    Saturation Stage 1
      ↓
    Saturation Stage 2
      ↓
    LPF
      ↓
    Output Level


    gain_db:
        Preamp 입력 Gain

    drive:
        Saturation 강도

    bias:
        비대칭 Saturation 정도

    low_cut_hz:
        불필요한 초저역 제거

    high_cut_hz:
        지나친 고역 제거

    output_db:
        최종 출력 레벨
    """

    # =========================
    # Pre EQ - Low Cut
    # =========================

    filtered = high_pass_filter(
        audio_data,
        sample_rate,
        cutoff=low_cut_hz,
    )

    # =========================
    # Preamp Saturation
    # =========================

    if filtered.ndim == 1:

        processed = (
            _process_preamp_channel(
                filtered,
                gain_db,
                drive,
                bias,
            )
        )

    else:

        processed = np.zeros_like(
            filtered,
            dtype=np.float64,
        )

        for channel in range(
            filtered.shape[1]
        ):

            processed[:, channel] = (
                _process_preamp_channel(
                    filtered[:, channel],
                    gain_db,
                    drive,
                    bias,
                )
            )

    # =========================
    # Post Preamp High Cut
    # =========================

    processed = low_pass_filter(
        processed,
        sample_rate,
        cutoff=high_cut_hz,
    )

    # =========================
    # Output Level
    # =========================

    output_gain = db_to_linear(
        output_db
    )

    processed *= output_gain

    return processed