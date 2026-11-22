# Satellite "dot" nodes — design

Small standalone LED blobs (2 ESP32 boards, `dot1`/`dot2`, 7 NeoPixels
each, mDNS `dot1.local`/`dot2.local`) that belong to the room's one
1920px ring but sit at repositionable physical locations — anywhere in
the room, not necessarily touching a strip.

## Config

Two files, same style as `nodes.json` / `ring.json`:

**nodes.json** — each dot is a normal node entry (host, pixel_count=7,
same UDP Frame Protocol v1):

```json
{"name": "dot1", "host": "dot1.local", "ip": null,
 "pixel_count": 7, "enabled": true}
```

**satellites.json** (new, root, optional/additive — a missing file or
empty list means zero satellites and changes nothing else):

```json
{"satellites": [
  {"node": "dot1", "anchor": 0,   "mode": "mirror", "color": null},
  {"node": "dot2", "anchor": 960, "mode": "mirror", "color": null}
]}
```

- `node` — must exist in `nodes.json`; that's where host/ip/port/pixel
  count live.
- `anchor` — a ring index `0..1919`, the point on the 1920px ring map
  the dot visually sits at. Runtime-repositionable
  (`POST /satellites/{name} {"anchor": N}`) **without** touching the
  ring's 1920px topology or the 3 strip segments — the ring stays
  exactly as-is; a dot just re-samples a different window of the same
  ring canvas next frame.
- `mode` — `"mirror"` (default) or `"independent"`. Explicit and
  validated so more modes can be added later.
- `color` — hex string, only used in `"independent"` mode.

The shipped placeholders (`dot1@0`, `dot2@960` — opposite sides of the
loop) are marked as placeholders pending physical testing in the room.

## Modes

- **mirror** (default): the dot shows a **7-pixel window of the ring
  canvas, centered on `anchor`, wrapping at the 0/1919 seam** — i.e.
  `ring[(anchor-3) % 1920 .. (anchor+3) % 1920]`. Chosen over "flood the
  anchor pixel's single color across all 7 LEDs" because it's exactly as
  cheap (one 7-wide numpy fancy-index per frame) but preserves whatever
  is actually crossing that point of the ring — the orbiting orb, a
  wave, a trail — as a small moving/blended blob instead of flattening
  it to a flat color. It reads as "part of the ring organism", which is
  the point.
- **independent**: the dot ignores the ring and shows a flat `color` on
  all 7 pixels. Set via `POST /satellites/{name} {"mode": "independent",
  "color": "#rrggbb"}`; release it by switching back to `"mirror"`
  (anchor can be changed in the same call).

Rendering happens inside `RingEngine._emit` right after the 3 strip
slices are dispatched — 14 extra pixels per frame, no extra render pass,
no extra clock.

## Boost — brightness-boost mechanism

The dots are physically weaker than the ring strips (7px, 3V3-powered):
when mirroring a dim ambient scene they can look anemic next to the
strips they're anchored on. `boost` is a per-satellite setting, applied
only in **mirror** mode — in **independent** mode the flat `color` is
already an explicit, maximal color choice, so boost is a documented
no-op there (`SatelliteRuntime._apply_boost` is simply never called: the
independent-with-color branch of `render()` returns first).

Three forms (both stored under `satellites.py`'s `normalize_boost()`,
which is the single source of truth for validation/canonicalization —
API, config-file load, and persisted-state restore all route through
it):

- `"off"` (default) or `1.0` — both spellings accepted on input, always
  canonicalized to `"off"` on output/persistence. Unchanged mirror
  window.
- a positive number, e.g. `2.5` — a **fixed multiplier** on the mirror
  window's RGB. Clipped at 255 by *rescaling the whole triple*, not
  per-channel saturation: if `window * gain` would blow out a channel,
  all three channels are scaled back down by the same factor so hue/
  ratios are preserved instead of desaturating toward white
  (`scale_preserve_hue()`).
- `"auto"` (the recommended default going forward) — per-frame **peak
  normalization**: each frame, the mirror window's RGB is scaled so its
  brightest channel hits exactly 255, so the dot is always maximally
  bright while every color ratio (hue, and the relative structure across
  the 7 pixels) stays intact. Two guard rails, both in
  `lightserver/satellites.py`:
  - **floor + max-gain cap** (`AUTO_BOOST_FLOOR = 8.0`,
    `AUTO_BOOST_MAX_GAIN = 12.0`): a fully black window (peak 0) stays
    black — `0 * gain` is still `0`, no divide-by-zero needed. A
    near-black window (peak `< 8`, out of 0..255) would need a huge gain
    to fully normalize (e.g. peak 1 -> gain 255), which would amplify
    flicker/single-pixel noise into a false bright flash — so below the
    floor the gain is capped at `12x` instead of fully normalizing. At
    or above the floor, full normalization (`gain = 255 / peak`) is used
    even though that can exceed 12x right at the floor (255/8 ≈ 31.9) —
    the cap is deliberately a below-floor-only guard, not a global
    ceiling.
  - **peak-hold envelope** (`BOOST_ENVELOPE_RELEASE_DECAY` — see
    "Peak-hold envelope" section below, **2026-08 fade-out bugfix**): the
    gain each frame is driven off a per-satellite envelope that jumps up
    INSTANTLY to a new brighter peak but only decays back down SLOWLY
    (~3s half-life) once the peak drops. This replaced a symmetric
    exponential smoothing (`AUTO_BOOST_SMOOTHING_ALPHA = 0.25`,
    `gain_t = gain_{t-1} + 0.25 * (target_t - gain_{t-1})`) that is kept
    only as a historical constant — see the bugfix section for why it had
    to change.
  - `GET /satellites` reports the live gain as `effective_gain`
    (for `"off"` this is always `1.0`; for a numeric boost it's just
    that number) — useful for confirming "auto" is actually boosting a
    dim scene without having to eyeball the LEDs.

Considered and **not added** (as a modifier of `"auto"` specifically): a
separate per-satellite saturation push bolted onto peak normalization.
That would fight `"auto"`'s "preserve relative coloring" guarantee. That
saturation control instead shipped as its own boost mode — see `"vivid"`
below — which is a genuinely different algorithm (per-pixel, not
global), not a modifier of `"auto"`.

- `"vivid"` — PEAK-ANCHORED hue-preserving boost (**revised 2026-08** —
  see "Bugfix history" below for what this replaced and why). The
  comparison to `"auto"` is closer than it first looks, worth stating
  plainly:

  - **`"auto"`** computes **one global gain** for the entire window from
    the window's peak *channel* value (0..255 RGB scale), so the
    window's internal relative brightness structure across the 7 pixels
    is preserved exactly — only *overall* brightness changes.
  - **`"vivid"`** also anchors to the window's peak — but computed in
    HSV, per-pixel, as each pixel's own `V` relative to the window's
    peak `V` (`V_peak`) — and additionally guarantees exact hue (and,
    optionally, a saturation floor) rather than leaving RGB ratios to
    fall out of a linear channel scale. The brightest pixel in the
    window still lands at max; every other pixel keeps its proportional
    position below it — **a dim pixel next to a bright one stays visibly
    dimmer**, same guarantee as `"auto"`, just computed in HSV with
    hue/saturation control on top. An optional `vivid_lift` exponent
    (default `0.7`) then mildly reshapes that proportional ratio for
    perceptual punch, without ever letting a dimmer pixel catch up to
    the peak.

  ### Bugfix history — why this changed

  The **original** `"vivid"` (per-pixel-independent maximization: every
  pixel at/above the black guard pushed to `V=1.0` regardless of its
  neighbors) shipped first and was live on the room's yellow-comet
  scene. Field report: *"the trails do not work now looks bad"* — the
  comet's fading trail, mirrored into the dot's 7px window, rendered as
  a flat block of max-yellow instead of a fade, because every trail
  pixel above the guard got maxed to the same brightness as the comet's
  head. User's corrective spec: *"it should still get dimmer when it
  gets dimmer — brightest for brightest."* That is a **peak-anchored**
  requirement, not a per-pixel-independent one, so the algorithm below
  replaces the old one outright (same mode name, same config field,
  genuinely different math).

  Algorithm (`apply_vivid()` in `lightserver/satellites.py`, using the
  vectorized `rgb_to_hsv_np()` / `hsv_to_rgb_np()` helpers — the whole
  window converted in one numpy call, no per-pixel `colorsys` loop):
  1. Convert the window to HSV.
  2. **H is always kept exactly as computed** — hue-accurate, unchanged
     from before: whatever color a pixel is, it stays that color, just
     brighter/more saturated.
  3. **Find `V_peak`** = the max `V` across the whole window this frame.
     If `V_peak < VIVID_BLACK_GUARD` (`8/255`, same value/philosophy as
     `AUTO_BOOST_FLOOR`), the **entire window** is left completely
     unchanged — "black stays black" — no divide-by-zero risk.
  4. Otherwise, per pixel **at/above** `VIVID_BLACK_GUARD`:
     `V_out = clip(V_in * gain, 0, 1) ** vivid_lift`, where `gain` is
     the (temporally-smoothed — see below) `1 / V_peak` factor. Because
     `gain` is identical for every pixel in the window, this is a pure
     proportional rescale: the peak pixel (`V_in == V_peak`) always
     lands at `1.0` pre-lift, and pixel **order is preserved** — a
     dimmer input pixel can never end up brighter than a brighter one
     (`x -> x**lift` is monotonic on `[0, 1]` for `lift > 0`). Pixels
     **below** `VIVID_BLACK_GUARD` are left completely unchanged
     regardless of `V_peak` — "dark stays dark", same as before, still
     applied per-pixel.
  5. **Lift shaping**: the new per-satellite field `vivid_lift`
     (`float`, default `0.7`, valid range `[0.3, 1.5]`,
     `validate_vivid_lift()`) is the exponent in step 4.
     - `1.0`: strictly linear/proportional — a pixel at half the peak's
       `V` comes out at exactly half the peak's `V_out`.
     - `< 1.0` (default `0.7`): a mild perceptual lift — dim trail
       pixels get relatively punchier without ever reaching the peak's
       brightness (`0.5 ** 0.7 ≈ 0.616`, still well under `1.0`).
     - Range is deliberately narrow (`0.3..1.5`) so it can only ever
       *reshape* the fade, never flatten it back into a block — that
       would reintroduce the original bug.
  6. **Saturation**: per-satellite field `vivid_saturation`
     (`float | None`, default `None`) — unchanged from before.
     - `None` (default): `S` is kept exactly as computed — fully
       hue-*and*-saturation-accurate, only `V` changes.
     - a float in `[0, 1]`: acts as a **floor**,
       `S_out = max(S_in, floor)` — only raises washed-out pixels toward
       full saturation, never lowers a pixel that was already more
       saturated than the floor.
  7. Convert back to RGB (round, clip 0..255).

  **Temporal control — peak-hold envelope (2026-08 fade-out bugfix,
  replaces the earlier symmetric smoothing).** See "Peak-hold envelope"
  below for the full writeup; summary: the `1 / V_peak` factor is now
  driven by a per-satellite envelope `E` (`E_t = max(V_peak_t, E_{t-1} *
  BOOST_ENVELOPE_RELEASE_DECAY)`, state field `_vivid_envelope`, feeding
  `_vivid_peak_gain = 1 / max(E, VIVID_BLACK_GUARD)`) instead of the
  raw per-frame `1/V_peak` smoothed symmetrically. Both fields are not
  persisted — per-frame render artifacts, re-settling from `0.0`
  (envelope) / `1.0` (gain) after a restart. The `vivid_lift` exponent
  itself remains a **static** per-satellite config value, not a
  per-frame signal, so it is not run through the envelope either.

  **Effective-gain reporting**: `GET /satellites`' `effective_gain` for
  `"vivid"` is the **mean** per-pixel gain (`V_out / V_in`) across pixels
  at/above the guard, computed fresh every render call — `1.0` if the
  whole window is at/under the guard (nothing changed). With
  `vivid_lift = 1.0` this mean is a single constant value (every pixel
  shares the same `1 / V_peak` gain); with `vivid_lift < 1.0` dimmer
  pixels have a slightly higher individual gain than the peak pixel,
  which the mean reflects.

  ### Peak-hold envelope (2026-08) — the fade-out bugfix

  **Field report**: a bright comet head crossing a dot's mirror window
  left a fading trail behind it — and the trail didn't fade. It held
  near-max brightness through most of what should have been a smooth
  fade-out, then abruptly snapped to black once it crossed
  `VIVID_BLACK_GUARD`. User: *"the fadeout does not work — it should
  fade out smoothly; we just wanted bias towards brighter."*

  **Root cause**: both `"auto"` and `"vivid"` computed their gain as a
  raw PER-FRAME peak target (`255/peak` or `1/V_peak`), only ever
  smoothed *symmetrically* (the old `AUTO_BOOST_SMOOTHING_ALPHA` EMA —
  rises and falls at the same rate). As the comet head exited the
  window, the window's own peak shrank frame over frame (only the
  dimmer trail remained), so the *target* gain kept **rising** — and the
  symmetric EMA dutifully chased it upward, re-amplifying the fading
  tail back toward max instead of letting it fade. Only once the trail's
  raw value finally dropped below the black guard did anything go dark —
  hence the "holds bright, then snaps to black" symptom.

  **Fix**: replace the raw per-frame target (and its symmetric smoothing)
  with a per-satellite **peak-hold envelope** `E` — the same
  attack/release-envelope idea as an audio compressor/AGC:

  ```
  E_t    = max(peak_t, E_{t-1} * BOOST_ENVELOPE_RELEASE_DECAY)
  gain_t = <same floor/cap formula as before, but of E_t, not peak_t>
  ```

  - **Attack is INSTANT**: `max(peak_t, ...)` means a brand-new, brighter
    peak (the comet head) immediately raises `E` — so the head still
    hits max brightness the very frame it crosses the window; nothing
    about "fixing the fade" should also blunt this.
  - **Release is SLOW**: while the current peak stays at/below
    `E_{t-1} * decay`, `E` instead decays geometrically toward it. This
    is the fix: once the comet head leaves, `E` stays pinned near the
    head's peak for the release window, so `gain = 1/E` stays ~flat —
    the trail is rendered at its OWN true (already-fading) brightness,
    scaled by a roughly-constant gain: an accurate, monotonically
    decreasing fade that matches what's actually happening on the strip,
    never a re-amplified one, and (because the gain isn't climbing) no
    more sudden "snap to black" either — the tail fades continuously all
    the way down through the guard.
  - **Decay constant**: chosen for a ~3-second half-life at the ring's
    ~30fps render rate — long enough that a comet-trail-scale fade (a
    few hundred ms to ~1-2s in the room's scenes) completes entirely
    while still "held" near the peak, short enough that ambient content
    is boosted bright again well within a typical pause in activity, not
    tens of seconds later:

    ```python
    BOOST_ENVELOPE_HALF_LIFE_S = 3.0
    BOOST_ENVELOPE_ASSUMED_FPS = 30.0
    BOOST_ENVELOPE_RELEASE_DECAY = 0.5 ** (1.0 / (BOOST_ENVELOPE_HALF_LIFE_S
                                                  * BOOST_ENVELOPE_ASSUMED_FPS))
    # == 0.992328...
    ```

  - **"bias towards brighter" preserved**: once `E` has fully relaxed
    down toward ambient level (a handful of seconds of quiet, no new
    bright peak), the gain climbs back up and dim ambient content is
    boosted bright again — exactly the original intent of `"auto"` /
    `"vivid"`, unaffected by this fix.
  - **Old symmetric smoothing dropped, not layered on top**: the
    envelope's own asymmetry (instant up / slow down) supplies all the
    debounce needed. Layering the old EMA on top of the envelope-derived
    target would blunt the now-required INSTANT attack (delaying "the
    head hits max this frame") without adding anything the release side
    doesn't already do — so it was removed from the gain computation
    (the constant `AUTO_BOOST_SMOOTHING_ALPHA` is kept in the module,
    unused, purely as a historical reference). Verified in simulation
    (`tests/test_satellites.py`, comet-fadeout section): a simulated
    comet pass (rising head → peak → exponentially-fading tail → black,
    then a long run of steady dim ambient content) asserts the tail
    output is monotonically decreasing and stays proportional to the
    input (no plateau at max, no re-brightening), the head frame hits
    max, and — after enough quiet — the same steady dim ambient content
    gets boosted bright again.
  - **Applies identically to `"auto"`**: `"auto"`'s gain (`255/peak`,
    floor `AUTO_BOOST_FLOOR`, cap `AUTO_BOOST_MAX_GAIN`) had the exact
    same latent bug and gets the exact same fix — its envelope is
    `_boost_envelope` (0..255 scale, i.e. `window.max()`'s own scale),
    feeding `_boost_gain` each frame with the same floor/cap formula as
    before, just evaluated against the held envelope instead of the raw
    instantaneous peak.

## API

```
GET  /satellites
     -> [{"node": "dot1", "anchor": 0, "mode": "mirror", "color": null,
          "boost": "vivid", "vivid_saturation": 0.6, "vivid_lift": 0.7,
          "effective_gain": 3.42,
          "pixel_count": 7, "online": false, "resolved_ip": null, ...}]

POST /satellites/{name}   {"anchor"?: int, "mode"?: "mirror"|"independent",
                           "color"?: "#rrggbb",
                           "boost"?: "auto"|"vivid"|number|"off"|1.0,
                           "vivid_saturation"?: float (0.0..1.0) | null,
                           "vivid_lift"?: float (0.3..1.5)}
     -> {"ok": true, "satellite": {...}}
     404 unknown satellite name (or satellites unavailable on a non-ring
         manager); 422 bad anchor (out of 0..ring_len-1), bad mode, bad
         boost (non-positive number, or a string that isn't
         "auto"/"vivid"/"off"/numeric), bad vivid_saturation (outside
         0.0..1.0), or bad vivid_lift (outside 0.3..1.5).
```

Any subset of `anchor`/`mode`/`color`/`boost`/`vivid_saturation`/
`vivid_lift` may be given; omitted fields are left unchanged.
`vivid_saturation` and `vivid_lift` are independently settable — e.g.
`POST /satellites/dot1 {"vivid_lift": 0.9}` adjusts the lift shaping on
an already-`"vivid"` dot without needing to resend `boost`. Every
satellite write calls `manager.persist()`, same as a scene write.

`boost` may also be set in `satellites.json` per satellite (same four
forms, default `"off"`), along with an optional `vivid_saturation` and
`vivid_lift` (used only when `boost` is `"vivid"`; `vivid_lift` defaults
to `0.7` when omitted), e.g. `{"node": "dot1", "anchor": 0, "mode":
"mirror", "color": null, "boost": "vivid", "vivid_saturation": 0.6,
"vivid_lift": 0.7}`.

## Offline dots are harmless

Each satellite gets its own lazy-mDNS `UdpSender` (identical tolerance to
a strip node): if `dot1.local` doesn't resolve, `send_frame` silently
drops the frame; resolution is retried automatically (~20s) with zero
impact on the ring or the other two strips. When a dot board is later
flashed and joins WiFi, mDNS resolution succeeds on the next retry and
frames start flowing — **no restart, no config change, no API call
needed.**

## Persistence

`anchor`/`mode`/`color`/`boost`/`vivid_saturation`/`vivid_lift` are
stored under a new top-level `"satellites"` key in the existing
`state.json`, written/read independently of the `"nodes"` key
(`StateStore.save_satellites()` / `.load_satellites()`) so a scene save
never clobbers a satellite reposition and vice versa. On boot,
`RingManager.restore()` applies persisted satellite state on top of
whatever `satellites.json` loaded as the default — a dot without a
persisted entry keeps its config-file anchor/mode/boost/
vivid_saturation/vivid_lift, and a persisted `boost`, `vivid_saturation`,
or `vivid_lift` value that fails `normalize_boost()` /
`validate_vivid_saturation()` / `validate_vivid_lift()` validation (e.g.
hand-edited garbage in `state.json`) is ignored the same way, falling
back to the config-file default rather than blocking restore. This also
covers a `state.json` written **before** `vivid_lift` existed: the key
is simply absent (not `null`), `dict.get("vivid_lift")` returns `None`,
and `validate_vivid_lift(None)` resolves that cleanly to
`VIVID_LIFT_DEFAULT` (`0.7`) — no migration, no error.

Note: the "auto" mode's peak-hold envelope/gain (`_boost_envelope`,
`_boost_gain`) and the "vivid" mode's peak-hold envelope/gain
(`_vivid_envelope`, `_vivid_peak_gain`) are NOT persisted — all four are
per-frame render artifacts, not configuration, so they re-settle from
`0.0` (envelopes) / `1.0` (gains) after a restart — since the envelope's
attack is instant, this re-settling is invisible in practice (the very
next bright frame re-establishes the envelope immediately; see "Peak-hold
envelope" above).

## MAX BRIGHTNESS LIMITS

Dots (`dot1`/`dot2`) are powered off the ESP32 board's **3V3 rail**
(through its onboard AMS1117 regulator), not 5V directly like the ring
strips — so their firmware power cap is lower and it is the hard
do-not-exceed line, not the scene layer:

- **Dots: 450mA** (`POWER_BUDGET_MA` in `dot1.ino`/`dot2.ino`,
  enforced via `FastLED.setMaxPowerInVoltsAndMilliamps(5, 450)`). Worst
  case is a full-window (all 7px) sustained white — already
  firmware-limited by FastLED's power management, which dims the whole
  strip proportionally if the budget would be exceeded rather than
  letting current run away.
- **Strips: 900mA** (`POWER_BUDGET_MA` in `fcob-strip*.ino`, same
  `setMaxPowerInVoltsAndMilliamps` call), also firmware-enforced.

The lightserver scene/DSL layer (`master.brightness`, per-particle
color, satellite `boost`, etc.) never needs to throttle brightness to
protect the hardware below these ceilings — full-blast white/yellow at
full particle intensity across the whole window is safe on both strips
and dots. **Firmware is the enforcement layer**; the server only needs
to get the *visual* output right (correct color, correct tone mapping),
not police current draw.
