# NXArranger

NXArranger slices a song into the sections of a Korg Pa3X-style arranger. Open an MP3, see its
waveform, give each pad a time range, and export every section as its own WAV file named after the
pad.

Pads follow the Pa3X front panel:

| Group | Pads |
|---|---|
| Intro | 1, 2, 3 |
| Variation | 1, 2, 3, 4 |
| Fill | 1, 2, 3, 4 |
| Break | 1 |
| Ending | 1, 2, 3 |

Built with C++17 and Qt 6. Runs on macOS and Windows.

## Download

Get the latest build from the [Releases](https://github.com/lazaronixon/nxarranger/releases) page.

- **macOS** (12 or later, Apple silicon and Intel): open `NXArranger-<version>-macos.dmg` and drag
  NXArranger to Applications. The app is not notarized, so the first time you open it, right-click
  it and choose **Open**, then confirm.
- **Windows** (10 or later, 64-bit): unzip `NXArranger-<version>-windows-x64.zip` and run
  `NXArranger.exe`. No installer is needed.

## Use

1. **File ▸ Import Song** (or drag an MP3 onto the window) to load a song.
2. Click a pad, then drag on the waveform to set its range. Drag the edges of the highlighted region
   to adjust it. To redraw a range, clear it first: press Delete, or right-click the pad.
3. Press the **START/STOP** pad to hear the selected pad's range.
4. **File ▸ Export Regions** to choose a folder. NXArranger writes one 16-bit WAV per pad that has a
   range, for example `Intro 1.wav` and `Variation 2.wav`.

Use **File ▸ Save** to keep your work as a project (`.nxa`). A project stores the song's location and
every pad's range; **File ▸ Open** restores them. Keep the song next to the project, or in the same
place relative to it, so the project still opens after you move the folder.

### Keyboard shortcuts

On Windows, use Ctrl where macOS uses ⌘.

| Key | Action |
|---|---|
| Space | Play / pause |
| Esc | Stop (returns to where playback started) |
| ← / → | Move the playhead one screen pixel (zoom in for finer steps) |
| Home or ⌘← | Go to start |
| Delete / Backspace | Clear the selected pad's range |
| ⌘+ / ⌘− / ⌘0 | Zoom in / zoom out / zoom to fit |
| ⌘I | Import song |
| ⌘E | Export regions |
| ⌘N / ⌘O / ⌘S / ⇧⌘S | New / open / save / save as project |

The mouse wheel zooms around the pointer. Shift+wheel or a sideways trackpad swipe scrolls.

## Build from source

You need CMake 3.21 or later, a C++17 compiler, and Qt 6 with the Multimedia and SVG modules.

**macOS** (Homebrew):

```sh
brew install cmake qt
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
open build/NXArranger.app
```

**Windows** (Visual Studio 2022 and the Qt online installer, with Qt Multimedia selected):

```bat
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --config Release --parallel
build\Release\NXArranger.exe
```

Run the tests with:

```sh
ctest --test-dir build -C Release --output-on-failure
```

## Release

GitHub Actions builds and tests every push and pull request on macOS and Windows. To publish a
release:

1. Set the new version in `project(NXArranger VERSION x.y.z)` in `CMakeLists.txt` and commit it.
2. Tag the commit and push the tag:

   ```sh
   git tag vx.y.z
   git push origin vx.y.z
   ```

The workflow checks that the tag matches the version in `CMakeLists.txt`, then builds the macOS DMG
and Windows ZIP and attaches them to a new GitHub Release. To test the packages without releasing,
run the **Build** workflow manually from the Actions tab.
