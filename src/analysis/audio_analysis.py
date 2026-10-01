import numpy as np


def calculate_peak(audio_data):
    return np.max(np.abs(audio_data))


def calculate_rms(audio_data):
    return np.sqrt(np.mean(audio_data ** 2))


def calculate_fft(audio_data, sample_rate):
    # Stereo -> Mono
    if audio_data.ndim > 1:
        audio_data = np.mean(audio_data, axis=1)

    fft_size = min(len(audio_data), 65536)

    signal = audio_data[:fft_size]

    # FFT leakage 감소
    window = np.hanning(len(signal))
    windowed_signal = signal * window

    spectrum = np.fft.rfft(windowed_signal)

    frequencies = np.fft.rfftfreq(
        len(windowed_signal),
        d=1 / sample_rate,
    )

    magnitude = np.abs(spectrum)

    magnitude_db = 20 * np.log10(
        magnitude + 1e-12
    )

    return frequencies, magnitude_db