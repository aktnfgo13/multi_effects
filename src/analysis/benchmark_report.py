from pathlib import Path


BAND_LABELS = {
    "sub": "Sub",
    "bass": "Bass",
    "low_mid": "Low Mid",
    "mid": "Mid",
    "high_mid": "High Mid",
    "presence": "Presence",
    "high": "High",
}


def interpret_difference(
    difference_db,
    tolerance_db=0.5,
):
    """
    Difference = Reference - Target

    Positive:
        Target가 Reference보다 부족

    Negative:
        Target가 Reference보다 강함
    """

    if abs(difference_db) <= tolerance_db:
        return "Similar"

    if difference_db > 0:
        return (
            f"Target lower by "
            f"{abs(difference_db):.2f} dB"
        )

    return (
        f"Target higher by "
        f"{abs(difference_db):.2f} dB"
    )


def generate_benchmark_report(
    results,
    output_path,
    reference_name="Reference",
    target_name="Target",
):
    """
    Benchmark 결과를 Markdown Report로 저장한다.
    """

    output_path = Path(
        output_path
    )

    output_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    lines = []

    # =====================================================
    # Header
    # =====================================================

    lines.append(
        "# Reference Benchmark Report"
    )

    lines.append("")

    lines.append(
        f"**Reference:** {reference_name}"
    )

    lines.append(
        f"**Target:** {target_name}"
    )

    lines.append("")

    lines.append("---")
    lines.append("")

    # =====================================================
    # Audio Information
    # =====================================================

    lines.append(
        "## Audio Information"
    )

    lines.append("")

    lines.append(
        f"- Sample Rate: "
        f"{results['sample_rate']} Hz"
    )

    lines.append(
        f"- Samples: "
        f"{results['samples']}"
    )

    lines.append(
        f"- Duration: "
        f"{results['duration_seconds']:.2f} sec"
    )

    lines.append("")

    # =====================================================
    # Latency
    # =====================================================

    lines.append(
        "## Latency / Alignment"
    )

    lines.append("")

    lines.append(
        f"- Candidate Latency: "
        f"{results['candidate_latency_samples']} samples "
        f"({results['candidate_latency_ms']:.4f} ms)"
    )

    lines.append(
        f"- Alignment Applied: "
        f"{results['alignment_applied']}"
    )

    lines.append(
        f"- Accepted Latency: "
        f"{results['latency_samples']} samples "
        f"({results['latency_ms']:.4f} ms)"
    )

    lines.append(
        f"- Correlation Before: "
        f"{results['pre_alignment_correlation']:.6f}"
    )

    lines.append(
        f"- Correlation After: "
        f"{results['post_alignment_correlation']:.6f}"
    )

    lines.append("")

    # =====================================================
    # Level / Dynamics
    # =====================================================

    lines.append(
        "## Level / Dynamics"
    )

    lines.append("")

    lines.append(
        "| Metric | Reference | Target |"
    )

    lines.append(
        "|---|---:|---:|"
    )

    lines.append(
        f"| Peak | "
        f"{results['reference_peak']:.6f} | "
        f"{results['target_peak']:.6f} |"
    )

    lines.append(
        f"| RMS | "
        f"{results['reference_rms']:.6f} | "
        f"{results['target_rms']:.6f} |"
    )

    lines.append(
        f"| Crest Factor | "
        f"{results['reference_crest_db']:.2f} dB | "
        f"{results['target_crest_db']:.2f} dB |"
    )

    lines.append("")

    lines.append(
        f"RMS Match Gain: "
        f"`{results['rms_match_gain']:.6f}`"
    )

    lines.append("")

    # =====================================================
    # Time Domain
    # =====================================================

    lines.append(
        "## Time Domain Comparison"
    )

    lines.append("")

    lines.append(
        f"- MAE: "
        f"{results['mae']:.6f}"
    )

    lines.append(
        f"- RMSE: "
        f"{results['rmse']:.6f}"
    )

    lines.append(
        f"- Correlation: "
        f"{results['correlation']:.6f}"
    )

    lines.append("")

    # =====================================================
    # Frequency Domain
    # =====================================================

    lines.append(
        "## Frequency Domain Comparison"
    )

    lines.append("")

    lines.append(
        f"- Spectral MAE: "
        f"{results['spectral_mae_db']:.3f} dB"
    )

    lines.append(
        f"- Spectral RMSE: "
        f"{results['spectral_rmse_db']:.3f} dB"
    )

    lines.append(
        f"- Spectral Correlation: "
        f"{results['spectral_correlation']:.6f}"
    )

    lines.append("")

    # =====================================================
    # Band Analysis
    # =====================================================

    lines.append(
        "## Frequency Band Analysis"
    )

    lines.append("")

    lines.append(
        "> Difference = Reference - Target"
    )

    lines.append("")

    lines.append(
        "| Band | Range | Difference | MAE | Interpretation |"
    )

    lines.append(
        "|---|---:|---:|---:|---|"
    )

    bands = results[
        "frequency_bands"
    ]

    for band_name, band in bands.items():

        label = BAND_LABELS.get(
            band_name,
            band_name,
        )

        interpretation = (
            interpret_difference(
                band[
                    "difference_db"
                ]
            )
        )

        lines.append(
            f"| {label} "
            f"| {band['min_hz']:.0f}"
            f"–{band['max_hz']:.0f} Hz "
            f"| {band['difference_db']:+.3f} dB "
            f"| {band['mae_db']:.3f} dB "
            f"| {interpretation} |"
        )

    lines.append("")

    # =====================================================
    # Visualization
    # =====================================================

    lines.append(
        "## Visualization"
    )

    lines.append("")

    lines.append(
        "### Spectrum"
    )

    lines.append("")

    lines.append(
        "![Benchmark Spectrum]"
        "(benchmark_spectrum.png)"
    )

    lines.append("")

    lines.append(
        "### Spectral Difference"
    )

    lines.append("")

    lines.append(
        "![Benchmark Difference]"
        "(benchmark_difference.png)"
    )

    lines.append("")

    # =====================================================
    # Notes
    # =====================================================

    lines.append(
        "## Notes"
    )

    lines.append("")

    lines.append(
        "- Positive difference means the Target is lower "
        "than the Reference in that frequency band."
    )

    lines.append(
        "- Negative difference means the Target is higher "
        "than the Reference in that frequency band."
    )

    lines.append(
        "- RMS level matching is applied before comparison."
    )

    lines.append(
        "- Spectral measurements use Welch averaged spectra."
    )

    lines.append(
        "- Correlation and error values are comparison "
        "metrics, not direct audio-quality scores."
    )

    lines.append("")

    report_text = "\n".join(
        lines
    )

    output_path.write_text(
        report_text,
        encoding="utf-8",
    )

    return output_path