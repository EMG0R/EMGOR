# smartGOR — Unified Smart Room Master Plan

**Supersedes:** the "Room Installation — Master System Plan" and the "Autonomous Plant Watering & Bio-Data System" design spec. Where they conflicted, this doc is canonical.

**Conflict resolutions baked in:**
- Plant watering = the **8-zone all-I2C design** (ESP32 Feather V2 + TCA9548A mux + 8x STEMMA soil sensors + KRIDA I2C relay board), not the older 3-plant analog version. 5 plants active now, 3 spare zones wired for growth.
- No WiFi smart bulbs anywhere — lighting is BLE lamps (stock firmware) + custom addressable LED (FCOB strip + NeoPixel jars).
- The new **BTF FCOB WS2812B strip is a first-class subsystem** and the first lighting build, since it's already in hand.

**Hardware in hand:** Raspberry Pi 4 (hub), portable travel router (network core), BTF-LIGHTING FCOB COB WS2812B strip — DC5V, 16.4ft/5m, 800 LEDs, 5mm wide, IP30, individually addressable, no PSU or controller included.

**Status (locked in as of 2026-08-02):**
- ✅ **FCOB strip v1 LIVE** — flashed, working, locked as the default version (see §2). Placement: right side of the room, possibly wrapping some of the front.
- ✅ **Reactive audio architecture decided** — one mic on the Pi, no per-jar mics (see §4a).
- 🔨 **Web dashboard scaffolding in progress** at `______2026NEW/smartGOR/dashboard/` (see §6).
- ⏭️ **Next milestone: Pi + network bring-up** — ESP32s get locked onto the IoT SSID after that.

---

## 1. System Architecture

```
                          ┌────────────────────────────────┐
                          │   Raspberry Pi 4 (little server) │
                          │  Mosquitto MQTT broker          │
                          │  FastAPI backend + websockets   │
                          │  Web dashboard (kiosk-able)     │
                          │  Tailscale (remote access)      │
                          │  BLE hub · router control        │
                          │  DNS filtering (opt-in, later)   │
                          └───────────────┬────────────────┘
                                          │ WiFi now → wired ethernet later
                                          │ IoT SSID / static lease (see §6.1)
      ┌──────────────┬────────────────────┼──────────────────┬─────────────────┐
      │              │                    │                  │                 │
┌─────▼──────┐ ┌─────▼───────┐   ┌────────▼───────┐  ┌───────▼───────┐ ┌───────▼────────┐
│ ESP32 BLE  │ │ ESP32 strip │   │ ESP32 jar #1   │  │ ESP32 jars    │ │ ESP32 Feather  │
│ bridge     │ │ controller  │   │ (mesh bridge)  │  │ #2, #3 …      │ │ V2 plant hub   │
│ → 2x       │ │ → FCOB 5m   │   │ mic + NeoPixel │  │ mic + NeoPixel│ │ I2C mux + 8x   │
│ Miortior   │ │ 800-LED     │   │ WiFi+ESP-NOW   │  │ ESP-NOW only  │ │ soil sensors,  │
│ lamps (BLE)│ │ strip       │   └───────┬────────┘  └───────▲───────┘ │ 8-ch relay →   │
└────────────┘ └─────▲───────┘           │    ESP-NOW mesh   │         │ valves + pump, │
                     │                    └───────────────────┘         │ tank sensor    │
                     └── optionally also on the ESP-NOW mesh           └────────────────┘
                         for beat-sync with the jars
```

Two control planes, on purpose:
- **MQTT over WiFi** — slow-path: scenes, palettes, schedules, sensor telemetry, dashboard. Everything hub-level speaks it.
- **ESP-NOW mesh** — fast-path (~1–2 ms): beat pulses and palette sync between reactive lighting nodes (jars, and optionally the strip controller). One jar doubles as the ESP-NOW↔MQTT bridge.

*(Diagram note: the "mic + NeoPixel" labels on the jar nodes are superseded — jars no longer carry mics. Audio comes from a single mic on the Pi; see §4a.)*

---

## 2. Subsystem A — FCOB Strip (v1 LOCKED — live on the wall)

### v1 as flashed (the current default version)
`______2026NEW/smartGOR/fcob-test/fcob-test.ino` on a DIYmalls ESP32 DevKit V1 clone is the production firmware for now:
- All **800 px active**, purple-drift palette (violet→magenta hue band with slow breathing shimmer)
- `BRIGHTNESS 128` requested, governed by **FastLED power limiter capped at 5V / 900 mA** — this cap, not the brightness value, is what makes USB power safe
- Data on **GPIO 13 (D13)**; strip powered from the board's **VIN pin off a USB supply** (use a 2A charger)
- Wiring: red→VIN, white→GND, green(data)→D13, into the **female/DIN end** (arrows point away from input)

### Explicitly deferred until Pi + network are up
- Dedicated 5V 15–20A PSU and both-end power injection
- 74AHCT125 level shifter on the data line
- WLED migration (and with it MQTT control of the strip)

Everything below this line in §2 is the reference design for that later upgrade — nothing in it blocks v1.

### What this strip actually is
FCOB = flip-chip COB: a continuous phosphor-free dense LED tape, WS2812B-protocol, individually addressable, 5V, 160 LEDs/m. It looks like a smooth uninterrupted light bar (no visible dots) — great for cove/desk/shelf edge lighting.

**Important caveat to verify on arrival:** on BTF FCOB addressable strips, one IC often drives a *group* of chips, so "800 LEDs" may enumerate as fewer addressable pixels (e.g. 8 or 16 chips per IC). Run a pixel-count test sketch first (light pixel 0..N until it stops) and use *that* number in firmware. It changes nothing about power, only your array length.

### Power budget (the part that bites people)
- Planning figure for dense 5V COB: **~2.5–3.5 A per meter at full white, full brightness** → 5 m worst case ≈ **13–18 A at 5V (65–90 W)**.
- You will never run full-white/full-bright in a room install. Practical budget at ≤50% brightness with colorful (non-white) scenes: **5–8 A typical**.
- **PSU: buy a 5V 20A (100W) supply** (Mean Well LRS-100-5 class, ~$20). Headroom = no sag, no heat, no flicker. Do not try to grow into this later — 5V/high-amp is the one place undersizing hurts immediately.
- **Firmware hard cap:** set FastLED `setMaxPowerInVoltsAndMilliamps(5, 15000)` (or lower) so no bug can exceed the PSU.

### Power injection
5V + dense strip = voltage drop is real. For this 5m run:
- Inject at **the start AND the far end** minimum; add a mid-point injection if you see pink-ish white at the middle at high brightness.
- Run 16–18 AWG feeder wire from the PSU to each injection point. Signal is still one wire from the controller at the start.
- Common ground everywhere: PSU GND ↔ strip GND ↔ ESP32 GND.

### The 3.3V question (answered honestly)
**Do not power the strip from the ESP32's 3.3V rail — not "dim," it will brown out or damage the board.** The 3.3V regulator on a dev board supplies a few hundred mA; even a handful of COB segments exceeds that, and 3.3V is also below the strip's operating voltage, so color/behavior gets flaky before current limits even matter. For a desk *test* of a short cut segment (~0.5 m) you can feed the strip 5V from the ESP32's **VIN/USB 5V pin** at very low brightness (`setBrightness(20)`, current-limit set), but the real install runs from the dedicated 5V PSU. Logic power for the ESP32 can then come from the same PSU's 5V into VIN.

### Data line & level shifting
- ESP32 outputs 3.3V data; WS2812B wants ~0.7×VDD = 3.5V minimum at 5V supply. It *often* works raw, but it's marginal — use a **74AHCT125** level shifter (same part already planned for the jars). ~$1.50, ends a whole class of ghost-flicker bugs.
- Keep the wire from level shifter to strip data-in short (<30 cm ideal); add a **300–500 Ω resistor** in series at the strip's data-in and a **1000 µF cap** across 5V/GND at the first injection point.

### Which end receives data (male vs female connector)
Addressable strips are **directional** — data flows one way only. The convention BTF (and nearly everyone) uses: **the female 3-pin JST-SM connector end is the INPUT (data-in)**, the male end is the output for chaining the next strip. So yes — your instinct is right: **feed data into the female side**. Don't trust convention alone though: confirm with the **arrows printed on the strip** — they point in the direction of data flow, away from the input end. If the arrows point away from an end, that end is the input.

### Controller & firmware
- Any ESP32 dev board works; a plain ESP32-WROOM DevKit is fine ($6–8). Give the strip its own dedicated controller — don't share with the plant hub.
- Firmware: **WLED** is the pragmatic choice for v1 — flash it, get instant effects, segments, and a native MQTT client that plugs straight into Mosquitto (`lights/strip/...` topics), plus a polished web UI for free. Write custom FastLED firmware later only if you want mic-reactive or ESP-NOW beat-sync on the strip itself (WLED also has Sound Reactive builds and UDP sync — evaluate before writing anything custom).
- Placement in the mesh: v1 = MQTT-only (scene control from the Pi). v2 = join the ESP-NOW/UDP sync group so it pulses with the jars.

### FCOB shopping list (beyond the strip you own)
| Part | Price |
|---|---|
| 5V 20A PSU (Mean Well LRS-100-5 or equiv.) | $20 |
| ESP32 DevKit (strip controller) | $7 |
| 74AHCT125 level shifter | $2 |
| 1000 µF cap, 330 Ω resistor, JST-SM pigtails | $6 |
| 16–18 AWG wire for injection runs, ferrules/WAGOs | $10 |
| Inline fuse holder + 15A fuse on PSU output (cheap insurance) | $5 |
| **Subtotal** | **≈ $50** |

---

## 3. Subsystem B — Miortior BLE Lamps

Unchanged from the prior plan; protocol already identified.
- ELK-BLEDOM family: GATT service `FFF0`, characteristic `FFF3`; color/brightness/on-off/effects commands documented.
- One ESP32 (NimBLE or ESPHome `ble_client`) connects to both lamp MACs sequentially, translates MQTT (`lights/lamp1/set`, `lights/lamp2/set`, JSON payloads) into BLE writes.
- Music-reactive command still unconfirmed → check `dave-code-ruiz/elkbledom` effects tables first, sniff with the phone app as fallback.
- **Reactive plan (locked):** v1 just puts both lamps into their **built-in dynamic/music-reactive mode** via a BLE mode command from the bridge — no custom reactive streaming. **Caveat:** if their "music mode" turns out to be phone-app-mic-driven (app streams colors over BLE) rather than an onboard mic, the fallback is the bridge streaming ~10–20 Hz color updates derived from the Pi's beat pipeline (§4a) — same data source as everything else, just delivered over BLE.
- **Note:** BLE + WiFi coexist on one ESP32 but share a radio — if the bridge gets flaky, this node is the one candidate for an ESP32 with external antenna, or keep its MQTT traffic minimal. It does *not* join the ESP-NOW mesh.
- **Alternative — drive the lamps from the Pi directly:** the Pi 4's onboard BLE can talk to these lamps itself (BlueZ + `bleak`), removing this ESP32 entirely. That's the **recommended starting point** now that the Pi is framed as a little server — see **§6.1 role 2** for the trade-off (Pi-direct vs dedicated bridge) and the same BLE↔WiFi radio-contention fallback (ESP32 bridge or USB BT dongle). This §3 bridge remains the documented fallback if Pi-direct BLE range/reliability disappoints.

---

## 4a. Reactive Audio Pipeline (LOCKED — centralized mic on the Pi)

**Decision: one mic, on the Pi. No mic on any ESP32 rig.** Per-jar mics are removed from the jar BOM and firmware plan.

Canonical reactive path:

```
USB mic → Pi 4 (ALSA small-buffer capture, ~256 samples ≈ 6 ms)
        → beat/energy/FFT detection in Python (aubio / numpy)
        → compact UDP packet over WiFi (NOT MQTT — broker hop adds jitter)
        → bridge ESP32
        → ESP-NOW broadcast → every LED node (jars, strip, …)
```

- **Expected end-to-end latency: ~20–50 ms** — perceptually tight for light-to-beat (lag reads as "off" only around ~100 ms).
- Budget roughly: capture 6–12 ms + detection a few ms + WiFi UDP 1–5 ms + ESP-NOW 1–2 ms + LED frame write.
- **MQTT keeps the slow path only** (scenes, palettes, telemetry); the beat stream never touches the broker.
- **Requires the fixed 2.4 GHz channel on the travel router** (§7) — ESP-NOW and the UDP hop both live or die by it.
- Upside beyond convenience: one mic = one consistent audio truth, so every fixture reacts to the *same* beat instant instead of N mics disagreeing.

---

## 4. Subsystem C — NeoPixel Jars (reactive mesh)

Per jar: ESP32-WROOM + WS2812B/SK6812 cluster + 5V supply + 74AHCT125 from day one. **No mic per jar** — reactive data arrives over ESP-NOW from the §4a pipeline.

- **Reactive layer:** receive beat/energy packets via ESP-NOW → drive pattern locally.
- **Mesh layer:** ESP-NOW broadcast of beat pulses / palette / scene ID between jars (and later the FCOB strip controller).
- **Bridge:** one jar (or a dedicated cheap ESP32) is the ESP-NOW↔MQTT/UDP gateway so the Pi can push both scenes and the beat stream to the whole mesh.
- Power sizing rule for any future long runs: ~20 mA/pixel planning at 1/3-brightness white; inject every 5 m.
- MQTT: `lights/jars/scene`, `lights/jars/palette`, `lights/jars/status`.

Build these **last** — most involved (audio + mesh + enclosures), and everything they need (broker, scene model, ESP-NOW channel discipline) will already exist.

---

## 5. Subsystem D — Plant Watering + Bio-Data (canonical 8-zone I2C design)

Full detail lives in the original watering spec; summary of the locked-in design:

- **ESP32 Feather V2** (STEMMA QT) → **TCA9548A 8-ch I2C mux** → 8x **Adafruit STEMMA capacitive soil sensors** (all at default 0x36, isolated per mux channel — no jumper soldering).
- Same I2C bus → **KRIDA 8-ch I2C relay board** (0x27/0x3F, no conflict with mux at 0x70) → 8x 12V NC solenoid valves + one relay for the pump. 12V 3A supply for the muscle side.
- Tank: float switch on a GPIO as **hard low-water cutoff**, optional JSN-SR04T ultrasonic for a %-full dashboard number (mind its 20–25 cm blind zone in a small tank).
- **Logic lives on the Pi**, not the ESP32: ESP32 publishes `plants/zone{N}/moisture` every 15–30 min; Pi holds per-species thresholds and issues `plants/zone{N}/water` commands with short durations (2–3 s bursts). **Queue rule: one valve open at a time, pump never on with zero valves open.** Moisture sensor is the feedback signal — no precision dosing needed.
- Zones 6–8 wired but excluded from rotation until planted.
- **Bio-data:** galvanic plant-tissue signal on 1–2 "hero" plants via spare ESP32 ADC pins → `plants/zone{N}/bio_signal` → free-floating art/data stream for the dashboard (and a fun future modulation source for the lighting scenes — a plant driving the FCOB palette is exactly the kind of thing this architecture makes trivial).
- Validation: dry rapid-cycle test (every valve 1 s, back to back) before water ever enters the lines.

---

## 6. Pi 4 Hub — the room's "little server"

The Pi 4 is not just the MQTT broker; treat it as **a small always-on home server** that grows into several roles over time. Its job is to be the room's brain: message broker, dashboard, optional Bluetooth radio, router controller, and (later) DNS/filtering and remote-access endpoint. Everything the installation *depends* on lives here, so the guiding principles are **bring roles up incrementally** (never all at once), **give it a stable identity on the network** (static IP, eventually wired), and **keep an eye on its finite budget** — one Pi 4 has 4 cores, 4/8 GB RAM, a shared USB 2.0 bus, and a single 2.4/5 GHz radio + onboard BLE that share silicon. Roles compete; the notes below flag where.

### 6.0 Core stack (running from day one)

| Layer | Choice | Notes |
|---|---|---|
| OS | Raspberry Pi OS Lite 64-bit | Headless; SSH from day one |
| Broker | Mosquitto | Username/password auth even on the IoT VLAN; listener on LAN + Tailscale interface |
| Backend | FastAPI + `paho-mqtt` / `aiomqtt` | Subscribes to everything, REST + websocket out, holds scene presets and plant thresholds (YAML/SQLite) |
| Dashboard | Plain HTML/JS or light React, served by the backend — **scaffolding in progress at `______2026NEW/smartGOR/dashboard/`** | Cards: lamps, strip, jars, 8 plant zones + tank, bio-signal graph; scene preset buttons that fan out across all lighting subsystems ("chill" → lamps + strip + jars together); smart-home toggles. Built now, deployed to the Pi once it's up |
| Remote | Tailscale | Dashboard + MQTT reachable from anywhere, zero port-forwarding — pairs perfectly with a travel router that may sit behind CGNAT/hotel-NAT |
| Extras | `mosquitto_sub -t '#' -v` as your debug console; optional InfluxDB/Grafana later for moisture history | Don't build logging infra before the system produces data |

### 6.1 Roles

**1. MQTT broker + dashboard backend** *(existing — the foundation)*
*What:* Mosquitto plus the FastAPI dashboard at `______2026NEW/smartGOR/dashboard/`.
*Why:* the message bus and control surface every subsystem already assumes.
*Software:* Mosquitto, FastAPI + `aiomqtt`. *Caveat:* both are light; this is the baseline load the other roles get layered on top of. Frame everything below as *additional* services sharing the same box.

**2. Bluetooth hub (Pi-direct BLE)**
*What:* the Pi 4 has onboard BLE and can drive the Miortior/ELK-BLEDOM lamps **directly** from Python — no separate ESP32 in the path.
*Why:* fewer devices, one brain; the lamp-control logic lives next to the scene-fanout logic instead of hopping MQTT→ESP32→BLE.
*Software:* BlueZ (the Linux BT stack) + `bleak` (async Python BLE) — connect to the two lamp MACs, write the `FFF0/FFF3` GATT commands from §3 directly. A small asyncio task inside the FastAPI backend subscribes to `lights/lamp{1,2}/set` and writes BLE.
*Trade-off (decide per-room):*
- **Pi-direct BLE** — one fewer node to build/flash/power, tighter integration, but the Pi's single radio now juggles BLE **and** Wi-Fi, and it must stay in BLE range of the lamps.
- **Dedicated ESP32 BLE bridge (§3)** — offloads the radio work, keeps the Pi free, and lets the bridge sit physically near the lamps; costs one ESP32 and a firmware.
*Recommendation:* **start Pi-direct** — it's the fastest path and removes a node. Keep the §3 ESP32 bridge as the documented fallback. *Caveat:* onboard BT and heavy 2.4 GHz Wi-Fi **contend on the same silicon** — if BLE range or reliability suffers once the Wi-Fi/ESP-NOW traffic is busy, fall back to the ESP32 bridge, or add a **USB Bluetooth dongle** to give BLE its own radio (also frees the onboard radio for Wi-Fi). This mirrors the same BLE↔Wi-Fi coexistence caveat already noted for the ESP32 bridge in §3.

**3. Connectivity: wireless-first, ethernet-later**
*What:* how the Pi attaches to the network core (the GL.iNet travel router).
*Why:* it starts as just another client but becomes the thing everything depends on, so stability matters more over time.
- **Phase 1 — Wi-Fi:** join SSID `YOUR_WIFI_SSID` (2.4/5 GHz). Fastest to stand up; fine for bring-up and light traffic.
- **Phase 2 — wired ethernet into the router:** once the Pi is *the broker/DNS/everything depends on it*, move it to a **wired link** into the GL.iNet router for stability and to take one heavy client off the 2.4 GHz air (which the ESP-NOW mesh also needs — see §4a/§7). Gigabit-to-the-router removes Wi-Fi as a failure mode for the room's core.
*Either way:* give the Pi a **static IP via DHCP reservation** (`smartgor-hub` → `192.168.8.10`, ROUTER_RUNBOOK.md §2) so its address never moves, over Wi-Fi or wire. *Note:* once the Pi is **ethernetted to the router, the Pi + router function as one control unit** — brain (Pi) bolted to network muscle (router), see role 4.

**4. Network manager / router control**
*What:* the Pi reads and edits the GL.iNet router's configuration programmatically.
*Why:* this is what lets a future dashboard "network panel" actually control the network — change the Wi-Fi channel, manage static DHCP leases, toggle the guest network, drive the VPN.
*Software:* the router's **JSON-RPC API (`POST /192.168.8.1/rpc`)** or **SSH + `uci`** — both fully documented in **ROUTER_RUNBOOK.md §6** (hashed-login flow, reference Python, and the uci snippets for channel/DHCP/isolation). Use `uci`/SSH for deterministic config changes; use `/rpc` to mirror the GUI and read GL-specific status (client list, VPN state).
*Framing:* the **Pi + router are one functional object** — Pi = brain, router = network muscle. The Pi doesn't just live on the network, it *operates* it. *Caveat:* both control paths are **root** on the only network core — a bad `uci commit` + reload can strand the room (ROUTER_RUNBOOK.md safety checklist). Back up first, one change at a time, and never hard-code the admin password/PSK in a committed script.

**5. Content / traffic filtering** *(future, opt-in — "like a little server")*
*What:* network-wide DNS filtering so every device in the room/house blocks ads, trackers, and unwanted domains.
*Why:* the option to run the Pi as a real filtering server, not just a broker.
*Software:*
- **Primary: Pi-hole** — network-wide DNS ad/tracker blocking with its own admin UI and blocklists.
- **Companions/alternatives:** **Unbound** (local recursive DNS resolver behind Pi-hole, so you're not trusting an upstream), **NextDNS** (hosted filtering if you'd rather not self-host the lists), or plain **firewall/traffic rules** on the router for coarser control.
*Topology:* point the **router's DHCP-advertised DNS at the Pi** (GL.iNet: LAN/DHCP settings, or `uci` on `dhcp` — see ROUTER_RUNBOOK.md §2/§6) so every device filters through the Pi automatically. *Caveat:* this makes the **Pi a hard network dependency** — if the Pi is down, DNS for the whole room dies. Set a **secondary DNS** (e.g. the router itself or `1.1.1.1`) in the DHCP options so name resolution **fails over** gracefully when the Pi is rebooting or offline. (Note the tension with role 6/§4a: heavy filtering traffic is one more thing sharing the Pi's CPU and network — cheap at room scale, worth watching if it also feeds a whole house.)

**6. VPN / remote access endpoint**
*What:* reach the dashboard, MQTT, and the router from anywhere.
*Software:* **Tailscale on the Pi** (already in the core stack). Optionally `--advertise-routes=192.168.8.0/24` so you also reach the router GUI and ESP32s that can't run Tailscale themselves.
*Why here:* this is the **recommended remote-access path for a CGNAT travel router** — Tailscale's outbound NAT traversal survives hotel/cellular uplinks where a self-hosted WireGuard *server* on the router cannot. Full reasoning, the CGNAT gotcha, and the setup commands are in **ROUTER_RUNBOOK.md §5**.

**7. "Little server" extras** *(future / optional — room to grow, not a spec)*
A menu the Pi can absorb as needs appear — bring these up only when there's a reason:
- **Reverse proxy** (Caddy or nginx) — clean local hostnames + automatic TLS in front of the dashboard and any other web UIs (Pi-hole, Grafana).
- **Docker / containers** — add services cleanly and isolated instead of piling packages onto the host; `docker-compose` doubles as the service manifest.
- **Monitoring / uptime** — a lightweight uptime/health check (e.g. Uptime Kuma, or a cron + `mosquitto_pub` heartbeat) so you know when a role or node dies.
- **File / log storage** — persist plant bio-data history (§5) and telemetry for later graphing (InfluxDB/Grafana when there's data worth graphing).
- **Scheduled jobs (cron)** — time-based automations (watering windows, nightly scene changes, config backups of the router per ROUTER_RUNBOOK.md §7).

### 6.2 Service management & incremental bring-up

Run each role as its own managed service so they start on boot and restart on failure — **systemd units** for host-installed services (Mosquitto, the FastAPI backend, a BLE task, Pi-hole, Tailscale all ship or register units), or **docker-compose** if you go the container route (role 7) so the compose file *is* the service manifest.

**Bring roles up one at a time, not all at once** — a Pi 4 has finite CPU/RAM and one shared radio + USB 2.0 bus, and each role you add is another thing competing for them and another way to break the room's core. Order of introduction: **OS + static networking** → **Mosquitto + dashboard** (the foundation, role 1) → then layer on **BLE (role 2)**, **router control (role 4)**, **filtering (role 5)**, and the **extras (role 7)** as you actually want them, confirming the room still behaves after each addition.

Optional: old phone/tablet on the wall in kiosk mode as the physical control surface.

---

## 7. Network Plan (portable travel router as core)

The travel router is actually a good fit — small, dedicated, and the whole installation becomes a **portable module**: the room can move (dorm/apartment/venue) and every static lease, hostname, and ESP-NOW channel setting moves with it.

1. **Dedicated IoT SSID** on the travel router; personal devices stay on the guest/other SSID or the upstream network. The router's WAN uplink (hotspot, ethernet, whatever) doesn't matter to the installation — everything internal keeps working with no internet at all.
2. **Static DHCP leases** for the Pi and every ESP32. mDNS (`raspberrypi.local` etc.) in configs, never hardcoded IPs — but keep the lease table as the source of truth since some ESP32 mDNS stacks are flaky.
3. **Fix the 2.4 GHz channel manually** (1, 6, or 11 — pick the cleanest). This is load-bearing: **ESP-NOW nodes must share the WiFi channel**, and if the router auto-hops, the mesh silently dies. Set it once, write it down, put the same channel constant in every ESP-NOW firmware.
4. Check the router supports **≥10 concurrent clients** comfortably (Pi + up to ~6 ESP32s + your laptop + kiosk tablet). Most GL.iNet-class travel routers do; the cheapest ones choke around 8.
5. **Tailscale on the Pi** = remote access solved regardless of what NAT the travel router is behind. Optionally also on your laptop/phone so the dashboard works away from home.
6. Travel-router caveat: if its WAN side changes networks often, disable any "reboot on WAN change" behavior — the room shouldn't blink because the uplink moved.
7. **Wire the Pi to the router once it's load-bearing.** The Pi starts on Wi-Fi but should move to a **wired ethernet link** into the router once it's the broker/DNS everything depends on (§6.1 role 3) — more stable, and it frees a heavy client off the 2.4 GHz air the ESP-NOW mesh shares. Wired or wireless, keep its **static DHCP reservation** (ROUTER_RUNBOOK.md §2). Ethernetted, the Pi + router act as **one control unit** — and the Pi can drive the router's config directly (§6.1 role 4 / ROUTER_RUNBOOK.md §6).
8. **Optional DNS filtering through the Pi** (§6.1 role 5): point the router's DHCP-advertised DNS at the Pi (Pi-hole) so every device filters through it — but set a **secondary DNS** so name resolution fails over if the Pi is down. Don't turn this on until the Pi bring-up is solid.

For the exact GL.iNet click-paths, `uci`/`/rpc` commands, safety checklist, and VPN setup behind all of the above, see **ROUTER_RUNBOOK.md** in this folder.

---

## 8. MQTT Topic Map (consolidated)

```
lights/lamp1/set            lights/lamp2/set            # JSON: color/brightness/effect
lights/lamp1/state          lights/lamp2/state
lights/strip/set            lights/strip/state          # WLED MQTT or custom
lights/jars/scene           lights/jars/palette         lights/jars/status
scenes/activate                                          # Pi fans out to all lighting
plants/zone{1-8}/moisture   plants/zone{N}/water        # cmd from Pi, short duration
plants/zone{N}/pump_status  plants/tank/level
plants/zone{N}/bio_signal
sys/{node}/status                                        # LWT online/offline per ESP32
```

Every ESP32 sets an MQTT **Last Will** on `sys/{node}/status` so the dashboard shows dead nodes honestly.

---

## 9. Consolidated BOM (beyond what's in hand)

| Block | Contents | Est. |
|---|---|---|
| FCOB strip support | PSU 20A, ESP32, shifter, wiring, fuse (§2) | $50 |
| BLE lamp bridge | 1x ESP32 DevKit | $7 |
| Plant electronics | Feather V2, 8x STEMMA sensors, TCA9548A, KRIDA relay, STEMMA cables, 12V 3A PSU | $180 |
| Plant plumbing (recommended tier) | Premium valves + budget pump + float switch + ultrasonic + drip kit + check valves ("Option B lite") | $200–230 |
| Jars (per jar) | ESP32, LEDs, mic, shifter, 5V supply | ~$20–30/jar |
| Hub | Pi 4 + travel router | owned |
| **Realistic total (before jars)** | | **≈ $440–470** |

Spend order matches build order — you can start tonight for ~$50 (strip block) since everything else about the strip is already on your desk except the PSU.

---

## 10. Build Order

1. ✅ **FCOB strip v1** — DONE. Flashed, purple drift, USB-powered with firmware current cap, mounted right side of room. (PSU/injection/shifter/WLED upgrade deferred — see §2.)
2. ⏭️ **Pi hub + network bring-up** (NEXT — user milestone): travel router IoT SSID, fixed 2.4 GHz channel, static leases; Pi joins on **Wi-Fi first** with a static DHCP reservation, running **Mosquitto + Tailscale + `mosquitto_sub`** debugging (core stack, §6.0). ESP32s get locked onto the network after this. Bring the Pi's extra roles up **incrementally, not all at once** (§6.2): foundation now → then layer on Pi-direct BLE (role 2), router control (role 4), DNS filtering (role 5), and "little server" extras (role 7) as wanted. Move the Pi to a **wired ethernet uplink** once it's the thing everything depends on (§6.1 role 3 / §7).
3. **Dashboard deploy**: scaffolding already being built at `smartGOR/dashboard/` — deploy to the Pi, wire to Mosquitto, first real control surface.
4. **BLE lamp bridge**: known protocol, one ESP32; put lamps in built-in reactive mode; establishes the scene-fanout pattern on the Pi.
5. **Plant watering**: self-contained; dry-cycle validation before water; can proceed in parallel with 4.
6. **Jars + ESP-NOW mesh + Pi mic pipeline (§4a)**: the deep build — beat pipeline, mesh, enclosures; strip controller joins the sync group here (this is also the natural moment for the strip's PSU/WLED upgrade).
7. **Network hardening pass**: static leases audit, fixed channel confirmed, Mosquitto auth, dashboard kiosk. Light version during step 2, full pass here.
8. **Future: room LED mapping** (§12) — after jars, when multiple fixtures exist to map.

---

## 11. Open Questions

- **FCOB addressable pixel count** — 800 chips, but how many ICs? Run the pixel-count sketch on arrival; update firmware array length and effect density accordingly.
- **Strip data-in confirmation** — verify female-end-is-input against the printed direction arrows on your specific reel before soldering anything.
- Miortior music-reactive BLE command (elkbledom lookup → sniff fallback).
- Exact jar count + total pixel run → jar PSU sizing.
- Travel router model's concurrent-client ceiling and whether its DHCP reservation + manual-channel settings persist across power cycles (test early).
- Final plant species list → per-zone moisture thresholds on the Pi.
- Whether WLED's UDP sync covers the jar use case well enough to skip custom jar firmware entirely (evaluate against the §4a Pi-mic pipeline before writing custom code; per-jar Sound Reactive builds are moot now that jars carry no mics).

---

## 12. Future: Room LED Mapping (placeholder — explicitly not now)

Future phase, after the jars exist: give **every pixel across all fixtures** (FCOB strip, jars, future strips) real room coordinates / a normalized ring position, so effects are authored against **room-space** (ring position, x-y) instead of per-strip indices. An effect can then travel smoothly in a ring around the room regardless of physical strip order, direction, gaps, or which controller owns which pixels — and different vibes/scenes get mapped onto that geometry.

- Implementation candidates: **WLED 2D/segment mapping**, or the standard **camera-assisted auto-map** — blink each pixel in sequence while a phone/Pi camera records where it lit, emitting the coordinate table automatically.
- The mapping layer lives **Pi-side** (room-space frame rendered centrally, or indices translated before dispatch) so per-node firmware stays dumb.

Slot in build order: after the jars phase (§10 step 8).

---

## Addendum — 2026-08-09: 3-node ring, multi-node lightserver, ring engine

*(Appended addendum — nothing above this line was changed.)*

- **LED fleet is final at 3 nodes** and now forms a **closed physical ring**,
  1920 px total: fcob-strip (800 px, 5 m) → fcob-strip2 (480 px, 3 m, MAC
  20:50:0d:19:e2:88) → fcob-strip3 (640 px, 4 m, MAC 68:09:47:4c:f1:b4) → back
  to fcob-strip. Full joint map, index map, and hardware table:
  **`RING_TOPOLOGY.md`** (canonical). This partially realizes §12 (room LED
  mapping) ahead of schedule, in 1-D ring form.
- **Lightserver** (Pi, `gorcave.local:8800`) went multi-node with persistence
  (`nodes.json` registry + `state.json`), and a **RingEngine** renders one
  1920 px canvas sliced per node. `/prompt` and `/scene` are **ring-wide
  only**; per-node ESP32 endpoints are debug-only.
- **New default scene: `ring-orbit`** — dark-purple↔violet breathing background
  plus a 10-LED soft-edged orb orbiting the full ring every 15 s.
- **Flash rules (learned/locked):** disconnect the strip's wires from the ESP32
  during any USB flash; address nodes by **mDNS names** (`fcob-stripN.local`),
  never by IP — DHCP leases float (an earlier board, MAC `e4:0c…`, is retired).
- Website chat agent ("Server") drives lights only through the abstract
  `<lights>vibe</lights>` → lightserver `/prompt` hook — vibe text only, no
  node addressing (audited 2026-08-09).
