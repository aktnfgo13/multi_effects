"""Offline input calibration. Synthetic picked notes, full-scale amplitude = 1."""
import argparse
import csv
from pathlib import Path
import subprocess
import tempfile

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
RATE = 48000


def rms(data):
    return np.sqrt(np.mean(data.astype(float)**2))


def db(value):
    return 20*np.log10(value)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--previous', type=Path, help='Optional approved Custom OD benchmark executable')
    parser.add_argument('--output', type=Path, default=ROOT/'cpp/build/input-calibration.csv')
    args = parser.parse_args()
    tool = ROOT/'cpp/build/custom_drive_benchmark'
    # Same 110 Hz, eleven-harmonic pluck as benchmark_grain.py.
    t = np.arange(RATE//2)/RATE
    carrier = sum(np.sin(2*np.pi*110*k*t)/k for k in range(1,12))
    pluck = carrier/max(abs(carrier))*(1-np.exp(-t/.0004))*np.exp(-t/.09)
    for amp in (.06,.6):
        print(f'Legacy {amp} pluck: peak {db(max(abs(amp*pluck))):.2f}, RMS {db(rms(amp*pluck)):.2f} dBFS')
    note = pluck[:int(.12*RATE)]
    rows = []
    with tempfile.TemporaryDirectory() as directory:
        src, dst = (Path(directory)/name for name in ('input.f32','output.f32'))

        def render(samples, exe, trim=None):
            samples.astype(np.float32).tofile(src)
            cmd = [str(exe),'custom_drive','1',str(RATE),str(src),str(dst)]
            if trim is not None:
                cmd.append(f'input_trim_db={trim}')
            result = subprocess.check_output(cmd,text=True)
            return np.fromfile(dst,np.float32), [float(x) for x in result.split()]

        linear, (_, _, nominal) = render(note*.0001, tool, 0)
        linear_gain = rms(linear)/rms(note*.0001)
        for label, offset in [('very weak',-18),('weak',-9),('nominal',0),('strong',6),('very hot',12)]:
            x = (note * (10**((nominal+offset)/20)/rms(note))).astype(np.float32)
            y, (peak, post_rms, _) = render(x,tool,0)
            if args.previous:
                frozen,_ = render(x,args.previous)
                np.testing.assert_array_equal(y,frozen)
            rows.append(dict(level=label,peak_dbfs=db(max(abs(x))),rms_dbfs=db(rms(x)),
                             crest_db=db(max(abs(x))/rms(x)),post_peak_dbfs=db(peak),
                             post_rms_dbfs=db(post_rms),od_rms_dbfs=db(rms(y)),
                             gain_loss_db=db(linear_gain/(rms(y)/post_rms))))
    with args.output.open('w') as f:
        writer=csv.DictWriter(f,fieldnames=rows[0]);writer.writeheader();writer.writerows(rows)


if __name__ == '__main__':
    main()
