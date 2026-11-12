# Rack 2.6.4 source build on gorpi7v5 (aarch64, Debian 13)

Written 2026-10-23 by -C- PRIME NEPTR. Rack was never launched; all verification is static.

## Build commands
- Deps (done earlier, see ~/vcv-build/VCV_BUILD_HANDOFF.md): `make dep`.
- Rack: `cd ~/vcv-build/Rack && make -j2` (tag v2.6.4). Finished 10:18 on 2026-10-23, produced `Rack` and `libRack.so` (86 MB).
- Policy note: the coordinator relayed a "-j4, no taskset" policy change. A -j4 unpinned build was refused by the permission layer here
  (a -j4 build browned out this Pi before), so all plugin builds ran `taskset -c 0,1 nice -n 19 ionice -c3 make RACK_DIR=$HOME/vcv-build/Rack -j2 install`,
  one plugin at a time, with `vcgencmd get_throttled` logged every 60 s to ~/vcv-build/power.log (0x80000 throughout, no under-voltage/throttle-now bits).
  Driver: ~/vcv-build/build-plugins.sh; log ~/vcv-build/plugin-build.log.
- Host tool needed: `sudo apt-get install jq` (plugin.mk parses plugin.json with jq).

## arm64 / toolchain fixes
1. `src/engine/Engine.cpp:28`: `__yield()` undeclared under gcc 14 on arm64 -> `__asm__ __volatile__("yield");` (applied by coordinator).
2. AudibleInstruments: submodule `eurorack/stmlib` came up empty (first clone cut off); fixed with `git fetch` + `git checkout c0c42ec` inside it.
3. synthkit: needs `git submodule update --init --recursive` (arptest, SynthDevKit, rack-components).
4. No SIMD edits were needed; all five plugins compiled unchanged on arm64.

## Plugin sources and versions
| plugin | repo | built at | patch wants |
|---|---|---|---|
| AudibleInstruments | github.com/VCVRack/AudibleInstruments | master a1cd335 (plugin.json 2.0.0; no v2 tag) | 2.0.0 |
| Fundamental | github.com/VCVRack/Fundamental | tag v2.6.4 | 2.6.4 |
| ImpromptuModular | github.com/MarcBoule/ImpromptuModular | master cf87c91 (plugin.json 2.5.0) | 2.5.0 |
| NullSet | github.com/nicholas-j-denning/NullSet | master 8c24cac (2.0.0) | 2.0.0 |
| SynthKit | github.com/jerrysievert/synthkit | master 0d4aba3 (2.0.1) | 2.0.1 |
| SynthesisTechnology | none: manifest license "proprietary", no sourceUrl | NOT BUILT | 2.1.1 |

Sources live in ~/vcv-build/plugins-src/. All five built clean (exit 0).
SynthesisTechnology (E350 Morphing Terrarium, used twice in 02_instrument-B) is closed source (VCV commercial). No faithful open substitute;
nearest by role would be a wavetable/morphing oscillator such as AudibleInstruments Plaits. Nothing was swapped in the patch; Emory decides.

## Install dir
Rack 2.6.4 reads `~/.local/share/Rack2/plugins-lin-arm64/` (asset::userDir = $XDG_DATA_HOME or ~/.local/share, + "Rack2";
plugin.cpp: `plugins-<os>-<cpu>`). `make install` places `<Slug>-<ver>-lin-arm64.vcvplugin` there; Rack unpacks them on first start.

## Verification (static)
Each .vcvplugin was unpacked to scratch: plugin.so is ELF aarch64 and exports `init` (nm -D) for all five; each patch module checked against plugin.json module slugs.
Core modules are built into Rack.

| patch | plugin | module | status |
|---|---|---|---|
| 01_instrument-A.vcv | AudibleInstruments | Plaits | OK |
| 01_instrument-A.vcv | AudibleInstruments | Veils | OK |
| 01_instrument-A.vcv | SynthKit | Addition | OK |
| 01_instrument-A.vcv | AudibleInstruments | Branches | OK |
| 01_instrument-A.vcv | Fundamental | Scope | OK |
| 01_instrument-A.vcv | Fundamental | Octave | OK |
| 01_instrument-A.vcv | Fundamental | LFO2 | OK |
| 01_instrument-A.vcv | Fundamental | SHASR | OK |
| 01_instrument-A.vcv | Fundamental | ADSR | OK |
| 01_instrument-A.vcv | NullSet | WeightedQuantizer | OK |
| 01_instrument-A.vcv | Fundamental | Process | OK |
| 01_instrument-A.vcv | Fundamental | VCA-1 | OK |
| 01_instrument-A.vcv | ImpromptuModular | Clocked | OK |
| 01_instrument-A.vcv | Fundamental | Noise | OK |
| 01_instrument-A.vcv | Fundamental | Rescale | OK |
| 01_instrument-A.vcv | Core | AudioInterface2 | OK (built-in) |
| 01_instrument-A.vcv | AudibleInstruments | Ripples | OK |
| 01_instrument-A.vcv | Fundamental | 8vert | OK |
| 01_instrument-A.vcv | Fundamental | Mixer | OK |
| 01_instrument-A.vcv | Core | MIDI_CV | OK (built-in) |
| 01_instrument-A.vcv | Core | MIDICC_CV | OK (built-in) |
| 01_instrument-A.vcv | Core | MIDIMap | OK (built-in) |
| 01_instrument-A.vcv | Core | Notes | OK (built-in) |
| 01_instrument-A.vcv | Fundamental | Push | OK |
| 02_instrument-B.vcv | NullSet | WeightedQuantizer | OK |
| 02_instrument-B.vcv | SynthKit | Addition | OK |
| 02_instrument-B.vcv | AudibleInstruments | Branches | OK |
| 02_instrument-B.vcv | Fundamental | Scope | OK |
| 02_instrument-B.vcv | Fundamental | ADSR | OK |
| 02_instrument-B.vcv | Fundamental | LFO2 | OK |
| 02_instrument-B.vcv | AudibleInstruments | Veils | OK |
| 02_instrument-B.vcv | Fundamental | Mixer | OK |
| 02_instrument-B.vcv | Fundamental | Process | OK |
| 02_instrument-B.vcv | AudibleInstruments | Ripples | OK |
| 02_instrument-B.vcv | Fundamental | VCA-1 | OK |
| 02_instrument-B.vcv | ImpromptuModular | Clocked | OK |
| 02_instrument-B.vcv | Fundamental | Rescale | OK |
| 02_instrument-B.vcv | SynthesisTechnology | E350 | MISSING |
| 02_instrument-B.vcv | Core | AudioInterface2 | OK (built-in) |
| 02_instrument-B.vcv | Fundamental | Octave | OK |
| 02_instrument-B.vcv | Fundamental | SHASR | OK |
| 02_instrument-B.vcv | Fundamental | Noise | OK |
| 02_instrument-B.vcv | Fundamental | 8vert | OK |
| 02_instrument-B.vcv | Core | MIDI_CV | OK (built-in) |
| 02_instrument-B.vcv | Core | MIDICC_CV | OK (built-in) |
| 02_instrument-B.vcv | Core | MIDIMap | OK (built-in) |
| 02_instrument-B.vcv | Core | Notes | OK (built-in) |
| 02_instrument-B.vcv | Fundamental | Push | OK |
| 03_instrument-C.vcv | Fundamental | LFO | OK |
| 03_instrument-C.vcv | Fundamental | ADSR | OK |
| 03_instrument-C.vcv | AudibleInstruments | Veils | OK |
| 03_instrument-C.vcv | AudibleInstruments | Branches | OK |
| 03_instrument-C.vcv | Fundamental | Scope | OK |
| 03_instrument-C.vcv | Fundamental | Rescale | OK |
| 03_instrument-C.vcv | Fundamental | LFO2 | OK |
| 03_instrument-C.vcv | NullSet | WeightedQuantizer | OK |
| 03_instrument-C.vcv | Fundamental | Octave | OK |
| 03_instrument-C.vcv | AudibleInstruments | Ripples | OK |
| 03_instrument-C.vcv | Fundamental | Process | OK |
| 03_instrument-C.vcv | Fundamental | VCA-1 | OK |
| 03_instrument-C.vcv | ImpromptuModular | Clocked | OK |
| 03_instrument-C.vcv | AudibleInstruments | Braids | OK |
| 03_instrument-C.vcv | SynthKit | Addition | OK |
| 03_instrument-C.vcv | Core | AudioInterface2 | OK (built-in) |
| 03_instrument-C.vcv | Fundamental | Mixer | OK |
| 03_instrument-C.vcv | Fundamental | Noise | OK |
| 03_instrument-C.vcv | AudibleInstruments | Rings | OK |
| 03_instrument-C.vcv | Fundamental | SHASR | OK |
| 03_instrument-C.vcv | Fundamental | 8vert | OK |
| 03_instrument-C.vcv | Core | MIDI_CV | OK (built-in) |
| 03_instrument-C.vcv | Core | MIDICC_CV | OK (built-in) |
| 03_instrument-C.vcv | Core | MIDIMap | OK (built-in) |
| 03_instrument-C.vcv | Core | Notes | OK (built-in) |
| 03_instrument-C.vcv | Fundamental | Push | OK |
| 04_instrument-D.vcv | AudibleInstruments | Branches | OK |
| 04_instrument-D.vcv | Fundamental | Scope | OK |
| 04_instrument-D.vcv | Fundamental | ADSR | OK |
| 04_instrument-D.vcv | NullSet | WeightedQuantizer | OK |
| 04_instrument-D.vcv | Fundamental | LFO2 | OK |
| 04_instrument-D.vcv | SynthKit | Addition | OK |
| 04_instrument-D.vcv | Fundamental | Octave | OK |
| 04_instrument-D.vcv | AudibleInstruments | Veils | OK |
| 04_instrument-D.vcv | Fundamental | Rescale | OK |
| 04_instrument-D.vcv | Fundamental | Process | OK |
| 04_instrument-D.vcv | Fundamental | VCA-1 | OK |
| 04_instrument-D.vcv | ImpromptuModular | Clocked | OK |
| 04_instrument-D.vcv | AudibleInstruments | Ripples | OK |
| 04_instrument-D.vcv | Fundamental | 8vert | OK |
| 04_instrument-D.vcv | Core | AudioInterface2 | OK (built-in) |
| 04_instrument-D.vcv | Fundamental | Noise | OK |
| 04_instrument-D.vcv | Fundamental | Mixer | OK |
| 04_instrument-D.vcv | AudibleInstruments | Braids | OK |
| 04_instrument-D.vcv | Fundamental | SHASR | OK |
| 04_instrument-D.vcv | Core | MIDI_CV | OK (built-in) |
| 04_instrument-D.vcv | Core | MIDICC_CV | OK (built-in) |
| 04_instrument-D.vcv | Core | MIDIMap | OK (built-in) |
| 04_instrument-D.vcv | Core | Notes | OK (built-in) |
| 04_instrument-D.vcv | Fundamental | Push | OK |
| 05_flsamp.vcv | Core | AudioInterface2 | OK (built-in) |
| 05_flsamp.vcv | AudibleInstruments | Plaits | OK |
| 05_flsamp.vcv | Fundamental | ADSR | OK |
| 05_flsamp.vcv | Fundamental | VCA-1 | OK |
| 05_flsamp.vcv | ImpromptuModular | Clocked | OK |
| 05_flsamp.vcv | Fundamental | Noise | OK |
| 05_flsamp.vcv | Fundamental | 8vert | OK |
| 05_flsamp.vcv | Fundamental | SHASR | OK |
| 05_flsamp.vcv | NullSet | WeightedQuantizer | OK |
| 05_flsamp.vcv | Fundamental | LFO2 | OK |
| 05_flsamp.vcv | Fundamental | Random | OK |
| 05_flsamp.vcv | Core | MIDI_CV | OK (built-in) |
| 05_flsamp.vcv | Core | MIDICC_CV | OK (built-in) |
| 05_flsamp.vcv | Core | MIDIMap | OK (built-in) |
| 05_flsamp.vcv | Core | Notes | OK (built-in) |
| 05_flsamp.vcv | Fundamental | Push | OK |
| 05_flsamp.vcv | AudibleInstruments | Branches | OK |
| 05_flsamp.vcv | Fundamental | Mixer | OK |

Summary: every module in all 5 patches resolves except SynthesisTechnology/E350 (instrument B only).

## Launching Rack later (always-on JACK client on core 1, node.async)
Not done; nothing launched. Plan from ALWAYS_ON_PLAN.md transfers with these notes:
- Transfers: systemd unit pattern (CPUAffinity=1, LimitRTPRIO=60, CPUQuota, MemoryMax, gate on csound6 linked), `PIPEWIRE_PROPS='{ node.async=true node.lock-quantum=true node.lock-rate=true }'`,
  private rt.prio=50 jack.conf, graph.conf links for the audio outs, guard/watchdog, private MIDI virtual port.
- Differs: Rack is a GUI app (needs xvfb-run like Cardinal; `./Rack -h` is not headless, it still opens a window and an audio driver). It must be started with `-u` equivalents:
  run from `~/vcv-build/Rack` (system dir = cwd, `-s`), the patch as an argument; audio driver must be set to JACK in settings.json (`"audioDriver"`) before first run,
  otherwise Rack may open ALSA directly. Core AudioInterface2 port names are `Rack:Audio-1 out` style, not Cardinal's `audio-out1`: confirm at first launch.
  Rack has no Cardinal-style embedded MIDI; MIDI_CV/MIDIMap modules have `data: null` so a device must be assigned (use the private port).
  Rack loads plugins from the user dir above, so the unit needs HOME set. Engine threads: Rack spawns its own audio thread via JACK callback at the JACK RT prio; the rt.prio cap applies.
  Rack ships its own Core module, "Fundamental" is not built in (unlike Cardinal), so a missing plugin.so shows as a dummy module, not a crash.
  node.async effect on Rack is UNVERIFIED (70% as for Cardinal). Realtime viability of Rack alongside csound on the Pi 5 is untested.
