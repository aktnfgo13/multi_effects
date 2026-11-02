# Prototype stability regression (base: 5414f8d)

Scope: validation and malformed-input rejection only. DSP equations, default tone,
GUI layout and valid preset format are unchanged. Preset and WAV IR services were
moved from MainComponent into app/PresetIO so GUI and tests use identical code.
This remains an application-layer JUCE dependency, not a DSP Core dependency.

## Run

With JUCE app targets enabled and configured as documented in standalone-prototype.md:

```
cmake --build cpp/build --parallel 4
ctest --test-dir cpp/build --output-on-failure
.venv/bin/python -m pytest -q
.venv/bin/python examples/compare_cpp_python_gain.py
.venv/bin/python examples/compare_cpp_python_distortion.py
.venv/bin/python examples/compare_cpp_python_filters.py
.venv/bin/python examples/compare_cpp_python_compressor.py
.venv/bin/python examples/compare_cpp_python_noise_gate.py
.venv/bin/python examples/compare_cpp_python_prototype.py
```

PresetIRRoundtrip is available with MULTI_EFFECTS_BUILD_APP=ON, but does not create
a GUI or open an audio device. It writes uniquely named fixtures beneath the CTest
working directory and removes only its own temporary directory on exit.
Core-only tests also work with the app option OFF. `ctest -LE long` excludes the
10-minute-per-rate simulation; `test_stability long 60` runs a shorter version.
Duration is simulated audio time, not wall-clock duration.

## New tests

- PresetIRRoundtrip: generated stereo 24-bit WAV at 48 kHz; channel averaging,
  DC removal and peak normalization; processing; JSON save and reload; independent
  instances including two delays; exact IDs, names, order, bypass, float parameters,
  embedded IR samples and source rate. Original WAV is deleted before reloading:
  presets embed IR content, not an external file reference. 25 disk roundtrips and
  all 12 rate/block combinations produce exact matching output. Default cabinet,
  empty chain, supported v2 input and failed-load preservation are checked too.
- BlockSampleRateMatrix: 64/128/256/512 at 44.1/48/96 kHz, against a single continuous
  reference. Includes irregular partitions, partial final blocks, zero-sized block,
  and changed sample-rate preparation on reused objects. Output is exactly equal.
- ChainStress: 1000 add/remove/reorder/prepare cycles, bypass and parameter changes,
  unique identity preservation, invalid index rejection, empty chain, and 50 complete
  construct/prepare/process/destroy lifetimes without retained C++ allocations.
- ConcurrentParameterUpdates: 10000 blocks while a separate writer changes supported
  atomic parameter/bypass controls. Structural edits are deliberately performed only
  with processing stopped, matching the application's ownership contract.
- OfflineLongRun: full ten-effect chain, 600 simulated seconds per rate, rotating all
  four block sizes, bursts/silence/DC/impulses, periodic bypass and mix updates.
  Checks finite output, non-silent/bounded signal, no hidden non-finite output guard
  activations, allocation/deallocation and live memory, plus macOS resident memory.

The engine's existing non-finite-to-zero guard now increments invalidSampleCount;
read this diagnostic only after processing has stopped. Valid processing and invalid
sample replacement are unchanged. Reset clears the diagnostic.

## Results on local AppleClang/macOS

- Release CTest: 13/13; Python pytest: 42/42; all six comparison scripts pass.
- 12 rate/block combinations: exact fixed/irregular partition and reprepare equality.
- Long-run: 26,460,000 / 28,800,000 / 57,600,000 samples at 44.1/48/96 kHz respectively.
  Total 112,860,000 samples (30 simulated minutes). Each rate: zero processing C++
  allocation/free, zero live-byte/block increase and zero observed RSS growth after
  warmup; no NaN/Inf or guard activations. Output peak approximately 0.480.
- AddressSanitizer + UndefinedBehaviorSanitizer: matrix, stress, concurrent controls
  and preset/IR tests pass, no reported invalid access/undefined behavior.
- ThreadSanitizer: concurrent parameter/bypass test passes with no reported race.
- Standalone Release is rebuilt against the shared IO functions.

Sanitizer builds use the native allocator to preserve sanitizer redzones; allocation
accounting runs separately in Release. The custom tracker covers replaceable C++
new/delete, including arrays, aligned and nothrow forms, not direct malloc/OS calls.
RSS is sampled periodically and allows a 32 MiB post-warmup growth ceiling. Neither
these checks nor finite outputs prove every internal DSP state is mathematically
valid or guarantee audio-device deadlines. LeakSanitizer is unsupported on this
macOS runtime; do not claim an LSan pass.

Sanitizer recipe (build directories remain ignored): configure a separate directory
under cpp/build with CMAKE_CXX_FLAGS and CMAKE_C_FLAGS set to
`-fsanitize=address,undefined -fno-omit-frame-pointer`, and CMAKE_EXE_LINKER_FLAGS set
to `-fsanitize=address,undefined`. Use the already downloaded JUCE source via
FETCHCONTENT_SOURCE_DIR_JUCE. Build test_stability and test_preset_ir, then run the
four named tests with ASAN_OPTIONS=detect_leaks=0 and UBSAN_OPTIONS=halt_on_error=1.
For a separate core-only ThreadSanitizer build, use `-fsanitize=thread` and run
`test_stability controls` with TSAN_OPTIONS=halt_on_error=1.

## Issues found and fixed

Embedded IR strings were silently coerced to zero, and invalid IR source rates were
not rejected until preparation. The parser now validates numeric type, finite float
representability and source rate immediately, along with typed structural fields.
Malformed files cannot be committed to the live chain. WAV channel/rate constraints
are checked before allocation. Normal mono/stereo WAV processing is unchanged.
The Gain comparison now fails its process when allclose is false.

An initial long-run test incorrectly counted its own error-message temporary string
as live growth; assertion helpers now accept string literals without allocating.
JUCE's default temporary location mapped to Library/Caches; test fixtures now use
the build working directory instead. These were test-harness issues, not DSP changes.

## Before listening

Use an instrument/Hi-Z input and headphones; select the intended input/output and
sample rate. Start muted with low interface/output gain, then unmute. Only the first
active input is processed and copied to outputs. Avoid double monitoring through the
interface and app. Chain edits/preset/IR changes intentionally interrupt processing
and reset tails. Default cabinet is synthetic. Each cabinet adds 128 samples latency;
IR length after resampling must not exceed 32768. Start at 512 samples and reduce to
256/128/64 while checking audible dropouts. Offline results do not validate physical
round-trip latency, clipping in the interface, feedback, or subjective tone.
