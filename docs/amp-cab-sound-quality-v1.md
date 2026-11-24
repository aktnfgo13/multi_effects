# Amp/Cab sound quality v1 — 2026-11-24

Baseline: 4bf41fd. Custom Drive and visualization algorithms unchanged; installed AU untouched.

## Finding and correction

Cabinet uses 128-sample uniform partitions (256-point FFT), 128-sample latency, up to 32768 taps and independent stereo histories. IR preparation uses offline windowed-sinc resampling. It previously preserved audio waveform amplitude rather than convolution-kernel integral: a smooth unit-sum 48 kHz IR measured steady gain 1 at 48 kHz but 0.91875 at 44.1 kHz. Multiplying resampled coefficients by source/destination rate corrects this. The new test measures gain 1 at 44.1/48/96 kHz. This is sample-rate compensation, not peak normalization. Same-rate IRs, IDs, schema and latency are unchanged. Existing cross-rate presets intentionally lose the erroneous rate-dependent gain; they may need listening-level adjustment. Edge-truncated or very short IRs retain the existing interpolation boundary limitations.

Amp is an input high-pass, input gain, two offset-compensated tanh stages, bass/mid/treble filters, power tanh, presence shelf, output gain and low-pass. Preamp/power oversampling is already supported independently of the chain. Plugin tone-stack gains are fixed at zero; it is not a physical amp tone stack. Bias subtraction makes silence zero but does not remove signal-dependent DC. No voicing or new DC filter was introduced without listening approval.

## Measurements

48 kHz, 2x, steady 1 kHz sine. Entries are RMS / peak / THD percent. Weak input peak .02 (RMS .014142); strong peak .4 (RMS .282843).

| Preamp dB | Weak | Strong |
|---|---|---|
| 0 | .013982 / .019766 / .0571% | .203325 / .262710 / 9.877% |
| 12 | .054734 / .077135 / .615% | .298008 / .325944 / 30.943% |
| 24 | .177169 / .235101 / 7.106% | .315436 / .365724 / 39.974% |

The same matrix ran at 44.1/96 kHz; raw results: `cpp/build/amp-cab-v1/amp_metrics.json`. These are steady sinusoidal dynamics, not a measured picking transient response. Largest absolute DC in the displayed matrix is .001243. Amp output is unchanged by this fix.

Reused `examples/benchmark_nonlinear.py`: Amp 7013 Hz alias residual is -13.95/-21.38/-32.81 dBc for 1x/2x/4x, matching the previous baseline. This is a signal-specific proxy, not a general sound-quality score. Mono 48k/128 Amp mean/max processing times: 5.77/25.29, 53.96/96.00, 100.44/136.58 microseconds. No stereo/device performance claim is made.

Focused tests: EngineTest, BlockSampleRateMatrix, StereoCoreMatrix, OversamplingQualityFoundation, CustomDriveFoundation, AnalysisTapSpectrum, PresetIRRoundtrip: 7/7 PASS. Coverage includes direct FIR convolution, sample-rate/stereo, reset/partition continuity, finite output, callback allocation checks, presets/IR and analysis invariance. Full regression and host builds were not repeated; installed AU remains the previous build.

Comparison sine WAVs (float, raw and input-RMS matched) are in `cpp/build/amp-cab-v1/`. They compare Amp on/off at 12 dB preamp, not an altered Amp algorithm or real guitar performance. Listening approval remains outstanding. Follow-up: guitar transient response, tone-stack response curves, and DC across the full parameter range; avoid changing approved Custom Drive while assessing Amp.

## Amp v1.1 diagnostics — 2026-11-24 (baseline 917e8be)

No DSP coefficients, algorithms, preset schema or latency changed. New `test_amp_quality` and `examples/benchmark_amp_v11.py` make the focused diagnostics reproducible. The DI script requires the existing local `cpp/build/headroom_logic_analysis/input_24_27.wav` asset (not distributed). Tone controls are not public Engine parameters: internal filters were probed at -12/0/+12 dB, a diagnostic range, not claimed product min/max.

At 48 kHz, +12 dB filter responses at 80/250/1000/3000/6000 Hz:

| Filter | 80 | 250 | 1000 | 3000 | 6000 |
|---|---:|---:|---:|---:|---:|
| Bass | 11.416 | 2.875 | .0169 | .0002 | .0000 |
| Mid | .249 | 2.217 | 10.326 | 1.622 | .397 |
| Treble | .0000 | .0004 | .0998 | 4.383 | 10.648 |

The corresponding negative-gain curves were measured too; neutral gain is unity within .001 dB (numerical identity check, not a voicing criterion). Maximum rate spread across 44.1/48/96 kHz at these probes: .1585 dB. No obvious calculation defect was identified; bilinear warping explains rate dependence. Raw records: `cpp/build/amp-v11/diagnostics.csv`.

DI playback processing, isolated Amp with 12 dB preamp, 2x: input peak .02/.4, input RMS .002959/.059185; output peak .072734/.325590, RMS .011404/.143148. Input increase 26.02 dB gives output RMS increase about 21.97 dB. Gain in the 20 ms maximum-energy region is 11.718/4.406 dB; in a later 100–300 ms region 11.732/6.535 dB. These are scaled copies of the same recorded performance, not two actual picking-strength performances. They indicate greater compression around the strong transient; they do not prove a particular amp feel. DI THD is not inferred from a non-periodic signal; sine THD remains in the preceding section.

Across 243 public-parameter corner/midpoint combinations (gain 0/15/30, drive .1/2.05/4, output -36/-15/6; three rates, three factors), no NaN/Inf occurred; silence settled below 1e-12 after .5 seconds, and reset produced exact zero. Allocation/free counts were zero under the existing C++ allocation tracker. Signal-dependent DC is NOT approved as acceptable: maximum absolute sine mean .0709692 at 48k/2x/gain30/drive4/output+6; maximum peak 1.65827 at 44.1k/4x, approximately +4.39 dBFS. Positive output gain permits float headroom; no automatic clamp was added. Strong DI mean was -.011962 (weak: -.0000121). Asymmetric saturation and asymmetric DI both contribute; the silence result excludes a persistent zero-input offset. A post-nonlinear DC blocker requires before/after low-frequency/transient listening and is deferred, not silently declared unnecessary.

Aliasing and CPU reuse the immediately preceding v1 measurement because Amp DSP is unchanged: 1x/2x/4x residual -13.95/-21.38/-32.81 dBc at coherent 7013 Hz, .5 peak, preamp24; latency 0/128/128 samples. Mono 48k/128 mean/max microseconds 5.77/25.29, 53.96/96.00, 100.44/136.58. These are offline timings, not host callback guarantees.

Focused EngineTest, ConcurrentParameterUpdates, OversamplingQualityFoundation, OversamplingConcurrentControls, StereoCoreMatrix: 5/5 PASS; the new diagnostic executable also passed numerical/identity/allocation assertions. DC magnitude and peak magnitude are measurements, not pass thresholds. Existing transition tests cover lifecycle and finite output, not perceptual click/pop approval. No new click metric, device/Logic test, full build, or global regression was performed.

Raw and input-RMS-matched DI WAVs: `cpp/build/amp-v11/di_*x_raw.wav` and `di_*x_matched.wav`; reference input WAVs are alongside. They compare existing oversampling modes and controlled input levels, not a revised Amp sound. Listen for low-note transient/sustain behavior, high-note aliasing, and residual low-frequency offset. Next priority is a separately evaluated DC-removal candidate, then transition click measurements. No new Amp sound is approved by this diagnostic cycle.
