# SFRR-Sampler

**Simple Fucking Round Robin Sampler** — a free, open-source round-robin multi-sampler plugin.

Built with [iPlug2](https://github.com/iPlug2/iPlug2).

---

## Features

- **Per-key sample loading** — click `+` on any key to assign a folder of WAV/AIFF files
- **Round-robin playback** — Cycle (sequential) or Random (no-repeat) mode
- **Velocity sensitivity** — toggle velocity-to-volume on or off
- **Sustain mode** — when ON, samples play to their natural end; note-off and retriggers never cut them
- **Per-key editor** — click any key to open its controls:
  - **Trim** — skip the first 0–500 ms of the sample (sample start offset)
  - **Gain** — 0–200%, unity at 100%
  - **Pitch** — coarse tuning ±12 semitones
  - **Pan** — stereo placement L–C–R
  - Double-click any knob to reset to default
- **DAW state saving** — all sample paths, labels, and per-key values persist across sessions
- **Clear All** with confirmation

---

## Releases

| Platform | Format | Status |
|----------|--------|--------|
| macOS | AU (Audio Unit v2) | ✅ v1.0.0 |
| macOS | VST3 | ✅ v1.0.0 |
| Windows | VST3 | 🔜 needs testing — see [Contributing](#contributing) |

---

## Building from Source

### macOS

**Requirements:** macOS 11+, Xcode 13+

```bash
# 1. Clone iPlug2
git clone https://github.com/iPlug2/iPlug2.git

# 2. Download plugin SDKs (VST3, CLAP)
cd iPlug2/Dependencies/IPlug && ./download-iplug-sdks.sh && cd ../../..

# 3. Clone this repo into the iPlug2 Examples folder
git clone https://github.com/Stereokroma/SFRR-Sampler.git iPlug2/Examples/RobinSampler

# 4. Open the Xcode project
open iPlug2/Examples/RobinSampler/projects/IPlugInstrument-macOS.xcodeproj
```

Build the `macOS-AUv2` or `macOS-VST3` scheme.

| Scheme | Output |
|--------|--------|
| `macOS-AUv2` | `~/Library/Audio/Plug-Ins/Components/SFRR-Sampler.component` |
| `macOS-VST3` | `~/Library/Audio/Plug-Ins/VST3/SFRR-Sampler.vst3` |
| `macOS-APP` | `~/Applications/SFRR-Sampler.app` (standalone, Debug) |

---

### Windows

**Requirements:** Visual Studio 2019+ with C++ desktop workload

```
1. Clone iPlug2:
   git clone https://github.com/iPlug2/iPlug2.git

2. Download plugin SDKs (run from iPlug2/Dependencies/IPlug/):
   ./download-iplug-sdks.bat

3. Clone this repo into the iPlug2 Examples folder:
   git clone https://github.com/Stereokroma/SFRR-Sampler.git iPlug2\Examples\RobinSampler

4. Open in Visual Studio:
   iPlug2\Examples\RobinSampler\projects\IPlugInstrument-vst3.vcxproj
```

The Windows audio loading uses `dr_wav.h` (bundled) for WAV and AIFF support.
The Windows build has not yet been tested — if you get it working, please open a PR!

---

## Contributing

The Windows VST3 port is the most wanted contribution right now. All the cross-platform code is already written:

- ✅ Audio loading (`dr_wav.h`, handles WAV + AIFF on Windows)
- ✅ Directory scanning (Win32 `FindFirstFile`/`FindNextFile`)
- ✅ Font path (`C:\Windows\Fonts\cour.ttf`)
- ✅ Binary name and project settings

A Windows developer (or someone using [Claude Code](https://claude.ai/code)) just needs to clone the repo, attempt a build, and fix any remaining compile errors. Open a PR with the working build.

---

## Credits

- Framework: [iPlug2](https://iplug2.github.io/) by Oli Larkin and contributors
- Plugin: [Stereokroma](https://stereokroma.com)

## License

MIT — see [LICENSE](LICENSE)
