# Pocket OpGorator — Buttonless USB Flashing + Audio Passthrough

**Status (2026-06-07):** Buttonless USB flashing **WORKS**. Audio passthrough (raw in→out, no DSP) **WORKS**. OLED renders. Known constraint: OLED + USB-serial don't co-enumerate on dock/bus power (see §6).

This is native **libDaisy** (C++/Makefile), **not** Arduino. The Arduino `.ino` sketches in this repo were drafts; the working toolchain is the Daisy `make` build.

---

## 1. What this firmware does

`main.cpp` (project: `~/Desktop/DaisyExamples/seed/PocketPassthru/`):
- **Audio:** raw passthrough — `out[0..1] = in[0..1]`. No gain, no HPF, no DSP. Synthesis block is commented out.
- **OLED:** SSD130x 128×64 over hardware SPI1 (DC=D3, RST=D4), shows `HELLO EMORY`.
- **USB CDC serial trigger:** enumerates as `/dev/cu.usbmodem*`; sending the bytes `dfu` reboots the board into ROM DFU with **no button**.
- **Onboard LED** blinks ~2×/s = proof-of-life.

Backups in the project dir:
- `main_midi_trigger.cpp.bak` — earlier MIDI-CC-trigger version (MIDI never enumerated on this host; kept for the future composite CDC+MIDI build).

---

## 2. The buttonless mechanism (the important part)

The STM32H750 only enters its ROM DFU bootloader when **BOOT0 is high at reset**. On the Daisy Seed, **PG3 is wired to BOOT0 through an RC**, so firmware can do it in software:

```cpp
System::ResetToBootloader(System::BootloaderMode::STM);
// drives PG3 -> BOOT0 high, waits for the cap, then HAL_NVIC_SystemReset()
// -> on reboot BOOT0 is high -> ROM DFU (USB 0483:df11) comes up. No button.
```

The firmware watches the USB CDC RX for the 3 bytes `dfu` and calls the above. So:
**send `dfu` over serial → board self-enters DFU → `dfu-util` flashes → new firmware boots.**
The new firmware also contains the trigger, so the loop continues forever. The **only** time you need the physical BOOT+RESET buttons is the very first flash (or if you flash firmware that lacks the trigger / can't enumerate USB).

---

## 3. Build

```bash
cd ~/Desktop/DaisyExamples/seed/PocketPassthru
make                      # -> build/PocketPassthru.bin
```
Toolchain already present: `arm-none-eabi-gcc`, `make`, `dfu-util` (brew), `libDaisy` prebuilt at `~/Desktop/DaisyExamples/libDaisy/build/libdaisy.a`.

## 4. Flash — buttonless (normal case)

```bash
PORT=$(ls /dev/cu.usbmodem* | head -1)
stty -f "$PORT" 115200 raw -echo
printf 'dfu' > "$PORT"          # board self-reboots into DFU (~0.5s)
# wait until: dfu-util -l | grep -c "Internal Flash"  == 1
make program-dfu                # flashes; "File downloaded successfully"
```

`dfu-util: Error during download get_status` / `Error 74` at the very end is **harmless** — the STM32H7 leaves DFU (`:leave`) before dfu-util reads final status. The flash already succeeded.

## 5. Flash — first time / recovery (the only manual DFU)

Hold **BOOT**, tap **RESET**, release BOOT → `dfu-util -l` shows `[0483:df11] @Internal Flash` → `make program-dfu`.

### Host-side MIDI/serial sender helper
`/tmp/midisend.c` (CoreMIDI lister/sender, compiled with `clang -framework CoreMIDI -framework CoreFoundation`) was used to confirm CoreMIDI ports. `sendmidi` from brew needs full Xcode; CoreMIDI C is the no-Xcode path. Serial just needs `printf`.

---

## 6. Known constraint: OLED + USB-serial don't co-enumerate (power)

With the OLED active, the USB CDC serial port does **not** appear on the host, even though the firmware is fully alive (LED blinks, OLED shows text). Isolation tests (all reproducible):

| Firmware | Serial enumerates? |
|---|---|
| CDC only | ✅ yes |
| CDC + audio passthrough | ✅ yes |
| CDC + audio + OLED (hardware SPI) | ❌ no |
| CDC + audio + OLED (**software** SPI) | ❌ no |

Ruled out: **not a pin conflict** (USB = PA11/PA12/PA9; OLED pins = PC10/PC9/PC12/PG10/PG11/PB4/PB5, no overlap) and **not the SPI peripheral** (software bit-bang OLED also breaks it). Remaining cause: **power/brownout** — the OLED module's **12V boost** + audio + USB exceeds the budget of a bus/dock-powered port, dropping USB enumeration while the low-current MCU keeps running.

**To get OLED + buttonless serial together:** power the board from a **direct high-current Mac port or a powered USB hub** (not the Dell dock), or give it external/battery power. Then re-test for `/dev/cu.usbmodem*`. If it still fails on direct power, it's not power and needs deeper USB-descriptor investigation.

**Dev workflow that always works today:** flash the no-OLED build (`CdcTest` project = CDC + audio + trigger) for buttonless firmware iteration; flash the OLED build when you want the display. Or just power it properly and run the full build.

---

## 7. Roadmap

- **Parallel USB MIDI I/O** (the instrument needs it): FS USB is single-class by default. Doing CDC **and** MIDI together needs a **composite CDC+MIDI USB descriptor** in libDaisy — separate task. The MIDI-trigger version is saved in `main_midi_trigger.cpp.bak`.
- Confirm OLED+serial on proper power (§6).
- Re-add synthesis later (oscillators / SD sample playback) where the commented block is.
