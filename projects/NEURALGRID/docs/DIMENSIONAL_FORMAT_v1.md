# Dimensional format v1

The stable contract in the middle of the system.

```
any interface ──▶ encoder ──▶ [ DIMENSIONAL FRAME ] ──▶ model ──▶ audio (7-dim)
                               ◀── stable contract ──▶          ──▶ letters / text
```

Interfaces swap above it. Models and renderers swap below it. Only the middle
is fixed. It is an hourglass, the same shape as IP: *everything over the format,
the format over everything.*

## The one rule

**A frame describes the shape of the hardware, and nothing else.**

Channels are typed by their geometric dimensionality — 1D, 2D, 3D — with every
value normalized to 0–1. No musical, perceptual or semantic meaning is assigned
here. An encoder author declares their geometry and is done; they never decide
what "trackpad-Y means in frequency terms".

That decision belongs to the models, and there are two of them reading the same
frames: one translating to the 7-dimensional audio space, one to letters.

## Everything is continuous; σ carries the difference

There is deliberately **no discrete/continuous flag**. A key press is not an
event, it is a signal that goes 0→1→0. Sampled, every channel is a stream.

Resolution differences live in `sigma`, which does triple duty:

| σ means | example |
|---|---|
| model uncertainty | "I'm not sure" |
| device capability | can't sense it at all → `SIGMA_UNKNOWN` (1.0) |
| quantization | an 8-step pad axis → 1/16 (half a cell) |

All three answer one question: *how much do we actually know?* A trackpad and an
8×8 pad report the same shape; only the precision differs.

**Time is not a channel.** It is the axis every channel is sampled along. (The
`time` axis in the 7-dim *output* space is a different thing — trajectory
position within a sound. Same word, different layer.)

## Shape

```python
DimensionalFrame(source, t_mono, channels, edges)
  Channel(id, dims, label, points, associated)
    Point(values[dims], sigma[dims], pid)
  Edge(channel, kind, t_mono, point)
```

- **points** holds *several simultaneous contacts* — multitouch, several pads
  held. Empty means inactive.
- **associated** is a soft hint (a global force channel naming the 2D space it
  sits under). It is deliberately **not** structural nesting: asserting the
  relationship in the format would be exactly the pre-decided meaning this layer
  exists to avoid. A model may use it or ignore it.
- **edges** carry their own sub-millisecond timestamps. The sampled stream runs
  at ~42 fps (24 ms of resolution) but the device knows a press far more
  precisely; edges exist so sampling does not throw that away, and the sub-10ms
  fast path survives.

### Labels are a controlled vocabulary

`position · force · distance · ribbon · key · tilt`

Free text would leave the geometry portable and the semantics not, which
defeats the format. A model that sees `force` on the grid and `force` on a
laptop can generalize; one that sees `fsr_under_pad` and `trackpad_force`
cannot. Extend the list deliberately, never ad hoc.

## Declared interfaces

**neuralGrid** — `septagraph/dimensional/encoders/neuralgrid.py`

| channel | dims | label | notes |
|---|---|---|---|
| `pad` | 2 | position | 8×8, σ = 1/16 per axis |
| `lidar` | 3 | distance | 4 sensors triangulated *in the encoder* |
| `force` | 1 | force | 4 FSRs summed — **global, not per-pad** (assoc: `pad`) |
| `softpot_n/s/e/w` | 1 | ribbon | calibrated travel |

Two choices worth not undoing:

- **Force is global.** The FSRs are unevenly placed and read the whole plate.
  The hardware cannot say which of two held pads is pressed harder, so a
  per-point z would be a lie. A Linnstrument or Push *would* have true per-point
  z — the format supports both shapes, and σ says which you have.
- **Triangulation happens in the encoder.** Device-specific math belongs there;
  meaning belongs in the model. Declaring a portable 3D *volume* means a webcam
  or Leap can later fill the same channel. Where the geometry is weakly
  determined — a half-seen axis, or z, which four perimeter sensors cannot
  really measure — the encoder raises σ rather than inventing confidence.

**laptop** — `septagraph/dimensional/encoders/laptop.py`

| channel | dims | label | notes |
|---|---|---|---|
| `trackpad` | 2 | position | continuous surface, σ ≈ 0.004 |
| `keyboard` | 2 | position | key grid, σ = half a cell |

This exists as the portability proof. A format with one implementation is a
data structure with delusions. It needs no new hardware and covers both kinds
of 2D at once.

## Calibration

`calibration.json`, loaded **at runtime** — recalibrating never needs a rebuild
or a redeploy, because it gets re-run every time the housing comes open.

Nine steps: one resting-offset pass, then four softpot positions, then four
lidar positions.

### N/S/E/W is discovered, never assumed

The firmware's N/S/E/W is **only wiring order** — softpots on GPIO32/33/34/35,
lidar XSHUT on 4/13/14/27. Which physical strip or sensor that actually is was
never established anywhere in this project, so the encoder must not assume
index order.

Each step therefore names a **position a human can see** (top, right, bottom,
left — and the grid lights that edge), waits to find out **which hardware input
answers**, claims it, and records that input's travel in the same sweep. An
input already claimed by an earlier position cannot be claimed twice, so a
mistake surfaces as an unclaimed step rather than a silently doubled binding.

The result is `softpot_map` / `lidar_map` in the calibration file:
position → hardware index. Undiscovered positions fall back to wiring order.

### FSR resting offsets

The housing pre-loads some force sensors, so they sit well above zero with
nothing touching them (measured 2026-09-16: `3319 / 1419 / 0 / 0`). That is a
property of the assembly and changes whenever it is opened — so `fsr_rest`
holds a per-sensor resting count, taken as a median over several steady
samples, and the encoder subtracts each sensor's own rest before summing.

With no `fsr_rest` in the file the encoder falls back to learning a single
global resting floor live, which is cruder but keeps force usable uncalibrated.

## Version

`FORMAT_VERSION = 1`. Freeze it before writing a third encoder, not after —
otherwise the next encoder quietly reshapes it to fit itself.
