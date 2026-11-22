Bela on-device backup — taken 2026-07-26, before attempting an f_uac2 kernel
module patch to fix the "Playback Inactive"/"Capture Inactive" USB audio
naming issue (those strings are hardcoded in the stock driver's C source).

Purpose: fast local restore path if the patched kernel module misbehaves,
without needing a full SD card reflash.

Device: root@bela.local / root@192.168.7.2 (ssh key auth)
Kernel running at backup time: 6.12.49-ti-arm64-r55-evl-2
Build tools on-device: gcc and make ARE present.
Kernel build dir (/lib/modules/<ver>/build) is MISSING - there is no
matching kernel headers/source tree on-device, so the standard
`make -C /lib/modules/$(uname -r)/build M=$(pwd) modules` workflow will
NOT work as-is. Whoever attempts the patch needs to either obtain a
matching kernel source tree/headers package, or cross-compile from a
build machine with the exact same kernel config, or find another route
to build a loadable module for this exact kernel version.

Files in this directory, with their exact original device path and the
command used to fetch them:

- bela-start-acm-ncm-rndis-old-gadget
  from: /usr/bin/bela-start-acm-ncm-rndis-old-gadget
  cmd:  scp root@bela.local:/usr/bin/bela-start-acm-ncm-rndis-old-gadget ...
  This is the PERSISTENT boot script that assembles the g_multi USB
  gadget (network/serial/MIDI + our added UAC2 audio function) on every
  boot. This already has the UAC2 addition baked in from earlier this
  session.

- bela-start-usb-gadgets
  from: /usr/bin/bela-start-usb-gadgets
  cmd:  scp root@bela.local:/usr/bin/bela-start-usb-gadgets ...
  Wrapper script invoked by the bela-usb-gadgets.service systemd unit;
  calls the script above.

- usbaudio-bridge.service
  from: /etc/systemd/system/usbaudio-bridge.service
  cmd:  scp root@bela.local:/etc/systemd/system/usbaudio-bridge.service ...
  Systemd unit for the non-realtime bridge process that feeds the UAC2
  gadget from hyperGuitar's shared-memory ring buffer. Has PartOf=
  bela_startup.service so it auto-restarts whenever hyperGuitar redeploys.

- usbaudio-bridge
  from: /usr/bin/usbaudio-bridge
  cmd:  scp root@bela.local:/usr/bin/usbaudio-bridge ...
  The compiled bridge binary itself (built from bela/usbaudio-bridge/bridge.c
  in the git repo - that source is already safe in git, this is just the
  compiled artifact as currently deployed).

- hyperGuitar6chPan-device-copy/render.cpp
  from: ~/Bela/projects/hyperGuitar6chPan/render.cpp (root's home on device)
  cmd:  scp root@bela.local:~/Bela/projects/hyperGuitar6chPan/render.cpp ...
  Point-in-time snapshot of the actual deployed source. Should match the
  git repo's bela/hyperGuitar6chPan/render.cpp, but this is the device's
  own copy at backup time, not re-derived from git.

- hyperGuitar6chPan-device-copy/settings.json
  from: ~/Bela/projects/hyperGuitar6chPan/settings.json (root's home on device)
  cmd:  scp root@bela.local:~/Bela/projects/hyperGuitar6chPan/settings.json ...

- usb_f_uac2.ko.xz.PRISTINE-BACKUP  ***MOST IMPORTANT FILE***
  from: /lib/modules/6.12.49-ti-arm64-r55-evl-2/kernel/drivers/usb/gadget/function/usb_f_uac2.ko.xz
  cmd:  scp root@bela.local:/lib/modules/.../usb_f_uac2.ko.xz "$BK/usb_f_uac2.ko.xz.PRISTINE-BACKUP"
  This is the STOCK, known-good, currently-loaded kernel module for the
  UAC2 USB audio gadget function, exactly as shipped on this board's SD
  card, still XZ-compressed as found on-device. If a patched module ever
  needs to be rolled back, this is the file to restore to
  /lib/modules/6.12.49-ti-arm64-r55-evl-2/kernel/drivers/usb/gadget/function/usb_f_uac2.ko.xz
  (restore by copying it back to that exact path, then `depmod -a` and
  reboot, or `rmmod`/`modprobe` cycle if doing it live).

- configfs-snapshot.txt
  from: recursive dump of /sys/kernel/config/usb_gadget/g_multi/*
  cmd:  ssh root@bela.local "find /sys/kernel/config/usb_gadget/g_multi \
        -type f -exec sh -c 'echo \"=== {} ===\"; cat {} 2>/dev/null' \;"
  Plain-text record of every live ConfigFS attribute value at backup
  time (sample rates, channel masks, string names, etc.) - not a binary
  backup, just a reference in case any setting needs to be manually
  reconstructed by hand later. First few lines are the SSH login banner
  text, not part of the actual dump - ignore those.

Nothing on the device was modified, deleted, or restarted to produce
this backup - every step above was a read-only fetch (scp/cat). The
board was left exactly as it was found.
