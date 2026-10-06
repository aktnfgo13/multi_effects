import numpy as np

from scipy.signal import welch


# =========================================================
# Welch Spectrum
# =========================================================

def calculate_welch_spectrum(
    audio,
    sample_rate,
    nperseg=8192,
    overlap_ratio=0.5,
):
    """
    Welch Method를 이용해
    평균 Power Spectrum Density를 계산한다.

    긴 WAV 전체를 한 번 FFT하는 대신,
    여러 Frame의 Spectrum을 평균내므로
    보다 안정적인 비교가 가능하다.

    Returns
    -------
    frequencies : np.ndarray

    spectrum_db : np.ndarray
        Power Spectrum Density [dB]
    """

    if len(audio) < 2:
        return (
            np.array([]),
            np.array([]),
        )

    segment_length = min(
        nperseg,
        len(audio),
    )

    noverlap = int(
        segment_length
        * overlap_ratio
    )

    noverlap = min(
        noverlap,
        segment_length - 1,
    )

    frequencies, power = welch(
        audio,
        fs=sample_rate,
        window="hann",
        nperseg=segment_length,
        noverlap=noverlap,
        detrend="constant",
        scaling="density",
    )

    spectrum_db = (
        10.0
        * np.log10(
            np.maximum(
                power,
                1e-20,
            )
        )
    )

    return (
        frequencies,
        spectrum_db,
    )


# =========================================================
# Spectral Comparison
# =========================================================

def calculate_welch_spectral_metrics(
    reference,
    target,
    sample_rate,
    min_frequency=20.0,
    max_frequency=20000.0,
    active_range_db=80.0,
):
    """
    Welch Spectrum을 이용해
    Reference / Target의 주파수 차이를 비교한다.
    """

    (
        frequencies,
        reference_db,
    ) = calculate_welch_spectrum(
        reference,
        sample_rate,
    )

    (
        target_frequencies,
        target_db,
    ) = calculate_welch_spectrum(
        target,
        sample_rate,
    )

    if (
        len(frequencies) == 0
        or len(target_frequencies) == 0
    ):
        return {
            "spectral_mae_db": 0.0,
            "spectral_rmse_db": 0.0,
            "spectral_correlation": 0.0,
        }

    max_frequency = min(
        max_frequency,
        sample_rate / 2.0,
    )

    frequency_mask = (
        (frequencies >= min_frequency)
        & (frequencies <= max_frequency)
    )

    highest_level = max(
        np.max(reference_db),
        np.max(target_db),
    )

    active_mask = (
        np.maximum(
            reference_db,
            target_db,
        )
        >= highest_level - active_range_db
    )

    mask = (
        frequency_mask
        & active_mask
    )

    reference_active = (
        reference_db[mask]
    )

    target_active = (
        target_db[mask]
    )

    if len(reference_active) == 0:
        return {
            "spectral_mae_db": 0.0,
            "spectral_rmse_db": 0.0,
            "spectral_correlation": 0.0,
        }

    difference_db = (
        reference_active
        - target_active
    )

    spectral_mae_db = float(
        np.mean(
            np.abs(
                difference_db
            )
        )
    )

    spectral_rmse_db = float(
        np.sqrt(
            np.mean(
                difference_db ** 2
            )
        )
    )

    if (
        np.std(reference_active) <= 1e-12
        or np.std(target_active) <= 1e-12
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