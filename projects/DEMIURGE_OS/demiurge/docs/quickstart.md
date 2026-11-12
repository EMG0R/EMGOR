# DEMIURGE Quickstart

The fastest path from "just booted" to "making sound."

> **Your names:** This doc uses `gorpi7` (username) and `demiurge.local` (hostname). Replace with whatever you set in Raspberry Pi Imager.

---

## The one file you edit

`~/demiurge/live.conf` — plain text, hot‑reloaded. Edit it over SSH, save, hear the change. The launcher polls the file's modification time once a second and reconciles: stages you didn't touch keep running, stages you added start, stages you removed stop, and the audio graph is re‑applied.

A minimal `live.conf` looks like this:

```ini
link = off
bpm  = 110

chain =
  ~/demiurge/examples/hello_world/hello.strudel
  ~/demiurge/examples/hello_patching/faust_delay.dsp
  ~/demiurge/examples/hello_patching/cpp_crush.cpp
  ~/demiurge/examples/hello_patching/csound_ringmod.csd
```

Two keys and a list:

- `link = on/off` — toggles Ableton Link network tempo/transport. `demiurge-clock` is respawned with the new flag on save.
- `bpm = 110` — initial tempo (applies when Link is off).
- `chain =` — ordered list of audio stages. They pipe top‑to‑bottom, left‑to‑right. Language is inferred from the file extension (`.ck`, `.csd`, `.pd`, `.scd`, `.dsp`, `.cpp`, `.strudel`, `.py`, `.nam`). `.dsp` and `.cpp` compile on demand. The launcher automatically appends `→ out` (the virtual sink), so you don't write it.

Hardware inputs are mono and addressed by **number**. Add `in1 ->` and/or `in2 ->` as the first lines of `chain =` to pipe physical input 1/2 into stage 0:

```ini
chain =
  in1 ->
  in2 ->
  ~/fx/reverb.dsp
```

There is no stereo input pair on DEMIURGE — every hardware input is mono. If you want both physical inputs into a stage, declare both numbers.

That's the whole user interface. `demiurge-clock` and the Pd clock bridge (when any stage is `.pd`) are implicit — you never list them.

---

## What `/boot/firmware/demiurge.conf` is for

The boot‑partition file still exists, but you rarely touch it. By default it points at `~/demiurge/live.conf`:

```ini
launch = /home/gorpi7/demiurge/live.conf
```

If `launch` is blank, the launcher falls back to `~/demiurge/live.conf` automatically. If you want to boot an alternate `live.conf` (e.g. a different show file), put its absolute path here. There is no other config format — `live.conf` is the only user-facing surface.

---

## Try it

With the default `live.conf` from the install: SSH in, run `sudo systemctl start demiurge`, and the reference chain comes up — Strudel drives a 4‑stage generative pipeline (Faust delay → C++ bitcrush → Csound ring‑mod → sink). Every stage tempo‑locks off `demiurge-clock`.

Edit `live.conf` over SSH:

```bash
ssh gorpi7@demiurge.local
nano ~/demiurge/live.conf
```

Flip `link = off` to `link = on` and save — the clock subprocess respawns with `--link=on` and joins any Ableton Link peer on the network. Comment out the `cpp_crush.cpp` line and save — that stage stops, the graph re‑applies without it, the rest of the chain keeps running without a glitch. Add your own file to the list and it joins the chain.

Every stage is subscribed bidirectionally to the shared MIDI bus (`Midi Through`, ALSA seq `14:0`). Clock ticks, controller hardware, and MIDI any stage writes all live in the same pool.

---

## MIDI‑only sidecars

Stages in `chain =` participate in the audio graph. For programs that should run and join the MIDI bus but NOT be wired into the audio chain — Strudel in pattern‑driver mode, Python drivers, clock‑reacting utilities — use a `midi =` block:

```ini
chain =
  ~/fx/reverb.dsp

midi =
  ~/patterns/driver.strudel
  ~/controllers/mod_wheel.py
```

Same hot‑reload semantics. Sidecars are supervised like any other stage; they just don't get audio‑port links applied.

---

## The auto‑managed devices block

The launcher writes this block near the top of `live.conf` every time your USB audio or MIDI devices change:

```ini
# DEMIURGE-DEVICES-BEGIN
# audio:
#   - Scarlett Solo 4th Gen Headphones / Line 1-2  (out only)
#   - Scarlett Solo 4th Gen Input 1 Inst/Line  (in only)
# midi:
#   - Launchkey Mini MK3
# DEMIURGE-DEVICES-END
```

It's a read‑only inventory — your hint about what hardware the launcher currently sees. Everything between the markers is rewritten on hotplug; don't edit inside it. The launcher only rewrites the block when `live.conf` has been idle for a few seconds so your in‑progress edits aren't clobbered.

---

## SSH in and see what's happening

```bash
# What's running right now?
systemctl status demiurge
pgrep -af 'chuck|pd|sclang|faust|csound|strudel'

# The audio graph — every link between every program
pw-link -l

# MIDI bus
aconnect -l
aseqdump -p "Midi Through"

# Launcher's own log
sudo journalctl -u demiurge -f
```

When the graph misbehaves, `journalctl -u demiurge` is the single source of truth — every hot‑reload, link, unlink, and scrub decision is logged there.

---

## Recording what's coming out

`pw-record` and `pw-cat --target` **silently capture the Scarlett mic** instead of the node you asked for. Use `pw-jack jack_rec` with literal JACK port names:

```bash
pw-jack jack_rec -f /tmp/mix.wav csound6:output1 csound6:output2
sox /tmp/mix.wav -n stat   # look for non-zero RMS
```

---

## More docs

- `config-reference.md` — full `live.conf` syntax and `demiurge.conf` keys.
- `language-setup.md` — per‑language quirks and how each language plugs into DEMIURGE.
- `clock.md` — `demiurge-clock` protocol reference and per‑language reader code.
- `gotchas.md` — the silent‑failure modes that cost real debugging time.
- `../INSTRUCTIONS.md` on the repo root — concept‑first recreation guide for the whole system.
