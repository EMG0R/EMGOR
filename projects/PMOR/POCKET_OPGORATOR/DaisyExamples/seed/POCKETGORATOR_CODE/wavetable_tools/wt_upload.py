#!/usr/bin/env python3
"""
wt_upload.py - upload one or more .PWT wavetable files to a Pocket OpGorator
(Daisy Seed) over USB-C CDC serial.

WIRE PROTOCOL (host <-> firmware, all ASCII unless noted):
    The firmware must already be in UPLOAD MODE (see README / FIRMWARE_INTEGRATION.md).
    Per file:
      1. host -> "WTUP <name> <bytelen>\n"   e.g. "WTUP WTBL00.PWT 8208\n"
                  <name>    : 8.3 destination filename on the SD card
                  <bytelen> : total payload byte count (header + PCM)
      2. fw   -> "OK\n"                       ready to receive   (or "ERR ...\n")
      3. host streams the payload in CHUNK-byte blocks (default 512).
         After each chunk it waits for:
         fw -> "K\n"                          chunk written to SD, send next
                                              (or "ERR ...\n" to abort)
      4. after the last chunk:
         fw -> "DONE\n"                       file closed & committed on SD
    The single per-chunk ACK is what paces the host so it never overruns the
    slow bit-banged SPI SD card. CHUNK should match the firmware RX ring size.

Auto-detect: this scans serial ports and prefers a Daisy/STMicroelectronics
usbmodem. IMPORTANT: a connected Samsung phone also enumerates as a usbmodem.
If detection is ambiguous, pass --port explicitly. NEVER point this at your
phone's port.

Usage:
    python wt_upload.py WTBL00.PWT WTBL01.PWT
    python wt_upload.py --port /dev/cu.usbmodem1234 *.PWT
    python wt_upload.py --list            # list candidate ports and exit
    python wt_upload.py --name WTBL05.PWT one.PWT   # override SD filename
"""
import argparse, os, sys, time
import serial
import serial.tools.list_ports as list_ports

CHUNK = 512
BAUD = 115200          # CDC ignores baud, but pyserial wants a value
ACK_TIMEOUT = 5.0      # seconds to wait for OK/K/DONE per step

def find_ports():
    """Return (candidates, phones). Candidates are likely-Daisy ports."""
    cands, phones = [], []
    for p in list_ports.comports():
        desc = f"{p.description} {p.manufacturer or ''} {p.product or ''}".lower()
        dev = p.device
        # phones: Samsung / Android typically show vendor strings
        if "samsung" in desc or "android" in desc or "sm-" in desc:
            phones.append(p); continue
        # Daisy = STMicroelectronics Virtual COM / usbmodem
        if ("stmicro" in desc or "daisy" in desc or "virtual com" in desc
                or "usbmodem" in dev.lower() or "ttyacm" in dev.lower()):
            cands.append(p)
    return cands, phones

def pick_port(explicit):
    if explicit:
        return explicit
    cands, phones = find_ports()
    if phones:
        print("note: ignoring likely phone port(s): "
              + ", ".join(p.device for p in phones), file=sys.stderr)
    if len(cands) == 1:
        print(f"auto-detected Daisy on {cands[0].device}", file=sys.stderr)
        return cands[0].device
    if not cands:
        sys.exit("error: no Daisy serial port found. Plug in the Daisy, enter "
                 "UPLOAD MODE, and/or pass --port. Run with --list to see ports.")
    sys.exit("error: multiple candidate ports: "
             + ", ".join(p.device for p in cands)
             + "\n  pass one with --port. Do NOT pick your phone.")

def read_line(ser):
    """Read one '\\n'-terminated ASCII line, stripped. Raises on timeout."""
    deadline = time.time() + ACK_TIMEOUT
    buf = bytearray()
    while time.time() < deadline:
        b = ser.read(1)
        if not b:
            continue
        if b == b"\n":
            return buf.decode("ascii", "replace").strip()
        if b != b"\r":
            buf += b
    raise TimeoutError(f"timed out waiting for reply (got {buf!r})")

def upload_one(ser, path, name):
    data = open(path, "rb").read()
    n = len(data)
    if data[:4] != b"PWT1":
        print(f"  warning: {path} does not start with 'PWT1' magic", file=sys.stderr)
    ser.reset_input_buffer()
    ser.write(f"WTUP {name} {n}\n".encode("ascii"))
    ser.flush()
    reply = read_line(ser)
    if reply != "OK":
        raise RuntimeError(f"firmware did not ACK header: {reply!r}")

    sent = 0
    while sent < n:
        chunk = data[sent : sent + CHUNK]
        ser.write(chunk)
        ser.flush()
        reply = read_line(ser)
        if reply != "K":
            raise RuntimeError(f"chunk @{sent} not ACKed: {reply!r}")
        sent += len(chunk)
        pct = 100 * sent // n
        print(f"\r  {name}: {sent}/{n} bytes ({pct}%)", end="", flush=True)
    print()

    reply = read_line(ser)
    if reply != "DONE":
        raise RuntimeError(f"no DONE after final chunk: {reply!r}")
    print(f"  {name}: committed on SD")

def main():
    ap = argparse.ArgumentParser(description="Upload .PWT wavetables to the Daisy over USB CDC")
    ap.add_argument("files", nargs="*", help=".PWT files to upload")
    ap.add_argument("--port", help="serial port (e.g. /dev/cu.usbmodemXXXX). Auto-detected if omitted")
    ap.add_argument("--name", help="SD filename override (only valid with a single file)")
    ap.add_argument("--list", action="store_true", help="list serial ports and exit")
    args = ap.parse_args()

    if args.list:
        cands, phones = find_ports()
        print("Daisy candidates:")
        for p in cands: print(f"  {p.device}  [{p.description}]")
        print("Likely phones (DO NOT USE):")
        for p in phones: print(f"  {p.device}  [{p.description}]")
        print("All ports:")
        for p in list_ports.comports(): print(f"  {p.device}  [{p.description}]")
        return

    if not args.files:
        sys.exit("error: no .PWT files given. (use --list to inspect ports)")
    if args.name and len(args.files) != 1:
        sys.exit("error: --name only valid with exactly one input file")

    port = pick_port(args.port)
    print(f"opening {port} ...", file=sys.stderr)
    with serial.Serial(port, BAUD, timeout=0.2, write_timeout=ACK_TIMEOUT) as ser:
        time.sleep(0.3)  # let the CDC link settle
        for path in args.files:
            name = args.name or os.path.basename(path).upper()
            if len(name) > 12:  # 8.3 = max 12 chars
                sys.exit(f"error: '{name}' is not a valid 8.3 SD filename (<=12 chars)")
            print(f"uploading {path} -> {name}")
            upload_one(ser, path, name)
    print("all uploads complete.")

if __name__ == "__main__":
    main()
