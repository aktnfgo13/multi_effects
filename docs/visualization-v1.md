# Visualization v1

The analysis tap copies actual samples immediately before and after the effect chain. It uses a fixed-capacity SPSC queue and drops new packets when full. The audio callback does not allocate, lock, log, or wait for the analysis worker. Waveform alignment uses the chain latency; FFT and display work run outside the callback. Analysis is disabled when its panel is hidden.

In the plugin editor, **Visualize** switches the existing effect editing area to the analysis panel. **Analysis** enables capture; the panel offers waveform or spectrum, input/output, lane, FFT size, zoom, and pause controls. The standalone uses the same analysis panel.

## Validation (2026-11-24)

- Release Standalone/AU/VST3 build and nine relevant CTest cases passed before Logic integration. After the Logic window fix, incremental AU/VST3 builds and `AnalysisTapSpectrum` / `AnalysisPluginIntegration` (2/2) passed.
- Existing regression logs report callback allocation/free 0 and bit-identical DSP output with analysis on/off. The integration fix changed only the plugin editor layout and its test.
- The installed user AU binary matches the new Release AU artifact (SHA-256 `65b05a925875582c9a5ee0d2041a9e4fa3e21976b47f4be8157b1a117ad7d2d5`). `auval -v aufx Mfx6 Mefx` passed.
- In a copy of the user's Logic project, the new AU displayed a live 48 kHz, two-channel input/output waveform during playback with 192-sample alignment and zero observed dropped frames. The original project was not edited.
- Logic's spectrum selection menu could not be operated reliably by UI automation. The user confirmed that the frequency graph appeared during playback after selecting Spectrum in the same test-project copy. FFT correctness is also covered by the C++ analysis test. Audio continuity and CPU change in Logic were not measured.

The previous user AU is backed up at `cpp/build/visualization-final/backup/Multi Effects.component` (ignored build directory). The test project copy is at `cpp/build/visualization-final/Logic Visualization Test.logicx`. Neither is a distribution artifact.
