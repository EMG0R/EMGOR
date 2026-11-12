# neuralGrid — fit test plate (rev 2, 2026-09-05)

`exports/FIT_TEST_PLATE.stl` — 194 × 92 × 4 mm plate (+12 mm bosses), 53.3 cm³.
Watertight, 0 non-manifold edges. Rebuild with `make_testplate.py`.

**4 mm thick on purpose** — the same as the shell wall, so hole depth and thread
engagement behave like the real part.

## Rows

| Row | Feature | Variants |
|---|---|---|
| Top | **Big-connector ladder**, round | Ø22.0 / 22.2 / 22.4 / 22.6 / 22.8 / 23.0 / 23.2 |
| Middle | Power hole, round | Ø15.6 / 15.8 / **16.0** / 16.2 |
| Middle right | Power hole **keyed** (flat at top) | Ø16.0, 15.0 across the flat |
| Middle left | SoftPot tail slot | 13.0 × 6.0 and 13.4 × 6.4 |
| Lower | Pad holes — **full 8-wide row at true 15.0 pitch** | 12.6 |
| Lower ends | M4 insert boss Ø14 × 12 | solid (left), Ø5.6 × 10 pilot (right) |

**Marker dots**: 1 dot = smallest, counting up left to right. The ladder runs
1–7 dots, the power row 1–4.

### Why one ladder instead of two rows

The Ethernet's nominal cutout is Ø22.0 and the USB's is Ø22.55 — only 0.55 mm
apart. Two separate 4-hole rows would have looked nearly identical and been easy
to mix up. A single 22.0 → 23.2 ladder brackets both parts, uses fewer holes, and
cannot be confused.

### Why every big hole is round

A flat makes a hole **smaller** on one side, so an unnecessary flat blocks a
round part rather than merely failing to key it — Ø22.4 with a 1 mm flat is only
21.4 across, and a round Ø22 body would not enter. Checking the drawings:

| Part | Drawing says | Verdict |
|---|---|---|
| Ethernet | 22,00 and 21,99 | 0.01 mm apart — **round** |
| USB | Ø22.55, no flat given | **round** |
| Power | 14.6 across a 15.6 circle | a real **1.0 mm flat** |

So only the power connector is keyed, and even that is round in the shell for now
— the keyed sample on this plate tells us whether the flat belongs at the top
before it gets committed to a 292 cm³ print.

### The pad row

Eight holes at true pitch, not a 2×2 patch, because what actually bites is
**accumulated** pitch error across eight buttons. Verified in the model: pitch
15.000 on all seven steps, span 105.000.

## Print it fast

0.28 mm layers, 2 perimeters, 10 % infill, no brim. It is a gauge, not a part.

## Then tell me

1. Which ladder hole the **Ethernet** fits (1–7 dots)
2. Which ladder hole the **USB** fits (1–7 dots)
3. Which power hole fits, and whether the keyed one accepts the connector
4. Whether the 8-hole pad row matches your elastomer end to end
5. Which slot fits the SoftPot tail
6. Which boss took the M4 insert better — solid or pilot

Six numbers and the shell is final.
