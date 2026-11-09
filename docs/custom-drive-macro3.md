# Custom Drive macro range / Headroom semantics candidate 3

Responds to real isolated-guitar feedback: Crunch/Grain are now audible but too subtle;
Headroom feels reversed. Changes are confined to those three macros, existing analysis
checks, tests and documentation. Drive input gain, Drive compensation, input HPF,
Tone pre/post shelves, Level/Mix, public IDs, schema and oversampling remain unchanged.
The resulting sound can change at identical Drive/Tone values because their downstream
nonlinear operating points changed. No subjective success is claimed before listening.

## Headroom investigation and mapping

The prior code did **not** contain an inverted public value: increasing Headroom raised
both thresholds, reduced envelope compression, and produced higher crest and cleaner
weak input on the previous synthetic probe. Thus the reported perceptual reversal is
not proven to be an inverse-mapping bug. Its cause remains unconfirmed. Static output
compensation and signal-dependent stage interaction can affect the perceived direction;
blindly substituting `1-h` would violate the explicit HIGH=cleaner/transient-preserving
success conditions. The UI is not inverted.

Actual internal changes for normalized h:

- A threshold: `0.93 * 32^(h-0.5)`, LOW0.1644 → HIGH5.2609.
- B threshold: `1.25 * 8^(h-0.5)`, LOW0.4419 → HIGH3.5355.
- Input envelope compression: `0.8*(1-h)^2`; HIGH has zero such compression.
- Attack0.2–8.2ms; recovery120–30ms. LOW reacts faster and recovers more slowly.
- Deterministic output compensation remains `(0.93/Athreshold)^0.53`, now following
  the new logarithmic threshold mapping. This is a parameter-only curve, not RMS/peak
  normalization, a limiter or an added compressor. It limits average differences on
  the transient probe, not on all possible guitars/settings.

The reference is Drive70. Weak-input THD at−24dBFS must be lower at HIGH than LOW;
strong-input THD at−6dBFS must exceed HIGH's weak-input THD. Tests also require higher
crest and greater strong/weak transient peak ratio at HIGH, and endpoint RMS difference
under3dB. This checks threshold/onset direction, not just the control label.

## Expanded Crunch / Grain

Crunch retains p=2↔p=8 clipping but A knee multiplier is now3→1 (previous2→1),
B excitation multiplier1→2.05 (previous1→1.75), and B contribution from Crunch alone
0→85% (previous0→60%). The wider soft knee, different curve and serial interaction
change clipping shape; Drive gain is untouched.

Grain's B rail mismatch reaches45% (previous40%), bounded bias coefficients are
0.9 /0.18 /0.4 for constant/envelope/signed memory (previous0.45 /0.12 /0.25),
rectified-memory residual0.65 (previous0.45), and contribution from Grain alone
0→98% (previous0→95%). Constant bias is subtracted through the same clipping curve;
DC blocking remains. No noise, treble boost or sample-rate reduction is added.

Combined contribution: `1-(1-0.85*c)*(1-0.98*g)` (maximum99.7%). Crunch owns knee
and excitation; Grain owns threshold mismatch, asymmetry and memory. Both influence
the serial contribution, so they are coupled but not identical macros. All remain
inside the existing oversampling island.

To restrain extreme HF excitation, existing interstage LP moves to
`11000 - 3500*g - 3500*c` Hz (Nyquist capped). It is not a change to the Tone mapping.
The coefficient cache now includes Crunch as well as Grain. Engine20ms smoothing
remains; no buffers or allocation were added. Fixed-state memory and latency remain
unchanged. Extra coefficient recalculation while moving Crunch is a CPU consideration.

## Reproduce / listen

```sh
cmake --build cpp/build --target custom_drive_benchmark
MPLCONFIGDIR=/tmp/multi-effects-mpl .venv/bin/python examples/benchmark_custom_drive.py \
  --mode 2 --check-macros --output cpp/build/v08-macro3
```

The existing tool produces raw and common-reference RMS-matched WAVs, including
`crunch_000/100`, `grain_000/100`, `headroom_000/100`,
`crunch000_grain100`, `crunch100_grain000`, and named Drive70/Headroom0/100 pairs.
Use the named Drive70 pair for the dynamics comparison; plain Headroom renders use
Drive50. Float WAVs are not limited: reduce playback gain if necessary.

No DI was downloaded. Synthetic weak/medium/strong harmonic transients are the same
as candidate2 so measured endpoint ratios can be compared. `metrics.json` includes
amplitude-dependent THD, transient peak/crest, recovery and harmonics. The original
macro checks are retained; new checks explicitly verify weak-input cleanliness,
strong-input onset and crest direction. A C++ allocation-tracked coherent harmonic
regression independently checks Headroom LOW/weak, HIGH/weak and HIGH/strong output.

Previous header/executable preserved in ignored build files:
`cpp/build/CustomDrive-before-macro3.h`, `cpp/build/custom_drive_benchmark_before_macro3`.
No previous work, presets or user changes were restored or removed.

## Final candidate measurements

2x, the same weak/medium/strong transient sequence and common-reference offline RMS
matching as candidate2:

| Macro | Previous matched waveform delta/reference | New | Ratio | Raw endpoint RMS difference |
|---|---:|---:|---:|---:|
| Crunch | 0.23931 | 0.3644 | 1.52x | +3.93dB |
| Grain | 0.30736 | 0.4639 | 1.51x | −6.79dB |
| Headroom (Drive70) | 0.44462 | 0.4996 | 1.12x | −1.21dB |

These are waveform-difference ratios, not a measured1.5x increase in perceived
contrast. Grain's raw gain reduction is substantial: compare matched renders before
judging texture. The endpoint is retained for listening rather than silently narrowed.

Headroom LOW/HIGH transient RMS0.06837/0.05945, peak0.2671/0.5489, crest11.84/19.31dB.
Weak attack peak0.2150/0.1467; strong attack peak0.2503/0.5489 (strong/weak ratios
1.16/3.74). C++ steady harmonic ratio at weak−24dBFS input is21.94%/1.62%; HIGH at
strong−6dBFS gives15.64%. The new tests verify LOW break-up earlier, HIGH cleaner on
weak input, and HIGH saturating progressively on stronger input. At5ms recovery gap,
LOW quiet-probe gain vs rested is−7.57dB and HIGH+0.43dB; at250ms−0.60/about0dB.
The small HIGH residual is filter/memory history, not a separate gain normalizer.

Crunch0/100440Hz THD4.58/28.12%; Grain0/10010.20/32.27%.
Crossed Crunch0/Grain100 vs Crunch100/Grain0: THD25.56/25.71% but H2−12.85dBc vs
numerical floor. Similar total harmonic energy does not imply the same texture.

Default IMD−9.95dBc vs preceding−12.05dBc (approximately2.1dB higher). Crunch100
−1.66dBc, Grain100−1.88dBc, all-extreme+5.21dBc remain P1 listening concerns.
Default alias proxy1x/2x/4x−40.11/−66.38/−108.90dBc; default2x is about3.9dB higher
than candidate2. Drive100−22.54/−43.08/−67.15dBc; all-extreme4x−52.49dBc improved
from−35.15dBc. These are stimulus-specific tradeoffs, not a claim of no alias/IMD
regression or better subjective sound. Steady DC across sweeps is at most5.53e−10.
No numerical instability, state growth, or failed silence/overload/reset guard found.
Pumping, low-end smear and real-world harshness cannot be cleared by these tests alone.

Raw and matched folders each contain29float WAVs under `cpp/build/v08-macro3/`.
Reports: `report.md`, `metrics.json`, `sweeps.csv`, `profile.csv`, `quality.png`.

## Single final performance / regression run

48k/128stereo,100warmup+2000measured blocks, deadline2666.67µs:

| Factor | Mean µs | Observed max µs | Max/deadline | Added samples |
|---|---:|---:|---:|---:|
| 1x | 8.657 | 21.667 | 0.81% | 0 |
| 2x | 60.702 | 175.125 | 6.57% | 64 |
| 4x | 112.332 | 215.916 | 8.10% | 64 |

No significant mean CPU increase observed; runs are scheduling dependent, not an
embedded estimate or worst-case guarantee. Moving macros incurs coefficient updates
not captured by this steady-control timing. Lane3064bytes/prepared stereo heap9160
bytes remain unchanged. Audio processing allocation/free remain0, stereo independence
and live macro output changes pass.

Final full regression once: CTest29/29, pytest48/48, all six existing Python↔C++
comparisons; ASan/UBSan27/27(non-long-run, leak detection disabled), TSan7/7 passed.
Core-only and Standalone Release builds, AU/VST3 builds succeeded. The new AU was
installed after backing up the previous installation to
`cpp/build/v08-macro3-previous-installed.component`; auval succeeded, installed/build
executable hashes match. Existing host sessions were not restarted; save and restart
to load the new binary. Parameter IDs/schema/state/automation/IR regression passed.
No commit/push. No additional features started.

Remaining: P0 none found automatically. P1 perceptual Headroom-direction acceptance,
matched Crunch/Grain contrast, high-Grain raw level reduction and endpoint IMD/alias.
P2 intermediate taper/range refinement only after listening. The prior perceptual
reversal's cause remains unconfirmed; do not describe this as a proven flipped-ID fix.
