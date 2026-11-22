# 5-Pin 3.5mm Switched Jack — Pin Reference

**Status: MEASURED AND LOCKED 2026-09-13.**

This file exists so the jack pinout is written down ONCE and never re-derived.
Datasheets for these parts are unreliable and the pin order varies between
sellers even for parts sold as "PJ-307". **Measure yours, record it here, done.**

---

## 1. Physical position naming (canonical — use these names everywhere)

Viewed from the **BOTTOM/PIN SIDE**, with the barrel (where the plug inserts)
pointing **AWAY** from you:

```
                  ┌─────────────────┐
                  │                 │
     barrel ◄─────┤       [T]       │   ← TOP  (single pin, upper row)
                  │                 │
                  │  [L] [LM] [RM] [R]  │   ← lower row, 4 pins
                  └─────────────────┘
```

| Short | Name | Position |
|-------|------|----------|
| **T**  | **TOP**          | the single pin in the upper row, on its own |
| **L**  | **LEFTMOST**     | lower row, far left |
| **LM** | **LEFT-MIDDLE**  | lower row, second from left |
| **RM** | **RIGHT-MIDDLE** | lower row, third from left |
| **R**  | **RIGHTMOST**    | lower row, far right |

> ⚠️ **Orientation matters.** "Left" is defined with the **pins facing you and
> the barrel pointing away**. Flip the jack over and left/right swap. Always
> restate the orientation when recording a measurement.

---

## 2. The five electrical functions to find

A 5-pin switched 3.5mm stereo jack has exactly these:

| Function | What it does |
|----------|--------------|
| **TIP**       | left channel signal |
| **RING**      | right channel signal |
| **SLEEVE**    | ground / shield |
| **NC→TIP**    | switch contact — **closed to TIP when NO plug**, opens when a plug is inserted |
| **NC→RING**   | switch contact — **closed to RING when NO plug**, opens when a plug is inserted |

The two NC (normally-closed) contacts are what give us the auto-switching:
mic on the input jack, speaker mute on the output jack.

---

## 3. MEASURED MAPPING — fill this in

**Measured on the actual parts, 2026-09-13. LOCKED.**

| Physical | Function | How it was confirmed |
|----------|----------|----------------------|
| **T**  (top)          | **SLEEVE**   | goes to the sleeve (lowest band) of the TRS |
| **L**  (leftmost)     | **RING**     | connects to RING of the TRS when a plug is in |
| **LM** (left-middle)  | **NC→RING**  | connected to **L** when NO plug; opens when plugged |
| **RM** (right-middle) | **NC→TIP**   | connected to **R** when NO plug; opens when plugged |
| **R**  (rightmost)    | **TIP**      | connects to TIP of the TRS when a plug is in |

**The pattern: outer pins are the live contacts, the inner pin beside each is
its switch, top is sleeve.**

```
                  ┌─────────────────────┐
     barrel ◄─────┤        [T]          │   SLEEVE
                  │                     │
                  │ [L]  [LM] [RM]  [R] │
                  │ RING  NC   NC   TIP │
                  │       RING  TIP     │
                  └─────────────────────┘
              (pins toward you, barrel pointing away)
```

---

## 4. How to measure it (multimeter, continuity mode)

You need a 3.5mm plug — any cable will do.

**Step 1 — find SLEEVE.**
Insert the plug. Probe each pin against the plug's **sleeve** (the barrel
nearest the cable). The pin that beeps is **SLEEVE**. It is the only pin whose
continuity does NOT change when you remove the plug.

**Step 2 — find TIP and RING.**
Still with the plug inserted, probe each remaining pin against the plug's
**tip** (very end) and its **ring** (middle band). Two pins will beep — one to
tip, one to ring. Those are **TIP** and **RING**.

**Step 3 — find the two switch contacts.**
The two pins left over are the NC contacts. To confirm which belongs to which:
- **Remove the plug.**
- Probe each leftover pin against the pin you identified as **TIP**. The one
  that beeps with no plug, and stops beeping once you insert the plug, is
  **NC→TIP**.
- Repeat against **RING** for **NC→RING**.

**Sanity check:** with no plug, NC→TIP–to–TIP and NC→RING–to–RING should both
beep. With a plug inserted, both should go open. If a contact never opens, the
spring is bent or the jack is a non-switched type.

---

## 5. Once measured — the wiring that depends on it

Both jacks use the NC contacts for hardware auto-switching. No firmware, no GPIO.

### INPUT jack — mic auto-switch

```
R  (rightmost)     TIP      ──→ Daisy AUDIO_IN_L
L  (leftmost)      RING     ──→ Daisy AUDIO_IN_R
T  (top)           SLEEVE   ──→ AGND
RM (right-middle)  NC→TIP   ──┬── MAX4466 OUT
LM (left-middle)   NC→RING  ──┘   (mono mic feeds both channels)
```

**No plug:** springs closed → mic reaches both ADCs. Mic live.
**Plug in:** springs lift → mic disconnected. External input takes over.

### OUTPUT jack — speaker auto-mute

```
R  (rightmost)     TIP      ◄── Daisy AUDIO_OUT_L   (headphones get this)
L  (leftmost)      RING     ◄── Daisy AUDIO_OUT_R
T  (top)           SLEEVE   ──→ AGND, via 10Ω ∥ 100nF lift (+ LIFT switch)
RM (right-middle)  NC→TIP   ──→ PAM8403 L_IN   + 10kΩ to GND
LM (left-middle)   NC→RING  ──→ PAM8403 R_IN   + 10kΩ to GND
```

**No plug:** springs closed → amp gets signal → speakers play.
**Plug in:** springs lift → amp input disconnected → speakers silent.

**The 10kΩ pulldowns are required.** With a plug inserted the amp inputs float,
and a floating class-D input buzzes. The pulldowns hold them at 0V.

**`D18` SHDN_CTRL is still used** — not for headphone muting (that is mechanical
now) but for **pop suppression at power events**. With `R_SHDN` pulling it low,
the amp is definitively muted whenever the Daisy is off, unprogrammed, or in
reset. That is what stops the power-up screech.

**`D19` HP_DETECT is NO LONGER NEEDED** — freed by the mechanical switch. v1
used it with a 10k pullup + RC filter to do this job in firmware.

> **Why the mechanical approach is fine here**, despite v1 rejecting hardware
> mute: v1's objection was that the switch is **TIP-referenced, not
> ground-referenced**, so driving **SHDN** from it picks up audio AC and is
> unreliable. True — but that is about driving SHDN. **Interrupting the signal
> path is a different thing**, and it is exactly what the input jack already
> does for the mic. The objection does not apply.

---

## 6. Related

- `WIRING-v2.md` — full pin map, power topology, ground rules
- `HOUSING-v2-BRIEF.md` — panel-mount requirements for these jacks
