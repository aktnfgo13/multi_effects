"""Focused Grain v3 listening smoke; reuses the existing C++ runner and analyzers."""
import argparse
import json
from pathlib import Path
import subprocess
import numpy as np
from scipy.io import wavfile
from benchmark_custom_drive import ROOT, metrics, residual_dbc, level_match, calculate_rms


def measure_interaction(run,render,reference,passages,sine,sr,rate,out,conduction,exe,previous,headroom_focus=False):
    """Steady sine and isolated picks share the same settings as the listening files."""
    def db(ratio):
        return float(20*np.log10(max(1e-15,ratio)))

    cases={(c,h,g) for c in (0,50,100) for h in (0,50,100) for g in (25,50)}
    cases.update((50,h,50) for h in (25,75))
    cases.update((c,50,g) for c in (0,50,100) for g in (0,25,50,75,100))
    cases.update((50,h,g) for h in (0,100) for g in (0,100))
    results={}; audio={}; matched={}
    for directory in ('raw','level_matched'):
        (out/directory).mkdir(exist_ok=True)
    for c,h,g in sorted(cases):
        key=f'c{c:03}_h{h:03}_g{g:03}'
        params=dict(crunch=c,headroom=h,grain=g)
        steady=metrics(run(sine,params)[-sr:],sr,440)
        strong=run(passages[4],params);weak=run(passages[5],params)
        y=render(params)
        results[key]=dict(rms=steady['rms'],peak=steady['peak'],thd=steady['thd'],
                          picking=metrics(y,rate),strong_weak_db=db(calculate_rms(strong)/calculate_rms(weak)),
                          transient_peak=float(np.max(np.abs(strong[:sr//50]))))
        attack=strong[:int(.03*sr)]
        body=strong[int(.03*sr):int(.15*sr)]
        seq=np.concatenate([passages[4][:int(.12*sr)],np.zeros(int(.02*sr)),passages[5]])
        rest=np.concatenate([np.zeros(int(.12*sr)),np.zeros(int(.02*sr)),passages[5]])
        start=len(seq)-len(weak)
        window=slice(start+64,start+64+int(.03*sr))
        results[key].update(attack=metrics(attack,sr),body=metrics(body,sr),
                            sustain=metrics(strong[int(.15*sr):],sr),
                            recovery_db=db(calculate_rms(run(seq,params)[window])/calculate_rms(run(rest,params)[window])))
        if g==50:
            pair=run(seq,params);alone=run(rest,params)
            recovery=[]
            for first,last in ((0,10),(10,30),(30,60),(60,100)):
                w=slice(start+64+sr*first//1000,start+64+sr*last//1000)
                recovery.append(db(calculate_rms(pair[w])/calculate_rms(alone[w])))
            results[key].update(weak_alone_rms=calculate_rms(alone[window]),
                                weak_sequence_rms=calculate_rms(pair[window]),recovery_windows_db=recovery)
        if c==h==50:
            t=np.arange(len(sine))/sr
            two=.32*np.sin(2*np.pi*60*t)+.08*np.sin(2*np.pi*7000*t)
            power=np.abs(np.fft.rfft(run(two,params)[-sr:]))**2
            hf=.3*np.sin(2*np.pi*7013*t)
            results[key].update(steady=steady,imd_dbc=float(10*np.log10(sum(power[7000+s*60*n] for s in (-1,1) for n in range(1,6))/power[7000])),
                                alias={1<<m:residual_dbc(run(hf,params,m)[-sr:],sr,7013) for m in (1,2)})
            if previous:
                results[key]['previous_alias']={1<<m:residual_dbc(run(hf,params,m,executable=previous)[-sr:],sr,7013) for m in (1,2)}
        audio[c,h,g]=y
        matched[key]=level_match(y,reference)
        wavfile.write(out/'raw'/f'{key}.wav',rate,y.astype(np.float32))
    trim=min(1.,.9/max(float(np.max(np.abs(y))) for y in matched.values()))
    for key,y in matched.items():
        wavfile.write(out/'level_matched'/f'{key}.wav',rate,(y*trim).astype(np.float32))
    def delta(a,b):
        return float(calculate_rms(level_match(b,a)-a)/calculate_rms(a))
    intervals={}
    for c in (0,50,100):
        intervals[c]=[delta(audio[c,50,g],audio[c,50,g+25]) for g in (0,25,50,75)]
    corners={h:delta(audio[50,h,0],audio[50,h,100]) for h in (0,100)}
    old={}
    if previous:
        for c in (0,50,100):
            for h in (0,100):
                params=dict(crunch=c,headroom=h,grain=25)
                old[f'{c}/{h}']=metrics(run(sine,params,executable=previous)[-sr:],sr)
        for c in (0,50,100):
            ys=[render(dict(crunch=c,grain=g),previous) for g in (0,25,50,75,100)]
            old[f'intervals{c}']=[delta(a,b) for a,b in zip(ys,ys[1:])]
            old[f'grain_span{c}']=delta(ys[0],ys[-1])
            prior=[]
            for h in (0,100):
                params=dict(crunch=c,headroom=h,grain=50)
                a,b=[run(passages[i],params,executable=previous) for i in (4,5)]
                prior.append(db(calculate_rms(a)/calculate_rms(b)))
            old[f'headroom_dynamic_span{c}']=prior[1]-prior[0]
    if previous and headroom_focus:
        errors=[np.max(np.abs(audio[c,50,g]-render(dict(crunch=c,headroom=50,grain=g),previous)))
                for c in (0,50,100) for g in (0,25,50,75,100)]
        old['neutral_max_error']=float(max(errors))
        assert max(errors)==0,'Headroom midpoint changed the frozen macros'
    span={c:delta(audio[c,50,0],audio[c,50,100]) for c in (0,50,100)}
    dominant=[conduction[g]['current_share'][1] for g in (0,25,50,75,100)]
    assert all(a<b for a,b in zip(dominant,dominant[1:]))
    report=dict(grain_span=span,matrix=results,conduction=conduction,grain_intervals=intervals,
                grain_at_headroom_extremes=corners,previous=old)
    for c in (0,50,100):
        low,mid,high=[results[f'c{c:03}_h{h:03}_g025'] for h in (0,50,100)]
        for a,b in ((low,mid),(mid,high)):
            assert b['transient_peak']>a['transient_peak']
            assert b['strong_weak_db']>a['strong_weak_db']
        assert abs(db(high['rms']/low['rms']))<1.
        assert high['picking']['crest_db']>low['picking']['crest_db']+2.
        assert min(intervals[c])>.035
    if headroom_focus:
        for c in (0,50,100):
            low,high=[results[f'c{c:03}_h{h:03}_g050'] for h in (0,100)]
            assert abs(db(high['rms']/low['rms']))<=.5
            assert abs(high['recovery_db'])<1 and abs(high['recovery_windows_db'][-1])<.2
            assert high['body']['crest_db']>low['body']['crest_db']+3
            if previous:
                assert high['strong_weak_db']-low['strong_weak_db']>old[f'headroom_dynamic_span{c}']+2
    assert min(corners.values())>.08
    (out/'interaction.json').write_text(json.dumps(report,indent=2))

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,default=ROOT/'cpp/build/grain-v3')
    parser.add_argument('--headroom-mapping',action='store_true',help='Five-point onset, dynamics and listening comparison')
    parser.add_argument('--interaction',action='store_true',help='Measure Crunch/Headroom/Grain matrices')
    parser.add_argument('--headroom-focus',action='store_true',help='Render recovery sequence and verify neutral against v1')
    parser.add_argument('--di',type=Path)
    parser.add_argument('--profile',action='store_true')
    parser.add_argument('--reference',type=Path)
    args=parser.parse_args();out=args.output;out.mkdir(parents=True,exist_ok=True)
    exe=ROOT/'cpp/build/custom_drive_benchmark';sr=48000;t=np.arange(sr*2)/sr
    def run(x,params,mode=1,rate=sr,executable=exe):
        np.asarray(x,np.float32).tofile(out/'input.f32')
        subprocess.run([str(executable),'custom_drive',str(mode),str(rate),str(out/'input.f32'),str(out/'output.f32')]+[f'{k}={v}' for k,v in params.items()],check=True)
        y=np.fromfile(out/'output.f32',np.float32).astype(float)
        if not np.isfinite(y).all():raise ValueError('Non-finite render')
        return y
    sine=.3*np.sin(2*np.pi*440*t);hf=.3*np.sin(2*np.pi*7013*t)
    two=.32*np.sin(2*np.pi*60*t)+.08*np.sin(2*np.pi*7000*t)
    local=np.arange(sr//2)/sr
    carrier=sum(np.sin(2*np.pi*110*k*local)/k for k in range(1,12));carrier/=np.max(np.abs(carrier))
    pluck=carrier*(1-np.exp(-local/.0004))*np.exp(-local/.09)
    rate=sr
    chord=sum(np.sin(2*np.pi*f*k*local)/k for f in (82.41,123.47,164.81) for k in range(1,7))
    chord/=np.max(np.abs(chord))
    attack=1-np.exp(-local/.0005)
    open_strings=sum(np.sin(2*np.pi*f*local) for f in (82.41,123.47,164.81,207.65,246.94,329.63))
    palm=.7*chord*attack*np.exp(-local/.028)
    passages=[palm,palm,palm,.6*chord*attack*np.exp(-local/.22),.6*pluck,.06*pluck,
              .11*open_strings*attack*np.exp(-local/.2),np.zeros(sr//4)]
    if args.headroom_focus or args.headroom_mapping:
        sequence=np.concatenate([passages[4][:int(.12*sr)],np.zeros(int(.02*sr)),passages[5]])
        listening=np.concatenate([passages[4],passages[5],sequence,*passages[:4],*passages[6:]])
    else:
        listening=np.concatenate(passages)
    # Measure path current at a reproducible normalized cell input, outside the callback.
    probe_path=out/'conduction-probe.f32'
    (4*listening).astype(np.float32).tofile(probe_path)
    conduction={}
    for grain in ((0,25,50,75,100) if args.interaction else (0,50,100)):
        result=subprocess.check_output([str(exe),'conduction',str(grain),str(probe_path)],text=True)
        currents=np.array([line.split() for line in result.splitlines()],float)
        conduction[grain]=dict(active_fraction=currents[:,0].tolist(),
                               current_share=(currents[:,1]/currents[:,1].sum()).tolist())
    if args.di:
        rate,listening=wavfile.read(args.di);listening=listening[:rate*60]
        if np.issubdtype(listening.dtype,np.integer):
            info=np.iinfo(listening.dtype)
            listening=(listening.astype(float)-(128 if info.min==0 else 0))/(128 if info.min==0 else max(abs(info.min),abs(info.max)))
        if len(listening)<rate or not np.isfinite(listening).all():raise ValueError('DI must be finite and at least one second')
    def render(params,executable=exe):
        if listening.ndim==1:return run(listening,params,rate=rate,executable=executable)
        return np.column_stack([run(x,params,rate=rate,executable=executable) for x in listening.T])
    if args.headroom_mapping:
        from benchmark_headroom_mapping import measure_mapping
        measure_mapping(run,render,passages,out,args.reference,sr,rate)
        return
    reference=render({'grain':25})
    if args.interaction:
        measure_interaction(run,render,reference,passages,sine,sr,rate,out,conduction,exe,args.reference,args.headroom_focus)
        return
    cases=[('grain_000',{'grain':0}),('grain_050',{'grain':50}),('grain_100',{'grain':100}),
           ('crunch000_grain100',{'crunch':0,'grain':100}),('crunch100_grain000',{'crunch':100,'grain':0}),
           ('crunch000_grain000',{'crunch':0,'grain':0}),('crunch100_grain100',{'crunch':100,'grain':100})]
    rows=[];audio={};matched_audio={}
    for directory in ('raw','level_matched'):(out/directory).mkdir(exist_ok=True)
    for name,params in cases:
        y=run(sine,params)[-sr:];row=dict(name=name,parameters=params,**metrics(y,sr,440))
        imd=run(two,params)[-sr:];power=np.abs(np.fft.rfft(imd-imd.mean()))**2
        row['imd_dbc']=float(10*np.log10(max(sum(power[7000+s*60*n] for n in range(1,6) for s in (-1,1)),1e-30)/max(power[7000],1e-30)))
        # Coherent E/B-like fifth. Count selected sum/difference products
        # separately from either note's harmonic series.
        f1,f2=83,125
        fifth=.22*(np.sin(2*np.pi*f1*t)+np.sin(2*np.pi*f2*t))
        spectrum=np.abs(np.fft.rfft(run(fifth,params)[-sr:]))**2
        products=[f2-f1,f1+f2,2*f1-f2,2*f2-f1,2*f1+f2,2*f2+f1]
        row['fifth_imd_dbc']=float(10*np.log10(spectrum[products].sum()/(spectrum[f1]+spectrum[f2])))
        row['alias_dbc']={str(1<<mode):residual_dbc(run(hf,params,mode)[-sr:],sr,7013) for mode in range(3)}
        raw=render(params);matched=level_match(raw,reference);audio[name]=raw
        row['listening']=metrics(raw,rate);row['matched_peak']=float(np.max(np.abs(matched)))
        wavfile.write(out/'raw'/f'{name}.wav',rate,raw.astype(np.float32))
        matched_audio[name]=matched;rows.append(row)
    # One shared offline headroom trim, not independent peak normalization or a
    # limiter: retain the common RMS and avoid playback clipping in the A/B files.
    common_gain=min(1.,.9/max(max(float(np.max(np.abs(x))),1e-12) for x in matched_audio.values()))
    for row in rows:
        matched=matched_audio[row['name']]*common_gain
        row['matched_peak']=float(np.max(np.abs(matched)))
        wavfile.write(out/'level_matched'/f"{row['name']}.wav",rate,matched.astype(np.float32))
    recovery=[]
    for grain in (0,50,100):
        for gap in (5,20,80):
            start=sr//4+int(sr*gap/1000);x=np.zeros(sr);x[:sr//4]=.6*np.sin(2*np.pi*440*np.arange(sr//4)/sr)
            probe=.045*np.sin(2*np.pi*440*np.arange(sr//10)/sr);quiet=np.zeros(sr)
            x[start:start+len(probe)]=probe;quiet[start:start+len(probe)]=probe
            a=run(x,{'grain':grain});b=run(quiet,{'grain':grain});window=slice(start+64,start+64+960)
            recovery.append(dict(grain=grain,gap_ms=gap,gain_db=float(20*np.log10(calculate_rms(a[window])/calculate_rms(b[window]))),
                                 matched_residual=float(calculate_rms(level_match(a[window],b[window])-b[window])/calculate_rms(b[window]))))
    freeze=None;previous_delta=None
    if args.reference:
        ramp=((np.arange(12000)%251-125)/512).astype(np.float32);worst=0;count=0
        for mode in range(3):
            for crunch in (0,25,50,75,100):
                for headroom in (0,25,50,75,100):
                    params=dict(crunch=crunch,headroom=headroom,grain=25)
                    a=run(ramp,params,mode);b=run(ramp,params,mode,executable=args.reference)
                    worst=max(worst,float(np.max(np.abs(a-b))));count+=1
        for mode in range(3):
            for key,values in [('drive',(0,100)),('tone',(0,100)),('level_db',(-24,12)),('mix',(0,.5))]:
                for value in values:
                    params={'grain':25,key:value}
                    a=run(ramp,params,mode);b=run(ramp,params,mode,executable=args.reference)
                    worst=max(worst,float(np.max(np.abs(a-b))));count+=1
        assert worst<2e-6, ('Frozen controls changed',worst)
        freeze=dict(cases=count,max_sample_error=worst)
        old_a=render({'grain':0},args.reference);old_b=render({'grain':100},args.reference)
        previous_delta=float(calculate_rms(level_match(old_b,old_a)-old_a)/calculate_rms(old_a))
    a=audio['grain_000'];b=audio['grain_100']
    delta=float(calculate_rms(level_match(b,a)-a)/calculate_rms(a))
    onset={}
    for grain in (0,50,100):
        onset[str(grain)]=[]
        for amplitude in (.02,.05,.1,.2,.4):
            probe=run(sine*(amplitude/.3),{'grain':grain})[-sr:]
            result=metrics(probe,sr,440)
            onset[str(grain)].append(dict(input_peak=amplitude,thd=result['thd'],rms=result['rms']))
    passage_delta={}
    if not args.di:
        for name,start,end in [('palm_mute',0,3),('power_chord',3,4),('strong_note',4,5),
                               ('weak_note',5,6),('open_chord',6,7)]:
            section=slice(start*(sr//2),end*(sr//2))
            fine,coarse=a[section],b[section]
            passage_delta[name]=float(calculate_rms(level_match(coarse,fine)-fine)/calculate_rms(fine))
    data=dict(passage_delta=passage_delta,conduction=conduction,onset=onset,sample_rate=sr,listening_rate=rate,input='DI' if args.di else 'synthetic transients',rows=rows,recovery=recovery,
              matched_common_gain_db=float(20*np.log10(common_gain)),matched_endpoint_delta=delta,previous_matched_endpoint_delta=previous_delta,freeze=freeze)
    (out/'metrics.json').write_text(json.dumps(data,indent=2))
    lines=['# Grain v3 focused measurements','', '2x macro measurements; signal-specific IMD/alias proxies, not an audibility score.',
           '| Setting | RMS | Peak | THD % | H2 | H3 | H5 | H7 | IMD dBc | Alias 2x | DC |', '|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|']
    for row in rows:
        harmonics=' | '.join(f"{row['harmonics_dbc'][k]:.2f}" for k in ('2','3','5','7'))
        lines.append(f"| {row['name']} | {row['rms']:.5f} | {row['peak']:.5f} | {row['thd']*100:.2f} | {harmonics} | {row['imd_dbc']:.2f} | {row['alias_dbc']['2']:.2f} | {row['dc']:.3g} |")
    lines+=['',f'Level-matched transient endpoint waveform delta/reference RMS: {delta:.5f}',f'Previous matched delta: {previous_delta}',f'Frozen controls: {freeze}',
            f'Raw WAVs are actual unclamped DSP output. Matched WAVs share one Grain25 RMS reference and a common {20*np.log10(common_gain):.2f} dB offline headroom trim (all files, no limiter). Raw peaks may exceed 0 dBFS.',
            'Recovery metrics and all 1x/2x/4x alias proxies are in metrics.json. No subjective approval inferred.']
    if args.profile:
        profile=subprocess.check_output([str(exe),'profile','100'],text=True);(out/'profile.csv').write_text(profile)
        lines+=['','48k/128 stereo performance smoke at Grain100 (mean/max observed, not a worst-case guarantee):','```csv',profile.strip(),'```']
    # Numeric guardrails, not a perceptual approval. The branch must redistribute
    # texture without replacing the approved hard/soft clipping axis.
    assert max(r['thd'] for r in rows[:3])/min(r['thd'] for r in rows[:3])<1.5, 'Steady distortion amount grew excessively'
    assert rows[3]['harmonics_dbc']['3']<rows[4]['harmonics_dbc']['3']-6, 'Crossed soft/hard axis collapsed'
    # v3 uses symmetric shunts; check measured conduction instead of v2's
    # deliberate even-harmonic boost.
    assert min(conduction[0]['current_share'])>.01
    assert conduction[100]['current_share'][1]>.99
    for soft,hard in ((rows[5],rows[4]),(rows[3],rows[6])):
        assert hard['harmonics_dbc']['3']>soft['harmonics_dbc']['3']+6, 'Crunch separation'
    for fine,coarse in ((rows[5],rows[3]),(rows[4],rows[6]),(rows[0],rows[2])):
        assert abs(20*np.log10(coarse['rms']/fine['rms']))<1., 'Grain level shift'
        assert .75<coarse['thd']/fine['thd']<1.33, 'Grain distortion amount'
    if not args.di:assert delta>.15, 'Synthetic matched texture change is too small'
    lines+=['','Focused texture/cross-axis guards PASS; actual guitar approval pending.']
    report='\n'.join(lines)+'\n';(out/'report.md').write_text(report);print(report)

if __name__=='__main__':main()
