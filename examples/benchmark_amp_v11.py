"""Focused Amp DI level response; run after building nonlinear_benchmark.
Uses the existing Logic DI extract, without operating Logic or changing the AU.
"""
from pathlib import Path
import json, subprocess
import numpy as np
from scipy.io.wavfile import read, write
root=Path(__file__).resolve().parents[1]
out=root/'cpp/build/amp-v11';out.mkdir(exist_ok=True)
sr,x=read(root/'cpp/build/headroom_logic_analysis/input_24_27.wav')
if x.dtype.kind=='i': x=x.astype(float)/max(abs(np.iinfo(x.dtype).min),np.iinfo(x.dtype).max)
if x.ndim==2:x=x[:,0]
x=x.astype(float);x=x/max(np.max(abs(x)),1e-20)
rows=[]
for peak in [.02,.4]:
 inp=x*peak;inp.astype('float32').tofile(out/'input.f32')
 for mode in range(3):
  subprocess.run([str(root/'cpp/build/nonlinear_benchmark'),'amp_sim',str(mode),str(sr),str(out/'input.f32'),str(out/'output.f32'),'12'],check=True)
  y=np.fromfile(out/'output.f32',dtype='float32').astype(float)
  latency=0 if mode==0 else 128
  aligned=y[latency:];source=inp[:len(aligned)]
  rms=lambda v:float(np.sqrt(np.mean(v*v)))
  window=max(1,int(sr*.01));env=np.sqrt(np.convolve(source*source,np.ones(window)/window,'valid'));idx=int(np.argmax(env))
  attack=slice(idx,min(idx+int(sr*.02),len(source)));sustain=slice(min(idx+int(sr*.1),len(source)-1),min(idx+int(sr*.3),len(source)))
  rows.append(dict(input_peak=peak,factor=1<<mode,input_rms=rms(source),output_rms=rms(aligned),output_peak=float(max(abs(aligned))),dc=float(aligned.mean()),peak_region_gain_db=float(20*np.log10(rms(aligned[attack])/rms(source[attack]))),later_region_gain_db=float(20*np.log10(max(rms(aligned[sustain]),1e-20)/max(rms(source[sustain]),1e-20)))))
  write(out/f'di_{peak}_{1<<mode}x_raw.wav',sr,y.astype('float32'))
  write(out/f'di_{peak}_{1<<mode}x_matched.wav',sr,(y*rms(inp)/max(rms(y),1e-20)).astype('float32'))
 write(out/f'di_{peak}_input.wav',sr,inp.astype('float32'))
(out/'di_metrics.json').write_text(json.dumps(rows,indent=2));print(json.dumps(rows,indent=2))
