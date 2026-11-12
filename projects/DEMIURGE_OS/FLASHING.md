# FLASHING: blank SD card to Demiurge node

**Bucket: sync.** State: STAGED, not run end to end (no golden image has been built or flashed, no fresh-Pi install tested).

Two routes:
- **PRIMARY: flash the golden image** (below). One Imager flash, boots straight into Demiurge, self-updating.
- **BACKUP: stock Pi OS Lite + first-boot installer** (second half). Builds from source over the network. Use when no golden image is available.

---

# PRIMARY: golden image via Raspberry Pi Imager

Pipeline that produces the image: `setup-script/image/README.md`.

## What you need
- Raspberry Pi Imager **2.x** (needed for the `cloudinit-rpi` customisation format), Pi 5, SD card
- The golden `demiurge-golden.img.xz`, either listed in A's os-list URL or downloaded as a local file
- Wi-Fi or Ethernet (needed for self-update and Claude login, not for first boot itself)

## Steps
1. **Imager**: pick the Pi 5. For the OS either:
   - set the content repository to A's os-list URL (App Options > Content Repository > Use custom, or `rpi-imager --repo <url>`) and pick **Demiurge**, or
   - *Use custom* and select the local `.img.xz`.
2. **OS customisation** (Edit settings):
   - Username: **`demiurge`** (the image bakes this user; do not change it) and a **password**
   - Wi-Fi: SSID, password, country (skip if Ethernet)
   - Services: **Enable SSH** (password or your public key)
   - Hostname: the node name, e.g. `neptr`. The hostname is the device name exactly (no prefix); the DEMIURGE FLASHER writes it directly. If left as `raspberrypi` it becomes `demiurge-<last 4 hex of the wlan0 MAC>`.
3. **Write** the card. Optional: to pin the identity explicitly, add `demiurge-device.conf` to the `bootfs` partition:
   ```
   DEVICE=neptr
   ROLE=instrument
   HARDWARE=pi5-16gb
   PAIR=
   ```
4. Put it in the Pi 5, power on. First boot regenerates ssh host keys and machine-id, sets hostname `<device>`, seeds `device.md`, then disables itself. The root filesystem expands to the card.
5. `ssh demiurge@<DEVICE>.local`

## Then, once per Pi (cannot be automated)
```bash
claude            # /login, finish the browser/code flow
gh auth login     # only to git push
```
Then pre-accept the trust prompts in `~/.claude.json` (`setup-script/CLAUDE_RC_AUTONOMOUS.md`, piece 6) or the agent hangs at "trust this folder?". Never copy `~/.claude.json` or `~/.claude/.credentials.json` between Pis (the image ships without them).

## Self-update
Same as the backup route: a 6 h timer stages newer commits from public GitHub into `/demiurge/versions/<hash>`, `/demiurge/current` flips at next boot before `demiurge.service`. Revert: `demiurge-update-revert`.

## Caveats
- The image user is always `demiurge`. Imager's username field must be `demiurge`, or you get a second user without the install.
- Without Imager customisation (raw `dd`), the baked account has a locked password and no ssh/wifi: you cannot log in. Always customise.
- Do not flash onto a card in a live audio rig.
- Untested: pishrink auto-expand on Trixie, the pinned `init_format`, and the whole first-boot regen. Test one spare card first.

---

# BACKUP: stock Pi OS Lite + first-boot installer (build from source)

Demiurge is git-based. Flash stock Pi OS Lite, drop in one hook, and the Pi pulls `bootstrap.sh` from public GitHub and installs itself. Slower (two boots, a full install over the network) and needs internet, but it needs no prebuilt image.

## What you need
- Raspberry Pi OS Lite **64-bit (Debian 13 Trixie)**, via **Raspberry Pi Imager**
- `flash/firstrun.sh` (this repo)
- Wi-Fi or Ethernet with internet on first boot

## Steps
1. **Imager**: pick the Pi model, OS = *Raspberry Pi OS (other) > Raspberry Pi OS Lite (64-bit)*, pick the card.
2. **OS customisation / advanced settings** (Edit settings):
   - Hostname: anything (bootstrap renames it to `<DEVICE>`)
   - Username + password: remember the username. It becomes `DEMIURGE_USER`. If unset the script uses the first uid-1000 user (the Imager user), never `pi`.
   - Wi-Fi: SSID, password, country (skip if Ethernet)
   - Services: **Enable SSH** (password or your public key)
3. **Write** the card. Do not eject yet if using step 5B.
4. **Edit the EDIT block** at the top of `flash/firstrun.sh` (copy the file first). Set `DEMIURGE_DEVICE`, `DEMIURGE_ROLE`, `DEMIURGE_HARDWARE`, `DEMIURGE_PAIR` (optional), `DEMIURGE_USER` (must match Imager). Leave `DEMIURGE_REMOTE` as the public GitHub URL.

   Primary NEPTR:
   ```bash
   DEMIURGE_DEVICE=neptr
   DEMIURGE_ROLE=instrument
   DEMIURGE_HARDWARE=pi5-16gb
   DEMIURGE_PAIR=
   DEMIURGE_USER=gorpi7        # whatever you set in Imager
   ```
   Neural Grid:
   ```bash
   DEMIURGE_DEVICE=neuralgrid-a
   DEMIURGE_ROLE=navigator
   DEMIURGE_HARDWARE=pi5-8gb
   DEMIURGE_PAIR=neuralgrid-b
   DEMIURGE_USER=demiurge
   ```
5. **Install the hook** (pick one):
   - **A. Paste.** Imager's custom first-run / run-script field (if your Imager version shows one): paste the whole edited file. It installs a systemd unit and arms itself.
   - **B. Boot partition.** Re-insert the card. On the `bootfs` partition copy the edited file as `firstrun.sh`, then append to the single line in `cmdline.txt` (same line, space-separated):
     `systemd.run=/boot/firmware/firstrun.sh systemd.run_success_action=reboot systemd.unit=kernel-command-line.target`
     First boot runs it once (install mode), strips those tokens, and reboots into normal boot.
   - **C. After first boot, over SSH:** `sudo bash firstrun.sh` (edited copy). Same result.
6. Put the card in the Pi, power on. Wait. Everything below happens by itself.

## What happens automatically
1. `demiurge-firstrun.service` waits for network, installs `git` + `curl` if missing.
2. Clones `DEMIURGE_REMOTE` to `/home/<user>/_______DEMIURGE` (as the Pi user).
3. Runs `bootstrap.sh` as that user: prereqs, `install.sh` (hostname `<DEVICE>`, `device.md`, phases 1-8), self-update wiring, agent unit.
4. **Phase 8 reboots the Pi.** The service is still armed, so after the reboot it runs again and finishes phases 9-11 + agent.
5. When `isolcpus=3` is active it writes `/var/lib/demiurge-firstrun/done`, disables and removes itself. It never runs again.

Log: `/var/log/demiurge-firstrun.log` (`tail -f` over SSH). Check: `systemctl status demiurge-firstrun`.

## The one manual step
Login is interactive OAuth. Once per Pi:
```bash
ssh <user>@<DEVICE>.local     # e.g. neptr.local
claude            # type /login, finish the browser/code flow
gh auth login     # only to git push; cloning public needs nothing
```
Then pre-accept the trust prompts in `~/.claude.json` (`setup-script/CLAUDE_RC_AUTONOMOUS.md`, piece 6) or the agent hangs at "trust this folder?". From then on the agent self-hosts every boot. Never copy `~/.claude.json` or `~/.claude/.credentials.json` between Pis.

## Self-update afterward
Public GitHub is the source. A timer (6 h) stages newer commits into `/demiurge/versions/<hash>`; `/demiurge/current` flips at next boot, before `demiurge.service`. Nothing restarts at runtime. Revert: `demiurge-update-revert`.

## Honest caveats
- First boot needs working internet. No network after ~5 min: the unit exits and retries next boot. Fix Wi-Fi, reboot.
- The Pi reboots mid-install (Phase 8). Do not unplug during it. Expect 2 boots before it is done.
- `claude /login` and `gh auth login` cannot be automated.
- Phase 8 normally asks you to type `yes` for the boot-config edit. The hook feeds `yes` on stdin (`DEMIURGE_AUTO_YES=1`). If the prompt text differs from what is expected, set it to `0` and run `bootstrap.sh` by hand over SSH.
- Not for a live audio rig: bootstrap skips the self-update layout on a Pi already running `demiurge`. Flash fresh cards only.
- Staged and untested end to end. Dry run first: `DRY_RUN=1 DEMIURGE_DEVICE=neptr bash firstrun.sh` (no root, changes nothing).
