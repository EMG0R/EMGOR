# SEPTAGRAPH / neuralGrid — Full Handoff (2026-09-03)

Continuation doc for the next AI/session. Author of record: emgor
(maintainer@example.invalid). This supersedes the legacy handoff (now at
`legacy-planning/HANDOFF.md`). Everything in `legacy-planning/` is
foundation, not current planning.

---

## 0. Read these, in order

1. `docs/SEPTAGRAPH_PLAN.txt` — **the official plan v2.** Thesis, space,
   protocol, window model, models, renderers, artistic defaults,
   milestones M0–M6, open items. Source of truth for intent.
2. `paper/septagraph.tex` — NIME paper draft v0.1 (LaTeX + TikZ figures;
   compiles on Overleaf; no local LaTeX toolchain on the Mac).
3. `docs/PAPER_DRAFT_v1.txt` — earlier plain-text title/abstract/skeleton
   (still says [NAME]; superseded in content by the .tex but kept).
4. `docs/PROTOCOL.md` — the ESP32↔Pi COBS serial contract (legacy-era but
   LIVE — it documents the running firmware; extended by lidar messages,
   see §4).
5. `legacy-planning/` — old spec (2026-07-06), dimensional map, plan v1,
   old handoff. Background only. Do not resurrect ideas from there
   without checking the new plan first.
6. `docs/superpowers/plans/2026-09-03-septagraph-implementation.md` —
   integrated implementation plan (Phases 0–6, TDD task-by-task). Embeds
   the numeric spec (§A/§B/§C) that Phase 0 wrote out as the three
   standalone docs below.
7. `docs/AXIS_SEMANTICS_v1.md` — §A, the frozen v1 numeric meaning of the
   8-value coordinate (`K`), physical maps, σ semantics.
8. `docs/WIRE_FORMAT_v1.md` — §B, the frozen v1 OSC wire grammar (all five
   message families), worked `/sept/v1/hit` byte example, conformance
   checklist, and the decisions made versus plan v2.
9. `docs/WINDOW_MATH_v1.md` — §C, the frozen v1 window projection math
   (controller-side only, never on the wire): center/extent/assignment,
   velocity, modulators, snapping, density field.

---

## 1. Project identity (naming, locked)

- **Septagraph** — the protocol / format / paper subject. A captured or
  authored sound-shape object is "a septagraph"; adjective
  "septagraphic".
- **neuralGrid** — the physical grid controller (this repo's hardware) —
  ONE implementation of the controller side.
- **PRISM** (provisional, pending collaborator Colton) — the audio-Pi
  reference renderer box.
- The system deliberately has two roles: controller (predicts the human,
  never hears) and renderer (makes sound, never sees the player), meeting
  ONLY in a shared perceptual coordinate space.

## 2. The locked concept (compressed — full detail in SEPTAGRAPH_PLAN.txt)

- **7-axis space v1** (versioned schema): frequency, loudness,
  harmonicity (tone↔noise), rate (temporal modulation), scale (spectral
  modulation), space (azimuth/spread/distance), time (trajectory
  parameter). Claim = independent STEERABILITY, not orthogonality.
  Grounding: spectro-temporal modulation representations (Shamma;
  McDermott/Simoncelli texture work). "Any sound = a point" is FALSE and
  never claimed: **a sound is a septagraph = trajectory of distributions
  μ(t), σ(t) over axes 1–6 along normalized time + one duration param.**
- **Protocol = 4 broadcast messages, 3 planes:**
  - `/manifest` (session plane; ONLY renderer→controller channel):
    reachable region. Machine Lab = measured catalog (each note tagged
    instrument, room position, mechanical constraints e.g. max re-strike
    rate). Synth = continuous bounds. Input side mirrors it: an INPUT
    MANIFEST file declares controller channels by geometry → brain is
    device-agnostic (trackpad = 1×2D continuous).
  - `/shape` (slow plane): septagraphs + window configs. Applied at
    **detected gesture boundaries** (stillness), NOT beats — clockless.
    Law: the mapping NEVER changes mid-gesture; model latency is thereby
    structurally inaudible. Shared clock demoted to optional feature.
  - `/hit` (fast plane, sub-10ms, no model ever): onset/release + VOICE
    ID + velocity + window-local coords.
  - `/move` (fast plane): continuous streams (ribbon/pressure/height)
    50–100 Hz, voice-bound when modulating a held note.
- **The window:** model NEVER maps gestures; a deterministic axis-aligned
  window (center+extent per axis + discrete assignment map) projects
  device space→7D instantly. The model only MOVES the window between
  gestures. EXTENT (playable range, config) ≠ SPREAD (σ, renderer
  liberty, from model uncertainty) — deliberately distinct. Two layers:
  PLAY window + LOOP window (loop frozen by default so loops keep their
  sound). Manifests merge into a density field; cells snap to catalog
  points first; optional gravity toward catalog-dense regions.
- **One controller model** (deaf, manifest-blind): tiny causal sequence
  model (GRU/TCN ~100k–1M params), MDN head → next staged config
  (windows + assignments + looper ops). Trained self-supervised
  next-action prediction + behavior cloning of manual play. σ = mixture
  entropy. Cold start law: fully usable with model off; manual → log →
  train → enable. Looper is deterministic live-layer capture/replay of
  protocol data; loop FX = geometric transforms on septagraphs (stutter =
  chop time axis, smear = widen σ, displace = translate frequency).
- **Renderers:** Csound (deterministic ref), DDSP (neural ref;
  harmonic/noise ↔ harmonicity axis; RAVE-class = Colton), Machine Lab
  (ONE renderer, measured catalog of all ~6–7 robot instruments,
  nearest-neighbor k-d tree; per-instrument split allowed later).
  **Residual rule:** flag; synth renders (requested − nearest catalog)
  and defers fully within snap radius r of a catalog point. Tonal
  alignment via shared frequency axis + shared tuning render tables
  (tuning is NEVER a model dimension).
- **Instruments/UI:** "instrument" = saved config. Stable instrument =
  window speed 0 (frozen on Machine Lab catalog; same window on DDSP =
  the mimic). Speed slider (web UI only) = cap on window velocity =
  model authority. Web UI = session plane; physical UI = performance
  plane only. Shipped presets v0: LAB DIRECT / CONTINUUM / HYBRID /
  MIRROR — these four ARE the demo script.
- **Artistic defaults (decided; change by playing not debating):** grid
  x→frequency, y→scale; velocity→loudness; softpots = per-axis live
  modulators N=rate E=dynamics S=space W=harmonicity (menus retired to
  web UI); lidar→space axis + approach velocity as anticipation feature;
  LED language (warm white = physical catalog point, blue-green =
  continuum, violet pulse = prediction ghosts, amber = loop playhead,
  ~150ms LED crossfade on every window swap).
- **MIDI positioning for the paper:** never self-claim "MIDI of
  perception"; the intro derives it — MIDI's radical underspecification
  lesson + the generative-audio control-vocabulary gap. σ =
  underspecification made explicit/performable. Multi-renderer interop
  matrix (controllers × renderers, every cell a working demo) is the
  paper's central evidence. NIME odds estimated: ~15–20% as-is, ~50–55%
  with two renderers + latency table, ~80–85% with full matrix +
  performances + experience data.

## 3. Hardware — AS BUILT AND VERIFIED (all working as of 2026-09-03)

**ESP32 (classic WROOM devkit, D-labels = GPIO numbers):**
- **NeoTrellis 8×8** (4× 4×4, addresses 0x2E 0x2F 0x30 0x31), I2C bus 0:
  SDA=GPIO21, SCL=GPIO22, now at **400 kHz** (dropped from 800 to meet
  VL53L1X max; side-effect: softpot stream ~42 fps instead of 50 —
  accepted). Trellis on its separate 5V rail, common ground.
- **4× SoftPot ribbons** → ADC1: N=GPIO32, S=GPIO33, E=GPIO34, W=GPIO35.
  - **Supply = 3.3V** (was VIN/5V — MOVED 2026-09-03; 5V was clipping the
    top ~⅓ of travel and over-volting the ADC pins. Watch for any
    residual per-channel jumpiness = possible ADC pin stress; spare ADC1
    pins exist if a remap is ever needed.)
  - **Pulldowns FITTED: ~82–100 kΩ wiper(middle pin)→GND, one per strip**
    (82k confirmed fine; anything 47k–220k ok, same value all four).
    Untouched = clean 0. The old floating-noise reality (violent
    wideband, see touch.py docstring) is GONE — touch.py's
    stability-machine assumptions are now obsolete for this hardware; a
    simple threshold+median+EMA law is the planned replacement (not yet
    written).
- **4× VL53L1X lidar**, SAME I2C bus as Trellis (21/22, piggybacked —
  decided over a second bus; fallback to Wire1 on GPIO25/26 documented in
  firmware README if LED traffic ever stutters).
  - XSHUT: N=GPIO4, S=GPIO13, E=GPIO14, W=GPIO27. INT pins unconnected.
  - Boot sequence assigns N=0x2A S=0x2B E=0x2C W=0x2D (all default 0x29;
    XSHUT stagger is the only reason they coexist — PROVEN live).
  - History: repeated per-position failures were (a) marginal
    XSHUT joints — a dangling XSHUT = sensor stuck awake at 0x29
    ("zombie") poisoning OTHER sensors' init, which made faults appear
    to move around; (b) one suspected-bad S module that came good after
    resoldering. Bus crowding was DEFINITIVELY ruled out (100 kHz test
    build failed identically). **All four now range.**
- Known quirk: GPIO14 (E XSHUT) outputs a clock pulse during ESP32 boot
  (MTMS) — harmless, sequence drives it LOW before use.

## 4. Firmware — `firmware/esp32/grid_link_lidar/` (fw=4, FLASHED)

Fork of `grid_link` (original untouched + rollback image kept:
`firmware/esp32/grid_link/build/grid_link.ino.merged.bin`).
FQBN `esp32:esp32:esp32`, core 3.3.8, Pololu VL53L1X 1.3.1, Adafruit
seesaw/NeoTrellis stack. Builds in `build/` (400k) and `build-100k/`
(bus test), mirrored on the Pi.

Still a **dumb bridge** (all logic Pi-side), plus:
- **TYPE_LIDAR 0x13** upstream: 4× uint16 LE mm, 0xFFFF=invalid, every
  50 ms (20 fps) alongside ANALOG (~42 fps) and KEY.
- **TYPE_LIDARDIAG 0x14** upstream (boot + with HELLO on every PING):
  per-channel responds-alone flags, zombie flag, 16B full-bus ACK
  bitmap (probe 0x08–0x77 with all XSHUT low; expect only Trellis),
  4B live per-channel bring-up failure stage.
- **Zombie handling:** a sensor found awake at 0x29 with all XSHUT low is
  readdressed ("parked") to 0x08 so 0x29 stays clean, and flagged.
- **Self-healing lidars:** 5s round-robin re-init of dead channels (with
  a 0x29-vacancy gate); runtime dead detection (no data-ready 5s →
  XSHUT low → retry pool).
- Compile-time `I2C_BUS_KHZ` flag (default 400; 100 for bus tests — see
  firmware README incl. compiler.cpp.extra_flags caveat).
- **Flash procedure (from the Pi — never the Mac):** stop the monitor
  service first; esptool 5.3.1 (no arduino-cli on Pi):
  `echo CHANGE_ME_PASSWORD | sudo -S systemctl stop neuralgrid-monitor`
  `PORT=$(ls /dev/serial/by-id/usb-Silicon_Labs_CP2102*)`
  `python3 -m esptool --chip esp32 --port "$PORT" --baud 460800
   write-flash 0x1000 <bootloader> 0x8000 <partitions> 0xe000
   boot_app0.bin 0x10000 <app>` then start the service. Full commands in
  `firmware/esp32/grid_link_lidar/README.md`.
- The ESP32 streams NOTHING until it receives a PING.

## 5. Pi — infra, services, network

- **Host:** `demiurge@neuralgrid.local`, Pi 5 16GB, DEMIURGE_OS. sudo
  password: CHANGE_ME_PASSWORD. Code at `~/neuralGrid/` (mirror of this repo's
  relevant subset; deploy = scp/rsync, ADDITIVE-ONLY except files noted
  editable below).
- **Network:** WiFi **GOR_CAVE** joined + autoconnect (wlan0
  192.168.8.233 as of 2026-09-02; ethernet optional). If `.local` fails
  in a browser (VPN/mDNS), use the IP: http://192.168.8.233:8765.
- **Monitor service (systemd, enabled): `neuralgrid-monitor`** →
  `~/neuralGrid/pi/tools/monitor.py`, port **8765** (HTTP + SSE,
  stdlib-only). Status: `systemctl status neuralgrid-monitor`; logs:
  `journalctl -u neuralgrid-monitor`. VERIFIED self-healing: boot
  autostart, crash restart (3s), ESP32 replug auto-reconnect (watchdog:
  no HELLO 12s → re-glob port → reopen → re-ping; 30s first-HELLO grace),
  port-absent retry loop, **hard-reset-to-run on every open** (rescues
  ESP32 stranded in ROM download mode). NEVER run a second process
  against the serial port while the service runs.
- **Monitor UI** (http://neuralgrid.local:8765): 8×8 live grid; 4 softpot
  lanes (raw bar, sparkline, jitter, TouchDetector gated state); 4 lidar
  lanes (mm bar 0–2000, state ok/dead/absent, addr+XSHUT labels);
  diagnostics panel (responds-alone per channel, ZOMBIE banner, bus ACK
  list, bring-up failure stage); link state + reconnect count + HELLO
  age; **lavalamp toggle**.
- **Lavalamp renderer** (inside monitor.py, daemon thread, 20 fps
  send_leds_full): purple field (hue ~0.78±0.05), pads drifting 5–15%
  brightness (3 drifting sine fields, ~15–30s periods); key press = pad
  snaps to 100% whitish-purple, snaps back on release (NOTE: user's
  original spoken spec was 10–30% field / 50% press / ~1s release fade;
  a mid-task revision made it 5–15% / 100% / no fade — user has not yet
  confirmed which they want; ask if it comes up). Endpoint:
  `GET /lavalamp?set=on|off|toggle`. Global brightness 120, re-sent per
  reconnect. LED index order verified: i → x=i%8, y=i//8.
- **Editable-by-tooling files** (created by tooling, may be modified):
  `pi/tools/monitor.py`, `pi/tools/README.md`, `pi/neuralgrid/codec_lidar.py`,
  `pi/neuralgrid/link_lidar.py`, `pi/test_lidar.py`,
  `firmware/esp32/grid_link_lidar/*`. EVERYTHING else in _EMGOR_SYNTH:
  additive-only, never move/rename/delete (git lock-in before risky ops).
- Keep Mac + Pi copies of monitor.py/README.md in sync when editing.

## 6. Pi-side code map (`pi/`)

- `neuralgrid/` — codec.py (COBS/CRC16, HELLO/KEY/ANALOG/LED msgs),
  link.py (GridLink, threaded reader), touch.py (stability-based softpot
  gate — see §3 note about obsolescence), codec_lidar.py (0x13/0x14
  parsing, fw3+fw4 bodies, unit-tested), link_lidar.py (LidarGridLink —
  plain GridLink silently drops unknown types), config.py (schema v1
  VECTOR_LEN=89 config + downbeat swap — legacy Phase 3, still the seed
  of the window/config mechanism), models/ (legacy Interface-VAE 4.5k
  params, synthetic-trained — SUPERSEDED conceptually by the MDN
  sequence model in the plan; keep as reference), live/ (legacy Phase 2
  menus/engine — menu interaction model retired in plan v2).
- `test_link.py` (--selftest codec proof; live mode = rainbow + stream),
  `test_lidar.py` (lidar/softpot printout; stop the service first),
  `run_live.py` (legacy live layer entry).
- `tools/monitor.py` — see §5.

## 7. Current state / what just happened (chronological, condensed)

1. Concept work: legacy "beat-quantized interface reconfiguration"
   evolved into the full Septagraph protocol (see §2 + plan). Old
   planning moved to `legacy-planning/` (git lock-in commit first).
2. Paper drafted: `paper/septagraph.tex` (space + protocol + window
   sections in prose; TikZ: 7-axis radar, μ/σ septagraph plot,
   architecture planes, window projection; skeleton comments for
   remaining sections; starter bib + TODO cites: Wekinator, DDSP, RAVE,
   MPE, OSC, Hunt/Wanderley).
3. Pi joined GOR_CAVE wifi; monitor UI built; softpot noise diagnosed →
   pulldowns fitted → softpots clean; supply moved 5V→3.3V.
4. Lidar campaign: firmware fork with XSHUT sequencing → 2 alive → zombie
   diagnosis → diagnostics firmware (fw4) → resolder → **all 4 alive**.
   Bus crowding ruled out empirically.
5. Monitor made fully self-healing + systemd (verified: kill, USB
   unbind/rebind, full reboot — zero manual steps).
6. Lavalamp idle/press renderer live on the grid.

## 8. Next steps (plan §9, in order)

- **M0 — DIRECT MACHINE LAB SCRIPT** (user-locked, next): standalone Pi
  script, no model/window — grid cells hand-mapped to the ~6–7 Machine
  Lab robot instruments, hits → OSC/MIDI to the lab server. Gig tool +
  end-to-end proof + seeds the catalog workflow. Machine Lab: CalArts
  MTIID mini at 192.168.0.124 ("blackberry"), existing OSC→MIDI/ChucK
  path — DO NOT modify the lab's own setup (keep-exactly-as-is
  invariant). NOTE: lab is on a different network than GOR_CAVE —
  connectivity plan needed.
- **M1** catalog measurement (record+analyze every robot note → manifest).
- **M2** window engine (write the window math FIRST — plan open item #1)
  + manual staging web UI + Csound PRISM v0.
- **M3** boundary detector, layers, capture front-end, looper+transforms.
- **M4** logging + MDN-GRU training + speed slider.
- **M5** DDSP renderer, residual rendering, Colton integration
  (wire-format freeze with him = plan open item #3), matrix video,
  performances.
- **M6** paper completion (latency table, matrix figure, related work).
- Small outstanding: rewrite touch.py for pulldown reality; decide
  lavalamp spec question (§5); confirm PRISM name with Colton; NIME
  deadline/logistics lookup.

## 9. Process rules (standing, from the user)

- Delegate implementation to subagents; keep the main window for
  conversation/design.
- Challenge the user's ideas hard; they want pushback, not agreement.
- Keep responses SHORT — the user is busy and stops reading long ones.
- Additive-only in the archive; git lock-in snapshot before anything
  risky; never modify the Machine Lab's own systems.
- Power off the ESP32 before touching I2C wiring (live rewiring wedged
  the bus once). After replug, nothing manual is needed anymore (service
  self-heals).

## 10. LATENCY (2026-09-03 — appended, not editing §3-9 above)

Input/LED latency pass on `firmware/esp32/grid_link_lidar/` (now **fw5**,
flashed and running) + `pi/tools/`. Full detail: firmware README's
"Latency work (fw5)" (root causes, mechanism) and `pi/tools/README.md`'s
matching section (before/after numbers, methodology). Compressed here:

- **Two real root causes found and fixed, both library bugs, not our own
  code:** (1) `Adafruit_MultiTrellis::read()` doubles up a 500us seesaw
  settling delay per board — `getKeypadCount()` already sleeps 500us
  internally, the library adds a second, fully redundant one — costing a
  fixed 2ms/loop for 4 boards, every loop, key pressed or not. (2)
  `seesaw_NeoPixel::setPixelColor()` does an I2C write PER PIXEL and
  `show()` always renders all 4 boards, so every LED frame — including the
  lavalamp's 20fps idle field — cost up to 64 I2C writes + 4 shows + up to
  4×300us latch waits regardless of what actually changed. Fixed with a
  fast custom key-scan (`pollTrellisKeys()`, same logic minus the
  redundant delay) and a shadow framebuffer + per-board dirty tracking
  (only changed pixels get written, only touched boards get shown).
- **Also:** key scan now runs first in `loop()`, unconditionally, never
  behind LED/lidar work; LED frames are coalesced ("keep latest" — a Pi
  burst of several LEDS_FULL/PARTIAL only ever renders the most recent
  one); lidar reads were already non-blocking (confirmed, not changed);
  lavalamp now sends `LEDS_PARTIAL` for small deltas (e.g. a single key
  press) instead of always re-sending all 64 pixels.
- **New protocol (fw5, versioned, PROTOCOL.md's locked messages
  untouched):** upstream `TYPE_ACK` (0x15, recv/done `micros()` + acked
  type — measures Pi→ESP32→`show()` latency at the source, immune to any
  Pi-side timing artifacts) and `TYPE_TIMING` (0x16, per-phase max-us
  telemetry, left on permanently — cheap, and how the two root causes
  above were actually found instead of guessed); downstream
  `TYPE_LOCAL_ECHO` (0x24, default OFF, optional Pi-gated immediate
  key→white-pad feedback that the Pi's own next frame always overrides —
  "dumb bridge" law unchanged).
- **Measured (fw4→fw5, same hardware/session, monitor stopped for a clean
  A/B):** LED apply (recv-complete→show()-complete) — not measurable on
  fw4 (no 0x15); fw5 isolated p50/p95 ≈ 5.1–5.2 / 6.2–6.6 ms (FULL and
  PARTIAL landed close together because the probe's own test pattern only
  touches ~2 pixels/frame — the dirty-tracking fix helped both equally);
  fw5 **live** (lavalamp running, real 20fps LED + full sensor traffic
  contending) PARTIAL p50/p95 = 8.31 / **9.81 ms** — under the task's
  <10ms p95 partial-update target under real load. **Link RTT (PING→HELLO)
  did NOT move** (~49.5ms p50 both before and after, within ~1ms across
  the board) — traced to `pi/neuralgrid/link.py`'s reader thread
  (`_serial.read(256, timeout=0.1)` polling loop), which is **not** in
  this task's editable-file set, so it could not be fixed here; called out
  explicitly rather than silently claimed fixed. This does not affect the
  ACK-based LED numbers above, which are timestamped entirely on-device.
- **Not verified:** physical key-press→Pi-arrival latency (<5ms p95
  target) end-to-end with an actual finger — no way to physically actuate
  the grid this session. The firmware change targeting it (key scan first,
  redundant delay removed) is real and shows up in the `keys`/`loop_total`
  phase telemetry, but wasn't timed against a real press. See
  `pi/tools/README.md` for a suggested follow-up test.
- **Sensor status at time of testing:** all 64 pads and 4 softpots not
  independently re-touched this session (no physical actuator available);
  automated check confirmed untouched softpot channels read clean 0
  (consistent with §3's documented pulldown fix — no regression). **Lidar
  N (0x2A) was intermittently/persistently reporting DEAD during this
  session's verification** (boot diagnostic showed it responding-alone
  fine, `fail_stage=ok`, so it passed init — the runtime "no data-ready
  for 5s" path is what's marking it dead, then the 5s self-healing retry
  cycle repeats). S/E/W ranged fine throughout (S≈69-70mm, E≈80mm,
  W≈32-33mm at the sensor's current physical position — very close-range
  readings, consistent with the sensor bench sitting close to something
  during testing, not a fault by itself). **This is very likely unrelated
  to the fw5 work**: the lidar init/retry/diagnostic code paths in
  `grid_link_lidar.ino` are byte-identical between fw4 and fw5 — nothing
  in this latency pass touched lidar logic. Recommend physically
  re-checking N's XSHUT joint (GPIO4) per the existing zombie/troubleshoot
  guidance in the firmware README before the next session.
- **Rollback:** `firmware/esp32/grid_link_lidar/build-fw4-rollback/`
  (Mac + Pi mirror) holds fw4's bins alongside `build/`'s new fw5 ones;
  exact flash command in the firmware README's "Build (Mac)"/rollback
  note. Service was stopped for flashing and restarted cleanly afterward
  (verified: `systemctl status` active, immediate HELLO fw=5, zero manual
  steps beyond the documented stop/flash/start).

### Follow-up (2026-09-03, later same day) — the link-RTT ~50ms fixed

The "link RTT did NOT move" negative result above is now resolved. Root
cause was entirely **Pi-side**, in `pi/neuralgrid/link.py`'s
`GridLink._reader_loop`: `self._serial.read(256)` with `timeout=0.1` — a
pyserial `read(n)` blocks until `n` bytes arrive **or** the timeout
expires, so a short frame (a HELLO/KEY/ACK is a handful of bytes, nowhere
near 256) sat in the OS driver buffer for up to the full 100ms before
being handed back to Python, mean ~50ms — exactly the measured ~49.5ms
PING→HELLO RTT.

`link.py` is frozen (legacy, not editable), so the fix lives in
`pi/neuralgrid/link_lidar.py`: `LidarGridLink` now overrides
`_reader_loop` with a `read(1)` (blocks only when the link is genuinely
idle) followed by a non-blocking drain of whatever pyserial's `in_waiting`
reports, fed into the existing `self._feed()`. Stop-event and
exception→break semantics are unchanged from the parent. A short frame
that has already arrived by the time the loop asks is dispatched in that
same iteration instead of waiting out the rest of a 100ms window. Unit
test: `pi/test_link_lidar.py` (fake serial, proves an 8-byte KEY frame is
dispatched via exactly `read(1)` + one drain read, never a `read(256)`).

**Measured on the Pi** (monitor stopped, `pi/tools/latency_probe.py
--rtt-samples 20 --led-samples 20 --stream-s 2.0`, same hardware/session,
fw5 unchanged from the pass above — this is a pure Pi-side fix):

| | before (fw5, old reader) | after (fw5, `read(1)`+drain reader) |
|---|---|---|
| link RTT p50 | ~49.5-49.6 ms | ~4.75-5.52 ms |
| link RTT p95 | ~52.5-52.9 ms | ~6.4-8.9 ms |
| link RTT max | ~91.7-94.6 ms | ~6.9-9.4 ms |

Two back-to-back probe runs both landed in the same low-single-digit-ms
band (p50 4.75-5.52ms, p95 6.4-8.9ms across runs of 20 samples each) —
consistent, ~10x improvement, no longer bottlenecked by the 100ms serial
timeout. Restarted `neuralgrid-monitor` afterward (`systemctl status`
active, immediate HELLO fw=5); its own rolling RTT window briefly carried
one ~440ms outlier from the very first post-reconnect sample (cold-start
artifact of the service's hard-reset-then-ping sequence, not the reader
fix) before settling into the same low-single-digit-ms band as the probe
once enough keepalive samples accumulated.

Does not touch `link.py` (frozen) or the firmware — purely a Pi-side
read-strategy change in the lidar-aware subclass, safe against older
firmware (it inherits everything else from `GridLink` unchanged).

## 11. FSR analog inputs (fw6, 2026-09-04) — appended, not editing §3-10 above

Four force-sensitive resistors (FSRs) added as new ADC channels on the same
firmware (`firmware/esp32/grid_link_lidar/`, now **fw6**). Full detail
(divider wiring, pin rationale, GPIO2-vs-GPIO12 tradeoff, timing impact) is
in the firmware README's "FSR wiring" and "fw6 timing impact" sections;
Pi-side detail (monitor UI lanes, unit tests) is in `pi/tools/README.md`'s
"FSR lanes (fw6+)" section. Compressed here as the §3/§4 addenda those
sections would have carried if this were a live edit:

### §3 addendum — hardware: 4x FSR

- **4x FSR** (force-sensitive resistor) → new ADC channels, appended to the
  existing 4 softpot channels (ANALOG `n`: 4 → 8; channel order
  `[0..3]=softpot N/S/E/W unchanged, [4..7]=FSR 0..3` — PROTOCOL.md already
  allowed this: "Adding FSR/piezo later only increases n").
  - **Pins:** FSR0=GPIO36(VP, ADC1), FSR1=GPIO39(VN, ADC1), FSR2=GPIO15
    (ADC2), FSR3=GPIO12 (ADC2). Only two ADC1 pins remained free (softpots
    already hold 32-35); the other two use ADC2, safe here specifically
    because this firmware never calls `WiFi.begin()`.
  - **GPIO2 rejected** as a candidate ADC2 pin: commonly carries an onboard
    LED on classic ESP32 DEVKIT-style boards, which would interfere with a
    divider reading. GPIO12 (MTDI strap) accepted over reassigning
    GPIO25/26 (the documented Wire1 bus-fallback pins) specifically to
    keep that fallback intact; the accepted risk is narrow (wrong flash
    voltage selected only if FSR3 is pressed at the exact instant of
    power-on/reset — see firmware README).
  - **Same divider convention as the softpots**: FSR leg → 3.3V (not 5V),
    other leg → ADC pin, **10kΩ to GND mandatory** on all four (GPIO36/39
    have no internal pulls at all, same ESP32 limitation as GPIO34/35).
    Untouched reads ~0, same as the softpot pulldown fix in §3 above.
  - **Not yet physically wired/tested this session** — see flash-status
    note under §4 addendum below.

### §4 addendum — firmware: fw6

Firmware bumped `firmware/esp32/grid_link_lidar/` **fw4→fw6** (`FW_VER`
now 6; `N_ANALOG` now 8). Compiles clean with `arduino-cli` (FQBN
`esp32:esp32:esp32`, same libs as fw5). `tickAnalog()` unchanged in cadence
(still gated at 20ms/~50Hz) — 4 extra `analogRead()` calls added, no new
phase; see firmware README's "fw6 timing impact" for the (currently
design-expectation, not yet live-measured) analysis. fw5's bins preserved
as rollback in `build-fw5-rollback/` alongside `build/`'s new fw6 images
and the existing `build-fw4-rollback/`, same pattern as the fw4→fw5
rollback. **Not flashed this session** — see the flash-status note
immediately below for why and the exact commands to flash when clear.

### Flash status — staged, not flashed

Another agent may have been using the Pi's serial port
(`python -m septagraph.pi.app` / `test_lidar.py` / `latency_probe.py`) at
authoring time; per this task's serial-port-coordination rule, flashing was
gated on the port being free. **Result: the fw6 binaries were built and
verified locally (compiles clean) but NOT flashed to the Pi in this
session** — see the top-level task report for the live port-contention
check outcome and the exact staged flash commands (same procedure as
firmware README's "Flash from the Pi", pointed at fw6's `build/` images).
Until flashed, the ESP32 continues running fw5 (`analog=4`); the Pi-side
code (monitor.py FSR lanes, codec/link parsing) is already generic in `n`
and will show `analog=8 fw=6` with live FSR lanes the moment fw6 is
flashed, no Pi-side redeploy needed for that part.

### Pi-side — generic-in-n confirmed, no frozen-file changes needed

`pi/neuralgrid/codec.py` (frozen) — `parse_analog`/`parse_hello` were
already generic in `n`/`n_analog` with no `n==4` assertion anywhere;
confirmed by new tests in `pi/test_codec_lidar.py` (`TestFsrAnalog`) at
n=8, not requiring the "parse ANALOG in codec_lidar.py instead" fallback
this task anticipated as a contingency. `pi/neuralgrid/link.py` (frozen)
dispatches `TYPE_ANALOG` generically too. `pi/neuralgrid/link_lidar.py`
unchanged (no FSR-specific work needed there). `pi/tools/monitor.py`:
`MonitorState.on_analog`/`snapshot` split the 8-channel tuple into the
existing 4 softpot values (unchanged TouchDetector path) and 4 new FSR
values (own deques, simple `raw >= 300` threshold badge, no gating) — see
`pi/tools/README.md`'s "FSR lanes (fw6+)" for the full writeup. UI: 4 new
FSR lanes (raw bar + sparkline + PRESSED badge, labeled with GPIO) next to
the softpot lanes.

## 12. LED transport de-flicker + lidar reliability check (2026-09-08)

**Root cause of the LED flicker: Pi-side, in `septagraph/py/septagraph/pi/app.py`, not firmware.**
Two compounding bugs, both in the app's LED send path (the app that drives
the grid day to day, replacing the monitor's lavalamp for normal use):

1. **No diff/skip.** `_led_loop` sent a full 64-pixel `LEDS_FULL` frame
   every tick, unconditionally, forever, at 20fps — even when the
   crossfade was inactive and the content was bit-identical to the last
   frame sent. fw6 already dedupes identical pixels on its own shadow
   framebuffer (`applyLedsFull`/`applyLedsPartial`), so this wasn't
   costing I2C writes on the ESP32, but it was still 20 pointless
   COBS-encode+serial-write+ACK round trips per second, permanently.
2. **The real bug: an unlocked data race.** `App._render_lock` was
   declared in `__init__` and never actually used anywhere. `_led_loop`
   (its own thread) calls `Crossfader.start()` — which mutates `_from`,
   `_to`, `_t0`, `_active` as four separate, non-atomic attribute writes —
   while `_on_key` (invoked from the serial reader thread on every
   keypress, per the module's own "fast path" docstring) concurrently
   calls `Crossfader.current()`, reading those same attributes. An
   unlucky interleaving hands `_on_key` a torn read (e.g. a new `_to`
   paired with a stale `_t0`), which gets computed into a wrong-colored
   frame and shipped straight onto the wire as an extra, uncoordinated
   `LEDS_FULL` send — a bad frame lands on the grid for one tick,
   immediately clobbered by the next correct one. This fires specifically
   on keypresses, i.e. exactly while the user is playing — matches the
   reported symptom.

**Fix (transport only — colour decisions in `modes.py`/`leds.py`'s colour
tables were untouched, per this task's coordination boundary with the
concurrent colour-mapping work):**

- `septagraph/py/septagraph/pi/leds.py`: added `diff(old, new)` (returns
  `(x,y,r,g,b)` for every changed pixel) and `PARTIAL_MAX_PIXELS = 24`.
- `septagraph/py/septagraph/pi/app.py`: added `_send_frame_locked(frame)` —
  skips the send entirely if `frame == self._last_sent_frame`; sends
  `LEDS_PARTIAL` when `<= PARTIAL_MAX_PIXELS` pixels changed, `LEDS_FULL`
  otherwise. `_render_lock` now actually guards every crossfader
  read/mutate + the send decision as one atomic unit, in `_render_leds_once`
  (`_on_key`'s path), `_led_loop`, and `switch_mode` — the three places
  that touch `self._crossfader`. Added `led_frames_sent_rate()` (frames
  actually written to the wire per second, reset on each call) surfaced in
  `get_state()` as `led_frames_sent_per_s` for exactly this kind of
  measurement, since eyeballing flicker isn't verifiable remotely.
- Firmware: **no change needed.** Read `grid_link_lidar.ino`'s `loop()` —
  it's single-threaded and strictly sequential
  (`pollSerial → pollTrellisKeys → tickAnalog → pollLidar → tickLidar →
  applyPendingLed → tickLidarReinit → tickTiming`), so `applyPendingLed()`
  (all pixel writes + `show()` for touched boards) always runs as one
  uninterrupted last step — lidar polling can never land mid-frame. The
  per-pixel dirty tracking and pending-frame coalescing from the fw5 work
  already cover the firmware side of this. Stayed on fw6.

**Measured on the Pi** (`neuralgrid-app-live`, real hardware, `--lab
192.168.0.103:8001`), via `led_frames_sent_per_s` and `/api/leds`:

- **Idle steady state: 0.0 fps sent**, sustained — down from a constant
  ~20fps of full-frame resends before the fix (every single one previously
  wasted, since nothing was changing).
- **On a genuine state change** (forced a mode switch to trigger the 150ms
  crossfade): rate jumped to **~22–24 fps for exactly the crossfade
  window**, then dropped back to **0.0 fps** the instant the frame landed
  on its exact final target (`/api/leds` hex confirmed the last frame sent
  matches the target bit-for-bit, no drift). This is "finish crossfades
  with an exact final frame" verified directly, not just by construction.
- One `TYPE_TIMING` sample taken under a synthetic LED stress (rapid
  alternating full-frame sends, ~100/s) showed `keys≈3.2ms`,
  `lidar_poll≈3.3ms`, matching the fw5/fw6 baseline in §10/§11 with no
  regression — consistent with the firmware structurally preventing LED
  traffic from ever starving lidar polling (see above).
- Added `septagraph/py/tests/test_led_transport.py` (7 tests): `diff()`
  correctness, skip-when-identical, small-delta-uses-partial,
  large-delta-uses-full, crossfade lands on the exact target frame past
  its duration, and a thread-hammer regression test that drives
  `_render_leds_once` and a `_led_loop`-shaped critical section from 6
  concurrent threads and asserts no exception — the concurrency class of
  bug this fix targets.
- **What the user should now see:** the grid should hold a rock-steady
  color at rest (no more animation-free 20fps churn), and should not show
  a stray flash/pop on individual keypresses (the race that could ship a
  torn/wrong-colored frame is now impossible — same lock guards every
  touch of the crossfader and every send).

**What was deployed to the Pi:** only the transport-owned pieces — the
`diff`/`PARTIAL_MAX_PIXELS` addition to `leds.py` and the send-path/lock
changes to `app.py` — built directly on top of what was already running on
the Pi (confirmed byte-identical to this repo's last commit before this
session) rather than scp'ing this session's full local working tree.
`modes.py` on the Pi already carried the concurrent colour-mapping agent's
in-flight work (note-shaded colours, gamma correction) independently of
this task; that was left untouched. A stray, unrelated `get_leds` debug
endpoint + `webui.py` signature change also present in the local working
tree (not authored by this task) was deliberately **not** deployed, since
`webui.py` is outside this task's edit set and the Pi's own `webui.py`
doesn't have the matching parameter yet — shipping just the `app.py` half
would have broken the live service. `app.py`'s `main()` call to
`webui.serve_forever(...)` was kept without the `get_leds=` kwarg to match.
Service restarted per the required stop/systemd-run/verify procedure;
confirmed `active`, no traceback, HELLO/API responsive.

**Lidar status (checked with the app stopped, `pi/test_lidar.py` + a
custom `LidarGridLink` probe, ~65s continuous streaming + a fresh
`LIDARDIAG` read):**

- **Boot-time bring-up: all four pass clean.** `LIDARDIAG`:
  `responded_alone = (True, True, True, True)`, `zombie = False`,
  `fail_stage = (0, 0, 0, 0)` (`"ok"` for N/S/E/W) — no XSHUT-joint
  symptom (a dangling XSHUT would show `responded_alone=False` or a
  zombie flag; neither is present).
- **Runtime, over 65+ continuous seconds: N alive and solid**
  (~15–18mm, a very close range — something is likely sitting right next
  to/on the N sensor on the bench). **S, E, W report `0xFFFF` (dead) for
  the entire 65s window, zero valid readings**, despite the firmware's 5s
  round-robin self-heal (with 3 simultaneously-dead channels, each gets a
  fresh full re-init roughly every 15s — so each was retried multiple
  times during the test window and never came back).
- This is the same **"started, then silent" signature §10/§11 already
  documented for N alone on 2026-09-03** (`fail_stage=ok` at boot, then
  "no data-ready for 5s" at runtime) — except now it's **S, E, and W**
  simultaneously, while **N** (previously the flaky one) is the one
  solid channel. That inversion, plus clean boot diagnostics on all four,
  points at a **shared-cause runtime/signal issue** (loose connector,
  bumped wiring, or a marginal joint that boot's brief probe doesn't
  stress) rather than firmware — firmware behavior here is unchanged from
  the documented fw4–fw6 self-heal logic, and boot bring-up itself is
  clean. **Per this task's explicit instruction, no software fix was
  attempted beyond the existing self-heal** — this needs physical
  inspection of the S/E/W XSHUT/SDA/SCL/VIN connections (same class of fix
  as the historical zombie/XSHUT-joint work in §3), most likely starting
  with whatever changed on the bench since 2026-09-03.
- Separately: `test_lidar.py`/a fresh `LidarGridLink` connection was
  noticeably flaky *while the app was stopped for this diagnostic* —
  several open attempts got no `HELLO` at all before one succeeded, with
  no code change involved (same script, same firmware, just retried). Not
  investigated further (out of scope for this task's "diagnose, don't
  software-fix the lidar" instruction), but worth a look if it recurs —
  possibly the ESP32 needs longer than pyserial's default DTR/RTS-triggered
  reset settle time before a fresh open's `PING` lands reliably back to
  back.

## 13. Flicker (still happening) + missing press-flash, round 2 (2026-09-08)

Follow-up to §12: after that session's Pi-side de-flicker fix (diff/lock),
the grid **still flickered** and a pressed pad **still didn't flash
white**. This session worked both, split cleanly by root cause:

### Press-flash: fixed and verified (Pi-side, `septagraph/py/septagraph/pi/`)

Two real bugs in `app.py`, neither touching firmware:

1. `_on_key`'s fast path called `_render_leds_once()`, which sent
   `self._crossfader.current()` — a **stale, already-computed** value —
   and never once consulted `active_mode.led_frame()`, which is what
   actually carries `DirectLabMode._press_times` → `leds.FLASH_WHITE`. The
   press was recorded correctly; the send just never looked at it.
2. `_led_loop` restarted a fresh 150ms `Crossfader` fade (`leds.CROSSFADE_S`)
   on **every** `led_frame()` change, not just real `switch_mode()` swaps.
   Direct Lab's `led_frame()` changes on basically every tick (presses,
   sequencer steps, FSR touches), so even a correctly-computed flash pixel
   got smeared across a perpetually-restarted 150ms fade racing a 120ms
   (`leds.FLASH_S`) flash window — it could never actually reach white
   before the target flipped back.

Fix: `app.py` now drives the crossfader ONLY from `switch_mode()` (a real
mode/window swap — what `leds.py`'s own module docstring says the
crossfade is for); ordinary per-tick content (`_render_leds_once()` and
`_led_loop`) goes through a new `_current_display_frame_locked()` that
sends `active_mode.led_frame()` live, blending through the crossfader only
while a switch-triggered fade is still in flight (and keeps that fade's
target fresh without resetting its timer, so a busy mode's own animation
doesn't freeze mid-fade). Covered by
`septagraph/py/tests/test_led_transport.py::test_key_press_flashes_pad_white_immediately`
(fake-grid, no hardware) — presses a pad, asserts white lands on the wire
synchronously inside the one `_on_key` call (one small `LEDS_PARTIAL`
send, not a full-frame flood), then asserts the real colour is restored
after `leds.FLASH_S` elapses via the same diff path. All 231 tests in
`septagraph/py/tests/` pass; all 25 in `pi/test_codec_lidar.py` +
`pi/test_link_lidar.py` pass. **This half is fully verified — no hardware
needed, `--fake-grid` covers it completely.**

### Lidar liveness misclassification: fixed (firmware, fw7)

Correction from the user mid-session: **S/E/W are not dead.** They range
correctly when something is actually in front of them; `0xFFFF` the rest
of the time just means nothing is in range, which is normal, not a fault.
§10-12 above misread that as "dead" — both conditions look identical
upstream (`0xFFFF` in `TYPE_LIDAR`) but are different firmware concepts:
`range_status != RangeValid` (no target; normal) vs. `lidarAlive[i] ==
false` (no `dataReady()` event at all for 5s; a real fault). `pollLidar()`
had a real, narrow gap: liveness (`lidarLastSeen[i] = now`) was recorded
in the RangeValid and not-valid-but-ran branches, but NOT in the
`timeoutOccurred()` (one-off I2C glitch) branch. Fixed: liveness is now
recorded the instant `dataReady()` is true, unconditionally, before the
result is even looked at. Since `tickLidarReinit()` only ever touches
`!lidarAlive[i]` channels, this alone should stop the 5s self-heal
round-robin from ever running against S/E/W — they were never actually
gone. **This reclassifies most of the "3 dead lidars, self-heal running
constantly" picture in §12 as a misread, not a hardware fault** — still
worth a physical joint check per §3's zombie/XSHUT guidance if a channel
*genuinely* stops producing any `dataReady()` at all, but that's a
different, narrower claim than "3 of 4 are dead."

### Flicker mechanism: NOT found in this source; NOT verified on hardware

The flicker itself is described as a fast, subtle **shimmer on lit pads
only** (dark pads look fine) — the classic seesaw/NeoTrellis symptom of
`show()` re-latching a board's WS2812 stream with unchanged data. This was
checked line by line against exactly that hypothesis (see the fw7 header
comment and `pollLidar()`'s comment in `grid_link_lidar.ino`, and the
firmware README's "fw7" section for the full writeup):
`applyPendingLed()` only ever runs when `pendingLedType != 0`, set ONLY by
`dispatch()` on a real incoming `TYPE_LEDS_FULL`/`TYPE_LEDS_PARTIAL`;
`applyLedsFull`/`applyLedsPartial` already diff every pixel against the
shadow framebuffer and only `show()` a board with an actually-changed
pixel (fw5's dirty-tracking fix, not new this session); `setBrightness()`
is only ever reached from an incoming `TYPE_BRIGHTNESS` message, never
per-loop. **No unconditional or insufficiently-gated `show()` path exists
in this repo's firmware source.** This does NOT mean the flicker isn't
real — it means one of these is true and needs checking next session,
in order:

1. **The Pi has no network path from this environment this session** —
   every `nc`/`ssh` attempt to `demiurge@192.168.0.242` timed out (the
   sandbox has no active Wi-Fi association at all), so **nothing here was
   verified against real hardware**. `pi/tools/led_idle_probe.py` (new
   this session) is written and ready — opens the link, PINGs, sends ONE
   real lit (non-blank — a shimmer on lit pads needs a lit baseline to
   even be testable) frame, then goes silent for 30s logging every
   `TYPE_ACK`/`TYPE_TIMING`/`TYPE_LIDAR` — run it FIRST next session,
   before touching anything else.
2. **Check which process is actually driving the grid**: `pi/tools/monitor.py`
   has its own, completely separate always-on 20fps lavalamp thread
   (`send_leds_full`, animating on purpose) that was never touched by
   either this session's or §12's Pi-side fix — if that's what's running
   instead of `septagraph/py/.../app.py`, the "0 fps at idle" measurement
   from §12 doesn't apply to it at all.
3. **Check the actually-flashed `fw_ver` via HELLO** — if the Pi is
   somehow still running something older than fw5, the dirty-tracking fix
   this analysis relies on isn't even present on the device.
4. If all three check out clean and the idle probe still shows nonzero
   `led_apply`/ACKs or a visible shimmer, the remaining candidate is
   electrical, not logical: I2C/XSHUT bus activity (even the throttled fw7
   re-init, or just normal lidar polling) coupled onto the shared rail
   glitching a seesaw board's WS2812 output mid-frame — would explain
   "lit pads only" (current draw only happens on lit pixels) without any
   firmware logic bug at all.

**fw7 changes made (compiled clean locally, `arduino-cli compile --fqbn
esp32:esp32:esp32`; +144B flash / +24B RAM vs fw6; `build-fw6-rollback/`
preserved) but NOT flashed or measured on hardware:**
- `pollLidar()`: liveness classification fix above (the one change here
  that's confidently correct regardless of the flicker mechanism).
- `tickLidarReinit()`: skips a reinit attempt entirely if an LED frame is
  currently pending (never overlaps the two in one `loop()` iteration);
  per-channel exponential backoff (5s → 10s → 20s → capped 40s) instead of
  a flat 5s retry forever for a channel that keeps failing; new downstream
  `TYPE_LIDAR_HEAL` (0x25, u8 on/off, default ON) so the Pi can pause
  self-heal entirely — the explicit fallback the task brief asked for if
  throttling alone isn't enough. Parsed Pi-side in
  `pi/neuralgrid/codec_lidar.py:lidar_heal()` /
  `pi/neuralgrid/link_lidar.py:LidarGridLink.send_lidar_heal()`
  (`pi/test_codec_lidar.py::TestLidarHeal` covers the frame shape).
- `FW_VER` 6 → 7.

**Do not report the flicker itself as fixed.** Next session, with Pi
network access: run `led_idle_probe.py` against the CURRENTLY RUNNING
firmware first (before flashing fw7) to get a real before-measurement,
check points 2-3 above, then flash fw7 (stop whichever service owns the
port → `build-fw6-rollback/` is the rollback → flash per the README's
"Flash from the Pi" → verify HELLO `fw=7` → restart the service exactly as
documented) and re-run the probe for the after-measurement.
