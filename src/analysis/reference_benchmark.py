import numpy as np
import soundfile as sf

from scipy.signal import (
    correlate,
    correlation_lags,
)


# =========================================================
# Audio Loading
# =========================================================

def load_audio(path):
    """
    WAV 파일을 읽는다.

    Stereo인 경우 Mono로 변환한다.
    """

    audio, sample_rate = sf.read(
        path,
        dtype="float32",
    )

    if audio.ndim > 1:
        audio = np.mean(
            audio,
            axis=1,
        )

    return audio, sample_rate


# =========================================================
# Basic Metrics
# =========================================================

def calculate_peak(audio):
    """
    Peak amplitude 계산
    """

    if len(audio) == 0:
        return 0.0

    return float(
        np.max(
            np.abs(audio)
        )
    )


def calculate_rms(audio):
    """
    RMS 계산
    """

    if len(audio) == 0:
        return 0.0

    return float(
        np.sqrt(
            np.mean(
                audio ** 2
            )
        )
    )


def calculate_crest_factor_db(audio):
    """
    Crest Factor 계산

    Peak / RMS 비율을 dB로 반환한다.
    """

    peak = calculate_peak(
        audio
    )

    rms = calculate_rms(
        audio
    )

    if rms <= 1e-12:
        return 0.0

    return float(
        20.0
        * np.log10(
            peak / rms
        )
    )


# =========================================================
# Length Alignment
# =========================================================

def align_length(
    reference,
    target,
):
    """
    두 오디오의 길이를
    짧은 파일 기준으로 맞춘다.
    """

    length = min(
        len(reference),
        len(target),
    )

    return (
        reference[:length],
        target[:length],
    )


# =========================================================
# Correlation
# =========================================================

def calculate_correlation(
    reference,
    target,
):
    """
    Pearson Correlation 계산

     1.0 : 높은 양의 상관
     0.0 : 선형 상관 거의 없음
    -1.0 : 높은 음의 상관
    """

    if (
        len(reference) == 0
        or len(target) == 0
    ):
        return 0.0

    reference_std = np.std(
        reference
    )

    target_std = np.std(
        target
    )

    if (
        reference_std <= 1e-12
        or target_std <= 1e-12
    ):
        return 0.0

    return float(
        np.corrcoef(
            reference,
            target,
        )[0, 1]
    )


# =========================================================
# Latency Detection
# =========================================================

def find_analysis_start(
    reference,
    sample_rate,
    max_lag_ms=200.0,
):
    """
    Reference 파일에서 실제 신호가 시작되는
    위치를 찾는다.

    긴 무음 부분을 제외해서
    latency 탐색 효율을 높인다.
    """

    peak = calculate_peak(
        reference
    )

    if peak <= 1e-12:
        return 0

    threshold = max(
        peak * 0.01,
        1e-5,
    )

    active_indices = np.where(
        np.abs(reference)
        >= threshold
    )[0]

    if len(active_indices) == 0:
        return 0

    first_active = int(
        active_indices[0]
    )

    padding = int(
        sample_rate
        * max_lag_ms
        / 1000.0
        * 2.0
    )

    return max(
        0,
        first_active - padding,
    )


def estimate_latency_samples(
    reference,
    target,
    sample_rate,
    max_lag_ms=200.0,
    analysis_seconds=10.0,
):
    """
    Cross-correlation을 이용해
    Target의 Reference 대비 지연을 추정한다.

    Positive latency
        Target이 Reference보다 늦음

    Negative latency
        Target이 Reference보다 빠름
    """

    if (
        len(reference) == 0
        or len(target) == 0
    ):
        return 0

    start = find_analysis_start(
        reference,
        sample_rate,
        max_lag_ms,
    )

    analysis_samples = int(
        sample_rate
        * analysis_seconds
    )

    end = min(
        start + analysis_samples,
        len(reference),
        len(target),
    )

    reference_segment = (
        reference[start:end]
        .astype(np.float32)
    )

    target_segment = (
        target[start:end]
        .astype(np.float32)
    )

    if (
        len(reference_segment) < 2
        or len(target_segment) < 2
    ):
        return 0

    # DC 제거
    reference_segment = (
        reference_segment
        - np.mean(
            reference_segment
        )
    )

    target_segment = (
        target_segment
        - np.mean(
            target_segment
        )
    )

    reference_std = np.std(
        reference_segment
    )

    target_std = np.std(
        target_segment
    )

    if (
        reference_std <= 1e-12
        or target_std <= 1e-12
    ):
        return 0

    # Level 차이가 correlation에
    # 미치는 영향을 줄이기 위해 정규화
    reference_segment = (
        reference_segment
        / reference_std
    )

    target_segment = (
        target_segment
        / target_std
    )

    # Cross-correlation
    correlation = correlate(
        target_segment,
        reference_segment,
        mode="full",
        method="fft",
    )

    lags = correlation_lags(
        len(target_segment),
        len(reference_segment),
        mode="full",
    )

    max_lag_samples = int(
        sample_rate
        * max_lag_ms
        / 1000.0
    )

    valid_mask = (
        np.abs(lags)
        <= max_lag_samples
    )

    valid_correlation = (
        correlation[
            valid_mask
        ]
    )

    valid_lags = (
        lags[
            valid_mask
        ]
    )

    if len(valid_lags) == 0:
        return 0

    best_index = int(
        np.argmax(
            valid_correlation
        )
    )

    latency_samples = int(
        valid_lags[
            best_index
        ]
    )

    return latency_samples


def align_by_latency(
    reference,
    target,
    latency_samples,
):
    """
    추정된 latency를 이용해
    두 오디오를 sample 단위로 정렬한다.
    """

    if latency_samples > 0:

        # Target이 Reference보다 늦음
        target = target[
            latency_samples:
        ]

    elif latency_samples < 0:

        # Target이 Reference보다 빠름
        reference = reference[
            -latency_samples:
        ]

    return align_length(
        reference,
        target,
    )


# =========================================================
# Level Matching
# =========================================================

def match_rms(
    reference,
    target,
):
    """
    Target의 RMS를
    Reference RMS와 동일하게 맞춘다.

    Returns
    -------
    target_matched
    gain
    """

    reference_rms = (
        calculate_rms(
            reference
        )
    )

    target_rms = (
        calculate_rms(
            target
        )
    )

    if target_rms <= 1e-12:

        return (
            target.copy(),
            1.0,
        )

    gain = (
        reference_rms
        / target_rms
    )

    target_matched = (
        target * gain
    )

    return (
        target_matched,
        float(gain),
    )


# =========================================================
# Time Domain Metrics
# =========================================================

def calculate_mae(
    reference,
    target,
):
    """
    Mean Absolute Error
    """

    return float(
        np.mean(
            np.abs(
                reference
                - target
            )
        )
    )


def calculate_rmse(
    reference,
    target,
):
    """
    Root Mean Squared Error
    """

    return float(
        np.sqrt(
            np.mean(
                (
                    reference
                    - target
                ) ** 2
            )
        )
    )


# =========================================================
# Frequency Spectrum
# =========================================================

def calculate_spectrum(
    audio,
    sample_rate,
):
    """
    Hann Window + FFT를 이용해
    Frequency Spectrum을 계산한다.
    """

    if len(audio) == 0:

        return (
            np.array([]),
            np.array([]),
        )

    window = np.hanning(
        len(audio)
    )

    windowed_audio = (
        audio * window
    )

    spectrum = np.fft.rfft(
        windowed_audio
    )

    frequencies = (
        np.fft.rfftfreq(
            len(audio),
            d=1.0 / sample_rate,
        )
    )

    magnitude = np.abs(
        spectrum
    )

    normalization = max(
        np.sum(window) / 2.0,
        1e-12,
    )

    magnitude = (
        magnitude
        / normalization
    )

    magnitude_db = (
        20.0
        * np.log10(
            np.maximum(
                magnitude,
                1e-12,
            )
        )
    )

    return (
        frequencies,
        magnitude_db,
    )


# =========================================================
# Spectral Metrics
# =========================================================

def calculate_spectral_metrics(
    reference,
    target,
    sample_rate,
):
    """
    Reference / Target의
    Frequency Spectrum 차이를 계산한다.
    """

    (
        frequencies,
        reference_db,
    ) = calculate_spectrum(
        reference,
        sample_rate,
    )

    (
        _,
        target_db,
    ) = calculate_spectrum(
        target,
        sample_rate,
    )

    if len(frequencies) == 0:

        return {
            "spectral_mae_db": 0.0,
            "spectral_rmse_db": 0.0,
            "spectral_correlation": 0.0,
        }

    # 비교 주파수 범위
    max_frequency = min(
        20000.0,
        sample_rate / 2.0,
    )

    frequency_mask = (
        (frequencies >= 20.0)
        & (frequencies <= max_frequency)
    )

    # 너무 작은 FFT bin 제거
    highest_level = max(
        np.max(reference_db),
        np.max(target_db),
    )

    active_mask = (
        np.maximum(
            reference_db,
            target_db,
        )
        >= highest_level - 80.0
    )

    mask = (
        frequency_mask
        & active_mask
    )

    reference_active = (
        reference_db[
            mask
        ]
    )

    target_active = (
        target_db[
            mask
        ]
    )

    if len(reference_active) == 0:

        return {
            "spectral_mae_db": 0.0,
            "spectral_rmse_db": 0.0,
            "spectral_correlation": 0.0,
        }

    difference = (
        reference_active
        - target_active
    )

    spectral_mae_db = float(
        np.mean(
            np.abs(
                difference
            )
        )
    )

    spectral_rmse_db = float(
        np.sqrt(
            np.mean(
                difference ** 2
            )
        )
    )

    if (
        np.std(reference_active)
        <= 1e-12
        or
        np.std(target_active)
        <= 1e-12
    ):

        spectral_correlation = 0.0

    else:

        spectral_correlation = float(
            np.corrcoef(
                reference_active,
                target_active,
            )[0, 1]
        )

    return {
        "spectral_mae_db":
            spectral_mae_db,

        "spectral_rmse_db":
            spectral_rmse_db,

        "spectral_correlation":
            spectral_correlation,
    }


# =========================================================
# Main Benchmark
# =========================================================

def compare_audio(
    reference_path,
    target_path,
):
    """
    Reference Benchmark v0.3

    Process
    -------
    1. WAV Load
    2. Sample Rate Check
    3. Candidate Latency Detection
    4. Candidate Alignment Test
    5. Alignment Accept / Reject
    6. RMS Level Matching
    7. Time Domain Analysis
    8. Frequency Domain Analysis
    """

    # -----------------------------------------------------
    # Load Audio
    # -----------------------------------------------------

    reference, reference_sr = (
        load_audio(
            reference_path
        )
    )

    target, target_sr = (
        load_audio(
            target_path
        )
    )


    # -----------------------------------------------------
    # Sample Rate Check
    # -----------------------------------------------------

    if reference_sr != target_sr:

        raise ValueError(
            "Sample rates must match. "
            f"Reference={reference_sr}, "
            f"Target={target_sr}"
        )


    # -----------------------------------------------------
    # Original Length Alignment
    # -----------------------------------------------------

    (
        pre_reference,
        pre_target,
    ) = align_length(
        reference,
        target,
    )


    # -----------------------------------------------------
    # Correlation Before Alignment
    # -----------------------------------------------------

    pre_alignment_correlation = (
        calculate_correlation(
            pre_reference,
            pre_target,
        )
    )


    # -----------------------------------------------------
    # Candidate Latency Detection
    # -----------------------------------------------------

    candidate_latency_samples = (
        estimate_latency_samples(
            reference,
            target,
            reference_sr,
        )
    )

    candidate_latency_ms = (
        candidate_latency_samples
        / reference_sr
        * 1000.0
    )


    # -----------------------------------------------------
    # Candidate Alignment
    # -----------------------------------------------------

    (
        candidate_reference,
        candidate_target,
    ) = align_by_latency(
        reference,
        target,
        candidate_latency_samples,
    )


    # -----------------------------------------------------
    # Candidate Correlation
    # -----------------------------------------------------

    candidate_correlation = (
        calculate_correlation(
            candidate_reference,
            candidate_target,
        )
    )


    # -----------------------------------------------------
    # Accept / Reject Candidate Alignment
    # -----------------------------------------------------

    minimum_improvement = 0.001

    if (
        candidate_correlation
        >
        pre_alignment_correlation
        + minimum_improvement
    ):

        latency_samples = (
            candidate_latency_samples
        )

        reference = (
            candidate_reference
        )

        target = (
            candidate_target
        )

        alignment_applied = True

    else:

        latency_samples = 0

        reference = (
            pre_reference
        )

        target = (
            pre_target
        )

        alignment_applied = False


    latency_ms = (
        latency_samples
        / reference_sr
        * 1000.0
    )


    # -----------------------------------------------------
    # RMS Level Matching
    # -----------------------------------------------------

    (
        target_matched,
        rms_match_gain,
    ) = match_rms(
        reference,
        target,
    )


    # -----------------------------------------------------
    # Correlation After Alignment
    # -----------------------------------------------------

    post_alignment_correlation = (
        calculate_correlation(
            reference,
            target_matched,
        )
    )


    # -----------------------------------------------------
    # Frequency Analysis
    # -----------------------------------------------------

    spectral_results = (
        calculate_spectral_metrics(
            reference,
            target_matched,
            reference_sr,
        )
    )


    # -----------------------------------------------------
    # Results
    # -----------------------------------------------------

    results = {

        # Audio
        "sample_rate":
            reference_sr,

        "samples":
            len(reference),

        "duration_seconds":
            len(reference)
            / reference_sr,


        # Candidate Latency
        "candidate_latency_samples":
            candidate_latency_samples,

        "candidate_latency_ms":
            candidate_latency_ms,

        "candidate_correlation":
            candidate_correlation,


        # Accepted Latency
        "alignment_applied":
            alignment_applied,

        "latency_samples":
            latency_samples,

        "latency_ms":
            latency_ms,


        # Correlation
        "pre_alignment_correlation":
            pre_alignment_correlation,

        "post_alignment_correlation":
            post_alignment_correlation,


        # Level Match
        "rms_match_gain":
            rms_match_gain,


        # Peak
        "reference_peak":
            calculate_peak(
                reference
            ),

        "target_peak":
            calculate_peak(
                target_matched
            ),


        # RMS
        "reference_rms":
            calculate_rms(
                reference
            ),

        "target_rms":
            calculate_rms(
                target_matched
            ),


        # Crest Factor
        "reference_crest_db":
            calculate_crest_factor_db(
                reference
            ),

        "target_crest_db":
            calculate_crest_factor_db(
                target_matched
            ),


        # Time Domain
        "mae":
            calculate_mae(
                reference,
                target_matched,
            ),

        "rmse":
            calculate_rmse(
                reference,
                target_matched,
            ),

        "correlation":
            post_alignment_correlation,
    }


    # -----------------------------------------------------
    # Add Spectral Results
    # -----------------------------------------------------

    results.update(
        spectral_results
    )

    return results