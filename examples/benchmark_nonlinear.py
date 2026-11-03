"""Run: .venv/bin/python examples/benchmark_nonlinear.py
Coherent high-tone residual is an alias proxy, not THD or a listening score.
Reuses repository RMS/peak/FFT/harmonic metrics; saves spectra for all signals.
"""
from pathlib import Path
import sys, subprocess, json
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
from analysis.audio_analysis import calculate_peak, calculate_rms, calculate_fft
from analysis.harmonic_analysis import calculate_harmonics, calculate_thd
root = Path(__file__).resolve().parents[1]
out = root / 'cpp/build/v03-analysis'; out.mkdir(exist_ok=True)
exe = root / 'cpp/build/nonlinear_benchmark'
sr = 48000; t = np.arange(sr * 2) / sr
signals = {'sine1k': .5*np.sin(2*np.pi*1000*t), 'high7013': .5*np.sin(2*np.pi*7013*t),
           'multitone': .2*(np.sin(2*np.pi*997*t)+np.sin(2*np.pi*3203*t)+np.sin(2*np.pi*7013*t)),
           'sweep': .5*np.sin(2*np.pi*(50*t+(20000-50)/4*t*t)),
           'guitar': sum(.22/k*np.sin(2*np.pi*110*k*t) for k in range(1,20))}
rows=[]; spectra={}
for effect in ['distortion','amp_sim']:
    for name,x in signals.items():
        x.astype('float32').tofile(out/'input.f32')
        for mode in range(3):
            subprocess.run([str(exe),effect,str(mode),str(sr),str(out/'input.f32'),str(out/'output.f32'),'24'],check=True)
            y=np.fromfile(out/'output.f32',dtype='float32').astype(float)[-sr:]
            assert np.isfinite(y).all()
            f,db=calculate_fft(y,sr);spectra[f'{effect}_{name}_{1<<mode}x']=db
            row=dict(effect=effect,signal=name,factor=1<<mode,rms=float(calculate_rms(y)),peak=float(calculate_peak(y)))
            if name in ('sine1k','high7013'):
                fundamental=1000 if name=='sine1k' else 7013
                harmonics=calculate_harmonics(y,sr,fundamental,24)
                row['thd']=float(calculate_thd(harmonics));row['harmonic_magnitudes']={k:float(v['magnitude']) for k,v in harmonics.items()}
            if name=='high7013':
                power=np.abs(np.fft.rfft(y-y.mean()))**2
                valid=np.zeros(len(power),bool);valid[0:2]=True
                for hz in range(7013,sr//2,7013): valid[hz-1:hz+2]=True
                row['alias_residual_dbc']=float(10*np.log10(max(power[~valid].sum(),1e-30)/power[7013]))
            rows.append(row)
for row in rows:
    base=spectra[f"{row['effect']}_{row['signal']}_1x"]
    current=spectra[f"{row['effect']}_{row['signal']}_{row['factor']}x"]
    mask=base>base.max()-80
    row['spectrum_mean_abs_db_difference_vs_1x']=float(np.mean(np.abs(current[mask]-base[mask])))
np.savez(out/'spectra.npz',frequency=f,**spectra)
(out/'metrics.json').write_text(json.dumps(rows,indent=2))
profile=subprocess.check_output([str(exe),'profile'],text=True)
(out/'profile.csv').write_text('effect,factor,mean_us,max_us,max_deadline_percent,latency_samples\n'+profile)
lines=['# Nonlinear measurement (48 kHz)','', 'Drive/preamp 24 dB; steady-state coherent 7013 Hz, amplitude 0.5.',
       'Residual excludes DC and legitimate in-band harmonic bins (±1 bin). No window on coherent residual FFT.',
       'This is a signal-specific alias proxy, not universal sound quality. 1 kHz THD includes intended distortion.', '',
       '| Effect | Factor | Alias residual dBc | RMS | Peak | THD |','|---|---:|---:|---:|---:|---:|']
for r in rows:
    if r['signal']=='high7013': lines.append(f"| {r['effect']} | {r['factor']} | {r['alias_residual_dbc']:.2f} | {r['rms']:.5f} | {r['peak']:.5f} | {r['thd']:.5f} |")
lines+=['','Profile: mean/max observed; excludes device/GUI overhead. 128 samples deadline = 2666.67 us.','```csv',profile.strip(),'```',
        '', 'metrics.json contains RMS/peak for sine, high tone, multitone, sweep and harmonic guitar signal; spectra.npz contains reusable FFT spectra.']
(out/'report.md').write_text('\n'.join(lines)+'\n')
print('\n'.join(lines))
for effect in ['distortion','amp_sim']:
    results={r['factor']:r['alias_residual_dbc'] for r in rows if r['effect']==effect and r['signal']=='high7013'}
    assert results[2]<results[1]-6 and results[4]<results[1]-6, (effect,results)
