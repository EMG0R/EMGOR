# device.md — per-node identity (TEMPLATE)

A `device.md` sits ON TOP of the shared `MD/DEMIURGE.md` (the parent class).
Every Pi runs the Demiurge distro, so they all share `DEMIURGE.md`; `device.md`
is the thin layer that says *which device this is*. **The device persona wins;
Demiurge is the parent.** (bathtub §8.2)

This is the file COD and Demiurge both agree on — COD reads it to register the
node into the tank; Demiurge reads it to pick device-specific behaviour. Agent D
(COD) consumes this format. Keep the front-matter keys stable; prose below is free.

```yaml
---
# --- identity ---
hostname:    demiurge-node1        # ALWAYS demiurge-<unique>. The distro name + the device name.
device_name: NODE ONE           # human display name
unique:      node1                 # the suffix of the hostname; the node's short id
node_id:     node_one           # tank node id (your laptop); stable, snake_case
role:        instrument            # instrument | server | navigator | controller | laptop
hardware:    pi5-8gb               # pi5-16gb | pi5-8gb | pi4-8gb | mac | ...
# pair:      demiurge-neuralgrid-b # OPTIONAL: other board of a two-board device (Neural Grid)

# --- audio profile (how DEMIURGE.md's chain runs HERE) ---
audio_hat:   none-yet             # e.g. hifiberry-dac-adc-pro | none-yet (waiting on 8in/8out HAT)
power:       high                  # live.conf power knob default for this device
runs_audio:  true                 # false for server/laptop that host UI but no RT chain

# --- cod tank ---
tank_node:   true                 # is this a COD tank node
codling:     null                 # device persona/codling md path, if any (device persona overrides demiurge)
---
```

> device.md must cover BOTH shapes: NODE ONE (role: instrument, hardware: pi5-8gb) and
> Neural Grid (role: navigator, 2x Pi 5: one logical instrument, two boards, so each board
> gets its own device.md with `pair:` pointing at the other). Hostname scheme for the two
> boards (e.g. demiurge-neuralgrid-a / -b) is NOT decided. Lives in `demiurge/local/` (local bucket).

## What this device IS
One paragraph, in the device's own voice if it has a persona. NODE ONE: the
Pi 5 8 GB used to finish the distro before it is flashed onto the final instrument.

## sync vs local on this device
State which Demiurge services are enabled here and whether this node's extra
files live in `/demiurge/sync` (ships to all) or `/demiurge/local` (this box only).
**Always answer: sync bucket or local bucket?** (standing rule)

## Notes
Anything device-specific an agent must know before touching it (e.g. "waiting on
8in/8out HAT, HDMI wanted, new housing + screen in progress").
```
