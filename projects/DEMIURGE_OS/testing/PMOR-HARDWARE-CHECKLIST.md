# PMOR (Pocket OpGorator) Hardware Checklist

Ordered, concrete steps for the first real-hardware session. Written
2026-08-12, right before the device's first arrival, alongside the new
pre-hardware test suites this checklist references:

- `testing/clock/run.sh` — 172 checks: PLL/ladder logic, realistic tick-stream
  simulation (dup + jitter + USB bursts), role-flip/stall/replug churn, fuzz.
- `src/demiurge-launcher-rs` (`cargo test` in that dir) — 26 checks:
  presence edge-detection, manual-vs-auto override, end-to-end role-handoff
  composition.
- `testing/neptr-clock/run.sh` — 7 checks + 1 cross-file consistency check:
  the csound-side loop-vs-external tempo arbiter, and the
  `gkGLITCH_extBpmSeen` latch regression.

Every item below is tagged:

- **[SIMULATED — low risk]** — a pre-hardware test already exercises this
  logic path with synthetic input. Failure here would mean the simulation's
  assumptions about the hardware were wrong, not that the logic itself is
  buggy — still worth a quick look, but not where surprises are expected.
- **[UNKNOWN — genuinely untested]** — nothing before today could exercise
  this: real USB enumeration, real analog/digital jitter, real timing
  against a real device, physical UI, actual audio content.

Steps are ordered so an early failure stops you before you waste time on
later steps that depend on it working (enumeration before MIDI before audio
before clock-lock before looper before a long soak).

---

## 0. Before you plug anything in

- [ ] **Power everything down first if this ever touches Pi GPIO/HAT
      headers.** (Not expected for a USB-only device, but if any session
      also involves re-seating a HAT: power off, double-check pin
      alignment before applying power — a previous Pi was lost to a
      mis-seated HAT.) **[N/A for a pure-USB device — confirm PMOR really is
      USB-only before skipping this.]**
- [ ] Confirm the Mac/Pi you're testing on has a free USB port and, if
      testing MIDI loopback claims below, a way to run `aseqdump`/`arecord`/
      `lsusb` (Linux/Pi) — this checklist assumes the **Pi**, since that's
      where `demiurge-clock`, `demiurge-sync`, and the `opgorator` chain
      token actually run. macOS was only ever the pre-hardware simulation
      host.

## 1. USB enumeration — **[UNKNOWN]**

The single biggest unknown: does the real TinyUSB composite descriptor
(UAC2 capture-only + MIDI + CDC) enumerate cleanly, especially **with the
OLED attached** (extra current draw / extra I2C traffic that could disturb
USB timing on a shared bus)?

```sh
lsusb -v 2>&1 | grep -A5 -i "opgorator\|daisy"
```
Expect: a composite device, product string **`Pocket OpGorator`** (per the
design spec — Demiurge's presence matcher looks for `OpGorator`/`Daisy`/
`PMOR` substrings, see `clockrole.rs::MATCH_PATTERNS` and
`supervisor.rs::resolve_opgorator_client`). Failure here means the presence
detection this whole checklist depends on (`demiurge-sync`, auto role
switch, `opgorator` chain token) has nothing to match against — **stop and
fix enumeration before continuing.**

```sh
arecord -l | grep -i "opgorator\|daisy"
```
Expect: one capture-only device (no playback device — capture-only is a
hard design requirement, see design spec §2 "Capture-only is a hard
requirement"). **If a playback interface shows up too, that's a real bug**:
a second USB playback sink silently knocks Demiurge out of direct-output
mode (+1 quantum of output latency on the MAIN path,
`aggregate.rs::direct_output_device`) — the opposite of what this device is
supposed to do.

```sh
arecord -D hw:OpGorator -c 2 -r 48000 -f S16_LE -d 5 /tmp/pmor_test.wav
sox /tmp/pmor_test.wav -n stats
```
Expect: 48 kHz fixed (firmware-declared, per design spec), non -inf RMS if
there's signal on the Daisy's ADC. Also check whether it actually came up
16-bit or 24-bit — the design spec calls 24-bit "attempted first, 16-bit
the tested fallback" specifically because 24-bit UAC2 on full-speed USB is
where community builds get fragile. **This determines a real format
question no simulation could answer.**

```sh
aconnect -l | grep -i "opgorator\|daisy"
```
Expect: a MIDI client, both directions eventually (Demiurge → device needs
the reverse-subscription fix from the design spec, `demiurge-midi-connect.sh`
mirroring `aconnect 14:0 "$client":0` so the Daisy hears `0xF8`/`CC116`/
`CC119` — check this explicitly, it's new and easy to have missed on deploy).

## 2. MIDI clock wiring — **[UNKNOWN, but with simulated fallback logic]**

Before trusting any recovered-BPM number, confirm the raw wire protocol is
actually what everything downstream assumes.

```sh
aseqdump -p "Midi Through" | grep -E "Clock|Start|Stop|Continue|controller 116"
```
- Set the OpGorator sequencer running. Expect a steady stream of `Clock`
  events (0xF8) — eyeball the timestamps, they should be roughly evenly
  spaced (24 per quarter note at whatever tempo the OpGorator is set to).
- **Measure the actual duplicate rate and offset.** `testing/clock/
  test_pll.cpp`'s test 7a/7b simulations assumed "~3% of ticks duplicated,
  0.3-1.5ms after the original" based on a prior measured range — confirm
  this is still accurate on THIS unit/firmware revision. If the real
  duplicate rate or offset is meaningfully different (much higher rate, or
  offset creeping toward the 6.25ms floor), the simulation's "should be
  safe" conclusion needs re-checking, not just re-trusting.
- Confirm `controller 116` appears when the OpGorator claims/releases
  master — value 1 on claim.

## 3. Role negotiation (CC116) round-trip — **[SIMULATED — low risk for the
   launcher's logic; UNKNOWN for the real CC116 wire round-trip]**

`src/demiurge-launcher-rs`'s `clockrole.rs` PresenceTracker + manual/auto
gating is now covered by 26 passing tests including full plug/unplug/replug
churn and manual-override-blocks-then-resumes composition — the pure LOGIC
of "device appears → auto role set → device vanishes → reverts" should not
surprise you. What's untested is the real round trip:

```sh
demiurge-sync                 # human status: role, live bpm, source alive?
demiurge-sync --list          # role|bpm|source_alive
```
- [ ] Plug in the OpGorator → within ~2s (heartbeat interval, see
      `clockrole.rs::pmor_present` doc comment), `demiurge-sync` should show
      `role = opgorator`, `source = auto`.
- [ ] Unplug it → within ~2s, reverts to `role = demiurge`.
- [ ] `demiurge-sync demiurge` (explicit, no `--auto`) while plugged in →
      role should NOT flip back to opgorator on the next heartbeat (manual
      wins). Confirmed in `clockrole.rs`'s
      `end_to_end_manual_override_blocks_mid_churn_then_auto_resumes` test
      at the logic level — verify the real sync.conf write survives a real
      heartbeat cycle.
- [ ] `demiurge-sync --auto opgorator` should never be typed by a human —
      confirm nothing in the UI accidentally exposes it as a button (per
      the wrapper's own doc comment, this form is launcher-only).

## 4. Recovered BPM accuracy — **[UNKNOWN — the actual number]**

The PLL math itself is heavily simulated (`testing/clock/run.sh`, tests
1-9, 172 checks total) and should NOT be where bugs hide. What's unknown is
whether the REAL signal matches the simulation's assumptions closely enough
for those guarantees to transfer.

```sh
tail -f ~/.demiurge/logs/clock.log
# expect: clock: status bpm=<X> role=follower source=external-midi
```
- [ ] Set the OpGorator to a known tempo (e.g. 120.0 BPM if the firmware
      exposes an exact value). Compare the daemon's recovered/broadcast BPM
      against it. **Simulation predicts:** long-run/settled accuracy within
      0.1%, single-window transients up to ~0.5% under the realistic model,
      and up to ~12% under an adversarial 8-15ms-spaced multi-tick-burst
      model that may or may not match real hardware (see test 7b's comment
      in `test_pll.cpp` — this is a real, documented limitation of
      single-outlier trimmed-mean filtering, not a bug). **If real hardware
      shows worse than ~12% transients, or the long-run value is off by
      more than 0.1%, that's a genuine finding** — the simulation's burst
      model may not match what real USB batching looks like.
- [ ] Change tempo on the OpGorator mid-stream — confirm Demiurge follows
      within a reasonable number of beats (test 4's simulation: settles
      within <100 ticks, ~4 beats, for a 120→140 jump).
- [ ] Pull the USB cable mid-stream (or power off the OpGorator) — confirm
      Demiurge freewheels at the last tempo for ~2s then either keeps
      freewheeling or (if `demiurge-sync` / auto-detect reverts role)
      reverts to master cleanly. **[SIMULATED — low risk]**: `test_pll.cpp`
      test 8 runs 25 rounds of exactly this churn (lock → stall → replug at
      a new tempo) and asserts no NaN/negative/zero BPM and clean re-lock
      every time.

## 5. Audio routing — **[UNKNOWN]**

```sh
# In live.conf's chain:
chain = opgorator
```
- [ ] With `chain = opgorator` present, confirm audio from the OpGorator
      actually reaches the mix — tap it:
      ```sh
      pw-jack jack_rec -d 4 -f /tmp/opgorator_tap.wav "DEMIURGE:monitor_FL" "DEMIURGE:monitor_FR"
      sox /tmp/opgorator_tap.wav -n stats
      ```
- [ ] **Direct-output regression** (design spec §6, explicitly called out
      as a test to run): with the OpGorator attached and only one OTHER
      wired output interface present, confirm Demiurge is STILL in
      direct-output mode (no extra quantum of latency) — the OpGorator
      being capture-only should mean it never appears in `output_sinks()`
      at all. If it somehow does show up as an output sink, that's a real
      bug (violates the "capture-only is a hard requirement" design
      constraint) and would silently cost a quantum of MAIN-path latency.
- [ ] Confirm the OpGorator does NOT get merged into the primary
      `demiurge-source` capture chain — `aggregate.rs` explicitly filters
      `OpGorator`/`Daisy` substrings out of `enumerate_usb_sources()` so it
      stays on its own parallel `opgorator` chain token instead. If it DOES
      show up merged in, the two audio paths (main input + opgorator chain)
      would double up.
- [ ] WirePlumber driver pin: confirm the OpGorator node never gets elected
      graph clock master (`priority.driver = 0`, `priority.session = 0` per
      the design spec) — `pw-cli info <node>` or `wpctl status` and check
      which node is actually driving the graph clock.

## 6. Looper grid-true capture — **[UNKNOWN — no simulation exercises real
   audio against the real looper]**

This is the part of the design spec (§4) most likely to only reveal itself
by ear/by measurement on real hardware — nothing in this pre-hardware pass
touched `NEPTR phase4/csound/effects/looper.orc` at all.

- [ ] With the OpGorator as clock master, record a loop on the NEPTR
      looper. Click off deliberately EARLY (before a beat) — confirm
      recording continues to the next grid line rather than truncating
      mid-beat.
- [ ] Click off deliberately LATE — confirm the length rounds back to the
      nearest grid line, not forward.
- [ ] Confirm loop START is grid-referenced too (phase-locked playback —
      the loop replays at the same clock phase it was performed at, not
      shifted).
- [ ] Compare loop length to `gkLpMasterLen`/derived BPM math via
      `/cb_bpm`/`/cb_subdiv` OSC telemetry (chase_bliss_clock.orc) if a UI
      or OSC monitor is available — cross-check against the arbiter logic
      that `testing/neptr-clock/run.sh` already validates purely (all 3
      priority rules + the `gkGLITCH_extBpmSeen` latch, 7/7 checks). If the
      arbiter's DECISION (loop-derived vs external) is ever visibly wrong
      on real hardware, that's a genuine regression since the decision
      logic itself is now regression-tested against the real
      `chase_bliss_clock.orc` file.

## 7. csound-side arbiter, end-to-end — **[SIMULATED — low risk for the
   decision logic; UNKNOWN for the real MIDI-out leg]**

`testing/neptr-clock/run.sh` validates instr 100's priority rules (no-loop
fallback, external-fresh-wins, external-stale-loop-wins) against the REAL
`chase_bliss_clock.orc` file, and separately proves the
`gkGLITCH_extBpmSeen` latch regression stays fixed. What it CANNOT validate
without hardware: the actual `midiout` call sending CC118 back out. Per
that file's own header comment, this was "believed fine... but could NOT
be verified end-to-end from the Mac (no MIDI output port to observe)."

```sh
aseqdump -p "Midi Through" | grep "controller 118"
```
- [ ] Record a loop (loop-derived tempo becomes authoritative) — confirm
      `CC118` actually appears on the bus with the right value, and that
      `demiurge-clock` retunes in response (watch its status line for
      `source=cc118`).
- [ ] Confirm CC118 stops being sent once external (OpGorator) becomes
      authoritative again (no feedback loop — see "OUTBOUND CLOCK" header
      block in `chase_bliss_clock.orc`).

## 8. Long-run stability soak — **[UNKNOWN]**

- [ ] 30-minute USB audio soak (per design spec §6): count drops/xruns via
      `pw-top` or journal, watch for slow drift in recovered BPM (the
      long-simulated-window test in `test_pll.cpp` covers the PLL math over
      3 simulated minutes and shows no drift — a real 30-minute session is
      10x that duration on real hardware, which no simulation ran).
- [ ] Leave the OpGorator plugged in and playing through several role
      flips (a few `demiurge-sync opgorator`/`demiurge-sync demiurge` cycles
      by hand) over that window — watch for any wedge, stuck role, or
      creeping latency.

## 9. Lower priority / optional this session

- [ ] PO-sync 3.5mm jacks (SY0-SY5) — firmware-only, out of scope for the
      Demiurge/NEPTR integration per the design spec; only relevant if
      testing the OpGorator as a bridge to real POs/Volcas.
- [ ] Mac TUI / web UI / NEPTR pygame UI sync pages, if built — all shell
      out to `demiurge-sync`, so item 3 above already covers the underlying
      correctness; these are presentation-layer checks.

---

## Quick reference — what to run BEFORE plugging in anything, to confirm
## the baseline is green

```sh
cd DEMIURGE_OS/testing/clock && ./run.sh          # 172/172 expected
cd DEMIURGE_OS/src/demiurge-launcher-rs && cargo test   # 26/26 expected
cd DEMIURGE_OS/testing/neptr-clock && ./run.sh    # 7/7 + consistency check expected
```

If any of these are red before hardware even arrives, fix that first —
everything above assumes the pure-logic baseline is solid so any failure
with real hardware attached points at a hardware/integration problem, not
a logic bug this session could have caught for free.
