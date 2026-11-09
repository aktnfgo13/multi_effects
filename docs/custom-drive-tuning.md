# Custom Drive listening revision / minimal Quality Lab

This revision responds to the user's first guitar listening report: excessive Drive
loudness growth and conservative Tone/Crunch/Headroom/Grain ranges. All mappings remain
provisional. Numerical differences are not a claim of better subjective sound.

## Before changing the mapping

The initial topology is documented in `custom-drive.md` (historical baseline).
Drive moved input gain 0–30 dB, stage B gain 1–1.6 and the 70–150 Hz input HPF,
without output compensation. Tone moved pre/post high shelves ±2/±6 dB.
Crunch blended p=2/p=4 rounded curves with only 60% hardness in stage B.
Headroom moved rails 0.3–1.5 / 0.7–1.5, envelope input compression and recovery
120–30 ms, but attack was fixed 1.5 ms. Grain applied small dynamic bias, signed
memory and rectified residual. All three alter distortion amount indirectly, but
Crunch is an odd static knee, Headroom is level/history dependent, Grain introduces
even harmonics and memory. The original ranges were intentionally conservative.
The increasing excitation and fixed output gain explain Drive's loudness growth.

## Revised mappings (public IDs, defaults and ranges unchanged)

- Drive input gain: −6 to +36 dB; stage B distribution and input HPF retained.
  Output compensation is a separate `driveCompensationDb(d)` function:
  `10 - 14 * (1 - exp(-4*d))` dB for normalized Drive `d`.
  This fixed curve observes no signal, adds no auto-normalization/limiter/compressor,
  and combines with Level before the existing dry/wet mix. It cannot equalize every
  source, Headroom or Tone setting; loudness-neutral bypass is not guaranteed.
- Tone pre/post shelves: ±4/±12 dB, centered at 50. A separate Drive-dependent
  `-10*d*d` dB high-frequency excitation shelf reduces high-gain HF saturation.
  Existing 1.4/1.8 kHz split frequencies and 10 kHz output low-pass remain. Tone 50
  means neutral *Tone adjustment*, not a flat drive path. No narrow post LP hides aliases.
- Crunch: blend p=2 and p=8 smooth odd curves, at both stages; same small-signal slope.
  Reciprocal evaluation above unity prevents eighth-power overflow. Crunch does not
  increase Drive gain. Total THD need not increase with a harder knee: harmonic
  distribution/transfer shape and listening are the relevant comparisons.
- Headroom `h`: A rail `0.18+3*h*h`, B rail `0.5+1.5*h`, envelope compression
  `0.5*(1-h)^2`, attack 0.5–4.5 ms, release 200–30 ms. Low settings saturate
  earlier and retain gain reduction longer; high settings preserve more dynamics.
- Grain: stronger bounded bias (`0.18 + 0.08*env/(1+env) +
  0.12*memory/(1+abs(memory))`), stage B excitation mismatch up to 35%, and rectified
  memory residual coefficient 0.40. No noise, bit reduction, sample-rate reduction or
  intentional alias generator. Signed 8 ms / rectified 25 ms memory remains.
  An intermediate unbounded bias experiment caused static foldback and was rejected;
  the original input-level monotonicity assertion was retained, not relaxed.

Input HPF remains 70–150 Hz: no additional bass removal to disguise IMD. DC blocking
remains 12 Hz. Existing oversampler, latency alignment, stereo state, parameter
smoothing, registry, GUI, IDs, schema and AU/VST3 mappings are unchanged. Old presets
restore the same values, but Custom Drive's sound intentionally changes with this
revision. Other effect algorithms are untouched.

## Reproducible Quality Lab

```sh
cmake --build cpp/build --target custom_drive_benchmark
MPLCONFIGDIR=/tmp/multi-effects-mpl .venv/bin/python examples/benchmark_custom_drive.py \
  --output cpp/build/v08-tuning --steps 0,25,50,75,100
# Optional owned/licensed DI (first 60 seconds, channels preserved):
MPLCONFIGDIR=/tmp/multi-effects-mpl .venv/bin/python examples/benchmark_custom_drive.py \
  --di /absolute/path/guitar.wav --output cpp/build/my-di-sweep
# Optional before/after metrics:
# --baseline cpp/build/v08-tuning-before/metrics.json
```

No DI was found in the repository; nothing was downloaded. Synthetic picking is the
fallback. `raw/` contains actual float32 DSP output; `level_matched/` applies offline
RMS gain against the *same default render* for every file. Default sweep creates 25
files in each folder (Drive/Tone/Crunch/Headroom/Grain × 5 values). Oversampling is 4x
for listening sweeps. No peak limiting is applied; matched peaks may exceed 0 dBFS.
Keep playback gain low. Input/output silence is preserved. WAV processing here is
per-channel offline using fresh independent state, not the device callback.

Outputs: `report.md`, `metrics.json`, `sweeps.csv`, `listening_manifest.json`,
`quality.png`, `spectra.npz`, `profile.csv`, and raw/matched WAVs. Generated files stay
under the ignored build tree. Fractional `--steps` are supported. The report includes
RMS/peak/DC, up to 32 harmonic bins/THD, 60 Hz + 7 kHz sideband IMD, coherent 7013 Hz
alias residual, macro-level input curves, cleanup and recovery. The alias metric uses
one second steady data; it is not a universal perceptual score. THD is intentional.

A/B diagnostics include Crunch0/Grain100 vs Crunch100/Grain0 and Drive70 with
Headroom20/80, plus an extreme bright/hard/high-Grain/low-Headroom corner. Metrics include
picking crest and early/late windows; these synthetic probes cannot establish palm-mute
clarity, smooth/coarse perception or sustain with a real guitar.

State memory is unchanged: 3064 bytes/lane on this arm64 ABI, 9160 bytes tracked
prepared stereo-effect heap (warmed registry excluded). No new buffers or allocations.
The CPU profile is stereo 48 kHz / 128 samples (deadline 2666.67 µs), warmed then
2000 blocks, mean and observed max. This is a Mac baseline, not an embedded estimate.

## Validation and remaining listening decisions

See the generated report for measured before/after values. Related tests include
all-macro extreme silence recovery at 1x/2x/4x, non-folding input-level response,
allocation/free guards, stereo/raw WAV preservation, common offline RMS reference,
and sweep validation. Existing regression tests are retained.

The increased maximum saturation intentionally broadens harmonics, including potential HF
alias residual at extreme combinations. 1x is not an anti-aliasing quality mode. Maximum
Drive/Crunch with low Headroom needs 4x A/B listening; even 4x is not alias-free.
Headroom output-level difference, the fixed compensation curve, Tone endpoints,
Crunch audibility, Grain coarseness, and high-gain LF IMD remain provisional. Do not
infer quality from THD or treat the aggregate IMD number as a pedal ranking.

## Measured final candidate

At 48 kHz, 440 Hz sine peak 0.3, 4x, other controls at defaults:

| Drive | Old THD % | New THD % | Old RMS | New RMS |
|---|---:|---:|---:|---:|
| 0 | 1.19 | 0.59 | 0.06740 | 0.12245 |
| 25 | 5.66 | 2.66 | 0.14965 | 0.15096 |
| 50 | 17.90 | 16.47 | 0.23317 | 0.22571 |
| 75 | 29.31 | 33.08 | 0.27699 | 0.23844 |
| 100 | 35.80 | 41.29 | 0.29920 | 0.24423 |

Drive RMS span: 12.95 → 6.00 dB on this probe, not universal loudness matching.
Default 7013 Hz alias proxy 1x/2x/4x: −43.40 / −74.92 / −126.21 dBc.
Drive100: −16.22 / −37.08 / −67.01 dBc (previous −16.03 / −36.17 / −63.71).
Default IMD: −11.34 dBc vs previous −9.45; Drive100 IMD is +4.34 dBc vs previous
−1.22. This upper-range IMD increase is a remaining P1 listening/tuning concern,
not hidden by the improved alias measurement. Carrier-relative IMD also changes
when the new excitation shelf attenuates the HF carrier. No claim of equivalent
input excitation or subjective improvement is made. Extreme corner alias at 4x is
−35.84 dBc, IMD +4.25 dBc: maximum settings remain a stress case.

Crunch0/Grain100 vs Crunch100/Grain0: H2 −39.11 vs numerical floor (−199 dBc),
THD 19.71% vs 14.81%, IMD −6.52 vs −16.83 dBc. The macros have distinct measured
profiles; audibility still requires listening. Drive70 Headroom20/80: picking crest
8.43/10.03 dB and THD35.83/30.09%, with separate onset curves in metrics.json.
Default cleanup input fractions 1/.7/.5/.3/.1: THD16.47/9.67/5.52/2.83/.82%.
Maximum absolute steady DC across macro sweeps is 3.2e−10. Default amplitude response
remains monotonic. Short-time asymmetry/memory and bypass level differences still exist.

| Factor | Mean µs | Observed max µs | Max/deadline | Added samples |
|---|---:|---:|---:|---:|
| 1x | 9.123 | 19.584 | 0.73% | 0 |
| 2x | 61.406 | 106.500 | 3.99% | 64 |
| 4x | 116.606 | 184.167 | 6.91% | 64 |

64 samples = 1.333 ms at 48 kHz. State and prepared heap measurements are unchanged.
These measurements precede the concurrent final builds; scheduling still affects max.

Final regression (one complete run after implementation): Release CTest28/28,
pytest47/47, all six existing Python↔C++ comparison scripts, ASan/UBSan26/26
(non-long-run, leak detection disabled), TSan7/7 concurrent tests passed. Core-only,
Standalone Debug/Release, AU and VST3 built. `auval -v aufx Mfx6 Mefx` passed for the
new installed AU; its executable hash matches the build. Previous installed AU is
backed up at `cpp/build/v08-tuning-previous-installed.component`.
The already-running Standalone/Logic sessions were not closed or reloaded; restart
when ready to listen to the new binaries. No new native-host listening result is claimed.
No commit or push was performed.

Remaining priorities: no new crash/allocation/state P0 found; P1 high-gain IMD,
extreme-corner alias and real DI/palm-mute listening; P2 refine compensation,
Tone endpoints and macro taper after feedback. No further effects/features were added.
