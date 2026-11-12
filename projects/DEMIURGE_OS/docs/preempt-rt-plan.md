# PREEMPT_RT on the Pi 5 — implementation plan

**Status:** PLAN ONLY. Nothing in here has been executed. No package was installed, no
file on the Pi was modified. Every fact below marked "verified" was read off
`gorpi7@demiurge.local` or off the Raspberry Pi kernel source on 2026-08-12; everything
else is explicitly flagged **UNVERIFIED**.

**Target machine:** Raspberry Pi 5 Model B Rev 1.1, `ssh gorpi7@demiurge.local`, Debian
GNU/Linux 13 (trixie), kernel `6.18.29+rpt-rpi-2712` (`Debian 1:6.18.29-1+rpt1`, built
2026-05-12 with `aarch64-linux-gnu-gcc-14`).

---

## 1. The problem, restated with its evidence

At graph quantum **64** (device period 64 × 8) the rig is clean: physical-loopback
round-trip **6.458 ms**, device error counter frozen, csound at **0.26–0.71** of its
deadline.

At quantum **32** (device period 32 × 16) round-trip falls to **3.812 ms** — a real
**2.65 ms** win, which on a guitar instrument is the difference between "responsive" and
"transparent" — but the ALSA output node bleeds errors continuously:

| headroom | ALSA output errors / 45 s | node BUSY | budget | B/Q |
|---|---|---|---|---|
| 32 frames | 10 | 2.6–5.3 µs | 667 µs | 0.01 |
| 64 frames | 6 | 2.6–5.3 µs | 667 µs | 0.01 |

csound over the same window: **0.31–0.42** of deadline, error count flat.

**A node doing 1 % of its budget of work that still misses its deadline is not
CPU-starved — it is being woken late.** Doubling headroom 32 → 64 frames (0.67 → 1.33 ms
of slack) halved but did not eliminate the misses, so the *worst-case* wakeup lateness
sometimes exceeds **1.33 ms**. That is a scheduling/interrupt-latency number, not a DSP
number.

The cheap knobs are already turned. Verified on the running system:

```
/proc/cmdline: … threadirqs usbcore.autosuspend=-1 isolcpus=3 nohz_full=3 rcu_nocbs=3 …
ps -eo pid,rtprio,policy,comm:
    117     85 FF  irq/140-dw_axi_dmac_platform     <- the I2S DMA IRQ, already SCHED_FIFO 85
    118     85 FF  irq/131-xhci-hcd:usb1
```

So threaded IRQs are on, the audio DMA IRQ thread already runs at RT priority 85, a core
is isolated and tickless. What remains between "IRQ fires" and "PipeWire's ALSA node
runs" on a `CONFIG_PREEMPT` kernel is exactly the class of delay `PREEMPT_RT` exists to
remove: non-preemptible spinlock sections and softirq context in the rest of the kernel.
That is the hypothesis this plan tests.

**Current kernel is not RT** — verified:

```
$ uname -a
Linux demiurge 6.18.29+rpt-rpi-2712 #1 SMP PREEMPT Debian 1:6.18.29-1+rpt1 (2026-05-12) aarch64
$ grep -E '^CONFIG_PREEMPT' /boot/config-6.18.29+rpt-rpi-2712
CONFIG_PREEMPT_BUILD=y
CONFIG_PREEMPT=y
CONFIG_PREEMPT_COUNT=y
CONFIG_PREEMPTION=y
CONFIG_PREEMPT_RCU=y
```

`CONFIG_PREEMPT_RT` is absent. Low-latency, not real-time.

---

## 2. Phase −1 — do the free experiments first (30 min, zero risk)

Do not build a kernel until these are ruled out. Both are reversible with a reboot and
both attack the same "woken late" symptom. If either one takes quantum 32 to zero errors,
the whole rest of this document is unnecessary.

### 2.1 The isolated core is not being used by the audio IRQ

`isolcpus=3 nohz_full=3 rcu_nocbs=3` reserves CPU 3 — and then nothing audio-critical is
pinned there. Verified:

```
$ cat /proc/irq/140/smp_affinity_list
2
$ grep '^ *140:' /proc/interrupts
140:   32822815   0   1250467   0   rp1_irq_chip  40 Level  dw_axi_dmac_platform
```

IRQ 140 is the `dw_axi_dmac` that services the I2S/HiFiBerry transfer, and it is running
on **CPU 2** — a general-purpose core that also carries WirePlumber, the web UI, the touch
daemon, systemd, and everything else. CPU 3, the one core deliberately cleared of the
timer tick and RCU callbacks, sits unused by it.

Test (root, non-persistent, gone on reboot):

```bash
# move the audio DMA IRQ to the isolated core
echo 3 | sudo tee /proc/irq/140/smp_affinity_list
# pin the PipeWire process (which runs the ALSA nodes) to the same core
sudo taskset -cp 3 "$(pgrep -x pipewire)"
```

Then rerun the quantum-32 measurement in §7.2. Interpretation:

- Errors go to zero → the problem was contention on CPU 2, not preemption. Make it
  persistent (an `irqbalance` ban + a `systemd` drop-in setting `CPUAffinity=3` for
  `pipewire.service`, plus `IRQBALANCE_BANNED_CPULIST`) and stop here.
- Errors unchanged → contention was not the mechanism; the lateness is inside the kernel.
  Proceed.

Note the risk-free asymmetry: pinning to an isolated core costs nothing and is trivially
reverted, whereas a kernel rebuild costs a day. Rule it out first.

### 2.2 Get an objective, audio-independent latency number

`rt-tests` is in trixie and **not currently installed** (verified: `apt-cache policy
rt-tests` → `Installed: (none)`, `Candidate: 2.6-1.1`). Install it and take a **baseline
on the current PREEMPT kernel**, because without a before-number there is no way to say
whether RT helped for the right reason:

```bash
sudo apt install rt-tests
# 10 minutes, RT prio 80, on the isolated core, while the instrument is PLAYING
sudo cyclictest -m -S -p80 -i200 -d0 -D10m -h400 -q
```

Record `Max` (worst-case wakeup latency, µs) per core. The hypothesis predicts a `Max`
somewhere north of 1000 µs on at least one core, matching the 1.33 ms wall. If `Max`
comes back at, say, 120 µs while PipeWire is still missing 667 µs deadlines, then the
lateness is **not** generic scheduler latency and PREEMPT_RT is the wrong tool — see §8.

Keep the histogram. It is the single cleanest before/after artefact this whole exercise
produces.

---

## 3. Verification that the RT route exists at all (10 min, do before anything else)

### 3.1 Refuting the generic Debian RT kernel — with evidence, not assumption

`apt-cache search linux-image` on the Pi does offer Debian's generic PREEMPT_RT arm64
kernels (verified present in the trixie archive):

```
linux-image-6.12.73+deb13-rt-arm64-unsigned  - Linux 6.12 for 64-bit ARMv8 machines, PREEMPT_RT
linux-image-6.12.86+deb13-rt-arm64-unsigned  - (same, newer point release)
```

**Do not install these.** Two independent, verified reasons:

**(a) No HiFiBerry.** The machine driver currently binding the card exists only in the
Raspberry Pi kernel fork. Verified by HTTP status against the raw source trees:

```
torvalds/linux v6.12  sound/soc/bcm/hifiberry_dacplusadcpro.c -> 404
torvalds/linux v6.18  sound/soc/bcm/hifiberry_dacplusadcpro.c -> 404
raspberrypi/linux rpi-6.18.y  sound/soc/bcm/hifiberry_dacplusadcpro.c -> 200
```

And that is precisely the module in use right now (verified `lsmod`):

```
snd_soc_hifiberry_dacplusadcpro  49152  3
clk_hifiberry_dacpro             49152  1
```

`pcm512x`/`pcm186x` codec drivers are upstream, but the *machine* driver and the DAC-Pro
clock driver are not — and the `dtoverlay=hifiberry-dacplusadcpro` binding names them. A
generic kernel gives you `card 0/1: vc4hdmi` and no `card 2: sndrpihifiberry`. The
instrument has no audio path at all.

**(b) Probably no network, on 6.12.** RP1 — the southbridge carrying Ethernet, USB and
GPIO on the Pi 5 — was not in mainline 6.12. Verified:

```
torvalds/linux v6.12  drivers/pinctrl/pinctrl-rp1.c -> 404
torvalds/linux v6.18  drivers/pinctrl/pinctrl-rp1.c -> 200
```

The running kernel builds `CONFIG_PINCTRL_RP1=y`, `CONFIG_MFD_RP1=y`, `CONFIG_FIRMWARE_RP1=m`,
`CONFIG_RP1_PIO=m` (verified in `/boot/config-6.18.29+rpt-rpi-2712`); none of these
symbols can exist in a 6.12 tree that lacks the source files. Ethernet is a Cadence `macb`
behind RP1 — the MAC driver is upstream, but without RP1 clocks/pinctrl/MFD it has nothing
to sit on. The concern in the brief is therefore **confirmed**: expect boot-with-no-SSH.

Debian ships no `+deb13-rt` build newer than 6.12 as of this writing (verified: the trixie
archive lists 6.12.73 and 6.12.86 only) — **UNVERIFIED** whether trixie-backports carries
a 6.18-based RT kernel; even if it did, reason (a) still kills it.

### 3.2 Confirming CONFIG_PREEMPT_RT is selectable in the Pi's own 6.18 tree

This is the load-bearing check. Verified against `raspberrypi/linux` branch `rpi-6.18.y`
(head `ce0873cb5e0c24dd4730d87f4777982035733a0d` at time of writing):

```
kernel/Kconfig.preempt:89
    config PREEMPT_RT
        bool "Fully Preemptible Kernel (Real-Time)"
        depends on EXPERT && ARCH_SUPPORTS_RT && !COMPILE_TEST
        select PREEMPTION

arch/arm64/Kconfig:110
        select ARCH_SUPPORTS_RT
```

Both dependencies are satisfiable on this machine:

- `ARCH_SUPPORTS_RT` — selected unconditionally by `config ARM64`. Free.
- `EXPERT` — already `CONFIG_EXPERT=y` in the running `/boot/config-6.18.29+rpt-rpi-2712`
  **and** in `arch/arm64/configs/bcm2712_defconfig`. Free.
- `COMPILE_TEST` — not set in a real build.

**Conclusion: `CONFIG_PREEMPT_RT=y` is a one-symbol change in the Pi's own tree.** RT was
merged into mainline at 6.12 and this is a 6.18 tree; Raspberry Pi simply ships
`CONFIG_PREEMPT=y` in their build config. No patch set, no `-rt` patch series, no
out-of-tree anything. Same RP1 drivers, same HiFiBerry machine driver, same 371 overlays.

Reproduce the check yourself in one command before committing to the build:

```bash
curl -sL https://raw.githubusercontent.com/raspberrypi/linux/rpi-6.18.y/kernel/Kconfig.preempt \
  | grep -A3 '^config PREEMPT_RT'
curl -sL https://raw.githubusercontent.com/raspberrypi/linux/rpi-6.18.y/arch/arm64/Kconfig \
  | grep -n 'ARCH_SUPPORTS_RT'
```

and again, authoritatively, after the source is on disk (§5.3) — the tree on disk is the
only thing that actually matters.

---

## 4. Where to build: native on the Pi (recommended)

| | native on the Pi | cross-compile on the Mac |
|---|---|---|
| Toolchain | already correct: `gcc 14.2.0 (Debian 14.2.0-19)` — the *same major version* the rpt kernel was built with (verified) | needs an `aarch64-linux-gnu` cross GCC from Homebrew or a container |
| Filesystem | ext4, case-sensitive | **APFS is case-insensitive by default** — the kernel tree contains files differing only in case and headers that collide; builds fail in confusing ways unless you first create a case-sensitive APFS volume |
| Container escape hatch | n/a | `docker` is **not installed** on this Mac (verified `command -v docker` → not found); macOS 26.2 on arm64 could run an arm64 Linux VM (`container`, Lima, UTM, Docker Desktop) and build *natively* inside it |
| Resources | 4 cores, 15 GiB RAM, **100 GiB free on `/`** (verified) | faster CPU, but you then have to ship ~1 GiB of modules + image over the network |
| Module install | `make modules_install` writes straight to the right place | needs `INSTALL_MOD_PATH=` staging + rsync, one more chance to get a path wrong |
| Wall clock | **~1.5–3 h** for the full rpt config (1890 modules) — **UNVERIFIED estimate**, not measured on this board | maybe 20–40 min in a VM, plus setup time you will spend once |

**Recommendation: build natively on the Pi.** The tradeoff is a few hours of wall clock
against eliminating an entire class of failure (wrong toolchain, wrong page size, case
collisions, staged-module path errors). The Pi is idle-capable — the build does not
require stopping `demiurge.service`, though see the note below. There is no deadline
pressure here and the machine is a live instrument: the correctness of the artefact
matters far more than how long it takes to produce.

Two practical notes for the native build:

- **Thermals.** The board is overclocked (`arm_freq=2800`, `over_voltage_delta=50000` in
  `config.txt`) and idles at **56.5 °C** (verified). A 3-hour `-j4` compile is the hottest
  load this board will ever see. Watch `vcgencmd measure_temp` and
  `vcgencmd get_throttled`; if it throttles the build gets slower, not wrong.
- **Play while you build, or don't.** If you want the instrument usable, `nice -n 19 make
  -j3` and leave a core. If you want the build fast, stop `demiurge.service` and use
  `-j4`.

If you nonetheless want to build on the Mac: create a case-sensitive APFS volume in Disk
Utility (`APFS (Case-sensitive)`), or better, install an arm64 Linux VM and build inside
it as a native build. Do **not** attempt a cross-build directly on the default macOS
filesystem.

---

## 5. Build procedure

All commands run as `gorpi7` on the Pi unless marked `sudo`.

### 5.1 Prerequisites

Missing on the Pi right now (verified: `bc`, `bison`, `flex` absent; `gcc`, `make`,
`git`, `rsync`, `pahole` present):

```bash
sudo apt update
sudo apt install -y bc bison flex libssl-dev libncurses-dev build-essential \
                    libelf-dev dwarves
```

All are current in trixie (verified candidates: `bc 1.07.1-4+b1`, `bison 2:3.8.2+dfsg-1+b2`,
`flex 2.6.4-8.2+b4`, `libssl-dev 3.5.6-1~deb13u2+rpt1`, `libncurses-dev 6.5+20250216-2`).

Freeze the stock kernel so an incidental `apt upgrade` does not move your fallback under
you. Note the archive currently offers **1:6.18.33-1+rpt1** while you are running
**1:6.18.29-1+rpt1** (verified) — an upgrade would rewrite `kernel_2712.img`:

```bash
sudo apt-mark hold linux-image-rpi-2712 linux-image-6.18.29+rpt-rpi-2712 \
                   linux-headers-rpi-2712
```

(Release the holds once the experiment concludes, either way.)

### 5.2 Back up the things that are your rollback path

```bash
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.pre-rt
sudo cp /boot/firmware/kernel_2712.img /boot/firmware/kernel_2712.img.stock-6.18.29
# /boot/firmware has 438 MB free (verified) — the 10 MB copy fits
sudo cp /boot/config-6.18.29+rpt-rpi-2712 ~/rt-build-seed.config
```

`/lib/modules/6.18.29+rpt-rpi-2712` (1890 modules, verified) must be left untouched. The
new kernel gets its **own** module directory because it gets its own `LOCALVERSION` —
that is the whole reason the rollback is safe. **Never** build with `LOCALVERSION` that
resolves to `6.18.29+rpt-rpi-2712`.

### 5.3 Get the source at the right point

The Debian source package for the rpt kernel is not available: `archive.raspberrypi.com`
is configured `Types: deb` only, with no `deb-src` (verified in
`/etc/apt/sources.list.d/raspi.sources`). It is **UNVERIFIED** whether that archive
publishes sources at all; you may test it in one shot with
`sudo sh -c 'echo "deb-src http://archive.raspberrypi.com/debian/ trixie main" >
/etc/apt/sources.list.d/rpi-src.list' && sudo apt update && apt-get source linux` —
if it works, that is the byte-exact tree and you should prefer it. Otherwise use git:

```bash
cd ~
git clone --branch rpi-6.18.y --depth 2000 https://github.com/raspberrypi/linux.git rt-linux
cd ~/rt-linux
```

Now select the newest commit on `rpi-6.18.y` that is still at stable level 6.18.29 — i.e.
the same upstream stable base as the running kernel, with the Raspberry Pi patch series on
top. This is deterministic:

```bash
for c in $(git rev-list HEAD | head -2000); do
  if [ "$(git show $c:Makefile | sed -n 's/^SUBLEVEL = //p')" = "29" ]; then
    echo "base commit: $c"; git checkout "$c"; break
  fi
done
git log -1 --oneline
grep -E '^(VERSION|PATCHLEVEL|SUBLEVEL) =' Makefile   # expect 6 / 18 / 29
```

For reference the upstream stable commit `Linux 6.18.29` is
`d31a849ff5011dad5c271b53819a0b279e367d68` (verified via the GitHub commit search API
against `raspberrypi/linux`); the loop above finds the rpi-branch tip that still carries
it.

**Honest caveat:** this is *not* guaranteed byte-identical to what Raspberry Pi built
`1:6.18.29-1+rpt1` from — it is the same upstream stable level plus the same downstream
patch series, ± a handful of rpi commits. That is close enough for a kernel experiment and
is verified sufficient by the DTB comparison in §5.6. If you need exactness, use the
`deb-src` route above.

Alternative, if you would rather ride the tip: skip the loop, stay on `rpi-6.18.y` HEAD
(currently ~6.18.33, which matches the archive's *candidate* version). It is equally
valid; the only cost is that the DTB comparison in §5.6 is more likely to differ and push
you to the `os_prefix` install variant.

### 5.4 Seed the config from the kernel that is actually running

This is the step that "preserves the existing module/overlay set". Do **not** start from
`bcm2712_defconfig` — that is Raspberry Pi's upstream defconfig, not the Debian-packaged
`rpt` config, and it differs (its `CONFIG_LOCALVERSION="-v8-16k"`, and the two configs
have diverged in driver selection). Start from the exact config of the kernel booted right
now:

```bash
cd ~/rt-linux
cp /boot/config-6.18.29+rpt-rpi-2712 .config
make olddefconfig            # should be a no-op-ish; answers any new 6.18.x symbols
```

If for any reason the seed is unusable, the fallback is `make bcm2712_defconfig` — verified
to contain `CONFIG_ARM64_16K_PAGES=y`, `CONFIG_PREEMPT=y`, `CONFIG_EXPERT=y`, `CONFIG_HZ_250`
— but you will get a different module set and the overlay/driver parity claim weakens.

### 5.5 Enable RT — and change exactly one thing

```bash
cd ~/rt-linux
./scripts/config --enable  EXPERT
./scripts/config --disable PREEMPT_NONE
./scripts/config --disable PREEMPT_VOLUNTARY
./scripts/config --disable PREEMPT
./scripts/config --enable  PREEMPT_RT
./scripts/config --set-str LOCALVERSION "-rt-2712"
./scripts/config --disable LOCALVERSION_AUTO
make olddefconfig
```

Verify before burning three hours of CPU:

```bash
grep -E '^CONFIG_(PREEMPT_RT|PREEMPT|PREEMPT_RCU|EXPERT|LOCALVERSION|ARM64_16K_PAGES|HZ)=' .config
make kernelrelease        # expect: 6.18.29-rt-2712
```

Expected, and worth reading carefully:

- `CONFIG_PREEMPT_RT=y` — the point of the exercise.
- `CONFIG_PREEMPT` is now **unset** (it is a choice-group member; RT replaces it).
- `CONFIG_ARM64_16K_PAGES=y` must survive. The running kernel uses **16 KiB pages**
  (verified: `getconf PAGE_SIZE` → `16384`). Changing page size would introduce a second
  variable into an experiment that is supposed to isolate one.
- `CONFIG_HZ=250` — leave it. Changing HZ at the same time as PREEMPT_RT makes the result
  uninterpretable. If RT helps but not enough, HZ_1000 is a *follow-up* experiment.
- `CONFIG_MODVERSIONS=y` is inherited; fine, modules are built alongside.
- `CONFIG_DEBUG_INFO_NONE=y` is inherited (verified) — good, it keeps the build far
  faster and sidesteps BTF/pahole issues. `CONFIG_MODULE_SIG` and
  `CONFIG_SYSTEM_TRUSTED_KEYS` are not set in the seed (verified by their absence), so
  there is no signing key to supply.

`make kernelrelease` output is the module directory name; note it, you need it below.
Everything from here assumes `6.18.29-rt-2712`.

### 5.6 Build

```bash
cd ~/rt-linux
time make -j4 Image.gz modules dtbs
```

Then the check that decides your install variant — does this tree produce the same device
tree the firmware is already loading?

```bash
cmp arch/arm64/boot/dts/broadcom/bcm2712-rpi-5-b.dtb /boot/firmware/bcm2712-rpi-5-b.dtb \
  && echo "DTB IDENTICAL -> simple install (§6.1)" \
  || echo "DTB DIFFERS -> use the os_prefix variant (§6.3)"
```

If identical, you do not install DTBs or overlays at all and the stock kernel's boot
assets stay bit-for-bit untouched — the cleanest possible rollback.

---

## 6. Install, keeping the current kernel bootable

### 6.1 The simple variant (DTBs identical)

```bash
cd ~/rt-linux
sudo make modules_install                    # -> /lib/modules/6.18.29-rt-2712
sudo cp arch/arm64/boot/Image.gz /boot/firmware/kernel_2712_rt.img
ls -la /boot/firmware/kernel_2712*.img       # both must be present
ls -d /lib/modules/*                         # stock 6.18.29+rpt-rpi-2712 must still exist
```

**No initramfs is required.** Verified in the seed config: `CONFIG_EXT4_FS=y`,
`CONFIG_MMC=y`, `CONFIG_MMC_BLOCK=y`, `CONFIG_MMC_SDHCI=y`, `CONFIG_MMC_SDHCI_IPROC=y`,
`CONFIG_MMC_BCM2835=y` — the root filesystem and SD controller are all built in, and the
existing `initramfs_2712` contains only 3 modules. The RT kernel can mount root
unassisted, which also means the `auto_initramfs=1` line in `config.txt` is harmless: it
looks for an initramfs matching the kernel name, does not find one, and continues
(**UNVERIFIED** for a custom kernel filename — if the firmware instead errors out, the
symptom is an immediate boot stop with no kernel banner, recoverable exactly as in §7).

### 6.2 The `config.txt` edit

Current `/boot/firmware/config.txt` has **no `kernel=` line** (verified), so the firmware
loads its default `kernel_2712.img` on a BCM2712 board. Append at the very end of the
file, after the existing `[all]` block:

```ini
# --- PREEMPT_RT experiment, 2026-08-12 -------------------------------------
# Boots the self-built RT kernel (6.18.29-rt-2712, modules in
# /lib/modules/6.18.29-rt-2712). Stock 6.18.29+rpt-rpi-2712 remains installed
# as /boot/firmware/kernel_2712.img with its own module tree, untouched.
#
# ROLLBACK: put a '#' in front of the kernel= line below and reboot. The
# firmware falls back to its default kernel_2712.img. That is the entire
# rollback — nothing else was modified.
kernel=kernel_2712_rt.img
# ---------------------------------------------------------------------------
```

The exact rollback edit, i.e. the one line you will type on a serial console or on a
laptop with the SD card mounted:

```ini
#kernel=kernel_2712_rt.img
```

Do not delete `kernel_2712.img`, do not delete `/lib/modules/6.18.29+rpt-rpi-2712`, and do
not release the apt holds until you are done. Those three things are the rollback.

### 6.3 The `os_prefix` variant (only if DTBs differ)

If §5.6's `cmp` reported a difference, the RT kernel may want its own device tree, and
overwriting `/boot/firmware/*.dtb` would change what the *stock* kernel boots with too —
that breaks the "current kernel stays bootable" guarantee. Isolate instead:

```bash
sudo mkdir -p /boot/firmware/rt/overlays
sudo cp arch/arm64/boot/Image.gz              /boot/firmware/rt/kernel_2712.img
sudo cp arch/arm64/boot/dts/broadcom/*.dtb    /boot/firmware/rt/
sudo cp arch/arm64/boot/dts/overlays/*.dtbo   /boot/firmware/rt/overlays/
sudo cp arch/arm64/boot/dts/overlays/README   /boot/firmware/rt/overlays/
```

and in `config.txt` use, instead of the `kernel=` line:

```ini
os_prefix=rt/
```

`os_prefix` redirects kernel, DTB and (via the default `overlay_prefix=overlays/`)
overlays into that subdirectory as a set — rollback is still one commented line. Check
free space first: `/boot/firmware` has **438 MB free** and the overlay set is 371 files
(verified), so it fits comfortably. **UNVERIFIED**: exact `os_prefix` + `overlay_prefix`
interaction on current firmware; confirm against
`/boot/firmware/overlays/README` and the official `config.txt` documentation before use.

---

## 7. Recovery — read this section before you start, not after

The realistic failure mode is **"it boots, but there is no network"**, and its cousin
**"it does not boot at all and prints nothing"**. Assume you will hit one of them.

### 7.1 What to have physically in hand BEFORE the first RT reboot

Ranked by how much grief they save:

1. **Serial console cable — get this one.** The Pi 5 has a dedicated 3-pin debug UART
   connector (JST-SH 1.0 mm, between the HDMI ports), which is what the kernel is already
   using: `/boot/firmware/cmdline.txt` sets `console=serial0,115200`, `/proc/cmdline`
   resolves that to `console=ttyAMA10`, `/dev/ttyAMA10` exists, and
   `serial-getty@ttyAMA10.service` is `enabled-runtime` (all verified). So a login prompt
   is *already waiting* on that port with no config change needed. Use the official
   **Raspberry Pi Debug Probe** (ships with the correct 3-pin JST-SH cable) or any 3.3 V
   USB-TTL adapter wired TX↔RX / RX↔TX / GND. From the Mac:
   `screen /dev/tty.usbmodem* 115200` (Debug Probe exposes a CDC-ACM port) or
   `screen /dev/tty.usbserial-* 115200`.
   This is the only tool that shows you the *kernel boot messages* — which is what you
   need if the kernel panics or the RP1 driver fails to probe.
   **UNVERIFIED**: whether GPIO 14/15 also carry a console on this configuration; the
   dedicated debug connector definitely does.
2. **HDMI monitor + USB keyboard.** `console=tty1` is also on the kernel command line
   (verified), so the boot log and a getty appear on HDMI. Enough to comment out one line
   in `config.txt` and reboot. Cheaper than a serial cable, but the display is only brought
   up once `vc4-kms-v3d` probes — if the failure is earlier than that, you see nothing,
   which is exactly when you wish you had the serial cable.
3. **SD card reader for the Mac — the guaranteed backstop.** The boot partition is FAT
   (`/dev/mmcblk0p1`, 505 MB, mounted at `/boot/firmware`) and mounts on macOS. Power off,
   pull the card, mount it, edit `config.txt` — the file is plain text and the rollback is
   one `#`. This works no matter how badly the kernel fails, because the fix happens
   before any kernel runs.

**Minimum acceptable kit: the SD card reader.** With only that, worst case is a power-off,
a card swap and five minutes. **Recommended kit: SD reader + serial cable**, because the
reader gets you *back*, but only the serial console tells you *why* — and without the why
you have no next move.

### 7.2 Recovery procedures

**Case A — boots, but no SSH.** Attach HDMI+keyboard or serial, log in as `gorpi7`, then
either fix forward (`ip a`, `dmesg | grep -i -E 'rp1|macb|eth'`) or roll back:
`sudo nano /boot/firmware/config.txt`, comment the `kernel=` line, `sudo reboot`.

**Case B — kernel panics / no login anywhere.** Serial console shows the panic. Roll back
via Case C.

**Case C — nothing at all on any console.** Power off at the wall. Move the SD card to the
Mac. The `bootfs` volume mounts; open `config.txt` in any editor; comment the `kernel=`
line (or `os_prefix=`); eject; card back in the Pi; power on. You are back on
`6.18.29+rpt-rpi-2712`, which never moved.

**Case D — boots RT but audio is broken / card missing.** Check `aplay -l` for
`card 2: sndrpihifiberry` and `lsmod | grep hifiberry`. If the module is missing, the most
likely cause is a module-tree mismatch (`uname -r` not matching a directory under
`/lib/modules`) — verify `ls -d /lib/modules/$(uname -r)` exists. Roll back if unclear;
this is an experiment, not a rescue mission.

Do **not** attempt any of this remotely from another city. Be at the machine.

---

## 8. Verifying RT is active, and whether it actually helped

### 8.1 Is it RT?

```bash
uname -a
# expect: Linux demiurge 6.18.29-rt-2712 #1 SMP PREEMPT_RT <date> aarch64 GNU/Linux
#                                              ^^^^^^^^^^^ this word is the whole check
uname -r                       # 6.18.29-rt-2712
ls -d /lib/modules/$(uname -r) # must exist
```

Sanity-check that the rig came back intact before measuring anything:

```bash
aplay -l | grep -i hifiberry           # card 2: sndrpihifiberry ... DAC+ADC Pro
lsmod | grep -E 'hifiberry|rp1'
ip -br a                                # network up (you are on SSH, so: yes)
systemctl --user is-active pipewire
ps -eo pid,rtprio,policy,comm | grep 'irq/140'   # DMA IRQ thread still SCHED_FIFO
```

Under RT, expect *more* `irq/*` threads and `ktimers/*` threads than before — that is
`PREEMPT_RT` forcing IRQ threading globally rather than relying on the `threadirqs`
command-line flag. `threadirqs` on the cmdline becomes redundant but harmless.

### 8.2 Did it help? — the measurement that decides it

Three numbers, in this order. **Change nothing else between the before and after runs.**

**(1) `cyclictest`, the mechanism check** — rerun exactly the §2.2 command and compare
`Max` against the baseline you took on the PREEMPT kernel. This says whether RT did what
RT does. A drop from ~1000+ µs to tens of µs is the expected shape.

**(2) The ALSA output node error counter at quantum 32 — the verdict.** Set
`quantum = 32` in `~/demiurge/live.conf` (currently `quantum = 64`, verified), with the
device period at 32 × 16 and headroom back at **32** (the harder of the two settings, so
the comparison is against the 10-errors/45 s baseline), then:

```bash
pw-top -b -n 90 | grep 'alsa_output.platform-soc_107c000000_sound.pro-output-0'
```

That node name is verified live on the machine. `pw-top -b` prints one batch line per
node per refresh; watch the **ERR** column over a full 45 s window while the instrument is
being *played*, not idle. The result is binary:

- **ERR flat at its starting value for 45 s → RT solved it.** Quantum 32 is now usable and
  the rig gains 2.65 ms of round-trip. Then, and only then, try dropping headroom back to
  32 if you raised it, and re-measure.
- **ERR still climbing → RT did not solve it.** See §9.

Also record the `B/Q` of the same node (baseline 0.01) and of `csound6` (baseline
0.31–0.42). If csound's `B/Q` has climbed materially under RT, that is the RT throughput
tax showing up and is worth writing down.

**(3) The physical loopback round-trip — the number that matters to the player.** With the
engine stopped and a physical output→input cable on the HiFiBerry:

```bash
systemctl --user stop demiurge.service    # engine stopped, per the calibrate contract
~/testing/latency/latency.sh calibrate
~/testing/latency/latency.sh table
```

(`latency.sh` verified present at `/home/gorpi7/testing/latency/latency.sh`; `calibrate` is
its documented one-time-per-interface physical-loopback mode, which stores the measured
hardware offset.) Expected at a clean quantum 32: **~3.8 ms**, versus **6.458 ms** at
quantum 64.

Log all three into the repo alongside the existing latency evidence, before *and* after.
A result nobody wrote down is a result you will re-litigate in three months.

---

## 9. Expected gain, stated honestly

**If the hypothesis is right:** PREEMPT_RT converts the remaining non-preemptible kernel
sections into preemptible, priority-inherited ones, worst-case wakeup lateness drops from
the ~1.33 ms+ implied by the current error rate to the tens-of-µs range typically seen on
ARM SoCs under RT, and quantum 32 runs clean. The payoff is **~2.65 ms off round-trip
(6.458 → 3.812 ms)** — a real, audible improvement in feel for a guitar instrument, and
arguably the last big block of latency left after the direct-output work already removed a
quantum from the graph.

**The cost, which is not zero:** RT trades throughput for determinism. Threaded interrupts
and sleeping spinlocks add overhead; typical figures are single-digit percent, and csound's
`B/Q` will likely rise from 0.31–0.42 to somewhat higher. At 0.4 there is room; if RT
pushes it past ~0.7 at quantum 32, you have traded one deadline problem for another and
the honest answer is to stay at quantum 64. The NAM neural amp model is the heaviest thing
in the graph and is the most likely to notice. Measure it (§8.2 step 2), do not assume.

**Also non-zero: you now maintain a kernel.** Every rpt kernel update leaves your RT build
behind. You must keep the apt holds, or accept re-running §5 for each upgrade. That is a
real recurring cost for a live instrument and is a legitimate reason to decide the 2.65 ms
is not worth it.

**If RT does not fix it** — ERR keeps climbing at quantum 32 with `cyclictest` `Max` now
tiny — that is not a wasted result, it is a *disproof*, and it relocates the problem
precisely: generic scheduler latency was never the mechanism. The remaining candidates,
roughly in order of plausibility:

1. **PipeWire's own wakeup estimation.** PipeWire wakes on a timerfd derived from its
   estimate of the DMA pointer, not on the interrupt itself. At 32 frames / 667 µs the
   *estimation error* can be a meaningful fraction of the period, and no kernel change
   fixes an inaccurate estimate. Test: force IRQ-driven timing / adjust
   `api.alsa.period-size` and `api.alsa.headroom` in the device rules rather than the
   graph quantum.
2. **DMA/period granularity in `dw_axi_dmac` + `designware_i2s`.** A 32-frame period is
   256 bytes per channel pair; if the controller's practical minimum burst or its
   descriptor turnaround exceeds that, the hardware simply cannot be serviced that finely,
   whatever the scheduler does.
3. **Clock-domain resync.** The HiFiBerry DAC-Pro runs its own clock
   (`clk_hifiberry_dacpro`, loaded — verified) while the graph driver runs off the device;
   resync events land as periodic hiccups that look exactly like lateness.
4. **Bus contention** from SD/USB DMA against the audio DMA on the same fabric.

In that case the pragmatic answer is: **stay at quantum 64** (6.458 ms, clean, proven), or
run quantum 32 at headroom 96–128, which reclaims part of the 2.65 ms while restoring the
margin. A clean 6.458 ms beats a glitching 3.812 ms on stage every single time.

---

## 10. Time estimate and points of no return

| Phase | What | Est. wall clock |
|---|---|---|
| −1 | IRQ affinity / CPU-pinning experiments + `cyclictest` baseline (§2) | 45 min |
| 0 | Assemble recovery kit (§7.1); order a serial cable if you lack one | 15 min (+ shipping) |
| 1 | Verify `CONFIG_PREEMPT_RT` availability, refute the generic route (§3) | 10 min |
| 2 | Install build deps, apt holds, backups (§5.1–5.2) | 15 min |
| 3 | Clone + select base commit (§5.3) | 15–30 min (network-bound) |
| 4 | Seed config, enable RT, verify `.config` (§5.4–5.5) | 15 min |
| 5 | **Build** (§5.6) | **1.5–3 h — UNVERIFIED estimate** |
| 6 | `modules_install`, copy image, edit `config.txt` (§6) | 10 min |
| 7 | Reboot + verify RT active and rig intact (§8.1) | 5 min, or 30–60 min if recovery |
| 8 | Measurement: cyclictest, `pw-top` ERR at Q32, loopback calibrate (§8.2) | 45 min |

Total: **one comfortable day**, most of it unattended compile.

**Points of no return: there are none, by construction — as long as three things stay
true.**

1. `/boot/firmware/kernel_2712.img` is never overwritten (that is why the RT image is
   named `kernel_2712_rt.img`, and why §5.2 keeps a `.stock-6.18.29` copy anyway).
2. `/lib/modules/6.18.29+rpt-rpi-2712` is never overwritten (that is why `LOCALVERSION`
   must be `-rt-2712` and why `make kernelrelease` is checked *before* the build).
3. The existing `*.dtb` and `overlays/` in `/boot/firmware` are never overwritten (that is
   why §5.6 compares DTBs and §6.3 exists).

Break any of those three and the rollback stops being one commented line. Keep them and
the worst outcome in this entire plan is: power off, move the SD card to a laptop, type
one `#`, put it back.

The only genuinely irreversible risk on the table is physical — pulling the SD card or the
HAT from a powered board. Power down at the wall first, and while the board is open,
confirm the HiFiBerry is fully and evenly seated on the header before applying power
again.
