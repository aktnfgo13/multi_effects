from pathlib import Path

import numpy as np
import soundfile as sf

from effects.amp_sim import (
    apply_amp_sim,
)
from effects.cabinet_ir import (
    load_ir,
    apply_cabinet_ir,
)


PROJECT_ROOT = (
    Path(__file__)
    .resolve()
    .parent
    .parent
)


INPUT_PATH = (
    PROJECT_ROOT
    / "audio"
    / "input"
    / "test.wav"
)


IR_PATH = (
    PROJECT_ROOT
    / "audio"
    / "ir"
    / "cabinet.wav"
)


RAW_OUTPUT_PATH = (
    PROJECT_ROOT
    / "audio"
    / "output"
    / "test_amp_preamp_raw.wav"
)


CAB_OUTPUT_PATH = (
    PROJECT_ROOT
    / "audio"
    / "output"
    / "test_amp_preamp_cab.wav"
)


# =========================================================
# Load Guitar DI
# =========================================================

audio_data, sample_rate = sf.read(
    INPUT_PATH
)


print(
    "=== AMP SIM v0.1 TEST ==="
)

print(
    f"Sample Rate : "
    f"{sample_rate} Hz"
)


# =========================================================
# Amp Preamp
# =========================================================

amp_output = apply_amp_sim(
    audio_data,
    sample_rate,

    gain_db=12.0,
    drive=1.5,
    bias=0.05,

    bass_db=2.0,
    mid_db=-1.0,
    treble_db=2.0,

    low_cut_hz=70.0,
    high_cut_hz=9000.0,

    master_db=-6.0,
)

print(
    f"Input Peak  : "
    f"{np.max(np.abs(audio_data)):.4f}"
)

print(
    f"Amp Peak    : "
    f"{np.max(np.abs(amp_output)):.4f}"
)


# =========================================================
# Raw Amp Output
# =========================================================

raw_peak = np.max(
    np.abs(amp_output)
)

raw_save = amp_output.copy()

if raw_peak > 0.95:

    raw_save = (
        raw_save
        / raw_peak
        * 0.95
    )


sf.write(
    RAW_OUTPUT_PATH,
    raw_save,
    sample_rate,
)


# =========================================================
# Cabinet IR
# =========================================================

ir_data = load_ir(
    IR_PATH,
    sample_rate,
)


cab_output = apply_cabinet_ir(
    amp_output,
    ir_data,
)


cab_peak = np.max(
    np.abs(cab_output)
)


cab_save = cab_output.copy()

if cab_peak > 0.95:

    cab_save = (
        cab_save
        / cab_peak
        * 0.95
    )


sf.write(
    CAB_OUTPUT_PATH,
    cab_save,
    sample_rate,
)


print()
print(
    f"Raw Amp Saved:"
    f"\n{RAW_OUTPUT_PATH}"
)

print()
print(
    f"Amp + Cab Saved:"
    f"\n{CAB_OUTPUT_PATH}"
)