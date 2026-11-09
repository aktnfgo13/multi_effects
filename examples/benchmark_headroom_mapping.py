"""Five-point Custom OD analysis, using the Grain listening harness."""
import csv

import numpy as np
from scipy.io import wavfile
from benchmark_custom_drive import level_match


def measure_mapping(run, render, passages, out, previous, sr, rate):
    points = (0, 25, 50, 75, 100)
    t = np.arange(sr // 4) / sr
    sine = np.sin(2 * np.pi * 440 * t)
    levels = np.arange(-64, 1, 1)
    steps = np.concatenate([10**(db / 20) * sine for db in levels])
    strong, weak = passages[4:6]
    before = np.concatenate([strong[:int(.12 * sr)], np.zeros(int(.02 * sr))])
    sequence = np.concatenate([before, weak])
    rows, audio = [], {}
    rms = lambda x: np.sqrt(np.mean(np.square(x)))
    peak = lambda x: np.max(np.abs(x))
    db = lambda x: 20 * np.log10(x)
    for c in (0, 50, 100):
        for g in (0, 50, 100):
            for h in points:
                params = dict(crunch=c, headroom=h, grain=g)
                pick, quiet = run(strong, params), run(weak, params)
                attack, body, sustain = np.split(pick, [int(.03 * sr), int(.15 * sr)])
                after = run(sequence, params)[len(before):]
                window = slice(64, 64 + int(.03 * sr))
                sweep = run(steps, params).reshape(len(levels), len(t))[:, -sr//10:]
                gains = db(np.sqrt(np.mean(sweep**2, axis=1))) - levels
                loss = gains[0] - gains
                crossing = np.flatnonzero(loss >= 1)[0]
                onset = np.interp(1, loss[crossing-1:crossing+1], levels[crossing-1:crossing+1])
                def thd(segment):
                    fft = np.abs(np.fft.rfft(segment))
                    return 100 * np.linalg.norm(fft[88:44*33:44]) / fft[44]
                steady = run(.3*sine, params)[-sr//10:]
                phrase = render(params)
                if g == 50:
                    audio[c,h] = phrase
                if previous and h == 50:
                    np.testing.assert_array_equal(phrase, render(params, previous))
                rows.append(dict(crunch=c, grain=g, headroom=h, rms=rms(steady), peak=peak(steady),
                                 thd=thd(steady), weak_thd=thd(sweep[levels.tolist().index(-24)]),
                                 attack_rms=rms(attack), attack_peak=peak(attack), body_rms=rms(body),
                                 body_crest=db(peak(body)/rms(body)), sustain_rms=rms(sustain),
                                 contrast=db(rms(pick)/rms(quiet)), onset_db=onset,
                                 recovery_db=db(rms(after[window])/rms(quiet[window])),
                                 phrase_rms=rms(phrase)))
    with (out/'mapping.csv').open('w') as f:
        writer = csv.DictWriter(f, fieldnames=rows[0])
        writer.writeheader()
        writer.writerows(rows)
    matched = {k: level_match(y, audio[50,50]) for k,y in audio.items()}
    trim = min(1, .95/max(map(peak, matched.values())))
    for name, signals, scale in [('raw',audio,1), ('level_matched',matched,trim)]:
        folder = out/name
        folder.mkdir(exist_ok=True)
        for (c,h), y in signals.items():
            wavfile.write(folder/f'crunch{c}_headroom{h}.wav', rate, (scale*y).astype(np.float32))

    for c in (0, 50, 100):
        for g in (0, 50, 100):
            group = [row for row in rows if row['crunch'] == c and row['grain'] == g]
            for key in ('onset_db', 'contrast', 'body_crest', 'recovery_db'):
                if not np.all(np.diff([row[key] for row in group]) > 0):
                    raise AssertionError(f'Crunch {c}, Grain {g}: {key} reverses')
            if g == 50:
                for key, direction in [('attack_peak', 1), ('weak_thd', -1), ('thd', -1)]:
                    if not np.all(direction * np.diff([row[key] for row in group]) > 0):
                        raise AssertionError(f'Crunch {c}, Grain {g}: {key} reverses')
