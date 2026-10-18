# MyEffect

MyEffect is a DSP development and prototyping platform for building audio effects
as standalone applications, VST3 plugins and, on macOS, Audio Units. It provides
a compact audio-plugin template centred on `MyEffect`, allowing development to
concentrate on audio and signal processing rather than repeatedly implementing
device, host, interface and plugin-format boilerplate.

The recommended starting point is the four files in `Source`. The `Framework`
layer handles JUCE integration, parameter controls, test-file playback, audio
devices and live analysis. This is a convenient development boundary, not a
closed abstraction: the complete implementation is included and may be
inspected, extended or replaced when an effect needs functionality beyond the
template.

## Open the project

- Visual Studio: `Projects/MyEffect.sln`
- Xcode: `Projects/MyEffect.xcodeproj`
- Visual Studio Code: `Projects/MyEffect.code-workspace`

The VS Code workspace uses `CMakePresets.json`. Install CMake 3.25 or newer
and Ninja; on macOS, also install Xcode and select it with `xcode-select`.
The presets use Ninja Multi-Config so Debug and Release share a build tree
and the requested configuration is respected on every platform. From a terminal:

```text
cmake --preset development
cmake --build --preset development-debug
cmake --build --preset development-release
```

If `Build/CMake` was configured with another generator, delete that generated
directory and configure again (JUCE also caches its helper generator). On macOS the
standalone app, VST3 and AU bundles are written to `Build/Products/Debug`
or `Build/Products/Release` (VST3 bundles are in the `VST3` subdirectory).

All generated binaries and intermediate files belong under `Build`. It is
always safe to close the IDE and delete that directory to force a clean build.

## Test and analyse an effect

Run the Standalone target. Its lower toolbar selects audio from `Tests` and
provides playback, bypass and audio-device controls. The analysis panel displays
the processed output as an oscilloscope, spectrum or sonogram. The same DSP code
can then be built as VST3 or AU for testing in a host.
