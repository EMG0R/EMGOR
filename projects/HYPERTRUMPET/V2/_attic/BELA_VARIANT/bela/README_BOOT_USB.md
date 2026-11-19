# HYPERTRUMPET V2 - Bela boot + USB audio infrastructure

Target: **Bela Gem Stereo** (PocketBeagle 2, 2 audio in / 2 audio out). Ported from `hyperGuitar/bela/` (Gem Multi, read-only source; nothing there was modified).
Status: **written, not run.** The board is not flashed. Nothing here has been tested on hardware. Items marked `[UNVERIFIED]` are guesses or carry-overs, not facts.

## 1. How the system works

```
 Bela project "hyperTrumpet" (RT thread, Xenomai/EVL)
   render(): ht_ring_push(FX L, FX R, dry in0, dry in1)   -- lock-free, never blocks, drops if reader behind
        |  POSIX shm  /hypertrumpet_usbaudio   (HtRing: 512 frames x 4 lanes x int16)
        v
 usbaudio-bridge (ordinary process, Nice=5)  -- ALSA writei, plughw:UAC2Gadget,0, S16_LE, 32-frame period x 3
        v
 UAC2 gadget function (kernel, configfs g_multi/functions/uac2.usb0)  -- appears on the Mac as a USB audio INPUT
 (same composite gadget also carries ACM serial + NCM + RNDIS network + MIDI -> ssh at 192.168.7.2)
```

**Gadget composition.** `bela-usb-gadgets.service` (enabled by the Gem image builder) runs `/usr/bin/bela-start-usb-gadgets`, which calls `/usr/bin/bela-start-acm-ncm-rndis-old-gadget`. That script builds one configfs gadget `g_multi` (only if the dir does not exist yet): RNDIS + NCM (network), ACM (serial), UAC2 (audio), MIDI, optional mass-storage, then writes `UDC`. Order matters: all functions are created and linked into `configs/c.1` **before** `ls /sys/class/udc > UDC` binds it; UAC2 cannot be changed after bind without unbinding (which drops the ssh link). So gadget changes only apply at boot. The hyperGuitar UAC2 addition: `c_/p_srate 44100`, `c_/p_chmask 3`, `c_/p_ssize 2`. Direction naming: `p_*` = gadget **playback** = what the host **captures**; `c_*` = what the host plays to the board (unused by the bridge).

**bridge.c (hyperGuitar original).** 2 channels S16_LE, 44100 Hz, hard-coded. ALSA period 32 frames x 3 periods = 96 frames (~2.2 ms buffer; the earlier 512x4 ~46 ms was a large share of reported latency). Ring: 512 frames (~11.6 ms ceiling); producer drops when full. **Active resync**: the backlog is steered to 64 frames (~1.45 ms); if it exceeds 192 (~4.35 ms) the bridge snaps forward instead of draining slowly (a persistent tiny rate mismatch otherwise pins the ring at the ceiling = latency creep). Estimated bridge-side latency ~1.5 ms ring + <=2.2 ms ALSA, plus USB/host buffering `[UNMEASURED]`. xrun/EIO "recover" spam ~2/s while no host app has the device open is harmless (isochronous stream not flowing). A `/run/hyperguitar-usbaudio-off` flag file makes it close the PCM and idle (written by the instrument's USB switch).
The instrument creates the shm ring fresh on **every** start (`shm_unlink` + create); the bridge maps it once, so the supervisor must `try-restart usbaudio-bridge` on every instrument (re)launch or no audio flows until replug.

**Supervisor.** The instrument exits on a codec watchdog/ESD fault (2 s of pathological input) and must be relaunched to re-init the codec. The supervisor is a bash loop (`-p 64`, `stdbuf -oL` so rt_printf reaches the log, 1 s delay) run by a systemd unit (`Restart=always`).

**Boot autostart: Bela's mechanism vs the custom one.**
| | Bela stock | custom (what we use) |
|---|---|---|
| enable | `make -C ~/Bela PROJECT=x CL="-p 64" startuploop` (= `scripts/set_startup.sh`) or IDE toggle | `systemctl enable hypertrumpet-supervisor` |
| state | `systemctl enable bela_startup` + writes `/opt/Bela/startup_env` (`ACTIVE=1 PROJECT=.. ARGS=..`) | none |
| runner | `bela_startup.service` -> `/opt/Bela/bela_startup.sh` -> `make PROJECT runonly`; **single-shot** per service (service has `Restart=always`) ; first lines of the script exec `/opt/Bela/local/bela_startup.sh` if present (override hook) | `/root/hyperTrumpet-supervisor.sh` |
| `ACTIVE=0` | script runs `make stopstartup` and exits | -- |

hyperGuitar first used the override hook (`bela_startup_local.sh`, kept here as `alt/`), then moved to its own unit with `Conflicts=bela_startup.service` and **disabled bela_startup permanently** because the two fought for the audio device (1/s crash storms, 2026-11-01; and a `usbaudio-bridge` start-limit loop 2026-09-01 caused by `Wants=/PartOf=bela_startup`). We follow the end state: one mechanism (supervisor unit), bela_startup disabled, `startup_env` = `ACTIVE=0`. [Bela mechanism read from BelaPlatform/Bela `Makefile`, `scripts/set_startup.sh`, `resources/stretch/rootfs/opt/Bela/bela_startup.sh`; the Gem (Bookworm) image's own copies were not inspected, but hyperGuitar's card confirms the service name and that it contends with a supervisor.]

## 2. Files

| file | installs to | purpose |
|---|---|---|
| `install.sh` | (Mac) | one-shot idempotent installer, see section 4 |
| `hypertrumpet.env` | `/etc/default/hypertrumpet` | knobs: block size, USB rate, UAC2 chmasks, USB product string. Created once, never overwritten |
| `usb-gadget/bela-start-acm-ncm-rndis-old-gadget` | `/usr/bin/` (stock saved as `.stock`) | gadget builder; hyperGuitar copy + env knobs + 4-ch UAC2 + "hyperTrumpet" product string |
| `usb-gadget/bela-start-usb-gadgets` | `/usr/bin/` | unchanged wrapper from hyperGuitar |
| `usbaudio-bridge/hypertrumpet_usbring.h` | `/root/usbaudio-bridge/` and beside `render.cpp` | shared ring definition (single source of truth) + RT-safe `ht_ring_push()` |
| `usbaudio-bridge/bridge.c` | built to `/usr/bin/usbaudio-bridge` | ring -> UAC2 ALSA bridge |
| `usbaudio-bridge/usbaudio-bridge.service` | `/etc/systemd/system/` | bridge unit |
| `hyperTrumpet-supervisor.sh` | `/root/` | relaunch loop |
| `hypertrumpet-supervisor.service` | `/etc/systemd/system/` | boot autostart, `Conflicts=bela_startup` |
| `set-usb-subnet.sh` | `/root/` (run from Mac via ssh) | find/replace the board's 192.168.7.x (optional, see 5) |
| `verify.sh` | `/root/ht-verify.sh` | read-only checklist probe |
| `alt/bela_startup_local.sh` | not installed | the override-hook alternative; use only instead of the supervisor unit |

## 3. USB audio mapping (Gem Stereo) and what bridge.c supports

- Gem Stereo has only **2 in / 2 out** codec channels. The USB gadget is virtual and independent of the codec, so UAC2 may carry more. Upstream `f_uac2.c` accepts any `p_chmask` within `0x07FFFFFF` (up to 27 channels) `[verified in torvalds/linux; the board runs TI 6.12.49 with the same configfs attrs per hyperGuitar's configfs-snapshot.txt]`. 4 channels @ 44.1 kHz S16 = ~353 kB/s, trivial for high-speed USB. macOS acceptance of a 4-ch UAC2 input from this gadget is `[UNVERIFIED]`.
- hyperGuitar's bridge was **hard-wired stereo**: it could not carry 4 channels. The trumpet bridge reads `p_chmask` from configfs at each PCM open and opens `ch = popcount(mask)` (env `HT_USB_CH` overrides, fallback 2). The ring always carries 4 lanes; lanes beyond the gadget's channel count are dropped, missing ones zero-filled.
- Mapping to host (default `HT_UAC2_P_CHMASK=15`): **1-2 stereo FX out, 3 = dry in0 (bell mic), 4 = dry in1 (contact mic 1)**. Set `HT_UAC2_P_CHMASK=3` for FX only (identical to hyperGuitar). Producer side in `render()`: `ht_ring_push(gBridge, wL, wR, dryIn0, dryIn1)`; dry taps should be pre-FX, pre-gain, scaled for int16 (hyperGuitar's diag tap used x8 on quiet piezos; mics need their own scaling).
- Sample rate: ring and gadget are not resampled. Bela project rate **must equal** `HT_USB_RATE` (44100). The Gem supports up to 96 kHz and "uniform sample rate" defaults on in new projects `[verified, learn.bela.io migration guide]`; the Gem default audio rate was not confirmed, so pin it in the project.
- `c_chmask` stays 3 as in hyperGuitar (host sees an unused output). `HT_UAC2_C_CHMASK=0` would remove it `[UNTESTED]`.

## 4. Install (run from the Mac, after the fresh Gem image boots; DO NOT run before the board is flashed)

```
cd ~/Documents/_EMGOR_SYNTH/______2026NEW/HYPERTRUMPET_V2/bela
bash install.sh --hostname hypertrumpet                 # infra only
bash install.sh --hostname hypertrumpet --project-dir ../bela/hyperTrumpet   # + sync/build the project (once it has *.cpp)
```
`install.sh` (idempotent, compares before every write): preflight (model, kernel, configfs, audio cards, gcc) -> stage -> install env (never overwritten) -> back up stock gadget scripts to `*.stock` once, warn if the stock script differs a lot from the hyperGuitar-era one -> install gadget scripts, build bridge, units, supervisor -> disable `bela_startup`, write `ACTIVE=0` -> enable bridge + supervisor -> optional project scp + `make ... Bela` -> start bridge only. It never restarts the gadget (would kill the ssh link) and never starts the instrument unless `--start`. **Reboot once** afterwards so the new gadget is created, then `ssh root@192.168.7.2 /root/ht-verify.sh`.
Prerequisites: stock Gem image flashed to the microSD (same procedure as `hyperguitar_1v2/README.md`: `xz -dc bela-gem.img.xz | sudo dd of=/dev/rdiskN bs=4m`), **Gem cape seated** (without it: "unrecognized Bela hardware" and a relaunch loop; one hyperGuitar boot showed only the red PB2 LED = reseat the cape), Bela powered directly or from a supply that can boot it (a USB isolator starved it: 7 s boot loop, `error -71`).

## 5. Two Belas on one Mac: IP clash

Both boards default to **192.168.7.2** (NCM/macOS; RNDIS side is 192.168.6.2 on older images `[BelaPlatform/bela-image-builder interfaces]`). Plugged into one Mac simultaneously, two USB-network interfaces would sit in 192.168.7.0/24 and ssh/OSC to .2 is ambiguous. Their USB serial/MAC (and product string, now `hyperTrumpet-xxxx` vs `Bela-xxxx`) differ, so the interfaces themselves coexist. Also change the mDNS name (`--hostname hypertrumpet` -> `hypertrumpet.local`; hyperGuitar stays `bela.local`).
Proposal: trumpet on **192.168.9.0/24** (board .2, Mac .1). Avoid .6/.7 (Bela), .8 (gorcave router 192.168.8.1), .0 (MTIID Mac mini lan).
How: `bash set-usb-subnet.sh --subnet 9` lists every file on the board containing `192.168.7.` (report only); review; `--apply` backs them up to `/root/ht-net-backup/`, rewrites, you reboot and reconnect at 192.168.9.2. `[UNVERIFIED]` where the Gem image assigns .7.2 (ifupdown vs systemd-networkd vs NM) and whether the board runs a DHCP server for the host side; the script finds the real mechanism instead of assuming. If no DHCP appears, give the new Mac interface 192.168.9.1/24 manually. Default after `install.sh` is **unchanged 192.168.7.2**, so the board is reachable first time. Do this only if both will be plugged in together; project OSC/UDP code that hard-codes 192.168.7.2 must follow.

## 6. Verification checklist

On the Bela: `ssh root@192.168.7.2 /root/ht-verify.sh` -> all PASS for: g_multi exists + bound to a UDC, functions rndis/ncm/acm/uac2, `UAC2Gadget` ALSA card, `p_chmask=15`, `p_srate=44100`, usb net address, bridge + supervisor enabled, `bela_startup` not enabled, `ACTIVE=0`.
By hand:
1. `aplay -l` shows the Gem codec with 2 channels and `UAC2Gadget`; `arecord -l` shows 2 capture channels (confirms Stereo, not Multi).
2. Mac `system_profiler SPUSBDataType | grep -A5 hyperTrumpet`; Audio MIDI Setup shows **hyperTrumpet-xxxx with 4 in** (or 2 if mask 3).
3. `journalctl -u usbaudio-bridge -n20` -> `p_chmask=15 -> 4 ch, rate 44100` and `PCM open, ch=4 rate=44100`; no `rate mismatch` warning.
4. After the project runs: `ls /dev/shm/hypertrumpet_usbaudio`; record the Mac input: ch1-2 = FX, ch3-4 = dry mics; check ch order and that dry channels are not silent.
5. **Reboot with nothing connected except power**, then connect: supervisor active, instrument running, `underruns` flat (`grep -c "Underrun detected" /root/hyperTrumpet-live.log`).
6. Kill the instrument process (with permission): supervisor relaunches, bridge was bounced (`journalctl -u usbaudio-bridge` shows restart), audio resumes without replug.
7. No-cape/bad-hardware boot: log relaunches back off (1,2,4..30 s), not 1/s.
8. `journalctl -k | grep -E 'usb .*disconnect|error -71'` empty over 10 minutes; ssh still works while USB audio streams.
9. Measure real USB latency loopback (not claimed here).

## 7. Rollback

- **Full:** `systemctl disable --now hypertrumpet-supervisor usbaudio-bridge; cp /usr/bin/bela-start-acm-ncm-rndis-old-gadget.stock /usr/bin/bela-start-acm-ncm-rndis-old-gadget; cp /usr/bin/bela-start-usb-gadgets.stock /usr/bin/bela-start-usb-gadgets; systemctl enable bela_startup` (only if you want Bela's own run-on-boot; set it with `make -C ~/Bela PROJECT=hyperTrumpet CL="-p 64" startuploop` so `startup_env` is rewritten); reboot. Remove `/etc/default/hypertrumpet`, `/root/ht-stage`, units if desired.
- **Drop only the dry channels:** set `HT_UAC2_P_CHMASK=3` in `/etc/default/hypertrumpet`, reboot (bridge follows automatically).
- **Drop USB audio entirely:** `systemctl disable --now usbaudio-bridge`; in the env add `USB_UAC2_DISABLED=yes` (stock script's own switch, sourced from `/etc/default/*`; put it in `/etc/default/hypertrumpet`), reboot. Network and serial stay.
- **Network lost (bad subnet):** restore from `/root/ht-net-backup/` over the ACM serial console, or reflash and rerun `install.sh`. A bad gadget script cannot be fixed over ssh (no USB): same recovery; the `.stock` copies are the fix.
- Reflash is always the last resort and loses nothing important: everything is regenerated by `install.sh`.

## 8. Every difference from hyperGuitar

| area | hyperGuitar (Gem Multi) | hyperTrumpet (Gem Stereo) |
|---|---|---|
| board audio | 10 in / 10 out; hex pickup on ch 2,3,4,5,8,9 | 2 in / 2 out; in0 bell mic, in1 contact mic 1 (PINMAP_V2); contact mic 2 on analog |
| project / binary / paths | `hyperGuitar6chPan`, `/root/hyperGuitar-supervisor.sh`, `hyperGuitar-live.log` | `hyperTrumpet`, `/root/hyperTrumpet-supervisor.sh`, `hyperTrumpet-live.log` |
| shm / flag names | `/hyperguitar_usbaudio`, `/run/hyperguitar-usbaudio-off` | `/hypertrumpet_usbaudio`, `/run/hypertrumpet-usbaudio-off` |
| USB audio to host | 2 ch (mix L/R) | 4 ch default (FX L/R + dry in0/in1), 2 via env |
| ring struct | duplicated in render.cpp + bridge.c ("must match") | one shared header; 4 lanes; acquire/release ordering on head/tail (aarch64) |
| bridge channels/rate | hard-coded 2 ch, 44100 | read from gadget configfs (`p_chmask`, `p_srate`), env overrides |
| bridge hw_params | results ignored | checked; retry on failure |
| hex-specific code | `sCapIn` diag (`/root/hg-capture-in` routes raw string 4/5 into the ring) | removed; dry taps are a first-class lane |
| gadget script | UAC2 hard-coded 44100 / mask 3 | env-driven (`/etc/default/hypertrumpet`); p mask 15; product string `hyperTrumpet-xxxx` (was `Bela-xxxx`) |
| supervisor | fixed 1 s relaunch, endless log, spins if binary missing | env block size, waits for binary, exponential backoff (<=30 s) if it dies in <20 s, log cap 20 MB, rc logged |
| unit | `hyperguitar-supervisor.service` | `hypertrumpet-supervisor.service` + `EnvironmentFile` |
| bridge unit | `After=sound.target` | adds `After=bela-usb-gadgets.service` (soft; bridge retries anyway) |
| startup_env | left as image default | set to `ACTIVE=0` so an accidental `bela_startup` enable cannot run anything |
| hostname / mDNS | `bela.local` | `hypertrumpet.local` (`--hostname`) |
| IP | 192.168.7.2 | default same; proposed 192.168.9.2 via `set-usb-subnet.sh` when co-plugged |
| installer | `restore.sh` (project-first, SD rebuild, mix.conf, build -p 64, card marker) | `install.sh` infra-first, project optional; no mix.conf; marker `/root/HYPERTRUMPET_CARD.txt` |
| block size | `-p 64` (floor; 32 underran) | `-p $HT_BLOCK` default 64, **unmeasured** for the trumpet chain |
| `-Jsettings.json` | always passed | passed only if `settings.json` exists `[arg carried over from hyperGuitar; not independently checked]` |
| `deploy.sh`, trellis, mix.conf, HGSET/HGMON UDP 9001 | present | not ported (project-level, not boot/USB infra) |
| kernel module patch (`usb_f_uac2.ko` rename attempt, 2026-07-26) | pristine .ko backed up | not ported; names are already configurable via configfs (`function_name`, `p_it_name`, ... present in hyperGuitar's snapshot); renaming is unneeded |
| hyperGuitar Bela's `/opt/Bela/local` hook | `bela_startup_local.sh` (historical) | `alt/` only |

## 9. Lessons carried over (hyperGuitar history)

- Never bind the bridge to `bela_startup` (`Wants`/`PartOf`): start-limit loop, 2026-09-01.
- `bela_startup` + supervisor together = 1/s crash storms, 2026-11-01. One mechanism only.
- The ring is recreated per instrument start; bridge must be bounced each time (otherwise silent until replug).
- Bridge xrun spam with no host attached is harmless; reopen backoff avoids hammering.
- A software restart does not clear a codec ESD latch; a cold power-cycle does. A DC-faulted input produces crackle/underrun storms, not a DSP bug (check log for `RAW-IN ... CODEC INPUT IS SICK`).
- Block 64 was the real-system floor for hyperGuitar; never size from a DSP-only bench; count underruns in the live log.
- Gadget mode flapping on hyperGuitar was a DSP mode-machine bug (pitch trackers), not USB; the USB side of "flapping" is the bridge reopen loop with no host app open.
- Bela power: an inline USB isolator starved the board (7 s boot loop, `-71`).
- Fresh card gotchas: no RTC (set clock; `restore.sh` set it from the Mac - not ported, add if builds complain about timestamps), new ssh host key (script uses `UserKnownHostsFile=/dev/null`).
- Never restart the instrument/supervisor or reboot a running Bela without Emory's go (applies to the trumpet rig too).

## 10. Verified vs unverified

Verified (sources): Gem Stereo = 2 audio in/out, 8 analog in, Debian 12 image, up to 96 kHz (learn.bela.io/get-started-guide/migrating-to-bela-gem; shop.bela.io Gem Stereo); `bela-usb-gadgets` enabled by the Gem image builder (BelaPlatform/gem-image-builder `target/chroot/bela.io-bookworm-arm64.sh`); `startup_env`/`make startup`/override hook (BelaPlatform/Bela Makefile, set_startup.sh, bela_startup.sh); UAC2 configfs attrs and 27-channel mask limit (upstream f_uac2.c, u_uac2.h, gadget-testing.rst); on-device attrs from hyperGuitar `configfs-snapshot.txt`.
Unverified: anything on the real Gem Stereo board; macOS handling of a 4-ch UAC2 input; Gem default sample rate; the Gem image's own `bela_startup.sh` content; how the Gem image assigns 192.168.7.2 and DHCP; `hostnamectl` behaviour on the image; `-p 64` adequacy; whether the shipped Gem Stereo image's stock gadget script matches hyperGuitar's 2026-03-25 one (install.sh diffs it and warns); that the Stereo build of the Bela runtime needs no extra flags vs Multi.
