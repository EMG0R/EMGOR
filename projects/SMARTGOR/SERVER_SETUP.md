# gorcave Server Setup — State as of 2026-08-27 (updated)

Raspberry Pi 4 "gorcave" hardened, storage share + backup timer + dashboard
API **now live**, router DHCP pinned. Nothing was deleted; every edited
config was backed up first.

**⚠️ Power hazard found and diagnosed this session — see "Power / USB budget
hazard" below. Must be fixed before the 8TB drive is attached.**

## Pi: what's configured now

**OS / base state**
- Debian 13 (trixie), aarch64, hostname `gorcave`
- 57 packages have pending upgrades (not applied — report only, per
  instructions). `unattended-upgrades` will now pull security/stable updates
  automatically going forward; a manual `apt upgrade` review is still
  advisable at some point for the backlog.
- NTP: synced, `systemd-timesyncd`/`timedatectl` reports
  `System clock synchronized: yes`, `NTP service: active`. No fake-hwclock
  needed (real RTC/NTP works fine).
- Swap: 2G zram swap configured, unused (0B used) — healthy.
- Temp: 51-53°C at idle, `throttled=0x0` (no throttling, ever). Fine for a
  case Pi.
- Tailscale is running and connected (TAILNET_IP, funnel on) — noted,
  not something I touched.

**Security / hygiene fixes applied**
- Installed and enabled `unattended-upgrades` + `apt-listchanges`. Default
  Debian config used as-is (`50unattended-upgrades` origins pattern covers
  Debian + Debian-Security; `20auto-upgrades` has periodic update+upgrade
  both `"1"`). Service is `active`/`enabled`.
- journald: confirmed persistent storage was already active
  (`/var/log/journal` exists, 8M in use). Capped it with
  `SystemMaxUse=200M` in `/etc/systemd/journald.conf` (backed up to
  `/etc/systemd/journald.conf.bak.<timestamp>` first), restarted
  `systemd-journald` to apply. Journal can no longer grow to threaten the
  3.0-3.2G free disk.

**Disk — root is 77-79% full (15G card, ~3G free). Top consumers found, nothing deleted:**

| Path | Size | Notes |
|---|---|---|
| `/home/server` | 4.5G | biggest single consumer |
| `/home/server/llama` | 2.6G | local LLM models (`llama/models` = 2.5G) |
| `/home/server/.cache` | 934M | `huggingface` 680M, `pip` 255M |
| `/home/server/SPEAK` | 870M | mostly `.venv` (860M) — did not touch, per instructions |
| `/home/server/lightserver` | 112M | mostly `.venv` (111M) — did not touch |
| `/usr` | 3.1G | normal OS footprint |
| `/var` | 2.7G | mostly `/var/cache/apt` (419M) + `/var/lib/apt` + `/var/lib/dpkg` — normal apt state, `apt-get clean` would reclaim some of the 419M cache if ever needed but I left it |

The llama models directory (2.6G) and the two `.venv`s (~970M combined) are
the most reclaimable if disk pressure becomes urgent, but nothing was
touched — this is a report only, per instructions.

**Storage share — LIVE (drive still not attached; empty share works fine)**
- Samba (`smbd`/`nmbd`) is now **enabled and active**. `/etc/samba/smb.conf`
  backed up before editing.
- `[storage]` (path `/srv/storage`) is a **fully open guest share** — no
  password, guest-writable — per explicit user request. `[timemachine]`
  (path `/srv/storage/timemachine`) is a dedicated Time Machine target with
  `fruit:time machine = yes`; kept separate from `[storage]` so TM's sparse
  bundles don't mix with general file dumping (see `STORAGE_SETUP.md` on the
  Pi for the full reasoning). Both use `vfs objects = catia fruit
  streams_xattr` for macOS Finder compatibility (resource forks, extended
  attributes). Global: `map to guest = Bad User`, `min protocol = SMB2`.
- `avahi-daemon` (already running) got a `_smb._tcp` service file at
  `/etc/avahi/services/smb.service` so `GORCAVE` now shows up automatically
  in the macOS Finder sidebar. `wsdd2` was not installed (Windows-only
  discovery, not needed for a Mac-only LAN).
- Verified from the Mac: `smbutil view //guest@gorcave.local` lists
  `storage`/`timemachine`; `mount_smbfs` + guest write both succeeded.
- Installed `rclone` v1.60.1 (Debian apt package, binary only) — still no B2
  remote configured (on hold pending B2 purchase approval, per plan).
- `/srv/storage` is now `server:server 775` (was `root:root 755`) with an
  empty `timemachine/` subdir pre-created; **when the real drive mounts,
  recreate `mkdir -p /srv/storage/timemachine` once** since the mount will
  shadow today's placeholder directory.
- Full runbook, including what's DONE vs. still pending, lives at
  **`/home/server/STORAGE_SETUP.md`** on the Pi (backed up before this
  session's edits).

**Backup — LIVE (daily noon + on-demand, gracefully SKIP-logs until drive+B2 exist)**
- `/home/server/bin/backup.sh`: logs timestamped runs to
  `/home/server/backup.log`, writes machine-readable status to
  `/home/server/backup-status.json`
  (`{last_run, result: ok|skip-no-drive|skip-no-b2|error, detail, running, bytes_transferred}`).
  Skips gracefully (exit 0) if `/srv/storage` isn't mounted, or if no
  `b2-gorcave:` rclone remote is configured — both true right now, so it
  logs `SKIP: no drive` and exits clean. Once B2 is approved and the drive
  is attached it'll `rclone sync` `/srv/storage/irreplaceable` and
  `/srv/storage/photos` (matching the original plan's selective-subdir
  approach) at `--bwlimit 15M`.
- `b2-backup.service` (oneshot) + `b2-backup.timer` (`OnCalendar=12:00`
  daily, `Persistent=true`) — **enabled and running now**, deliberately left
  on so the daily SKIP-log itself proves the pipeline works end to end.
  Manual trigger: `sudo systemctl start b2-backup.service`, or via the
  dashboard API (below).

**Dashboard API — LIVE**, new small FastAPI service for the website
dashboard (built by a parallel agent against this exact contract):
- `/home/server/dashboard/app.py`, own venv at
  `/home/server/dashboard/.venv` (fastapi + uvicorn[standard]).
- systemd unit `dashboard-api.service` — uvicorn on `127.0.0.1:8790`,
  enabled + active. CORS allows all origins (site is on Netlify).
- Endpoints: `GET /api/dash/health`, `GET /api/dash/stats` (temp, throttled,
  uptime, load, mem, disk (root + storage), service states, lightserver
  scene/fps/nodes-online, passive net rx/tx sampled off `eth0`),
  `GET|POST /api/dash/speedtest`, `GET|POST /api/dash/backup`.
- Speed test: `speedtest-cli` (already installed) run by
  `/home/server/dashboard/speedtest_runner.py`, writing
  `/home/server/speedtest-status.json`. New `speedtest.service` +
  `speedtest.timer` (every 6h, `RandomizedDelaySec=5m`) — enabled + active.
  `POST /api/dash/speedtest/run` triggers the same service on demand.
- `POST /api/dash/backup/run` calls `sudo systemctl start
  b2-backup.service`. **Permission approach**: no new sudoers drop-in was
  needed — user `server` already has passwordless sudo (per this session's
  access grant), so the API (running `User=server`) just shells out to
  `sudo systemctl start ...` directly; verified working via journal log
  (`sudo[...]: server : ... COMMAND=/usr/bin/systemctl start
  b2-backup.service`). If that passwordless sudo is ever tightened, add a
  narrow drop-in scoping only `systemctl start b2-backup.service` and
  `systemctl start speedtest.service` for user `server`.
- **Exposed via Tailscale Funnel**: added `/api/dash` to the existing
  `tailscale serve` config (mirrors how `/api/chat` etc. are set up) —
  existing paths (`/api/chat`, `/api/health`, `/api/voice/*`) were left
  untouched. Note: the plain `tailscale serve --set-path` command briefly
  demoted the whole funnel to tailnet-only (a `serve`/`funnel` mode
  interaction) — caught and fixed same-session with `tailscale funnel --bg
  443`, confirmed all paths (including `/api/dash`) back under
  `(Funnel on)`.
- Verified publicly from the Mac:
  `curl https://gorcave.YOUR-TAILNET.ts.net/api/dash/health` →
  `{"ok":true}`; `/api/dash/stats` returns full live JSON; `/api/dash/backup`
  and `/api/dash/speedtest` return real status (one speed test was run
  manually so the dashboard has data on first load: ~44 Mbps down / 8 Mbps
  up / 34ms ping — expect this to change once the router's power/uplink
  situation is fixed, see below).

**Monitoring**
- Added `/home/server/bin/health.sh` (executable). One command reports:
  temp + throttled flag, disk (`/` and `/boot/firmware`), memory, uptime/load,
  status of `lightserver.service`, `server-agent.service`,
  `unattended-upgrades.service`, `smbd`/`nmbd`, and a pull of
  `localhost:8800/state` (lightserver strip states) for a live LED-strip
  health check. Tested — works, and confirmed all three ESP32 strips
  (`fcob-strip`/`2`/`3`) online and streaming during the test run.
- `~/SPEAK` was **not touched** at any point (per instructions — another
  agent may be editing it).
- `lightserver.service` and `server-agent.service` were **not restarted**;
  verified both still `active` and lightserver still streaming
  (`localhost:8800/state` returns live frame counts) after all router/Pi
  changes.

## Router — DONE (reachable this session)

GL.iNet **GL-SFT1200** (Opal), firmware **4.3.17**, OpenWrt 18.06 base
(`Linux GL-SFT1200 4.14.90`, mips/siflower). SSH was reachable on all 3
attempts once the rsa host-key-algorithm workaround plus the given password
were both supplied together (earlier password-less attempts got
"Permission denied" — that's an auth prompt, not unreachability).

**Notable router state (unchanged, reported only):**
- WAN is actually a **wireless uplink** (repeater mode) — `wireless.sta`
  bridges to an upstream network called `Samsungfridge420`, not a wired WAN
  connection. Worth knowing if internet ever drops for the whole LAN.
- No firewall port forwards configured (`uci show firewall | grep redirect`
  was empty).
- Guest network SSIDs exist (`GL-SFT1200-4c0-Guest` 2.4G,
  `GL-SFT1200-4c0-5G-Guest` 5G) but I did not check/change whether they're
  actively broadcasting — left as found.
- Main SSID `YOUR_WIFI_SSID` (2.4G) / `GOR_CAVE` (5G) — untouched, per
  hard rule.

**DHCP static leases added** (backed up `/etc/config/dhcp` to
`/etc/config/dhcp.bak.<timestamp>` on the router first, then `uci add dhcp
host` x4, `uci commit dhcp`, `/etc/init.d/dnsmasq restart`):

| Host | MAC | Pinned IP |
|---|---|---|
| gorcave (Pi) | `dc:a6:32:d7:cf:05` | 192.168.8.205 |
| fcob-strip | `a0:b7:65:dc:ca:00` | 192.168.8.168 |
| fcob-strip2 | `20:50:0d:19:e2:88` | 192.168.8.138 |
| fcob-strip3 | `68:09:47:4c:f1:b4` | 192.168.8.169 |

fcob-strip's MAC wasn't given in the task — found it by reading the
router's own DHCP lease table (`/tmp/dhcp.leases`), where it showed as
`esp32-DCCA00` already sitting at 192.168.8.168, matching the expected IP.
All four leases now show their friendly hostnames in `/tmp/dhcp.leases`
after the dnsmasq restart, confirming the pin took effect. Verified
lightserver's `/state` endpoint still shows all three strips `online: true`
immediately after the restart — no disruption.

## Power / USB budget hazard — diagnosed this session, fix required before drive arrives

The Pi was found powered off (user had to reboot it), and rebooting it took
the LAN WiFi down too. Diagnosis:

- **Root cause: the GL.iNet router is powered from the Pi's own USB port.**
  The Pi 4's USB bus shares a single ~1.2A power budget; the router alone
  draws close to 1A of it. Under load — a speedtest happened to be running
  at the exact moment this Pi went down — the combined draw exceeded what
  the supply could deliver and the board brownout-shut-itself-off.
- Evidence: `journalctl --list-boots` showed the previous boot's journal
  data lost (consistent with power loss corrupting/discarding the
  unflushed journal, not a clean `shutdown`/`reboot`); the next boot's
  `dmesg` showed `EXT4-fs: orphan cleanup on readonly fs` — an unclean
  unmount signature, again consistent with sudden power loss rather than a
  controlled reboot.
- **Router and Pi uptimes matched to the minute** (`uptime` sampled on both
  within seconds of each other: both ~4 minutes) — confirming they power
  cycle together, i.e. the router really is riding on the Pi's power, not
  independently powered. This is why rebooting "the Pi" kills the WiFi: the
  router goes down with it.
- `vcgencmd get_throttled` read `0x0` post-reboot (that register resets each
  boot, so it can't retroactively prove undervoltage at the moment of the
  crash — but combined with the shared-uptime finding, a USB power budget
  brownout under the parallel-running speedtest is the most likely
  explanation, not a bad PSU per se).

**This must be fixed before the 8TB USB drive is attached.** A bus-powered
spinning drive draws real current through that same shared USB rail; with
the router already eating most of the budget there will be nothing left for
the drive, and brownouts would likely become frequent or immediate.

**Fix — physical, for the user to do (not done remotely/blind this
session):** move the GL.iNet router onto its own wall-powered 5V/2A adapter,
off the Pi's USB port entirely. Once that's done the Pi's full USB budget is
free for the drive, and the router's power stops depending on the Pi being
up. This same note was added to the power section of
`/home/server/STORAGE_SETUP.md` on the Pi.

## Storage / backup plan (target state, not yet active)

```
USB drive (~4TB, arriving soon)
   → /srv/storage (ext4 or existing NTFS/exFAT, mounted via fstab UUID+nofail)
   → Samba share "storage" on the LAN (user: server) — general file access
   → rclone selective sync (irreplaceable subdirs only, not full 4TB)
   → Backblaze B2 bucket, ~$6.95/TB/mo → ~$28/mo for ~4TB
     (S3-compatible API — website/tools can read it later if desired)
```

Full step-by-step runbook with copy-pasteable commands and systemd unit
files: `/home/server/STORAGE_SETUP.md` on the Pi.

## Remaining human steps

1. **Move the GL.iNet router to its own wall power adapter, off the Pi's USB
   port** — see "Power / USB budget hazard" above. Do this before step 2.
2. Plug in the USB drive → SSH to the Pi and follow
   `/home/server/STORAGE_SETUP.md` steps 1-3 (identify, format-or-keep,
   fstab). The Samba share (step 4) and backup script/timer (step 5b/5c)
   are already live and need no further action — files just appear in the
   share once the drive mounts. Remember to `mkdir -p
   /srv/storage/timemachine` once after the real drive is mounted.
3. Decide on and approve the Backblaze B2 purchase (~$28/mo for ~4TB) —
   nothing in step 5a of the runbook (B2 account, `rclone config`) has been
   executed; it's explicitly gated on this approval. Once done, `backup.sh`
   needs no changes — it already looks for the `b2-gorcave:` remote and
   will start actually syncing instead of skipping.
4. Optional: review the 57 pending apt upgrades at some point
   (`apt list --upgradable`) and decide whether to run a manual
   `apt upgrade` beyond what unattended-upgrades will pick up automatically
   (unattended-upgrades only auto-applies Debian/Debian-Security origin
   packages by default).
5. Optional: `/home/server/llama/models` (2.5G) and the two Python
   `.venv`s are the biggest reclaimable disk if the Pi ever gets tight —
   no action taken, just flagging for later.
6. Known issue, not caused by this session's work: `fcob-strip` (one of the
   three LED strip nodes) was showing `online: false` /
   `[Errno 101] Network is unreachable` in lightserver's `/state` after the
   power-outage reboot, while `fcob-strip2`/`3` were fine. Likely just needs
   a power-cycle/reconnect of that ESP32 node; not touched, per the
   do-not-touch-lightserver rule.
