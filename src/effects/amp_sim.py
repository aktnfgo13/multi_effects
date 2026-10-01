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
# Saturation
# =========================================================

def _preamp_saturation(
    signal,
    drive,
    bias,
):
    """
    비대칭 Saturation

    bias가 0이면 비교적 대칭적이고,
    bias가 생기면 비대칭 왜곡 성분이 추가된다.
    """

    biased_signal = (
        signal + bias
    )

    saturated = np.tanh(
        biased_signal * drive
    )

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

    # Preamp Stage 1
    stage_1 = _preamp_saturation(
        stage_input,
        drive=drive,
        bias=bias,
    )

    # Preamp Stage 2
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
    Amp Preamp Stage
    """

    filtered = high_pass_filter(
        audio_data,
        sample_rate,
        cutoff=low_cut_hz,
    )

    if filtered.ndim == 1:

        return _process_preamp_channel(
            filtered,
            gain_db,
            drive,
            bias,
        )

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
    Amp Tone Stack v0.1

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
# Complete Amp Sim v0.1
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
    low_cut_hz=70.0,
    high_cut_hz=9000.0,
    master_db=-6.0,
):
    """
    Amp Sim v0.1

    Input
      ↓
    Low Cut
      ↓
    Preamp Gain
      ↓
    Nonlinear Saturation
      ↓
    Bass / Mid / Treble
      ↓
    High Cut
      ↓
    Master
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
    # High Cut
    # =========================

    processed = low_pass_filter(
        processed,
        sample_rate,
        cutoff=high_cut_hz,
    )

    # =========================
    # Master Level
    # =========================

    master_gain = db_to_linear(
        master_db
    )

    processed *= master_gain

    return processed