# Standalone prototype

## Reproducible build

Core: C++17, AppleClang/MSVC-oriented, no JUCE dependency. App: JUCE 8.0.12,
commit `29396c22c93392d6738e021b83196283d6e4d850`, fetched by CMake only when enabled.
All fetched source and binaries remain under ignored `cpp/build`.

```
cmake -S cpp -B cpp/build -DMULTI_EFFECTS_BUILD_APP=ON -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --parallel 4
ctest --test-dir cpp/build --output-on-failure
```

macOS bundle: `cpp/build/app/MultiEffectsStandalone_artefacts/Release/Multi Effects Prototype.app`.
Debug builds use the `Debug` subdirectory. Core-only builds use
`-DMULTI_EFFECTS_BUILD_APP=OFF`. Initial dependency download requires network access.
For an offline source checkout, set `FETCHCONTENT_SOURCE_DIR_JUCE` to that checkout.
Windows build execution has not been verified.

## Operation

Starts muted. Select an audio device and the guitar input using the device panel,
use headphones to avoid acoustic feedback, then clear Mute. The dedicated Input selector
lists the device's physical channels (Input 1, Input 2, ...); selecting a disabled
channel enables it through a device restart. Switching already-enabled inputs needs
no restart. Selected mono input is processed and duplicated to available outputs.
The practical v0.2 target is **48 kHz / 128 samples**: set both in the device panel;
the status row reports the actual device settings rather than assuming the target.
A missing selected channel falls back to the first active input during prepare, or
none if there are no active inputs. An invalid runtime selection supplies zero input
while existing DSP tails continue. Input choice is session-local, not part of a preset.
JUCE's device-panel Test tone bypasses the app Mute/output clamp/meters: keep hardware
volume low when using Test; it is not a test of the DSP chain. Output gain is -6 dB and
final output clamps to +/-1. Add/remove/reorder effects using the left panel.
Controls are generated from the Core registry's parameter metadata. Cabinet defaults
to a generated HP/LP impulse (not a captured commercial cabinet); Load cabinet IR
accepts a user-owned WAV. The loader averages channels, removes DC and normalizes
peak. Resampling uses an offline windowed-sinc implementation, not SciPy's exact
resample_poly algorithm. Max IR: 32768 samples including resampled length.

## Levels and clipping

Input/output peak meters refresh at 10 Hz with peak accumulation between GUI reads.
Numbers use 20*log10(abs peak); digital silence displays -inf dBFS. Input is measured
before DSP. Output is measured after the existing mute/-6.02 dB gain and +/-1 clamp.
Input CLIP means abs(input) >= 1. Output CLIP means abs(pre-clamp output) >= 1;
indicators hold for about two seconds. Internal intentional amp/distortion saturation
is not an output CLIP event. These are sample-peak indicators, not true-peak/RMS or
analogue interface clip detectors. NaN/Inf input is replaced with zero before DSP;
non-finite final output is also replaced with zero and reports INVALID SAMPLE.

## Architecture and ownership

`effects/` contains algorithms. `engine/Engine` contains JUCE-independent Effect,
Parameter, registry and Chain. The app owns device/GUI/file dialogs and JSON IO.
No external cabinet assets or models are bundled.

`engine/AudioIO.h` provides a non-owning planar AudioBlock boundary and MonoAudioIO
policy. It maps physical input IDs to JUCE/CoreAudio's packed active input buffers.
The JUCE adapter supplies that map during stopped prepare. Future stereo processing
can use the same planar boundary with separate channel state; current Effects remain
mono. Channel maps/scratch allocations are prepare-only. Selection, mute and level
publication use lock-free atomics; string formatting and widgets remain on the UI
thread. prepare failure silences output and displays PREPARE FAILED; select a valid
rate/IR/device to retry. Restart clears the output gain ramp and resets DSP tails.

Each effect owns its state. `prepare` and `reset` occur with processing stopped;
reset may reconstruct an algorithm and allocate. Processing accepts arbitrary block
lengths and performs no explicit allocation, IO or mutex locking. GUI parameter
targets and bypass are lock-free atomics; smoothing state is owned by audio thread.
20 ms smoothing applies to continuous controls and bypass. This is not sample-accurate
host automation. A cabinet has 128 samples latency, also retained during bypass.
Multiple cabinets add latency. No plugin latency reporting is implemented yet.

Structural edits/preset/IR changes remove the callback before modifying or destroying
objects, then reattach and prepare. This intentionally interrupts audio and resets
tails; seamless live graph replacement is deferred. Audio callback works in prepared
scratch chunks if the device supplies a larger-than-expected block. DSP continues
while bypassed to preserve state/tails. ScopedNoDenormals belongs to the JUCE adapter.

Legacy Gain retains Python clipping. The product registry's Gain is explicitly
headroom-preserving. Amp's low-cut uses double coefficients/state to prevent
low-frequency float coefficient error from being magnified by nonlinear stages.
All external audio buffers remain float. Existing Filter setters retain their original
reset behavior unless the new retain-state flag is enabled (used by realtime EQ).
Delay preserves Python's feedback-scaled first echo. Integer delay-time changes can
still produce pitch/click artifacts; a fractional/crossfaded delay is a later quality
improvement. Oversampling and anti-aliasing are not yet implemented.

## Gain staging and parameter audit (v0.2)

Nominal device samples lie in [-1,1], but finite values outside that range are not
normalized internally. There is no additional limiter, normalization or compressor.

| Stage | Range/headroom and accumulation |
| --- | --- |
| Input / Gate | Input meter observes raw selected samples; Gate gain stays 0..1. No hard clamp. |
| Compressor | Envelope gain reduction plus makeup (-12..18 dB); not a limiter, makeup/attack can exceed full scale. |
| Registry Gain | -60..24 dB, up to about 15.85x; deliberately retains float headroom. Legacy CLI Gain clipping is separate and unchanged. |
| Distortion | Intentional tanh wet saturation; dry/wet mix can exceed 1 if dry signal does. No new clamp. |
| EQ | Fixed 1 kHz peaking EQ, +/-12 dB; boosted bands/transients can exceed full scale. No exposed frequency parameter yet. |
| Amp | Biased tanh preamp stages and tanh power saturation are intentional; tone/presence/output filters mean final peak is not guaranteed <=1. |
| Cabinet | Linear FIR plus output gain; peak-normalized IR does not imply unity convolution gain. Bound depends on sum(abs(IR)); long/custom IR can amplify. |
| Delay | Feedback <=.95; repeated correlated material can accumulate (feedback buffer bound up to 20x bounded input). Wet/dry is not a limiter. |
| Reverb | Four combs averaged, then two allpasses; feedback <=.92 in registry. Stable bounded controls do not guarantee peak <=1; tails can accumulate. |
| Output | Existing 0.5 gain with mute ramp, then explicit +/-1 clamp. CLIP exposes potential audible hard clipping; no automatic gain correction added. |

Existing 20 ms one-pole smoothing is retained for Gain, Drive, EQ gain, Amp,
Delay time/feedback, Reverb mix and bypass. Parameter targets and bypass are atomic;
DSP smoothing history is audio-owned. Independent multi-parameter edits are not a
transactional/sample-accurate snapshot. EQ retains filter state on gain update.
TODO (quality): integer Delay tap movement can click/change pitch even with smoothed
milliseconds; evaluate fractional delay or dual-tap crossfade against reference tests
in a later tone-changing task. Input switching also selects a new waveform immediately;
a route crossfade is deferred. Structural edits intentionally interrupt/reset tails.

Known realtime limits: our processing path performs no allocation/free, file IO,
mutex, logging, resize, strings or GUI work; the per-sample cabinet type comparison
is now a precomputed bool. JUCE AudioDeviceManager/CoreAudio still have framework
locks and scratch-buffer sizing outside this adapter. Tests instrument C++ new/delete,
not every malloc/driver operation. Level peak publication is lock-free CAS, not a
formal wait-free deadline proof. Maximum-length/multiple IR convolution and concurrent
GUI load need hardware profiling; default-chain timings do not cover worst cases.

## Presets

App schema `multi-effects`, version 3. Stable effect instance IDs, type IDs and
parameter IDs are serialized independently of display names. Embedded IR samples
and their source rate are saved with cabinet nodes; files are not read in callback.
Version 2 is accepted for supported types/parameters. Unknown types/parameters are
rejected instead of silently producing a different sound. Full legacy compatibility
is not implied. Parsing and preparation precede replacement where possible.

## Validation

`test_engine` exercises all registry effects at 44.1/48/96 kHz for arbitrary block
partition equivalence, reset, finite outputs, bypass/control updates, and watched
C++ new/new[] allocations. It also compares multi-partition convolution to direct FIR.
This is not a proof against every possible allocator or an audio deadline benchmark.
`examples/compare_cpp_python_prototype.py` compares Delay/Reverb/Amp/Tremolo/Cabinet
against float64 Python references. Cabinet comparison accounts for 128-sample latency.
Reverb tail is produced by subsequent zero-input samples, not by changing block size.

## Dependencies and distribution

JUCE modules are dual-licensed AGPLv3/commercial JUCE license:
https://github.com/juce-framework/JUCE/blob/8.0.12/LICENSE.md
The local build does not select a commercial license or authorize distribution.
Before distribution choose a compatible license route and audit bundled dependencies.
IR rights remain the user's responsibility; no third-party IR/model is included.
Core source licensing is not changed by this work. Training, tone matching, plugins,
stereo spatial effects and embedded hardware are outside this prototype implementation.

## Verified locally (2026-11-02)

- AppleClang 21, arm64 macOS: Debug and Release `.app` builds succeeded.
- Release CTest: 8/8 passed; Python pytest: 42/42 passed.
- Python comparison: all existing effects and new Delay/Reverb/Amp/Cabinet/Tremolo
  passed. New effects tested at 44.1/48/96 kHz. Maximum Amp absolute difference
  was 4.511e-5 with unchanged tolerance 1e-4 (relative 1e-5).
- GUI save/load round trip restored gate bypass and chain order in Debug.
- Release launched with built-in input/output, 48 kHz, 512 samples. Callback counter
  advanced continuously; observed JUCE CPU display approximately 1.7–2.6% for the
  default chain. This is a short smoke observation, not worst-case profiling.
- Release GUI parameter edits, bypass, reorder, add and remove were exercised.
- Output remained muted. Guitar/interface monitoring, audible quality, round-trip
  hardware latency and prolonged dropout testing require a connected guitar setup.
- Default cabinet is synthetic; WAV IR GUI import and embedded-IR preset roundtrip
  are implemented but have not been exercised through the native file chooser yet.


## v0.2 validation (2026-11-03)

- Release CTest: 15 tests, including preset/IR roundtrip and 600 seconds of offline
  audio at each of 44.1/48/96 kHz. Existing tests/tolerances remain unchanged.
- AudioIORoutingMeterMatrix covers physical 0/1, sparse active map, invalid/null/zero
  inputs, mono/stereo/null outputs, channel-count changes, failed IR reprepare/recovery,
  silence/near/full-scale/overload/non-finite input, mute and stopped output.
- At 44.1/48/96 kHz and 64/128/256/512 samples, fixed/irregular partitions and repeated
  prepare match the legacy Input 1 processing/output-gain formula exactly.
- AudioIOConcurrentControls exercises routing/mute/meter publication while processing
  10000 blocks. Watched processing allocations/frees are zero, including the IO adapter.
- Python pytest: 42 passed; all six Python/C++ comparison scripts passed unchanged.
- ASan/UBSan: six relevant tests; TSan: both parameter and IO concurrency tests.
  Apple ASan does not support detect_leaks=1 here; use detect_leaks=0. Existing tracked
  live-memory/long-run regression remains enabled in normal builds.
- Debug and Release macOS app builds succeeded. Debug targeted IO/engine/matrix/preset
  tests also ran. Offline Release 48k/128 default-chain timing observed ~15 us mean,
  ~52 us maximum versus a 2667 us block budget; this is not a hard realtime guarantee.
- Release actually launched, device initialized, input selector and live input levels
  observed. Built-in input/output at 48k/128 ran muted with advancing callback count
  (10676 -> 22716); a short JUCE CPU observation was ~0.8%. Changing device buffer
  size reinitialized processing successfully. User's pre-existing preset/embedded IR
  was backed up under ignored build output and restored through the new GUI.
- Built-in microphone exposes one input: external interface Input 2, audible output,
  clip indicators under actual guitar overload and prolonged 128-sample playing remain
  user checks. Output mute behavior/replication/clipping are covered by automated IO tests.

Run the added tests with `ctest --test-dir cpp/build -R AudioIO --output-on-failure`.
Sanitizer build directories remain under ignored `cpp/build/sanitizers` and `cpp/build/tsan`.

## v0.3 nonlinear foundation

Distortion and Amp expose **Oversampling: 0=1x, 1=2x, 2=4x**. This is a discrete
prepare-time parameter (default 0), saved under stable ID `oversampling`. Changing
it stops the callback, reconstructs/prepares the chain and resets tails; do not
switch modes mid-performance expecting a seamless transition. Core clients must
apply staged mode targets with stopped prepare/reset. Continuous parameters retain
20 ms smoothing. Existing v2/v3 presets missing the field default to exact legacy 1x;
v3 schema and IR data are unchanged. Older application binaries may reject presets
saved with the new field: backward loading is supported, forward loading is not.

`effects/Oversampler.h` is a JUCE-independent, fixed-capacity C++17 interpolation /
nonlinearity / decimation utility. 1x calls the original nonlinear transfer directly.
2x/4x use symmetric Blackman-windowed sinc FIRs of 64*factor+1 taps with cutoff
0.45/factor cycles per high-rate sample. Coefficients have unity DC sum; interpolation
uses factor gain and polyphase evaluation. Decimation filters the high-rate nonlinear
output and samples phase zero. Finite transition bands are intentional (not a
brick-wall filter); high-frequency response changes and FIR ringing are tradeoffs.
All storage is embedded/prepared, no process allocation; reset clears histories.
Non-finite input/nonlinear result is replaced with zero at the oversampling boundary.
The callback supplied to this utility operates at the elevated rate; the current
nonlinear islands are memoryless. Stateful future models must explicitly use that rate.

One island adds **64 native samples** at either 2x or 4x. Distortion has one island,
with delayed dry mix. Amp has two, adding 128 samples. Engine `latencySamples()`
exposes the prepared latency and bypass dry delay follows it. With one Cabinet the
full default chain has 128 samples DSP latency at 1x, 320 with both effects enabled
at 2x/4x (2.67 / 6.67 ms at 48 kHz), excluding device/driver buffers and converters.
The added 192 samples are 4 ms. Dry latency is retained while bypassed.

Amp analysis: native-rate 70 Hz double-state low-cut -> input gain -> asymmetric
biased tanh pair (preamp island) -> native bass/mid/treble filters -> tanh power island
with master gain -> native presence/output gain/low-pass. Bias, stage drives, filter
voicing and output compensation remain unchanged; no subjective voicing or loudness
normalizer was added. `AmpSim` remains the algorithm facade behind the same registry
entry and parameter IDs. A future algorithm can replace its internals without rewriting
GUI/preset; a tonal/model change should acquire an explicit version/selector rather
than silently changing old presets. No neural runtime/model framework was introduced.

Gain audit: oversampled Distortion can exceed +/-1 after reconstruction filtering
(observed peak ~1.13 on the high-tone test), even though tanh itself is bounded. This
is retained as float headroom, not clamped/normalized away. Dry/wet and bypass are
time-aligned but not loudness-matched. Compressor makeup, Gain/EQ, custom IR and
feedback tails can still accumulate gain; the v0.2 final clamp and CLIP indicators
remain unchanged. Lower gain/output manually when CLIP appears.

Cabinet already uses uniform partitioned FFT convolution (128-frame partitions,
256-point transforms), so it was not rewritten. A partition must be collected before
output, explaining its 128-sample delay. At 32768 samples, 256 partitions require
about 1 MiB for complex<float> kernel/history spectra, plus workspace/source IR;
frequency-domain MAC work is 32x the default 1024-sample IR, not necessarily 32x
end-to-end CPU. Resampling remains stopped, windowed-sinc, with 32768 resampled-length
limit. Existing WAV DC-removal/peak normalization is unchanged, and embedded samples
are not renormalized on preset reload. Synthetic default IR is not a cabinet capture.
Long-IR impulse tests compare every sample against known sparse taps, including the
last partition. Existing WAV/silence/size/rate/preset tests remain enabled.

### Reproduce measurements

```
cmake --build cpp/build --parallel 4
.venv/bin/python examples/benchmark_nonlinear.py
ctest --test-dir cpp/build --output-on-failure
```

The script reuses `analysis.audio_analysis` and `analysis.harmonic_analysis` and writes
RMS, peak, THD/harmonics, spectral difference, FFT arrays, alias proxy and CPU profile
under ignored `cpp/build/v03-analysis`. See [measurement snapshot](nonlinear-benchmark.md).
It asserts at least 6 dB alias-residual reduction vs 1x for the specified coherent high
sine in both 2x and 4x. This is not a universal alias bound or subjective sound rating.

v0.3 checks include 72 nonlinear rate/mode/block cases, reset/irregular split,
latency-aligned bypass, invalid samples, staged mode changes, 32768-tap convolution,
all three modes' preset roundtrip and old presets' 1x default. Full 4x chain runs
60 seconds of simulated audio at each 44.1/48/96 kHz with changing block sizes, in
addition to existing 600-second/rate legacy runs. Separate concurrent controls tests
run the active 4x processors while changing atomic targets (mode remains staged).

Known limits: FIR CPU/latency is deliberately conservative; a shorter/halfband or IIR
backend requires new response/phase/alias measurements. Built-in microphone tests do
not verify external Input 2, audible gain changes, playing feel or hardware latency.
During initial v0.3 launch one CoreAudio `AudioDeviceStart` hang occurred before DSP
prepare; restarting only the app recovered. Keep this as a device-init reliability
issue for external-interface regression, not a proven DSP regression. JUCE framework
locks, Test-tone Mute bypass and Delay time-transition limitations remain as documented.

Final local v0.3 validation: Release CTest **18/18**, pytest **42/42**, all six existing
Python/C++ comparison scripts passed without tolerance changes. ASan/UBSan **9/9**
selected tests and TSan **3/3** concurrency tests passed (including active 4x DSP).
Debug/Release app builds passed; Debug oversampling/preset tests **2/2** passed.
The Release GUI ran both nonlinear effects at 4x, 48 kHz/128 samples, muted, with
callback counter advancing from 8051 to 436612 and live input meter updates. This
observes liveness, not a hardware dropout counter or audible-quality result. After
validation the original user's v0.2 preset/embedded IR was restored in the v0.3 GUI,
returning absent mode fields to 1x. Output remains muted. No commit/push was performed.
