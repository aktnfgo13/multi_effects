import numpy as np


def apply_gain(audio_data, gain_db):
    gain_linear = 10 ** (gain_db / 20)

    output = audio_data * gain_linear
    output = np.clip(output, -1.0, 1.0)

    return output