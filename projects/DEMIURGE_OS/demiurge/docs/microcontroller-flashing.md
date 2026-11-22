# Microcontroller flashing — over SSH, no button press

A Teensy **or a Raspberry Pi Pico** plugged into the Pi can be reflashed entirely over SSH. No physical access to the program / BOOTSEL button, no moving the USB cable to a laptop. Edit the firmware anywhere, compile it, ship the file to the Pi, run one command.

- **Teensy** → `demiurge-flash-teensy firmware.hex` (see below)
- **Pico / Pico 2 (RP2040 / RP2350)** → `demiurge-flash-pico firmware.uf2` (see [Pico](#pico-rp2040--rp2350) at the end)

Both use the identical trick: a magic baud rate on the board's USB serial port makes the running firmware reboot into its bootloader, then the matching uploader flashes it. Only the constants differ.

This is how the Arduino IDE has always worked under the hood — DEMIURGE just exposes it as a Pi-side workflow.

## How it works

Two pieces of plumbing make this possible. DEMIURGE wires both in `setup-demiurge.sh`:

1. **`teensy-loader-cli`** — the same uploader Arduino IDE bundles, installed from apt. Talks to HalfKay (Teensy's bootloader) over libusb and pushes a `.hex`.

2. **`demiurge-flash-teensy`** — wrapper at `/usr/local/bin/`. Sends the *soft-reboot signal* the running Teensy firmware listens for, waits for HalfKay to enumerate, then invokes `teensy_loader_cli`.

The soft-reboot signal is the trick. Any Teensyduino firmware compiled with a USB mode that includes Serial (Serial, Serial+MIDI, Serial+Keyboard+Mouse+Joystick, etc.) has a hook in its USB stack: **set the line baud rate to 134, and the firmware jumps to HalfKay**. This is in the Teensy core itself — you do not write it. Arduino IDE's upload button does exactly this:

```sh
stty -F /dev/ttyACM0 134
```

That one line is the entire "remote reset" magic. The Teensy disappears from USB and reappears as HalfKay (VID:PID `16c0:0478`). `teensy_loader_cli -w` polls for HalfKay and flashes the moment it sees it. When the flash completes, HalfKay reboots into the new firmware automatically and the device re-enumerates back to its original USB mode. DEMIURGE's hot-plug picks the MIDI side back up; the audio graph keeps playing.

If your firmware is in a USB mode *without* Serial (pure MIDI, pure HID, pure raw-USB) you lose this trick — there's no serial endpoint to send baud 134 to. The fix is to add Serial to the USB mode in your sketch — it does not cost you anything on the MIDI side, and you get remote flashing forever. (Going forward, build NEPTR-class firmware in Serial+MIDI, never plain MIDI.)

## The workflow

The Pi never needs an Arduino toolchain. Compile somewhere — your laptop is fine — and ship the `.hex`.

```sh
# 1. Edit firmware locally. The .ino lives wherever you keep it (it does not
#    need to be on the Pi; DEMIURGE only cares about the .hex).

# 2. Compile to a .hex. Arduino IDE 2 bundles arduino-cli; use it headless:
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" \
    compile --fqbn 'teensy:avr:teensy41:usb=serialmidi,speed=600,opt=o2std,keys=en-us' \
    --output-dir /tmp/build path/to/sketch
# On Linux, install arduino-cli + the Teensy core via the Boards Manager and
# the command is the same minus the absolute path.

# 3. Ship the .hex to the Pi.
scp /tmp/build/firmware.ino.hex demiurge.local:~/firmware.hex

# 4. Flash. The Teensy can be running, doing audio, controlling NEPTR — it
#    does not matter. The wrapper handles soft-reboot and flash.
ssh demiurge.local "demiurge-flash-teensy ~/firmware.hex"
```

Default MCU is `TEENSY41`. Override for other boards:

```sh
ssh demiurge.local "demiurge-flash-teensy ~/firmware.hex --mcu=TEENSY40"
```

## What the wrapper actually does

```
demiurge-flash-teensy firmware.hex
    │
    ├── find /dev/ttyACMx with idVendor=16c0      ← locate the Teensy
    ├── lsusb | grep 16c0:0478                    ← already in HalfKay? skip reboot
    ├── stty -F /dev/ttyACMx 134                  ← soft-reboot to HalfKay
    ├── poll lsusb for 16c0:0478, up to 5 s       ← wait for bootloader
    └── teensy_loader_cli --mcu=TEENSY41 -w -v firmware.hex   ← upload
```

The `-w` flag means "wait for HalfKay if not present yet" — fallback if the soft-reboot signal didn't land (you can still flash by holding the program button when you plug the Teensy in).

## Permissions

The teensy-loader-cli apt package installs `/lib/udev/rules.d/49-teensy.rules`, which grants access via `TAG+="uaccess"`. That mechanism only works for users logged in to a graphical seat through systemd-logind. **SSH sessions are not logged in to a seat**, so without supplemental rules `teensy_loader_cli` fails over SSH with `Unable to claim interface, check USB permissions`.

`setup-demiurge.sh` installs `/etc/udev/rules.d/99-demiurge-teensy.rules` which grants `plugdev` group access to every Teensy USB mode, including HalfKay. The default user (`gorpi7`) is in plugdev, so over-SSH flashing works without `sudo`.

If you create a new user that needs to flash:

```sh
sudo usermod -aG plugdev <user>
```

(Log out and back in for the group to take effect.)

## When the soft-reboot fails

The wrapper prints `HalfKay did not appear after soft-reboot`. Causes:

- **Firmware USB mode has no Serial endpoint.** Rebuild with Serial included (e.g. switch from "MIDI" to "Serial + MIDI" in Tools → USB Type, or pass `usb=serialmidi` to arduino-cli). One-time pain; permanent fix.
- **Firmware has crashed or hung in `setup()`** before the USB stack is up. The wrapper falls through to `teensy_loader_cli -w`, which polls forever — press the program button on the board once, and the flash proceeds.
- **`/dev/ttyACM*` is busy** with an exclusive open from another process. Nothing on DEMIURGE holds the serial endpoint exclusively (csound uses ALSA seq for MIDI, not raw serial), so this shouldn't happen in practice. If it does: `sudo lsof /dev/ttyACM0` to find the owner.

## What happens to the audio graph

The Teensy disappears for ~1 second during the flash and reappears as MIDI. DEMIURGE's MIDI hot-plug rule (Phase 4 of the installer) auto-rewires the new MIDI client to `Midi Through` (client 14:0), so csound / ChucK / Pd / anything else on the bus picks the controller back up without any manual reconnect. Audio keeps playing through the gap.

## Why not just put the toolchain on the Pi?

You can — `arduino-cli` runs fine on aarch64 and the Teensy core supports it. But the toolchain is ~500 MB and rarely needed; DEMIURGE keeps the Pi lean and lets you compile on whatever machine you already have, since the `.hex` is portable.

If you do want full Pi-side iteration, install arduino-cli and the Teensy core on the Pi the same way you would on a laptop, then compile in place — `demiurge-flash-teensy` works regardless of where the `.hex` was produced.

---

## Pico (RP2040 / RP2350)

Everything above is written for the Teensy, but the Pico works the same way with different constants. DEMIURGE ships a parallel wrapper, `demiurge-flash-pico`, and the matching pieces of plumbing.

| | Teensy | Pico / Pico 2 |
|---|---|---|
| Uploader (apt) | `teensy-loader-cli` | `picotool` |
| Wrapper | `demiurge-flash-teensy` | `demiurge-flash-pico` |
| Firmware file | `.hex` | `.uf2` (also `.elf` / `.bin`) |
| USB VID | `16c0` | `2e8a` |
| Bootloader | HalfKay (`16c0:0478`) | BOOTSEL / `RPI-RP2` (`2e8a:0003` RP2040, `2e8a:000f` RP2350) |
| Remote-reset baud | **134** | **1200** |
| udev rule | `99-demiurge-teensy.rules` | `99-demiurge-pico.rules` |

### The trick

Touching the Pico's USB serial port at **baud 1200** makes the running firmware reboot into the BOOTSEL UF2 bootloader — the exact analogue of the Teensy's baud-134 jump to HalfKay. This is built into the Arduino-Pico core, PlatformIO, MicroPython, and CircuitPython; you do not write it. The wrapper does it for you:

```sh
stty -F /dev/ttyACM0 1200      # Pico re-enumerates as RPI-RP2 (2e8a:0003 / 000f)
picotool load -x firmware.uf2  # load and execute
```

Your firmware **must present a USB serial (CDC) port** for the remote reset to work — the same caveat as the Teensy needing Serial in its USB mode. Arduino-Pico's default USB stack does; a bare pico-sdk build only does if you enabled `pico_enable_stdio_usb`. If there is no serial port, hold the BOOTSEL button while plugging the Pico in and re-run — `picotool` flashes whatever is already in BOOTSEL.

### The workflow

```sh
# 1. Compile to a .uf2 somewhere (laptop is fine). For Arduino-Pico via
#    arduino-cli the FQBN is rp2040:rp2040:rpipico (or :rpipico2 for Pico 2):
arduino-cli compile --fqbn rp2040:rp2040:rpipico \
    --output-dir /tmp/build path/to/sketch

# 2. Ship the .uf2 to the Pi.
scp /tmp/build/sketch.ino.uf2 demiurge.local:~/firmware.uf2

# 3. Flash. The Pico can be running — the wrapper handles reset + load.
ssh demiurge.local "demiurge-flash-pico ~/firmware.uf2"
```

### Permissions

`setup-demiurge.sh` installs `/etc/udev/rules.d/99-demiurge-pico.rules`, which grants `plugdev` group access to every `2e8a` device (running firmware *and* the BOOTSEL endpoint), so over-SSH flashing works without `sudo` for any plugdev user — same as the Teensy. If you add a new user: `sudo usermod -aG plugdev <user>` (then re-login).
