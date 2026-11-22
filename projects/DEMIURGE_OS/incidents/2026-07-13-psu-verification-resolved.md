# 2026-07-13 — PSU verification: RESOLVED / VERIFIED GOOD

## Follow-up being closed

`incidents/2026-05-14-bypass-mode-collapses-sync-latency.md` listed, under
"Known follow-ups (not done this session)":

> 1. **PSU verification** — confirm 5V/5A USB-C PSU + `usb_max_current_enable=1`
>    to stop the 0x50005 events from recurring.

That follow-up traced back to a mid-session under-voltage event
(`vcgencmd get_throttled = 0x50005` — under-voltage NOW + currently
throttled + both occurred-since-boot) which briefly dropped the Pi to
1 GHz and was suspected to correlate with FX-engage glitches even in
bypass mode.

## Verified in a live session (2026-07-13)

- **PSU**: confirmed official 5V/5A USB-C supply in use (not a phone
  charger).
- **`usb_max_current_enable=1`**: confirmed present and applied in
  `/boot/firmware/config.txt` (this is the flag Phase 8 of
  `setup-script/setup-demiurge.sh` writes automatically).
- **EXT5V rail measured 4.95 V under load** — comfortably within spec,
  no sag under DSP + USB audio load.
- **Full 2.8 GHz overclock config** (the Phase 8 `config.txt`/`cmdline.txt`
  block: `arm_freq=2800`, `over_voltage_delta=50000`, `gpu_freq=500`,
  `usb_max_current_enable=1`, plus the `threadirqs isolcpus=3
  nohz_full=3 rcu_nocbs=3` cmdline tokens) confirmed **stable**.
- **Zero undervolt events across 7 days of kernel logs** — no recurrence
  of the `0x50005` throttle flag seen on 2026-05-14.

## Status

**RESOLVED — VERIFIED GOOD.** The PSU/current-delivery hypothesis from
2026-05-14 is confirmed fixed by `usb_max_current_enable=1` (now applied
automatically by Phase 8 of the installer) combined with the correct
5V/5A supply. No further action needed on this follow-up. If undervolt
events reappear, re-check `vcgencmd get_throttled` and the physical PSU
first before re-opening investigation elsewhere.
