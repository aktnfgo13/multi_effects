# Custom Drive perceptual macro candidate 2

This candidate responds to the user's isolated-guitar listening result: Crunch and
Grain remained hard to distinguish; Headroom mostly sounded like volume. Previous
numeric differences and passing propagation tests did not establish perceptual success.
This implementation still needs **level-matched listening and real playing** before it
can meet all three success criteria. No claim of improved subjective quality is made.

## Why the old macro design could be weak

Previously both stages blended the same p=2/p=8 curve, and the final signal always
came from B. Crunch did not vary the serial-stage contribution; at strongly saturated
inputs both curves approach similar rails. Grain's bounded bias, 35% B gain mismatch,
and small rectified-memory addition changed harmonics but did not change A/B weighting.
Bias subtraction and the existing DC blocker remove constant components. These are
structural explanations consistent with the feedback, not proof of a listener's cause.
Headroom changed A/B rails and envelope input compression, so greater rail height also
raised output amplitude. There was no Headroom-specific compensation.

An archived pre-change executable/header and 2x endpoint measurements are under
`cpp/build/*before_macro2*`, `cpp/build/CustomDrive-before-macro2.h`, and
`cpp/build/v08-macro2-before/`. Those generated files are ignored by Git.

## Current topology and controls

Drive gain, Drive compensation, Tone shelves, native input HPF, output filter/DC
block, Level/Mix, oversampling algorithm and IDs/defaults/ranges are unchanged.
Changing the nonlinear island necessarily changes the final response at identical
Drive settings; no legacy sound identity is promised for Custom Drive presets.

For normalized Crunch c, Headroom h, Grain g:

- Crunch controls p=2↔p=8 smooth clipping, A knee `Arail*(2-c)`, B excitation
  multiplier `1+0.75*c`, and serial-stage contribution. At c=0/g=0 only filtered A
  contributes; at c=1/g=0 B contributes60%. This combines a broad rounded knee vs
  sharper onset with different serial interaction. It does not change Drive gain.
- Headroom retains A rail `0.18+3*h*h`, B rail `0.5+1.5*h`, input sag
  `0.5*(1-h)^2`, attack0.5–4.5ms, release200–30ms. Its fixed output compensation is
  `(0.93/Arail)^0.53`, independent of input/envelope/RMS. No auto-normalizer, limiter
  or separate compressor was added. Low Headroom responds to weak input sooner and
  retains more gain reduction after strong input; high Headroom preserves more peak
  contrast. Compensation limits average differences on the defined transient probe,
  not every guitar/pickup/Drive setting.
- Grain controls B rail mismatch `Brail*(1-0.4*g)`, bounded dynamic bias
  `g*(0.45+0.12*env/(1+env)+0.25*memory/(1+abs(memory)))`, and rectified-memory
  residual coefficient0.45*g. The B weighting is `1-(1-0.6*c)*(1-0.95*g)`:
  g=0/c=0 gives0%; g=1/c=0 gives95%; both maxima give98%.
- A feeds B in series. The output interpolates A and B's **serial residual inside
  the oversampling island**, rather than adding a separately excited parallel drive.
  Crunch sets the knee/excitation; Grain sets threshold mismatch/bias/history. Their
  shared contribution control is explicit, not complete independence.
- Grain also moves the existing interstage one-pole LP from11kHz to7kHz (Nyquist
  capped) to restrain B's HF excitation. It is not a treble boost. No noise, bitcrusher,
  sample-rate reduction, new oversampler, ADAA or additional nonlinear stage was added.

A candidate with an overly broad rational knee increased aliasing and was rejected.
A second candidate's low-Crunch rail was too low: after RMS matching it resembled the
hard endpoint. The final low-Crunch knee is wider; endpoint measurements were checked
before generating25/50/75 values. No tolerances were widened.

All continuous controls retain the Engine's20ms smoothing. Coefficient caching also
checks Grain because it now controls interstage damping. Fixed lane state and heap
capacity are unchanged; the processor adds arithmetic only. No audio-thread allocation,
free, lock, I/O, logging or resize is introduced. The existing abnormal-input guard
and finite-output checks remain. Reported FIR latency remains0/64/64samples.

## Listening and diagnostics

```sh
cmake --build cpp/build --target custom_drive_benchmark
MPLCONFIGDIR=/tmp/multi-effects-mpl .venv/bin/python examples/benchmark_custom_drive.py \
  --mode 2 --check-macros --output cpp/build/v08-macro2
# Optional owned/licensed DI: --di /absolute/path/guitar.wav
# Iteration only, no profiling: --skip-profile --steps 0,100
```

The existing Quality Lab now accepts macro/listening factor1/2/4 (`--mode`) and an
optional archived benchmark executable (`--executable`). Default remains4x for existing
commands. It also renders named cross-comparisons. No DI was found or downloaded.
Synthetic listening input alternates weak/medium/strong110Hz harmonic transients
(.06/.2/.6peak) twice, with0.5s between attacks. This is not a real palm-mute recording.

`raw/` and `level_matched/` each contain29WAVs:25single-macro sweep files and
`crunch000_grain100.wav`, `crunch100_grain000.wav`,
`drive070_headroom000.wav`, `drive070_headroom100.wav`. Each matched file uses the
same default-render RMS. Files are float32 with no limiting; reduce playback gain
if peaks exceed0dBFS. Parameter Headroom-only files use default Drive50; named
Drive70 files isolate the requested higher-drive comparison. User DI channels are
preserved and processed independently.

`metrics.json` records harmonic bins, coherent alias proxy, IMD, amplitude curves,
recovery and early/late transient windows. `report.md`, `sweeps.csv`, `quality.png`
and `profile.csv` are generated alongside WAVs. `--check-macros` checks matched
waveform differences, Headroom average-level bounds and weak/strong transient peak
ratio. These are regression guards, **not an audibility or quality classifier**.

Keep follow-up listening fixed-input and level matched: Crunch softness/onset,
Headroom weak/strong peak and recovery, Grain texture vs unwanted HF or bass smear,
and crossed Crunch/Grain endpoints. Test2x first, then4x for the harshest combination.
No native app/Logic session is closed automatically during this work.

## Candidate measurements (2x unless stated)

On the same synthetic transient sequence, after offline RMS matching:

| Macro | Previous difference/reference RMS | Candidate difference/reference RMS | Candidate raw endpoint RMS difference |
|---|---:|---:|---:|
| Crunch | 0.1191 | 0.2393 | +3.11dB |
| Headroom (Drive70) | 0.4322 | 0.4446 | −0.61dB (previous +13.07dB) |
| Grain | 0.0927 | 0.3074 | −2.13dB |

These are difference-waveform ratios, not loudness percentages or perceptual scores.
Crunch0/100 steady440Hz THD7.16/24.05%; Grain0/100 THD9.38/26.92%, H2 numerical
floor/−19.65dBc. Crossed Crunch0/Grain100 vs Crunch100/Grain0 has H2−20.14dBc vs
numerical floor, and THD19.61% vs21.70%: similar total distortion with different
harmonic distribution. Stage weighting changes should be judged on matched files.

Drive70 Headroom0/100 transient RMS0.06706/0.06253, crest11.84/17.56dB. Weak attack
peaks0.2070/0.1562; strong attack peaks0.2346/0.4584. Low Headroom's strong/weak
peak ratio1.13 vs high Headroom2.94 is not explainable by a single static gain.
Steady440Hz THD35.31/13.25% on that comparison. Input-level curves and recovery
probes are retained in metrics.json; a real guitar is still needed for sustain/pumping.

Default IMD−12.05dBc (previous approximately−11.35). Endpoint Grain100−2.82 and
Crunch100−4.82dBc show stronger intentional nonlinear interaction, not a universal
quality improvement. Default alias proxy1x/2x/4x−43.60/−70.24/−114.77dBc. Drive100
−19.27/−39.90/−62.46dBc. Compared with the preceding design, default2x alias is
about4.7dB higher and Drive1004x about4.5dB higher; this tradeoff is reported, not
hidden. Grain1002x−52.39dBc, Crunch1002x−54.38dBc; the all-extreme corner remains
much worse (2x−20.72 /4x−35.15dBc, IMD+4.68dBc). Extreme settings need listening
and possibly narrower ranges after feedback. No claim of alias-free distortion.

Maximum absolute steady DC across sweeps4.92e−10. Transient-window means are not
steady DC and can be larger. The fixed-state silence/reset/overload/nonfolding-input
and realtime output-update tests pass without changing their existing tolerances.

## One final CPU and regression run

Stereo48k/128; deadline2666.67µs;100warmup then2000measured blocks:

| Mode | Mean µs | Observed max µs | Max/deadline | Added latency |
|---|---:|---:|---:|---:|
| 1x | 9.384 | 18.625 | 0.70% | 0samples |
| 2x | 67.527 | 957.791 | 35.92% | 64samples |
| 4x | 117.917 | 324.834 | 12.18% | 64samples |

This is the requested single final profile, before parallel final builds. Observed
max includes scheduling; the2x spike is retained, not replaced by a favorable rerun.
Mean is in the previous implementation's range (previous9.123/61.406/116.606µs).
No embedded inference or worst-case deadline guarantee is made. Moving controls can
add coefficient recalculation cost. Lane state3064bytes and prepared stereo heap9160
bytes remain unchanged on this ABI; callback allocation/free are both zero.

Final regression performed once: CTest29/29, pytest48/48, six legacy Python↔C++
comparison scripts, ASan/UBSan27/27(non-long-run, leak detection disabled), TSan7/7
concurrency tests passed. Core-only, Standalone Release, AU and VST3 builds succeeded.
Preset/state/embedded IR and automation regression passed, including live macro
changes through the actual PluginEditor and independent stereo outputs.

After explicit user approval, the new AU was installed and
`auval -v aufx Mfx6 Mefx` passed (exit 0, `AU VALIDATION SUCCEEDED`). The installed
executable hash matches the new Release build. Validation log:
`cpp/build/v08-macro2-auval.log`. The previous installed AU remains backed up at
`cpp/build/v08-macro2-previous-installed.component`. Existing app/host sessions
and unsaved user changes were preserved. Restart the host when ready to load the
updated binary; no new Logic listening result is claimed. No commit/push.

P0: none newly found in automated correctness/realtime checks. P1: actual matched
listening/playing acceptance, high-endpoint IMD/alias/fizz and low-end smear/recovery.
P2: narrow endpoint ranges and intermediate tapers from that feedback. Do not count
this as perceptual success merely because numerical guards passed.
