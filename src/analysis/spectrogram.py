import numpy as np


def calculate_stft(
    audio_data,
    sample_rate,
    frame_size=2048,
    hop_size=512,
):
    # Stereo -> Mono
    if audio_data.ndim > 1:
        audio_data = np.mean(audio_data, axis=1)

    window = np.hanning(frame_size)

    frames = []

    for start in range(
        0,
        len(audio_data) - frame_size,
        hop_size,
    ):
        frame = audio_data[
            start:start + frame_size
        ]

        windowed = frame * window

        spectrum = np.fft.rfft(windowed)

        magnitude = np.abs(spectrum)

        magnitude_db = 20 * np.log10(
            magnitude + 1e-12
        )

        frames.append(magnitude_db)

    spectrogram = np.array(frames).T

    frequencies = np.fft.rfftfreq(
        frame_size,
        d=1 / sample_rate,
    )

    times = (
        np.arange(spectrogram.shape[1])
        * hop_size
        / sample_rate
    )

    return (
        spectrogram,
        frequencies,
        times,
    )