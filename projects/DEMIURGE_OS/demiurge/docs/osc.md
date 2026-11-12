# OSC on DEMIURGE

DEMIURGE has two control fabrics, and they are deliberately different:

| | **MIDI Through** (`Midi Through`, ALSA seq 14:0) | **OSC** |
|---|---|---|
| Shape | Broadcast / shared namespace | Point-to-point / addressed |
| Who hears it | Every program that subscribes | Only the socket you send to |
| Good for | Clock (`0xF8`), BPM (ch16 CC119), the note pool | "set `/synth/cutoff` on *that* stage", patch loads, named params |
| Provided by | The kernel (`snd-seq-dummy` merge port) | Each program's own UDP socket |

The MIDI bus is a **pool** because the kernel hands us a free many-to-many merge port (see `INSTRUCTIONS.md` §4). **OSC has no such primitive** — there is no "Midi Through for OSC" — and it doesn't need one, because OSC's whole strength is *addressing* a specific endpoint. Forcing OSC into a broadcast pool would throw away the thing OSC is good at.

So the rule on DEMIURGE:

> **MIDI is the shared coordination bus. OSC is per-stage, point-to-point control. A stage that wants OSC opens its own socket; you do OSC *inside* the audio program, in its native objects.**

DEMIURGE runs **no OSC router daemon** by design. The only shared infrastructure it provides is a reserved **port range** so stages and tooling agree on where to listen without hardcoding.

## The port convention

`demiurge-env.sh` (sourced by every `demiurge-run-*` wrapper) exports:

```sh
DEMIURGE_OSC_BASE=9000      # default; override by exporting before launch
```

- **Reserved range:** `9000–9099` for DEMIURGE stage control.
- **Convention:** a chain stage at index *N* listens on `DEMIURGE_OSC_BASE + N`.
- **Clear of:** SuperCollider (`scsynth` 57110, `sclang` 57120) and the RNBO runner's control port (UDP **1234**, the runner's own app default — *not* part of this range; see below).

Why a base port and not a per-stage variable yet: assigning each stage its own `DEMIURGE_OSC_PORT` is a launcher (Rust) change tracked in the RNBO integration spec (`specs/2026-06-05-rnbo-demiurge-integration-design.md`, Phase 1). Until then, `DEMIURGE_OSC_BASE` reserves the range and gives every stage a documented anchor it can offset from.

## Using OSC in each language

Every audio language already has native OSC — DEMIURGE does not mediate it:

| Language | Receive | Send |
|---|---|---|
| SuperCollider | `OSCdef(\x, {...}, '/addr')` | `NetAddr("127.0.0.1", port).sendMsg("/addr", v)` |
| Pure Data | `netreceive -u`, `oscparse` | `oscformat`, `netsend -u` |
| ChucK | `OscIn in; in.port(p);` | `OscOut`/`OscMsg` |
| Csound | `OSClisten` | `OSCsend` |
| Python | `python-osc` / `liblo` | same |

Bind to your stage's assigned port (e.g. `DEMIURGE_OSC_BASE + index`) so you never collide with another stage.

## Testing OSC by hand

`liblo-tools` is installed by the setup script (Phase 1), so you get CLI OSC for free:

```sh
oscdump 9000                       # watch everything arriving on stage 0
oscsend localhost 9000 /test f 0.5 # poke a stage
oscsend localhost 1234 /rnbo/inst/0/params/cutoff f 0.4   # poke the RNBO runner
```

## The RNBO runner (first real OSC consumer)

The RNBO OSCQuery runner listens on **UDP 1234** and auto-exposes every named parameter / inport / outport. DEMIURGE drives it point-to-point from `live.conf` (patch-slot load + parameter values), replacing the runner's web panel as the user-facing surface. That control path is exactly the model above: addressed OSC to one known endpoint, no pool required. Full design: `specs/2026-06-05-rnbo-demiurge-integration-design.md`.

## What DEMIURGE deliberately does *not* do

- **No central OSC router/dispatcher.** It would re-create a broadcast model OSC doesn't want, add a daemon to a lean Pi, and solve a problem we don't have.
- **No OSC clock pool.** Tempo/transport live on the MIDI bus (`clock.md`). If an OSC-native program ever needs sync without parsing MIDI, the right move is a small bridge that *republishes* clock/BPM to a fixed OSC address — added only when something needs it, not before.
