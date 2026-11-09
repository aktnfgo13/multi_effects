# Custom Drive comparison candidate: reversed Headroom / texture-only Grain

The user explicitly selected **actual Headroom direction inversion for listening**
after being informed that this also reverses the measured cleanliness/crest direction.
This candidate follows that selection. It does not claim to simultaneously satisfy the
contradictory earlier physical HIGH=cleaner requirement. Actual guitar listening is
required to approve the new perceptual direction.

## Headroom

`internal h = 1 - public Headroom/100`. Both thresholds, envelope compression,
attack/release and the existing deterministic compensation use that same h. No UI-only
label swap, new parameter, migration or realtime normalization was added. Compared at
Grain25, new public0 matches previous public100 and vice versa; midpoint50 is unchanged.
Preset schema and automation IDs remain identical; stored non-midpoint Headroom values
intentionally sound like the opposite previous position. Existing presets are not rewritten.

Consequently, measured public0 is the cleaner/higher-crest endpoint and public100 is
the more compressed endpoint in this explicitly requested comparison. The prior physical
assertions were changed to test the mirrored mapping, not loosened. The legacy
`--check-macros` analyzer mode still checks the former direction; use `--check-texture`
for this candidate. Passing its checks is not a subjective success determination.

## Grain / frozen Crunch

Crunch's p=2/p=8 curve, knee `Arail*(3-2*c)`, excitation `stageBGain*(1+1.05*c)`,
taper and compensation are unchanged. Drive and Tone expressions are unchanged.
Grain's former gross B-threshold, contribution and HF-damping modulation are removed:

- B rail multiplier fixed at `1-0.45*0.25`.
- Serial contribution fixed to approved reference Grain25:
  `1-(1-0.85*c)*(1-0.98*0.25)`; only Crunch moves it.
- Interstage LP fixed to the same reference Grain25:
  `11000-3500*0.25-3500*c` Hz, Nyquist capped; Grain adds no treble boost/cut.

A is dominant at low Crunch (75.5%); at high Crunch B dominates as before. Grain no
longer changes that ratio. Its axis is bias and short-time history, rather than clipping
amount or stage weighting. Changes at Grain other than25 are intentional; freezing
Crunch does not mean retaining the old Grain endpoint sounds.

For normalized public Grain g, bias/residual strength q is g up to0.25, then
`0.25+0.4*(g-0.25)` (maximum0.55). B bias is
`q*(0.9+0.18*env/(1+env)+0.4*memory/(1+abs(memory)))` and rectified residual0.65*q.
Bias is bounded and centered by subtracting the curve's bias-only output; existing12Hz
DC blocking remains. No noise, bitcrusher, new nonlinear stage or HF boost is used.
Signed-memory time is2–26ms; rectified-memory time10–25–40ms across0/25/100.
At public Grain25 these exactly recover the previous8/25ms operating point.

This rotates even/odd and low/upper harmonic distribution without deliberately raising
main knee hardness or gross threshold. Some harmonics decrease while others increase;
not every upper harmonic should rise together. Total THD is a guard, not a quality score.
Memory coefficients are prepared/updated from smoothed controls, with no process allocation.

## Freeze and focused validation

An archived pre-change executable was compared with the candidate over75conditions:
1x/2x/4x × Crunch0/25/50/75/100 × Headroom0/25/50/75/100, Grain25, matching the old
Headroom to100-new. Maximum sample difference1.49e−7. Results are stored in
`cpp/build/v08-macro4-freeze/report.json`. This checks the approved Crunch reference
and full Headroom inversion, not just metadata or parameter targets.

C++ tests include seven retained waveform fixtures at2x, the mirrored Headroom weak/
strong response, live propagation, stereo independence, silence/reset/nonfinite handling
and allocation/free guards. Fixture tolerance2e−6 is newly defined for retained float
outputs; existing numeric tolerances were not widened. Python adds an even/odd energy
metric sanity check with known second/third harmonics, excluding the fundamental.

The previous broad-macro waveform-size check (`--check-macros`, Grain>0.2) is retained
for prior designs; it is not the success criterion for this texture-only revision.
`--check-texture` instead requires Grain endpoint steady RMS within1dB, THD ratio<1.2,
H2 redistribution>15dB and H5 change>3dB, plus the explicitly reversed Headroom dynamic
and average-level bounds. These named design changes are not hidden tolerance relaxations.

## Rendering / analysis

```sh
cmake --build cpp/build --target custom_drive_benchmark
MPLCONFIGDIR=/tmp/multi-effects-mpl .venv/bin/python examples/benchmark_custom_drive.py \
  --mode 2 --check-texture --output cpp/build/v08-macro4
```

Existing synthetic weak/medium/strong transients and steady probes are reused. No DI
was found/downloaded. Optional `--di` supports owned/licensed audio. Raw and common-RMS
matched folders each contain29float WAVs, including Headroom/Grain endpoints and both
Crunch/Grain cross-comparisons. Float outputs are not peak limited; lower playback gain
where necessary. `metrics.json` now records per-burst RMS/peak/crest and Grain recovery,
and the report includes H2/H3/H5/H7 and even/odd energy ratio excluding the fundamental.

No new framework, GUI controls, IDs or schema. Existing user changes are preserved.

## Final results (2x unless specified)

Headroom, Drive70, first weak/medium/strong burst (input peaks0.06/0.2/0.6):

| Public Headroom | Burst | RMS | Peak | Crest dB |
|---|---|---:|---:|---:|
| 0 | weak | 0.01529 | 0.14669 | 19.64 |
| 0 | medium | 0.04196 | 0.28852 | 16.75 |
| 0 | strong | 0.09277 | 0.54887 | 15.44 |
| 100 | weak | 0.04506 | 0.21502 | 13.57 |
| 100 | medium | 0.06843 | 0.24733 | 11.16 |
| 100 | strong | 0.08551 | 0.26712 | 9.89 |

Whole sequence RMS0.05945/0.06837 (+1.21dB), crest19.31/11.84dB. This is **the
intentional reversal** chosen by the user, not proof of physical HIGH=cleaner.
Weak-input coherent harmonic ratio at public0/100 is1.62%/21.94%; public0 strong
input15.64%. Compensation is unchanged mathematically and mirrored together with
threshold/envelope; individual weak-picking levels differ even though sequence RMS
is close. No signal-dependent volume matching is applied in realtime.

Grain endpoints at fixed Drive/Tone/Crunch/Headroom:

| Grain | RMS | THD % | H2 dBc | H3 dBc | H5 dBc | H7 dBc | even/odd dB |
|---|---:|---:|---:|---:|---:|---:|---:|
| 0 | 0.23752 | 13.608 | numerical floor | −17.61 | −29.48 | −45.67 | numerical floor |
| 100 | 0.21955 | 14.072 | −20.45 | −20.41 | −35.68 | −43.85 | +0.40 |

Steady RMS change−0.68dB, THD ratio1.034. Dynamic sequence RMS difference−1.61dB;
RMS-matched waveform difference/reference0.0934. This is deliberately less overall
waveform change than macro3's0.4639: the new target is harmonic distribution at similar
THD, not more clipping. Whether this is a sufficiently audible and distinct texture
requires listening. The prior amount-based guard remains documented and is not claimed
to pass for this different design criterion.

Grain recovery5ms probe: gain vs rested−3.01/−2.41dB; after matching probe RMS,
waveform residual0.0988/0.4368 for Grain0/100. At20ms residual0.0449/0.2005, and
250ms0.00446/0.00535. Envelope compression remains common; the changed signed/rectified
memory adds a short transient shape difference, not sustained gain control.

Crossed Crunch0/Grain100 vs Crunch100/Grain0: THD4.37/27.97%, H2−31.18dBc vs numerical
floor. One remains soft with even-harmonic texture; the other remains hard/odd-dominant
in these probes. No subjective identity claim is made.

Grain0/100 IMD−10.05/−9.72dBc; alias proxy−64.78/−62.14dBc (2x). Default alias
1x/2x/4x−40.11/−66.38/−108.90dBc and default IMD−9.95dBc match the frozen midpoint.
Maximum steady DC across sweeps5.53e−10. All-extreme combination still has IMD+3.65dBc,
alias4x−43.98dBc: real-listening P1, not a clean/distortion-free claim. Comparisons
with old extreme Headroom settings must account for the intentionally mirrored knob.

## Single final CPU / regression

48k/128stereo,100warmup+2000measured blocks, deadline2666.67µs:

| Factor | Mean µs | Observed max µs | Max/deadline | Added samples |
|---|---:|---:|---:|---:|
| 1x | 8.781 | 17.459 | 0.65% | 0 |
| 2x | 58.216 | 81.792 | 3.07% | 64 |
| 4x | 109.798 | 154.167 | 5.78% | 64 |

No significant mean CPU increase observed. These are steady-control Mac measurements,
not embedded estimates or worst-case bounds. Grain movement recalculates memory
coefficients. Lane3064bytes and prepared stereo effect heap9160bytes unchanged.
Processing allocation/free0; no new locks/I/O/logging/resize. Stereo and live GUI/host
parameter propagation regression pass.

Final full regression once: CTest29/29, pytest49/49, all six legacy Python↔C++ scripts,
ASan/UBSan27/27(non-long-run, leak detection disabled), TSan7/7 passed. Core-only,
Standalone Release, AU/VST3 built. Previous installed AU backed up to
`cpp/build/v08-macro4-previous-installed.component`; new AU installed and auval passed.
Installed executable hash matches the build. Host sessions and unsaved user edits
were preserved; restart when ready to load the new binary. Preset/state/automation
IDs and schema remain compatible, with intentionally changed Headroom/Grain sound.
No commit/push; no additional features started.

Next listening: actual Headroom direction vs the previous candidate; matched Grain
texture at fixed Crunch; crossed soft-textured vs hard-cohesive presets; transient
pumping, low-end smear and extreme fizz. No new automated P0 found. Perceptual axis
separation and Headroom acceptance remain P1 until the user approves real listening.
