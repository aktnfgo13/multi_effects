# Nonlinear measurement (48 kHz)

Drive/preamp 24 dB; steady-state coherent 7013 Hz, amplitude 0.5.
Residual excludes DC and legitimate in-band harmonic bins (±1 bin). No window on coherent residual FFT.
This is a signal-specific alias proxy, not universal sound quality. 1 kHz THD includes intended distortion.

| Effect | Factor | Alias residual dBc | RMS | Peak | THD |
|---|---:|---:|---:|---:|---:|
| distortion | 1 | -13.09 | 0.95870 | 1.00000 | 0.31625 |
| distortion | 2 | -24.64 | 0.92310 | 1.13115 | 0.24883 |
| distortion | 4 | -48.71 | 0.92161 | 1.12544 | 0.24883 |
| amp_sim | 1 | -13.95 | 0.26206 | 0.38222 | 0.00648 |
| amp_sim | 2 | -21.38 | 0.25503 | 0.35891 | 0.00343 |
| amp_sim | 4 | -32.81 | 0.25460 | 0.36189 | 0.00346 |

Profile: mean/max observed; excludes device/GUI overhead. 128 samples deadline = 2666.67 us.
```csv
distortion,1,1.06017,9.541,0.357787,0
distortion,2,23.4117,58.542,2.19532,64
distortion,4,45.3526,68.708,2.57655,64
amp_sim,1,5.78071,14.291,0.535912,0
amp_sim,2,54.2222,140.833,5.28124,128
amp_sim,4,100.616,134.958,5.06092,128
cabinet,1,5.66931,12.292,0.46095,128
cabinet,2,5.84705,14.875,0.557812,128
cabinet,4,5.74843,12.709,0.476587,128
cabinet_long,1,47.1754,66.166,2.48122,128
cabinet_long,2,47.4552,60.375,2.26406,128
cabinet_long,4,47.417,65.375,2.45156,128
chain,1,16.9212,31.042,1.16407,128
chain,2,87.1483,131.541,4.93279,320
chain,4,156.014,224.208,8.4078,320
```

metrics.json contains RMS/peak for sine, high tone, multitone, sweep and harmonic guitar signal; spectra.npz contains reusable FFT spectra.

Measured on local arm64 macOS Release, 2026-11-03. Cabinet rows labelled 2/4 do not
oversample Cabinet; they repeat the native-rate baseline. cabinet_long uses 32768 taps.
The current table is a quieter repeat after builds/sanitizers completed. A prior run
concurrent with builds recorded full-chain 4x max **8180.75 us (306.8% deadline)**;
that observation is preserved in `cpp/build/v03-benchmark-concurrent-build.log`.
Wall-clock scheduling outliers are not removed; neither run proves worst-case realtime
safety or embedded performance. External-interface sustained playing is still required.
