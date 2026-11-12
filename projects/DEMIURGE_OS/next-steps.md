# DEMIURGE — Next Steps

The foundation is done: PipeWire/WirePlumber layer, shared MIDI bus, global clock with optional Ableton Link, Rust launcher with event-driven USB audio aggregation, per-language wrappers. All seven languages verified bidirectionally (MIDI-in / MIDI-out / BPM-sync / audio-in / audio-out) on the Pi 5. Documentation and examples reflect the learned state.

This file is the forward-looking list — what could come next for anyone continuing the project. It deliberately does not prescribe milestones; pick what matters to you and carve your own.

---

## Immediately useful

- **Grow the virtual channel count beyond 4.** `demiurge-sink` and `demiurge-source` are 4-channel today (FL FR RL RR) — enough for two stereo interfaces. Three or four simultaneous stereo interfaces need the pipewire-virtual.conf channel count bumped and the aggregation pair table extended in `src/demiurge-launcher-rs/src/aggregate.rs`.
- **`wire_strudel_audio`** launcher step: after the strudel-runner registers `cpal_client_out`, auto-link `out_0/out_1` into whatever the next `chain =` stage in `live.conf` is. Today the user has to know that `cpal_client_out` is the JACK client name; the launcher should just do it.
- **Status line** at `~/.demiurge/status` — one-line summary (running programs, current BPM, xrun count) refreshed every second. Lets any shell see session health in one glance.
- **Per-stage output channel routing in session files.** Today every `patch … -> out` feeds `demiurge-sink:playback_FL/FR` and the aggregation layer decides which physical interface that lands on. A natural extension is `patch foo -> out.rear` or `patch foo -> out.2`, so a multi-interface rig can route specific stages to specific hardware outs declaratively.

## Language additions

- **Csound MIDI out** verified via `-Q14:0` in the hello example, but no example actually writes anything back. A "Csound reacts to a knob and echoes a CC back on channel 1" snippet would make the bidirectional story complete per language.
- **Python** as a first-class chain stage, not just a sidecar. `python3-jack-client` in the process callback, `mido` for MIDI — there's no example yet that acts as a full audio node.
- **Lua / LuaJIT** via Protoplug or a hand-rolled JACK client. The Pi has enough headroom; a 7th language would be a nice test of whether the pattern generalizes cleanly.

## Patching / composition

- **Per-stage bypass controllers**: every effect stage responds to a "dry/wet" CC so the chain can be auditioned incrementally from the shell. The hello_patching stages partly do this but not uniformly.
- **Named scenes** (`scene.conf` overlays) that change which CCs map to which controllers at runtime without restarting the session.
- **Multi-output chain** — fork the patch graph after the Faust delay so both a dry and wet tap reach separate output pairs independently (needs the per-stage output routing noted above).

## Clock / sync

- **MTC / LTC output** from `demiurge_clock` so the Pi can drive external sequencers / DAWs over SMPTE or MIDI Time Code.
- **Clock "record mode"** — dump every 0xF8/CC broadcast to a timestamped log so a session can be reconstructed exactly later.

## Hardware / platform

- **USB MIDI controller default mapping**: ship a sensible default CC map so any class-compliant USB MIDI controller works out of the box without per-device configuration.
- **Pi 5 thermal profile** for long sessions at 48k/64 — characterize xruns vs ambient temp and decide whether a case fan is required for anything longer than a studio session.

## Distribution / reproducibility

- **Boot image.** `dd` an SD card into a compressed `DEMIURGE.img` that boots cleanly on any Pi 5. Version it.
- **Read-only root partition** with `/boot/firmware/demiurge.conf` and `~/demiurge/` as the only writable surfaces. Makes field gigs survive power pulls.

## Creative / documentation

- **A sixth hello_world**: one patch in each language that sounds intentional (not just "proves the pipe works"). Short, listenable pieces that double as regression targets for audio quality after system updates.
- **Video pass**: record each hello_* playing, and each hello_patching stage solo/ensemble. Gives the project a face.
- **Reader's guide to `strudel-runner.mjs`'s audio callback** — swap the sawtooth voice for something with proper sample playback, mirror a subset of superdough's voices. This is the first place a user will want to fork.

---

Keep what's useful, ignore the rest. None of this is blocking.
