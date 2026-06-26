---
name: sample-live-launcher
description: Architecture, build/run workflow, data model, and coding conventions for the Sample Live Launcher app — a cross-platform JUCE/C++ desktop app to fire audio samples live, routing each track to specific physical sound-card outputs. Use whenever working in c:\www\sample-live-launcher (building features, debugging audio/routing, editing any Source file, or onboarding into a fresh conversation).
---

# Sample Live Launcher

Cross-platform (JUCE/C++) desktop app to play audio **samples live** on stage. The device (PC first; iPad / Mac / Android later) is connected to a sound card with multiple outputs. Each song is made of one or more audio files (mp3/wav/aiff/flac), and each file is routed to chosen **physical output channels** (e.g. file A → outputs 1-2 = FOH, file B → outputs 3-4 = click to the drummer). Songs are grouped into setlists, and a stripped-down live view fires songs one by one in setlist order.

This is being **vibe-coded**: the assistant writes the C++, the user builds/runs/tests on their Windows machine and reports back. The user does not need deep C++ knowledge.

## Platform / stack decisions (settled)

- **v1 target: Windows.** Other platforms (iPad/Mac/Android) come later — JUCE already builds for all of them from the same source.
- **Framework: JUCE 8** (C++17). Chosen because per-output multichannel routing is the core requirement and JUCE is the pro-audio standard that handles it natively across all target OSes.
- Audio output currently via **Windows Audio (WASAPI shared)**. **ASIO** (lower latency, exposes all physical channels on multi-out USB cards) is planned for later — only needed once the user has a multi-output card. The routing engine already handles N channels, so it will work unchanged.

## Build & run (Windows)

Toolchain already installed: **MSVC 14.44** (VS Build Tools 2022, C++ workload), **CMake**, **git**, **VS Code**. No Visual Studio IDE needed — VS Code (C/C++ + CMake Tools extensions) drives edit/build/run/debug; Build Tools provides `cl.exe` + the `cppvsdbg` debugger.

```bash
# Configure (first time, or after adding source files to CMakeLists.txt)
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Debug --parallel
```

Run the app:
`build\SampleLiveLauncher_artefacts\Debug\Sample Live Launcher.exe`

**IMPORTANT — always kill running instances before rebuilding**, or the linker fails with `LNK1168: impossible d'ouvrir ... .exe pour écrire` (the running exe locks the file):

```powershell
Get-Process "Sample Live Launcher" -ErrorAction SilentlyContinue | Stop-Process -Force
```

JUCE lives as a shallow clone in `libs/JUCE` (tag 8.0.4). `build/` is git-ignored.

## Source layout (`Source/`)

- **Main.cpp** — `JUCEApplication` + main window host.
- **MainComponent.{h,cpp}** — `AudioAppComponent`. Owns the `Library`, `AudioFormatManager`, `AudioEngine`, and a `TabbedComponent` with 4 tabs: **Morceaux / Setlists / Live / Réglages**. Delegates the audio callback to the engine. Persists the audio device state to `audio.xml`. `SettingsView` (inline in MainComponent.h) wraps the `AudioDeviceSelectorComponent`.
- **AudioEngine.{h,cpp}** — the multi-track router/mixer (`juce::AudioSource`). Holds a list of tracks, each with its own `AudioTransportSource` + `AudioFormatReaderSource` (read-ahead via a shared `TimeSliceThread`). `loadSong(const Song&)` rebuilds the track list from song data. `playAll()/stopAll()` start/stop all tracks synchronously. In `getNextAudioBlock` it reads each track into a pre-allocated scratch buffer, then sums it into the chosen physical output channels with per-track gain. **This is the heart of the app.** Mutated under a `CriticalSection` (audio thread vs message thread).
- **Models.{h,cpp}** — plain data: `TrackData` (name, filePath, channels[0-based], gain), `Song` (id, name, tracks[]), `Setlist` (id, name, songIds[]), and `Library` (songs + setlists, JSON load/save).
- **TrackRow.{h,cpp}** — one editable UI row for a track: name, output-channels text field, gain slider, remove button. Channels are shown **1-based** to the user and converted to **0-based** internally (`parseChannels` / `channelsToText`).
- **SongsView.{h,cpp}** — Morceaux tab (DONE): song list + per-song editor (name, add files, track rows, local Play/Stop).
- **SetlistsView.{h,cpp}** — Setlists tab (STUB — not yet implemented).
- **LiveView.{h,cpp}** — Live tab (STUB — not yet implemented).
- **Text.h** — `operator""_t` UTF-8 string literal (see Encoding below).

## Data model & routing rules

Channel mapping in `AudioEngine::getNextAudioBlock`, per track (`srcCh` = source file channels, `dst` = chosen 0-based output channels):
- **mono file** → fed to every chosen output.
- **N source channels → 1 output** → downmixed to mono (summed, scaled by 1/srcCh).
- **else** → one-to-one mapping up to `min(srcCh, dst.size())`.

Per-track gain is applied during the sum (`addFrom`), not on the transport.

## Persistence

- Library: `%APPDATA%\SampleLiveLauncher\library.json` (songs + setlists). Saved automatically on every edit via `Library::save()`; views call it through their `onLibraryChanged` callback, which also refreshes the other tabs.
- Audio device: `%APPDATA%\SampleLiveLauncher\audio.xml` (restored on launch, saved on quit).

## Conventions

- **Encoding gotcha (important):** JUCE 8 `String(const char*)` decodes bytes as Latin-1 (`CharPointer_ASCII`), so a plain `"Réglages"` literal becomes mojibake. Source is compiled with MSVC `/utf-8` (set in CMakeLists), so literal bytes are valid UTF-8 — but you must tell JUCE to decode them. **For any user-facing string containing non-ASCII characters, append the `_t` literal suffix** (`#include "Text.h"`): `"Réglages"_t`, `"à venir"_t`. Pure-ASCII literals need nothing.
- Style: JUCE house style (4-space indent, spaces before `(`, `juce::` qualified). Match the surrounding code.
- When adding a new `.cpp`, add it to `target_sources` in `CMakeLists.txt` and reconfigure.
- UI text is in **French**.

## Status & next steps

- ✅ Milestone 1 — minimal audio app: device selector, load file, play. Validated (sound on USB card).
- ✅ Milestone 2 — multichannel routing engine + per-track output/gain UI. Validated (output 1 = L, output 2 = R on the user's current single-stereo-output card).
- ✅ Milestone 3 — data model + JSON persistence + tabbed shell + **Morceaux** tab + **Réglages** tab + UTF-8 fix.
- ⏭️ **Next: Setlists tab** — create/rename/delete setlists, add/remove/reorder songs (drawn from the song library).
- ⏭️ Then: **Live view** — pick a setlist, big stripped-down UI, fire songs one by one in order (prev / play / stop / next), highlight current song.
- ⏭️ Later: ASIO support; then port to iPad / Mac / Android.

The user's band context: Midnight Theory (backing tracks on stage).
