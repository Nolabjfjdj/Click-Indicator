# Click Indicator

A Geode mod for Geometry Dash that displays a visual indicator for click timing.

## Platform

- iOS (ARM64)

## Build

The iOS build runs through GitHub Actions using the Geode SDK version declared in `mod.json`.

The iOS CMake configuration follows the working approach used in CBF-iOS-NCI:
- ARM64-only iOS architecture
- FMOD header forced into generated Geode bindings
- Ad-hoc code signing of the built mod library

## Status

Initial project structure. The visual indicator and click-timing detection are not implemented yet.
