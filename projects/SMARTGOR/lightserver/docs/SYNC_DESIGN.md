# Output-delay AV sync — design

The room has two visible outputs driven from the same ring canvas: the
LED strips/satellites (near-zero latency, direct UDP to firmware) and a
projector, fed by a canvas tap through GORMAPPER's own render pipeline
onto physical surfaces. That tap → render → glass chain has ~100-200ms of
**unavoidable** latency GORMAPPER can't remove. Without correction, the
wall visibly lags the LEDs for anything with a hard onset (a strobe, a
particle spawn, a color cut).

## The fix: delay the fast output, not the slow one

Rather than trying to predict or speed up the projector path, hold the
**physical LED outputs** back by `output_delay_ms` so they land in step
with the projector instead. Canvas taps (GORMAPPER's own feed — both the
raw pixel stream and the semantic side-channel, see `docs/TAPS_DESIGN.md`)
keep getting the freshest frame immediately; they're the ones already
carrying the latency, so delaying them too would just double it.

This works for **any** content — no scene-specific tuning, no per-effect
prediction — because it's a pure output-side buffer, not a model of what
the projector will render.

## Implementation (`lightserver/sync.py`, wired in `ring.py`)

`SyncManager` owns:
- `output_delay_ms` (`0..500`, default `0`), live-settable.
- a bounded ring buffer (`collections.deque`) of the last N rendered
  uint8 `(ring_len, 3)` frames, `N = ceil(delay_ms / frame_period_ms) + 1`
  at the **maximum** delay (500ms) — at 30fps that's 16 frames × 1920px ×
  3 bytes ≈ 90KB, trivial. The buffer is always sized for the max so
  raising the delay never needs a reallocation.

`RingEngine._emit` (see `ring.py`), once per rendered frame:

```python
if self.calibration.active:
    frame = self.calibration.render()
    delayed = frame                       # bypass: overlay everywhere, undelayed
else:
    self.sync.push(frame)                 # record the CURRENT frame
    delayed = self.sync.delayed_frame(frame)   # nearest frame from delay_ms ago

# strips + satellites (+ their boost pipeline) render from `delayed`
# taps (pixel AND semantic) render from `frame` (always current)
```

- **Nearest-frame, no interpolation** — simpler, and at 30fps the
  quantization error is at most one frame period (~33ms), well under
  perceptible sync tolerance for this use case.
- **Calibration bypass**: while room calibration (`docs/CALIBRATION_DESIGN.md`)
  is active, the delay buffer is skipped entirely and the overlay frame
  goes out to strips, satellites, AND taps identically, undelayed — the
  calibration UI's marker sliders need to respond instantly, and there's
  no "current scene" being delayed anyway during calibration.
- **Startup ramp-up**: if the buffer doesn't yet hold enough history for
  the requested delay (e.g. right after boot or right after raising the
  delay), `delayed_frame()` clamps to the oldest frame available rather
  than raising or underflowing — the delay is briefly shorter than
  requested until the buffer fills, then converges.
- **Live changes**: setting `output_delay_ms` takes effect on the very
  next frame — no restart, no buffer resize (see above).

## API

```
GET  /sync                              -> {"output_delay_ms": int}
POST /sync   {"output_delay_ms": int}   -> {"output_delay_ms": int}
                                            0..500, 422 if out of range
```

`POST /sync` persists immediately (like a scene/satellite/tap change) —
see Persistence below.

### Semantic packet field

Every semantic packet (`docs/TAPS_DESIGN.md`) carries
`"output_delay_ms": <int>` — the value in effect when that packet was
built. This lets GORMAPPER know "the LEDs are currently N ms behind me,
you don't need your own trim on top of this" — that's GORMAPPER's side to
act on; the server's job here is just exposing the number honestly.

## Persistence

`{"output_delay_ms": int}` under a new top-level `"sync"` key in
`state.json`, written/read independently of `nodes`/`satellites`/`taps`/
`calibration` (same pattern as every other top-level key — see
`persist.py`'s `save_sync()`/`load_sync()`). A missing/corrupt/
out-of-range value falls back cleanly to `0` and never blocks startup.

## Calibration page (`GET /sync/ui`)

A phone-first, self-contained HTML page (same visual style as
`GET /calibrate` — dark, no external deps):

- **One big slider**, "LED delay", `0..500ms` step `5ms`, live readout.
  Drag events POST to `/sync` throttled to ~10Hz (trailing value only —
  same pattern as the calibration page's marker-move throttling).
- **START PULSE / STOP PULSE** toggle button: `POST /sync/pulse/start`
  activates the installed `scenes/sync-pulse.json` scene (the whole ring
  flashes bright white at exactly 1.000Hz with a hard on/off edge, no
  fade — the eye judges sync purely by flash *onset*). Stopping (button
  toggle, or **DONE**) calls `POST /sync/pulse/stop`, which restores
  **exactly** whatever scene was running before the pulse started.
- **DONE**: stops any running pulse and confirms — the delay value is
  already live and persisted (each drag already POSTed), there's nothing
  extra to save.
- On-page instructions: *"Start pulse. Watch wall + LEDs flash. Drag
  until they flash as ONE. Done."*

### `/sync/pulse/start` and `/sync/pulse/stop`

```
POST /sync/pulse/start   -> {"ok": true, "scene": "sync-pulse"}
POST /sync/pulse/stop    -> {"ok": true, "scene": "<whatever was running before>"}
```

`RingManager` remembers the pre-pulse scene in `RingEngine._pre_pulse_scene`
(an in-memory field, not persisted — a pulse in progress across a restart
isn't a scenario worth supporting) the first time `sync_pulse_start()` is
called while no pulse is already in progress, so a second `start` call
(e.g. a page reload) doesn't clobber the remembered scene with the pulse
scene itself. `sync_pulse_stop()` restores that scene verbatim via
`set_scene(..., crossfade=0)` and clears the remembered value; calling
`stop` with no pulse in progress is a harmless no-op that just reports the
currently-running scene.

## Why this is the calibration measurement, not just a UI nicety

While the pulse scene plays: GORMAPPER's projector renders the
**undelayed** tap feed (taps are exempt from the delay by design), so the
wall flashes at its natural (laggy) latency relative to the ring's
render clock. The LEDs flash `output_delay_ms` behind the *ring's* render
clock. Dragging the slider until the two flashes are perceived as
**one** event means: `output_delay_ms` (LED lag behind the ring) now
equals the projector's actual end-to-end latency (wall lag behind the
ring) — i.e. **the slider's final value literally *is* the measured
projector latency**, not an approximation of it. That's why no separate
"measure latency" step exists; calibrating IS measuring.
