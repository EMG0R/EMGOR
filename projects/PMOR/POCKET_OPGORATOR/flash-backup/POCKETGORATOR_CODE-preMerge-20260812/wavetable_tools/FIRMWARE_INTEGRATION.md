# FIRMWARE INTEGRATION — Wavetable Upload over USB CDC

This spec is for the firmware integrator working in
`POCKETGORATOR_CODE.cpp`. It adds an **UPLOAD MODE** that receives `.PWT`
wavetable files from the host (`wt_upload.py`) over the USB CDC serial link and
writes them to the microSD card via FatFs.

The host tooling is already written and tested against this exact protocol.
Match it byte-for-byte.

---

## 1. Verified libDaisy API (from `libDaisy/src/hid/usb.h`)

The board object `hw` (a `DaisySeed`) exposes `hw.usb_handle` of type
`UsbHandle`. Relevant members:

```cpp
void   Init(UsbPeriph dev);                              // FS_INTERNAL for the USB-C port
Result TransmitInternal(uint8_t* buff, size_t size);     // host <- device (our ACKs)
void   SetReceiveCallback(ReceiveCallback cb, UsbPeriph dev);
// typedef void (*ReceiveCallback)(uint8_t* buff, uint32_t* len);
enum UsbPeriph { FS_INTERNAL, FS_EXTERNAL, FS_BOTH };
```

- The Daisy Seed's onboard USB-C is **`UsbHandle::FS_INTERNAL`**.
- `SetReceiveCallback` fires **from the USB interrupt** each time a CDC packet
  arrives. Keep it tiny: copy bytes into a ring buffer, set a flag, return.
  Do **no** SD I/O inside the callback.
- `TransmitInternal` sends our short ASCII ACKs back to the host.
- If you currently use `hw.StartLog()` / `Log::Print`, that internally owns the
  CDC device and its own RX callback. You must **not** call `StartLog()` and
  also `SetReceiveCallback` on the same peripheral — pick one. For upload, init
  USB yourself and own the callback (see §4). Do this once at boot.

---

## 2. Wire protocol (must match `wt_upload.py`)

All control lines are ASCII, `\n`-terminated. Payload is raw bytes.

Per file:

| # | Direction | Bytes                              | Meaning                                  |
|---|-----------|------------------------------------|------------------------------------------|
| 1 | host → fw | `WTUP <name> <bytelen>\n`          | open `<name>` on SD, expect `<bytelen>`  |
| 2 | fw → host | `OK\n`  (or `ERR <reason>\n`)      | file opened, ready for data              |
| 3 | host → fw | up to 512 raw bytes (a chunk)      | one chunk of the `.PWT` payload          |
| 4 | fw → host | `K\n`   (or `ERR <reason>\n`)      | chunk written to SD; send next           |
|   |           | …repeat 3–4 until `bytelen` sent…  |                                          |
| 5 | fw → host | `DONE\n`                           | last bytes written, file closed/committed|

Key points:

- `<name>` is an 8.3 SD filename, e.g. `WTBL00.PWT` (≤ 12 chars). The host
  uppercases it.
- `<bytelen>` is the **total** payload (16-byte header + PCM). The firmware
  counts down from it and emits `DONE` when 0 remain.
- The per-chunk `K` ACK is the **flow control**: the host sends exactly one
  chunk, then blocks on `K`. This guarantees the host can never overrun the
  slow bit-banged SPI SD writes — the firmware sets the pace.
- Chunk size is **512 bytes** on the host. The last chunk may be shorter.
  Your RX ring must be ≥ 512 (use 1024 for headroom against IRQ arrival).
- On any error (`f_open` fail, `f_write` fail, byte-count mismatch, timeout),
  send `ERR <reason>\n`, close/unlink the partial file, and leave UPLOAD MODE
  cleanly. The host treats any non-`K`/non-`OK`/non-`DONE` reply as fatal.

The host's ACK timeout is **5 s** per step — well within a single SD chunk
write even on the bit-banged driver, but do not add artificial delays.

---

## 3. UPLOAD MODE state

Add a global mode flag and an entry gesture. Recommended: a **DASH → UPLOAD**
menu item, or an encoder chord, that sets `g_upload_mode = true`. While set:

- The main loop **skips** normal control-plane work (trellis, sensors, OLED
  animation, encoder menus) and instead runs `uploadServiceTick()`.
- The audio callback may keep running (it doesn't touch SD), but you can also
  gate it to silence to avoid glitches — your call. SD writes happen only in
  the main loop, never in the audio IRQ.
- Show `UPLOAD MODE — waiting…` on the OLED, then a byte/percent counter.
- Exit on: `DONE` of a file *and* host idle for ~2 s, or a long-press, or
  power-cycle. Re-enable normal operation by clearing `g_upload_mode`.

> The existing audio-save code (`audSaveTick`, lines ~980–1023) is a good model
> for "one SD chunk per service call, FatFs `f_write` with a `UINT bw` count".
> Reuse its temp-file → rename-commit pattern if you want crash-safety:
> write to `WTBL00.TMP`, then `f_rename` to `WTBL00.PWT` on `DONE`.

---

## 4. Copy-pasteable firmware skeleton

Adapt names to the existing file's conventions.

```cpp
// ───────────────────────── USB CDC RX ring ─────────────────────────
// Filled from the USB IRQ callback; drained in the main loop. Power-of-two
// size so head/tail masking is cheap. 1024 >= host's 512B chunk + headroom.
static volatile uint8_t  s_rx_buf[1024];
static volatile uint32_t s_rx_head = 0;   // written by IRQ
static volatile uint32_t s_rx_tail = 0;   // read by main loop
#define RX_MASK (sizeof(s_rx_buf) - 1)

// USB interrupt context — keep it minimal, no SD, no blocking.
static void usbRxCb(uint8_t* buf, uint32_t* len) {
    for(uint32_t i = 0; i < *len; i++) {
        uint32_t nh = (s_rx_head + 1) & RX_MASK;
        if(nh == s_rx_tail) break;        // ring full -> drop (flow control
                                          //   makes this practically impossible)
        s_rx_buf[s_rx_head] = buf[i];
        s_rx_head = nh;
    }
}

static inline uint32_t rxAvail() { return (s_rx_head - s_rx_tail) & RX_MASK; }
static inline uint8_t  rxPop()   { uint8_t b = s_rx_buf[s_rx_tail];
                                   s_rx_tail = (s_rx_tail + 1) & RX_MASK; return b; }

static void usbSend(const char* s) {
    hw.usb_handle.TransmitInternal((uint8_t*)s, strlen(s));
}

// Call ONCE at boot (in main(), after hw.Configure/Init, before the loop).
// Do NOT also call hw.StartLog() — it would fight for the same CDC RX callback.
static void uploadUsbInit() {
    hw.usb_handle.Init(UsbHandle::FS_INTERNAL);
    hw.usb_handle.SetReceiveCallback(usbRxCb, UsbHandle::FS_INTERNAL);
}

// ───────────────────── line + payload helpers ─────────────────────
// Blocking read of one '\n'-terminated ASCII line from the ring, with timeout.
// Returns length (excluding terminator) or -1 on timeout. Strips trailing '\r'.
static int rxReadLine(char* out, int maxlen, uint32_t timeout_ms) {
    uint32_t deadline = System::GetNow() + timeout_ms;
    int n = 0;
    while(System::GetNow() < deadline) {
        if(!rxAvail()) continue;
        uint8_t b = rxPop();
        if(b == '\n') { if(n && out[n-1]=='\r') n--; out[n] = 0; return n; }
        if(n < maxlen - 1) out[n++] = (char)b;
    }
    return -1;
}

// ───────────────────────── the upload loop ─────────────────────────
// Runs while g_upload_mode. Handles WTUP commands until host goes idle.
static void uploadServiceTick() {
    char line[64];
    int  ln = rxReadLine(line, sizeof(line), 2000);  // 2s idle -> return/exit
    if(ln < 0) { g_upload_mode = false; return; }     // host done / disconnected

    // Expect: "WTUP <name> <bytelen>"
    char name[16]; long total = 0;
    if(sscanf(line, "WTUP %15s %ld", name, &total) != 2 || total <= 0) {
        usbSend("ERR badcmd\n"); return;
    }

    char tmp[20]; // write to *.TMP, commit by rename on DONE (crash-safe)
    snprintf(tmp, sizeof(tmp), "%.12s", name);
    // (optional: derive a .TMP twin and f_rename at the end as audSaveTick does)

    FIL f;
    if(f_open(&f, tmp, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        usbSend("ERR open\n"); return;
    }
    usbSend("OK\n");

    static uint8_t chunk[512];
    long remaining = total;
    bool ok = true;
    while(remaining > 0 && ok) {
        // pull up to 512 bytes (or whatever's left) from the ring, blocking
        int want = remaining < 512 ? (int)remaining : 512;
        int got  = 0;
        uint32_t deadline = System::GetNow() + 5000;   // matches host timeout
        while(got < want && System::GetNow() < deadline) {
            if(rxAvail()) chunk[got++] = rxPop();
        }
        if(got < want) { ok = false; break; }          // starved -> abort

        UINT bw = 0;
        if(f_write(&f, chunk, got, &bw) != FR_OK || bw != (UINT)got) { ok = false; break; }
        remaining -= got;
        usbSend("K\n");           // ACK -> host releases the next chunk

        // (optional: update OLED progress here using total/remaining)
    }

    f_close(&f);
    if(ok) {
        // commit: if you wrote to a .TMP, f_rename(tmp, name) here.
        usbSend("DONE\n");
    } else {
        f_unlink(tmp);            // drop the partial file
        usbSend("ERR write\n");
    }
    // loop returns; next WTUP (if any) handled on the next tick.
}
```

Main-loop wiring:

```cpp
// once, in main() setup:
uploadUsbInit();

// inside the main while(1):
if(g_upload_mode) {
    uploadServiceTick();      // owns the loop while uploading
    continue;                 // skip normal control-plane work this iteration
}
```

---

## 5. Notes & gotchas

- **One CDC owner.** If `hw.StartLog()` is anywhere in boot, remove it (or stop
  relying on `Log::Print`) — it claims the same RX callback we need.
- **IRQ ring discipline.** The RX callback runs in IRQ context. Only the ring
  copy belongs there. Everything else (SD, OLED, ACKs) is in the main loop.
  `s_rx_head`/`s_rx_tail` are `volatile`; single-producer/single-consumer makes
  them safe without locks.
- **SD speed.** The bit-banged SPI SD is slow but the per-chunk `K` ACK means
  the host always waits for the previous `f_write` to finish, so there is no
  overrun. A 64-frame bank (~256 KB) is ~500 chunks; expect a few seconds.
- **8.3 names.** Keep filenames ≤ 12 chars (`WTBL00.PWT`). The host enforces
  this too.
- **Loading the tables.** This spec only covers *getting bytes onto SD*. A
  separate step (read `WTBLxx.PWT`, parse the 16-byte header, `f_read` the
  `int16` PCM into the wavetable bank `wtbl[][]` / a new SDRAM buffer) is the
  engine-side work — out of scope here, but the header layout in
  `README.md` §3 is what to parse: magic `PWT1`, `uint16 frameLen`,
  `uint16 numFrames`, `uint32 sampleRate`, 4 pad, then PCM.
- **OLED during upload.** Updating the OLED is SPI too; doing it every chunk
  slows throughput. Update every ~16 chunks or on percent change.
```
