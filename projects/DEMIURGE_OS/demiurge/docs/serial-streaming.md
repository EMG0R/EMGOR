# Watching Teensy serial telemetry — on your laptop, over SSH

A NEPTR-class Teensy plugged into the Pi prints a structured telemetry frame
over USB serial 100 times a second. You can watch that stream live in your
laptop terminal without moving the USB cable, installing anything, or using
`sudo`. One SSH pipe does it.

This is the read-only sibling of `microcontroller-flashing.md`: that doc pushes
firmware *to* the Teensy over SSH; this one pulls telemetry *back*.

## The topology

```
Teensy 4.1  ──USB──▶  Pi (/dev/ttyACM0)  ──SSH──▶  your laptop terminal
  prints                  raw serial            ssh + cat
  NEPTR2 frames           115200 baud
  @ 100 Hz
```

The Teensy never touches the laptop. It enumerates on the Pi as
`/dev/ttyACM0` (USB type Serial+MIDI, 115200 baud) and emits one newline-
terminated frame every 10 ms. Each line starts with `NEPTR2 ` followed by
space-separated `key=value` fields — encoder deltas, button states, joystick,
expression, FSRs, ToF distances, accelerometer, mic envelopes. The exact field
list is documented at the top of `TEENSYY/TEENSYY.ino`. A sample frame:

```
NEPTR2 t=134037 e1=0 ... jx=2054 jy=2081 exp=3205 fsr1=5 fsr2=5 tof1=134 tof2=1957 ax=-1610 ay=-656 az=-17851 ml=5 mr=7
```

## How it works

USB CDC serial on the Pi is just a character device. Two commands turn it into
a stream you can read:

```sh
stty -F /dev/ttyACM0 raw 115200 -echo   # put the port in raw 115200, no echo
cat /dev/ttyACM0                          # read bytes until you Ctrl-C
```

`stty` configures the line discipline so `cat` sees the Teensy's bytes verbatim
(raw mode = no canonical processing, `-echo` = don't echo anything back to the
Teensy). Run both over SSH and the frames land in your terminal:

```sh
ssh gorpi7@demiurge.local "stty -F /dev/ttyACM0 raw 115200 -echo && exec cat /dev/ttyACM0"
```

That's the whole mechanism. No pyserial, no bridge, no daemon — `cat` reading a
tty is the simplest reliable serial monitor there is.

## The workflow

`setup-demiurge.sh` installs that incantation as `/usr/local/bin/demiurge-stream-teensy`
(the read-only sibling of `demiurge-flash-teensy`). It auto-detects the Teensy
port (first `/dev/ttyACM*` with USB vendor `16c0`) and raw-cats it. So the whole
thing from the laptop is one command:

```sh
ssh demiurge.local demiurge-stream-teensy
ssh demiurge.local demiurge-stream-teensy /dev/ttyACM1   # override port
```

The laptop-side `./scripts/stream_teensy_serial.sh` in the NEPTR phase4 repo is a
thin convenience around that — it calls the wrapper if present, falls back to the
inline `stty`+`cat` if not, and uses `ssh -t` so Ctrl-C tears the remote reader
down cleanly instead of leaving a `cat` holding the port:

```sh
./scripts/stream_teensy_serial.sh                 # default Pi + /dev/ttyACM0
./scripts/stream_teensy_serial.sh user@host        # override target
./scripts/stream_teensy_serial.sh user@host /dev/ttyACM1   # override port too
```

Pipe it into the usual tools — `grep` for a field, `awk` to pull a column:

```sh
./scripts/stream_teensy_serial.sh | grep --line-buffered 'tof1='
```

## No sudo

`/dev/ttyACM0` is `crw-rw---- root plugdev`. The default Pi user `gorpi7` is in
the `plugdev` group (the same membership that makes over-SSH *flashing* work —
see `microcontroller-flashing.md`), so it can both `stty` and `cat` the port
with no elevation. If a new user can't read the port, add them to plugdev:

```sh
sudo usermod -aG plugdev <user>   # then log out and back in
```

## Gotcha: port contention with the bridge

The serial port is a **single-reader resource**. The Pi-side bridge
(`csound/serial_bridge.py`, normally launched by `scripts/launch_phase4.sh`)
reads `/dev/ttyACM0` to turn frames into MIDI. If that bridge — or any other
reader — is running, the kernel hands out bytes to whichever reader asks first,
so two readers **split the stream**: you'll see garbled, half-line, or dropped
frames, and the bridge's MIDI output will glitch too.

`cat` does not open the port exclusively, so this script won't *crash* the
bridge — it just produces unusable output while the bridge runs. To see clean
telemetry, run the stream only when NEPTR audio isn't live, or stop the bridge
first. Check who's holding the port:

```sh
ssh gorpi7@demiurge.local 'lsof /dev/ttyACM0'   # no sudo needed for your own procs
```

Empty output means the port is free and the stream will be clean.

Note this is *separate* from the flashing soft-reboot trick: setting baud 134
reboots the Teensy into HalfKay; baud 115200 here just reads telemetry. Don't
point a flashing command at the port while streaming.
