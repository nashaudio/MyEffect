# MyEffect

MyEffect is the PB Level 5 audio-programming template. The four files in
`Source` are the student workspace; everything under `Framework` is supporting
infrastructure. The framework is included directly, so cloning the repository
requires no submodule setup. Students should only edit `Framework` when taking
the optional advanced JUCE route.

## Open the project

- Visual Studio: `Projects/MyEffect.sln`
- Xcode: `Projects/MyEffect.xcodeproj`
- Visual Studio Code: `Projects/MyEffect.code-workspace`

The VS Code workspace uses `CMakePresets.json`. From a terminal, the same
workflow is:

```text
cmake --preset student
cmake --build --preset student-debug
```

All generated binaries and intermediate files belong under `Build`. It is
always safe to close the IDE and delete that directory to force a clean build.

## Test harness

Run the Standalone target. Its lower toolbar selects audio from `Tests`, and
provides playback, bypass and audio-device controls. The analysis panel in the
plugin editor displays the processed output as an oscilloscope, spectrum or
sonogram.
