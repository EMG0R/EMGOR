# EMGOR

Source for emgor.online (the site at the repo root) plus every project snapshot under `projects/`. All work here is open source. Personal information and credentials are not in this tree.

## Layout
- Repo root: the website, deployed as-is by Netlify. `projects/` is not served (404).
- `projects/<NAME>/`: current snapshot of each project (files only, no private history).
- `projects/LARGE_FILES.md`: files over 50 MB and build outputs that were left out.

## Projects
| Project | What it is |
|---|---|
| [NEPTR](projects/NEPTR) | Csound guitar/bass multi-FX instrument: ~60 effects, looper, NAM, presets, Teensy body, pygame touch UI |
| [DEMIURGE_OS](projects/DEMIURGE_OS) | Audio-first Raspberry Pi 5 OS: PipeWire hub, multi-language chain, Rust launcher, one live.conf |
| [HYPERGUITAR](projects/HYPERGUITAR) | Hex-pickup guitar on Bela: per-string pitch engine, formant, resonators |
| [HYPERTRUMPET](projects/HYPERTRUMPET) | Hyper-instrument trumpet work |
| [NEURALGRID](projects/NEURALGRID) | 8x8 NeoTrellis + sensor grid instrument, gesture-to-sound-space translator |
| [PMOR](projects/PMOR) | Daisy Seed pocket groovebox firmware (snapshot of PMOR_V3 macbert/max, 2026-11-12) |
| [DUODECIMUS](projects/DUODECIMUS) | Teensy 4.1 12-encoder MIDI controller |
| [SMARTGOR](projects/SMARTGOR) | Smart room: LED ring, ESP32 strips, lightserver, gate, runbooks |
| [SPEAK](projects/SPEAK) | Speak-and-Spell voice stack |
| [GORMAPPER](projects/GORMAPPER) | Projector mapping app |
| [MACHINE_LAB](projects/MACHINE_LAB) | CalArts Machine Lab robot-orchestra control work |
| [NEPTR_VIDEO](projects/NEPTR_VIDEO) | Fractal/feedback video synth (C++17/GLES3) |
| [OP1_OS](projects/OP1_OS) | Own firmware for the OP-1 |
| [THE_WATCH](projects/THE_WATCH) | ESP32-S3 watch firmware |
| [WE_REMOTE](projects/WE_REMOTE) | 3D-printed Wii Remote shell |
| [PROTOTYPES_2026](projects/PROTOTYPES_2026) | Quadro Punch Packer, WC Mixer, 4-i-Gor |
| [PAPERS](projects/PAPERS) | NAM-in-Csound, luthier/ROTURA, NIME papers |
| [UNIVERSAL_USB_PROTOCOL](projects/UNIVERSAL_USB_PROTOCOL) | Rust USB protocol experiment |
| [PRIME_RECOVERY](projects/PRIME_RECOVERY) | Recovery patches and autopilot for the NEPTR rig |
| [DOCS_2026NEW](projects/DOCS_2026NEW) | Cross-project docs |

## Clone only one project
```
git clone --filter=blob:none --sparse https://github.com/EMG0R/EMGOR.git
cd EMGOR
git sparse-checkout set projects/NEPTR
```

## Notes
- Credentials, WiFi/OTA passwords, tailnet addresses and contact details were replaced with placeholders (`CHANGE_ME_PASSWORD`, `YOUR_WIFI_SSID`, `YOUR-TAILNET`). Set your own before flashing or deploying.
- No license file exists in this repo; none was added.
