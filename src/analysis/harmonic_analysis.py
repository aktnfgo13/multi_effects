import numpy as np


def calculate_harmonics(
    audio_data,
    sample_rate,
    fundamental=1000,
    max_harmonic=10,
):
    # Stereo -> Mono
    if audio_data.ndim > 1:
        audio_data = np.mean(audio_data, axis=1)

    # DC 제거
    audio_data = audio_data - np.mean(audio_data)

    # Window 적용
    window = np.hanning(len(audio_data))
    windowed = audio_data * window

    # FFT
    spectrum = np.fft.rfft(windowed)
    frequencies = np.fft.rfftfreq(
        len(windowed),
        d=1 / sample_rate,
    )

    magnitude = np.abs(spectrum)

    harmonics = {}

    for harmonic_number in range(1, max_harmonic + 1):
        target_frequency = fundamental * harmonic_number

        if target_frequency > sample_rate / 2:
            break

        index = np.argmin(
            np.abs(frequencies - target_frequency)
        )

        harmonics[harmonic_number] = {
            "frequency": frequencies[index],
            "magnitude": magnitude[index],
        }

    return harmonics


def calculate_thd(harmonics):
    fundamental = harmonics[1]["magnitude"]

    harmonic_power = 0.0

    for harmonic_number, data in harmonics.items():
        if harmonic_number == 1:
            continue

        harmonic_power += data["magnitude"] ** 2

    thd = np.sqrt(harmonic_power) / fundamental

    return thd