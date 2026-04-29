# SFRR-Sampler

**Simple Fucking Round Robin Sampler** — a free, open-source round-robin multi-sampler plugin for macOS.

Built with [iPlug2](https://github.com/iPlug2/iPlug2).

---

## Features

- **Per-key sample loading** — click `+` on any key to assign a folder of WAV/AIFF files
- **Round-robin playback** — Cycle (sequential) or Random (no-repeat) mode
- **Velocity sensitivity** — toggle velocity-to-volume on or off
- **Sustain mode** — when ON, samples play to natural end; note-off and retriggers never cut them
- **Per-key editor** — click any key to open its controls:
  - **Trim** — skip the first 0–500 ms of the sample
  - **Gain** — 0–200%, unity at 100%
  - **Pitch** — coarse tuning ±12 semitones
  - **Pan** — stereo placement L–C–R
  - Double-click any knob to reset to default
- **DAW state saving** — all sample paths, labels, and per-key values persist across sessions
- **Clear All** with confirmation

## Formats

| Format | Status |
|--------|--------|
| AU (Audio Unit v2) | ✅ macOS |
| VST3 | ✅ macOS |
| Windows | planned |

## Building from Source

### Requirements

- macOS 11+, Xcode 13+
- [iPlug2](https://github.com/iPlug2/iPlug2)

### Setup

```bash
# 1. Clone iPlug2
git clone https://github.com/iPlug2/iPlug2.git

# 2. Download plugin SDKs (VST3, CLAP)
cd iPlug2/Dependencies/IPlug && ./download-iplug-sdks.sh && cd ../../..

# 3. Clone this repo into the iPlug2 Examples folder
git clone https://github.com/stereokroma/SFRR-Sampler.git iPlug2/Examples/RobinSampler

# 4. Open the Xcode project
open iPlug2/Examples/RobinSampler/projects/IPlugInstrument-macOS.xcodeproj
```

### Build targets

| Scheme | Output |
|--------|--------|
| `macOS-AUv2` | `~/Library/Audio/Plug-Ins/Components/SFRR-Sampler.component` |
| `macOS-VST3` | `~/Library/Audio/Plug-Ins/VST3/SFRR-Sampler.vst3` |
| `macOS-APP` | `~/Applications/SFRR-Sampler.app` (standalone, Debug only) |

## Credits

- Framework: [iPlug2](https://iplug2.github.io/) by Oli Larkin and contributors
- Plugin: [Stereokroma](https://stereokroma.com)

## License

MIT — see [LICENSE](LICENSE)
