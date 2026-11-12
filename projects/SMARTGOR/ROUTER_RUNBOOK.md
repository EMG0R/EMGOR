# Travel Router Runbook — GL.iNet (OpenWrt / GL GUI)

Detailed how-to for hardening and operating the smart-room network on the GL.iNet
travel router. This is the hands-on companion to the network section of
`SMART_ROOM_MASTER_PLAN.md` (same folder) — the master plan says *what* the
topology should be; this doc says *exactly which buttons and commands* get you there.

**Device facts (do not re-probe — these are known):**

| Thing | Value |
|---|---|
| Router LAN IP | `192.168.8.1` |
| Admin panel | `http://192.168.8.1` (GL GUI on OpenWrt, firmware build ~2024-06-06) |
| JSON-RPC API | `POST http://192.168.8.1/rpc` (user `root`, GL.iNet hashed login) |
| SSH | port `22`, `root` shell |
| HTTPS | port `443` |
| WiFi SSID | `YOUR_WIFI_SSID` |
| Band / security | 2.4 GHz, WPA2-PSK, PSK `CHANGE_ME_PASSWORD` |
| Admin password | known to the user (not stored here) |

**Smart-room devices that live on this network:**
- Raspberry Pi 4 hub — Mosquitto MQTT broker + FastAPI/web dashboard (also the audio/beat source).
- ESP32 nodes — 2.4 GHz-only (e.g. `smartgor-strip1` / `fcob-strip.local`), plus jar nodes that run an ESP-NOW mesh.

---

## ⚠️ Safety checklist — do this BEFORE changing anything

1. **Back up the config first.** GUI: **System → Upgrade → Backup** (or **System → Advanced (LuCI) → Backup/Flash**). CLI backup:
   ```bash
   ssh root@192.168.8.1 "sysupgrade -b /tmp/backup-$(date +%F).tar.gz"
   scp root@192.168.8.1:/tmp/backup-*.tar.gz ~/router-backups/
   ```
   Keep a copy of `/etc/config/wireless`, `/etc/config/dhcp`, and `/etc/config/network` specifically — those are the three files this runbook touches.

2. **Make WiFi-radio changes from a wired device or a second device on a different link.** Channel changes, AP-isolation toggles, and SSID edits restart the radio and *will drop the client that issued them*. If you change the radio from the same laptop you're connected on, you can lock yourself out mid-change. Use the router's Ethernet/USB-Ethernet port, or drive it from the Pi over SSH while you sit on a phone hotspot.

3. **Expect brief reconnects.** Any `wifi reload`, DHCP restart, or firmware step will bounce every client for a few seconds. The ESP32s will re-associate on their own, but the ESP-NOW mesh and MQTT sessions will blip — don't do this mid-show.

4. **One change at a time.** Apply, confirm devices came back (check the GL client list), *then* make the next change. Batching radio + DHCP + firewall edits makes a lockout impossible to diagnose.

5. **Know your rollback.** If a change strands you: power-cycle usually reverts an un-committed LuCI change; a bad committed change is recovered by restoring the backup tarball, or worst case GL.iNet's **UBoot failsafe** (hold Reset during boot) to reflash.

---

## 1. Fixed 2.4 GHz channel lock

**Why it matters:** ESP-NOW and the fast-path UDP beat hop (master plan §4a/§7) live or die by a stable 2.4 GHz channel. On **Auto**, OpenWrt can re-pick a channel after a reboot or interference event; ESP-NOW peers that were locked to the old channel go deaf. Pin it.

**Pick a clear channel first.** Use only **1, 6, or 11** (the non-overlapping 2.4 GHz channels). Survey the air and choose the least-crowded of the three:
```bash
# From the Pi or over SSH on the router:
iwinfo wlan0 scan | grep -E "Channel|ESSID|Signal"
# or on the Pi:
sudo iwlist wlan0 scan | grep -E "Channel|ESSID|Signal"
```
Count APs per channel; also treat channels adjacent to a strong neighbor as "used" (2.4 GHz APs bleed ~±2 channels). Fewest neighbors wins.

**GL GUI click-path:**
`http://192.168.8.1` → **Wireless** → 2.4 GHz radio → gear/settings → set **Channel** from *Auto* to your chosen **1 / 6 / 11**, keep **Bandwidth = 20 MHz** (20 MHz is friendlier to ESP32s and reduces the chance of a channel offset). **Apply**.

**CLI (uci):**
```bash
ssh root@192.168.8.1
uci set wireless.radio0.channel='6'        # radio0 is the 2.4 GHz radio on most GL units
uci set wireless.radio0.htmode='HT20'      # 20 MHz
uci set wireless.radio0.channel                # (read back to confirm)
uci commit wireless
wifi reload
```
> Confirm `radio0` is actually 2.4 GHz first: `uci show wireless | grep -E 'band|hwmode|channel'`. On dual-band units the 2.4 GHz radio is usually `radio0` with `band '2g'` / `hwmode '11g'`.

**Risk/caveat:** This restarts the radio → all clients reconnect (see safety #2). 20 MHz caps WiFi throughput, which is fine for MQTT/telemetry but note it if you later stream heavy data. If ESP-NOW *still* drops after locking, double-check the ESP32 firmware's channel matches the pinned channel — ESP-NOW peers must agree on the channel number.

---

## 2. Static DHCP reservations (fixed IPs by MAC)

**Why it matters:** The Pi's MQTT broker and dashboard need a stable address, and it's far easier to point ESP32 firmware / dashboards at `192.168.8.x` that never moves. Bind by MAC so a device gets the same lease forever.

**Find a device's MAC in the GL GUI:**
`Clients` (main page list) → each connected device shows **hostname, IP, and MAC**. Look for `smartgor-strip1`, the Pi's hostname, etc. Note the MAC (form `AA:BB:CC:DD:EE:FF`). CLI alternative — read the live lease table:
```bash
ssh root@192.168.8.1 "cat /tmp/dhcp.leases"
# columns: <expiry> <MAC> <IP> <hostname> <client-id>
```

**Suggested reservation scheme (edit to taste):**

| Device | Hostname | Reserved IP |
|---|---|---|
| Pi 4 hub | `smartgor-hub` | `192.168.8.10` |
| FCOB strip ESP32 | `smartgor-strip1` | `192.168.8.21` |
| Jar node 1 (ESP-NOW↔MQTT bridge) | `smartgor-jar1` | `192.168.8.22` |
| Jar node 2 | `smartgor-jar2` | `192.168.8.23` |

**GL GUI click-path:**
`More Settings` → **Network → LAN** (or **DHCP Server** / **Address Reservation** depending on firmware) → **Add** a binding → pick the device from the client list (or paste MAC) → enter the fixed IP → **Save/Apply**. GL.iNet also exposes this as a "reserve" pin/pushpin next to a client in the **Clients** list on newer builds.

**CLI (uci):**
```bash
ssh root@192.168.8.1
uci add dhcp host
uci set dhcp.@host[-1].name='smartgor-hub'
uci set dhcp.@host[-1].mac='AA:BB:CC:DD:EE:FF'   # the Pi's MAC
uci set dhcp.@host[-1].ip='192.168.8.10'
uci commit dhcp
/etc/init.d/dnsmasq restart
```
Repeat one `uci add dhcp host` block per device. Verify:
```bash
uci show dhcp | grep -A3 host
```

**Risk/caveat:** Keep reserved IPs **outside the dynamic DHCP pool** if possible, or at least ensure no other device currently holds them (a conflict = a device that can't get its reserved lease until the squatter's lease expires). `dnsmasq restart` briefly interrupts DHCP but does not drop already-connected clients. A reserved IP only applies after the device renews — reboot the device or wait out its lease to make it take immediately.

---

## 3. Guest / IoT segmentation — and the isolation trap

**Why it matters:** You want untrusted gear (a guest phone, a random travel-hotel device) unable to touch the smart-room hub, *without* breaking the two things that make the room work: Pi↔ESP32 MQTT and the ESP32↔ESP32 ESP-NOW mesh.

### 🚨 The critical caveat — do NOT put the ESP32s behind client isolation

GL.iNet's **Guest network** (and any AP **"client isolation"** setting) blocks *station-to-station* traffic: clients on that AP can reach the internet/gateway but **not each other**. If you drop the ESP32s and/or the Pi onto an isolated guest AP:
- **MQTT breaks** — the ESP32s can no longer reach the Pi's Mosquitto broker (they're isolated from it even though both are "on WiFi").
- **ESP-NOW is unaffected by AP isolation directly** (it's link-layer, not routed through the AP), **but** the moment you split devices across two SSIDs/subnets or two channels, the mesh and the UDP beat hop fall apart because ESP-NOW peers must share one channel and the bridge must share one L2 segment with its MQTT broker.

**Correct topology:** keep **all smart-room devices (Pi + every ESP32) on ONE mutually-reachable network with client isolation OFF**, and isolate *that whole network* from untrusted devices — not the devices from each other. Two clean ways:

- **Simple (recommended for a travel router):** run the smart-room devices on the **main SSID `YOUR_WIFI_SSID` with AP isolation OFF**, and hand any visitor a **separate Guest SSID** (isolation ON). Guests are walled off; the smart room stays flat and reachable.
- **Stricter:** put the smart-room devices on their own SSID/VLAN/subnet (an "IoT" network) with **isolation OFF within it**, and use firewall rules so the *untrusted* zone can't initiate into the IoT zone. Heavier to set up on a travel router; only worth it if you regularly share this router with people you don't trust.

Either way the invariant is the same: **smart-room devices must be able to reach each other; the wall goes between that group and outsiders, never inside it.**

**Check / toggle AP isolation:**

GUI: `Wireless` → radio settings → look for **AP Isolation** (sometimes under advanced/LuCI). Make sure it's **OFF** on the SSID the smart-room devices use. The **Guest** feature has its own isolation that is ON by default — leave that ON for the guest SSID, and simply don't join smart-room gear to it.

CLI (uci) — inspect and disable isolation on the smart-room AP:
```bash
ssh root@192.168.8.1
uci show wireless | grep -E 'isolate|ssid'
# find the iface whose ssid is 'YOUR_WIFI_SSID', e.g. wireless.default_radio0
uci set wireless.default_radio0.isolate='0'    # 0 = clients CAN see each other
uci commit wireless
wifi reload
```
To create a proper guest AP with isolation ON (stricter option), that's a new `wifi-iface` in `/etc/config/wireless` bound to a separate `guest` network/firewall zone — easiest via GUI **Network → Guest Network** on GL firmware, which wires the firewall zone for you.

**Risk/caveat:** `wifi reload` bounces clients (safety #2). Don't confuse **AP isolation** (station-to-station block, the thing that breaks MQTT) with a **firewall zone** (network-to-network policy, the thing you actually want between guests and IoT). If MQTT works over Ethernet/localhost but ESP32s can't reach the broker over WiFi, isolation being ON is the #1 suspect.

---

## 4. mDNS / `.local` hostname resolution

**Why it matters:** The dashboard and firmware refer to devices as `fcob-strip.local` / `smartgor-hub.local` instead of chasing IPs. mDNS lets that resolve on the LAN.

**How it works here:** mDNS is *peer-to-peer* — each device (the Pi via `avahi-daemon`, ESP32s via their Arduino/ESP-IDF mDNS responder) answers for its own `.local` name. The router doesn't have to do much, **but** it must not block multicast between clients — which loops back to §3: **AP isolation OFF** is required for `.local` discovery to work across WiFi clients.

**On the Pi (make sure it advertises):**
```bash
sudo apt install avahi-daemon
sudo systemctl enable --now avahi-daemon
avahi-resolve -n smartgor-hub.local     # sanity check
```

**On the router — optional, so the router itself answers `.local` / serves LAN DNS:**
GUI: recent GL/OpenWrt builds resolve DHCP hostnames on the LAN automatically (dnsmasq). To also do mDNS reflection across interfaces you'd install **avahi** via LuCI **System → Software** (`opkg update && opkg install avahi-daemon`). Usually unnecessary on a single flat LAN.

CLI — confirm dnsmasq is serving LAN names (so even non-mDNS clients resolve DHCP hostnames):
```bash
ssh root@192.168.8.1 "uci show dhcp | grep -E 'local|domain'"
# expect something like dhcp.@dnsmasq[0].local='/lan/' and .domain='lan'
```

**Risk/caveat:** mDNS (`.local`, UDP 5353 multicast) does **not** cross subnets/VLANs without a reflector — if you ever adopt the stricter split-subnet topology from §3, you must add avahi reflection or `.local` names will resolve only within each subnet. Client isolation kills `.local` the same way it kills MQTT. ESP32 mDNS responders are also flakier than the Pi's — keep static DHCP reservations (§2) as the reliable fallback addressing.

---

## 5. VPN for remote access

**Why it matters:** You want to reach the dashboard / MQTT / the router from outside the room (another building, on the road) without exposing anything to the public internet. Two good options — and one big travel-router gotcha.

### The travel-router gotcha: CGNAT
Travel routers very often sit behind **CGNAT** (carrier-grade NAT — hotel WiFi, cellular hotspots, guest uplinks). Under CGNAT you have **no public inbound IP**, so anything that requires a port to be reachable from the internet — including a self-hosted **WireGuard *Server*** — simply cannot be reached from outside. This drives the recommendation below.

### (a) GL.iNet built-in WireGuard **Server**
GUI: `VPN → WireGuard Server` → **Enable** → **Add a client** (generates a keypair) → **Export / show QR** the client config → import on phone (WireGuard app scans the QR) or `wg-quick up` a `.conf` on a laptop.
CLI: GL manages this via its own service; the underlying config lands in `/etc/config/wireguard_server` and `wg show` reports peers over SSH.

- **Pros:** fast, self-contained, no third party, gives you the *whole* LAN (router + Pi + ESP32s) once connected.
- **Cons:** **requires a reachable public IP / working port-forward on the uplink** → **breaks under CGNAT**, which is the common travel case. You'd need DDNS + a real public IP to rely on it.

### (b) Tailscale on the Pi (recommended)
Install Tailscale on the Pi (the master plan already lists Tailscale on the hub):
```bash
curl -fsSL https://tailscale.com/install.sh | sh
sudo tailscale up
# optional: advertise the whole smart-room subnet so you reach the ESP32s too
sudo tailscale up --advertise-routes=192.168.8.0/24
# then approve the route + disable key expiry in the Tailscale admin console
```
Reach the Pi from any of your devices by its Tailscale name/IP; enable **subnet routes** (above) to also hit `192.168.8.x` devices, or **MagicDNS** for names.

- **Pros:** **works behind CGNAT** (NAT traversal / DERP relays punch out from the inside — no inbound port needed), no router config, survives the router changing uplinks, per-device auth, easy to revoke.
- **Cons:** depends on Tailscale's coordination service (a third party); to reach ESP32s (which can't run Tailscale) you must enable **subnet routing** on the Pi.

### ✅ Recommendation
**Use Tailscale on the Pi as the primary remote-access path**, with `--advertise-routes=192.168.8.0/24` so you also reach the router GUI and ESP32s. Reasoning: this is a *travel* router that will routinely sit behind CGNAT and change uplinks, and Tailscale's outbound NAT traversal is exactly what survives that — a WireGuard *server* on the router does not. Keep GL.iNet's WireGuard Server as a **secondary** option *only* for the rare venue where you control the uplink and have a real public IP/port-forward. (If you prefer everything on the router, GL.iNet also supports **Tailscale as a client on the router itself** under `VPN → Tailscale` on recent firmware — same CGNAT-beating benefit, gives the whole LAN, at the cost of running it on the weaker router CPU. The Pi is the stronger host, hence the primary pick.)

---

## 6. Programmatic control reference (for future automation)

Two ways to drive the router from scripts: the **GL.iNet JSON-RPC API** (`/rpc`) and plain **SSH + uci**. Document both so automation can pick whichever fits.

### 6a. GL.iNet JSON-RPC hashed-login flow (`POST /rpc`)

The GL GUI itself uses this. Login is a challenge/response so the plaintext password never crosses the wire.

**Step 1 — challenge.** Call `challenge` with the username; the router returns a `salt`, a `nonce`, and an `alg` (algorithm id).
**Step 2 — compute the hash (alg 1):**
1. `cipher = crypt(password, '$1$' + salt)` — Unix MD5-crypt with the returned salt (this yields the classic `$1$<salt>$<hash>` string).
2. `hash = sha256_hex(username + ':' + cipher + ':' + nonce)`
**Step 3 — login.** Call `login` with `{username, hash}`; on success you get a **`sid`** (session id).
**Step 4 — call.** Invoke any service method with `method: "call"` and `params: [sid, service, method, params]`.

Reference implementation (Python, stdlib + `requests`):
```python
import crypt, hashlib, requests

BASE = "http://192.168.8.1/rpc"
USER = "root"
PASSWORD = "<admin password>"   # do NOT commit this; pull from env/secret store

def rpc(method, params, _id=1):
    r = requests.post(BASE, json={"jsonrpc": "2.0", "id": _id,
                                  "method": method, "params": params})
    r.raise_for_status()
    return r.json()

# 1) challenge
ch = rpc("challenge", {"username": USER})["result"]
salt, nonce, alg = ch["salt"], ch["nonce"], ch["alg"]

# 2) hash (alg == 1)
assert alg == 1, f"unexpected alg {alg}"
cipher = crypt.crypt(PASSWORD, "$1$" + salt)
hsh = hashlib.sha256(f"{USER}:{cipher}:{nonce}".encode()).hexdigest()

# 3) login -> sid
sid = rpc("login", {"username": USER, "hash": hsh})["result"]["sid"]

# 4) call a service method, e.g. read wifi config
print(rpc("call", [sid, "wifi", "get_config", {}]))
```
Notes:
- `crypt.crypt(pw, "$1$"+salt)` is Python's MD5-crypt; equivalent to `openssl passwd -1 -salt <salt> <pw>`. On non-Linux dev machines where `crypt` is unavailable, use the `passlib` library's `md5_crypt`.
- Service/method names (`wifi`, `dhcp`, `firewall`, `system`, …) mirror GL's GUI actions; the safest way to learn a specific call is to open the GUI with the browser dev-tools Network tab and watch the `/rpc` payloads.
- The `sid` expires — re-challenge/login on `Access denied`-style errors.

### 6b. SSH + uci (simplest for config changes)

Everything in sections 1–4 is `uci` under the hood; scripting it is just SSH:
```bash
# key-based, non-interactive (set up a key first: ssh-copy-id root@192.168.8.1)
ssh root@192.168.8.1 'uci set wireless.radio0.channel=6 && uci commit wireless && wifi reload'

# read config as machine-parseable text
ssh root@192.168.8.1 'uci show dhcp'
```
For repeatable automation, keep a small library of `uci` snippets (one per section here) and always `uci commit <package>` + reload the relevant service (`wifi reload`, `/etc/init.d/dnsmasq restart`, `/etc/init.d/firewall restart`).

**Which to use:** use **uci/SSH for config changes** (channel, DHCP, isolation — deterministic, greppable, easy to back up). Use **`/rpc`** when you want to mirror exactly what the GUI does or read GL-specific status the GUI surfaces (client list, VPN state) without knowing the underlying OpenWrt file layout.

**Risk/caveat:** Both paths are **root** — a bad `uci commit` + reload can lock you out (§ safety). Never hard-code the admin password or PSK in a committed script; read them from an environment variable or a secret file outside the repo. Test automation against a config you've just backed up.

---

## 7. Firmware update & config backup

**Why it matters:** The firmware is a ~2024-06-06 build; updates bring security fixes, but a bad flash on your only network core strands the whole room. Backups make every other section in this doc reversible.

**Always back up first** (repeat of safety #1, because it's the one that saves you):
```bash
ssh root@192.168.8.1 "sysupgrade -b /tmp/backup-$(date +%F).tar.gz"
scp root@192.168.8.1:/tmp/backup-*.tar.gz ~/router-backups/
```
GUI backup: **System → Upgrade → Backup** downloads a restore-able archive. Do this before *any* change, not just firmware.

**Safe update path:**
1. **Back up config** (above) and note current firmware version (GUI **System → Upgrade** shows it).
2. GUI: **System → Upgrade → Online Upgrade** → check for a newer GL.iNet release for *this exact model*. If offline, download the correct `.img`/`.bin` for the model from GL.iNet's site on another machine and use **Local Upgrade**.
3. **Verify the model string matches exactly** before flashing — wrong-model firmware bricks the unit.
4. **Do it on wired/local access, not remotely.** A firmware flash reboots and drops WiFi; if you're on Tailscale-through-the-Pi-through-this-router, you'll cut your own access mid-flash.
5. Choose **"keep settings"** only for minor updates; for major version jumps, flash **clean** and restore only the specific `/etc/config` files you need, or re-apply sections 1–4 by hand — carrying a stale full config across a major version can reintroduce quirks.
6. After reboot: confirm SSID, the pinned channel (§1), DHCP reservations (§2), and isolation-off (§3) all survived. Then let the ESP32s and Pi re-associate and re-check MQTT + ESP-NOW before calling it done.

**Restore a backup:**
```bash
# GUI: System -> Upgrade -> Restore  (upload the .tar.gz)
# CLI:
scp ~/router-backups/backup-YYYY-MM-DD.tar.gz root@192.168.8.1:/tmp/
ssh root@192.168.8.1 "sysupgrade -r /tmp/backup-YYYY-MM-DD.tar.gz && reboot"
```

**Risk/caveat:** Never flash over an unstable/remote link or on low battery — an interrupted write can brick the router. Keep at least one known-good backup *off* the router (copied to the Pi or your laptop). If a flash goes wrong, GL.iNet units recover via **UBoot failsafe** (hold Reset while powering on, browse to the UBoot web UI, upload firmware) — worth reading GL.iNet's failsafe guide for your model *before* you need it.

---

*This runbook is documentation only. Nothing here has been executed against the live router — every command is provided for you to run deliberately, after the safety checklist, from a device that won't strand you.*
