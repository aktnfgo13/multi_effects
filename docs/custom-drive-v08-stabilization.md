# Custom Drive v0.8 stabilization checkpoint

This checkpoint preserves the existing HeadroomPick and Bias/Symmetry sound. It is not listening approval. No installed AU is replaced.

## Interaction criterion

No repository test or current design contract requiring **all Headroom endpoint RMS differences below 1 dB** was found in the Custom Drive tests, benchmark or design documents. `custom-drive-macro4.md` assigns a 1 dB steady-state criterion to **Grain**; `benchmark_custom_drive.py` has opt-in historical Headroom tuning checks with a 3 dB limit and historical direction/crest expectations. These are stimulus/model-specific checks, not universal loudness normalization requirements. None was widened or removed to manufacture a pass.

The previously reported 4.06–5.86 dB Headroom interaction span (baseline 3.87–5.85 dB) is therefore a gain-staging/listening concern, not by itself a processing regression. This session did not recover the exact earlier interaction stimulus, so those numbers remain user-supplied context. An explicit independent repeat uses a 48 kHz 440 Hz sine, peak 0.437805, Drive 25/50/75, H0/50/100, Grain50, Tone/Crunch/Tight/Mid50, Bias/Symmetry pairs 0/100, 50/50, 100/0, Level−6, Mix1, OS2x. RMS is measured over the last 0.5 s of a 1 s render.

| Drive | Baseline H100−H0 RMS range | Candidate range |
|---|---:|---:|
| 25 | 3.238–3.448 dB | 3.355–3.545 dB |
| 50 | 5.305–5.475 dB | 5.446–5.595 dB |
| 75 | 3.054–3.295 dB | 3.137–3.383 dB |

Headroom changes thresholds and dynamic attenuation; existing fixed compensation is not an AGC and cannot normalize arbitrary input/Drive/macro combinations. The raw level difference must be reported, and listening comparisons must also be RMS matched. The existing level-matching metric tests remain intact. Reproducible local measurements are in `cpp/build/headroom_candidate/stabilization_measure.py` and `stabilization-measurements.json` (ignored generated analysis).

## Float headroom versus output safety

The previous +3.26 dBFS observation is not a new HeadroomPick regression. A full-scale 440 Hz sine at Drive/Tone/Crunch/Grain100, H100, Bias/Symmetry50, Level−6, OS2x produces **+3.301 dBFS in both baseline and candidate**. H50 is +2.572 dBFS in both; H0 decreases from +1.797 to +1.582 dBFS. No sample became non-finite.

- Core and plugin audio are floating point. `PluginProcessor::render` preserves finite values above ±1 and flags the output CLIP meter. This is valid internal headroom, **but can clip at a downstream nonlinear stage, integer export or hardware output**. It is not safe merely because the buffer is float.
- Standalone `AudioIO` applies its existing 0.5 output gain (about −6.02 dB), then clamps to ±1 and reports pre-clamp clipping. The observed +3.30 dBFS isolated effect would be approximately −2.72 dBFS after that gain, absent additional effects. Other chains can still exceed full scale and hit the clamp.
- Do not add an unsolicited limiter or alter DSP compensation. For the measured stress case, reducing Custom Drive Level by at least 4 dB provides margin (e.g. −6 to −10 dB); for other material, verify the final meter and leave additional headroom. A host gain stage must precede any downstream processor that clips.
- Added a plugin boundary regression using a +3.26 dBFS float input: samples remain unclamped and CLIP is asserted. Existing AudioIO tests cover hardware clamp/CLIP behavior. This specifies the two distinct output contracts without changing sound.

## Candidate AU initialization and rendering

The candidate factory loaded under the execution sandbox, but `AudioComponentRegister` returned null. The same executable and binary passed outside that sandbox. This establishes an environment restriction in the test, not a candidate binary registration defect.

`test_installed_au_grain` now accepts an optional **AU executable path**. It loads that exact file and uses `AudioComponentRegister`'s process-local registration, retaining the returned component handle instead of searching installed components. This does not install an AU, edit AU caches, or change the system registry. The dylib remains loaded for the registrar's lifetime.

Command (run where macOS AudioComponent services are accessible):

```sh
cpp/build/app/plugin/test_installed_au_grain \
  "$PWD/cpp/build/headroom_candidate/CustomOD-HeadroomPick-v1/Multi Effects.component/Contents/MacOS/Multi Effects"
```

PASS: AudioUnit initialization, stereo rendering, parameter IDs/readback, live Grain/Headroom/Bias/Symmetry updates, 128/47 frame partition equivalence and per-sample AU/Core agreement within 1e-6. Grain endpoint difference max/RMS = 0.205367/0.0694482; Headroom endpoint difference = 0.210857/0.125504. These are deterministic probe values, not audibility claims. Logic project reload and installed-candidate auval were not performed; the candidate remains uninstalled.

Candidate binary SHA256: `fc4954d922bc75e2dd56556c39fd29c9bf0907384bcd81e2395eafd509f8ac29`.
Installed AU preserved: `c0e974af2bc5e9917fa4118279c9ee208cb847daa48b8b0379243bc9cffbdffe`.

## Validation

- Release test target rebuild succeeded; no DSP or candidate AU rebuild was needed for these test/documentation changes.
- CTest: **30/30 PASS**, including audio output safety, Custom Drive, Bias/Symmetry, stereo, automation, allocation/free, state/preset and long-run checks.
- Focused Python metric tests: **7/7 PASS**.
- Candidate native AU host initialization/render test: **PASS** outside the execution sandbox.
- No full Python suite or sanitizer rerun; no tolerances relaxed.
- `repomix-output.xml` remains untracked and unchanged. Build binaries, DI assets, logs and generated analysis are excluded from the commit.

Remaining: subjective Headroom/Bias/Symmetry approval, gain staging at extreme settings, and the later installed-candidate Logic session check. These are explicitly not represented as completed listening/product validation.
