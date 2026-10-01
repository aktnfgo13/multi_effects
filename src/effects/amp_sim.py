import numpy as np

from effects.filters import (
    high_pass_filter,
    low_pass_filter,
    low_shelf_filter,
    high_shelf_filter,
)

from effects.eq import (
    peaking_eq,
)


def db_to_linear(db):
    return 10 ** (db / 20)


# =========================================================
# Preamp Saturation
# =========================================================

def _preamp_saturation(
    signal,
    drive,
    bias,
):
    """
    비대칭 Preamp Saturation
    """

    biased_signal = (
        signal + bias
    )

    saturated = np.tanh(
        biased_signal * drive
    )

    # Bias 때문에 생기는 DC Offset 보정
    dc_reference = np.tanh(
        bias * drive
    )

    return (
        saturated - dc_reference
    )


# =========================================================
# Preamp Channel
# =========================================================

def _process_preamp_channel(
    signal,
    gain_db,
    drive,
    bias,
):
    gain_linear = db_to_linear(
        gain_db
    )

    stage_input = (
        signal * gain_linear
    )

    # Stage 1
    stage_1 = _preamp_saturation(
        stage_input,
        drive=drive,
        bias=bias,
    )

    # Stage 2
    stage_2 = _preamp_saturation(
        stage_1 * 1.3,
        drive=drive * 0.8,
        bias=-bias * 0.5,
    )

    return stage_2


# =========================================================
# Preamp
# =========================================================

def apply_amp_preamp(
    audio_data,
    sample_rate,
    gain_db=12.0,
    drive=1.5,
    bias=0.05,
    low_cut_hz=70.0,
):
    """
    Amp Preamp
    """

    filtered = high_pass_filter(
        audio_data,
        sample_rate,
        cutoff=low_cut_hz,
    )

    # Mono
    if filtered.ndim == 1:

        return _process_preamp_channel(
            filtered,
            gain_db,
            drive,
            bias,
        )

    # Stereo
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

    return processed


# =========================================================
# Tone Stack
# =========================================================

def apply_tone_stack(
    audio_data,
    sample_rate,
    bass_db=0.0,
    mid_db=0.0,
    treble_db=0.0,
):
    """
    Tone Stack v0.1

    Bass:
        Low Shelf

    Mid:
        Peaking EQ

    Treble:
        High Shelf
    """

    processed = low_shelf_filter(
        audio_data,
        sample_rate,
        frequency=180.0,
        gain_db=bass_db,
        slope=1.0,
    )

    processed = peaking_eq(
        processed,
        sample_rate,
        frequency=800.0,
        gain_db=mid_db,
        q=0.8,
    )

    processed = high_shelf_filter(
        processed,
        sample_rate,
        frequency=3500.0,
        gain_db=treble_db,
        slope=1.0,
    )

    return processed


# =========================================================
# Power Amp Saturation
# =========================================================

def _power_amp_saturation(
    signal,
    drive,
):
    """
    단순 Power Amp Saturation

    Preamp보다 완만한 Saturation을
    의도한 기본 모델
    """

    return np.tanh(
        signal * drive
    )


def apply_power_amp(
    audio_data,
    sample_rate,
    master_db=-3.0,
    power_drive=1.2,
    presence_db=0.0,
    output_db=-6.0,
):
    """
    Power Amp v0.1

    master_db:
        Power Amp에 들어가는 레벨.
        높일수록 Saturation도 증가.

    power_drive:
        Power Amp 비선형 강도.

    presence_db:
        출력단 고역 성향.

    output_db:
        최종 볼륨.
        Power Amp 왜곡량과 독립적으로 사용.
    """

    # =========================
    # Master
    # =========================

    master_gain = db_to_linear(
        master_db
    )

    driven = (
        audio_data * master_gain
    )

    # =========================
    # Saturation
    # =========================

    if driven.ndim == 1:

        processed = (
            _power_amp_saturation(
                driven,
                power_drive,
            )
        )

    else:

        processed = np.zeros_like(
            driven,
            dtype=np.float64,
        )

        for channel in range(
            driven.shape[1]
        ):

            processed[:, channel] = (
                _power_amp_saturation(
                    driven[:, channel],
                    power_drive,
                )
            )

    # =========================
    # Presence
    # =========================

    processed = high_shelf_filter(
        processed,
        sample_rate,
        frequency=4000.0,
        gain_db=presence_db,
        slope=1.0,
    )

    # =========================
    # Output Level
    # =========================

    output_gain = db_to_linear(
        output_db
    )

    processed *= output_gain

    return processed


# =========================================================
# Complete Amp Sim
# =========================================================

def apply_amp_sim(
    audio_data,
    sample_rate,

    gain_db=12.0,
    drive=1.5,
    bias=0.05,

    bass_db=0.0,
    mid_db=0.0,
    treble_db=0.0,

    master_db=-3.0,
    power_drive=1.2,
    presence_db=0.0,

    low_cut_hz=70.0,
    high_cut_hz=9000.0,

    output_db=-6.0,
):
    """
    Amp Sim v0.1

    Input
      ↓
    Low Cut
      ↓
    Preamp
      ↓
    Tone Stack
      ↓
    Master
      ↓
    Power Amp Saturation
      ↓
    Presence
      ↓
    High Cut
      ↓
    Output
    """

    # =========================
    # Preamp
    # =========================

    processed = apply_amp_preamp(
        audio_data,
        sample_rate,
        gain_db=gain_db,
        drive=drive,
        bias=bias,
        low_cut_hz=low_cut_hz,
    )

    # =========================
    # Tone Stack
    # =========================

    processed = apply_tone_stack(
        processed,
        sample_rate,
        bass_db=bass_db,
        mid_db=mid_db,
        treble_db=treble_db,
    )

    # =========================
    # Power Amp
    # =========================

    processed = apply_power_amp(
        processed,
        sample_rate,
        master_db=master_db,
        power_drive=power_drive,
        presence_db=presence_db,
        output_db=output_db,
    )

    # =========================
    # Final High Cut
    # =========================

    processed = low_pass_filter(
        processed,
        sample_rate,
        cutoff=high_cut_hz,
    )

    return processed