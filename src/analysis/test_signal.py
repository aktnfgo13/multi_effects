import numpy as np


def generate_sine(
    frequency=1000,
    sample_rate=48000,
    duration=2.0,
    amplitude=0.2,
):
    t = np.arange(
        int(sample_rate * duration)
    ) / sample_rate

    signal = amplitude * np.sin(
        2 * np.pi * frequency * t
    )

    return signal

if __name__ == "__main__":
    signal = generate_sine()

    print(signal[:20])
    print(f"Samples: {len(signal)}")
    print(f"Peak: {abs(signal).max():.4f}")