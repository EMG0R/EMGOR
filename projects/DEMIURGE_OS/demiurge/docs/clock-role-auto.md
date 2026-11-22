# Clock-role auto-switch (device-presence driven)

Before this feature, whether Demiurge or the external groovebox (Pocket
OpGorator / "PMOR", a Daisy Seed device) is clock master was 100% manual —
a human ran `demiurge-sync opgorator` or `demiurge-sync demiurge` by hand
every time the device was plugged in or unplugged. See
[`clock.md`](clock.md) for the full CC116 role-negotiation protocol and
`sync.conf` format this builds on; this document only covers the
auto-switch layer added on top of it.

## What it does

The launcher (`demiurge-launcher-rs`) now watches for the OpGorator/PMOR
device and calls `demiurge-sync` for you:

- **Device appears** → Demiurge becomes **follower** (`demiurge-sync --auto
  opgorator`).
- **Device disappears** → Demiurge reverts to **master** (`demiurge-sync
  --auto demiurge`).

No config, no button — it just works when the cable goes in or comes out.

## Where the code lives

`src/demiurge-launcher-rs/src/clockrole.rs` owns all of this:

- `PresenceTracker` — a pure edge-detection state machine. It only ever
  says "act" on the tick where presence actually *changes* — repeated
  "still present" / "still absent" observations are silent. This is what
  keeps the launcher from re-injecting CC116 on every unrelated USB
  hot-plug or every 2s heartbeat while the device sits steady.
- `pmor_present()` — the presence oracle. Checks **both**:
  - the PipeWire graph for an `alsa_*` node whose client name contains
    `OpGorator`, `Daisy`, or `PMOR` (same substring patterns
    `supervisor::resolve_opgorator_client()` already uses for graph
    wiring), and
  - `devices::scan_usb_midi()`'s ALSA-seq client list, for the same
    substrings.
- `apply_action()` — shells out to `demiurge-sync --auto <role>`. This
  module **never writes `~/demiurge/sync.conf` directly** — see "One
  write path" below.

It is wired into `main.rs` at three points:

1. **Boot settle** — once, right after the first `graph::apply()` /
   `midi::wire_bus()` pass, so a device already attached at launcher
   startup is picked up immediately rather than waiting for the next
   hot-plug event (no boot-time race).
2. **`Event::UsbDeviceChange`** — pw-mon's existing USB-audio hot-plug
   signal (`events::spawn_pwmon`), for the common case (device has an
   audio interface).
3. **`Event::Heartbeat`** (every ~2s) — re-evaluates `pmor_present()`
   regardless of what triggered the tick. See "The MIDI-only gap" below
   for why this exists.

All three paths feed the *same* `PresenceTracker` instance, so no matter
which one notices the transition first, only one action fires.

## Evidence, not presence (2026-10-03)

Plugging PMOR in flipped the role to `opgorator` even though PMOR emitted no
clock (a CC117 "playing" with an empty tick stream). The election now needs
EVIDENCE: PMOR counts as sounding only if `clock_status.conf` has
`pmor_transport = playing` AND `ext_clock = yes`. `ext_clock` is written by
the daemon: a sustained (>= 24 ticks, gaps < 0.25 s, last seen within 5 s)
stream of 0xF8 that is NOT the daemon's own Midi Through echo. A status file
without the field (older daemon) reads as no evidence, so the launcher stays
master -- rebuild and install both together. Rebuild commands:
`clock.md`, "Rebuilding the daemon and launcher on the Pi".

## One write path

`sync.conf` has exactly one writer: the `demiurge-sync` CLI
(`src/wrappers/demiurge-sync`) — same doctrine as `interface.conf` and
`live.conf` elsewhere in this project (one file, one owner, everyone else
goes through the CLI). The launcher's auto-switch is not an exception: it
calls the same script a human calls, just with a `--auto` flag tacked on.
That flag exists purely so the script can tell "a human typed this" apart
from "the launcher noticed a device" — the file format and role semantics
don't change, only who gets credit for the write (see `source` below).

## Manual always wins

If a human has explicitly run `demiurge-sync opgorator` or `demiurge-sync
demiurge` (no `--auto`), plugging or unplugging the device must **not**
silently steal the role back. `sync.conf` gained a second key:

```
role = opgorator
source = auto
```

- `source = manual` — set by any plain (non-`--auto`) `demiurge-sync
  opgorator|demiurge` call. A human, or a UI button acting on the human's
  behalf, chose this.
- `source = auto` — set by `demiurge-sync --auto opgorator|demiurge`
  (only the launcher's presence auto-switch calls this form).
- Missing file, or a missing `source` key (a `sync.conf` written before
  this feature existed) → treated as `auto`, so upgrading in place does
  not silently disable auto-detection.

**The rule, enforced in `demiurge-sync` itself** (not just the launcher —
belt and suspenders): an `--auto` call is rejected outright — role left
untouched, CC116 not sent, file not rewritten — whenever the file
currently says `source = manual`. Only a plain (non-`--auto`) call can
move `source` off `manual`. A plain call always proceeds regardless of
the current source — that IS the human overriding whatever was there.

The same rule is duplicated as a pure, hermetically-tested function in
Rust, `clockrole::auto_may_apply(Source) -> bool`, purely so the launcher
can skip shelling out at all when it already knows the call would be
rejected (and log why) — `demiurge-sync` remains the actual enforcement
point and single writer.

## The MIDI-only gap

`pw-mon` (and therefore `events::UsbDeviceChange`) only watches PipeWire
**audio** nodes (`alsa_output.*` / `alsa_input.*` —
`util::is_selectable_audio_node`). A device connected **MIDI-only** (no
audio interface registered) never creates one of those nodes, so it never
fires that event — full stop, regardless of hot-plug timing.

Two things close this gap:

1. **Heartbeat polling.** `pmor_present()` is re-evaluated on every
   `Event::Heartbeat` tick (~2s) independent of whatever triggered it,
   and it checks the ALSA-seq client list (`devices::scan_usb_midi()`, now
   `pub`) in addition to the PipeWire graph. A MIDI-only device is
   therefore detected within one heartbeat of appearing or disappearing,
   with or without any hot-plug event ever reaching the launcher.
2. **udev companion rule.** `setup-demiurge.sh`'s MIDI hot-plug udev rule
   (`/etc/udev/rules.d/99-demiurge-midi.rules`) previously fired
   `demiurge-midi-connect.sh` on `ACTION=="add"` only. It now also fires
   on `ACTION=="remove"`, for symmetry with the bus-wiring job that
   script already does. This is *not* the mechanism that fixes
   role-tracking (the launcher process isn't a udev subscriber at all) —
   that's the heartbeat poll above — but it closes the same asymmetry in
   the adjacent script and costs nothing: re-running the connect script
   against a vanished ALSA-seq client is a harmless, idempotent no-op.

Both are deliberately fail-safe: a device that's absent, or that vanishes
mid-scan, just means `pmor_present()` returns `false` and/or
`scan_usb_midi()`/`aconnect -l` returns an empty or stale list — never a
panic, never a hang. Nothing here can wedge the launcher.

## Testing

`clockrole.rs` has unit tests for the two pieces of pure logic:

- `PresenceTracker` edge detection — absent→present→present→absent
  sequences (and flappier ones) produce exactly one action per genuine
  transition, none on repeats.
- `parse_source` / `auto_may_apply` — the manual-vs-auto override rule,
  including legacy-file defaulting and case-insensitivity.

Run them with:

```sh
cd src/demiurge-launcher-rs
cargo test
```

To exercise `demiurge-sync`'s side of the contract directly (works on
any machine, no ALSA/PipeWire required — degrades gracefully when
`aseqsend` is missing, e.g. on macOS):

```sh
demiurge-sync opgorator            # source=manual
demiurge-sync --auto demiurge      # rejected: manual wins, file unchanged
demiurge-sync --list               # role|bpm|source_alive — unchanged format
```
