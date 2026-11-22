# NEPTR VIDEO V2 — implementation plan and status

This file records what we are building, the decisions made so far, the safety
constraints, and the boundary between implemented code and work that still
needs measurement.

## 1. The goal

Turn NEPTR VIDEO into a deliberately two-channel visual instrument:

```text
Channel A source -> A processing ----┐
                                     ├-> jit.op -> particles/layers -> master chain -> PRESENT
Channel B source -> B processing ----┘
```

The system must remain playable and recoverable while sources, operators,
effects, displays, and network conditions change. The internal render graph has
one unified resolution. Physical displays and wireless video are boundary
consumers of that graph, not separate render graphs.

## 2. What the user asked for

### Video out and video in

Support both wired Ethernet and wireless links, with tunable bandwidth,
latency, resolution, frame rate, codec quality, and transport. Automatic
adaptation is desirable, but it must preserve the visual engine before it
degrades the network boundary.

For phone-camera input, explore all practical formats rather than assuming one:

- RTSP with H.264/HEVC: primary controlled-camera candidate.
- NDI HX/HX3: useful when an NDI workflow matters; more buffering and SDK
  constraints.
- SRT/RIST: resilient on unstable Wi-Fi or routed links, at the cost of latency.
- RTP/UDP H.264/HEVC: simple controlled-LAN path with explicit bitrate/latency.
- WebRTC: attractive for browser-originated cameras, but a later dependency-heavy
  phase.
- USB/UVC: fallback for compatible phones/cameras, not assumed for iPhone.

For video output:

- Full NDI / SpeedHQ is a wired-gigabit option, with 1080p30 and 720p60 as
  pragmatic starting profiles.
- RTP/HEVC or SRT/RIST are better wireless candidates.
- WebRTC is a later browser-viewer option.
- Raw RGB/UYVY is diagnostic or very-low-latency wired mode only.

The actual encoders, decoders, SDKs, and Pi bandwidth must be measured before
calling any transport production-ready.

### Resolution

- A and B always render at the same internal resolution.
- `jit.op`, particles, feedback, and the master chain never combine different
  internal sizes.
- Wireless input/output resolution is independent and belongs at the boundary.
- HDMI/PRESENT scales the unified result to the detected physical output.
- Display mode detection should follow the connected output automatically, with
  manual output/profile controls available.
- Network adaptation must never silently lower internal render resolution.

### Palette and appearance

The desired result is a unified palette/color language across the whole system,
with palette changes available live. A source-local palette stage may be useful
later for special effects, but the default and final appearance should be
unified rather than A and B looking like unrelated instruments.

Current direction: keep the shared dual-row palette LUT and apply any later
global palette treatment after source combination where it improves coherence.

### Mapping and sensors

LFO, sensor, and mapping infrastructure is not considered a finished user
feature yet. It should remain id-addressed and testable, but it is not the next
priority. Do not spend the V2 critical path pretending those mappings are final.

## 3. Decisions already made

- Implement foundations and dual-fractal/jit.op behavior before camera, NDI, or
  deployment integration.
- Use explicit `op.use_val` semantics: an inactive B channel supplies the
  selected operator identity unless the user explicitly requests `op.val`.
- Keep exactly two channels.
- Reserve `b.*` for Channel B. Existing V1 morph ids used `b.k.*`; those must be
  migrated, never reinterpreted.
- Keep `/nv/kernel_b` as the A morph target and use `/nv/kernel_b2` for the
  Channel B generator.
- Unsupported source adapters fall back truthfully to Fractal/None until their
  actual adapters exist. The UI/wire must not claim a camera is rendering when
  the engine is drawing a fractal.
- Mappings are not a reason to delay the render-graph foundation.

## 4. Implemented so far

### Repository safety

- Git was initialized in `NEPTR_VIDEO` before V2 work.
- No commits were made, per instruction.
- `git ls-files` remains empty; all existing files remain uncommitted user work.
- Generated build/cache artifacts are ignored.

### State and parameter bank

- `ParamBank::find()` is memoized by stable string id.
- Dynamic storage is fixed-width and separated as:

```text
[global]
[Channel A macro knobs: 4]
[Channel B macro knobs: 4, b.*]
[A morph target knobs: 4, a.mk.*]
[effect knobs]
```

- Channel B and morph target values no longer share storage.
- Legacy V1 `b.k.*` preset values migrate to `a.mk.k.*` only for old preset
  versions.

### `jit.op`

- Stable append-only operator catalogue exists in `src/chain/ops.cpp`.
- CPU semantics and GLSL semantics include arithmetic, comparisons, bitwise,
  unary, screen, displacement placeholders, and related operators.
- Inactive-channel identity and explicit constant behavior are covered by tests.
- PASS B has a special inactive fallback to PASS A because no scalar B input can
  preserve the primary picture by itself.
- The operator is a uniform-switch pass, avoiding shader recompilation while an
  operator control is swept.

### Control, presets, and state

Implemented wire/state hooks:

```text
/nv/op <operator-index>
/nv/source <channel> <source-kind>
/nv/kernel_b2 <channel-B-fractal-index>
```

`/nv/sel` appends operator, source A, source B, and Channel B kernel state while
preserving its append-only compatibility rule.

Presets now persist source selections and Channel B kernel selection. V1 morph
namespace compatibility is mutation-tested.

### Resolution and I/O profile seam

- Internal render sizing remains one shared `renderW/renderH` for A, B, and
  `jit.op`.
- PRESENT/output dimensions remain physical-output concerns.
- `src/io/profile.*` defines the boundary profile for future transports.
- The following settings are parsed, clamped, and reported at startup:

```text
NV_IO_TRANSPORT=ndi|rtp|srt|rist|webrtc|raw
NV_IO_WIDTH / NV_IO_HEIGHT
NV_IO_FPS
NV_IO_BITRATE
NV_IO_LATENCY
NV_IO_AUTO=0|1
```

- Automatic adaptation changes, at most, one boundary quality dimension per
  decision in this order: FPS, boundary resolution, bitrate. It never changes
  internal render resolution.
- No encoder/decoder is falsely implied by this seam; adapters come later.

### Harness safety

- `NV_HEADLESS=1` keeps Mac tests from showing/focusing windows.
- The list-bus harness now forces headless mode.
- HOME honors `NV_MAX_FRAMES`.
- Dynamic ids in `NV_SET` are applied again after the dynamic parameter bank is
  built.
- `pkill -f` remains forbidden for Pi process control.

## 5. Current validation status

The clean build succeeds.

The complete non-GL suite currently passes:

```text
15/15 tests passed
```

This includes compiler, palette, patch, control, preset, HOME, rotation,
lifecycle, modulation, motion-clock, governor, games, control-bus, jit.op, and
I/O-profile tests.

A controlled Mac test was also run with:

- `NV_HEADLESS=1`
- `NV_SINGLE_WINDOW=1`
- `NV_MAX_FRAMES=1`
- watchdog timeout
- no `NV_WINDOW`

It initialized GLFW, compiled the renderer, rendered one frame, and wrote a
headless PPM capture to `/private/tmp/neptr_v2_headless.ppm`. No user-facing
window was opened. This proves startup plus one frame, not sustained motion or
visual correctness by eye.

The GL-backed `list_bus` suite still needs a clean rerun after the latest wire
changes. Earlier attempts were affected by the Mac AppKit/GLFW startup behavior;
the watchdog-controlled one-frame run now gets through that path.

## 6. Remaining implementation phases

### Phase 0 — validation and measurement

1. Run the controlled headless Mac capture across single-channel and dual-
   fractal/operator configurations.
2. Verify GLSL operator compilation on Mac and Pi GLES 3.0.
3. Add pixel probes for inactive B identity, `op.val`, swap, mix, and plane
   behavior.
4. Reproduce the animating-fractal frame-jump bug with governor enabled before
   changing it. The leading suspect remains governor re-planning mid-run.
5. Extend motion proofs beyond governor 0 and kernel 0.

### Phase 1 — complete the two-channel graph

1. Add true per-channel pre-FX and feedback stages.
2. Make particles consume the combined `jit.op` field as specified.
3. Keep all channel buffers at the same internal size.
4. Preserve one shared/global palette language by default.
5. Measure pass count, fill rate, VRAM, and frame-time cost with real captures.

### Phase 2 — output boundary

1. Create one readback/downscale boundary frame that can fan out to multiple
   consumers.
2. Add output profiles and actual HDMI/output mode observation.
3. Add measured encoder queueing and adaptation telemetry.
4. Keep manual controls separate from the internal render-scale control.

### Phase 3 — sources and transports

1. Add a non-blocking `StreamSource` contract: latest frame, timestamp,
   dimensions, connected state, and hold-last-good-frame dropout policy.
2. Implement the first practical phone input, recommended RTSP/HEVC where the
   phone publisher supports it.
3. Add optional NDI input only when the SDK/build path is available.
4. Implement wired full NDI output and measure CPU/memory/bandwidth on the Pi.
5. Add RTP/HEVC or SRT/RIST wireless paths after the wired boundary is proven.
6. Consider WebRTC only when browser-native video is worth its dependency cost.

### Phase 4 — Pi integration and deployment

1. Never overwrite the Pi's live control-plane patches during deploy.
2. Compare Mac and Pi trees before transfer.
3. Preserve the Pi-only control-plane patches 1 and 2.
4. Use safe process targeting; never `pkill -f` over SSH.
5. Test boot to HOME, touchscreen HOME activation, web VISUALS, HDMI hotplug,
   wireless loss/reconnect, and preset recall on the actual hardware.

## 7. Explicit non-goals for the current slice

- No claim that camera input works yet.
- No claim that NDI output works yet.
- No claim that wireless auto-adaptation is connected to a live encoder yet.
- No per-channel internal resolutions.
- No deployment to the Pi until the Mac and code-level tests are stable.
- No preset-file invention beyond the implemented format; existing disk state is
  still the source of truth.

## 8. Landmines that must stay visible

- `b.*` is not available for morphing; `b.k.cre` is Channel B's namespace now.
- The Mac `DEMIURGE_OS` tree does not contain the Pi's live control-plane
  patches 1 and 2.
- Claims come from executing and measuring, not from reading code alone.
- Reproduce a bug before fixing it.
- Mac test runs must remain headless. The Pi screen may be captured when needed.
- 16/16 was a narrow baseline: motion suites disabled the governor and used
  kernel 0, so they did not cover the known governor re-plan suspicion.
- No preset files currently exist on disk; that is a test/setup fact, not proof
  that preset persistence is visually verified on hardware.
