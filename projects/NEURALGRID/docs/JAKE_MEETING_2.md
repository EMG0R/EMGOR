# neuralGrid — system state and technical positions

What exists, where it goes, and the technical content of the two open
disagreements: letters vs bols, and music downstream of letters.

---

## 1. What exists now

### The dimensional format (`docs/DIMENSIONAL_FORMAT_v1.md`, `dimensional/format.py`)

The stable contract in the middle of the system.

```
any interface → encoder → [ DIMENSIONAL FRAME ] → model → audio (7 axes)
                           ← frozen contract →           → letters / text
```

A frame describes **what shape the hardware is**, nothing else. Channels
are typed by geometric dimensionality (1D / 2D / 3D), every value
normalised 0–1. No perceptual or musical meaning is assigned at this
layer — that is the layer's purpose.

Channel labels come from a controlled vocabulary:
`position, force, distance, ribbon, key, tilt`. Free text would make the
geometry portable but not the semantics.

**σ carries three things at once**, which is why there is no
discrete/continuous flag:

1. model uncertainty — "I'm not sure"
2. device capability — "I cannot sense this" (`SIGMA_UNKNOWN = 1.0`)
3. quantisation — "I sense it, but only this precisely"

An 8-step pad axis reports σ = 1/16 (half a cell). A trackpad reports far
less. Everything is continuous; only precision differs.

**Edges** carry onset/release at sub-millisecond resolution, separate from
the sampled stream. The stream runs ~42–50 Hz (≈20–24 ms), so onset timing
would otherwise be destroyed by the act of sampling.

### neuralGrid encoder (`dimensional/encoders/neuralgrid.py`, ~900 lines)

| channel | dims | label | notes |
|---|---|---|---|
| `pad` | 2D | position | 8×8 NeoTrellis |
| `force` | 1D | force | `associated=("pad",)` — a hint, not nesting |
| `softpot_n/s/e/w` | 1D | ribbon | four strips |
| `lidar` | 3D | distance | fused volume above the grid |

Device-specific maths absorbed inside the encoder so nothing downstream
sees it: 4-sensor trilateration → a blurred 3D point; height-corrected
flat solve (estimate z slowly, subtract, solve the plane); 64-pad FSR
response surface with projection onto summed columns and cosine gating so
resting fingers don't drag the reading; onset-referenced aftertouch;
auto-ranging with a slow scale rate; asymmetric smoothing (fast attack,
slow release).

Two structural decisions:

- **Lidar is its own 3D channel, not a z on the pad.** Pad xy is measured;
  a fused z would be inferred. Different frames, different rates, not
  simultaneous in practice (hand on pad vs hand in air).
- **Force is one global 1D channel, not per-pad.** The four FSRs are
  unevenly placed and global. Per-pad z would misdescribe the hardware. A
  Linnstrument genuinely has per-point z; the format supports both.

### What the output actually looks like

One frame, every channel populated, from `format.frame_to_json`.

```json
{
 "v": 1, "source": "neuralgrid", "t": 12.345,
 "channels": [
  {"id": "pad", "dims": 2, "label": "position", "points": [{"v": [0.44, 0.56], "s": [0.0625, 0.0625], "pid": 0}], "assoc": []},
  {"id": "force", "dims": 1, "label": "force", "points": [{"v": [0.62], "s": [0.06], "pid": 0}], "assoc": ["pad"]},
  {"id": "softpot_n", "dims": 1, "label": "ribbon", "points": [{"v": [0.31], "s": [0.02], "pid": 0}], "assoc": []},
  {"id": "softpot_e", "dims": 1, "label": "ribbon", "points": [{"v": [0.78], "s": [0.02], "pid": 0}], "assoc": []},
  {"id": "softpot_s", "dims": 1, "label": "ribbon", "points": [{"v": [0.12], "s": [0.02], "pid": 0}], "assoc": []},
  {"id": "softpot_w", "dims": 1, "label": "ribbon", "points": [{"v": [0.95], "s": [0.02], "pid": 0}], "assoc": []},
  {"id": "lidar", "dims": 3, "label": "distance", "points": [{"v": [0.58, 0.97, 0.41], "s": [0.29, 0.29, 0.64], "pid": 0}], "assoc": []}
 ],
 "edges": [
  {"ch": "pad", "kind": "onset", "t": 12.3401, "p": [0.44, 0.56]}
 ]
}
```

Same frame, one readable line:

```
12.345  pad 0.44,0.56 ±0.06   force 0.62 ±0.06   softpot_n 0.31
        softpot_e 0.78   softpot_s 0.12   softpot_w 0.95
        lidar 0.58,0.97,0.41 ±0.64   onset
```

Structure:

```
frame   = { v, source, t, channels[], edges[] }
channel = { id, dims, label, points[], assoc[] }
point   = { v: [...dims values], s: [...one sigma per value], pid }
```

- **All four ribbons are always present**, every frame. An untouched one
  is `"points": []` — the channel exists, nothing is on it. That is a
  different statement from the channel being absent, which would mean the
  device cannot sense it at all.
- **Lidar is one fused 3D coordinate**, `[0.58, 0.97, 0.41]`. The four
  individual rangefinder distances never enter the frame; trilateration
  happens inside the encoder. Raw per-sensor values exist only in the
  diagnostic view.
- **σ is per value, not per channel.** Lidar reports 0.29 laterally and
  **0.64 vertically** — the encoder stating it is genuinely unsure about
  height in that position. A consumer knows to trust x/y and discount z
  without knowing anything about rangefinders.
- **pad σ = 0.0625** is half a cell: the most an 8-step axis can claim.
- **`"assoc": ["pad"]`** on force is a hint that force belongs to the pad,
  without structurally nesting it. A model may use it or ignore it.
- **The edge timestamp (12.3401) precedes the frame's own (12.345).** The
  frame says where things are; the edge says exactly when the strike
  happened.

### Rate

The encoder does not push at a fixed rate. `frame(t)` is a snapshot taken
on demand. What has rates is the input:

| | rate | why |
|---|---|---|
| analog — 4 ribbons + 4 force | ~42–50 Hz | I2C at 400 kHz, ~20–24 ms/sample |
| lidar | ~20 Hz | sensor limit |
| pad keys | event-driven | not polled |
| **onset/release edges** | sub-millisecond | carried separately |

The sampled stream only resolves ~20–24 ms, far too coarse for a strike,
which is why edges carry their own timestamps. Current consumers sample at
25 Hz (UI) and 20 Hz (LED loop), but that is their choice, not the
encoder's — a model could read every frame at 50 Hz, or read only edges,
or both.

### Second encoder (`dimensional/encoders/laptop.py`, 127 lines)

Trackpad and keyboard as two 2D `position` channels. Exists to prove the
format is not shaped around one device — it covers continuous 2D and
discrete 2D on hardware with no pressure and no real onsets.

### Running

On the neuralGrid Pi as a mode inside `neuralgrid-app`. Calibration loaded
at runtime from `~/.neuralgrid/calibration.json` (64 FSR curves, softpot
map, grid rotation). ESP32 fw6, verified.

### Not built

- **No logging. No recorded sessions.** Everything model-shaped is blocked
  behind this.
- No model of any kind, either head.
- 26-class separability on this hardware is unmeasured.
- Laptop encoder has no transport feeding it.

---

## 2. Where it goes

One trunk, two heads, one translator.

```
                        ┌→ HEAD A: symbolic units → speech synthesis
gesture → encoder → ────┤
          (frame)       └→ HEAD B: 7-axis continuous output (μ and σ)
```

The encoder itself has no heads — it is a single program emitting the one
frame stream shown above. The heads are separate models that would consume
it, and neither is written.

- **Trunk** = the dimensional encoder. Hand-built, geometric, per-device.
- **Head A** = classifier over motion → a symbolic inventory → speech.
  Formant/articulatory synthesis, not TTS: formants map to the spectral
  axis, voiced/unvoiced to harmonicity.
- **Head B** = continuous control across the 7 axes, as **distributions**
  (μ and σ), not point values. A word maps to a *region*, not a note.
- **Translator** (baaj) = what maps symbolic units onto axes. It is the
  instrument: the same units through a different translator is a different
  instrument. Mappings are free — time can be locked to something by
  hard-coding the translator, or not; both are equally available.

Both heads read identical frames. Head B does not read Head A's output.

---

## 3. Letters vs bols

The technical difference is **where the class inventory comes from**.

**Letters** — a closed, fixed, externally-defined alphabet of 26.
- Buys an existing semantic space for free: word embeddings, a
  pronunciation dictionary, an obvious evaluation target.
- Makes internal state legible and gives one number to report
  (classification accuracy on held-out gestures).
- Costs: the inventory is imposed before measuring what the hardware can
  actually distinguish. If the interface separates 14 motion classes
  reliably, forcing 26 produces a confusion matrix with structural
  collisions that no amount of training removes.

**Bols** — an inventory defined by *the properties of the motion through
the neuralGrid*: where on the grid, how hard, which trajectory, what
timing. Not tabla strokes, not which hand, not resonance — the borrowed
word is for the structural role (atom, then theka/taal for pattern and
cycle, kaida/tihai for variation and resolution), not the content.
- Buys an inventory sized to the sensor's real discriminative capacity.
- Costs: no external semantic space arrives for free; you have to build or
  borrow one.

**They are not exclusive.** The defensible construction is: derive the
inventory from measured separability, then *map* it to letters where a
letter mapping is useful (the speech path). Inventory size becomes an
empirical result rather than an assumption.

**The measurable question:** how many motion classes does this hardware
separate, at what accuracy, with how many examples? That is answerable
with logged data and nothing else. Until it is answered, "26" and "bols"
are both assertions.

---

## 4. Music directly off the letters

**The symbolic bottleneck belongs where the output is symbolic. Speech
qualifies. Continuous musical control does not.**

### 4.1 Bit rate

A symbol stream at ~5 units/sec over a 26-class inventory carries
≈ log₂(26) × 5 ≈ **23 bits/sec**.

The encoder stream is multi-channel, ~42–50 Hz, each value continuous with
its own σ — orders of magnitude more. Routing musical control through the
symbol stream discards that difference by construction, then asks a
decoder to reconstruct it.

### 4.2 What specifically is lost

Not uniform detail — three things the symbol cannot carry:

- **intra-gesture trajectory** (the path, not the endpoint)
- **pressure and velocity dynamics** within a single unit
- **timing microstructure** below the segmentation boundary

Those are the dimensions that carry expression. A symbol is the label on a
gesture; the music is in the gesture.

### 4.3 The decoder must invent

Given only a symbol, the decoder produces the same output every time, or
it produces variation that did not come from the player. Both are
generation, not instrument behaviour.

The usual fix is to condition the decoder on a continuous side-channel —
at which point the continuous path has been reintroduced, and the symbolic
layer is no longer the bottleneck it appeared to be. That reconstructs the
parallel architecture by a longer route.

### 4.4 Latency

A symbol cannot be emitted until the gesture it labels is complete —
segmentation is a precondition of classification. That imposes a delay of
one whole gesture before any sound. The continuous path answers within a
frame, and onset edges carry sub-millisecond timestamps specifically to
protect a sub-10 ms response.

For an instrument this is the difference between playable and not.

### 4.5 Why speech is the exception

Speech is already symbolic at the phoneme level, so the bottleneck is the
*correct* representation for that output rather than a lossy stage in
front of it. The symbol picks the target; the continuous path supplies
coarticulation — how you get there. Prosody (pitch, stress, rate) rides
the continuous path.

Tractable v1: vowels continuous via formant filters, consonants as
triggered transients.

This is also what separates the project from a gesture keyboard driving
off-the-shelf TTS. Spelling a word and having it spoken is a 40-year-old
idea. It is an instrument only if the speech is *played* — prosody under
continuous hand control, live.

---

## 5. Supporting positions

**Why hand-build the encoder rather than learn it.** Portability: a new
interface is an afternoon of code, not a retraining run, and both heads
work on it immediately. Debuggability: a hand-built encoder is
falsifiable — a wrong lidar reading points at the trilateration. A learned
trunk fails silently and a sensor fault is indistinguishable from a model
fault. The learning sits above the waist, where the open problem is.

**Why not OSC or MIDI.** OSC is a transport, not a semantics — it says
nothing about what a value means or how certain it is. MIDI presumes
notes, keys and channels, which is a musical commitment baked into the
wire. The dimensional format declares geometry plus uncertainty and
nothing else.

**Why 7 axes if the first renderer drives 3.** The number of axes the
*format* can address and the number a *renderer* drives are separate
commitments. Driving 3 first costs nothing and is reversible; narrowing
the format is neither, and a narrow space is the wrong place to ask a
classifier to separate a large inventory.

---

## 6. Open, and answerable only with data

- How many motion classes does this hardware separate, and at what accuracy?
- How many examples per class are needed to get there?
- Does the continuous path actually sound like an instrument, or only
  argue like one?

All three are blocked on logging, which does not exist yet.

---

## 7. What training actually looks like, both ways

Both need the same first step and the same data. They differ in what
happens after it, and in whether anything makes sound before training
finishes.

### 7.0 Shared prerequisite — logging (does not exist)

Identical for both. Record the full frame stream at native rate plus edges:

- `frame_to_dict()` already serialises a frame; JSONL at ~42–50 Hz is
  ~3–5 MB per minute uncompressed, trivially compressible.
- Per session: frames, edges, monotonic timestamps, calibration hash, a
  label track.
- Labels: the intended symbol per gesture. Either prompted (screen or grid
  shows the target, player performs it) or post-hoc segmented and tagged.
  Prompted is cheaper and gives balanced classes.

Volume, for a 26-class inventory at ~100 examples/class and ~1.5 s per
gesture: ≈65 minutes of *pure* gesture, realistically 3–5 hours of
sessions once rest, errors and re-takes are included. Fewer classes scales
down linearly.

### 7.1 Jake's proposal — pipeline

**Stage 1: gesture → letter.** Supervised sequence classification.
- Input: a window of frames (channels × time), each value with σ.
- Model: small 1D-CNN or GRU/transformer encoder → 26-way softmax.
- Loss: cross-entropy.
- Segmentation is a precondition — you must know where a gesture starts
  and ends before you can label it. Either use onset edges as boundaries,
  or avoid the problem with **CTC** over the continuous stream, training on
  (stream, letter-sequence) pairs with no per-frame alignment. CTC removes a whole class of labelling error.
- Metric: held-out accuracy, plus the confusion matrix — which also
  answers whether 26 classes are separable at all.

**Stage 2: letters → words.** Not really training. A dictionary decode,
optionally with an n-gram or small language-model prior to correct stage-1
errors. This is where word embeddings enter. Mostly off-the-shelf.

**Stage 3: words/embeddings → 2–3 sound dimensions.** This is the
unresolved part, and the difficulty is not engineering.

> **There is no supervised target.** Nothing in the world states what a
> given word "should" sound like on those axes. There is no ground truth
> to regress against and no loss to minimise.

Available options:
- hand-design the mapping (then it isn't trained, and the embeddings are
  decorative);
- train against human preference (needs a rating loop, many trials, and
  produces a model of the rater);
- pick an unsupervised proxy — e.g. preserve embedding distances in the
  audio space — which optimises a geometric property nobody listens for.

Nothing makes sound until stages 1 and 3 both exist, and stage 3 has no
defensible objective. The objection is to the *loss function*, not to
taste.

### 7.2 This proposal — shared trunk, two heads

**Trunk: no training.** Hand-built encoder. Zero data. Works today.

**Head A: symbolic.** Identical to Jake's stage 1 — same architecture, same
CTC-vs-segmentation choice, same data, same accuracy metric. Nothing is
avoided or made easier.

**Head B: continuous 7-axis.** Three tiers, in build order:

1. **Untrained translator.** A hand-designed map from frame geometry to
   the 7 axes. No data, no model, playable immediately. This is what makes
   the instrument exist before any training does.
2. **Density head.** Emit μ *and* σ per axis rather than point values —
   a mixture-density output. Trainable self-supervised as a **forward
   model**: predict the next frame's geometry from the recent past. The
   target is the player's own subsequent motion, so it needs no labels and
   no audio ground truth, and σ acquires a principled meaning (predictive
   uncertainty) rather than being a knob.
3. **Reactive translator.** Adapts to what the player returns to. The
   model cannot hear, so the signal is behavioural: repetition, dwell,
   re-visitation of regions. That is an online/bandit-shaped objective,
   not supervised learning.

**Speech path:** formant/articulatory synthesis is parametric — largely
not learned. Phoneme targets from Head A, coarticulation and prosody from
the continuous path.

### 7.3 Honest comparison

| | Jake's pipeline | Shared trunk |
|---|---|---|
| labelled gesture data | required | required, identical |
| hard classification problem | stage 1 | Head A — same problem |
| sound before training | no | yes, tier 1 |
| supervised target for audio | **none exists** | none exists either |
| audio evaluation | by ear | by ear |
| latency to first sound | one full gesture | one frame |

- **The classification work is the same in both.** This proposal adds a
  path that works while the hard part is unsolved; it does not remove it.
- **Neither proposal has a supervised objective for the musical output.**
  The difference is that the pipeline *requires* that missing objective to
  produce any sound at all, while the trunk architecture only needs it for
  tier 2/3 — tier 1 is hand-designed and already playable. Tier 2's
  forward-model objective is the one genuinely well-posed learning problem
  on the audio side, and it exists only because the continuous path was
  kept.

---

## 8. Code

- `septagraph/py/septagraph/dimensional/format.py` — the contract
- `septagraph/py/septagraph/dimensional/encoders/neuralgrid.py` — the encoder in use (917 lines)
- `septagraph/py/septagraph/dimensional/encoders/laptop.py` — second encoder (127 lines)
- `septagraph/py/septagraph/dimensional/trilaterate.py` — lidar geometry
- `docs/encoder_neuralgrid_bare.py` — the encoder with comments and docstrings stripped, 448 lines, for reading
