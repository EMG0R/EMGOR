# NEW_PI_BOOTSTRAP.md — start here for a blank Pi 5

DEMIURGE_OS is the system/OS layer for a Raspberry Pi 5 live-audio rig: it
replaces the stock audio stack with PipeWire, builds a virtual audio sink
and a shared virtual MIDI bus that every program connects to, and runs a
Rust launcher (`demiurge.service`) that starts and supervises whatever
audio-language program the user's config points at. NEPTR is a separate,
sibling Csound application (its own git repo) that runs *on top of*
DEMIURGE_OS — DEMIURGE_OS doesn't know or care about NEPTR's internals, it
just launches it via `src/wrappers/demiurge-run-csound` like any other
program. This file is the entry point for standing up a brand-new Pi 5
against this repo, with no other context required.

All paths below are relative to this repo's root (DEMIURGE_OS), unless
written as an absolute Pi-side path (e.g. `/opt/demiurge/...`).

## 1. Standard install

Copy this repo onto the Pi (git clone or rsync), then run the installer
over SSH from the Pi itself:

```bash
ssh <your-pi-user>@<your-pi-host>     # e.g. ssh gorpi7v2@demiurge.local
cd ~/_______DEMIURGE            # or wherever this repo landed on the Pi
./setup-script/setup-demiurge.sh
```

The installer gets the Pi username from `PI_USER` (one variable at the top
of `setup-demiurge.sh`, defaulting to whoever is logged in and running it —
see that file's header). Logging in as a renamed user (e.g. `gorpi7v2`) and
running the script as normal is all a rename needs; nothing else in this
repo hardcodes the old name into anything the installer touches. Mac-side
scripts that reach the Pi over SSH (`neptr/scripts/sync_to_pi.sh`,
`pi-monitor/pi-monitor.sh`, `setup-script/deploy-nam.sh`, etc.) read their
default from `pi-rig.conf` at the repo root — edit that one file once to
point them at the new rig.

Invocation patterns (from the script's own header comment,
`setup-script/setup-demiurge.sh` lines 10-12):

```bash
./setup-demiurge.sh          # full run, all phases in order, from Phase 1
./setup-demiurge.sh 4        # resume from Phase 4 (skip 1-3)
./setup-demiurge.sh 8        # boot config + reboot only
```

The script is idempotent — every phase checks state before acting, so
re-running it (or resuming from a specific phase) is safe.

### What a full run installs automatically (Phases 1-10)

1. **Foundation** — apt base (build tools, alsa-utils, sox, ffmpeg, liblo,
   curl, teensy-loader-cli, picotool, nodejs), `audio` group membership,
   Rust toolchain via rustup.
2. **PipeWire** — replaces PulseAudio; pipewire/pipewire-alsa/pipewire-jack/
   wireplumber/pipewire-pulse, enabled as user services.
3. **Virtual audio layer** — `demiurge-sink`, the one virtual sink every
   program connects to.
4. **Virtual MIDI layer** — `Midi Through` (ALSA seq `14:0`), the shared
   MIDI bus every program reads and writes.
5. **Audio languages** — csound, puredata, supercollider, chuck, faust,
   strudel (node), a Python venv (mido, python-rtmidi, python-osc, ctcsound,
   numpy).
6. **Launcher + staging** — builds and installs `demiurge-clock` and the
   Rust `demiurge-launcher`, installs every `src/wrappers/demiurge-run-*`
   to `/opt/demiurge/bin/`, rsyncs `demiurge/` to `~/demiurge/` (never
   overwrites an existing `~/demiurge/live.conf`), installs
   `demiurge.service`.
7. **M8 performance pack** — RT limits, PipeWire quantum 128 @ 48 kHz,
   `performance` CPU governor, thermal watchdog, IRQ isolation, and the
   audio IRQ-thread RT boost (`demiurge-audio-irq-rt`), which derives the
   interrupt that paces *this* interface at runtime rather than assuming a
   bus. Also installs `demiurge-audio-bounce.sh` (script only, no boot unit)
   for the launcher's `ensure_usb_rates_stable()`. On an existing Pi this
   phase also disables and removes the retired `demiurge-usb-irq-rt` and
   `demiurge-scarlett-{bounce,rebounce}` units.
8. **Boot config (overclock + kernel cmdline)** — interactive/DANGEROUS:
   prompts `[yes/no]` before editing `/boot/firmware/config.txt` and
   `/boot/firmware/cmdline.txt` (2.8 GHz overclock, `usb_max_current_enable=1`,
   `isolcpus=3` — `nohz_full`/`rcu_nocbs` are appended but rejected by the
   stock kernel, etc.), backs up both files first,
   then reboots. Because it reboots, run it as its own step
   (`./setup-demiurge.sh 8`) rather than expecting it to fall through from
   an earlier phase in the same session.
9. **Web UI** — installs `web/demiurge_web.py` + `web/static/` to
   `/opt/demiurge/web/` and `demiurge-web.service`; serves
   `http://demiurge.local:8080`. Since Phase 8 ends in a reboot, run this
   separately on a fresh install: `./setup-demiurge.sh 9`.
10. **RNBO** — adds Cycling '74's apt repo (informational candidate check
    only, nothing installed from it), installs the from-source build
    toolchain (apt packages + pinned conan 1.66.0), then clones and builds
    `rnbo.oscquery.runner` **from source** (pinned tag `v1.4.5-9`,
    on-device compile enabled — the prebuilt apt package ships that
    disabled) via `setup-script/rnbo-build-runner.sh`, installing the
    result to `/opt/rnbo/bin/rnbooscquery` plus the gate script, auto-reload
    watcher, and `rnbo-runner.service`. This does a real compile (of the
    runner and, on a cold conan cache, its dependencies too) — budget at
    least 20-40 minutes on a Pi 5 the first time; re-runs are much faster.
    Runs automatically as part of a full install (`./setup-demiurge.sh`
    with no argument, or any start phase `<= 10`).

See `setup-script/README.md` for the full phase table (all 10 phases) and
`INSTRUCTIONS.md` (repo root) for the concept-first walkthrough of *why*
each phase exists.

## 2. RNBO — now part of the standard install (Phase 10)

RNBO (Cycling '74) support used to be a separate opt-in chain of four
standalone scripts; it is now folded into the standard installer as Phase
10 (see above) and runs automatically with everything else. If you only
want to (re)run the RNBO phase — e.g. after a runner version bump, or to
retry a build that failed partway — resume from it directly:

```bash
./setup-script/setup-demiurge.sh 10
```

The installed `rnbo-runner.service` is **fail-closed**: it's `enable`d at
boot, but `config/rnbo/demiurge-rnbo-gate.sh` (an `ExecCondition`) keeps it
from actually starting unless `~/demiurge/live.conf` has `rnbo = on` or a
`*.rnbo` file is present in the chain. Full rationale, verification steps,
and a from-scratch reapplication walkthrough: `demiurge/docs/rnbo.md`.

## 3. Known-good baseline (as of this fix)

The following has been verified and/or committed as of this pass over the
repo:

- **PSU verified good.** Official 5V/5A USB-C PSU + `usb_max_current_enable=1`
  confirmed; EXT5V measured 4.95 V under load; full 2.8 GHz overclock
  stable; zero undervolt events across 7 days of kernel logs. Record:
  `incidents/2026-07-13-psu-verification-resolved.md` (closes the PSU
  follow-up opened in `incidents/2026-05-14-bypass-mode-collapses-sync-latency.md`).
- **Rebounce timer RETIRED (2026-08-10).** The old
  `demiurge-scarlett-{bounce,rebounce}` units were gated on one USB vendor
  ID and did nothing for any other interface; the rebounce also failed at
  every boot when WirePlumber was slow, costing 15s and leaving a permanent
  `systemctl --failed` entry. Recovery is now condition-driven inside the
  launcher's `ensure_usb_rates_stable()`, which calls the vendor-neutral
  `demiurge-audio-bounce.sh` only when a USB audio node exists. Phase 7
  disables and removes the old units on upgrade. Background incidents:
  `incidents/2026-05-13-latency-and-cpu-split.md`, `demiurge/docs/gotchas.md` Q0e.
- **Outbound MIDI clock wrapper fix committed.** `src/wrappers/demiurge-run-csound`
  now opens `Midi Through` (`14:0`) for both input (`-M 14:0`) and output
  (`-Q 14:0`) on both exec paths, so a Csound app (e.g. NEPTR's
  `chase_bliss_clock.orc`) can steer the demiurge clock daemon over ch16
  CC118. The `csound_extra`/`--omacro:PRESET_BANK` fold-back that had
  drifted between a live-Pi edit and this repo was restored in the same
  commit. See `git log --oneline -- src/wrappers/demiurge-run-csound`
  (commit message starts "fix(wrapper): outbound MIDI clock").
- **Web UI live on port 8080** — Phase 9 above, `http://demiurge.local:8080`.
- **Safe single-wrapper deploy path exists.** `setup-script/deploy-wrapper.sh
  <wrapper-filename> [user@host]` diffs the local repo copy of a
  `src/wrappers/*` file against what's actually deployed on the Pi *before*
  overwriting anything, stages via `/tmp`, installs with
  `sudo install -m0755`. This is the fix for the root cause that let
  `demiurge-run-csound` drift out of sync in the first place — use it
  instead of hand-copying wrapper files to a live Pi.

## 4. If something's wrong

- `demiurge/docs/` — `quickstart.md`, `gotchas.md`, `config-reference.md`,
  `companion.md`, `clock.md`, `osc.md`, `nam.md`, `rnbo.md`,
  `microcontroller-flashing.md`, `serial-streaming.md`,
  `language-setup.md`.
- `INSTRUCTIONS.md` (repo root) §12 — failure modes F1–F29, the
  diagnostic tree for libjack collisions, DSP-off Pd silence, `--sched` /
  Csound scheduling crashes, the Faust hardware auto-link problem, and how
  to verify audio with `pw-jack jack_rec` (never `pw-record --target=`,
  which silently captures the mic instead of the chain).
- `incidents/` — dated incident write-ups with symptom, hypothesis,
  verification, and follow-ups. Read the most recent ones first; several
  cross-reference each other (e.g. the PSU and rebounce-timer files above
  both point back to `incidents/2026-05-13-latency-and-cpu-split.md` and
  `incidents/2026-05-14-bypass-mode-collapses-sync-latency.md`).

## 5. What this file does NOT cover

This file does not touch, and was not used to touch, the live Pi. Nothing
in this repo pass involved SSHing to or otherwise contacting a running
Pi — there's an unresolved SSH host-key question on the live rig that a
human needs to clear first. Everything above is a description of what the
scripts in this repo *will* do when a human runs them.
