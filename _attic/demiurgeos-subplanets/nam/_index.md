---
id: emgor.papers.demiurgeos.nam
title: NAM
blurb: Neural Amp Modeler as a chain stage — drop a .nam file in, done
parent: emgor.papers.demiurgeos
source: ______2026NEW/DEMIURGE_OS/nam
downloads:
  - files/nam_stage.csd
  - files/demiurge-run-nam
tags: [nam, neural-amp-modeler, csound, opcode, guitar]
updated: 2026-07-28
draft: false
---

# NAM

Neural Amp Modeler runs as a first-class DEMIURGE language. Drop a `.nam` amp profile into the chain like any other stage:

```ini
chain =
  in1 ->
  ~/profiles/jcm800_crunch.nam
  ~/fx/reverb.csd
```

Guitar in, modeled amp, Csound reverb, out. No amp sim GUI, no plugin host.

## How it works

The launcher sees `.nam`, dispatches a thin wrapper that resolves the profile to a (directory, index) pair and execs the standard Csound wrapper with a fixed template orchestra (`nam_stage.csd`). Csound auto-loads `csound-nam.so`, a custom plugin providing the `NAMProcess` opcode, which drives NeuralAmpModelerCore directly. Audio I/O, JACK wiring, RT scheduling, and MIDI are handled identically to any other Csound stage — no new infrastructure.

## Real-time safety

`NamLoader` runs a background thread that watches for profile-index changes, calls `nam::get_dsp()` off the audio thread, and hot-swaps the model behind a mutex. The audio thread only acquires the mutex to copy a `shared_ptr` — no blocking I/O ever touches the RT path. Profiles switch live, mid-performance.

## Live control

The template opens an OSC socket (default port 9000):

| Address | Meaning |
|---|---|
| `/nam/idx` | 0-based profile index — hot-swap the amp |
| `/nam/bypass` | 1 = passthrough, 0 = NAM active |

```sh
oscsend demiurge.local 9000 /nam/idx i 2
```

The same `csound-nam.so` plugin is embedded inside NEPTR's full effect orchestra; this standalone path is for anyone who wants NAM in a chain without the rest of NEPTR. Profiles come from ToneHunt / tone3000, organized however you like — `{amp}_{gain_stage}.nam`.
