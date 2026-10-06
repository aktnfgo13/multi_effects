import numpy as np

from analysis.benchmark_spectrum import (
    calculate_welch_spectrum,
)


# =========================================================
# Frequency Bands
# =========================================================

FREQUENCY_BANDS = {
    "sub": (
        20.0,
        80.0,
    ),

    "bass": (
        80.0,
        200.0,
    ),

    "low_mid": (
        200.0,
        500.0,
    ),

    "mid": (
        500.0,
        2000.0,
    ),

    "high_mid": (
        2000.0,
        5000.0,
    ),

    "presence": (
        5000.0,
        8000.0,
    ),

    "high": (
        8000.0,
        20000.0,
    ),
}


# =========================================================
# Band Analysis
# =========================================================

def calculate_band_metrics(
    reference,
    target,
    sample_rate,
    active_range_db=80.0,
):
    """
    Reference / Target Spectrum을
    주파수 대역별로 비교한다.

    Difference 정의:

        Reference - Target

    Positive:
        Target이 해당 주파수 대역에서 부족

    Negative:
        Target이 해당 주파수 대역에서 더 강함
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
        return {}

    # -----------------------------------------------------
    # Active Spectrum Threshold
    # -----------------------------------------------------

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

    results = {}

    # -----------------------------------------------------
    # Analyze Each Frequency Band
    # -----------------------------------------------------

    for (
        band_name,
        (
            min_frequency,
            max_frequency,
        ),
    ) in FREQUENCY_BANDS.items():

        max_frequency = min(
            max_frequency,
            sample_rate / 2.0,
        )

        band_mask = (
            (frequencies >= min_frequency)
            & (frequencies < max_frequency)
            & active_mask
        )

        if not np.any(
            band_mask
        ):
            results[band_name] = {
                "min_hz":
                    min_frequency,

                "max_hz":
                    max_frequency,

                "difference_db":
                    0.0,

                "mae_db":
                    0.0,

                "rmse_db":
                    0.0,

                "bins":
                    0,
            }

            continue

        reference_band = (
            reference_db[
                band_mask
            ]
        )

        target_band = (
            target_db[
                band_mask
            ]
        )

        difference = (
            reference_band
            - target_band
        )

        # Signed average difference
        difference_db = float(
            np.mean(
                difference
            )
        )

        # Absolute average difference
        mae_db = float(
            np.mean(
                np.abs(
                    difference
                )
            )
        )

        # Large errors receive more weight
        rmse_db = float(
            np.sqrt(
                np.mean(
                    difference ** 2
                )
            )
        )

        results[band_name] = {
            "min_hz":
                min_frequency,

            "max_hz":
                max_frequency,

            "difference_db":
                difference_db,

            "mae_db":
                mae_db,

            "rmse_db":
                rmse_db,

            "bins":
                int(
                    np.sum(
                        band_mask
                    )
                ),
        }

    return results