# Pi 5 core layout: three audio cores, one management core

**Status: STAGED, NOT APPLIED.** Nothing here has been run, installed or
measured. Do not apply it during a performance. EMGOR owns
`/boot/firmware/cmdline.txt` and all throttling/`power=` settings; every step
below is by hand, at the bench, engine idle, with a reboot.

## Layout

| Core | Role | Contents | Sched |
|---|---|---|---|
| 0 | Utility / management | OS, launcher + helpers, Demiurge transport clock, web UI, OSC/MIDI pool, updater, COD, utility audio, all device IRQs | normal; pacing IRQ threads FIFO 85 |
| 1 | Csound (RT) | `csound` / `demiurge-csound-host` | FIFO 74 (unchanged) |
| 2 | Cardinal (RT) | `Cardinal` JACK standalone engine | FIFO 70 |
| 3 | JACK (RT) | pipewire-jack `data-loop.*` (graph driver); third audio process later | pipewire's own FIFO 83 (not changed) |

Priority order if a core is ever shared: IRQ 85 > pipewire 83 > Csound 74 >
Cardinal 70 (matches `docs/cardinal.md`). Separate cores remove contention,
not the deadline: each core still has to finish its block in the period.

## How it extends demiurge-power (one mechanism)

The `demiurge-power` family (`demiurge-power.sh` + `.service`, the
`demiurge-power`/`demlow`/`demmed`/`demhigh` wrapper) owns CPU frequency,
governor and cores online, driven by `live.conf power=`. The pinning half
already exists too: `DEMIURGE_RT_PIN` in `src/wrappers/demiurge-env.sh`
(`taskset -c <isolcpus core>` plus `chrt --fifo 74` in each `demiurge-run-*`).
This work only generalises that single-core pin to a per-role core:

- `config/pi5-performance/demiurge-coremap.conf` — the map and priorities.
- `config/pi5-performance/demiurge-coremap.sh` — `plan` / `verify` (read-only),
  `exec <role> -- cmd` (what a future `demiurge-run-cardinal` would call, same
  `taskset ... chrt --fifo` form as csound), `apply` (re-pin running procs).
- `config/pi5-performance/cmdline.txt.coremap` — the isolcpus change.

No new unit, no boot hook, no second pinner. Guards: all modes except
`plan`/`help` need `DEMIURGE_COREMAP_CONFIRM=bench-not-live`; `apply` also needs
`DEMIURGE_COREMAP_ALLOW_RUNNING=yes` and refuses unless `isolated` reads `1-3`.

## Changes to make by hand (one bench session)

1. `cmdline.txt`: replace `isolcpus=3` with `isolcpus=1-3`
   (see `cmdline.txt.coremap`). Keep `threadirqs usbcore.autosuspend=-1`.
   `nohz_full`/`rcu_nocbs` stay inert on the stock kernel.
2. `demiurge-irq-isolate.sh`: `MASK_HEX=7`->`1`, `MASK_LIST=0-2`->`0`
   (all IRQs and unbound workqueues on core 0). The existing audit showed all
   device IRQs already land on CPU 0, so this is expected to be low risk.
3. `config/systemd/demiurge.service.d/cpu-affinity.conf`: `CPUAffinity=0-2`->`0`;
   keep `AllowedCPUs=0-3` (do not use `AllowedCPUs=3`/single core: measured
   worse, see that file).
4. Wrappers: csound needs core 1, not "last isolated core". `demiurge-env.sh`
   currently picks the last token of isolcpus (would be 3 = JACK's core). Set
   `rt_core = 1` in `live.conf` (existing key) or `DEMIURGE_RT_CORE=1`.
   A future `demiurge-run-cardinal` uses core 2 / FIFO 70.
5. `power =`: **do not use `low`** with this layout. `demiurge-power.sh` low keeps
   only 3 cores online (cpu0 + first isolated + one more) and would offline a
   core the map needs. `medium`/`high` keep all four. `demiurge-power.sh` also
   reads only the first isolated core from `isolated` ("1-3" -> 1); fine for
   medium/high, needs a small patch if low must work.
6. Reboot, then (engine idle):
   `DEMIURGE_COREMAP_CONFIRM=bench-not-live DEMIURGE_COREMAP_ALLOW_RUNNING=yes demiurge-coremap apply`
   only if a process was started without the wrapper pin; otherwise `verify`.

## RT / kernel

- Audio group limits already allow rtprio 99 (`demiurge-audio.limits.conf`).
- Kernel is `PREEMPT` (not RT). `docs/preempt-rt-plan.md` is the separate,
  unexecuted plan for `PREEMPT_RT`; the core map does not depend on it, and
  RT is expected to help the wakeup-lateness issue, not replace isolation.
- Existing finding to keep: `data-loop.0` belongs on the isolated core
  (pinning it off was measured worse), so JACK on core 3 is consistent.
  The pipewire main loop and wireplumber go to core 0.

## Target and module-budget method

Target: JACK quantum 64-128 at 48 kHz (`live.conf quantum`; 64 is current,
128 is the fallback if Cardinal xruns). Find the Cardinal ceiling:

1. Idle rig, Csound chain loaded at its normal patch, layout applied and
   `verify`d.
2. Load a Cardinal patch; add modules a few at a time.
3. Per step, run several minutes watching `jack_cpu_load` and the xrun count
   (`pw-top` ERR column, `jack_` xrun log).
4. The first xrun is the ceiling; back off one step. Budget = module
   count/patch where xruns stay 0 and `jack_cpu_load` stays under the agreed
   headroom. Repeat at quantum 64 and 128 and record both.

## Assumptions / must be bench-verified

- Cardinal's engine thread(s) honour affinity and its JACK process thread is
  what `taskset -a` catches; unverified (no Cardinal wrapper exists yet).
- pipewire-jack client RT threads run in the client process (Cardinal on core 2,
  Csound on core 1) while the graph driver runs on core 3; the cross-core
  wakeups add latency vs. today's single core. Measure round-trip
  (`demiurge-ringtest`) before and after.
- `isolcpus=1-3` leaves only core 0 for everything else; today's audit showed
  cores 0-2 ~6% busy, but the UI + web + clock + updater on one core under
  Cardinal load is unmeasured.
- Phone/low-power laptop mode (3-core `low`) is incompatible; see step 5.
- Verify afterwards: `cat /sys/devices/system/cpu/isolated` -> `1-3`;
  `demiurge-coremap verify`; no IRQs on cores 1-3 in `/proc/interrupts`.
