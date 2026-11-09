# Input System v0.1

The host adapter selects the physical input or host bus. InputSystem processes the routed planar float buffers in place before the first effect. A null right pointer selects mono. Stereo lanes share a gain ramp and have independent signal samples. The plug-in saves the untrimmed, latency-aligned host bypass signal before applying input trim. Standalone retains its device routing fades and output protection.

Input Trim is a system control in dB. The default is unity, with a ±12 dB range and a 5 ms linear gain ramp. Standalone stores it in application settings. Plug-in instances expose input.trim to the host and store input_trim_db in project state. Older states use unity. Loading an effect preset leaves the input calibration alone.

## Meter interpretation

The existing input peak measures the selected source before trim. DSP input measures the signal delivered to the chain. Its peak is the maximum absolute sample across channels. Its RMS is the square root of mean sample power across frames and channels. Both report the most recently completed 20 ms window. Values between adjacent windows may be missed by UI polling, but full-scale crossings latch until the UI consumes them. The digital HOT threshold is −3 dBFS peak and CLIP is 0 dBFS peak. A float host can supply samples above full scale. The input stage passes finite over-range audio and replaces nonfinite samples or trim overflow with zero. DC passes at unity.

## Calibration

InputReference defines the provisional digital nominal level of −20 dBFS RMS. RMS uses full-scale amplitude 1 as its reference. Compare active playing separately from long silence and tails; window duration changes the reading. The listening benchmark's 500 ms synthetic strong pluck measures −20.8 dBFS RMS, −5.28 dBFS peak. The calibration sweep uses its first 120 ms at five known levels. Run `.venv/bin/python examples/benchmark_input_calibration.py` after building custom_drive_benchmark. Gain loss compares output/input RMS with the same note rendered at small-signal level through the same OD settings. It includes all level-dependent behavior.

To establish a physical reference, collect unprocessed mono DI from single coils, humbuckers and active pickups with repeatable weak/normal/hard single notes, chords, palm mutes, and a separate idle interval. Record interface, Hi-Z mode/input impedance, analog gain/pad, ADC full-scale input voltage if known, and sample rate. Retain originals and report active-note RMS with its time window, peak, crest, DC mean and upstream clip indication. An eventual hardware adapter can supply those properties outside the callback and supply an independent analog clip flag. A digital peak measurement alone cannot locate analog overload.

A future wet/dry block inside the effect chain should take its dry reference from the defined post-trim input and align its processing latency. Host global bypass continues to use its existing host-input reference.
