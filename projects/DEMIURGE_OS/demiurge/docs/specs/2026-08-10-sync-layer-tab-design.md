# Sync Layer tab — design spec

**Status:** spec only. Not built. Another agent owns the UI; this document is
the hand-off.
**Date:** 2026-08-10
**Context:** the sync-layer graph became worth exposing on the day it stopped
being a black box. Two routing decisions (direct-vs-loopback on output, and now
on input) are made automatically on every pw-mon tick, and until 2026-08-10 one
of them was silently broken — the chain's first stage had zero incoming links,
so the instrument never reached the engine in sync mode, and nothing in any UI
would have told you. That is the argument for the tab: not "let the user
re-patch", but "let the user SEE".

---

## The one-line version

A read-mostly status page that makes the current routing legible at a glance,
with a small number of high-value write actions behind it — backed by a new
`demiurge-graph` CLI which is the single write path, exactly as
`demiurge-interface` is for interface selection.

---

## Why this is mostly a display, not a patchbay

The temptation is to ship a patchbay: every port, every link, drag to connect.
Resist it, for three reasons.

1. **The launcher owns the graph and will fight you.** `graph::apply` runs on
   every pw-mon event and every live.conf reload. It computes a `desired` set
   and then actively scrubs every link into a session client that is not in it
   (`scrub_stray_session_links`), plus stray sink inputs, stray hardware
   playback links, and sink feedback loops. A link the user makes by hand in a
   patchbay UI is, by construction, not in `desired`, so it will be deleted on
   the next tick — typically within a second, and certainly on the next
   hot-plug. A patchbay whose edits silently evaporate is worse than no
   patchbay. Any real re-patching feature has to write something the launcher
   reads (live.conf, or a new persisted overlay), not poke pw-link.

2. **480x800, played standing up.** The graph at rest has ~10 nodes and ~20
   links. Rendering that as a node editor on a 5-inch touchscreen produces
   targets far below the ~9 mm minimum for reliable finger use, and the user is
   typically standing at an instrument, not seated. Everything on this tab has
   to be legible at arm's length and operable with a thumb.

3. **The failure this tab exists to surface is a MISSING link, not a wrong
   one.** "csound has no input" is a sentence. It does not need a diagram.

So: status first, and a deliberately short list of actions.

---

## What it should show

Four blocks, top to bottom, in decreasing order of how often the answer matters.

### 1. Path — the headline

One line each for the two directions, in the same language the launcher logs:

```
IN    HiFiBerry  →  neptrPhase4      direct
OUT   neptrPhase4  →  HiFiBerry      direct
```

with the mode word being the load-bearing part: `direct` or `loopback`. When it
is `loopback`, say why in the same breath, because "loopback" alone reads as a
fault when it is usually correct:

```
IN    HiFiBerry + Volt 2  →  neptrPhase4    loopback (2 interfaces)
OUT   neptrPhase4  →  demiurge-sink         loopback (Bluetooth speaker)
```

The reasons are already known to the code — `direct_input_device()` /
`direct_output_device()` return `None` for exactly three causes (multiple
devices, Bluetooth present, no device at all) and the CLI should report which.

A **red state** is needed and is the entire point of the block: when a
direction has no links at all, say so loudly.

```
IN    ⚠ no input reaching neptrPhase4
```

### 2. Clock

```
CLOCK   HiFiBerry   48000 Hz   quantum 64   (1.33 ms)
```

Plus, when it applies, the two states that confuse people:
- `snd-aloop (no interface attached)` — the always-on virtual clock. Users who
  do not know this exists conclude the rig is broken when it is idling
  correctly.
- a follower list when more than one interface is attached, since the sticky
  master rule (first-connected wins) is genuinely surprising.

### 3. Measured latency

Round-trip if it has ever been measured on this interface, with its date, from
whatever `testing/latency` last wrote — and an explicit `not measured` when it
has not. Do **not** compute and display a number from buffer arithmetic. That
is precisely the habit that produced years of confidently wrong figures on this
project; see `docs/latency-measurements.md`. If there is no cable measurement,
the honest display is "not measured", not an estimate.

### 4. Links — collapsed by default

The full port-level link list, one row per link, behind a disclosure. This is
the diagnostic escape hatch: it is what you screenshot and send when something
is wrong. It does not need to be pretty and it does not need to be
interactive. Group by node, show `source:port → dest:port`, monospace, scrollable.

---

## What it should let you DO

Three actions. All of them are things that currently require SSH.

| Action | Backed by | Why it earns its place |
|---|---|---|
| **Re-apply graph** | `demiurge-graph apply` | Idempotent, safe, instant. The universal "it looks wrong, kick it" button, and it is genuinely the fix when a link got scrubbed by a race. Lowest-risk useful button on the tab. |
| **Input on/off** | `demiurge-graph input on\|off` | Writes `input = off` in live.conf. Real musical need: kills a feedback loop when the mic and the monitors are in the same room, without unplugging anything. Bounded, reversible, one bit. |
| **Reset to default** | `demiurge-graph reset` | Drops any overlay and re-derives from live.conf. The way back from any state the user got themselves into. |

### What to leave out, and why

- **Per-link connect/disconnect.** See above: the launcher scrubs it within a
  tick. Offering it would teach the user that the tab lies.
- **Forcing direct-vs-loopback mode.** It is derived from what is attached, and
  the derivation is right. A manual override is a footgun that presents as an
  intermittent audio fault weeks later, when the user has long forgotten they
  set it — and it would have to be reconciled against hot-plug, which is where
  the complexity explodes.
- **Quantum / period-size.** These belong with the latency tooling, not here,
  and period-size in particular has hard-locked this board before
  (`gotchas.md` Q7c-i). Not a touchscreen control.

---

## The CLI behind it — ALREADY BUILT, do not write a second one

`src/wrappers/demiurge-graph` exists as of 2026-08-10 (written in parallel with
this spec by another agent on the same day, prompted by the same bug). It is
the single write path onto the graph and the UI must call it rather than
shelling out to `pw-link`. Its surface:

```
demiurge-graph                    human-readable picture of the audio path
demiurge-graph --list             machine lines for UIs
demiurge-graph --status-machine   one line: mode|in_path|out_path|dangling|links
demiurge-graph connect    <src> <dst>
demiurge-graph disconnect <src> <dst>
demiurge-graph reset
```

It already reports direct-vs-loopback per direction, already flags dangling
engine ports as `warn|` lines, and already exits **3** for "there is no graph
right now" (bypass mode, or PipeWire not running) so a UI can tell "nothing to
show" apart from a real failure. Honour that exit code — rendering an error
banner when the user has simply chosen bypass mode is the obvious wrong move.

Two consequences for the tab:

1. **The blocks above map onto `--status-machine` and `--list` as they stand.**
   The one thing this spec asks for that the CLI does not yet expose is the
   *reason* a direction is on the loopback (multi-interface vs Bluetooth vs no
   device vs disabled). Until it does, the tab should show `loopback` without
   inventing an explanation.
2. **The tab does not need a `--json` mode invented for it.** `--list` and
   `--status-machine` are the house style (`demiurge-interface --list` is the
   same shape). Adding a third parse format for one consumer is how you end up
   maintaining three.

### The `input on|off` action needs a home

`demiurge-graph` does not currently own live.conf's `input =` key (added to the
launcher on 2026-08-10 alongside the direct-input path; `input = off` opts the
chain head out of the default hardware input). Either `demiurge-graph input
on|off` or `demiurge-audio` should own that write. It should NOT be written by
the UI directly, and it should not be a `connect`/`disconnect` pair — it is a
persistent policy bit, and expressing it as link surgery would put it back to
being scrubbed on the next tick.

### Persistence: `graph.conf`

`demiurge-graph connect` records overrides in `~/demiurge/graph.conf` as
`connect = <src> -> <dst>`, and its own header notes that the launcher does not
read that file yet, so a hand-made link is live now but not guaranteed to
survive the next graph event. **The launcher side of that handoff is the
prerequisite for the tab shipping any re-patch affordance at all** — without it
the tab would offer a button whose effect silently expires. Sequence the work
that way: launcher reads `graph.conf` first, tab second.

---

## Placement

It is a **diagnostic** tab, not a performance tab. It belongs next to whatever
shows interface selection — the two answer adjacent questions ("which device?"
/ "how is it wired?") and the interface tab is where a user goes when audio is
wrong. It should never be the tab the UI boots into.

---

## Open questions for whoever builds it

1. Does the launcher publish status today, or does `demiurge-graph` shell out
   to `pw-link -l` and re-derive? Prefer publishing. Re-deriving means the
   direct-mode predicate exists twice and they will drift.
2. Should `input on|off` be global or per-set? live.conf is rewritten wholesale
   by `demiurge-set` on every patch switch, which is why the interface pick
   needed a separate global file. `input` probably wants the same treatment, or
   it will silently reset every time the user changes patch.
3. Is there an existing measured-latency artefact for the `latency` block to
   read, or does `testing/latency` need to start writing one? As of this spec
   the measurements live in a hand-maintained markdown doc, which a CLI should
   not be parsing.
