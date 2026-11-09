"""Reproducible synthetic quality characterization, not a perceptual score.
Run: .venv/bin/python examples/benchmark_custom_drive.py [--di licensed.wav]
Uses existing RMS/peak/FFT/harmonic analysis. No analysis normalization enters DSP.
"""
from pathlib import Path
import argparse
import csv
import json
import subprocess
import sys
import numpy as np
from scipy.io import wavfile
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'src'))
from analysis.audio_analysis import calculate_rms, calculate_peak, calculate_fft
from analysis.harmonic_analysis import calculate_harmonics, calculate_thd


def level_match(x, reference):
    """Offline RMS matching; silence stays silent, no peak limiting."""
    rms = float(calculate_rms(x))
    return x * (float(calculate_rms(reference)) / rms) if rms > 1e-12 else x.copy()


def metrics(y, sr, frequency=None):
    result = dict(rms=float(calculate_rms(y)), peak=float(calculate_peak(y)), dc=float(np.mean(y)))
    result['crest_db'] = float(20*np.log10(max(result['peak'], 1e-15)/max(result['rms'], 1e-15)))
    if frequency:
        harmonics = calculate_harmonics(y, sr, frequency, min(32, (sr//2-1)//frequency))
        result['thd'] = float(calculate_thd(harmonics))
        first = harmonics[1]['magnitude']
        even=sum(v['magnitude']**2 for k,v in harmonics.items() if k>=2 and k%2==0)
        odd=sum(v['magnitude']**2 for k,v in harmonics.items() if k>=3 and k%2==1)
        result['even_odd_db']=float(10*np.log10(max(even,1e-30)/max(odd,1e-30)))
        result['harmonics_dbc'] = {str(k): float(20*np.log10(max(v['magnitude'], 1e-15)/max(first, 1e-15))) for k, v in harmonics.items()}
    return result


def residual_dbc(y, sr, fundamental):
    # One second coherent unwindowed FFT. Only legitimate in-band harmonics
    # and DC are removed. In steady periodic tests, remaining bins proxy aliases.
    power = np.abs(np.fft.rfft(y-y.mean()))**2
    valid = np.zeros(len(power), bool); valid[:2] = True
    for hz in range(fundamental, sr//2, fundamental): valid[hz-1:hz+2] = True
    return float(10*np.log10(max(power[~valid].sum(), 1e-30)/max(power[fundamental], 1e-30)))


def sweep_values(text):
    values = [float(v) for v in text.split(',')]
    if not values or any(not np.isfinite(v) or v < 0 or v > 100 for v in values):
        raise argparse.ArgumentTypeError('Sweep values must be finite percentages in [0, 100]')
    if len(set(values)) != len(values):
        raise argparse.ArgumentTypeError('Duplicate sweep values')
    return values


def write_listening_sweeps(out, signal, rate, run, values, contrasts=False):
    """Float WAV preserves headroom; one common RMS reference, never per-file limiting.

    DI channels are rendered independently with fresh state and preserved in output.
    """
    if not np.isfinite(signal).all():
        raise ValueError('Listening input contains non-finite samples')
    def render(params):
        if signal.ndim == 1:
            return run(signal, params=params, rate=rate)
        return np.column_stack([run(channel, params=params, rate=rate) for channel in signal.T])
    reference = render({})
    manifest = []
    for folder in ('raw', 'level_matched'):
        (out/folder).mkdir(parents=True, exist_ok=True)
    for parameter in ('drive', 'tone', 'crunch', 'headroom', 'grain'):
        for value in values:
            y = render({parameter: value})
            matched = level_match(y, reference)
            label = f'{value:03g}'.replace('.', 'p')
            name = f'{parameter}_{label}.wav'
            wavfile.write(out/'raw'/name, rate, y.astype(np.float32))
            wavfile.write(out/'level_matched'/name, rate, matched.astype(np.float32))
            manifest.append(dict(parameter=parameter, value=value, file=name,
                                 raw_rms=float(calculate_rms(y)), raw_peak=float(calculate_peak(y)),
                                 matched_rms=float(calculate_rms(matched)), matched_peak=float(calculate_peak(matched))))
    if contrasts:
        for name,params in [('crunch000_grain100',{'crunch':0,'grain':100}),
                            ('crunch100_grain000',{'crunch':100,'grain':0}),
                            ('drive070_headroom000',{'drive':70,'headroom':0}),
                            ('drive070_headroom100',{'drive':70,'headroom':100})]:
            y=render(params);matched=level_match(y,reference)
            wavfile.write(out/'raw'/f'{name}.wav',rate,y.astype(np.float32))
            wavfile.write(out/'level_matched'/f'{name}.wav',rate,matched.astype(np.float32))
            manifest.append(dict(file=f'{name}.wav',parameters=params,raw_rms=float(calculate_rms(y)),
                                 raw_peak=float(calculate_peak(y)),matched_rms=float(calculate_rms(matched))))
    (out/'listening_manifest.json').write_text(json.dumps(manifest, indent=2))
    return manifest


def main():
    parser = argparse.ArgumentParser(); parser.add_argument('--di', type=Path)
    parser.add_argument('--steps', type=sweep_values, default=sweep_values('0,25,50,75,100'))
    parser.add_argument('--baseline', type=Path, help='Previous metrics.json for before/after comparison')
    parser.add_argument('--executable',type=Path,default=ROOT/'cpp/build/custom_drive_benchmark')
    parser.add_argument('--mode',type=int,choices=(1,2,4),default=4,help='Macro analysis/listening oversampling factor')
    parser.add_argument('--check-texture',action='store_true',help='Grain distribution and explicitly reversed Headroom comparison checks')
    parser.add_argument('--check-macros',action='store_true',help='Check redesign endpoint/dynamic guards (not perceptual approval)')
    parser.add_argument('--skip-profile',action='store_true',help='Do not profile during tuning iterations')
    parser.add_argument('--output', type=Path, default=ROOT/'cpp/build/v08-quality')
    args = parser.parse_args(); out=args.output; out.mkdir(parents=True, exist_ok=True)
    executable=args.executable; sr=48000; t=np.arange(2*sr)/sr
    default_mode={1:0,2:1,4:2}[args.mode]
    latency=0 if args.mode==1 else 64
    def run(x, mode=default_mode, params=None, effect='custom_drive', rate=sr):
        np.asarray(x, dtype=np.float32).tofile(out/'input.f32')
        subprocess.run([str(executable), effect, str(mode), str(rate), str(out/'input.f32'), str(out/'output.f32')]+[f'{k}={v}' for k,v in (params or {}).items()], check=True)
        y=np.fromfile(out/'output.f32', dtype=np.float32).astype(float)
        if not np.isfinite(y).all(): raise RuntimeError('Non-finite benchmark output')
        return y
    signals={f'sine{hz}': .3*np.sin(2*np.pi*hz*t) for hz in (100,440,1000,7013)}
    signals['two_tone']=.32*np.sin(2*np.pi*60*t)+.08*np.sin(2*np.pi*7000*t)
    signals['harmonic_rich']=sum(.24/k*np.sin(2*np.pi*110*k*t) for k in range(1,20))
    signals['multitone']=sum(.10*np.sin(2*np.pi*f*t) for f in (197,997,3203,7013))
    signals['picking']=signals['harmonic_rich']*np.exp(-np.mod(t,.25)/.07)
    # Repeated weak/medium/strong plucks, with enough space to observe recovery.
    dynamic_t=np.arange(3*sr)/sr
    dynamic=np.zeros(3*sr)
    for index,amplitude in enumerate((.06,.2,.6,.06,.2,.6)):
        start=index*sr//2;local=np.arange(sr//2)/sr
        carrier=sum(np.sin(2*np.pi*110*k*local)/k for k in range(1,12))
        carrier/=np.max(np.abs(carrier))
        dynamic[start:start+sr//2]=amplitude*carrier*(1-np.exp(-local/.0004))*np.exp(-local/.09)

    records=[]; spectra={}; outputs={}
    for name,x in signals.items():
        for mode in range(3):
            y=run(x,mode); steady=y[-sr:]; f,db=calculate_fft(steady,sr); spectra[f'{name}_{1<<mode}x']=db
            row=dict(signal=name,factor=1<<mode,**metrics(steady,sr,int(name[4:]) if name.startswith('sine') else None))
            if name=='sine7013': row['alias_residual_dbc']=residual_dbc(steady,sr,7013)
            if name=='two_tone':
                power=np.abs(np.fft.rfft(steady-steady.mean()))**2
                bins=[7000+sign*60*n for n in range(1,6) for sign in (-1,1)]
                row['imd_sidebands_dbc']=float(10*np.log10(max(sum(power[b] for b in bins),1e-30)/max(power[7000],1e-30)))
            records.append(row);outputs[name,mode]=y
    stress=[]
    for mode in range(3):
        y=run(signals["sine7013"],mode,{"drive":100})[-sr:]
        stress.append(dict(factor=1<<mode,drive=100,alias_residual_dbc=residual_dbc(y,sr,7013),**metrics(y,sr,7013)))
    sweeps=[]
    for parameter in ('drive','tone','crunch','headroom','grain'):
        for value in args.steps:
            y=run(signals['sine440'],params={parameter:value}); row=dict(parameter=parameter,value=value,**metrics(y[-sr:],sr,440))
            hf=run(signals['sine7013'],params={parameter:value});row['alias_residual_dbc']=residual_dbc(hf[-sr:],sr,7013)
            two=run(signals['two_tone'],params={parameter:value});power=np.abs(np.fft.rfft(two[-sr:]-two[-sr:].mean()))**2
            row['imd_sidebands_dbc']=float(10*np.log10(max(sum(power[7000+s*60*n] for n in range(1,6) for s in (-1,1)),1e-30)/max(power[7000],1e-30)))
            matched_sweep=level_match(y[-sr:],outputs['sine440',default_mode][-sr:])
            _,matched_db=calculate_fft(matched_sweep,sr);_,base_db=calculate_fft(outputs['sine440',default_mode][-sr:],sr)
            mask=base_db>base_db.max()-80
            row['matched_peak']=float(calculate_peak(matched_sweep));row['matched_spectrum_difference_db']=float(np.mean(np.abs(matched_db[mask]-base_db[mask])))
            row['level_curve']=[]
            for fraction in (1,.7,.5,.3):
                probe=run(signals['sine440']*fraction,params={parameter:value})[-sr:]
                row['level_curve'].append(dict(input_fraction=fraction,**metrics(probe,sr,440)))
            sweeps.append(row)
    cleanup=[]
    for amplitude in (1,.7,.5,.3,.1):
        y=run(signals['sine440']*amplitude)
        cleanup.append(dict(input_fraction=amplitude,**metrics(y[-sr:],sr,440)))
    dynamics=[]
    for h in (0,50,100):
        x=.3*np.sin(2*np.pi*440*t);x[(t>=.5)&(t<1)]=0;x[(t>=1)&(t<1.5)]*=.15;x[t>=1.5]=0
        y=run(x,params={'headroom':h})
        dynamics.append(dict(headroom=h,attack_rms=float(calculate_rms(y[latency:latency+480])),sustain_rms=float(calculate_rms(y[19200:24000])),quiet_probe_rms=float(calculate_rms(y[62400:67200])),tail_peak=float(calculate_peak(y[-2400:]))))
    recovery=[];grain_recovery=[]
    for key,results in (('headroom',recovery),('grain',grain_recovery)):
        for value in (0,100):
            for gap_ms in (5,20,80,250):
                probe_start=int(sr*(.25+gap_ms/1000));x=np.zeros(sr)
                x[:sr//4]=.6*np.sin(2*np.pi*440*np.arange(sr//4)/sr)
                probe=.045*np.sin(2*np.pi*440*np.arange(sr//10)/sr)
                x[probe_start:probe_start+len(probe)]=probe
                baseline=np.zeros(sr);baseline[probe_start:probe_start+len(probe)]=probe
                y=run(x,params={key:value});quiet=run(baseline,params={key:value})
                window=slice(probe_start+latency,probe_start+latency+960)
                results.append({key:value,'gap_ms':gap_ms,
                                'probe_gain_vs_rest_db':float(20*np.log10(calculate_rms(y[window])/calculate_rms(quiet[window]))),
                                'matched_probe_difference':float(calculate_rms(level_match(y[window],quiet[window])-quiet[window])/calculate_rms(quiet[window]))})
    curve=[]
    for db in (-60,-48,-36,-30,-24,-18,-12,-6,0):
        x=10**(db/20)*np.sin(2*np.pi*440*t);y=run(x)
        curve.append(dict(input_peak_dbfs=db,**metrics(y[-sr:],sr,440)))
    # Old/new matching is for comparison files only, not an asserted equivalence.
    reference=run(signals['harmonic_rich'],params={'drive_db':15,'mix':1},effect='distortion')
    candidate=outputs['harmonic_rich',default_mode];matched=level_match(candidate[latency:],reference[latency:])
    wavfile.write(out/'old_distortion.wav',sr,reference[latency:].astype(np.float32))
    wavfile.write(out/'custom_level_matched.wav',sr,matched.astype(np.float32))
    comparison=dict(old=metrics(reference[latency:],sr),new=metrics(candidate[latency:],sr),matched=metrics(matched,sr))
    old_two=run(signals['two_tone'],params={'drive_db':15,'mix':1},effect='distortion')[-sr:]
    power=np.abs(np.fft.rfft(old_two-old_two.mean()))**2
    comparison['old_two_tone_imd_dbc']=float(10*np.log10(sum(power[7000+s*60*n] for n in range(1,6) for s in (-1,1))/power[7000]))
    listening=dynamic; listening_rate=sr
    di=None
    if args.di:
        rate,x=wavfile.read(args.di);x=x[:rate*60]
        if np.issubdtype(x.dtype,np.integer):
            info=np.iinfo(x.dtype);x=(x.astype(float)-(128 if info.min==0 else 0))/((info.max+1)/2 if info.min==0 else max(abs(info.min),abs(info.max)))
        if len(x)<rate: raise ValueError('DI must contain at least one second')
        listening=x;listening_rate=rate
        x=x[:,0] if x.ndim==2 else x
        di=[];baseline_di=None
        for amplitude in (1,.7,.5,.3):
            y=run(x*amplitude,rate=rate)
            if baseline_di is None: baseline_di=y.copy()
            _,base_db=calculate_fft(baseline_di,rate);_,current_db=calculate_fft(level_match(y,baseline_di),rate);mask=base_db>base_db.max()-80
            di.append(dict(input_fraction=amplitude,matched_spectrum_difference_db=float(np.mean(np.abs(current_db[mask]-base_db[mask]))),**metrics(y,rate)))
    listening_manifest=write_listening_sweeps(out,listening,listening_rate,run,args.steps,contrasts=True)
    independence=[]
    independence_audio={}
    for name,params in [('soft_coarse',{'crunch':0,'grain':100}),('hard_smooth',{'crunch':100,'grain':0}),
                        ('drive70_headroom20',{'drive':70,'headroom':20}),('drive70_headroom80',{'drive':70,'headroom':80}),
                        ('drive70_headroom0',{'drive':70,'headroom':0}),('drive70_headroom100',{'drive':70,'headroom':100}),
                        ('extreme',{'drive':100,'tone':100,'crunch':100,'headroom':0,'grain':100})]:
        y=run(signals['sine440'],params=params)
        independence_audio[name]=y[-sr:]
        two=run(signals['two_tone'],params=params)[-sr:];power=np.abs(np.fft.rfft(two-two.mean()))**2
        pick=run(signals['picking'],params=params)
        row=dict(name=name,params=params,**metrics(y[-sr:],sr,440),picking=metrics(pick,sr),
                 imd_sidebands_dbc=float(10*np.log10(max(sum(power[7000+s*60*n] for n in range(1,6) for s in (-1,1)),1e-30)/max(power[7000],1e-30))))
        row['alias_by_factor']={}
        for mode in range(3):
            hf=run(signals['sine7013'],mode,params)[-sr:]
            row['alias_by_factor'][str(1<<mode)]=residual_dbc(hf,sr,7013)
        row['picking']['initial_10ms_rms']=float(calculate_rms(pick[latency:latency+480]))
        row['picking']['late_10ms_rms']=float(calculate_rms(pick[latency+4800:latency+5280]))
        row['level_curve']=[]
        for db in (-48,-36,-24,-18,-12,-6,0):
            probe=run(10**(db/20)*np.sin(2*np.pi*440*t),params=params)[-sr:]
            row['level_curve'].append(dict(input_peak_dbfs=db,**metrics(probe,sr,440)))
        independence.append(row)
    macro_dynamics=[]
    for key in ('crunch','headroom','grain'):
        pair=[]
        for value in (0,100):
            params={key:value}
            if key=='headroom':params['drive']=70
            y=run(dynamic,params=params);pair.append(y)
            bursts=[]
            latency=0 if args.mode==1 else 64
            for index,amplitude in enumerate((.06,.2,.6,.06,.2,.6)):
                begin=index*sr//2+latency
                early=y[begin:begin+480];late=y[begin+2400:begin+4800]
                burst=y[begin:min(begin+sr//2,len(y))]
                bursts.append(dict(input_peak=amplitude,**metrics(burst,sr),attack_peak=float(calculate_peak(early)),
                                   attack_rms=float(calculate_rms(early)),late_rms=float(calculate_rms(late))))
            macro_dynamics.append(dict(parameter=key,value=value,**metrics(y,sr),bursts=bursts))
        dynamic_matched=level_match(pair[1],pair[0])
        macro_dynamics[-1]['matched_difference_ratio']=float(calculate_rms(dynamic_matched-pair[0])/calculate_rms(pair[0]))
        macro_dynamics[-1]['endpoint_rms_difference_db']=float(20*np.log10(calculate_rms(pair[1])/calculate_rms(pair[0])))
    with (out/'sweeps.csv').open('w',newline='') as handle:
        fields=['parameter','value','rms','peak','dc','thd','imd_sidebands_dbc','alias_residual_dbc','harmonics_dbc','level_curve']
        writer=csv.DictWriter(handle,fieldnames=fields,extrasaction='ignore');writer.writeheader()
        for row in sweeps:
            writer.writerow({**row,'harmonics_dbc':json.dumps(row['harmonics_dbc']),'level_curve':json.dumps(row['level_curve'])})
    data=dict(grain_recovery=grain_recovery,macro_dynamics=macro_dynamics,macro_factor=args.mode,listening_manifest=listening_manifest,independence=independence,sample_rate=sr,signals=records,high_drive_alias=stress,sweeps=sweeps,cleanup=cleanup,dynamics=dynamics,recovery=recovery,level_curve=curve,level_matching=comparison,di=di)
    (out/'metrics.json').write_text(json.dumps(data,indent=2));np.savez(out/'spectra.npz',frequency=f,**spectra)
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig, axes=plt.subplots(2,2,figsize=(11,7))
    for factor in (1,2,4):
        db=spectra[f'sine7013_{factor}x'];axes[0,0].plot(f/1000,db-db.max(),label=f'{factor}x',linewidth=.8)
    axes[0,0].set(xlabel='Frequency (kHz)',ylabel='Relative spectrum (dB)',ylim=(-140,5),title='7013 Hz / Drive 50');axes[0,0].legend()
    for parameter in ('crunch','grain'):
        rows=[r for r in sweeps if r['parameter']==parameter]
        axes[0,1].plot([r['value'] for r in rows],[max(-100,r['harmonics_dbc']['2']) for r in rows],'o-',label=parameter)
    axes[0,1].set(xlabel='Macro value',ylabel='Second harmonic (dBc; floor -100)',title='Different macro roles / 440 Hz');axes[0,1].legend()
    axes[1,0].plot([r['input_fraction'] for r in cleanup],[100*r['thd'] for r in cleanup],'o-')
    axes[1,0].set(xlabel='Input amplitude fraction',ylabel='THD (%)',title=f'Synthetic volume clean-up / {args.mode}x')
    axes[1,1].plot([r['input_peak_dbfs'] for r in curve],[20*np.log10(r['rms']) for r in curve],'o-')
    axes[1,1].set(xlabel='Input peak (dBFS)',ylabel='Output RMS (dBFS)',title=f'Level curve / 440 Hz / {args.mode}x')
    for ax in axes.flat:ax.grid(alpha=.2)
    fig.suptitle('Custom Drive provisional measurements — not a listening quality score')
    fig.tight_layout();fig.savefig(out/'quality.png',dpi=150);plt.close(fig)
    profile='Skipped during tuning' if args.skip_profile else subprocess.check_output([str(executable),'profile'],text=True);(out/'profile.csv').write_text(profile)
    lines=['# Custom Drive provisional measurement','', '48 kHz; synthetic inputs, no guitar DI unless explicitly supplied. Default Drive/Tone/Crunch/Headroom=50, Grain=25, Level=-6 dB, Mix=100%.',
        'THD includes intentional distortion (up to 32 in-band harmonics). IMD: 60 Hz + 7 kHz, 4:1 input amplitude; ten +/-60..300 Hz sidebands relative to 7 kHz carrier. Not a complete IMD specification.',
        '7013 Hz residual excludes legitimate in-band harmonic bins +/-1 Hz and DC; signal-specific alias proxy, not a universal sound quality metric. No post-analysis normalization in these metrics.', '',
        '| Factor | 440 Hz THD % | IMD dBc | Alias dBc | 440 Hz DC |','|---|---:|---:|---:|---:|']
    for factor in (1,2,4):
        get=lambda name: next(r for r in records if r['signal']==name and r['factor']==factor)
        lines.append(f"| {factor} | {get('sine440')['thd']*100:.3f} | {get('two_tone')['imd_sidebands_dbc']:.2f} | {get('sine7013')['alias_residual_dbc']:.2f} | {get('sine440')['dc']:.3g} |")
    lines+=['', 'Drive 100 alias residual (dBc), 1x/2x/4x: '+', '.join(f"{r['alias_residual_dbc']:.2f}" for r in stress)]
    lines+=['',f'## Volume clean-up ({args.mode}x)','| Input fraction | THD % | RMS |','|---|---:|---:|']
    for r in cleanup:lines.append(f"| {r['input_fraction']} | {r['thd']*100:.3f} | {r['rms']:.5f} |")
    lines+=['','## Stereo 48k/128 profile','```csv',profile.strip(),'```','', 'metrics.json contains all macro sweeps, harmonic spectra, level curve and dynamic probes. spectra.npz reuses the existing Hann FFT analyzer. Old/new WAVs are RMS matched offline; they are synthetic, not guitar captures. Maximum times are observed scheduling-dependent values, not hardware guarantees.']
    lines+=['',f'## Macro sweeps ({args.mode}x)','| Macro | Value | RMS | Peak | DC | THD % | IMD dBc | Alias dBc |','|---|---:|---:|---:|---:|---:|---:|---:|']
    for r in sweeps:
        lines.append(f"| {r['parameter']} | {r['value']:g} | {r['rms']:.5f} | {r['peak']:.5f} | {r['dc']:.3g} | {r['thd']*100:.2f} | {r['imd_sidebands_dbc']:.2f} | {r['alias_residual_dbc']:.2f} |")
    lines+=['','## Macro independence / extreme corner', '| Configuration | THD % | H2 dBc | H3 dBc | Picking crest dB | IMD dBc |','|---|---:|---:|---:|---:|---:|']
    for r in independence:
        lines.append(f"| {r['name']} | {r['thd']*100:.2f} | {r['harmonics_dbc']['2']:.2f} | {r['harmonics_dbc']['3']:.2f} | {r['picking']['crest_db']:.2f} | {r['imd_sidebands_dbc']:.2f} |")
    if args.baseline:
        before=json.loads(args.baseline.read_text())
        lines+=['','## Before / after Drive mapping','| Drive | Previous RMS | Current RMS | Previous THD % | Current THD % |','|---|---:|---:|---:|---:|']
        for row in sweeps:
            previous=next((r for r in before['sweeps'] if r['parameter']==row['parameter'] and r['value']==row['value']),None)
            if previous and row['parameter']=='drive':
                lines.append(f"| {row['value']:g} | {previous['rms']:.5f} | {row['rms']:.5f} | {previous['thd']*100:.2f} | {row['thd']*100:.2f} |")
    lines+=['','## Transient endpoints', '| Macro | Matched waveform delta / reference RMS | Raw RMS difference dB |', '|---|---:|---:|']
    for row in macro_dynamics:
        if row['value']==100:lines.append(f"| {row['parameter']} | {row['matched_difference_ratio']:.4f} | {row['endpoint_rms_difference_db']:.2f} |")
    lines+=['','## Grain harmonic distribution', '| Grain | RMS | THD % | H2 | H3 | H5 | H7 | even/odd dB (excluding fundamental) |', '|---|---:|---:|---:|---:|---:|---:|---:|']
    for row in sweeps:
        if row['parameter']=='grain':lines.append(f"| {row['value']:g} | {row['rms']:.5f} | {row['thd']*100:.3f} | "+' | '.join(f"{row['harmonics_dbc'][k]:.2f}" for k in ('2','3','5','7'))+f" | {row['even_odd_db']:.2f} |")
    if args.check_texture:
        lines+=['','Headroom comparison intentionally uses internal h=1-public h, as explicitly selected by the user. The previous physical HIGH=cleaner/greater-crest direction is therefore reversed; this is a listening candidate, not a claim that both contradictory criteria passed.']
    lines+=['',f'Listening render factor: {args.mode}x. Synthetic input alternates weak/medium/strong transients; no listening result is inferred.', 'Listening: raw/ and level_matched/ contain float WAVs. All matched files use the same default render RMS. Peaks may exceed 0 dBFS; lower playback volume. No limiter is applied. Synthetic picking is used unless --di is supplied (maximum 60 seconds, channels preserved).', 'Each sweep includes harmonic bins and amplitude response in sweeps.csv / metrics.json. Extreme-corner IMD and residuals are diagnostics, not proof of perceptual quality.']
    report='\n'.join(lines)+'\n';(out/'report.md').write_text(report);print(report)
    alias={r['factor']:r['alias_residual_dbc'] for r in records if r['signal']=='sine7013'}
    assert alias[2]<alias[1]-6 and alias[4]<alias[1]-6, alias
    assert stress[1]['alias_residual_dbc']<stress[0]['alias_residual_dbc']-6 and stress[2]['alias_residual_dbc']<stress[0]['alias_residual_dbc']-6, stress
    assert cleanup[-1]['thd'] < cleanup[0]['thd']*.25
    assert all(b['rms']>=a['rms']*.99 for a,b in zip(curve,curve[1:])), curve
    soft=independence_audio['soft_coarse'];hard=level_match(independence_audio['hard_smooth'],soft)
    assert calculate_rms(soft-hard)>calculate_rms(soft)*.02, 'Crunch/Grain profiles collapsed'
    if args.baseline:
        old_drive=[r for r in before['sweeps'] if r['parameter']=='drive']
        new_drive=[r for r in sweeps if r['parameter']=='drive']
        if {r['value'] for r in new_drive} >= {0,100}:
            old_max=next(r for r in old_drive if r['value']==100)
            new_max=next(r for r in new_drive if r['value']==100)
            assert new_max['thd']>old_max['thd']*1.1, 'Maximum drive range did not expand'
            span=lambda rows:20*np.log10(max(r['rms'] for r in rows)/min(r['rms'] for r in rows))
            assert span(new_drive)<span(old_drive)-3, 'Drive level span did not improve'
    assert abs(calculate_rms(matched)-calculate_rms(reference[latency:]))<1e-10
    if args.check_texture:
        grain_rows={r['value']:r for r in sweeps if r['parameter']=='grain'}
        a,b=grain_rows[0],grain_rows[100]
        assert abs(20*np.log10(b['rms']/a['rms']))<1, 'Grain steady gain change'
        assert max(a['thd'],b['thd'])/min(a['thd'],b['thd'])<1.2, 'Grain became a distortion amount control'
        assert b['harmonics_dbc']['2']>a['harmonics_dbc']['2']+15, 'Grain even harmonic distribution'
        assert abs(b['harmonics_dbc']['5']-a['harmonics_dbc']['5'])>3, 'Grain odd harmonic distribution'
        public={r['value']:r for r in macro_dynamics if r['parameter']=='headroom'}
        assert abs(public[100]['endpoint_rms_difference_db'])<3, 'Headroom level difference'
        assert public[0]['crest_db']>public[100]['crest_db']+3, 'Explicit Headroom inversion not reflected in dynamics'
        print('Grain distribution/RMS/THD and reversed Headroom comparison guards PASS; listening pending')
    if args.check_macros:
        high={r['parameter']:r for r in macro_dynamics if r['value']==100}
        for key,minimum in (('crunch',.2),('grain',.2),('headroom',.3)):
            assert high[key]['matched_difference_ratio']>minimum, (key,high[key])
        assert abs(high['headroom']['endpoint_rms_difference_db'])<3, high['headroom']
        low=next(r for r in macro_dynamics if r['parameter']=='headroom' and r['value']==0)
        peak_ratio=lambda row:row['bursts'][2]['attack_peak']/row['bursts'][0]['attack_peak']
        assert peak_ratio(high['headroom'])>1.5*peak_ratio(low), 'Headroom collapsed to a static gain'
        assert high['headroom']['crest_db']>low['crest_db']+3, 'Headroom crest direction'
        onset={r['name']:r for r in independence}
        low_curve={r['input_peak_dbfs']:r['thd'] for r in onset['drive70_headroom0']['level_curve']}
        high_curve={r['input_peak_dbfs']:r['thd'] for r in onset['drive70_headroom100']['level_curve']}
        assert high_curve[-24]<low_curve[-24]*.5, 'High Headroom weak input is not cleaner'
        assert high_curve[-6]>high_curve[-24]*2, 'High Headroom does not distinguish weak/strong input'
        print('Macro matched-waveform, Headroom level/dynamic guards PASS (listening still required)')


if __name__=='__main__':main()
