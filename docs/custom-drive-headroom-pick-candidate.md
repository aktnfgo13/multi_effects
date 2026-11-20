# Headroom pick-loading candidate

Candidate: CustomOD-HeadroomPick-v1 + BiasSymmetry-v1. Listening approval pending; installed AU is not replaced.

## Evidence and cause

Reuses the verified Logic Track 2 test_copy_1 DI at bars 24–26 inclusive (source offset 62936, length 432000 at 48 kHz). Input peak −7.1744 dBFS, RMS −23.7725 dBFS; gate/compressor/gain and other effects bypassed, trim 0 dB. All nine renders use identical preroll, OS 2x, 64-sample alignment. See cpp/build/headroom_logic_analysis/REPORT.md and input_provenance.json.

Moving both clipping rails produces similar normalized waveforms. Existing slow sag does not strongly distinguish initial picking transients; high-Drive static compensation additionally reduces raw loudness differences. Input is not generally below the operating region.

## Minimal candidate

Preserve rails, compensation, existing envelope (including Grain bias), and all other macro formulas. Add one fast envelope (0.2 ms attack / 12 ms release). Only below H50, positive fast-minus-existing envelope adds bounded, rail-relative loading to the existing pre-stage sag denominator. Strength is 2 * max(0, 1 - 2H/100)^2; loading is strength * onset/(1+onset). This changes attack-versus-body rather than expanding clipping rails. H50–100 is unchanged; the new detector always tracks to avoid stale state when automating. Reset clears it, prepare sets coefficients; four floats per lane, no heap or new latency.

This is an experimental analytical interaction, not a physical circuit model. It can reduce instantaneous distortion during a pick by attenuating stage input; sustained clipping semantics are unchanged. High-Drive transient authority remains limited.

## Actual DI before → after (H0)

| Drive | Peak dBFS | RMS dBFS | Small-signal deficit dB | H0/H100 matched residual % |
|---|---|---|---|---|
| 25 | -13.410 → -14.564 | -26.849 → -27.260 | 2.051 → 2.462 | 7.66 → 11.83 |
| 50 | -11.856 → -12.812 | -22.363 → -22.843 | 5.881 → 6.357 | 13.11 → 17.05 |
| 75 | -9.898 → -9.997 | -17.349 → -17.781 | 12.518 → 12.940 | 11.81 → 14.87 |

H50/H100 DI outputs are sample-identical to baseline (all three Drives). Small-signal deficit is a level deficit against the same DI attenuated 60 dB then rescaled, not an internal compressor gain-reduction meter. Matched WAVs are −24 dBFS RMS, not perceptual loudness normalization. Sine-control THD is measured separately; real guitar DI cannot provide isolated DSP THD.

H0 440 Hz control THD before → after: D25 3.052 → 3.091%; D50 6.820 → 6.803%; D75 15.359 → 15.591%. Maximum absolute mean DC across nine DI outputs is 2.81e-5 (below −91 dBFS); H0 output RMS falls by about 0.41–0.48 dB. No normalization/limiting was introduced.

## Reproduction and listening files

cpp/build/headroom_candidate/compare.py reuses the existing custom_drive_benchmark CLI and verified source input; results.json contains all nine combinations. raw/ and level_matched/ hold nine WAVs each. Original logs/renders remain untouched in cpp/build/headroom_logic_analysis/. before/ contains the previous header, ProductInfo and benchmark executable. These generated files are ignored build artifacts.

Compare the matched H0/H100 files at each Drive first, then H50. Listen for first pick suppression versus body, palm-mute recovery and pumping. Larger numerical differences do not establish audibility or quality; final guitar/Logic approval remains with the user.

## Focused validation and candidate AU

- Custom Drive focused suite PASS: existing Bias/Symmetry tests and frozen H50 references, added 27 Drive/Headroom/OS combinations (reset, irregular blocks, stereo isolation, recovery to silence, smoothed parameter-step residual). Processing allocation/free both 0. Concurrent-control focused run PASS.
- Plugin drive-propagation and grain-state focused modes PASS, including Bias/Symmetry, stereo, state restore and processing allocation/free. No full regression run.
- Release AU build PASS; candidate binary matches the build artifact. Candidate: `cpp/build/headroom_candidate/CustomOD-HeadroomPick-v1/Multi Effects.component`. Binary SHA256 `fc4954d922bc75e2dd56556c39fd29c9bf0907384bcd81e2395eafd509f8ac29`.
- Installed AU remains untouched, SHA256 `c0e974af2bc5e9917fa4118279c9ee208cb847daa48b8b0379243bc9cffbdffe`. Candidate retains existing AU identity and schema; it is an alternative build artifact, not a second side-by-side Logic plugin. Candidate installation, Logic launch and auval were not performed.
- 48k/128 stereo H0, Grain50 smoke after compilation: 1x/2x/4x mean 13.85/76.17/139.60 microseconds, maxima 24.13/91.88/161.83 microseconds. Baseline means 12.67/72.80/133.60 microseconds. Latency unchanged 0/64/64 samples. These are scheduling-sensitive offline observations, not a hard realtime bound. Initial overlapping compilation/profile results are retained separately and excluded from the idle comparison.
- Source edits this task: CustomDrive.h, ProductInfo.h.in, test_custom_drive.cpp, this document. Other pre-existing edits and repomix-output.xml preserved. No commit/push.
