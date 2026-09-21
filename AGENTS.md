# MyEffect teaching template

- Treat `Source/` as the student workspace. Implement ordinary DSP, controls,
  presets and helper classes there.
- Treat `Framework/` as vendored infrastructure. Do not edit it unless the user
  explicitly asks for adapter, harness, custom-UI or JUCE work.
- Treat `Projects/` as supplied IDE configuration. Do not regenerate it with
  Projucer.
- Put every generated file under `Build/`; never create build output in
  `Source/` or `Projects/`.
- The repository is deliberately flattened. Do not introduce Git submodules.
- For VS Code/CMake verification, use `cmake --preset student` followed by
  `cmake --build --preset student-debug --target MyEffect_Standalone`.
