# Room calibration — design

Six live colored markers on the physical 1920px LED ring, moved by
sliders in a phone-friendly web UI (`GET /calibrate`), persisted as named
ring positions. First (and currently only) consumer of the saved
positions: **GORMAPPER**, a projection-mapping app that needs to know
where on the ring its projector's usable window (`projL`/`projR`) sits.

## Overlay model — never touches the running scene

Calibration is a server-side overlay implemented in `calibration.py`
(`CalibrationManager`) and wired into `RingEngine._emit` (`ring.py`).
While active, `_emit` substitutes the calibration overlay frame for
whatever the scene rendered, for **every** consumer fed by `_emit` —
strip segments, satellite dots, canvas taps, terminal preview:

```python
def _emit(self, frame) -> None:
    if self.calibration.active:
        frame = self.calibration.render()
    if self.streaming:
        ...  # segments + satellites, using `frame`
    self.taps.emit(frame)
    ...
```

Crucially, this substitution happens **after** `render_frame()` has
already advanced the real scene by one timestep — the scene's layers,
particle physics, crossfade timers, etc. all keep running normally,
completely unaware that calibration is active. `RingEngine._scene` /
`_old_scene` are never touched by calibration code. So:

- `POST /calibration/done` and `POST /calibration/cancel` both just flip
  `CalibrationManager.active` back to `False` — the very next frame's
  `_emit` call goes back to using the real rendered `frame`, i.e. the
  scene resumes **exactly** where it left off (same scene JSON, same
  animation phase), with no crossfade, no re-render, no discontinuity.
- Satellites keep mirroring normally during calibration — no
  special-casing needed. A satellite anchored somewhere on the ring
  simply mirrors whatever `frame` is at that moment, which during
  calibration is the dim base or a marker passing through its anchor —
  exactly the same code path as mirroring a real scene.
- Canvas taps (including the live `gormapper` tap) keep streaming during
  calibration unaffected — no special-casing needed there either, for
  the same reason: `TapManager.emit(frame)` receives whatever `frame`
  `_emit` decided on that tick. This is actually desirable: a consumer
  watching the tap during calibration sees the markers too.

## Marker rendering

- Base: flat neutral gray at ~5% brightness (`int(round(255 * 0.05))` =
  13 per channel) across the whole ring — dim enough to see the room,
  bright enough that markers pop.
- Each point draws a 5px bright marker: `index - 2 .. index + 2`
  (inclusive), each index taken `mod ring_len` so it wraps correctly
  across the ring seam (e.g. a marker at index 1918 lights up
  1916, 1917, 1918, 1919, 0).
- Colors are exact sRGB hex → uint8 (no gamma round-trip) so what the UI
  shows as a color swatch is what actually lights up.
- If two points overlap, later insertion order in the points dict wins
  for the overlapping pixels (last-write in a plain dict iteration) —
  not expected in normal use (6 named points, spread by default), not
  specially resolved.

## Endpoints

All under the ring-mode `RingManager`; a non-ring `NodeManager` doesn't
expose `calibration_*` methods, so every endpoint 404s with `"calibration
not available"` in that deployment shape (same pattern as `/satellites`
and `/taps`).

### `POST /calibration/start`

Body (optional): `{"points": {name: {"index": int?, "color": "#rrggbb"?}}}`.

Enters calibration mode. Per point:
- `color`: the given value, else the default color for a known point
  name, else a `422` (a custom point name needs an explicit color).
- `index`: the given value, else the previously **saved** index for that
  name (if `POST /calibration/done` was ever called with a matching
  name), else an evenly-spread default.

Body omitted entirely (or `{}`/no `points` key) → the default 6-point
set: `frontL #ff4444`, `frontR #ff44ff`, `backL #4444ff`, `backR #44ffff`,
`projL #44ff44`, `projR #ffff44`, at indices `0, 320, 640, 960, 1280,
1600` (i.e. `ring_len // 6 * i`) unless a calibration was previously
saved, in which case each matching name starts from its saved index.

Response `200`:
```json
{"ok": true, "status": {"active": true,
  "points": {"frontL": {"index": 0, "color": "#ff4444"}, ...}}}
```
`422` on a bad color or an out-of-range index in the request body.

### `POST /calibration/move`

Body: `{"name": index}` (a single point) **or**
`{"points": {"name": index, ...}}` (multiple). Moves live markers —
cheap, safe to call at ~10-20Hz while a UI slider drags (no scene
re-render, no persistence write, just an int assignment + a fresh
overlay `render()` call already happening every frame regardless).

- Unknown point name → `404`.
- Index outside `[0, 1919]` → `422`.
- All names in the request are validated *before* any point is mutated —
  a bad name in a multi-point move applies none of the updates, not a
  partial set.
- Calling while calibration mode isn't active → `422`.

Response `200`: `{"ok": true, "status": {"active": true, "points": {...}}}`
(same shape as `/calibration/start`'s `status`).

### `POST /calibration/done`

No body. Persists all current live point positions to the calibration
store, exits calibration mode. Normal scene rendering resumes on the very
next frame (see overlay model above).

Response `200`: `{"ok": true, "points": {name: index, ...}, "updated": "<iso8601>"}`.

### `POST /calibration/cancel`

No body. Exits calibration mode **without** saving — nothing is written
to the calibration store, whatever was last `done()`-saved (or nothing,
if never saved) remains the saved state. Normal scene resumes instantly.

Response `200`: `{"ok": true, "active": false}`.

### `GET /calibration` — the GORMAPPER consumer contract

Returns the **saved** (persisted) named points, not the live/in-progress
ones:

```json
{"points": {"frontL": 0, "frontR": 320, "backL": 640,
           "backR": 960, "projL": 1280, "projR": 1600},
 "updated": "2026-08-28T19:41:12.345678+00:00"}
```

`points` is `{}` and `updated` is `null` if calibration has never been
`done()`-saved.

**Seam-wrap semantics (important for GORMAPPER):** the projection window
is `ring[projL .. projR]` walking forward around the ring. If
`projL < projR`, the window is the straightforward contiguous slice
`ring[projL:projR]`. **If `projL > projR`, the window crosses the ring
seam** — it wraps through index 0, i.e. it's
`ring[projL:1920] + ring[0:projR]`. GORMAPPER must handle both cases;
this is the same wrap convention used everywhere else in this codebase
(ring seam at 1919→0).

### `GET /calibration/status`

Whether calibration mode is currently active, plus the live (in-progress,
possibly unsaved) point positions and colors while active:

```json
{"active": true,
 "points": {"frontL": {"index": 12, "color": "#ff4444"}, ...}}
```
`points` is `{}` when `active` is `false` (mode was never entered, or was
just cancelled/done).

## `GET /calibrate` — the companion web UI

A single self-contained HTML page (`CALIBRATE_HTML` in `api.py`, served
via `HTMLResponse` — this codebase has no `static/` directory or existing
HTML-serving pattern, so an inline Python string was the simpler,
consistent choice). No external JS/CSS.

Behavior:
- Dark-themed, mobile-first layout. On load, `POST /calibration/start`
  (no body) is called; the response's `status.points` is used to build
  one full-width `<input type=range min=0 max=1919>` slider per point,
  dynamically — **no point names, colors, or indices are hardcoded in
  the page**. Each slider's label text color and thumb accent color are
  set to that point's hex color from the server response, and a live
  numeric readout sits next to the label.
- Dragging a slider fires on `input`; moves are throttled client-side to
  ~10Hz (trailing-value debounce — only the latest value during a 100ms
  window is sent) before `POST /calibration/move`, so a fast drag doesn't
  flood the server with one request per pixel.
- A large **DONE** button calls `POST /calibration/done` and shows a
  "Saved" confirmation message for ~2s.
- A **CANCEL** button calls `POST /calibration/cancel` and shows a
  "Cancelled" confirmation message for ~2s.
- Footer: "markers show live on the ring".

Because the page is built entirely from what the server returns, a future
change to the default point set (names, colors, count) on the server
needs **zero** client-side changes.

## Auto-cancel timeout — decision: none, by design

If the `/calibrate` page is closed (or the phone locks, loses WiFi, etc.)
mid-calibration without hitting Done or Cancel, calibration mode simply
**stays active** until something explicitly calls `/calibration/done` or
`/calibration/cancel`. There is **no** server-side inactivity timeout.

Rationale:
- The room only has one calibration operator at a time, doing a short,
  deliberate, supervised task (walking around comparing marker position
  to physical spot) — not a shared or unattended resource that needs a
  safety net against being left on by accident for hours.
- The overlay is highly visible (dim base + 6 bright colored dots) — an
  operator who left it on will *see* it's still in calibration mode the
  next time they look at the ring, unlike e.g. a silent background
  process. It fails loud, not silent.
- A hidden auto-cancel timer would add a real failure mode of its own: a
  slow/interrupted calibration session (operator gets called away for 12
  minutes) would silently revert to the running scene mid-task with no
  feedback, discarding slider positions the operator was mid-drag on —
  worse than just staying in calibration mode until they come back.
- Recovery is trivial and always available: `POST /calibration/cancel`
  (or loading `/calibrate` again, which re-`start()`s cleanly) restores
  normal operation in one call, from any client, at any time — there's
  no scenario where calibration mode being "stuck" active actually
  blocks anything for long.

If this ever becomes a real operational problem (e.g. calibration gets
triggered by an automated/unattended process instead of a human with eyes
on the room), the natural fix is a `expires_at` timestamp on
`CalibrationManager`, checked cheaply in `RingEngine._emit` the same way
`TapManager.emit()` checks tap TTLs — but that's speculative machinery
for a failure mode that hasn't shown up in practice, so it's deliberately
not built.

## State.json schema

```json
{"calibration": {
  "points": {"frontL": 0, "frontR": 320, "backL": 640,
            "backR": 960, "projL": 1280, "projR": 1600},
  "updated": "2026-08-28T19:41:12.345678+00:00"
}}
```

Only the **saved** (`POST /calibration/done`-committed) points and an
ISO-8601 UTC `updated` timestamp are persisted — never the live,
in-progress slider positions. `StateStore.save_calibration()` /
`load_calibration()` (in `persist.py`) touch only the top-level
`"calibration"` key, preserving whatever `"nodes"`, `"satellites"`, and
`"taps"` currently hold on disk (same pattern as `save_taps`/
`save_satellites`), and vice versa — persisting a scene or a satellite
move never clobbers the calibration key. A missing/corrupt/odd-shaped
`"calibration"` key or file falls back cleanly to
`{"points": {}, "updated": None}`, never blocking startup.

## Files

- `lightserver/lightserver/calibration.py` — `CalibrationManager`,
  `CalibrationPoint`, `DEFAULT_POINTS`, `spread_indices`,
  `validate_hex_color`.
- `lightserver/lightserver/ring.py` — `RingEngine.calibration` +
  `_emit` overlay substitution; `RingManager.calibration_*` methods +
  `restore()`/`persist()` wiring.
- `lightserver/lightserver/persist.py` — `StateStore.load_calibration()`
  / `save_calibration()`.
- `lightserver/lightserver/api.py` — the 5 `/calibration*` endpoints,
  `GET /calibrate`, `CALIBRATE_HTML`.
- `lightserver/tests/test_calibration.py` — unit tests for
  `CalibrationManager` (lifecycle, seam-wrap rendering, persistence
  round-trip/non-clobbering), `RingEngine` overlay-vs-scene isolation
  tests, and API-level lifecycle/validation/contract tests.

## Interaction with output-delay AV sync (`docs/SYNC_DESIGN.md`)

Room calibration and output-delay AV sync are two independent server-side
overlays/buffers on the same `RingEngine._emit`, and they're ordered
deliberately: **calibration wins**. While `self.calibration.active`,
`_emit` bypasses the sync delay buffer entirely — the overlay frame goes
out to strips, satellites, AND taps identically, undelayed — because the
calibration UI's marker sliders need to respond to drags instantly, and
there's no meaningful "current scene" to delay while calibration owns the
output anyway. The delay buffer resumes (with a brief empty-buffer
ramp-up, same as at boot) the moment calibration ends.
