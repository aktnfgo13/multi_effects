from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from analysis.reference_benchmark import (
    align_by_latency,
    align_length,
    calculate_correlation,
    calculate_spectrum,
    estimate_latency_samples,
    load_audio,
    match_rms,
)


# =========================================================
# Prepare Audio
# =========================================================

def prepare_comparison_audio(
    reference_path,
    target_path,
    minimum_improvement=0.001,
):
    """
    Benchmark v0.3과 동일한 방식으로
    두 오디오를 정렬하고 RMS를 맞춘다.
    """

    reference, reference_sr = load_audio(
        reference_path
    )

    target, target_sr = load_audio(
        target_path
    )

    if reference_sr != target_sr:
        raise ValueError(
            "Sample rates must match. "
            f"Reference={reference_sr}, "
            f"Target={target_sr}"
        )

    # -----------------------------------------------------
    # Original Alignment
    # -----------------------------------------------------

    pre_reference, pre_target = align_length(
        reference,
        target,
    )

    pre_correlation = calculate_correlation(
        pre_reference,
        pre_target,
    )

    # -----------------------------------------------------
    # Candidate Latency
    # -----------------------------------------------------

    candidate_latency = estimate_latency_samples(
        reference,
        target,
        reference_sr,
    )

    candidate_reference, candidate_target = (
        align_by_latency(
            reference,
            target,
            candidate_latency,
        )
    )

    candidate_correlation = (
        calculate_correlation(
            candidate_reference,
            candidate_target,
        )
    )

    # -----------------------------------------------------
    # Accept / Reject Alignment
    # -----------------------------------------------------

    if (
        candidate_correlation
        >
        pre_correlation
        + minimum_improvement
    ):
        reference = candidate_reference
        target = candidate_target

        latency_samples = candidate_latency
        alignment_applied = True

    else:
        reference = pre_reference
        target = pre_target

        latency_samples = 0
        alignment_applied = False

    # -----------------------------------------------------
    # RMS Level Match
    # -----------------------------------------------------

    target_matched, rms_gain = match_rms(
        reference,
        target,
    )

    return {
        "reference": reference,
        "target": target_matched,
        "sample_rate": reference_sr,
        "latency_samples": latency_samples,
        "alignment_applied": alignment_applied,
        "rms_match_gain": rms_gain,
    }


# =========================================================
# Spectrum Resampling
# =========================================================

def create_log_frequency_spectrum(
    audio,
    sample_rate,
    min_frequency=20.0,
    max_frequency=20000.0,
    points=2000,
):
    """
    FFT 결과를 Log Frequency Grid로 변환한다.

    수백만 개 FFT bin을 그대로 Plot하지 않고
    약 2000개 지점으로 줄여서 시각화한다.
    """

    frequencies, magnitude_db = (
        calculate_spectrum(
            audio,
            sample_rate,
        )
    )

    max_frequency = min(
        max_frequency,
        sample_rate / 2.0,
    )

    mask = (
        (frequencies >= min_frequency)
        & (frequencies <= max_frequency)
    )

    frequencies = frequencies[mask]
    magnitude_db = magnitude_db[mask]

    if len(frequencies) < 2:
        raise ValueError(
            "Not enough spectrum data."
        )

    log_frequencies = np.geomspace(
        min_frequency,
        max_frequency,
        points,
    )

    interpolated_db = np.interp(
        log_frequencies,
        frequencies,
        magnitude_db,
    )

    return (
        log_frequencies,
        interpolated_db,
    )


# =========================================================
# Spectrum Plot
# =========================================================

def save_spectrum_plot(
    reference,
    target,
    sample_rate,
    output_path,
):
    """
    Reference와 Target Spectrum을
    동일한 그래프에 표시한다.
    """

    frequencies, reference_db = (
        create_log_frequency_spectrum(
            reference,
            sample_rate,
        )
    )

    _, target_db = (
        create_log_frequency_spectrum(
            target,
            sample_rate,
        )
    )

    plt.figure(
        figsize=(12, 6)
    )

    plt.plot(
        frequencies,
        reference_db,
        label="Reference",
    )

    plt.plot(
        frequencies,
        target_db,
        label="Target",
    )

    plt.xscale(
        "log"
    )

    plt.xlim(
        20,
        min(
            20000,
            sample_rate / 2,
        ),
    )

    plt.xlabel(
        "Frequency (Hz)"
    )

    plt.ylabel(
        "Magnitude (dB)"
    )

    plt.title(
        "Reference vs Target Spectrum"
    )

    plt.grid(
        True,
        which="both",
        alpha=0.3,
    )

    plt.legend()

    plt.tight_layout()

    plt.savefig(
        output_path,
        dpi=150,
    )

    plt.close()


# =========================================================
# Difference Plot
# =========================================================

def save_difference_plot(
    reference,
    target,
    sample_rate,
    output_path,
):
    """
    Frequency Difference를 표시한다.

    Difference =
        Reference - Target

    Positive dB:
        Target에 해당 대역이 부족

    Negative dB:
        Target에 해당 대역이 더 많음
    """

    frequencies, reference_db = (
        create_log_frequency_spectrum(
            reference,
            sample_rate,
        )
    )

    _, target_db = (
        create_log_frequency_spectrum(
            target,
            sample_rate,
        )
    )

    difference_db = (
        reference_db
        - target_db
    )

    plt.figure(
        figsize=(12, 6)
    )

    plt.plot(
        frequencies,
        difference_db,
    )

    plt.axhline(
        0.0,
        linewidth=1.0,
    )

    plt.xscale(
        "log"
    )

    plt.xlim(
        20,
        min(
            20000,
            sample_rate / 2,
        ),
    )

    plt.xlabel(
        "Frequency (Hz)"
    )

    plt.ylabel(
        "Reference - Target (dB)"
    )

    plt.title(
        "Spectral Difference"
    )

    plt.grid(
        True,
        which="both",
        alpha=0.3,
    )

    plt.tight_layout()

    plt.savefig(
        output_path,
        dpi=150,
    )

    plt.close()


# =========================================================
# Main Visualization
# =========================================================

def create_benchmark_plots(
    reference_path,
    target_path,
    output_directory,
):
    """
    Benchmark용 Spectrum / Difference
    그래프를 생성한다.
    """

    prepared = prepare_comparison_audio(
        reference_path,
        target_path,
    )

    reference = prepared[
        "reference"
    ]

    target = prepared[
        "target"
    ]

    sample_rate = prepared[
        "sample_rate"
    ]

    output_directory = Path(
        output_directory
    )

    output_directory.mkdir(
        parents=True,
        exist_ok=True,
    )

    spectrum_path = (
        output_directory
        / "benchmark_spectrum.png"
    )

    difference_path = (
        output_directory
        / "benchmark_difference.png"
    )

    save_spectrum_plot(
        reference,
        target,
        sample_rate,
        spectrum_path,
    )

    save_difference_plot(
        reference,
        target,
        sample_rate,
        difference_path,
    )

    return {
        "spectrum_path":
            spectrum_path,

        "difference_path":
            difference_path,

        "latency_samples":
            prepared[
                "latency_samples"
            ],

        "alignment_applied":
            prepared[
                "alignment_applied"
            ],

        "rms_match_gain":
            prepared[
                "rms_match_gain"
            ],
    }