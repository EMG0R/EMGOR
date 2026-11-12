# Canvas taps — design

A **tap** is a self-expiring UDP subscription that streams a slice of the
live ring canvas (`ring[offset..offset+count]`) to an external consumer —
not a strip, not a satellite, just a raw copy of the pixels for something
outside the room-light system to consume. First consumer: **GORMAPPER**, a
projection-mapping app that wants a live feed of the ring to reproject
onto physical surfaces.

Unlike strips/satellites, a tap has **no entry in nodes.json**, no mDNS,
no firmware assumptions, and no persistent identity beyond a name the
caller chose. It exists only as long as someone keeps asking for it.

## Consumer contract (implement this exactly — agreed with GORMAPPER's team)

**Wire format: Frame Protocol v1** (see `protocol.py`), little-endian:

```
bytes 0-1 : magic 'F','C'
byte  2   : version = 1
byte  3   : flags   (bit0 = SHOW)
bytes 4-5 : uint16  pixel offset (absolute ring index)
bytes 6-7 : uint16  pixel count N
then 3*N bytes RGB
```

**Chunking.** Each rendered frame is split into datagrams of **at most
480 pixels** (`TAP_CHUNK_PIXELS` in `taps.py`) — a full 1920px ring tap is
**4 packets per frame**, ~1446 bytes each, safely under a typical WiFi
MTU. Each chunk carries its own correct absolute `offset`/`count` (not
relative to the tap's own window). **SHOW (flag bit0) is set ONLY on the
LAST chunk of each frame** — GORMAPPER should buffer chunks per frame and
treat SHOW as "frame complete, present it," exactly like the strip
firmware treats SHOW as "latch the buffer."

Example: default full-ring tap (offset 0, count 1920), one frame:

| packet | offset | count | flags     |
|--------|--------|-------|-----------|
| 1      | 0      | 480   | 0         |
| 2      | 480    | 480   | 0         |
| 3      | 960    | 480   | 0         |
| 4      | 1440   | 480   | SHOW (1)  |

A narrower tap (e.g. `offset=1000, count=500`) still chunks the same way,
just fewer/smaller packets: `(1000,480,flags=0)`, `(1480,20,SHOW)`.

**fps_divisor.** A tap can ask for a fraction of the engine's 30fps by
setting `fps_divisor` (integer ≥ 1): the tap sends every Nth engine
frame. `fps_divisor=2` → ~15fps, `fps_divisor=3` → 10fps, etc. Default 1
(every frame, ~30fps).

## Presence / TTL — the "self-expiring" part

Each tap has a `ttl` in seconds (default 15). `POST /taps/{name}` is both
**create** (first call, `port` required) and **keepalive** (later calls —
`port` optional, reuses whatever was last set):

```
POST /taps/gormapper
{"port": 21325, "fps_divisor": 2, "ttl": 15}
```

Every accepted POST resets `expires_at = now + ttl`. **The destination
host is always derived from the HTTP request's source IP
(`request.client.host`), never from the request body** — a tap cannot be
pointed at an arbitrary third-party host by a spoofed body field.

**Recommended keepalive cadence: every 5 seconds** for the default 15s
TTL (3 keepalives' worth of margin against one dropped/delayed HTTP
request). GORMAPPER should treat "no more frames arriving" defensively
regardless — a missed keepalive silently stops the stream, no error is
pushed to the consumer.

If no keepalive arrives within `ttl` seconds, the tap disables itself the
next time the engine's render loop checks (a cheap monotonic-clock
comparison in `TapManager.emit()`, no background timer) and the engine
**skips all slice/send work for it** from then on — zero steady-state
overhead for an abandoned tap.

`DELETE /taps/{name}` (or `POST /taps/{name} {"enabled": false}`) disables
immediately, same effect as a TTL expiry, without waiting.

`GET /taps` lists every configured tap (enabled or not): `name`, `host`,
`port`, `offset`, `count`, `fps_divisor`, `ttl`, `semantic_port`,
`enabled`, `seconds_until_expiry`, `frames_sent`.

## Persistence

Only **config** — `port`, `fps_divisor`, `ttl`, `offset`, `count`, and
(when set) `semantic_port`, keyed by tap name — is persisted, in
`state.json`'s own top-level `"taps"` key
(alongside `"nodes"` and `"satellites"`, each written independently so
none clobber each other — see `persist.py`).

**Never persisted: `enabled` and `host`.** After a lightserver restart,
every previously-configured tap exists again (so `GET /taps` shows it)
but starts **disabled**, with `host: null`. A live keepalive re-enables it
and re-derives `host` from that request's source IP — a stale consumer
can't silently keep receiving frames across a restart, and a restart
never streams to a host that hasn't reconfirmed it's still listening.

## Validation (422)

- `port`: 1..65535
- `fps_divisor`: ≥ 1
- `ttl`: 1..3600 seconds
- `offset`: ≥ 0
- `count`: ≥ 1
- `offset + count`: ≤ ring length (1920 for the current 3-strip ring)
- `semantic_port` (optional): 1..65535
- missing `port` on first create of a not-yet-configured tap name

`DELETE /taps/{name}` on a name that was never created → 404.

## Semantic side-channel

A tap can additionally set `semantic_port` (1..65535) to get ground-truth
scene telemetry in parallel with the raw pixel stream — GORMAPPER (or any
consumer) that wants particle positions/colors/velocities and coarse
canvas stats without having to image-process the RGB feed itself:

```
POST /taps/gormapper
{"port": 21325, "semantic_port": 21326, "fps_divisor": 2, "ttl": 15}
```

**Cadence.** One JSON UDP datagram every `SEMANTIC_FRAME_DIVISOR` (3)
engine frames — 10Hz at the engine's fixed 30fps — sent to
`(same derived host, semantic_port)`. This is independent of the tap's
own `fps_divisor` (which only throttles the *pixel* stream). Same
TTL/presence rules as the pixel stream: an expired or disabled tap sends
nothing, on either channel; a tap without `semantic_port` set never gets
a semantic packet at all.

**Packet** (compact JSON, `separators=(",", ":")`, kept ≤ ~1400 bytes so
it's always one WiFi-MTU-safe datagram):

```json
{"v":1,"seq":42,"ts":1735000000.123,"scene":"comet","output_delay_ms":150,
 "stats":{"energy":0.12,"dominant":["#ffdd00","#402000"],"strobe_phase":null},
 "tracks":[{"id":3,"layer":0,"pos":812.4,"vel":420.0,
            "color":"#ffdd00","size":3.0,"brightness":1.0}],
 "projector":null}
```

- `ts`: `time.time()` (Pi unix time, float seconds) at the moment this
  packet was built — a consumer can diff it against its own clock
  (ideally after estimating clock offset via `GET /time`, see below) to
  measure/verify one-way latency of the semantic channel.
- `stats.energy`: mean canvas luminance, 0..1 (cheap numpy over the just-
  rendered uint8 frame — same weights as standard Rec.601 luma).
- `stats.dominant`: top-2 colors by a coarse (4 bits/channel) quantized
  histogram, **excluding near-black pixels** (luminance < 8/255) so a dark
  scene reports its actual accent colors instead of "mostly black". If
  every pixel is near-black, `["#000000"]`.
- `stats.strobe_phase`: `0..1` position within the current strobe cycle
  when `master.strobe.mode` is active, else `null`.
- `tracks`: ground truth from **particle layers only** — field layers
  (`wave`/`noise`/`plasma`/`gradient`/`solid`/`paint`) contribute none.
  Pooled across every particle layer in the scene and truncated to the
  **top-8 by `brightness`** (`semantic.py::MAX_TRACKS`), which is what
  actually guarantees the size cap regardless of how many particles are
  alive; `pos` is the canonical wrapped ring position (`0..ring_len`),
  `vel` is signed px/s, `color` is the fully resolved per-particle color
  (same `color_mode`/palette resolution as rendering — see
  `physics.py::ParticleSystem.resolved_colors`, a thin public wrapper
  around the existing `_dynamic_colors`), `size` is the particle's
  gaussian sigma in px, `brightness` is `0..1` (spawned/resolved color
  intensity × the same lifetime fade-out curve rendering uses). `id` is a
  monotonic per-particle id assigned at spawn (`ParticleSystem.pid`,
  survives compaction) so a consumer can track "the same particle" frame
  to frame; it is *not* unique across different particle layers on its
  own — pair it with `layer` for a compound key.
- `projector`: the scene's own top-level `"projector"` block, forwarded
  **verbatim**, or `null` when the scene has none. See the next section.
- `output_delay_ms`: the current LED output-delay AV sync setting
  (`0..500`, see `docs/SYNC_DESIGN.md`). Frames on this tap (pixel AND
  semantic) are **always the current, undelayed frame** — the delay only
  holds back the physical strip/satellite outputs. This field just tells
  GORMAPPER how far behind the LEDs currently are; GORMAPPER's own trim
  should be `0` whenever this is nonzero (the server is already doing the
  alignment work on its side).

**Perf.** Building a semantic packet touches particle SoA arrays and does
a numpy histogram over the canvas — real but bounded work, so it's done
**only** on the 1-in-3 frames it's actually sent, and skipped entirely
(no scan of `taps` beyond a cheap attribute check) when no enabled tap
has `semantic_port` set. One packet is built per qualifying engine frame
and reused (re-sent as identical bytes) across every tap that wants it
that frame, rather than rebuilt per tap.

## Clock sync (`GET /time`)

`GET /time` returns `{"unix": <float>, "monotonic": <float>}` — `time.time()`
and `time.monotonic()` on the Pi at request time. No auth, no state, trivially
cheap. It exists so a consumer can estimate the Pi's clock offset the usual
min-RTT way: poll it a handful of times, take the sample with the smallest
round-trip, and assume the Pi's `unix` time was roughly `unix + rtt/2` at the
moment the local reply was received. Combine with a semantic packet's `ts`
field to measure/verify end-to-end latency of the semantic channel.

## The `"projector"` scene-DSL key — wall programs (v2)

Scenes may carry an optional top-level `"projector"` key: any JSON
object, validated only for *shape* (`validate_scene` rejects a
string/array/number there, accepts any object) — **the engine never
interprets it**. It is stored with the scene, returned verbatim by
`GET /scene`, and forwarded verbatim in every semantic packet (`null`
when the scene has none). Precedence: **the scene's own `projector`
block, if present, is authoritative** — a consumer should prefer it over
any of its own defaults/heuristics for that scene.

The vocabulary below is **owned by the projection-mapping consumer**
(GORMAPPER), not validated or enforced by the lightserver — documented
here purely so scene authors (`nl.py`'s prompt spec, the Pi's
`room-lights` skill, humans) know what a `projector` block can say. All
keys optional; unknown keys are ignored by the consumer.

```json
"projector": {
  "style": "particles" | "aurora" | "flowfield" | "wash" | "react" | "bleed",
  "params": {"density": 0.0, "speed": 0.0, "turbulence": 0.0,
             "hue_drift": 0.0, "fade": 0.0},   // each 0..2 (hue_drift 0..1)
  "energy": 0.6,
  "palette": ["#ff2f6e", "#00e0ff"],
  "brightness": 0.85,
  "glsl": "<fragment body>"
}
```

**`glsl`, when present, OVERRIDES `style`/`params`** — it's an on-the-spot
authored fragment program instead of a named preset. Interface the
authored code must target:

- Define `vec3 wall(vec2 uv, float t)` — `uv` is 0..1 across the wall
  zone (y down), `t` is seconds; return linear-ish RGB.
- Available to it: `ringAt(u) -> vec3` (calibrated LED window colors at
  ring position `u`, 0..1), `uEnergy` (0..1.5, smoothed room energy),
  `uPalA`/`uPalB` (vec3, dominant scene colors), `uComets[8]`
  `vec4(u, age_s, vel, strength)` + `uCometCount` (ground-truth moving
  objects, sync-corrected — see `semantic.py`'s `tracks`), and
  `rhash`/`rnoise`/`rfbm` noise helpers.
- The consumer hot-compiles with last-good fallback: **broken glsl never
  blanks the wall**. Status readback: check the wall status endpoint per
  GORMAPPER's own docs — `TODO: exact URL` (expected under
  `/api/wall/status`, not yet finalized as of this writing). A scene
  author that posts `glsl` should check that endpoint after the change
  and be prepared to regenerate on a reported compile error.

**Core mechanic — "a pin dropped in water".** Treat a moving object
crossing the LED window (a comet track) as striking the wall like a
surface: its ripple/wake **expands outward from that `u` position, all
the way to the screen edges** — full-screen generative reach, never a
band hugging a strip-shaped region. **Color truth**: sample `ringAt(u)`
at the event's/source's own position and let *that exact color* flow
outward from `u` — spatial continuity, not a re-picked or averaged hue.
What leaves the strip at `u` continues down/across the wall from `u`.

**Creative doctrine** (bias baked into `nl.py` and the room-lights
skill — a center of gravity, not a formula): favor **particle systems**,
**aurora-like fields**, and **ripple/wake propagation** over flat washes.
Ring data is **influence, not image** — `uEnergy` drives spawn
rate/tempo, `ringAt(u)` drives color at the point of contact, comets are
**events** (ripples/gusts/blooms/showers at their `u`, expanding
full-screen), never a literal reproduction. **Never a mirrored strip** —
the wall is its own canvas. **Agent discretion is first-class**: the
scene-writing agent decides what actually goes on the wall each scene —
vary style, palette use, and motion between scenes; full creative
latitude, full vibes, never a rote template.

Example scene with a declarative projector block:

```json
{
  "name": "comet",
  "layers": [ ... ],
  "projector": {"style": "react", "palette": ["#ffdd00"], "energy": 0.5}
}
```

Example scene with an authored glsl wall program (comet ripples: pin-
in-water rings expanding full-screen, colored by `ringAt` at the comet's
own position):

```json
{
  "name": "deep-space-aurora",
  "layers": [ ... ],
  "projector": {
    "palette": ["#0a1a3a", "#2fe0c0", "#7a2fe0"],
    "glsl": "vec3 wall(vec2 uv, float t) { vec3 col = vec3(0.0); for (int i = 0; i < 8; i++) { if (i >= uCometCount) break; vec4 c = uComets[i]; float d = distance(uv, vec2(c.x, 0.5)); float radius = c.y * 0.6; float ring = exp(-pow(d - radius, 2.0) * 60.0) * exp(-c.y * 0.3); col += ringAt(c.x) * ring * c.w * (0.5 + uEnergy); } return col; }"
  }
}
```

## Implementation notes

- `lightserver/taps.py`: `TapRuntime` (per-tap state) + `TapManager`
  (owns all taps for one `RingEngine`, does slice/chunk/send in
  `emit()`, one shared raw UDP socket — no mDNS needed since the host is
  always a concrete IP from the HTTP request).
- `RingEngine.taps` (a `TapManager`) is created alongside `senders`/
  `satellites` in `__init__` and driven from `_emit()` on every rendered
  frame, independent of `self.streaming` (a tap is presence-based, not
  tied to whether the strips themselves are currently streaming).
  `RingEngine.shutdown()` closes the tap socket.
- `RingManager` exposes `taps_status()` / `tap_status(name)` /
  `set_tap(...)` / `disable_tap(name)`, and its `restore()`/`persist()`
  round-trip tap config through `StateStore.load_taps()`/`save_taps()`.
- `api.py`: `GET /taps`, `POST /taps/{name}`, `DELETE /taps/{name}` —
  only available when the manager is ring-backed (`taps_status`/`set_tap`
  attributes present); `[]`/404 otherwise, same pattern as `/satellites`.
- Taps are a ring-only feature (they read the whole 1920px canvas before
  it's sliced per strip); a non-ring single/multi-node deployment simply
  has no taps.
- `lightserver/semantic.py`: pure functions, no state — `build_semantic_packet(seq,
  scene, frame)` builds one packet dict from a live `SceneInstance` +
  its just-rendered uint8 canvas (`_energy`/`_dominant`/`_strobe_phase`/
  `_particle_tracks` are the individual pieces, each independently
  testable). `TapManager.emit()` (in `taps.py`) owns the cadence gate
  (`SEMANTIC_FRAME_DIVISOR = 3`) and the actual `sendto()`s;
  `RingEngine._emit()` passes it the live `SceneInstance` (read under
  `self._lock`, same as `current_scene`).
- `physics.py::ParticleSystem` gained `pid` (a monotonic per-particle id
  array, assigned at spawn, filtered by `_compact` like every other SoA
  array — never reused) and `resolved_colors(t)`, a one-line public
  wrapper around the pre-existing `_dynamic_colors(t)` so `semantic.py`
  doesn't have to duplicate the `color_mode`/palette resolution logic.
