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
use headphones to avoid acoustic feedback, then clear Mute. First enabled input
is processed as mono and duplicated to active outputs. Output gain is -6 dB and
final output clamps to +/-1. Add/remove/reorder effects using the left panel.
Controls are generated from the Core registry's parameter metadata. Cabinet defaults
to a generated HP/LP impulse (not a captured commercial cabinet); Load cabinet IR
accepts a user-owned WAV. The loader averages channels, removes DC and normalizes
peak. Resampling uses an offline windowed-sinc implementation, not SciPy's exact
resample_poly algorithm. Max IR: 32768 samples including resampled length.

## Architecture and ownership

`effects/` contains algorithms. `engine/Engine` contains JUCE-independent Effect,
Parameter, registry and Chain. The app owns device/GUI/file dialogs and JSON IO.
No external cabinet assets or models are bundled.

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
