# Multi Effects 0.7.0 Alpha — macOS

This release packages the existing stereo DSP in a selected-effect UI. It does not
change the DSP algorithms, parameter IDs, AU identity, or VST3 identity. The name,
version and Alpha label are defined in `cpp/cmake/Product.cmake`, which generates
a header for both applications. Standalone keeps `com.multieffects.prototype` for
settings/permission continuity; plugins keep `com.multieffects.plugin`, AU `aufx /
Mefx / Mfx6`. The older “Prototype” app may remain in an existing build directory;
launch the **Multi Effects.app** artifact below.

## UI and presets

Both editors show a persistent ordered chain, selected-effect controls, bypass,
preset/status area, meters and latency. Standalone retains add/remove/reorder,
input routing, stereo input, Mute and device settings. Plugin has no device controls
or structural edit buttons: load a Standalone preset to change its chain. Plugin
host automation still addresses the first instance of each effect type. Duplicate
instances are shown but read-only in its parameter panel; their preset values are
preserved. Host automation can still see all stable parameter banks, including
absent effects; the custom editor shows only the selected effect.

Shared `ui/AlphaUI.h` renders units, short numeric values, percentages for normalized
controls and explicit 1x/2x/4x choices. Tooltips show range/default; double-click a
continuous slider to reset. Plugin controls send host gestures and receive host
automation through JUCE ParameterAttachment. Oversampling uses the existing deferred
rebuild; stop transport when changing mode. Tail resets / host PDC transitions are
unchanged from v0.6.

Factory / User entries share the browser. Select an entry then Load; Open imports
an arbitrary v2/v3 JSON preset. Save updates the current user file; Save As asks for
a destination and confirms replacement. Factory entries are immutable recipes;
Save makes a user copy. Five conservative demonstration presets: Clean, Crunch,
High Gain, Ambient, Lead. These names are descriptions, not emulations or listening
quality claims. All use the existing synthetic default IR. No external IR is bundled.
Load user IR imports WAV, validates it and embeds samples/rate/name in presets and
plugin project state. Original WAV files are never modified.

A `*` marks modified presets. Plugin state adds optional `preset_modified` metadata
to the existing `plugin_state: 1` envelope; older v0.6 states default to clean. Chain,
IR and automation values still use the same schema. The plugin's file-save destination
is intentionally editor-local: after reopening the editor or receiving host state,
Save asks for a destination. This avoids silently overwriting a source file from a
restored project. The project always preserves the sound configuration and preset
name, independent of that file. Standalone retains the existing last-file/settings
behavior; a factory recipe with no saved user copy is not a last-file restore target.

## User data

- Factory presets: compiled recipes in `app/PresetLibrary.h`, no writable files.
- Default user presets: `~/Library/Application Support/Multi Effects/Presets/*.json`.
- Standalone settings: existing
  `~/Library/Application Support/Multi Effects/MultiEffectsStandalone.settings`.
- Previously selected preset folders and existing files remain supported. Browser
  includes the standard User folder and current external folder. No migration/deletion.
- User IR: original file stays where selected; embedded copy travels in preset/project.
- Plugin host state: owned by the DAW project; excludes Standalone device preferences.
- Build/log/temp artifacts: ignored `cpp/build/`; not the default for user data.

## Build and local testing (no public distribution)

```sh
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release \
  -DMULTI_EFFECTS_BUILD_APP=ON -DMULTI_EFFECTS_BUILD_PLUGIN=ON
cmake --build cpp/build --parallel 3
```

From the repository root, the Release artifacts are:

- `cpp/build/app/MultiEffectsStandalone_artefacts/Release/Multi Effects.app`
- `cpp/build/app/plugin/MultiEffectsPlugin_artefacts/Release/AU/Multi Effects.component`
- `cpp/build/app/plugin/MultiEffectsPlugin_artefacts/Release/VST3/Multi Effects.vst3`

Debug Standalone: configure `cpp/build/debug` with `CMAKE_BUILD_TYPE=Debug`, APP ON,
PLUGIN OFF. Its app is under the equivalent `Debug/Multi Effects.app` path. The
existing pinned JUCE checkout may be reused via `FETCHCONTENT_SOURCE_DIR_JUCE`.

Quit the DAW before replacing a test plugin, and preserve a backup of the previous
bundle outside its scanned plugin folder. Copy the AU into
`~/Library/Audio/Plug-Ins/Components/`, or VST3 into
`~/Library/Audio/Plug-Ins/VST3/`. Use user-local folders, not system-wide installation.
Run `auval -v aufx Mfx6 Mefx`, then rescan in Logic Plugin Manager if needed. Logic
loads AU, not VST3; use a VST3 host for VST3 acceptance. Do not overwrite an unrelated
bundle or open an important project without a backup when testing this Alpha.

## Signing preparation

No Developer ID signing, notarization submission, installer or publication is part
of this sprint. Local linker/JUCE ad-hoc signatures are not Developer ID distribution
signatures. Inspect each bundle with `codesign -dv --verbose=4 <bundle>` and
`codesign --verify --strict --verbose=2 <bundle>`; retain the output with build logs.

For future distribution: obtain Apple Developer Program access and a Developer ID
Application certificate/private key. Select the intended architectures and minimum
macOS version first. On staged copies, sign nested executable code inside-out with
Developer ID, secure timestamp and hardened runtime; grant only necessary entitlements
(including audio-input access for a hardened Standalone). Verify each bundle, package
it, submit using `xcrun notarytool submit ... --keychain-profile ... --wait`, inspect
the notarization log, staple supported deliverables with `xcrun stapler staple ...`,
and validate the ticket/Gatekeeper behavior on a clean Mac. Keep credentials in
Keychain, not this repository. Signing is a separate release step, not forced in
local CMake builds. Do not use recursive `--deep` signing to hide nested-code issues.

Apple references:
[Developer ID](https://developer.apple.com/developer-id/),
[notarization](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution),
[custom workflow](https://developer.apple.com/documentation/security/customizing-the-notarization-workflow).

## Acceptance checklist

- Standalone: launch the new app; confirm 0.7.0 Alpha, devices/input route, Mute,
  48k/128, meters/CLIP; select each effect; edit/reset parameters and bypass; retain
  add/remove/reorder and error visibility.
- Both: load all five Factory presets at low monitoring level, save a User copy,
  edit it (modified marker), Save As, reopen; import an IR and verify embedded restore;
  invalid JSON/WAV must report an error without replacing the current configuration.
- Logic: open a copy of a v0.6 project, verify chain/IR/parameters/modes and automation;
  record/edit a control, reopen editor, save/reopen project; verify host bypass/PDC.
- VST3 host: scan and perform equivalent state/automation tests; an AU validation
  pass is not proof of VST3-host behavior.

## v0.7 validation record

- Release CTest **26/26**, including long-run and PluginAlphaUX; pytest **42/42**;
  all six existing Python/C++ comparison scripts passed in the final full run.
- Standalone Debug/Release and AU/VST3 Release builds succeeded. Core DSP source,
  audio callback code, algorithm tolerances and automation layout were not changed.
- The final Release Standalone launched at 48 kHz / 128 frames with advancing
  callbacks, output-only built-in speakers and Mute ON. Selected Amp controls and
  synthetic IR status were visually inspected. No external guitar/input listening
  test was performed in this sprint.
- `auval` passed **Component Version 0.7.0 (0x700)**. The first validation saw cached
  0.6.0 metadata, so only the user's AudioComponentRegistrar was restarted and the
  new version was validated. Logic and audio devices were not terminated. The
  previous installed AU is preserved at `cpp/build/v07-previous-installed.component`;
  the new user-local AU is installed. Restart the host before v0.7 acceptance.
- Current Release binaries are arm64; the local toolchain selected macOS 27.0 as
  minimum OS because CMake's deployment target is unspecified. Set/test an explicit
  supported deployment target and architecture set before sending builds to testers
  on different Macs.
- Signature inspection: Standalone and AU have linker ad-hoc executable signatures
  with no bundle resource seal; strict bundle verification fails. VST3 has a JUCE
  ad-hoc bundle signature, but its subsequently generated moduleinfo.json changes
  the seal, so strict verification also fails. These are **local build artifacts,
  not distribution-ready signatures**. The future release signing step must run
  after all bundle resources/manifest generation; no signing workaround was applied.
- Logs: ignored `cpp/build/v07-*.log`; signature details are
  `v07-bundles-signatures.log`. VST3 native-host acceptance remains manual; no VST3
  validator was installed.

Remaining priorities: no newly observed P0. P1: actual Logic/VST3-host and external
interface acceptance, and final bundle signing/deployment target before external
sharing. P2: duplicate-effect editing/slot automation, preset search/favorites and
more refined presentation; these are outside this sprint. Existing deferred mode
switch/tail-reset limitations remain unchanged.

Changed files for this sprint:

- `README.md`

- `cpp/app/Main.cpp`, `AppState.h`, `PresetLibrary.h`, `CMakeLists.txt`
- `cpp/ui/AlphaUI.h`
- `cpp/cmake/Product.cmake`, `ProductInfo.h.in`
- `cpp/plugin/PluginEditor.cpp`, `PluginProcessor.cpp`, `PluginProcessor.h`, `CMakeLists.txt`
- `cpp/tests/test_app_state.cpp`, `test_plugin.cpp`
- `docs/macos-alpha.md`, `macos-plugin.md`, `standalone-prototype.md`

ASan/UBSan: **24/24 passed**, including the new UI test; long-run tests excluded
from this sanitizer run and ASan leak detection disabled. Release CTest above
includes the long-run tests. Existing allocation tracking remains enabled in the
Release processing tests and reports zero processing allocations/frees.

TSan: **6/6 concurrency tests passed**. All final regression commands exited zero;
no tolerances were broadened. Commit/push was not performed.

48 kHz / 128-frame stereo profiling (`test_plugin profile`, 2000 measured blocks,
interleaved direct Core and plugin adapter; plugin editor closed):

| Mode | Core mean µs | Adapter mean µs | Adapter max µs | Max / 2666.67 µs deadline |
|---|---:|---:|---:|---:|
| 1x | 37.45 | 37.59 | 322.71 | 12.10% |
| 2x | 186.49 | 186.93 | 308.38 | 11.56% |
| 4x | 336.30 | 338.45 | 583.63 | 21.89% |

Mean adapter overhead was 0.1–2.2 µs in this local run. Background scheduling affects
maxima and comparisons with earlier sessions; this is not a worst-case realtime
bound or a whole-DAW benchmark. Source review confirms no processing-path changes;
processing allocation/free checks and the Core/adapter output matrix passed.
