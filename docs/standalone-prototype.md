# Standalone prototype

Current development: **v0.5 Stereo Alpha candidate**. v0.3 external guitar/interface testing
was reported successful by the user. v0.4 hardware input/reconnect acceptance remains
pending; see the CoreAudio limitation and validation record at the end.

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
no restart. By default the selected mono input feeds both DSP lanes; spatial effects
can generate different L/R tails. Stereo input pair uses the selected physical input
as L and the next as R (missing R is silent). A single output receives (L+R)/2.
The practical v0.2 target is **48 kHz / 128 samples**: set both in the device panel;
the status row reports the actual device settings rather than assuming the target.
A missing selected channel falls back to the first active input during prepare, or
none if there are no active inputs. An invalid runtime selection supplies zero input
while existing DSP tails continue. Input choice and stereo input mode are app preferences, not part of a preset.
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

`engine/AudioIO.h` provides a non-owning planar AudioBlock boundary and AudioIO
policy (`MonoAudioIO` remains a source-compatible alias). It maps physical input IDs
to JUCE/CoreAudio's packed active input buffers. The adapter prepares two DSP lanes;
legacy Core callers still default to one. Channel maps/scratch allocations are prepare-only. Selection, mute and level
publication use lock-free atomics; string formatting and widgets remain on the UI
thread. Prepare failure silences output and retains a readable cause; v0.4 attempts
the last working device configuration, or use Reconnect audio. Restart clears the
output gain ramp and resets DSP tails.

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
Delay preserves Python's feedback-scaled first echo. v0.4 adds a 10 ms dual-tap
crossfade for time changes; fixed-time reference behavior is unchanged. Distortion
and Amp support v0.3's 1x/2x/4x oversampling, described below.

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
v0.4 freezes two integer delay taps during each 10 ms crossfade and coalesces new
time requests until it completes. Input switching fades out for 5 ms then in for
5 ms. Neither adds fixed signal-path latency. Structural edits still reset tails.

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
locks and Test-tone Mute bypass remain; Delay transitions are improved in v0.4 below.

Final local v0.3 validation: Release CTest **18/18**, pytest **42/42**, all six existing
Python/C++ comparison scripts passed without tolerance changes. ASan/UBSan **9/9**
selected tests and TSan **3/3** concurrency tests passed (including active 4x DSP).
Debug/Release app builds passed; Debug oversampling/preset tests **2/2** passed.
The Release GUI ran both nonlinear effects at 4x, 48 kHz/128 samples, muted, with
callback counter advancing from 8051 to 436612 and live input meter updates. This
observes liveness, not a hardware dropout counter or audible-quality result. After
validation the original user's v0.2 preset/embedded IR was restored in the v0.3 GUI,
returning absent mode fields to 1x. Output remains muted. No commit/push was performed.

## v0.4 operation and recovery

Use the same build/bundle above. Start with **48 kHz / 128 samples**, hardware volume
low and **Mute DSP** on. Select output/input devices in the device panel, then the
physical guitar input in the app selector. Check callback active, input meter and
CLIP before unmuting. Test tone is JUCE's independent device test: it bypasses DSP
Mute, app meters and final clamp, as the orange device-panel label states.

Preset controls show the current name and `*` for unsaved changes. Save updates the
current file; Save As chooses another JSON file and confirms replacement. Open loads
an arbitrary file; the browser lists JSON presets in that file's folder. Loading or
quitting with unsaved changes asks Discard/Cancel. No automatic preset overwrite or
delete occurs. Cabinet selection shows synthetic/default versus custom/embedded IR,
including the optional filename. Schema v3 is retained; `ir_name` is optional and
old embedded IRs remain usable without their original WAV. Cabinet convolution,
WAV preparation, Amp, Distortion, oversampling and gain compensation are unchanged.

JUCE ApplicationProperties stores device XML (rate, buffer and enabled channels),
preferred input, last saved preset path and browser folder separately from presets:
`~/Library/Application Support/Multi Effects/MultiEffectsStandalone.settings`.
The initial Save As folder is `Presets` alongside that settings file. Missing or
invalid preferences fall back; missing/bad presets report an error. Every launch and
explicit reconnect starts muted; unsaved edits are not a crash-recovery autosave.

Configuration edits validate a cloned chain before stopping the callback. Rejected
edits retain the old chain. Device prepare uses a candidate and retains configuration
on failure, silencing until successful prepare. Errors are formatted outside process.
A failed device prepare requests one deferred attempt to restore the last working
device XML; **Reconnect audio** closes/reopens it explicitly. Check callback active
after recovery: reopening requested is not proof of device liveness. Channel maps
are rebuilt during prepare; unavailable preferred inputs fall back without invalid
access and can restore on reconnect. Control/lifecycle operations share a mutex;
audio processing never acquires it. GUI callbacks resolve stable IDs, not stale DSP
pointers. Successful graph/device reconstruction resets tails and interrupts audio.

Input changes use a 5 ms fade out/in. Delay time changes use a 10 ms convex blend of
fixed integer taps, with pending requests coalesced, including the feedback signal.
Feedback cap .95 and constant-time Python reference behavior are retained. These
reduce measured steps, not a promise of artifact-free arbitrary modulation. At 48 kHz
the input test's maximum step is .00167; the 100 Hz high-feedback delay test decreases
from about 3.983 (legacy abrupt change) to .0261. No process allocations are introduced.

Monitoring shows callback activity/count, CPU, rate/buffer, DSP latency and nonlinear
modes, input/output levels and CLIP. No callback progress for about two seconds raises
a warning and mutes DSP. Severity uses Error/Warning/Info. The resizable window has a
minimum size to keep controls usable; window dimensions are not persisted.

### v0.4 validation and remaining limits

Release CTest **21/21**, pytest **42/42**, all **six** Python/C++ comparison scripts
passed without tolerance changes. New tests: `AlphaTransitionsRollback`,
`AlphaConcurrentControls`, `AlphaSettingsPresetUX`. They cover failed preparation,
instance preservation, reconnect-like channel/rate/block transitions, silence,
input/Delay continuity, split/reset, allocation tracking, concurrent controls,
real settings-file roundtrip, Save/Save As/failure/overwrite guard and embedded IR name.
ASan/UBSan selected regression **11/11** (leak detection disabled); TSan concurrency
**4/4**. Existing 600-second/rate legacy and 60-second/rate 4x offline runs pass.
These are simulated stream durations, not hours of live hardware testing.

48k/128 offline profile (microseconds/block, deadline 2666.67 us):

| Full default chain modes | Mean | Observed max | Max/deadline | DSP latency |
|---|---:|---:|---:|---:|
| Both nonlinear effects 1x | 16.94 | 47.75 | 1.79% | 128 / 2.67 ms |
| Both 2x | 85.68 | 137.54 | 5.16% | 320 / 6.67 ms |
| Both 4x | 152.37 | 211.13 | 7.92% | 320 / 6.67 ms |

No fixed latency was added in v0.4. Device/driver/converter latency is extra. Timing
is local observed performance, not a hard realtime bound or embedded projection.
Logs and CSV are ignored `cpp/build/v04-*.log` and `cpp/build/v04-profile.csv`.

Release GUI Save As/dirty/cancel/Save/reload and embedded IR display were exercised.
Output-only built-in speakers run at 48k/128 with advancing callbacks; persisted
device/preset and startup mute were observed on relaunch. A long embedded IR rejects
an excessive resampled length with the original cause visible; the final deferred
automatic recovery and explicit Reconnect both restored 48k/128 with active callbacks.
The original user preset was restored and survived relaunch, still muted. Its backup
is `cpp/build/v04-preexisting-session.json`; move/save it outside build before cleaning
build outputs. Debug and Release build successfully.

**P1 release blocker:** this machine repeatedly stalled when opening the built-in
microphone inside CoreAudio `AudioDeviceCreateIOProcID` / `AudioDeviceStart`, with
HAL error `0x10004003`, before DSP processing. No application mutex wait appeared in
the sampled blocked stack. An initialization marker now skips automatic reopening
on the next launch after an unfinished start, allowing access to the GUI. This is
recovery, not a cancellation/timeout of the blocked native call. System audio services
and privacy settings were not modified. v0.4 input audio and physical unplug/replug
are **not validated** by the output-only test; diagnose with the external interface
before declaring daily-use Alpha acceptance. Asynchronous rate-change recovery also
needs that hardware regression; a deferred retry avoids reopening inside the same
notification burst, but callback active remains the acceptance criterion.

P2: seamless graph changes/tails, preset folder management, small-display layout,
live RSS/dropout telemetry and subjective transition listening remain future work.
Final guitar checks: Input 1/2, meters/CLIP, Mute, Delay sweep with feedback, 1x/2x/4x,
custom IR Save/relaunch, 48k/128, unplug/reconnect and a sustained playing session.

## v0.5 stereo contract and policies

`ProcessSpec { sampleRate, maxBlockSize, channels }` accepts one or two DSP lanes;
omitted channels defaults to one for existing Core callers. The original mono
`process(float*, frames)` remains the reference path. After preparing two lanes,
`Effect::processStereo(left, right, frames)` and `Chain::processStereo` accept distinct,
non-null, writable float planes of at least `frames` samples. Do not alternate mono
and stereo calls within a prepared stream: reset/reprepare when changing contracts.
Invalid stereo pointers or an unprepared second lane are rejected without processing.
No JUCE types or OS APIs enter the Core. Block size may vary or exceed prepare's hint.

Each effect owns two independent processor captures and two latency-aligned bypass
histories. Copies are made only from reset state during prepare/reset, including deep
copies of convolution/feedback buffers. Parameters and bypass are shared targets;
smoothing advances **once per frame**, then both lanes consume the same values.
Non-finite inputs are replaced before entering stateful processors. Reset/prepare
still require stopped processing and may allocate. There is no process allocation.

| Effects | Stereo policy |
|---|---|
| Gate, Compressor, Gain, Distortion, EQ, Amp | Dual mono; independent dynamics/filter/nonlinear histories, same controls |
| Cabinet | Same mono IR applied independently to L/R; independent convolution histories |
| Tremolo | Independent lane state with synchronized LFO phase; retains center modulation |
| Delay | Independent feedback lines; L retains legacy time, R uses min(2000 ms, 1.03 × time) |
| Reverb | Independent networks; L retains legacy times, R adds 23/44100 seconds to each line |

Delay/Reverb have `StereoPolicy::Decorrelated`; other effects use `DualMono`. The
small fixed wet-path time offsets create differing tails from mono guitar without
crossfeeding channels or moving the dry center. Delay retains its 10 ms tap crossfade
on both lanes and .95 feedback cap. Reverb feedback/damping/mix and dry/wet gains are
unchanged. This is a conservative decorrelation policy, not a new room model or a
claim of improved sound. At the Delay 2000 ms cap both times coincide; very short
times can quantize to the same integer tap. Spatial effects deliberately are not
invariant to swapping L/R because their line timings differ. Dual-mono effects are.

For finite input, the left lane of every effect matches its existing mono reference
exactly; dual-mono effects produce identical L/R from mono. Only spatial tails differ.
Stereo IR files/dual IR selection are deferred. The planar interface already allows
different kernels per lane later; current presets still store one mono IR. Existing
schema v2/v3, effect IDs, parameter IDs and oversampling defaults are unchanged.
Stereo input preference is separate from DSP presets and defaults off when absent.

The Standalone always prepares two lanes. Mono input duplicates **before** DSP;
Stereo input pair selects adjacent physical inputs, enabling channels through the
existing stopped-device setup when needed. Switching modes uses the same 5 ms route
fade as input switching. Missing R is silence. Output slots alternate L/R; a one-slot
device receives (L+R)/2. Aggregate meters show the maximum of both lanes and either
lane can trigger CLIP. Check mono fold-down when listening to the spatial effects.

Oversamplers are owned per lane, including their dry-alignment delays. Cabinet keeps
128-sample partitions; stereo doubles kernel/history storage and convolution work
but does not double latency. Full default chain latency at 48 kHz remains 128 samples
(2.67 ms) at 1x or 320 samples (6.67 ms) with Amp and Distortion both at 2x/4x. Wet
Delay/Reverb timing is an effect characteristic, not extra dry signal-path latency.

### v0.5 measurement and verification

Run `cpp/build/test_stereo profile` for the default nine-effect chain (without
Tremolo). Local Release measurements, 48k/128, 100 warmup and 2000 measured blocks:

| Lanes | Nonlinear factor | Mean us | Observed max us | Max / 2666.67 us deadline |
|---|---|---:|---:|---:|
| Mono | 1x | 18.92 | 29.54 | 1.11% |
| Mono | 2x | 88.01 | 208.29 | 7.81% |
| Mono | 4x | 152.07 | 194.71 | 7.30% |
| Stereo | 1x | 30.19 | 138.25 | 5.18% |
| Stereo | 2x | 169.50 | 244.46 | 9.17% |
| Stereo | 4x | 302.80 | 522.17 | 19.58% |

Observed maxima include scheduler noise and are not worst-case execution guarantees.
No hardware/embedded inference is intended. CSV: ignored `cpp/build/v05-profile.csv`.

`StereoCoreMatrix` covers all registered effects, three rates and oversampling modes,
mono equivalence/center, left/right-only isolation, dual-mono swap, reset/reprepare,
64/128/256/512 and irregular partitions, rate changes, non-finite input, known and
32768-tap Cabinet impulses, spatial parameter changes, routing/meter/mute, stereo
rollback and process allocation/free accounting. `StereoConcurrentControls` changes
shared atomic targets while active 4x stereo processing runs. The settings/preset UX
test also restores stereo input preference and compares both output planes after
an embedded-IR preset roundtrip. Existing tests and tolerances are retained.

Release v0.5 GUI opened with the user's v0.4 preset, 48k/128 and advancing callbacks.
Stereo input pair toggling kept callbacks active; it was returned to mono-input mode.
Output remains muted. This was output-only on built-in speakers: external stereo
input, audible stereo imaging and physical reconnect are not claimed verified.
The v0.4 CoreAudio microphone-opening limitation remains a P1 hardware acceptance
item. P2: linked stereo dynamics, adjustable spatial width and shared immutable IR
kernel storage; none is required for channel-safe processing. No new effects,
plugins, hardware code, Windows work or oversampling algorithm changes were added.

Final v0.5 regression (one complete Release run after implementation): CTest **23/23**,
pytest **42/42**, all six existing Python/C++ comparisons passed unchanged. ASan/UBSan
**21/21** non-long-run tests passed (`ASAN_OPTIONS=detect_leaks=0`); TSan **5/5**
concurrent tests passed. Debug and Release app builds succeeded. The existing two
long-run tests ran in the Release suite; sanitizer long runs were excluded. Logs
are ignored `cpp/build/v05-*.log`. No commit or push was performed.
