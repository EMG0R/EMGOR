# neuralGrid — deliverable1


**"Pick 2–3 output dimensions first."**
I think it would be best to handle just speech first, then translate speech
onto the 7 dimensions as a later step. Speech seems like a solid first
milestone for now.

**"Design the gesture → letter inventory."**
I think bols are a better reference point than letters, because they can be
mapped to gestures that match their feel, get easier access to forming
sentences, and can then train on language data focused on those words. Also
accesses wealth of influence from parallels in the language of tabla.

**"How do letter sequences drive each dimension?"**
First "layer" translates gestures into bols. Second "layer" uses bols to
predict and interact with the user's gestures on a language and speech
level. Third "layer" maps this onto all 7 musical axes.

---

## the 7 axes

all values 0–1. equal steps ≈ equal perceptual steps. 


| # | axis | meaning | default map |
|---|---|---|---|
| 0 | freq | pitch, log | `hz = 20 · 1000^k` |
| 1 | loud | level | `db = −60 + 60k` |
| 2 | harm | noise → tone | harmonic fraction = k |
| 3 | rate | modulation over time, log | `hz = 0.5 · 128^k` |
| 4 | scale | modulation over frequency, log | `cyc/oct = 0.25 · 32^k` |
| 5 | az | azimuth, full circle | `deg = −180 + 360k` |
| 6 | spread | width | point → diffuse |
| 7 | dist | distance | dry → wet |
| 8 | el | elevation | `deg = −90 + 180k` |
| — | time | position in trajectory | not a coordinate |

7 axes, 9 wire values: `space` splits into az/el/dist/spread, a point
on a sphere. `time` is the
trajectory parameter, normalised 0–1, stretched at render.

rate = modulation across time. scale = modulation across frequency.

the space is continuous. no note numbers, no instrument indices. a
renderer's catalog is a discrete set of points inside it.

---

## encoder

| channel | dims | label | source |
|---|---|---|---|
| pad | 2 | position | 8×8 grid, one point per held pad |
| force | 1 | force | 4 sensors under the whole grid, global |
| softpot_n/s/e/w | 1 | ribbon | 4 strips |
| lidar | 3 | distance | 4 rangefinders, trilaterated to one point |

---

## encoder output

```json
{
 "v": 1, "source": "neuralgrid", "t": 8.8,
 "channels": [
  {"id": "pad", "dims": 2, "label": "position", "points": [{"v": [0.062, 0.062], "s": [0.062, 0.062], "pid": 0}, {"v": [0.438, 0.312], "s": [0.062, 0.062], "pid": 19}, {"v": [0.688, 0.688], "s": [0.062, 0.062], "pid": 45}, {"v": [0.938, 0.938], "s": [0.062, 0.062], "pid": 63}], "assoc": []},
  {"id": "lidar", "dims": 3, "label": "distance", "points": [{"v": [0.400, 0.561, 0.368], "s": [0.22, 0.22, 0.484], "pid": 0}], "assoc": []},
  {"id": "force", "dims": 1, "label": "force", "points": [{"v": [0.0], "s": [0.06], "pid": 0}], "assoc": ["pad"]},
  {"id": "softpot_n", "dims": 1, "label": "ribbon", "points": [{"v": [0.311], "s": [0.02], "pid": 0}], "assoc": []},
  {"id": "softpot_s", "dims": 1, "label": "ribbon", "points": [{"v": [0.121], "s": [0.02], "pid": 0}], "assoc": []},
  {"id": "softpot_e", "dims": 1, "label": "ribbon", "points": [{"v": [0.779], "s": [0.02], "pid": 0}], "assoc": []},
  {"id": "softpot_w", "dims": 1, "label": "ribbon", "points": [{"v": [0.947], "s": [0.02], "pid": 0}], "assoc": []}
 ],
 "edges": [
  {"ch": "pad", "kind": "onset", "t": 8.0, "p": [0.062, 0.062]},
  {"ch": "pad", "kind": "onset", "t": 8.0, "p": [0.438, 0.312]},
  {"ch": "pad", "kind": "onset", "t": 8.0, "p": [0.688, 0.688]},
  {"ch": "pad", "kind": "onset", "t": 8.0, "p": [0.938, 0.938]}
 ]
}
```

same frame in the ui, one line per dimension:

```
pad.x        0.062  ±0.06
pad.y        0.062  ±0.06
pad2.x       0.438  ±0.06
pad2.y       0.312  ±0.06
pad3.x       0.688  ±0.06
pad3.y       0.688  ±0.06
pad4.x       0.938  ±0.06
pad4.y       0.938  ±0.06
pad.n        4
lidar.x      0.400  ±0.22
lidar.y      0.561  ±0.22
lidar.z      0.368  ±0.48
force        0.000  ±0.06
softpot_n    0.311  ±0.02
softpot_s    0.121  ±0.02
softpot_e    0.779  ±0.02
softpot_w    0.947  ±0.02
```

---

## swapping hardware

bc of standardized dimentional, model would function similarly with swapped hardware+encoder 

```
neuralGrid ─┐
laptop    ──┼→ [ frame ] → bols → language / speech → 7 axes
new device ─┘
```

an encoder author declares geometry: how many 1D, 2D and 3D channels, at
what precision. they do not decide what a channel means.

a device that cannot sense something reports σ = 1.0 instead of omitting
the channel, so a model reads "unknown" rather than a hole.

second encoder already exists: laptop trackpad + keyboard, two 2D position
channels.


---

## word bank

| tier | n | words |
|---|---|---|
| frame | 11 | who what when where why how yes no not is/are the |
| people, pointing | 10 | I you we it this that here there my your |
| verbs | 16 | go come get give make do have want need like see say know can stop help |
| relations | 10 | in on out up down to from with for of |
| amount, degree | 10 | more less all some none again very too big little |
| time | 8 | now then before after always never today soon |
| joining | 6 | and or but if because so |
| social | 6 | please thanks hello bye sorry good |
| generic nouns | 8 | thing person place time way part one other |

`is/are` is one entry, no conjugation. `the`, instead of `a`({)
