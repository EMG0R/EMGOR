# DEMIURGE: system overview

This is the parent document for every DEMIURGE node: what the system is, where
things live, and the rules that keep it working. A human or an agent working on
a node reads this first, then the node's own `device.md` (see "Node identity").
Where the two disagree, the device layer wins.

This file is a map, not a manual. Detail belongs in the file that implements the
thing. Do not copy detail in here; a second source of truth drifts.

---

## What DEMIURGE is

An audio-first operating layer for a Raspberry Pi 5. It is not a separate
distribution: it is a staged install on top of Raspberry Pi OS Lite (64-bit,
Debian 13 Trixie) that configures the Pi as a low-latency audio host.

- **PipeWire owns audio and MIDI.** There is no `jackd`. Programs that speak
  JACK are served by `pipewire-jack`; PulseAudio clients by `pipewire-pulse`.
  A virtual sink and source (`demiurge-sink`, `demiurge-source`) sit above the
  hardware so devices can be hot-plugged without reconfiguring programs.
- **Several audio languages are installed and wired the same way:** Csound, Pure
  Data, SuperCollider, ChucK, Faust, RNBO, Strudel, C++ (JACK) and Python. Each
  has a `demiurge-run-<language>` wrapper that sets the shared environment.
- **One config file.** `~/demiurge/live.conf` describes the session: tempo, rate,
  buffer size ("quantum"), whether the sync layer is on, and the `chain =` list of
  programs. It is the only user-facing config surface.
- **One MIDI clock** shared by every program on the box, with optional Ableton
  Link.
- **A performance pack** for the Pi 5: real-time limits, core isolation, IRQ
  priorities, a power knob (`power = low|medium|high`), a thermal watchdog and a
  network stability guard.
- **A self-updater** that stages new versions in the background and swaps them in
  only at boot.
- **A device layer** on top: each node carries a `device.md` that says which node
  it is and what it runs.

---

## The map

Paths are relative to the repository root. The installed layout is in
`INSTRUCTIONS.md`.

| Area | Where | Notes |
|---|---|---|
| Entry point | `bootstrap.sh`, `BOOTSTRAP.md` | fresh Pi OS Lite to a running node |
| Installer wrapper | `install.sh` | hostname, `device.md`, phase ordering across the reboot |
| Installer phases | `setup-script/setup-demiurge.sh` | idempotent and resumable: `./setup-demiurge.sh <phase>` |
| First boot from an image | `flash/firstrun.sh`, `FLASHING.md` | runs `bootstrap.sh` on first boot |
| Node identity seeding | `setup-script/node-repo/init-node-repo.sh` | creates `demiurge/local/device.md`, never overwrites it |
| Runtime config | `~/demiurge/live.conf` | the single user-facing config surface |
| Per-language wrappers | `src/wrappers/demiurge-run-*` | one per language |
| Tools | `src/wrappers/demiurge-*` | interface chooser, mixer, meter, graph, ringtest, power, wifi, agent, and others |
| Embedded Csound host | `src/demiurge-csound-host.cpp` | Csound computed inside the JACK callback |
| PipeWire / WirePlumber | `config/pipewire/`, `config/wireplumber/` | installed by glob |
| Performance pack | `config/pi5-performance/` | limits, IRQ and core pinning, power, thermal watchdog |
| Network guard | `config/network/` | link failover, NetworkManager defaults, persistent journal |
| RNBO | `config/rnbo/`, `setup-script/rnbo-*.sh` | opt-in: source build of the Cycling '74 OSCQuery runner |
| Updater | `setup-script/updater/` | stage, swap and revert; ships disabled |
| Docs | `docs/` | design notes for specific subsystems |

Components that are not part of this tree yet (the Rust launcher that supervises
the chain, the clock daemon and the web UI) are skipped by the installer when
absent, with a `skip:` line. The rest of the install completes without them.

---

## Invariants

These have each cost real debugging time when broken.

1. **Treat the audio engine as live equipment.** Do not restart
   `demiurge.service`, PipeWire, WirePlumber or any running audio program
   without the node's owner agreeing, each time. Never leave the engine down.
   Recovering an engine that is already dead is fine.
2. **No audio tests without permission.** Do not play signals through someone's
   speakers unannounced.
3. **`live.conf` is the only config surface.** Do not invent a second one. The
   installer never overwrites an existing `live.conf`; neither should anything
   else.
4. **One mechanism per job.** When you find two ways of doing the same thing,
   consolidate them rather than adding a third.
5. **Install by glob, not by name.** `src/wrappers/*` and
   `config/wireplumber/*.conf` install wholesale. A hand-maintained list silently
   skips whatever nobody remembered to add.
6. **`$HOME`, never a literal `/home/<user>`.** A hard-coded home directory breaks
   the moment the user account is named differently.
7. **`loginctl enable-linger` must be set.** Without it the user systemd manager,
   and therefore PipeWire and WirePlumber, exists only while someone is logged
   in. A headless node cannot run unattended without it.
8. **Boot firmware and `config.txt` are the owner's call.** Do not change
   `config.txt` throttling or power settings, and do not run
   `rpi-eeprom-update`, unless asked. Run `sync` after writing `config.txt`.
9. **Quantum and ksmps move together.** `live.conf` `quantum` and the Csound
   `ksmps` must match. Mismatched, Csound rounds the buffer up and the audio
   path buffers the difference.
10. **Guard, do not delete, in installers.** Steps that need an optional file are
    conditional on it existing and log a `skip:` line otherwise.

---

## Verifying audio

Check before believing anything:

```sh
aplay -l                          # is the card present at the ALSA level
wpctl status                      # is its node present in PipeWire
pw-link -l                        # what each program is actually wired to
demiurge-interface                # which interface DEMIURGE will use
systemctl is-active demiurge
journalctl -u demiurge -f         # routing decisions and respawns
```

Capture with `pw-jack jack_rec`, not `pw-record` or `pw-cat`, which can silently
record the wrong node. `jack_rec` writes 16-bit regardless of flags, so it cannot
prove anything about bit depth on a quiet signal.

`demiurge-ringtest` captures an interface's input and output at the same instant
and shows which side an artifact is on. Use it before theorising.

Zero xruns does not mean the audio is clean. A sample-rate or buffer-size
conversion inside the path can click with no xrun and no CPU load.

## Common symptoms

| Symptom | Likely cause |
|---|---|
| no interface and no loopback fallback sink found | PipeWire has no device; usually linger is off, or configs were installed after PipeWire started |
| `quantum: NOT verified (demiurge-quantum-limits not installed)` | the launcher cannot verify the quantum; the wrapper must be on the box |
| a second audio card makes the path go through a loopback | more than one wired output is present; hide the unused card with a WirePlumber rule in `config/wireplumber/` |
| agent backends report NOT INSTALLED but are present | the npm global prefix is missing from a non-interactive PATH; `demiurge-agent` sets it itself |
| a tool typed over SSH says `command not found` | `/opt/demiurge/bin` is not in sshd's default PATH; user-facing tools are symlinked into `/usr/local/bin` |

---

## Updating

`setup-script/updater/` holds three scripts and their systemd units. A timer
(every six hours, low priority) compares `DEMIURGE_REMOTE` and `DEMIURGE_BRANCH`
from `/demiurge/local/updater.conf` against the remote and shallow-clones anything
newer into `/demiurge/versions/<hash>`. A stage that fails its checks is discarded.
At the next boot, before `demiurge.service`, `/demiurge/current` is flipped to the
staged version. `demiurge-update-revert` flips it back and pauses updates.
Nothing audio is restarted at runtime. The updater ships disabled; see
`setup-script/updater/README.md`.

`/demiurge/local/` sits outside the versioned tree and is linked into whichever
version is current, so node-local files survive updates.

---

## Node identity

Two layers:

- **This file** is the parent: what DEMIURGE is, the same on every node. It ships
  with the repository.
- **`demiurge/local/device.md`** is the child: YAML front matter (hostname, role,
  hardware, optional pair, audio hat) plus prose that says what this particular
  node is and does. It is created by `setup-script/node-repo/init-node-repo.sh`,
  belongs to the node, is never overwritten, and is not part of the shared repo.

Read the parent, then the child. If you do not know which node you are on, read
the child before acting.

---

## Agents on the node

`demiurge-agent` (installed by Phase 11) runs `claude`, `gemini` or `codex` on the
Pi, in the repository, so the agent can read `aplay -l` and `journalctl` while a
fault is happening. Authentication is interactive and one-time per backend;
nothing is logged in by the installer and no credentials belong in the repository.

```sh
demiurge-agent --list
demiurge-agent "why is csound crash-looping?"
demiurge-agent --bg "long investigation"
demiurge-agent --attach
```

---

## Local modifications

This section belongs to the node's owner. Agents: read it, do not rewrite it.
Append below the marker only when asked. If it conflicts with anything above, this
section wins.

<!-- LOCAL MODIFICATIONS BELOW. Agents do not edit above this line. -->

- _(nothing recorded yet)_
