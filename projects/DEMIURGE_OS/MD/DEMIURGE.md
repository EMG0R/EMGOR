# DEMIURGE — system orientation for agents

You are probably reading this because you are an agent working on this rig,
possibly **running on the Pi itself** (see `demiurge-agent`). Read this first.

**This file is a MAP, not a manual.** Detail lives in `demiurge/docs/` (19
files). Do not copy detail in here — a second source of truth drifts, and this
project has been bitten by exactly that (see "Invariants" below).

---

## What DEMIURGE is

An audio-first operating layer for a Raspberry Pi 5 — "a modern Satellite
CCRMA". It is not a distro; it is a staged install on top of Pi OS Lite (64-bit,
Debian 13 Trixie) that turns the Pi into a live instrument.

It is the brain of **NEPTR Phase 4**, the user's main performing instrument:
a Csound guitar/bass multi-FX engine (19+ menus, ~60 effects, 5-channel looper,
NAM neural amp models, IR reverb), a hand-drawn pygame touch UI, a Teensy 4.1
body with encoders/joystick/sensors, and a video synth. It also clocks **PMOR**,
a Daisy-based pocket groovebox, over USB MIDI.

**It has to work tonight.** This is performance equipment, not a lab. Prefer the
boring, reversible fix.

---

## The map

| Area | Where | Notes |
|---|---|---|
| Installer | `setup-script/setup-demiurge.sh` | 11 phases, idempotent, resumable: `./setup-demiurge.sh <phase>` |
| Bootstrap guide | `setup-script/NEW_PI_BOOTSTRAP.md` | current for phase content; NOT a full rebuild guide |
| Runtime config | `~/demiurge/live.conf` | **the single user-facing config surface** |
| Launcher (Rust) | `src/demiurge-launcher-rs/` | supervises the chain, owns graph routing + aggregation |
| Clock daemon | `src/demiurge-clock.cpp` | ALSA-seq MIDI clock, Ableton Link, follower PLL |
| Embedded host | `src/demiurge-csound-host.cpp` | csound inside the JACK callback (no `-B` ring) |
| Per-language wrappers | `src/wrappers/demiurge-run-*` | csound/Pd/SC/ChucK/Faust/RNBO/Strudel/NAM |
| Power knob | `src/wrappers/demiurge-power` (+ `demlow`/`demmed`/`demhigh`), `config/pi5-performance/demiurge-power.{sh,service}` | live.conf `power =` low/medium/high; one root oneshot applies it at boot and on change. Replaced demiurge-cpu-governor and power-restore |
| Tools | `src/wrappers/demiurge-*` | interface, graph, meter, ringtest, sync, wifi, audio… |
| PipeWire / WirePlumber | `config/pipewire/`, `config/wireplumber/` | installed by glob — see Invariants |
| NEPTR engine + UI | `neptr/` | `csound/` (107 .orc/.csd), `neptrPhase4_UI.py`, `shell/` (3D prints) |
| Video synth | `../NEPTR_VIDEO/` | separate repo; `deploy/` has the mode scripts (HOME screen) |
| Incidents | `incidents/` | **read these before debugging anything audio** |
| Open handoffs | `MD/HANDOFF-*.md` | **in-flight work. Read before touching git remotes or the games folder** — `HANDOFF-2026-10-05-git-unification.md` documents a live hazard: pulling on the Pi deletes 24.3 MB of ROMs unless you back them up first. |
| Agent docs / skills | `MD/` | this folder |

---

## Invariants — breaking these has cost real sessions

1. **Never restart `demiurge.service`, csound, or `neptr-ui` without asking,
   each time.** Never leave the engine down. Recovering an already-dead engine
   is fine and expected.
2. **No audio tests without explicit permission each time.** Playing a tone
   through his monitors unannounced is the thing this guards against.
3. **`live.conf` is the only config surface.** Do not invent a second one.
   Do not overwrite it — the installer deliberately never does.
4. **One mechanism per job.** This is the project's strongest rule. Two
   mechanisms for one job has caused: a dead `ctrl7` path alongside a working
   `midiin` path (clock CCs read 0 for months), a duplicated loop→BPM search
   cross-commented "must stay identical", and a bidirectional MIDI fix that
   existed in a udev one-shot but not in the reconciling service that replaced
   it. When you find two, consolidate — do not add a third.
5. **Install by glob, not by name.** `src/wrappers/*` and
   `config/wireplumber/*.conf` install wholesale. Named lists silently skip
   whatever nobody remembered: `demiurge-quantum-limits`,
   `demiurge-interface`, `demiurge-ringtest` and the Bela WirePlumber rule were
   all in the repo and all absent from a rebuilt rig.
6. **`$HOME`, never `/home/<user>`.** Hardcoded paths caused three separate
   production failures in one day (csound hunting impulse responses in another
   user's home; the UI crash-looping 157×; a deploy rsyncing to a nonexistent
   path). `pi-rig.conf` is the single Mac-side source for user@host.
7. **`loginctl enable-linger`** must be set. Without it the user systemd manager
   — and therefore PipeWire and WirePlumber — only exists while someone is
   logged in. PipeWire comes up on SSH login and dies on logout, taking the
   interface's node with it. The rig cannot run unattended without this.
8. **`usb_max_current_enable=1` stays.** The user owns `power=` / config.txt
   throttling; do not touch it.
9. **Never touch the EEPROM / `rpi-eeprom-update`.** Firmware is the user's call.
10. **Quantum and ksmps move in lockstep.** `live.conf quantum` and
    `globals.orc KSMPS` must match, via `csound_extra = --omacro:KSMPS=<n>`.
    Mismatched, csound rounds `-b` up and pipewire-jack buffers the difference.

---

## Verifying audio (do this before believing anything)

```sh
aplay -l | grep -i hifiberry      # card present at the ALSA level
wpctl status | grep pro-output    # node present in PipeWire
pw-link -l | grep -A1 csound6     # what csound is ACTUALLY wired to
demiurge-interface                # which interface DEMIURGE will use
systemctl is-active demiurge
journalctl -u demiurge -f         # routing decisions, respawns, suspends
```

Use `pw-jack jack_rec` to capture, **not** `pw-record`/`pw-cat` — those silently
mis-target. Note `jack_rec` writes **16-bit** regardless of flags, so it cannot
prove anything about bit depth or quantization on a quiet signal. That mistake
was made and over-claimed on 2026-10-02.

`demiurge-ringtest` captures input and output at the same instant and tells you
which side an artifact is on. Reach for it before theorising.

**Zero xruns does not mean the audio is fine.** The 128↔256 loopback conversion
clicks with no xrun and no CPU load.

---

## Known-bad states and their tells

| Tell | Cause |
|---|---|
| `aggregate: multi-interface — 2 wired outputs` | a second card joined; direct mode off, loopback in path. The Bela's dead 44.1 kHz USB audio card does this — `config/wireplumber/99-demiurge-disable-bela-audio.conf` hides it. |
| `no interface and no snd-aloop fallback sink found` | PipeWire has no device. Usually linger, or configs installed after PipeWire started. |
| `pygame.error: kmsdrm not available` | pip's pygame bundles an SDL2 without kmsdrm. Use apt `python3-pygame` + `venv --system-site-packages`. |
| `quantum: NOT verified (demiurge-quantum-limits not installed)` | the launcher cannot verify the quantum and bounces the card in a loop. |
| `could not unbind driver` repeating in touch-poll | SPI is off. Needs `dtparam=spi=on`. |
| UI touch works but swipes do not | no `~/demiurge/touch-cal.conf`; thresholds are in screen pixels. |
| Audio sounds bitcrushed and NOTHING downstream changes it | the ADC is hard-clipping. Check `demiurge-ringtest`: `INPUT peak -0.0 dBFS` is the tell. There are **two** input gain stages (analogue `PGA Gain L/R` and digital `ADC Capture Volume`), each -12..+40 dB; stacked at +20/+20 they clip a guitar at the converter. `demiurge-mixer gain <dB>` owns both. |
| `demiurge-agent: command not found` over SSH | `/opt/demiurge/bin` is not in sshd's default PATH. User-facing tools (typed over SSH) are symlinked into `/usr/local/bin` — add new ones there too. |
| every agent backend reports NOT INSTALLED but they are present | `~/.npm-global/bin` missing from a non-interactive PATH. `demiurge-agent` now sets it itself. |
| `Missing optional dependency @openai/codex-linux-arm64` | the network dropped mid-install and npm skipped the platform binary. Fix: `npm install -g @openai/codex@latest`. Not a node/npm version problem. |

---

## Running agents on the Pi

`demiurge-agent` (installed by Phase 11) runs **claude**, **gemini** or **codex**
on the Pi, in the repo, non-interactively:

```sh
demiurge-agent --list                      # which backends are installed
demiurge-agent "why is csound crash-looping?"
demiurge-agent --gemini "..."
demiurge-agent --bg "long investigation"   # detached in tmux, survives SSH drop
demiurge-agent --attach                    # watch a --bg run
```

From the **Mac**, `setup-script/pi-agent.sh` is the same thing without the SSH
plumbing — it learns the rig from `pi-rig.conf` like every other Mac-side script:

```sh
setup-script/pi-agent.sh --auth            # walks the one-time logins (interactive)
setup-script/pi-agent.sh --list
setup-script/pi-agent.sh "why is csound crash-looping?"
setup-script/pi-agent.sh --bg "long investigation"
setup-script/pi-agent.sh --pull            # bring the Pi agent's commits to the Mac
setup-script/pi-agent.sh --push            # send the Mac's commits to the Pi
```

**Installed on the rig (verified 2026-10-03):** claude 2.1.288, gemini 0.62.0,
codex-cli 0.160.0, gh 2.102.0, tmux, git.

**Auth is interactive, one-time per backend, and no credentials live in this
repo.** Nothing is logged in by the installer. `gh auth login` is what lets an
agent on the Pi push its own work.

**Moving work between the two repos.** The Mac drives both directions, because
the Mac can reach the Pi and the Pi cannot reach the Mac. The Pi repo has no
network remote; `pi-agent.sh --push/--pull` uses an SSH remote named `pi`.
Phase 11 sets `receive.denyCurrentBranch=updateInstead` on the Pi, so a push
updates its **working tree** too and an agent there sees the new files at once.
It refuses rather than clobbers when the Pi's tree is dirty — commit or discard
there first. Do not go back to passing `git bundle` through `/tmp`.

**PATH, twice bitten.** `/opt/demiurge/bin` is not in sshd's default PATH and
`~/.npm-global/bin` is only in `.bashrc`, which non-interactive shells never
source. So `ssh pi <tool>` fails with "command not found", and a tool that
checks `command -v claude` reports every backend missing while all three are
installed. User-facing tools are therefore symlinked into `/usr/local/bin`, and
`demiurge-agent` puts the npm prefix on its own PATH. Follow both when adding a
tool meant to be typed over SSH.

**Shared memory.** The rig clones `EMGOR_SKILLS` (identity, the two-tier memory
index, skills) to `~/EMGOR_SKILLS` from the GitHub mirror over HTTPS, using the
`gh` credential helper — no SSH key on the rig. `~/.claude/CLAUDE.md` `@`-includes
`_EMORY.md` and `memory/MEMORY.md`, so an agent here starts knowing the owner and
the hard rules (including *never restart the audio*). Hooks sync on session
start/stop, and `emory-core-sync.timer` runs a full sync 90s after boot and daily
at 03:10, so the Pi publishes what it learned even if nobody opens a session.

Conflicts never wedge it and never lose a fact: `MEMORY.md` indexes union-merge
(`.gitattributes`), any other conflicted file keeps OURS in place and THEIRS
beside it as `<name>.conflict-<host>-<ts>.md`, logged to `memory/_CONFLICTS.md`.
Run `bash ~/EMGOR_SKILLS/scripts/sync.sh --doctor` when memory looks wrong: it
reports staleness, every remote, the timer, conflicts, a held lock and any
deleted-but-unpushed memories. Publishing a mass deletion is refused outright.

**Why this exists:** an agent on the Mac cannot read `aplay -l`, cannot watch
`journalctl` while a fault happens, and cannot run `demiurge-ringtest` while an
artifact is audible. The machine with the problem should do the work.

---

## USER MODIFICATIONS

> This section belongs to Emory. Agents: **read it, do not rewrite it.** Append
> below the line only if asked to record something here. If a change here
> conflicts with anything above, this section wins — it is the owner's intent,
> and the rest of this file is description.

<!-- USER MODIFICATIONS BELOW — agents do not edit above this line -->

- _(nothing recorded yet)_

