# Custom Drive live parameter investigation (2026-11-05)

The reported lack of audible Crunch/Headroom/Grain changes was **not reproduced as
an implementation propagation failure**. No sound mapping, DSP algorithm or production
parameter path was changed. The listening symptom remains unresolved.

## Traced path

Standalone `MainComponent::selectedRowsChanged` constructs `ParameterRow` from the
selected effect's metadata and target value. `slider.onValueChange` forwards the
value; the callback captures instance ID and parameter ID by value, resolves that
instance in `runtime.chain`, and calls `Parameter::set`. This GUI callback takes the
control mutex; the audio callback does not.

`Parameter::set` clamps and writes the lock-free atomic target. `Builtin::render`
loads every parameter target and updates `values_` every sample with the same 20 ms
smoothing rule for all five macros. Registry indices are Drive0, Tone1, Crunch2,
Headroom3, Grain4, Level5, Mix6, Oversampling7. The processor passes indices0..6 to
`CustomDrive::Controls` in that exact order. Both independent lane captures receive
the same updated values vector; they do not copy parameter targets only at prepare.

`CustomDrive::update` recalculates Headroom coefficients when Headroom changes.
Crunch hardness and Grain are assigned on **every** sample outside that cache branch.
The nonlinear island reads those members. Bypass and Mix can suppress any macro, but
there is no macro-specific bypass branch. No ID typo/duplicate was found in the
registry. Drive/Tone use the same transport and smoothing as the reported controls.

PluginEditor uses `ParameterAttachment` to stable `custom_drive.<key>` host IDs;
`Processor::processBlock` copies their atomics into the prepared effect's targets,
then uses the same Core path. Continuous parameters do not rebuild the chain.

## Added output regression

`CustomDriveRealtimePropagation` is in `test_plugin.cpp` and registered in the
plugin CMake file. It tests the shared GUI ParameterRow and real Standalone Runtime
with the same ID-resolving callback as MainComponent (not a native click test of the
full MainComponent). It also tests the **actual PluginEditor attachments**.

Two instances start identically at parameter0. After one second of live block
processing, only one GUI slider changes to100 synchronously; no prepare/reset or
chain rebuild occurs. Steady output after smoothing is compared against an unchanged
control fed the same input. This rules out mere object changes and startup transients.
Both Standalone lanes must change by more than1% of reference waveform RMS, at
1x/2x/4x. Drive/Tone are positive controls. Plugin tests also require exactly silent
R for active L, and all processing is allocation/free tracked.

At48k/128 and2x, RMS(output_changed-output_held)/RMS(output_held):

| Parameter | Standalone L | Standalone R | Plugin L |
|---|---:|---:|---:|
| Drive | 1.53939 | 2.54010 | 1.13484 |
| Tone | 1.21099 | 0.79235 | 1.08352 |
| Crunch | 0.44702 | 0.41039 | 0.44284 |
| Headroom | 4.14832 | 2.89467 | 4.80177 |
| Grain | 0.15118 | 0.19645 | 0.13086 |

These are waveform differences, **not** perceived loudness percentages. Standalone
uses two different input signals; plugin uses mono excitation with silent R.

## Observed native app / limits

The running Release executable was launched after the latest build timestamp.
Read-only inspection found Custom Drive ON, Mix100%,2x; downstream Distortion and
Amp Simulation were ON. Input/output were MacBook Pro microphone/speakers at48k/512.
No device, parameter, preset or unsaved session state was changed. This does not
establish what hardware/settings were used for the reported guitar listening.
Downstream nonlinear masking, input level and monitoring path remain hypotheses,
not confirmed root causes. The next discriminating listening check is an isolated
Custom Drive with the interface's direct-monitor path excluded, comparing Headroom
0/100 first at a fixed input/Drive; then re-enable downstream effects.

Validation logs: `cpp/build/v08-propagation-test.log`,
`cpp/build/v08-propagation-regression.log`, `cpp/build/v08-propagation-release-build.log`.
No full CTest/pytest/sanitizer rerun and no commit/push.
