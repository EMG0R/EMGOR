# Handoff — NAM fix, RNBO gate, Pi power (session 2026-06-14/15)

Context for the next agent. Three intertwined threads came out of one task ("NAM
mutes audio when toggled on; debug + fix + deploy"). Read the **Boundaries** and
**Pi environment** sections before touching the Pi — there's a hardware power
problem and a user-owned throttle system you must not stomp.

## Status at a glance

| Thread | State | Next action |
|---|---|---|
| **NAM mute fix** | ✅ built + deployed + binary-verified on Pi | live-verify (test-nam.sh + audio) on proper power |
| **NAM deep audit** | ✅ done — no other defect found | none |
| **RNBO runner gate** | 🟡 written + unit-tested in source, **NOT deployed** | deploy to Pi (see steps) |
| **Pi power / throttle** | 🔴 user-owned system **NOT deployed to Pi**; brownouts ongoing | user deploys their throttle; get real PSU |
| **NAM test tooling** | ✅ created in DEMIURGE_OS/testing/nam, **not run** | run on Pi when powered |

---

## Thread 1 — NAM mute fix  ✅ DONE (not live-tested)

**Root cause (confirmed by binary inspection, not docs):** the `.so` running on
the Pi was the stale `thread=5`/`nam_aperf` build. CSound 6.18's LINKAGE never
schedules `thread=5` (aperf), so NAM's output buffers stayed zero → audio muted
when toggled on. The fixed source (`thread=3`/`nam_kperf`) existed on the laptop
but **had never been compiled+installed on the Pi**. That gap was the entire bug.

**Fix:** rebuilt on Pi and installed. Verified in the deployed binary:
`nm -C /usr/local/lib/csound/plugins64/csound-nam.so` → `nam_kperf` present,
`nam_aperf` absent.

**Deep static audit (all clean) — checked against the actually-deployed files:**
- signal_chain.orc: separate `aNAM_in/out`, `kNAMBlend` blend ✔
- globals.orc: `gkNAMProfileIdx init -1`, wet 1.0, bypass 1 ✔
- effects/nam.orc: NAMProcess UDO commented (no 6.18 shadow) ✔
- profile dir `~/demiurge/profiles/nam` + 5 `.nam` files ✔
- nam_loader.{cpp,h}: `current_idx_{-1}` → loads profile 0 at init (no race) ✔
- all 5 profiles are `SlimmableContainer` v0.7.0 → `get_dsp` returns
  `ContainerModel : public DSP, public SlimmableModel`, so the
  `dynamic_cast<nam::SlimmableModel*>` cross-cast SUCCEEDS → `SetSlimmableSize(1.0)`
  fires → full-quality submodel (not lo-fi) ✔

**Open:** NOT live-tested (user instructed no testing this session). To verify when
on proper power: `bash DEMIURGE_OS/testing/nam/test-nam.sh`, then `redem` and
toggle NAM on menu 8 — expect amp-processed audio, not mute or dry.

**If you ever rebuild:** use `-j1` on laptop USB-C power. `build-on-pi.sh` uses
`-j$(nproc)` (=4) which browns out the weak supply. Manual clean build:
```
cd ~/NEPTR_phase4/csound/nam
setsid bash -c "rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j1 && sudo cmake --install build && echo NAM_BUILD_DONE || echo NAM_BUILD_FAIL" > ~/nam_build.log 2>&1 < /dev/null &
```
(setsid so it survives SSH drops; passwordless sudo is configured so install runs unattended.)

---

## Thread 2 — RNBO runner gate  🟡 STAGED, NOT DEPLOYED

**Finding:** `rnbo-runner.service` (rnbooscquery, on-device patch compiler) was
`enable --now` at boot — always-on, burning CPU/power even with **no** RNBO patch
loaded. User wants it to run only when RNBO is actually in use.

**Implemented in `DEMIURGE_OS` source (parser logic unit-tested locally, all cases pass):**
- `demiurge/live.conf` — new `rnbo = on | off` section (default off), above `chain =`
- `config/rnbo/demiurge-rnbo-gate.sh` — ExecCondition helper (same live.conf
  resolver as demiurge-gate/demiurge-power). Runs the runner iff `rnbo = on` OR a
  `*.rnbo` token is in `chain` (comments stripped; fail-closed)
- `config/rnbo/rnbo-runner.service` — added `ExecCondition=/usr/local/bin/demiurge-rnbo-gate.sh`
- `setup-script/deploy-rnbo-wave1.sh` — installs the gate helper
- `demiurge/examples/rnbo_neptr/live.conf` — "always-on" comment corrected

**To deploy (light op; was about to run when session ended):**
1. `sudo install -m0755 config/rnbo/demiurge-rnbo-gate.sh /usr/local/bin/`
2. `sudo install -m0644 config/rnbo/rnbo-runner.service /etc/systemd/system/`
3. **Surgically** ensure `rnbo = off` in the Pi's `~/demiurge/live.conf` — append
   the rnbo section only if no `rnbo =` line exists. DO NOT overwrite the file
   (shared with throttle work — see Boundaries).
4. `sudo systemctl daemon-reload && sudo systemctl restart rnbo-runner`
5. Verify clean condition-skip: `systemctl status rnbo-runner` should show
   "condition failed"/ skipped (NOT failed); `pgrep rnbooscquery` empty.

---

## Thread 3 — Pi power / throttle  🔴 OPEN (user-owned)

**Hardware reality:** the Pi 5 is powered from a **MacBook USB-C port (~15W)**, not
its required 5V/5A (25W) PSU. This causes repeated brownouts: active under-voltage
(`vcgencmd get_throttled` = `0x50005`), CPU throttling, and full PMIC under-voltage
shutdowns/reboots. RAM is fine (16GB, not OOM). The compile current spike (esp.
`-j4` with 4 cores ramping = di/dt) is what tips it over.

**CRITICAL discovery:** the user's throttle system is **NOT deployed to the Pi**.
`demlow`/`demmed`/`demhigh` not found, no `demiurge-power` unit, `live.conf power=`
empty on the Pi. It all lives only in `DEMIURGE_OS` source. THIS is why the Pi
keeps booting to `performance`/2.8GHz/4-core and browning out. Deploying it (a
user-owned action) is the durable fix.

**What got the build through:** Claude's *ephemeral* `/sys` cap (powersave + flat
1.5GHz `scaling_max_freq` = no di/dt spike + single core). These reset on reboot.

**Real fix:** use the proper 5V/5A PSU. Until then, deploy the user's `power=low`
profile (flat lowest OPP + 2 cores) + the `config.txt.laptop.snippet` boot layer
(over_voltage off, arm_freq down).

---

## Boundaries (do not violate)

- **Do NOT touch the user's throttle artifacts:** `live.conf` `power=`/`quantum=`
  lines, `config/pi5-performance/demiurge-power.{sh,service}`, `config.txt*`,
  `setup-demiurge.sh`, the cpu-governor unit. User owns system throttle.
- Claude's only legit power lever is build `-j` count + ephemeral `/sys` writes.
- **`live.conf` is edited by two efforts** (rnbo gate + throttle). Make additive,
  well-delimited edits; never overwrite the whole file.
- `redem`/`stodem` (Mac zsh aliases) edit `audio=` in live.conf + gate. `stodem`
  leaves demiurge in `failed`/stopped (expected).

---

## Pi environment / gotchas

- Host `gorpi7@demiurge.local`. **Passwordless sudo configured** (`/etc/sudoers.d/010-gorpi7-nopasswd`,
  synced). If a brownout eats it before fsync, recreate with `sudo -S` then `sync`.
- SSH drops during brownouts. Run long ops detached (`setsid ... </dev/null &`) and
  poll a logfile; never kill+relaunch over a laggy link (that corrupts builds — it
  bit us repeatedly).
- Pi 5, 16GB RAM, 2GB swap. `c++` killed = brownout/PMIC, not OOM.
- Source-of-truth note: NEPTR lives in top-level `NEPTR phase4`. DEMIURGE_OS has a
  separate standalone `nam/` track — keep nam_loader.cpp/csound_nam_opcode.cpp in
  sync (same thread=3/kperf fix applies).

## Leftover cleanup (harmless, optional)

A cleanup pass didn't finish (Pi dropped). On the Pi: `rm -rf ~/NEPTR_phase4/csound/nam/build ~/nam_build.log`
(self-cleans on next build anyway).

## Open issues — prioritized

1. **Get a 5V/5A PSU** — eliminates the brownout root cause; everything else is workaround.
2. **Live-verify NAM** — `test-nam.sh` + audio toggle, on proper power.
3. **Deploy RNBO gate** (Thread 2 steps).
4. **Deploy user's throttle system** to the Pi (user-owned; coordinate, don't unilaterally).
5. **Reconcile `live.conf`** between rnbo + throttle edits when both land.

## File inventory (this session)

Created (DEMIURGE_OS): `config/rnbo/demiurge-rnbo-gate.sh`, `testing/nam/{test-nam.sh,nam_signal_test.csd,README.md}`
Edited (DEMIURGE_OS): `config/rnbo/rnbo-runner.service`, `demiurge/live.conf` (+rnbo section), `setup-script/deploy-rnbo-wave1.sh`, `demiurge/examples/rnbo_neptr/live.conf`
Pi: rebuilt+installed `csound-nam.so`; `/etc/sudoers.d/010-gorpi7-nopasswd`
Source already-fixed (synced to Pi earlier): `csound/nam/src/csound_nam_opcode.cpp` (kperf), signal_chain.orc, globals.orc, effects/nam.orc
