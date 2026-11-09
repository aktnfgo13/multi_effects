> Historical initial v0.8 mapping and measurement. For the subsequent listening-driven
> revision and current Quality Lab, see [Custom Drive tuning](custom-drive-tuning.md).

# Custom Drive v0.8 — provisional analytical engine

This adds `custom_drive` as a separate registry effect and appended host parameter
bank. Existing Distortion, Amp, Cabinet, time effects, factory presets and default
chains retain their algorithms/values. Add Custom Drive in Standalone, save a preset
and load it in AU/VST3; the plugin keeps its existing safe preset-based chain editing.
The eight controls use the existing selected panel in two columns, not a new UI.
Version is 0.8.0 Alpha; host/bundle identities stay unchanged.

## Topology and units

Input → wet-path HPF → pre high-shelf → Drive gain → oversampled island
[envelope gain compression → nonlinear A → interstage LP → nonlinear B with dynamic
bias → rectified memory residual] → post high-shelf + fixed LP → DC block → Level
→ latency-aligned dry/wet Mix. The island contains every nonlinear operation,
including envelope/memory interaction; native filters/mixing remain at host rate.
L/R have independent envelope, filter, memory, oversampler and dry histories.

Stable IDs are `custom_drive.drive`, `.tone`, `.crunch`, `.headroom`, `.grain`,
`.level_db`, `.mix`, `.oversampling`, `.bypass`. Engine IDs omit the prefix.
New parameters append to the host list; previous IDs/order and schema v2/v3 remain.
Existing states without this effect do not insert it or change their sound.
Older binaries cannot load presets containing an effect they do not implement.

All mappings below are **provisional**, not listening-optimized values:

| Control | Range/default | Mapping |
|---|---|---|
| Drive | 0–100 / 50 | `10^(30*d/20)` input gain and `1+0.6*d` interstage gain; wet HPF 70–150 Hz |
| Tone | 0–100 / 50 | Pre high-shelf ±2 dB at 1.4 kHz, post high-shelf ±6 dB at 1.8 kHz |
| Crunch | 0–100 / 50 | Blend smooth p=2/p=4 clipping knees; B uses 0.6 of A's hardness |
| Headroom | 0–100 / 50 | A rail 0.3–1.5, B rail 0.7–1.5; envelope gain compression strength `0.65*(1-h)^2`; release 120–30 ms |
| Grain | 0–100 / 25 | B bias from fixed offset + rectified input envelope + signed 8 ms memory; 25 ms rectified residual adds even harmonics |
| Level | −24…+12 dB / −6 | Manual wet output gain |
| Mix | 0–1 / 1 | Linear wet/dry interpolation; dry aligned to FIR latency |
| Oversampling | 1x/2x/4x / **2x** | Existing FIR oversampler; prepare/rebuild only |

Here `d,h` are normalized 0–1 values. Tone 50 means unity **adjustable shelves**;
the underlying HPF/interstage/post filters remain active, so it is not a flat wire.
Continuous targets keep the Engine's 20 ms smoothing. Discrete mode changes use
existing stopped/deferred preparation, resetting tails as before.

## Nonlinearity and dynamics

A uses `f2(x)=x/sqrt(1+x*x)` and `f4(x)=x/(1+x^4)^(1/4)`; Crunch interpolates them.
Both are smooth odd functions, unity slope at zero, bounded asymptotes ±1. B uses
the same family with a smaller hardness blend and `f(v+bias)-f(bias)`, preventing
bias from producing output from silence. Grain also adds `0.18*g*(r-memory25ms)`,
where `r=B²/(1+B²)`. Overall wet gain includes a fixed 0.7 factor, not automatic
normalization. There is no noise, bit crushing, sample-rate reduction or tanh cascade.

Headroom's detector tracks absolute driven input (capped at 8 for detector stability),
with 1.5 ms attack and a headroom-dependent release. It compresses the stage input by
`1/(1+sag*envelope)`; it does **not** lower the saturation rail with input level.
An initial rail-sag experiment produced static foldback and was rejected after
measurement. The final measured −60…0 dBFS input curve is monotonic. Lower Headroom
changes both onset and recovery/compression; higher Headroom preserves more relative
transient range. No claim of a physical tube/pedal circuit or subjective realism.
Crunch controls knee shape, Grain controls even-harmonic/memory interaction. THD need
not grow monotonically with Crunch: at the measured operating point a firmer knee
preserves more of the small-signal region while redistributing harmonics.

## Filtering, latency and safety

The HPF is a double-state one-pole complement; its cutoff rises 70→150 Hz with Drive
for pre-distortion bass control. Shelves are low/high one-pole splits. Interstage LP
is 11 kHz and native post LP is 10 kHz (both capped below host Nyquist); DC block is
12 Hz. These constitute explicit provisional voicing, not a hidden analyzer filter.
The dry path is unfiltered apart from delay, so Mix=0 preserves dry input DC.

Existing Blackman windowed-sinc FIR: `64*factor+1` taps, cutoff `0.45/factor`
cycles/high-rate sample; interpolation gain=factor, decimation unity DC. Total
added linear FIR delay: **0 / 64 / 64 samples** at 1x/2x/4x (1.333 ms at 48 kHz).
IIR phase/group delay is frequency-dependent and is not included as a fixed host
latency. No whole-chain oversampling or ADAA was introduced. `island()` separates
nonlinear processing for future alternative anti-aliasing comparisons.

No processing allocation, locks, I/O, logging or resize. Fixed per-lane state is
3064 bytes on this arm64 build; two lane payloads total 6128 bytes. The prepared
stereo Effect (including metadata, targets, bypass delays, callbacks, buffers) had
9160 tracked live C++ heap bytes beyond warmed registry storage. Allocator overhead
and plugin/GUI storage are excluded; do not extrapolate this ABI/CPU to embedded ARM.
Repeated prepare returns to the same tracked live allocation size. Invalid samples
are sanitized; finite inputs beyond ±64 (36 dBFS) are abnormal-overload guarded before
filter state. No limiter/normalizer is applied to normal audio. Level can exceed
0 dBFS; retain the existing meter/CLIP workflow.

## Reproducing quality measurements

```sh
cmake --build cpp/build --target custom_drive_benchmark test_custom_drive
.venv/bin/python examples/benchmark_custom_drive.py
# Optional, user-owned/licensed WAV, first channel, up to 60 seconds:
.venv/bin/python examples/benchmark_custom_drive.py --di /absolute/path/guitar.wav
```

Results: ignored `cpp/build/v08-quality/metrics.json`, `spectra.npz`, `quality.png`,
`profile.csv`, `report.md`, and level-matched old/new synthetic WAVs. No suitable DI
was found in the repository and none was downloaded. Old/new and macro comparisons
use offline RMS matching only; raw RMS/peak/DC/THD/IMD/alias measurements remain
unmatched. Optional DI includes level-matched spectral change versus full input;
it is not assigned a synthetic-tone THD score.

Signals: 100/440/1000/7013 Hz sines, 60 Hz + 7 kHz two-tone (4:1 amplitude), multitone,
110 Hz harmonic-rich waveform and decaying synthetic picks. All five macros sweep
0/25/50/75/100 at 4x. Volume fractions 1/.7/.5/.3/.1; peak input curve −60…0 dBFS.
Headroom probes include onset/sustain, silence decay and 5/20/80/250 ms recovery gaps.
Recovery is a whole-engine probe, including filters, not an isolated envelope reading.

THD uses up to 32 in-band harmonics via the existing analyzer. IMD sums ten sidebands
at 7 kHz ±60…300 Hz relative to the 7 kHz carrier. Alias proxy is the non-harmonic
residual of a one-second coherent 7013 Hz FFT, excluding DC and legitimate in-band
harmonics ±1 bin. It is signal-specific, and values near −125 dBc approach the float
numerical floor; this is not proof of universal alias suppression. High-drive stress
is reported separately. Oversampling does not remove intentional IMD.

## Listening priorities

Tune Drive gain range/distribution; Tone shelf pivots/amounts; Crunch knee blend;
Headroom onset, attack/release and compression strength; Grain fixed/dynamic bias,
memory times and residual amount; bass tightening and post/interstage bandwidth;
manual Level default and wet/dry balance. In particular, high-drive low-Headroom IMD
remains substantial. Compare level-matched recordings at actual interface levels,
guitar volume settings and palm-muted/transient passages before subjective voicing.
No new amp model, cabinet, model-drive clone or other effect is part of this sprint.

## Recorded synthetic results (this Mac)

At input sine peak 0.3, default macros, Level −6 dB, full wet:

| Factor | 440 Hz THD | Two-tone IMD | 7013 Hz alias proxy | Drive 100 alias proxy |
|---|---:|---:|---:|---:|
| 1x | 17.907% | −9.48 dBc | −32.83 dBc | −16.03 dBc |
| 2x | 17.898% | −9.46 dBc | −76.44 dBc | −36.17 dBc |
| 4x | 17.896% | −9.45 dBc | −125.09 dBc | −63.71 dBc |

The near-floor default 4x number must not be generalized to high-drive or all macro
combinations. At Drive 100, two-tone IMD is approximately −1.22 dBc at 4x: substantial
intentional mixing remains even when aliasing is reduced. Old Distortion at 15 dB,
4x, wet=1 measured −2.93 dBc on the same two-tone; the engines have different filters
and gain distributions, so this is not an isolated waveshaper superiority test.

4x 100 Hz / 440 Hz / 1 kHz THD: 12.589% / 17.896% / 17.451%. With Grain 0→100,
second harmonic moves from the numerical floor to −35.5 dBc; the Crunch sweep keeps
it around −47 dBc at this operating point, illustrating different roles. All harmonic
orders are retained in metrics.json, rather than using THD as a sound-quality score.

440 Hz volume fractions 1 / .7 / .5 / .3 / .1 give THD 17.896 / 12.506 / 8.020 /
3.608 / 0.584%. This demonstrates nonlinearity changing with input, not just a quieter
output. For a loud burst followed by a quiet probe, Headroom 0 recovers from about
−7.0 dB at a 5 ms gap to −0.66 dB at 250 ms relative to the rested probe. Headroom 100
is approximately 0 dB. Low-Headroom recovery/pumping needs real-guitar judgment.

Default 4x 440 Hz: RMS 0.23317, peak 0.28042, steady DC about −3.1e−11. Measured
−60…0 dBFS input peak curve is monotonic, compressing toward RMS 0.2676 at 0 dBFS.
Silence-tail probes decay to around 1e−11. Level and Mix remain manual; old/new WAVs
are normalized only in the separate analysis output.

Stereo 48 kHz / 128 samples, 100 warmup + 2000 measured blocks, steady controls:

| Factor | Mean µs | Max observed µs | Max / 2666.67 µs deadline |
|---|---:|---:|---:|
| 1x | 8.05 | 31.63 | 1.19% |
| 2x | 63.06 | 86.33 | 3.24% |
| 4x | 118.83 | 162.17 | 6.08% |

This is effect-only timing on this Mac, with background build activity possible.
It excludes DAW/GUI graph overhead and is not a worst-case bound. Continuous control
motion may add coefficient recalculation cost; no embedded CPU claim is made.

Validation: Release CTest 28/28, pytest 45/45, six legacy Python/C++ comparisons,
Debug/Release Standalone and Release AU/VST3 builds passed. ASan/UBSan 26/26 passed
(long-run excluded, ASan leak detection disabled); Release includes the existing
long-run tests. Processing allocation/free is zero, independent L/R/variable-block
output and reset are deterministic. New tests cover custom preset/state, eight-control
UI bounds, automation, reported latency, memory stability and invalid/overload input.
AU `auval` confirmed version 0.8.0 and passed. The old user-local AU is backed up at
`cpp/build/v08-previous-installed.component`. Native Standalone screen verification
completed after unlocking the Mac: selecting Custom Drive displays all eight controls
in two columns without overlap, with units, defaults, bypass and 2x oversampling
visible. The observed session used Scarlett 2i2 Input 2 at 48 kHz / 512 samples;
callback counts and input/output meters advanced. Custom Drive was bypassed in that
session. Existing unsaved edits and audio settings were preserved; only the selected
effect was changed. This confirms UI/runtime operation, not physical guitar listening
quality or an external-interface 128-sample stability test.

Changed files:

- `cpp/include/effects/CustomDrive.h`, `cpp/src/engine/Engine.cpp`
- `cpp/tools/custom_drive_benchmark.cpp`, `examples/benchmark_custom_drive.py`
- `cpp/tests/test_custom_drive.cpp`, `cpp/tests/test_plugin.cpp`, `tests/test_custom_drive_analysis.py`
- `cpp/app/Main.cpp`, `cpp/plugin/PluginEditor.cpp`, `cpp/ui/AlphaUI.h`
- `cpp/CMakeLists.txt`, `cpp/cmake/Product.cmake`
- `README.md`, `docs/custom-drive.md`

TSan: **7/7 concurrent tests passed**, including CustomDriveConcurrentControls.
No commit/push was performed. No existing DSP comparison tolerance was changed.
