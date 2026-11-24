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
