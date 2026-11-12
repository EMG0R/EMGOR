# Demo run-of-show — Tuesday 2026-09-08

Ten-line version first, detail below.

1. Ethernet the Pi into the lab's switch (192.168.0.x); `ping 192.168.0.103`.
2. `ssh demiurge@neuralgrid.local` (fallback `192.168.8.233`), stop the monitor.
3. `python3 -m septagraph.pi.app --map ~/neuralGrid/septagraph/py/renderers/lab_map.json --lab 127.0.0.1:9999 --dry-run-lab` and eyeball printed messages against `LAB_OSC_CONTRACT.md`.
4. Open `http://neuralgrid.local:8767/` on a phone; confirm the grid mirrors real key presses.
5. **Direct Lab** (opens in **FULL** layout — all 64 pads play, colour-coded per instrument): run marimbot bottom-left→top-right (rows 3→0 ascend like a keyboard), tap lydia (rows 4-5), tammy (row 6), breakBot | ganaPati (row 7, 4+4), page a segment (row 6 has 4 pages; row 7's halves page independently), press an FSR — confirm dynamics. Flip to **SPLIT** if you want the on-grid sequencer on rows 4-7.
6. Load/save a sequencer pattern slot; hit play.
7. Switch to **Septagraph**, press LAB_DIRECT — confirm `[dry-run-lab]` prints for grid hits.
8. If a laptop is running a renderer on `--renderers`, switch to HYBRID — confirm both the lab prints and the laptop react.
9. Switch to **MIDI**, pick the output port, play a scale, tap an FSR pad.
10. When ready for the real lab: drop `--dry-run-lab`, set `--lab 192.168.0.103:8001` (**lab OSC listener already confirmed running by someone else — never start it yourself**), restart the app, then restart `neuralgrid-monitor` when done.

## Exact commands

```
ssh demiurge@neuralgrid.local            # fallback: ssh demiurge@192.168.8.233
echo CHANGE_ME_PASSWORD | sudo -S systemctl stop neuralgrid-monitor
cd ~/neuralGrid/septagraph/py

# 1. dry run first, always -- confirm messages before they can touch a robot
python3 -m septagraph.pi.app --http-port 8767 \
    --map ~/neuralGrid/septagraph/py/renderers/lab_map.json \
    --lab 127.0.0.1:9999 --dry-run-lab

# 2. when the dry-run output matches LAB_OSC_CONTRACT.md and you are ready
#    for the real robots (lab's own OSC listener already confirmed running):
python3 -m septagraph.pi.app --http-port 8767 \
    --map ~/neuralGrid/septagraph/py/renderers/lab_map.json \
    --lab 192.168.0.103:8001

# when done:
# Ctrl-C
echo CHANGE_ME_PASSWORD | sudo -S systemctl start neuralgrid-monitor
```

Phone/laptop URL: **`http://neuralgrid.local:8767/`** (fallback IP if mDNS
is flaky on the lab's network: `http://192.168.8.233:8767/`).

## Network checklist (before touching any UI)

- [ ] Pi is on the lab's 192.168.0.x network via **ethernet** into the lab
      switch (not the Pi's own wifi — the Machine Lab's listener is only
      reachable from that subnet).
- [ ] `ping 192.168.0.103` succeeds (the MTIID Mac mini running the lab's
      OSC listener).
- [ ] First run is **always** `--dry-run-lab` — compare every printed
      `[dry-run-lab] -> ...` line against
      `septagraph/py/renderers/LAB_OSC_CONTRACT.md` (address + arg shape
      per instrument) before dropping the flag.
- [ ] The lab's OSC listener is *already running* on the mini — this app
      never starts it, never SSHes into the mini, per the Machine Lab
      keep-exactly-as-is invariant.
- [ ] **All instruments are enabled** (per the GOR Python-server contract,
      2026-09-08 — see `renderers/LAB_OSC_CONTRACT.md`; there is no
      chuck/python mode switch). `lab_map.json` has 123 of 132 points
      enabled: marimbot 36 / tammy 26 / lydia 20 / modulettes 11 /
      rattleTron 11 / breakBot 11 / ganaPati 8 — the gaps are physically
      broken beaters (breakBot 0/7, ganaPati 0/4/5) and 3 marimbot notes
      outside the server's 36-note scale, not a mode gate. modulettes and
      rattleTron have no default FULL-layout row; assign one from the
      "Rows & segments" UI panel if you want them on the grid.
- [ ] The **LAB pill** in the UI header goes green once the app's `/ping`
      (sent every 2 s) gets a `/pong gor_server <uptime>` reply back — if
      it stays red, the lab's OSC listener isn't reachable even though the
      steps above may all check out.
- [ ] `neuralgrid-monitor.service` and this app **cannot run at the same
      time** (both own the ESP32 serial port) — always stop the monitor
      first, always restart it when the demo/session ends.

## Direct Lab grid at a glance (FULL layout, the default)

Bottom = low, top = high (keyboard convention). LED colour = instrument.

| row | what | pages | LED |
|---|---|---|---|
| 0 | marimbot page 3 — 83 84 86 89 90 92 93 94 | 4 | amber-orange `(255,120,0)` |
| 1 | marimbot page 2 — 69 71 72 73 74 77 79 81 | 4 | amber-orange |
| 2 | marimbot page 1 — 57 59 60 62 64 65 66 67 | 4 | amber-orange |
| 3 | marimbot page 0 — 45 47 48 50 52 53 54 55 | 4 | amber-orange |
| 4 | lydia page 1 — 70..76 (x=7 dark) | 2 | cyan `(0,200,255)` |
| 5 | lydia page 0 — 62..69 | 2 | cyan |
| 6 | tammy page 0 — notes 0..7 (then 8..15, 16..23, 24..25) | 4 | green `(60,255,90)` |
| 7 | x0-3 breakBot 1 2 3 4 (3 pages of 4) \| x4-7 ganaPati 1 2 3 6 (2 pages of 4) | 3 \| 2 | red `(255,40,60)` \| violet `(170,60,255)` |

Layout toggle (UI, top of the Direct Lab panel): **FULL** = every pad
plays, sequencer runs from the UI only; **SPLIT** = rows 0-3 play, rows
4-7 = on-grid 4x8 sequencer (dim warm white, amber playhead). Each row is
a list of segments (`instrument / width / page i/n`, widths sum to 8) —
edit them in "Rows & segments"; the layout is saved to
`~/.neuralgrid/direct_layout.json` (v2; an old v1 file is migrated
automatically, keeping pad overrides and FSR assignments).

`curl -s http://neuralgrid.local:8767/api/state | jq .state.direct_lab.rows`
shows the live segment table from any laptop on the LAN.

## Mode cheat-sheet: what each softpot/FSR does per mode

| Control | DIRECT LAB | SEPTAGRAPH | MIDI |
|---|---|---|---|
| Pad (rows 0-3) | strike the pad's catalog point (segment-paged) | strike `/hit` — snap to catalog or continuum per preset | note-on (row-major, base note + scale) |
| Pad (rows 4-7) | FULL: strike the pad's point · SPLIT: sequencer step toggle | — (sequencer targets Direct/MIDI only) | sequencer step toggle |
| Softpot N | tempo ±40 BPM | rate axis modulator (held voice) | CC 1 |
| Softpot E | velocity offset ±0.3 | loudness axis modulator (held voice) | CC 3 |
| Softpot S | (unused) | space axis modulator (held voice) | CC 2 |
| Softpot W | (unused) | harmonicity axis modulator (held voice) | CC 4 |
| Lidar (nearest) | (unused) | spread axis (global, unheld `/move`) | CC 74 |
| FSR0-3 | 4 assignable strike pads, velocity = pressure | `/hit` at window center (voice `grid:fsrN`), held pressure -> `/move` on loudness | 4 notes (default 36/38/42/46), velocity = pressure×127, held -> channel aftertouch (off by default) |

## Septagraph presets, what they actually do now

- **LAB_DIRECT** — window on the Machine Lab catalog's centroid/bbox,
  snap 1.0 (always snaps), traffic -> local `LabRenderer` only.
- **CONTINUUM** — window at center, wide freq/loud/harm/scale extent, no
  snap, traffic -> `--renderers` only (needs a renderer running there).
- **HYBRID** — same window as CONTINUUM, snap 0.06, traffic -> both the
  lab and `--renderers`. **This is the one to demo with a laptop running
  a ChucK/DDSP renderer** (`--renderers HOST:PORT` when starting the app).
- **MIRROR** — greyed out, no model built yet.
