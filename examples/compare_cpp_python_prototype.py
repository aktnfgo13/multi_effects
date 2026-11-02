"""Offline reference validation, including the cabinet's documented 128-sample latency."""
import argparse
from pathlib import Path
import subprocess
import numpy as np
from effects.delay import apply_delay
from effects.reverb import apply_reverb
from effects.amp_sim import apply_amp_sim
from effects.tremolo import apply_tremolo
from effects.cabinet_ir import apply_cabinet_ir

parser = argparse.ArgumentParser()
parser.add_argument('--executable', type=Path, default=Path('cpp/build/prototype_cli'))
args = parser.parse_args()
for rate in (44100, 48000, 96000):
    x = np.random.default_rng(42).normal(0, .1, int(rate * .5)).astype(np.float32).astype(np.float64)
    x = np.concatenate([x, np.zeros(int(rate * .5))])
    refs = {
        'delay': apply_delay(x, rate),
        'reverb': apply_reverb(x, rate, tail_seconds=0),
        'amp': apply_amp_sim(x, rate),
        'tremolo': apply_tremolo(x, rate),
        'cabinet': np.concatenate([np.zeros(128), apply_cabinet_ir(x, np.array([.75, -.25, .125]))])[:len(x)],
    }
    for name, expected in refs.items():
        result = subprocess.run([str(args.executable), name, str(rate)], input='\n'.join(map(str, x)), text=True, capture_output=True, check=True)
        actual = np.fromstring(result.stdout, sep='\n')
        print(rate, name, 'max_abs_error', np.max(np.abs(actual - expected)), flush=True)
        np.testing.assert_allclose(actual, expected, atol=1e-4, rtol=1e-5)
