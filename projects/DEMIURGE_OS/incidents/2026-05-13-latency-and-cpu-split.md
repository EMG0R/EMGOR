# 2026-05-13 — Latency + multi-core split + rebounce recovery

## Symptoms

- "Weird samplerate sound" on effect activation, ~30% of cold boots.
- Manual unplug+replug of Scarlett interface caused total sample-rate glitchiness on this session.
- Dry latency tolerable, FX-active latency "slightly too much" — user wanted to push further.
- User intuition: maybe the sync layer is too costly for live audio? (Spoiler: no.)

## Diagnosis

### Latency budget at quantum=256 (the existing state)

| Stage | Latency | Type |
|---|---|---|
| mic capture (USB ADC) | 5.33 ms | wait |
| csound process (ksmps=256) | 5.33 ms | wait |
| demiurge-sink loopback hop | 5.33 ms | wait |
| alsa_output ring (period-num=4, headroom=0) | ~10.7 ms | wait |
| **Total RT** | **~26 ms** | |

Csound's actual CPU cost is ~30-50 µs per 5333 µs window — virtually all of the per-period budget is *waiting for the next quantum boundary*. The sync layer adds exactly one quantum hop on output (~5.33 ms). It is NOT inherently impractical; it is a fixed, predictable cost. Multi-core does not shorten waits — it only buys reliability headroom for lower quantum.

### Replug-glitch root cause

- `demiurge-scarlett-rebounce.timer` was `Loaded: loaded (disabled; preset: enabled)` — never enabled. The script that "RELIABLY clears stuck sample-rate state by bouncing AFTER demiurge is running" had no fire mechanism.
- No udev hook for user-initiated replugs, so manual yank had zero safety net beyond WP's per-node `audio.rate=48000` pin (which is correct but apparently insufficient on its own).
- The graph rate metadata held at 48000 post-replug (verified live), but the audible glitch was a transient resampling event during the renegotiation window that only a fresh unbind/bind cycle reliably clears.

### CPU layout (before)

- isolcpus=3 only
- demiurge.service CPUAffinity=3 ✓
- PipeWire RT (data-loop.0) floating on CPUs 0-2
- IRQ mask off CPU 3 ✓
- No pipewire affinity pin

This works at quantum=256. At quantum<=128 the PipeWire driver thread starts competing with system housekeeping IRQs for deadlines.

## Phase 1 changes (this session, pending reboot verification)

### Reliability — Scarlett replug recovery

- `/etc/systemd/system/demiurge-scarlett-rebounce.timer` — cleaned up malformed `Documentation=` lines that systemd was parsing as URLs.
- `sudo systemctl enable --now demiurge-scarlett-rebounce.timer` — done.
- `/etc/systemd/system/demiurge-scarlett-replug.service` — new oneshot unit, `ExecCondition` filters out uptime<60s so it doesn't race with the boot timer.
- `/etc/udev/rules.d/99-demiurge-scarlett-replug.rules` — `ACTION=="bind" SUBSYSTEM=="usb" ATTRS{idVendor}=="1235"` → fires the replug service via `SYSTEMD_WANTS`.

### Multi-core split (latency headroom)

- `/boot/firmware/cmdline.txt`: `isolcpus=3 nohz_full=3 rcu_nocbs=3` → `isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3`
- `/usr/local/bin/demiurge-irq-isolate.sh`: `MASK_HEX=7` → `MASK_HEX=3`; `MASK_LIST=0-2` → `MASK_LIST=0-1`
- `/etc/systemd/user/pipewire.service.d/cpu-affinity.conf` (new): `CPUAffinity=2`
- demiurge.service CPUAffinity=3 unchanged (csound stays on 3)

Layout after reboot:
- CPU 0,1: system, WirePlumber policy, networking, kernel housekeeping
- CPU 2: PipeWire RT driver + loopback threads (isolated, no general scheduling, no IRQs)
- CPU 3: csound, chuck, launcher, demiurge-clock (isolated, no general scheduling, no IRQs)

## Phase 2 (next session, pending Phase 1 confirmation)

Quantum 256 → 128. Cuts RT floor to ~13 ms. Six-place lockstep edit per [[project_pipewire_rate_pin]] and [[project_jack_bufsize_quantum_rule]]:

1. `/etc/pipewire/pipewire.conf.d/99-demiurge-lowlatency.conf` — quantum, force-quantum, max-quantum → 128
2. `/etc/systemd/system/demiurge.service.d/wait-pipewire.conf` — `pw-metadata clock.force-quantum 128`
3. `/etc/pipewire/pipewire.conf.d/demiurge-virtual.conf` — `node.latency = 128/48000` × 4
4. `/etc/wireplumber/wireplumber.conf.d/50-demiurge.conf` — `api.alsa.period-size = 128`
5. `src/demiurge-launcher-rs/src/util.rs` — `FORCE_QUANTUM = 128`; rebuild + install
6. `/opt/demiurge/lib/demiurge-env.sh` — `PIPEWIRE_LATENCY=128/48000`
7. `/opt/demiurge/bin/demiurge-run-chuck` — `--bufsize:128`
8. Csound `.csd` files with hardcoded `ksmps = 256` (currently neptrPhase4.csd) → 128. FFT effects at `giFFT=1024` become 8× overlap at ksmps=128 — fine, more CPU but well within Pi 5 budget.

Verify after: `pw-top` shows QUANT=128, RATE=48000, ERR=0 across all nodes. 60s xrun watch with effects active. If anything wrong → revert step 1 first (re-pin force-quantum=256) to un-stick the graph, then re-debug.

## Verification checklist for next boot

- [ ] `cat /sys/devices/system/cpu/isolated` → `2-3`
- [ ] `cat /proc/irq/default_smp_affinity` → `3` (binary 0011)
- [ ] `ps -L -o psr,comm $(pgrep -x csound)` → all CPU 3
- [ ] `ps -L -o psr,comm $(pgrep -x pipewire)` → data-loop.0 on CPU 2
- [ ] `systemctl is-enabled demiurge-scarlett-rebounce.timer` → `enabled`
- [ ] `journalctl -u demiurge-scarlett-rebounce.service --since boot` shows the rebounce script ran
- [ ] `pw-metadata -n settings 0 clock.force-rate` → 48000 (unchanged in Phase 1)
- [ ] `pw-metadata -n settings 0 clock.force-quantum` → 256 (unchanged in Phase 1)
- [ ] `pw-top` 30s sample → ERR=0 across all rows
- [ ] Listening test: dry, then activate spectraldelay or reverb, listen for crackle and the "weird sample rate" sound. Should be gone.
- [ ] Manual yank+replug Scarlett — should auto-recover within ~5s, no bit-crush.

---

## Phase 1 — verified after reboot ✓

- `isolated` = 2-3, IRQ default mask = 3, PipeWire data-loop.0 on CPU 2 (RTPRIO 88), csound on CPU 3 (RTPRIO 74-83), rebounce timer fired at boot, no orc/csd errors.
- xhci IRQs landed on CPU 0 (not 2/3) — IRQ isolation working as designed.
- ExecCondition uptime guard on `demiurge-scarlett-replug.service` had a shell-quoting bug (the `$1` inside double-quoted awk got eaten by bash). Replaced with `/usr/local/bin/demiurge-replug-uptime-guard.sh` (sh script reading /proc/uptime). Cosmetic safety net — boot-time rebounce already nailed the device's last-used rate to 48k, so user-replugs negotiate cleanly via the WP pin alone.
- **WirePlumber pinned to CPU 1**: discovered WP's gmain/gdbus/pool-spawner threads (SCHED_OTHER) were running on CPU 2 and evicting PipeWire RT's hot cache lines. Added `/etc/systemd/user/wireplumber.service.d/cpu-affinity.conf` with `CPUAffinity=1`. Eliminated the baseline crackle on the clean signal.

## Phase 2 — NEPTR preset + effects overhaul (same session)

**Preset row reorganization** (5 buttons, left→right): CLEAN · TS · PHASER · TREMOLO · AMBIENT

| Slot | CC | Button | Type | Preset index |
|---|---|---|---|---|
| 0 | CC72 | CLEAN | preset | 0 |
| 1 | CC71 | TS (was SLDIRTY) | preset | 1 |
| 2 | CC76 | PHASER (was DIRTY) | toggle | n/a (`/pha_state`) |
| 3 | CC75 | TREMOLO | toggle | n/a (`/trem_state`) |
| 4 | CC74 | AMBIENT | preset | 3 |

DIRTY preset (was index 2) removed entirely. Preset 2 is now unused. AMBIENT stays at preset 3 — the UI uses `SLOT_TO_PRESET = {0: 0, 1: 1, 4: 3}` to map slots to preset indices so slot 4 lights when active_preset=3.

**Files changed** (all under `neptr/`):

| File | Changes |
|---|---|
| `csound/effects/tubescreamer.orc` | Restored RMS volume compensation, with final `* 1.449` boost (1.20 × 1.15 × 1.05 cumulative) for boost-pedal feel above unity. |
| `csound/effects/delay.orc` | Added HP@180Hz + LP@4kHz filtering on `aMemL/R` feedback memory — each successive repeat darkens, classic RE-201/Echoplex spring-delay character. Dry path untouched. |
| `csound/midi_handler.orc` | DIRTY preset block removed. CLEAN + TS preset values updated (see below). CC76 menu 0 now toggles PHASER with classic-rock defaults. CC75 menu 0 restored to TREMOLO toggle (original behavior). All preset activations also clear `gkPHA_bypass=1` so phaser cleared on preset switch (mirrors tremolo). Vibrato encoder scaling capped at `* 0.005` (max 5ms) instead of `* 0.03` (max 30ms). |
| `neptrPhase4_UI.py` | `MENUS[0]` updated to the new 5-entry layout. Lighting logic special-cases `pha` and `trm` codes (read from `bypass_states["pha"]` / `bypass_states["trm"]`) and uses the `SLOT_TO_PRESET` map for preset slots. |

**Preset parameter values (final)**:

CLEAN (`gkCurrentPreset = 0`):
- vibrato: bypass=0, freq=0.483 Hz, **depth=0.001s** (was 0.004s — minimal added latency)
- delay: bypass=0, **gkDelayMS=53125** (53.125 ms ≈ 32nd at 120 BPM, 15% faster), **gkFeedback=0.62**, **gkDelMix=0.20** (80% dry / 20% wet)
- TS: bypass=1
- reverb: bypass=1

TS (`gkCurrentPreset = 1`, was SLDIRTY):
- vibrato: bypass=0, freq=0.483 Hz, **depth=0.001s**
- delay: bypass=0, **gkDelayMS=53125**, **gkFeedback=0.62**, **gkDelMix=0.20**
- TS: bypass=0, **gkTSAmtAcc=0.36** (was 0.15 in SLDIRTY)
- reverb: bypass=1

AMBIENT (`gkCurrentPreset = 3`):
- vibrato: bypass=0, freq=0.483 Hz, **depth=0.001s**
- delay: bypass=1 (ambient uses reverb instead)
- TS: bypass=1
- reverb: bypass=0, FB=0.92, mix=0.70
- vol pedal: bypass=0, level=1, CC29 active

PHASER toggle (on activate ON):
- rate=0.8 Hz, depth=0.7, freq=0.15, feedback=0.55, **stages=6**, mix=0.7, shape=8 (dramatic classic-rock voicing, more notches + resonance than basic Phase 90)

**Latency analysis recorded** (because user kept asking):
- Vibrato: inherent — `vdelayxw` mean delay = `gkVIB_depth` seconds. Was 4ms, now 1ms in presets. Cannot be zero (vibrato IS delay modulation).
- Delay (preset): dry path is direct → 0ms added on dry component. Wet path includes a `pitchshifter4` (FFT pitch shifter, ~10ms FFT latency) but at mix=0.20 the wet component is quiet, and that latency is just the echo offset which is the point of a delay.
- Phaser: all-pass chain, zero inherent latency.
- Tremolo: amplitude modulation only, zero inherent latency.
- TS: tanh saturation, zero inherent latency.

So the "weird sample rate latency feeling" the user attributed to effects was the previously-too-long delay (gkDelayMS=40320 → 40.3ms, weirdly close to a slap-back) combined with the high vibrato depth (4ms). With delay properly tuned (53ms tight 32nd-feedback echo) and vibrato shallow (1ms), effects path latency is essentially the dry path + ~1ms.

**Iteration trail for TS volume calibration** (final state is 1.449× boost above matched-loudness):
1. Started: original `kComp = kInRMS × (1.0 + 0.45 × gkTSAmtAcc) / kOutRMS`
2. User wanted boost-pedal feel: stripped comp → too loud
3. Brought back comp + ×1.20 final → still a hair quiet
4. ×1.20 × 1.15 = ×1.38 → still wanted a bit more
5. ×1.38 × 1.05 = **×1.449** (final)

**Iteration trail for delay character**:
1. Original SLDIRTY: gkDelayMS=40320 (40ms), fb=0.25, mix=0.236
2. Dropped to 0.18 mix, fb 0.40
3. Bumped to gkDelayMS=280000 (280ms) "spring delay" voicing — user said "this is 18th notes I need 32nd notes"
4. 62500 (62.5ms = 32nd at 120 BPM), fb=0.62, mix=0.25
5. **53125 (53ms, 15% faster than 32nd), fb=0.62, mix=0.20** (final)
6. Added HP/LP filtering on feedback for spring tone shaping (180-4000Hz band)

