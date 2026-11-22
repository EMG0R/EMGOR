# config/network/ — connectivity stability pack

The box is addressed by name — `demiurge.local` — by every tool in this
project: the companion TUI, the deploy scripts, the web UI, every `ssh` in
every doc. So "is the network up" is not a background concern here, it is the
precondition for everything. This directory is what makes that name resolve to
a working address, every time, without a human.

It exists because the Pi spent a night dropping off the network six-plus times
over several hours — each time completely unreachable (mDNS dead, no answer on
its IP), then coming back later on its own.

---

## What was actually wrong: three independent causes

Any **one** of these reproduces the symptom exactly. They were all happening,
which is why it looked random and why fixing one thing never held.

### (a) The home wifi profile was deleted, so the box joined a stranger's LAN

`/etc/NetworkManager/system-connections/GOR_CAVE.nmconnection` was gone. It had
existed — an orphaned lease at
`/var/lib/NetworkManager/internal-f479ebb6-…-wlan0.lease` still held
`ADDRESS=192.168.8.156` — and the directory's mtime pinned the deletion to a
specific minute. The likely culprit is our own `demiurge-wifi forget`.

With no profile for the home network, NetworkManager did the reasonable thing
and autoconnected to the only other saved network in range: a neighbour's AP,
on `192.168.1.0/24`. **A different subnet.** The Pi was up, healthy, running
audio — and completely unreachable from a Mac on `192.168.8.0/24`. From the
outside that is indistinguishable from a crash.

> `demiurge-wifi forget <home ssid>` deletes the box's only route home. It
> refuses to delete the network you are *currently on*, which is not the same
> guard at all — you can be on ethernet and forget the wifi you will need the
> moment the cable moves.

### (b) mDNS advertised BOTH addresses, and `demiurge.local` handed out the wifi one

The subtle one, and the one that explains the user's own contradiction:
*"ethernet just worked perfectly"* alongside *"it's always failing to
connect"*.

Wired and wireless are both on the GL-iNet's `192.168.8.0/24`. With both links
up the Pi held **two** addresses — `.189` wired, `.190` wifi — and avahi
advertised both under the same name. Route metrics (100 wired / 600 wifi) only
decide what the Pi **sends** on. They have no say whatsoever in which address
a *resolver* picks when a Mac looks up `demiurge.local`.

It was picking the wifi one. Measured, not guessed:

```
$ dns-sd -timeout -G v4 demiurge.local
demiurge.local.   192.168.8.190      <-- the wifi address
```

So every tool in the project was talking to the Pi over the flaky radio link
while a perfectly good cable sat idle. Whether any given session worked was a
coin flip on which record mDNS returned.

### (c) An RT audio livelock + the hardware watchdog = a spontaneous reboot

Some outages were not network faults at all. The Pi has the SoC watchdog armed:

```
$ cat /sys/class/watchdog/watchdog0/identity   ->  Broadcom BCM2835 Watchdog timer
$ cat /sys/class/watchdog/watchdog0/state      ->  active
$ cat /sys/class/watchdog/watchdog0/timeout    ->  60
$ sudo fuser -v /dev/watchdog0                 ->  PID 1  systemd
$ systemctl show -p RuntimeWatchdogUSec        ->  RuntimeWatchdogUSec=1min
```

`systemd` (PID 1, ordinary scheduling priority) must pet `/dev/watchdog` or the
SoC hard-resets the board after 60 s. The audio engine runs threads at
`SCHED_FIFO` 74–85. If one of those spins — a too-tight quantum, an interface
that stops clocking — it starves PID 1, systemd misses the pet, and the
hardware resets the box. No shutdown sequence, no log line, no warning.

The fingerprint is still on disk:

```
kernel: FAT-fs (mmcblk0p1): Volume was not properly unmounted.
```

That is an unclean shutdown, and it is the opposite kind of fault from (a) and
(b) with the opposite fix. **Distinguishing them was impossible** until now,
because of…

### The reason none of this could be diagnosed: the journal was volatile

`journalctl -b -1` had nothing. Ever. Raspberry Pi OS ships
`/usr/lib/systemd/journald.conf.d/40-rpi-volatile-storage.conf` with
`Storage=volatile`, so the journal lived in `/run` and **every reboot erased
the evidence**. Six outages produced zero logs.

Fixed by `99-demiurge-persistent-journal.conf`. The `99-` is load-bearing:
systemd merges drop-ins from `/etc` and `/usr` into a single list sorted by
*filename* — `/etc` only beats `/usr` for the *same* filename — so a `10-*`
file in `/etc` silently loses to the vendor's `40-*`. Anything sorting after
`40` wins. **Do not renumber that file down.**

---

## Telling the two failure modes apart, next time

Now that the journal survives reboots, one command decides it:

```bash
journalctl --list-boots            # did the outage cross a boot boundary?
```

| Evidence | Diagnosis | Fix lives in |
|---|---|---|
| Same boot ID across the outage; NM logs show disassociation / auth timeout / DHCP failure | **Network fault** — (a) or (b) | this directory |
| New boot ID, no clean shutdown record, `FAT-fs … not properly unmounted` | **Watchdog reset after an RT livelock** — (c) | audio: quantum, RT priorities |
| New boot ID *with* a clean `systemd-shutdown` sequence | Someone rebooted it | — |

```bash
journalctl -b -1 -u NetworkManager        # what the network was doing before
journalctl -b -1 -e                       # last thing it said before it died
journalctl -u demiurge-netguard -f        # live link state, one line per 20s
```

---

## What is installed

| File | Installed to | Purpose |
|---|---|---|
| `99-demiurge-persistent-journal.conf` | `/etc/systemd/journald.conf.d/` | Survive reboots so outages can be post-mortemed. 200 MB cap, 1-month retention. |
| `10-demiurge-network.conf` | `/etc/NetworkManager/conf.d/` | NM-wide defaults: powersave off, permanent MAC, **infinite autoconnect retries**, ethernet metric 100 / wifi 600. |
| `demiurge-netguard.sh` | `/usr/local/bin/` | The guard. Enforces one-link-at-a-time and recovers dead links. |
| `demiurge-netguard.service` / `.timer` | `/etc/systemd/system/` | Runs the guard every 20 s. |
| `demiurge-network.conf.default` | `/etc/demiurge/network.conf` | Guard config — pinned SSID, interfaces, thresholds. |
| `90-demiurge-failover` | `/etc/NetworkManager/dispatcher.d/` | Fires the guard the instant the wired link changes → ~2 s failover instead of ~20 s. |

Not installed, because Raspberry Pi OS already does it:
`/lib/modprobe.d/rpi-brcmfmac.conf` ships `options brcmfmac roamoff=1`.
Firmware roaming — the usual suspect for unexplained Pi disassociations — was
**already disabled** here, so it was never the cause. Verify with
`cat /sys/module/brcmfmac/parameters/roamoff` → `1`. Do not add a second
drop-in for it.

### The one setting that matters most

```ini
connection.autoconnect-retries=0
```

NetworkManager's default is **4**. After four failed association attempts it
gives up on the profile entirely and will not try again until something
external pokes it. That turns a 30-second router reboot into an indefinite
outage that looks exactly like a dead Pi. `0` means retry forever.

---

## The invariant: one link at a time

> **If you find wifi "mysteriously disconnected" while the cable is in, that is
> deliberate. Read this section before reconnecting it.**

While ethernet's gateway actually answers, the guard **parks** wifi
(`nmcli dev disconnect wlan0`). One interface up means one address, means
`demiurge.local` is unambiguous — which is the fix for cause (b). Pull the
cable and wifi comes straight back up.

Set `ETH_EXCLUSIVE=0` in `/etc/demiurge/network.conf` to keep both links up
instead, and accept that the name may resolve to either address.

Two safety properties make this safe to run unattended:

**It never parks wifi on an assumption.** Ethernet must be `connected` *and*
answer a gateway ping **pinned to the interface** (`ping -I eth0`). Both links
share a subnet, so an unpinned ping could be answered over wifi — and the
guard would park the very link carrying the reply.

**It cannot strand the box.** `nmcli dev disconnect` also clears the *device's*
autoconnect flag, so NM will not bring wifi back by itself — meaning recovery
would otherwise depend on this script being alive and correct. It doesn't: the
guard and the dispatcher hook both run `nmcli dev set wlan0 autoconnect yes`
**before** attempting to connect. If the `con up` fails, if the script dies
halfway, if it is deleted or masked — NetworkManager's own autoconnect still
recovers the link, and with `autoconnect-retries=0` it retries forever. The
flag is runtime-only, so a reboot re-arms it too. Worst case is a power cycle;
never a keyboard and monitor.

Manual recovery, if you ever need it: `sudo nmcli dev connect wlan0`.

### Recovery ladder

Per consecutive failed gateway probe, 20 s apart, when wifi is the primary
link. Wired is **never** bounced — if the cable is in and the gateway is quiet
that is the router's problem, and yanking `eth0` would drop every ssh session
on the box.

| Consecutive failures | Action |
|---|---|
| 1 | log and wait |
| 2 (`BOUNCE_AFTER`) | `nmcli dev disconnect/connect wlan0` |
| 6 (`RADIO_CYCLE_AFTER`) | `nmcli radio wifi off; sleep 2; on` |

Wandered onto the wrong SSID while the home network is in range? The guard
steers it back — but only when `HOME_SSID` is genuinely visible, so a Pi that
is legitimately out of range keeps whatever fallback it found.

To stop the guard acting while you deliberately sit on another network:

```bash
sudo touch /run/demiurge/netguard.pause     # cleared by reboot
```

### It always exits 0

An earlier service in this project poisoned `systemctl --failed` by exiting 1
when a dependency wasn't up. A network watchdog reporting *failed* because the
network is down is noise at exactly the moment you need signal. Every path in
the guard exits 0 and logs instead.

---

## Verifying it

```bash
# one default route, wired, metric 100
ip route | grep default

# the name resolves to the WIRED address (from the Mac)
dns-sd -timeout -G v4 demiurge.local

# guard is healthy and says something sensible every 20s
systemctl is-active demiurge-netguard.timer
journalctl -u demiurge-netguard -n 5 -o cat

# and it is not in the failed list
systemctl --failed
```

Cable-pull failover, with a safety net so a failed test cannot strand the box
(ethernet returns in 75 s no matter what):

```bash
sudo systemd-run --on-active=75 --unit=eth-restore --collect nmcli dev connect eth0
sudo systemd-run --on-active=4  --unit=eth-drop    --collect nmcli dev disconnect eth0
```

Measured on this box: cable out → wifi carrying traffic in **~6 s**; cable back
→ wired primary restored, wifi parked, name back to the wired address.

---

## 2026-08-16: the demiurge wifi system, and a hardware ceiling

`demiurge-wifi` (`src/wrappers/demiurge-wifi`) is this project's single write
path for wifi — join/forget/disconnect/radio, used identically by the web UI,
the on-device NEPTR touchscreen UI, and the CLI. Everything in this section
was found and fixed on one long night getting a specific network to connect
reliably. Read this before assuming a wifi "hang" or "won't connect" report is
new — check the list below first.

### Bugs found and fixed (all live, all durable across reboots/redeploys)

- **Polkit denied every UI-driven wifi action silently.** Neither the web UI's
  service nor an ssh session has an active local logind seat, and
  NetworkManager's stock policy requires one for `network-control`. A click in
  the UI did nothing and looked unresponsive. Fixed by
  `50-demiurge-netdev.rules`, granting `netdev`/`sudo` group members control
  unconditionally.
- **netguard was silently overriding manual network choices.** Its fallback
  (when no `HOME_SSID` is pinned) used to call `nmcli dev connect wlan0` —
  which picks NM's own "best" candidate and IGNORES every profile's
  `autoconnect=no`, because that flag only suppresses NM's own passive logic,
  not an explicit connect request. Every ~20s (the guard's timer) it would
  silently steal the link back to whatever it preferred. Fixed: when no SSID
  is pinned, the guard now does nothing and leaves wifi exactly as the user
  left it.
- **Infinite autoconnect retries caused real self-inflicted flapping.**
  `connection.autoconnect-retries=0` (retry forever) on a profile that
  genuinely can't connect meant NM never stopped disconnecting a working link
  to go try the broken one again. Bounded to 20 in
  `10-demiurge-network.conf`.
- **The on-device UI could fire a second join on top of one already
  mid-handshake.** `_activate_network()` in `neptr/neptrPhase4_UI.py` had no
  guard against `self.wifi_busy` — any stray touch (a known resistive-panel
  quirk) during an in-progress join canceled it with `reason=new-activation`
  before it could ever finish. Fixed with a `wifi_busy` check at the top of
  the tap handler.
- **`demiurge-wifi join`/`disconnect` had no bounded timeout and could hang
  or leave a zombie NM activation running after the caller gave up.** Both
  now use `nmcli -w` plus explicit cleanup (`con down`) on failure.
- **TRIED AND REVERTED: giving wpa_supplicant `SCHED_FIFO` priority.** The
  theory (`demiurge-csound-host` runs `SCHED_FIFO` 74 continuously while
  wpa_supplicant/NetworkManager/dbus/polkit were plain `SCHED_OTHER` 0, so the
  local `need-auth → prepare → config` phase before any radio traffic could
  get starved for seconds) was plausible and the core-isolation math checked
  out (audio confirmed on its own isolated core 3, wpa_supplicant on core 1 —
  never actually contending). But applying `CPUSchedulingPolicy=fifo
  CPUSchedulingPriority=15` to wpa_supplicant caused a real, worse regression:
  immediately after the service restarted under it, EVERY network — GOR_CAVE,
  EMGOR, Mymapop2020, all of them — started failing with
  `CTRL-EVENT-ASSOC-REJECT status_code=16` (auth timeout), confirmed in
  wpa_supplicant's own log. Reverted (drop-in file deleted, service
  restarted back to `SCHED_OTHER`) and the regression cleared immediately.
  **Do not re-apply this without a real explanation of why FIFO scheduling
  broke wpa_supplicant's own auth-frame timing** — whatever the mechanism is,
  it made things categorically worse, not better. The CPU-starvation theory
  for the original slow/inconsistent joins is therefore back to unproven,
  not confirmed fixed.
- **A redundant `join` on an already-active SSID was forcing needless
  disconnect/reconnect cycles.** `do_join` had no "already connected" check,
  so calling it again on a network you were already on (easy to do
  accidentally while testing) tore down a working link for no reason — this
  looked exactly like random instability on an otherwise-fine connection.
  Fixed: `do_join` now short-circuits immediately if the requested SSID is
  already active.

### The hardware ceiling: the Pi's onboard radio and multi-AP/mesh networks

After every software fix above, one specific home network (a router +
wifi extender rebroadcasting the same SSID — confirmed via scan: 3+ distinct
BSSIDs answering under one SSID name, on both the 2.4GHz and 5GHz bands)
still connects only intermittently. Diagnosis, in order:

1. CPU starvation is unproven either way — the RT-priority attempt to test
   this caused a worse regression (see above) and was reverted before it
   could actually confirm or rule anything out.
2. Ruled out AP-selection ambiguity — pinned the connection to one specific
   BSSID (`802-11-wireless.bssid`) and it **still** showed the same rapid
   `associating → disconnected → scanning → associating` cycling against
   that single, unambiguous target, before eventually succeeding cleanly on
   a later retry with nothing changed.
3. That pattern — repeated raw 802.11 association failures against one
   specific radio, then a clean success later with no config change — is the
   signature of RF-layer flakiness (channel congestion, or a real limitation
   in the Pi 5's onboard Broadcom `brcmfmac` radio/firmware handling
   association under marginal conditions), not anything fixable in
   NetworkManager, netguard, or `demiurge-wifi`.

**Conclusion: the Pi's base hardware wifi radio is not reliably compatible
with multi-AP/mesh-style networks (multiple physical radios answering one
SSID).** A single, unambiguous AP (this box's actual home network, or a
phone hotspot) is the case this hardware handles perfectly — every fix above
made exactly that case rock-solid. There is no known config-level fix for the
mesh case; the only remaining levers are physical (a USB wifi adapter with a
different chipset, or moving the mesh AP to a less congested channel) — not
something to keep re-investigating in software.

### 2026-08-16, later: confirmed working after all the above

User confirmed switching networks worked cleanly on a single-AP connection
(low-power cable / hotspot-style setup) after the full fix set above landed
and the RT-priority regression was reverted. Matches the diagnosis: single,
unambiguous AP is the case this hardware handles reliably.
